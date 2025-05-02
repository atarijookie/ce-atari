#include "WiFi.h"
#include <Preferences.h>

#include "defs.h"
#include "connection.h"
#include "utils.h"

#include "settings_for_development.h"

extern Preferences preferences;

bool wifiSettingsLoaded;
String ssid;
String password;

#define SERVER_UDP_PORT 7200 // port number where CE listens for client requests
#define CLIENT_UDP_PORT 7201 // port where this client should listen for CE responses

NetworkUDP udp;
bool udpInitialized;

NetworkClient clientHdd;
NetworkClient clientIkbd;

uint8_t hostIp[4];
String hostIpString;
uint16_t hostPortHdd;
uint16_t hostPortFdd;
uint16_t hostPortIkbd;

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

    // read wifi settings if they aren't loaded yet
    if (!wifiSettingsLoaded)
    {
        wifiSettingsLoaded = true;

        preferences.begin("credentials", PREFERENCES_RO_MODE);
        ssid = preferences.getString("ssid", ""); 
        password = preferences.getString("password", "");
        preferences.end();

        SET_SETTINGS_FOR_DEVELOPMENT(ssid, password, hostIp, hostIpString, hostPortHdd, hostPortFdd, hostPortIkbd);
    }

    // no wifi SSID stored? cannot connect
    if (ssid.length() == 0)
    {
        return;
    }

    // not connected to wifi yet, try to connect
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
}

// Send broadcast to find any CE server on the network.
void ceDiscoverySend(void)
{
    static uint32_t lastAttempt = 0xffff0000; // -65k

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

    // get local ip and mask, create broadcast ip
    IPAddress ip = WiFi.localIP();
    uint32_t ip32 = (((uint32_t)ip[0]) << 24) | (((uint32_t)ip[1]) << 16) | (((uint32_t)ip[2]) << 8) | (((uint32_t)ip[3]));

    IPAddress mask = WiFi.subnetMask();
    uint32_t mask32 = (((uint32_t)mask[0]) << 24) | (((uint32_t)mask[1]) << 16) | (((uint32_t)mask[2]) << 8) | (((uint32_t)mask[3]));
    uint32_t mask32inv = ~mask32;

    uint32_t ip32broadcast = ip32 | mask32inv; // create broadcast addr by setting all subnet bits to 1
    IPAddress addrBroadcast(ip32broadcast);    // from uint32_t to object

    // send upd broadcast
    uint8_t updPacket[4];
    strcpy((char *)updPacket, "CELC");

    // broadcast to subnet devices (e.g. 192.168.1.255)
    udp.beginPacket(addrBroadcast.toString().c_str(), SERVER_UDP_PORT);
    udp.write(updPacket, 4);
    udp.endPacket();

    // broadcast to all possible devices (255.255.255.255)
    udp.beginPacket("255.255.255.255", SERVER_UDP_PORT);
    udp.write(updPacket, 4);
    udp.endPacket();
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
    }
}

void connectToCEhost(void)
{
    static uint32_t lastAttempt = 0xffff0000; // -65k

    if (clientHdd.connected())  // && clientIkbd.connected())
    { // already connected? quit
        return;
    }

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

    // start connection attempt
    clientHdd.connect(hostIpString.c_str(), hostPortHdd);
    // clientIkbd.connect(hostIpString.c_str(), hostPortIkbd);
}

void connectToHost(void)
{
    // not connected to wifi? try to connect, skip the rest
    if (WiFi.status() != WL_CONNECTED)
    {
        connectToWifi();
        return;
    }

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
        if(client->available() < 10)    // not enough data for full header, no header received
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
            header->atnCode = getWord(rest);
            header->len = getDword(rest + 2);

            return true;                        // got complete header
        }
    }

    return false;       // no valid header
}

THeader hddHeader;      // keep the hader global to preserve syncTag between calls

void handleIncommingData(void)
{
    if(getIncommingHeader(&clientHdd, SYNC_TAG_HDD, &hddHeader))   // if got valid hdd header
    {

    }

}
