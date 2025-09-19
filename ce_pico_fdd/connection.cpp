#include <stdio.h>
#include <string.h>

#include "pico/cyw43_arch.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/tcp.h "

#include "pico/stdlib.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"

#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "display.h"
#include "psram.h"
#include "client_context.h"

#define SERVER_UDP_PORT 7200 // port number where CE listens for client requests
#define CLIENT_UDP_PORT 7201 // port where this client should listen for CE responses

extern Settings_t Settings;

bool udpInitialized;

struct udp_pcb* pcbUpd;

typedef struct {
    struct tcp_pcb *pcb;
    ClientContext *cc;
} TConnection;

TConnection clientFdd;
TConnection clientIkbd;

uint8_t hostIp[4];
std::string hostIpString;
ip_addr_t hostIpAddr;
uint16_t hostPortHdd;
uint16_t hostPortFdd;
uint16_t hostPortIkbd;

bool connectedToWifi;
bool connectedToHost;

THeader fddHeader;      // keep the header global to preserve syncTag between calls

extern uint8_t imgTracks, imgSides, imgSectorsPerTrack;
extern char imageFileName[32];
extern bool diskChanged;
extern int imageState;

void storeMacAddress(void);

void showRunningStateOnDisplay(void)
{
    char msg1[128];
    sprintf(msg1, "ssid: %s", Settings.ssid);

    char msg2[64];
    sprintf(msg2, "host: %s %c", hostIpString.c_str(), Settings.ikbdEnabled ? 'I' : ' ');

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
        hostIpAddr = *addr;
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

        xprintf("ceDiscoveryReceive - got host ip: %s, ports: %d, %d, %d\n", hostIpString.c_str(), hostPortHdd, hostPortFdd, hostPortIkbd);
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

    // Bind to any IP, given port
    err_t err = udp_bind(pcbUpd, IP_ADDR_ANY, CLIENT_UDP_PORT);
    if (err != ERR_OK) {
        xprintf("udp_bind failed: %d\n", err);
        udp_remove(pcbUpd);
        return;
    }

    // Register receive callback
    udp_recv(pcbUpd, udp_recv_callback, NULL);
    xprintf("UDP receiver listening on port %u\n", CLIENT_UDP_PORT);

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

    char msg[128];

    // no wifi SSID stored? cannot connect
    if (strlen(Settings.ssid) == 0) {
        xprintf("connectToWifi - not connecting to WIFI yet, because no SSID stored\n");
        return;
    }

    // not connected to wifi yet, try to connect
    int new_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    if (new_status != CYW43_LINK_UP) {      // not connected? connect now
        xprintf("connectToWifi - connecting, ssid: %s, password: %s\n", Settings.ssid, Settings.password);

        sprintf(msg, "ssid: %s", Settings.ssid);
        displayMessage("wifi connecting", msg);

        cyw43_arch_wifi_connect_async(Settings.ssid, Settings.password, CYW43_AUTH_WPA2_AES_PSK);
    } 
    else
    {
        xprintf("connectToWifi - not connecting, status: %d\n", new_status);
    }

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

    // get ip, mask, create broadcast address
    u32_t ip_u32 = ip4_addr_get_u32(netif_ip_addr4(netif_default));
    u32_t mask_u32 = ip4_addr_get_u32(netif_ip4_netmask(netif_default));
    u32_t bcast_u32 = (ip_u32 & mask_u32) | (~mask_u32);

    if(ip_u32 == 0) {   // no ip? don't send broadcast
        xprintf("ceDiscoverySend - no IP yet\n");
        return;
    }

    udpInitialize();

    displayMessage("wifi connected", "CE host discovery");

    // alloc buffer, copy data to payload part
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 4 + 1, PBUF_RAM);
    memcpy((char *)p->payload, "CELC", 4);

    whichBroadcastAddr = !whichBroadcastAddr;   // toggle this flag

    if(whichBroadcastAddr)      // send to subnet broadcast addr?
    {
        ip4_addr_t bcast;
        ip4_addr_set_u32(&bcast, bcast_u32);
        xprintf("ceDiscoverySend to %d.%d.%d.%d\n", bcast_u32 & 0xff, (bcast_u32 >> 8) & 0xff, (bcast_u32 >> 16) & 0xff, (bcast_u32 >> 24) & 0xff);
        udp_sendto(pcbUpd, p, &bcast, SERVER_UDP_PORT);                 // broadcast to subnet devices (e.g. 192.168.1.255)
    } 
    else        // send to generic broadcast addr
    {
        xprintf("ceDiscoverySend to 255.255.255.255\n");
        udp_sendto(pcbUpd, p, IP_ADDR_BROADCAST, SERVER_UDP_PORT);     // broadcast to all possible devices (255.255.255.255)
    }

    pbuf_free(p);
}

size_t clientFddWrite(const uint8_t *buf, size_t size)
{
    if (!clientFdd.cc || !size) {
        return 0;
    }

    clientFdd.cc->setTimeout(500);
    return clientFdd.cc->write((const char*)buf, size);
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

bool clientFddAvailable(void)
{
    if(!clientFdd.cc || !clientFdd.cc->availableForWrite()) {
        return false;
    }

    return true;
}

bool clientFddConnected(void)
{
    if (!clientFdd.cc || clientFdd.cc->state() == CLOSED) {
        return false;
    }

    return clientFdd.cc->state() == ESTABLISHED || clientFddAvailable();
}

void connectToCEhost(void)
{
    static uint32_t lastAttempt = 0xffff0000; // -65k
    static bool loggedOnce = false;

    if (clientFddConnected())   // already connected? quit
    { 
        if(!loggedOnce) {
            xprintf("connectToCEhost - connected!\n");
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
    xprintf("connectToCEhost - IP: %s, port: %d\n", hostIpString.c_str(), hostPortFdd);

    // start connection attempt

    if (clientFdd.cc) {
        clientFdd.cc->close();
        clientFdd.cc->unref();
        clientFdd.cc = nullptr;
    }

    tcp_pcb* pcb = tcp_new();
    if (!pcb) {
        return;
    }

    clientFdd.cc = new ClientContext(pcb, nullptr, nullptr);
    clientFdd.cc->ref();
    clientFdd.cc->setTimeout(5000);

    clientFdd.cc->connectAsync(&hostIpAddr, hostPortFdd);

    clientFdd.cc->setSync(true);
    clientFdd.cc->setNoDelay(true);
}

void connectToHost(void)
{
    static bool prevConnectedToHost = false;
    static bool prevConnectedToWifi = false;

    connectedToWifi = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
    connectedToHost = connectedToWifi && clientFddConnected();

    if(prevConnectedToWifi != connectedToWifi) {    // connectedToWifi changed since last time? log it
        prevConnectedToWifi = connectedToWifi;
        xprintf("connectedToWifi: %d\n", connectedToWifi);
    }

    // on connected state changed
    if(prevConnectedToHost != connectedToHost)
    {
        prevConnectedToHost = connectedToHost;

        if(connectedToHost) {     // now in connected state, display state on display
            imageState = IMAGE_NOT_LOADED;
            showRunningStateOnDisplay();
        }
    }

    if(!connectedToWifi) {
        connectToWifi();
    }

    if(connectedToHost)     // we're connected to host? don't send discovery packets
    {
        return;
    }

    // device connected to wifi, do discovery if needed
    ceDiscoverySend();

    // connect to CE server
    connectToCEhost();
}

bool getIncommingHeader(void)
{
    if(!clientFddConnected()) {   // clientFdd not connected, no header received
        return false;
    }

    while(true)
    {
        // Determine how many bytes are needed to be received, if we want to get 10 bytes of header.
        // If we already got some bytes in the syncTag, we need less than 10 bytes.
        int needed = 10;
        if((fddHeader.syncTag & 0xffffff) == (SYNC_TAG_FDD >> 8))      // got c050d1 (3 bytes) already? need only 7 more
        {
            needed = 7;
        }
        else if((fddHeader.syncTag & 0xffff) == (SYNC_TAG_FDD >> 16))  // got c050 (2 bytes) already? need only 8 more
        {
            needed = 8;
        }
        else if((fddHeader.syncTag & 0xff) == (SYNC_TAG_FDD >> 24))    // got c0 (1 bytes) already? need only 9 more
        {
            needed = 9;
        }
        else                // in other cases, expect to have all 10 bytes available before trying to read header
        {
            needed = 10;
        }

        // not enough data for full header, no header received
        int available = clientFdd.cc->getSize();
        if(available < needed)
        {
            return false;
        }

        uint32_t data = clientFdd.cc->read(); // read byte
        fddHeader.syncTag = fddHeader.syncTag << 8;     // shift previous sync tag one byte up
        fddHeader.syncTag |= data;                      // add lowest byte to syncTag

        if(fddHeader.syncTag == SYNC_TAG_FDD)           // found expected syncTag
        {
            uint8_t restOfHeader[6];
            clientFdd.cc->read(restOfHeader, 6);  // read rest of the header

            fddHeader.cmdCode = getWord(restOfHeader);
            fddHeader.len = getDword(restOfHeader + 2);

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
    if(!clientFddConnected()) {   // clientFdd not connected, no header received
        return;
    }

    int lenData = MIN(READTRACKDATA_SIZE_BYTES, fddHeader.len);  // limit read length to buffer length
    clientFdd.cc->read(tmpTrackBfr, lenData, 500);    // read into tmpTrackBuffer size lenData, wait max specified timeout

    // read track # and side # from bfr
    int trackNo = MIN(tmpTrackBfr[0], MAX_TRACKS - 1);
    int sideNo = MIN(tmpTrackBfr[1], 1);

    // store the track track data into PSRAM
    psramStoreTrack(trackNo, sideNo, tmpTrackBfr + 2);

    xprintf("Rx %d side %d\n", trackNo, sideNo);

    receivedTracks++;

    if(imageState == IMAGE_REQUESTED) {
        showImageLoadProgress();
    }
}

void handleImageReceived(void)
{
    if(!clientFddConnected()) {   // clientFdd not connected, no header received
        return;
    }

    int lenData = MIN(READTRACKDATA_SIZE_BYTES, fddHeader.len);  // limit read length to buffer length
    clientFdd.cc->read(tmpTrackBfr, lenData, 500);    // read into tmpTrackBuffer size lenData, wait max specified timeout

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

    xprintf("handleImageReceived %s, imgTracks: %d, imgSides: %d, imageFileName: %s\n", (tmpTrackBfr[0] == 1) ? "END" : "START", imgTracks, imgSides, imageFileName);
}

void handleIncommingData(void)
{
    if(getIncommingHeader())        // if got valid hdd header
    {
        fddHeader.syncTag = 0;      // clear sync tag

        switch(fddHeader.cmdCode) {
            case ATN_SEND_TRACK: handleTrackReceived(); break;
            case ATN_SEND_WHOLE_IMAGE: handleImageReceived(); break;
            default: xprintf("unknown cmdCode %x\n", fddHeader.cmdCode); break;
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
    if(!clientFddConnected())
    {
        return false;
    }

    uint32_t writtenCount = clientFddWrite(bfr, dataSizeBytes);
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
