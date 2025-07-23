#include "WiFi.h"
#include <Preferences.h>

#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "captive_portal.h"
#include "display.h"

extern Preferences preferences;
extern uint8_t enabledIDs;

bool wifiSettingsLoaded;
String ssid;
String password;

#define SERVER_UDP_PORT 7200 // port number where CE listens for client requests
#define CLIENT_UDP_PORT 7201 // port where this client should listen for CE responses

NetworkUDP udp;
bool udpInitialized;

NetworkClient clientHdd;
NetworkClient clientFdd;
NetworkClient clientIkbd;

uint8_t hostIp[4];
String hostIpString;
uint16_t hostPortHdd;
uint16_t hostPortFdd;
uint16_t hostPortIkbd;

extern uint8_t state;
extern uint32_t dataCnt;
extern uint8_t statusByte;
extern bool dataReceived;

bool connected;

extern volatile bool ikbdEnabled;   // if true, should send data to host; otherwise just loopback ikdb data back

THeader hddHeader;      // keep the header global to preserve syncTag between calls

void storeMacAddress(void);

void showRunningStateOnDisplay(void)
{
    char msg1[128];
    sprintf(msg1, "ssid: %s", ssid.c_str());

    char msg2[64];
    sprintf(msg2, "host: %s", hostIpString.c_str());

    String msg3 = "devs: ";
    for(int i=0; i<8; i++) {
        if(enabledIDs & (1 << i)) {     // if ID bit enabled, add to string
            msg3 += i;
            msg3 += " ";
        }
    }

    if(ikbdEnabled) {       // if ikbd is enabled
        msg3 += "IKBD";
    }

    displayMessage(msg1, msg2, msg3.c_str());
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

    if(WiFi.status() == WL_CONNECTED) {     // already connected to wifi? quit
        return;
    }

    // read wifi settings if they aren't loaded yet
    if (!wifiSettingsLoaded)
    {
        wifiSettingsLoaded = true;

        preferences.begin("credentials", PREFERENCES_RO_MODE);
        ssid = preferences.getString("ssid", "");
        password = preferences.getString("password", "");
        preferences.end();
    }

    Serial.print("connectToWifi - ssid: ");
    Serial.print(ssid);
    Serial.print(", password: ");
    Serial.println(password);

    char msg[128];

    // no ssid and no passowrd? run captive portal
    if(ssid.length() == 0 && password.length() == 0) {
        Serial.println("connectToWifi - no wifi settings, starting captive portal");
        runCaptivePortal();
    }

    // no wifi SSID stored? cannot connect
    if (ssid.length() == 0)
    {
        return;
    }

    sprintf(msg, "ssid: %s", ssid.c_str());
    displayMessage("wifi connecting", msg);

    Serial.print("connectToWifi - ssid: ");
    Serial.println(ssid);

    // not connected to wifi yet, try to connect
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    storeMacAddress();      // copy wifi mac address to fw report buffer
}

// Send broadcast to find any CE server on the network.
void ceDiscoverySend(void)
{
    static uint32_t lastAttempt = 0xffff0000; // -65k
    static bool whichBroadcastAddr = false;

    // already got hostIp and port? don't do discovery
    if (hostIp[0] != 0 && hostPortHdd != 0)
    {
        return;
    }

    // if last attempt was less than a moment ago, don't try
    if ((millis() - lastAttempt) < 1000)
    {
        return;
    }

    lastAttempt = millis();

    // if UDP not initialized, do that now
    if (!udpInitialized)
    {
        udp.begin(WiFi.localIP(), CLIENT_UDP_PORT);
        udpInitialized = true;
    }

    displayMessage("wifi connected", "CE host discovery");

    // send upd broadcast
    uint8_t updPacket[4];
    strcpy((char *)updPacket, "CELC");

    whichBroadcastAddr = !whichBroadcastAddr;   // toggle this flag

    if(whichBroadcastAddr)      // send to subnet broadcast addr?
    {
        // get local ip and mask, create broadcast ip
        IPAddress ip = WiFi.localIP();
        uint32_t ip32 = (((uint32_t)ip[0]) << 24) | (((uint32_t)ip[1]) << 16) | (((uint32_t)ip[2]) << 8) | (((uint32_t)ip[3]));

        IPAddress mask = WiFi.subnetMask();
        uint32_t mask32 = (((uint32_t)mask[0]) << 24) | (((uint32_t)mask[1]) << 16) | (((uint32_t)mask[2]) << 8) | (((uint32_t)mask[3]));
        uint32_t mask32inv = ~mask32;

        uint32_t ip32broadcast = ip32 | mask32inv; // create broadcast addr by setting all subnet bits to 1

        IPAddress addrBroadcast((uint8_t) (ip32broadcast >> 24), (uint8_t) (ip32broadcast >> 16), (uint8_t) (ip32broadcast >> 8), (uint8_t) ip32broadcast);    // from uint32_t to object

        Serial.print("ceDiscoverySend to ");
        Serial.println(addrBroadcast.toString().c_str());

        // broadcast to subnet devices (e.g. 192.168.1.255)
        udp.beginPacket(addrBroadcast.toString().c_str(), SERVER_UDP_PORT);
        udp.write(updPacket, 4);
        udp.endPacket();
    } 
    else        // send to generic broadcast addr
    {
        Serial.println("ceDiscoverySend to 255.255.255.255");

        // broadcast to all possible devices (255.255.255.255)
        udp.beginPacket("255.255.255.255", SERVER_UDP_PORT);
        udp.write(updPacket, 4);
        udp.endPacket();
    }
}

// Receive response from server if there is any and store it if it's valid.
// Serves also for dropping any additional udp packets.
void ceDiscoveryReceive(void)
{
    static uint32_t lastAttempt = 0xffff0000; // -65k

    // if last attempt was less than a moment ago, don't try
    if ((millis() - lastAttempt) < 100)
    {
        return;
    }

    lastAttempt = millis();

    // if UDP not initialized, do that now
    if (!udpInitialized)
    {
        udp.begin(WiFi.localIP(), CLIENT_UDP_PORT);
        udpInitialized = true;
    }

    // check if any udp packet was received, handle it
    while (true)
    {
        int packetSize = udp.parsePacket();

        // no packet received? can stop trying to read it
        if (packetSize == 0)
        {
            break;
        }

        /*
            received packet structure:
            0..3    'CELR' string
            4..5    port for hdd
            6..7    port for fdd
            8..9    port for ikbd
        */
        uint8_t buffer[10];
        memset(buffer, 0, 10);
        udp.read(buffer, 10);

        // start of the data isn't CELR? skip the rest
        if (strncmp((const char *)buffer, "CELR", 4) != 0)
        {
            continue;
        }

        // store server's ip address
        IPAddress remoteIp = udp.remoteIP();
        for (int i = 0; i < 4; i++)
        {
            hostIp[i] = remoteIp[i];
        }

        IPAddress addr(hostIp[0], hostIp[1], hostIp[2], hostIp[3]); // octets to IPAddress
        hostIpString = addr.toString();                             // copy the ip address as string

        // store ports and stop receiving
        hostPortHdd = getWord(buffer + 4);
        hostPortFdd = getWord(buffer + 6);
        hostPortIkbd = getWord(buffer + 8);

        Serial.print("ceDiscoveryReceive - got host ip: ");
        Serial.print(hostIpString);
        Serial.print(", ports: ");
        Serial.print(hostPortHdd);
        Serial.print(", ");
        Serial.print(hostPortFdd);
        Serial.print(", ");
        Serial.println(hostPortIkbd);
    }
}

void connectToCEhost(void)
{
    static uint32_t lastAttempt = 0xffff0000; // -65k
    static bool loggedOnce = false;

    if (clientHdd.connected())  // && clientIkbd.connected())
    { // already connected? quit
        if(!loggedOnce) {
            Serial.println("connectToCEhost - connected!");
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

    Serial.print("connectToCEhost - IP: ");
    Serial.print(hostIpString.c_str());
    Serial.print(", port: ");
    Serial.println(hostPortHdd);

    // start connection attempt
    clientHdd.connect(hostIpString.c_str(), hostPortHdd);
    clientHdd.setNoDelay(true);

    // clientIkbd.connect(hostIpString.c_str(), hostPortIkbd);
}

void connectToHost(void)
{
    static bool prevConnected = false;
    connected = clientHdd.connected() && WiFi.status() == WL_CONNECTED;

    // on connected state changed
    if(prevConnected != connected)
    {
        prevConnected = connected;

        if(connected) {     // now in connected state, display state on display
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
    ceDiscoveryReceive();

    // connect to CE server
    connectToCEhost();
}

bool getIncommingHeader(NetworkClient* client, uint32_t expectedSyncTag, THeader* header)
{
    if(!client->connected())    // client not connected, no header received
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
        int available = client->available();
        if(available < needed)
        {
            return false;
        }

        uint32_t data = ((uint8_t) client->read()); // read byte
        header->syncTag = header->syncTag << 8;     // shift previous sync tag one byte up
        header->syncTag |= data;                    // add lowest byte to syncTag

        if(header->syncTag == expectedSyncTag)      // found expected syncTag
        {
            uint8_t rest[6];
            client->read(rest, 6);               // read rest of header
            header->cmdCode = getWord(rest);
            header->len = getDword(rest + 2);

            return true;                        // got complete header
        }
    }

    return false;       // no valid header
}

void handleAcsiConfig(uint32_t len)
{
    uint8_t data[32];
    memset(data, 0, 32);
    uint32_t readLen = MIN(32, len);        // limit read length to buffer length

    clientHdd.read(data, len);              // read data

    for(int i=0; i<readLen; i++)            // go through all the received data
    {
        if(data[i] == CMD_ACSI_CONFIG) {
            uint8_t newAcsiIds = data[i + 1];
            i++;    // move 1 more byte forward, as we've read it

            // check if new config different from previous, then write settings
            if(enabledIDs != newAcsiIds) {
                enabledIDs = newAcsiIds;

                Serial.print("handleAcsiConfig - storing new ids: ");
                Serial.print(newAcsiIds, HEX);
                Serial.println("");

                preferences.begin("acsi", PREFERENCES_RW_MODE);
                preferences.putUChar("ids", newAcsiIds);
                preferences.end();

                showRunningStateOnDisplay();
            }
        }
    }
}

void handleSendStatus(void)
{
    statusByte = clientHdd.read();      // read the status byte
    state = STATE_READ_STATUS;          // transition to READ STATUS state
}

void handleReadStart(bool withStatus)
{
    uint8_t data[4];
    memset(data, 0, 4);
    clientHdd.read(data, 4);

    dataCnt = get24bits(data);
    statusByte = data[3];

    state = withStatus ? STATE_DATA_READ_WITH_STATUS : STATE_DATA_READ_WITHOUT_STATUS;
}

void handleWriteStart(void)
{
    uint8_t data[4];
    memset(data, 0, 4);
    clientHdd.read(data, 4);

    dataCnt = get24bits(data);
    statusByte = data[3];

    state = STATE_DATA_WRITE;
}

void handleReadDataReceived(void)
{
    dataCnt = hddHeader.len;
    dataReceived = true;
}

void handleIncommingData(void)
{
    if(getIncommingHeader(&clientHdd, SYNC_TAG_HDD, &hddHeader))   // if got valid hdd header
    {
        dataReceived = false;
        hddHeader.syncTag = 0;      // clear sync tag

        switch(hddHeader.cmdCode) {
            case CMD_ACSI_CONFIG: handleAcsiConfig(hddHeader.len); break;
            case CMD_DATA_READ_WITH_STATUS: handleReadStart(true); break;
            case CMD_DATA_READ_WITHOUT_STATUS: handleReadStart(false); break;
            case CMD_DATA_MARKER: handleReadDataReceived(); break;
            case CMD_DATA_WRITE: handleWriteStart(); break;
            case CMD_SEND_STATUS: handleSendStatus(); break;
            default: Serial.print("unknown cmdCode "); Serial.println(hddHeader.cmdCode); break;
        }
    }
}

/*
    Send data as is to host using the desired socket.
    This just selects the right socket and sends the count of data specified in dataSizeBytes.
    @param whichSock SOCK_HDD or SOCK_FDD
    @param bfr Pointer to start of the data buffer
    @param dataSizeBytes Size of the data you want to send.
*/
bool sendDataToHost(uint8_t whichSock, uint8_t *bfr, uint32_t dataSizeBytes)
{
    NetworkClient* client = NULL;

    switch(whichSock)
    {
        case SOCK_HDD: client = &clientHdd; break;
        case SOCK_FDD: client = &clientFdd; break;
        case SOCK_IKBD: client = &clientIkbd; break;
        default: return false;
    }

    if(!client->connected())
    {
        return false;
    }

    uint32_t writtenCount = client->write(bfr, dataSizeBytes);
    return (writtenCount == dataSizeBytes);
}

/*
    Send header and data to host using the desired socket.
    This is just extension to sendDataToHost, because it also stores the data size in the header and sends the header, too.
    @param whichSock SOCK_HDD or SOCK_FDD
    @param bfr Pointer to start of the data buffer
    @param dataSizeBytes Size of the data portion after the header (header is TX_HEADER_SIZE bytes big) in bytes
*/
bool sendHeaderAndDataToHost(uint8_t whichSock, uint8_t *bfr, uint32_t dataSizeBytes)
{
    storeDword(bfr + 6, dataSizeBytes);     // store the tx length on index 6..9
    return sendDataToHost(whichSock, bfr, TX_HEADER_SIZE + dataSizeBytes);
}
