#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/tcp.h "

#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "captive_portal.h"
#include "display.h"
#include "psram.h"

bool wifiSettingsLoaded;
std::string ssid;
std::string password;

#define SERVER_UDP_PORT 7200 // port number where CE listens for client requests
#define CLIENT_UDP_PORT 7201 // port where this client should listen for CE responses

// WiFiUDP udp;
bool udpInitialized;

struct udp_pcb* pcbUpd;

typedef struct {
    struct tcp_pcb *pcb;
    bool connected;
} TConnection;

TConnection clientFdd;
TConnection clientIkbd;

uint8_t hostIp[4];
std::string hostIpString;
uint16_t hostPortHdd;
uint16_t hostPortFdd;
uint16_t hostPortIkbd;

bool connected;
extern volatile bool ikbdEnabled;   // if true, should send data to host; otherwise just loopback ikdb data back

THeader fddHeader;      // keep the header global to preserve syncTag between calls

extern uint8_t imgTracks, imgSides, imgSectorsPerTrack;
extern char imageFileName[32];
extern bool diskChanged;
extern int imageState;

void storeMacAddress(void);

void showRunningStateOnDisplay(void)
{
    char msg1[128];
    sprintf(msg1, "ssid: %s", ssid.c_str());

    char msg2[64];
    sprintf(msg2, "host: %s %c", hostIpString.c_str(), ikbdEnabled ? 'I' : ' ');

    char msg3[64];
    sprintf(msg3, "image: %s", imageFileName);

    displayMessage(msg1, msg2, msg3);
}

// Callback for incoming UDP packets
static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    if (p == NULL) {        // no buffer?
        return;
    }

    printf("Received UDP packet from %s:%u, length %d\n", ip4addr_ntoa(ip_2_ip4(addr)), port, p->len);

    if(p->len < 10) {       // too short?
        return;
    }

    uint8_t* payload = (uint8_t*) p->payload;

    /*
        received packet structure:
        0..3    'CELR' string
        4..5    port for hdd
        6..7    port for fdd
        8..9    port for ikbd
    */

    // CELR found at start
    if (strncmp((const char *) payload, "CELR", 4) == 0)
    {
        uint32_t sender_ip = lwip_ntohl(ip4_addr_get_u32(ip_2_ip4(addr)));

        int shift = 24;
        for(int i=0; i<4; i++) {
            hostIp[i] = sender_ip >> shift;
            shift -= 8;
        }

        hostIpString = ip4addr_ntoa(ip_2_ip4(addr));

        // store ports and stop receiving
        hostPortHdd = getWord(payload + 4);
        hostPortFdd = getWord(payload + 6);
        hostPortIkbd = getWord(payload + 8);

        printf("ceDiscoveryReceive - got host ip: %s, ports: %d, %d, %d\n", hostIpString, hostPortHdd, hostPortFdd, hostPortIkbd);
    }

    // Free the buffer after processing
    pbuf_free(p);
}

void udpInitialize(void)
{
    if(udpInitialized) {    // already initialized, quit
        return;
    }

    pcbUpd = udp_new();

    // udp_set_flags(pcbUpd, UDP_FLAGS_BROADCAST);

    // Bind to any IP, given port
    err_t err = udp_bind(pcbUpd, IP_ADDR_ANY, CLIENT_UDP_PORT);
    if (err != ERR_OK) {
        printf("udp_bind failed: %d\n", err);
        udp_remove(pcbUpd);
        return;
    }

    // Register receive callback
    udp_recv(pcbUpd, udp_recv_callback, NULL);
    printf("UDP receiver listening on port %u\n", CLIENT_UDP_PORT);

    udpInitialized = true;
}

// Read wifi settings, connect to wifi if not connected, don't try too often.
void connectToWifi(void)
{
    static uint32_t lastAttempt = 0xffff0000; // -65k

    // if last attempt was less than a moment ago, don't try
    if ((millis() - lastAttempt) < 15000)
    {
        return;
    }

    // we're connecting now
    lastAttempt = millis();

    // already connected to wifi? quit
    if(cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
        return;
    }

    // read wifi settings if they aren't loaded yet
    if (!wifiSettingsLoaded)
    {
        wifiSettingsLoaded = true;
        getSsidAndPassword(ssid, password);
    }

    printf("connectToWifi - ssid: %s, password: %s\n", ssid, password);

    char msg[128];

    // no ssid and no passowrd? run captive portal
    if(ssid.length() == 0 && password.length() == 0) {
        printf("connectToWifi - no wifi settings, starting captive portal");
        runCaptivePortal();
    }

    // no wifi SSID stored? cannot connect
    if (ssid.length() == 0)
    {
        return;
    }

    sprintf(msg, "ssid: %s", ssid.c_str());
    displayMessage("wifi connecting", msg);

    printf("connectToWifi - ssid: %s\n", ssid);

    // not connected to wifi yet, try to connect
    cyw43_arch_wifi_connect_async(ssid.c_str(), password.c_str(), CYW43_AUTH_WPA2_AES_PSK);

    storeMacAddress();      // copy wifi mac address to fw report buffer
}

// Send broadcast to find any CE server on the network.
void ceDiscoverySend(void)
{
    static uint32_t lastAttempt = 0xffff0000; // -65k
    static bool whichBroadcastAddr = false;

    // already got hostIp and port? don't do discovery
    if (hostIp[0] != 0 && hostPortFdd != 0)
    {
        return;
    }

    // if last attempt was less than a moment ago, don't try
    if ((millis() - lastAttempt) < 1000)
    {
        return;
    }

    lastAttempt = millis();

    udpInitialize();

    displayMessage("wifi connected", "CE host discovery");

    // alloc buffer, copy data to payload part
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 4 + 1, PBUF_RAM);
    memcpy((char *)p->payload, "CELC", 4);

    whichBroadcastAddr = !whichBroadcastAddr;   // toggle this flag

    if(whichBroadcastAddr)      // send to subnet broadcast addr?
    {
        struct netif *netif = netif_default;
        const ip4_addr_t *ip = netif_ip4_addr(netif);
        const ip4_addr_t *netmask = netif_ip4_netmask(netif);

        ip4_addr_t bcast;
        u32_t ip_u32 = ip4_addr_get_u32(ip);
        u32_t mask_u32 = ip4_addr_get_u32(netmask);
        u32_t bcast_u32 = (ip_u32 & mask_u32) | (~mask_u32);

        ip4_addr_set_u32(&bcast, bcast_u32);
        printf("ceDiscoverySend to %d.%d.%d.%d\n", (bcast_u32 >> 24) & 0xff, (bcast_u32 >> 16) & 0xff, (bcast_u32 >> 8) & 0xff, bcast_u32 & 0xff);
        udp_sendto(pcbUpd, p, &bcast, SERVER_UDP_PORT);                 // broadcast to subnet devices (e.g. 192.168.1.255)
    } 
    else        // send to generic broadcast addr
    {
        printf("ceDiscoverySend to 255.255.255.255\n");
        udp_sendto(pcbUpd, p, IP_ADDR_BROADCAST, SERVER_UDP_PORT);     // broadcast to all possible devices (255.255.255.255)
    }

    pbuf_free(p);
}

uint32_t tcp_client_write(TConnection* con, uint8_t* data, uint32_t len)
{
    err_t wr_err = tcp_write(con->pcb, data, len, TCP_WRITE_FLAG_COPY);

    if (wr_err == ERR_OK) {
        tcp_output(con->pcb);
        return len;
    }
    
    printf("tcp_write failed: %d\n", wr_err);
    return 0;
}

TConnection* pcbToConnection(tcp_pcb* tpcb)
{
    if(tpcb == clientFdd.pcb) {
        return &clientFdd;
    } else if(tpcb == clientIkbd.pcb) {
        return &clientIkbd;
    }

    return NULL;
}

// Called when data is received from the server
static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    TConnection* con = pcbToConnection(tpcb);
    if(!con) {
        return ERR_OK;
    }

    if(!p) {
        printf("Connection closed by remote host\n");
        tcp_close(con->pcb);
        con->connected = false;
        return ERR_OK;
    }

    // printf("Received %d bytes: %.*s\n", p->len, p->len, (char *)p->payload);

    // Tell lwIP we've consumed the data
    tcp_recved(con->pcb, p->len);
    pbuf_free(p);

    return ERR_OK;
}

// Called when connection is successfully established
static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    TConnection* con = pcbToConnection(tpcb);
    if(!con) {
        return ERR_OK;
    }

    if(err != ERR_OK) {
        con->connected = false;
        printf("Connection failed: %d\n", err);
        return err;
    }

    con->connected = true;

    // Set receive callback
    tcp_recv(con->pcb, tcp_client_recv);

    return ERR_OK;
}

// Called on fatal errors (connection reset, timeout, etc.)
static void tcp_client_error(void *arg, err_t err)
{
    printf("TCP connection aborted, err=%d\n", err);
}

void tcp_client_connect(const char *remote_ip, uint16_t remote_port, TConnection* con)
{
    ip_addr_t server_addr;

    if (!ip4addr_aton(remote_ip, &server_addr)) {
        printf("Invalid IP address: %s\n", remote_ip);
        return;
    }

    con->pcb = tcp_new_ip_type(IPADDR_TYPE_V4);

    if (!con->pcb) {
        printf("Failed to create PCB\n");
        return;
    }

    // Register error callback
    tcp_err(con->pcb, tcp_client_error);

    printf("Connecting to %s:%u\n", remote_ip, remote_port);

    // Initiate connection (async)
    tcp_connect(con->pcb, &server_addr, remote_port, tcp_client_connected);
}

void connectToCEhost(void)
{
    static uint32_t lastAttempt = 0xffff0000; // -65k
    static bool loggedOnce = false;

    if (clientFdd.connected)
    { // already connected? quit
        if(!loggedOnce) {
            printf("connectToCEhost - connected!\n");
            loggedOnce = true;
        }

        return;
    }
    loggedOnce = false;

    // if last attempt was less than a moment ago, don't try
    if ((millis() - lastAttempt) < 3000)
    {
        return;
    }

    // we're connecting now
    lastAttempt = millis();

    // no host IP? not connecting
    if (hostIpString.length() == 0)
    {
        return;
    }

    displayMessage("wifi connected", "connecting to host:", hostIpString.c_str());
    printf("connectToCEhost - IP: %s, port: %d\n", hostIpString.c_str(), hostPortFdd);

    // start connection attempt
    tcp_client_connect(hostIpString.c_str(), hostPortFdd, &clientFdd);
}

void connectToHost(void)
{
    static bool prevConnected = false;
    connected = clientFdd.connected && (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP);

    // on connected state changed
    if(prevConnected != connected)
    {
        prevConnected = connected;

        if(connected) {     // now in connected state, display state on display
            imageState = IMAGE_NOT_LOADED;
            showRunningStateOnDisplay();
        }
    }

    // socket connected, wifi connected? just quit
    if(connected)
    {
        return;
    }

    connectToWifi();

    // device connected to wifi, do discovery if needed
    ceDiscoverySend();

    // connect to CE server
    connectToCEhost();
}

bool getIncommingHeader(TConnection* client, uint32_t expectedSyncTag, THeader* header)
{
    if(!client->connected)    // client not connected, no header received
    {
        return false;
    }

    while(true)
    {
        // Determine how many bytes are needed to be received, if we want to get 10 bytes of header.
        // If we already got some bytes in the syncTag, we need less than 10 bytes.
        int needed = 10;
        if((header->syncTag & 0xffffff) == (expectedSyncTag >> 8))      // got c050d1 (3 bytes) already? need only 7 more
        {
            needed = 7;
        }
        else if((header->syncTag & 0xffff) == (expectedSyncTag >> 16))  // got c050 (2 bytes) already? need only 8 more
        {
            needed = 8;
        }
        else if((header->syncTag & 0xff) == (expectedSyncTag >> 24))    // got c0 (1 bytes) already? need only 9 more
        {
            needed = 9;
        }
        else                // in other cases, expect to have all 10 bytes available before trying to read header
        {
            needed = 10;
        }

        // not enough data for full header, no header received
        // int available = client->available();     // TODO:
        int available = 0;
        if(available < needed)
        {
            return false;
        }

        // uint32_t data = ((uint8_t) client->read()); // read byte TODO:
        uint32_t data = 0;
        header->syncTag = header->syncTag << 8;     // shift previous sync tag one byte up
        header->syncTag |= data;                    // add lowest byte to syncTag

        if(header->syncTag == expectedSyncTag)      // found expected syncTag
        {
            uint8_t rest[6];
            // client->read(rest, 6);               // read rest of header      // TODO:
            header->cmdCode = getWord(rest);
            header->len = getDword(rest + 2);

            return true;                        // got complete header
        }
    }

    return false;       // no valid header
}

int receivedTracks = 0;

void showImageLoadProgress(void)
{
    static uint32_t lastDisplay = 0;

    uint32_t now = millis();
    if(now - lastDisplay < 200) {       // too soon after last display? quit
        return;
    }
    lastDisplay = now;

    uint32_t totalTracks = MAX(imgTracks * imgSides, 1);
    uint32_t percent = (receivedTracks * 100) / totalTracks;    // calc progress in percents

    char progress[32];
    memset(progress, 0, 32);
    for(int i=0; i<20; i++) {
        progress[i] = (percent > (i * 5)) ? '*' : ' ';
    }

    displayMessage("Loading image", imageFileName, progress);  // show on display
}

uint8_t tmpTrackBfr[READTRACKDATA_SIZE_BYTES];

void handleTrackReceived(void)
{
    int lenData = MIN(READTRACKDATA_SIZE_BYTES, fddHeader.len);  // limit read length to buffer length

    int len = lenData;
    uint8_t* pBfr = tmpTrackBfr;        // read into temp track buffer

    uint32_t start = millis();
    while(len > 0)
    {
        uint32_t now = millis();
        if((now - start) > 2000) {
            printf("handleTrackReceived TIMEOUT!\n");
            return;
        }

        // int readLen = clientFdd.read(pBfr, len);    // read data
        int readLen = 0;
    
        if(readLen > 0)     // something was read? decrease size of what we need to read, advance in buffer
        {
            len -= readLen;
            pBfr += readLen;
        }
    }

    // read track # and side # from bfr
    int trackNo = MIN(tmpTrackBfr[0], MAX_TRACKS - 1);
    int sideNo = MIN(tmpTrackBfr[1], 1);

    // store the track track data into PSRAM
    psramStoreTrack(trackNo, sideNo, tmpTrackBfr + 2);

    printf("Rx %d side %d\n", trackNo, sideNo);

    receivedTracks++;

    if(imageState == IMAGE_REQUESTED) {
        showImageLoadProgress();
    }
}

void handleImageReceived(void)
{
    int lenData = MIN(READTRACKDATA_SIZE_BYTES, fddHeader.len);  // limit read length to buffer length
    // clientFdd.read(tmpTrackBfr, lenData);

    if(tmpTrackBfr[0] == 1)     // image receiving finished?
    {
        diskChanged = true;
        imageState = IMAGE_LOADED;
        showRunningStateOnDisplay();
    }
    else                // image receiving started?
    {
        receivedTracks = 0;
        imageState = IMAGE_REQUESTED;
    }

    imgTracks = tmpTrackBfr[1];
    imgSides = tmpTrackBfr[2];
    imgSectorsPerTrack = tmpTrackBfr[3];

    memset(imageFileName, 0, 32);
    strncpy(imageFileName, (const char*) (tmpTrackBfr + 4), 31);        // store file name, up to 31 chars

    printf("handleImageReceived %s, imgTracks: %d, imgSides: %d, imageFileName: %s\n", (tmpTrackBfr[0] == 1) ? "END" : "START", imgTracks, imgSides, imageFileName);
}

void handleIncommingData(void)
{
    if(getIncommingHeader(&clientFdd, SYNC_TAG_FDD, &fddHeader))   // if got valid hdd header
    {
        fddHeader.syncTag = 0;      // clear sync tag

        switch(fddHeader.cmdCode) {
            case ATN_SEND_TRACK: handleTrackReceived(); break;
            case ATN_SEND_WHOLE_IMAGE: handleImageReceived(); break;
            default: printf("unknown cmdCode %x\n", fddHeader.cmdCode); break;
        }
    }
}

/*
    Send data as is to host using the desired socket.
    This just selects the right socket and sends the count of data specified in dataSizeBytes.
    @param bfr Pointer to start of the data buffer
    @param dataSizeBytes Size of the data you want to send.
*/
bool sendDataToHost(uint8_t *bfr, uint32_t dataSizeBytes)
{
    if(!clientFdd.connected)
    {
        return false;
    }

    uint32_t writtenCount = tcp_client_write(&clientFdd, bfr, dataSizeBytes);
    return (writtenCount == dataSizeBytes);
}

/*
    Send header and data to host using the desired socket.
    This is just extension to sendDataToHost, because it also stores the data size in the header and sends the header, too.
    @param bfr Pointer to start of the data buffer
    @param dataSizeBytes Size of the data portion after the header (header is TX_HEADER_SIZE bytes big) in bytes
*/
bool sendHeaderAndDataToHost(uint8_t *bfr, uint32_t dataSizeBytes)
{
    storeDword(bfr + 6, dataSizeBytes);     // store the tx length on index 6..9
    return sendDataToHost(bfr, TX_HEADER_SIZE + dataSizeBytes);
}
