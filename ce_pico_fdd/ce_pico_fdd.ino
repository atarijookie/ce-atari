#include "WiFi.h"
// #include <SPI.h>

#include "defs.h"
#include "utils.h"
#include "connection.h"
#include "captive_portal.h"
#include "display.h"
#include "ikbd.h"
#include "buttons.h"

/*
    Arduino IDE: 2.3.6
    Board: 'Raspberry Pi Pico 2W'
*/

uint16_t version[2] = {0xf025, 0x0616}; // this means: Franz, 2025-06-16
uint8_t atnSendFwVersion[ATN_SENDFWVERSION_LEN_TX];
uint8_t atnSendTrackRequest[ATN_SENDTRACK_REQ_LEN_TX]; 
uint8_t atnSendWholeImageRequest[TX_HEADER_SIZE];

extern volatile bool ikbdEnabled;   // if true, should send data to host; otherwise just loopback ikdb data back

void setupAtnBuffers(void);
void requestTrack(uint8_t side, uint8_t track);
void requestWholeImage(void);

SStreamed posStreamed, hwPosition, posWritten;

extern bool connected;

SingleTrack tracks[2 * MAX_TRACKS];
int imageState = IMAGE_NOT_LOADED;
uint8_t imgTracks, imgSides, imgSectorsPerTrack;
char imageFileName[32];
bool diskChanged = false;
uint32_t diskChangeEnd;
uint32_t dataIndexInTrack = STREAM_START_OFFSET;

#define WRPOS_SIZE  4
uint8_t wrPosCnt, wrPosStore, wrPosLoad;
uint16_t writePos[WRPOS_SIZE];
uint32_t lastPosPutTime;

void wrPosClear(void)
{
    wrPosCnt = 0;
    wrPosStore = 0;
    wrPosLoad = 0;
}

void wrPosPut(uint8_t side, uint8_t track, uint8_t sector)
{
    // buffer full? don't store
    if(wrPosCnt >= WRPOS_SIZE) {
        return;
    }

    // pack track, side, sector into single uint16_t
    uint16_t sideTrack = track | ((side != 0) ? 0x80 : 0);
    uint16_t sideTrackSector = (sideTrack << 8) | sector;

    // store and update storing position
    writePos[wrPosStore] = sideTrackSector;
    wrPosStore++;

    if(wrPosStore >= WRPOS_SIZE) {
        wrPosStore = 0;
    }

    wrPosCnt++;
}

void wrPosGet(uint8_t* pSideTrack, uint8_t* pSector)
{
    // got anything stored? get it
    if(wrPosCnt > 0) {
        uint16_t sideTrackSector = writePos[wrPosLoad];
        wrPosLoad++;

        if(wrPosLoad >= WRPOS_SIZE) {
           wrPosLoad = 0;
        }

        wrPosCnt--;

        *pSideTrack = (uint8_t) (sideTrackSector >> 8);
        *pSector = (uint8_t) sideTrackSector;
    }
    // nothing stored? return zeros
    else
    {
        *pSideTrack = 0;
        *pSector = 0;
    }
}

TWriteBuffer wrBuffer;  // buffer for written sectors

// interrupt handler for STEP signal
void /*IRAM_ATTR*/ floppyStepISR(void)
{
    static uint32_t lastStepTime = 0;

    uint32_t now = millis();

    if((now - lastStepTime) < 1) {  // last step ISR was less than 2 ms ago? this is a glitch, ignore it
        return;
    }
    lastStepTime = now;

    if(BIT_IS_H(PIN_MOT_EN)) {       // motor not enabled? Skip the following code.
        return;
    }

    if(BIT_IS_H(PIN_DIR)) {  // direction is High? track--
        if(hwPosition.track > 0) {
            hwPosition.track--;
        }
    } else  {                // direction is Low? track++
        if(hwPosition.track < 85) {
            hwPosition.track++;
        }
    }

    if(hwPosition.track == 0) {   // if track is 0, TRACK00 is L
        BIT_CLR(PIN_TRACK00);
    } else {                        // if track is not 0, TRACK00 to H
        BIT_SET(PIN_TRACK00);
    }
}

void setup(void)
{
    Serial.begin(115200);   // uart0 for debug strings
/*  // TODO:
    Serial1.begin(7812, SERIAL_8N1, PIN_KEYB_TX_ORIG, PIN_KEYB_TX); // uart1 for IKBD - RXD PIN, TXD PIN
    Serial2.begin(7812, SERIAL_8N1, PIN_KEYB_RX, PIN_TXD2);         // uart2 for IKBD - RXD PIN, TXD PIN
*/
    Serial.println("setup() starting");

    #define INPUTS_COUNT 9
    int inputs[INPUTS_COUNT] = {PIN_SDA, PIN_DRIVE_SEL, PIN_MOT_EN, PIN_DIR, PIN_STEP, PIN_WGATE, PIN_SIDE1, PIN_MFM_RXE};

    for (int i = 0; i < INPUTS_COUNT; i++)
    {
        pinMode(inputs[i], INPUT);
    }

    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);    // boot pin needs pullup enabled

    #define OUTPUTS_COUNT 8
    int outputs[OUTPUTS_COUNT] = {PIN_SCL, PIN_DENSITY, PIN_INDEX, PIN_TRACK00, PIN_WPROTECT, PIN_DSKCHG, PIN_FLCC_OE, PIN_CS};
    int levels[OUTPUTS_COUNT]  = {    LOW,         LOW,       LOW,         LOW,         HIGH,        LOW,        HIGH,   HIGH};

    for (int i = 0; i < OUTPUTS_COUNT; i++)
    {
        pinMode(outputs[i], OUTPUT);
        digitalWrite(outputs[i], levels[i]);
    }

    /*
    uint32_t psramSize = ESP.getPsramSize();
    Serial.print("Found PSRAM: ");
    Serial.println(psramSize / 1024);

    if(psramSize < (2 * MAX_TRACKS * READTRACKDATA_SIZE_BYTES)) {
        Serial.println("Not enough PSRAM, will fail to work! HALT!");
        while(1);
    }

    // allocate and init tracks
    for(int trackNo=0; trackNo<MAX_TRACKS; trackNo++) {
        for(int sideNo=0; sideNo<2; sideNo++) {
            int index = trackNo*2 + sideNo;
            tracks[index].loaded = false;
            tracks[index].track = trackNo;
            tracks[index].side = sideNo;
            tracks[index].data = (uint8_t*) ps_malloc(READTRACKDATA_SIZE_BYTES);

            if(tracks[index].data == NULL) {
                Serial.println("ps_malloc() failed! HALT!");
                while(1);
            }
        }
    }

    // do a short PSRAM check
    tracks[0].data[0] = 0xab;
    tracks[(2 * MAX_TRACKS) - 1].data[0] = 0xcd;

    if(tracks[0].data[0] == 0xab && tracks[(2 * MAX_TRACKS) - 1].data[0] == 0xcd) {
        Serial.println("PSRAM used and working");
    } else {
        Serial.println("PSRAM not working correctly! HALT");
        while(1);
    }
    */

    readTrackData_goToStart();

    ikbdEnabled = getIkbdEnabled();

    setupAtnBuffers();
    wrPosClear();

    createIkbdTask();   // this task sends ikdb data to host and back
    displayInit();

    // SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
    // SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));

    // attachInterrupt(PIN_STEP, floppyStepISR, FALLING);       // TODO:
}

void requestTrack(uint8_t side, uint8_t track)
{
    atnSendTrackRequest[TX_HEADER_SIZE + 0] = side;
    atnSendTrackRequest[TX_HEADER_SIZE + 1] = track;
    sendHeaderAndDataToHost(atnSendTrackRequest, ATN_SENDTRACK_REQ_LEN_TX - TX_HEADER_SIZE);
}

void requestWholeImage(void)
{
    Serial.println("requestWholeImage");
    sendHeaderAndDataToHost(atnSendWholeImageRequest, 0);
}

uint32_t timeTrackStart;

void readTrackData_goToStart(void)
{
    dataIndexInTrack = STREAM_START_OFFSET;     // stream index to start
    timeTrackStart = millis();                  // time of track start to now
}

void getMfmDataToBuffer(uint8_t* bfr, int len)
{
    static int prevTrackIndex = 255;

    // update SIDE var
    hwPosition.side = BIT_IS_H(PIN_SIDE1) ? 0 : 1; // get the current SIDE

    // get current track and side we should be streaming, limit them to maximum values
    int trackNo = MIN(hwPosition.track, MAX_TRACKS);
    int sideNo = MIN(hwPosition.side, 1);

    // find out from track and side vars which track we should stream, then in that track find the pointer to next position
    int trackIndex = trackNo * 2 + sideNo;                       // track + side create index into tracks array

    if(prevTrackIndex != trackIndex) {      // track or side changed? restart stream
        readTrackData_goToStart();
    }
    prevTrackIndex = trackIndex;

    uint8_t* pTrackData = &tracks[trackIndex].data[dataIndexInTrack];  // copy data from here
    uint8_t* pTrackDataEnd = &tracks[trackIndex].data[READTRACKDATA_SIZE_BYTES - 1];

    memset(bfr, 0x55, len);                // init all values to 0x55

    for(int i=0; i<len; ) {
        uint8_t val = *pTrackData;

        // end of array or end-of-track marker? we've at the end, don't copy anything more
        if(pTrackData >= pTrackDataEnd || val == CMD_TRACK_STREAM_END) {
            break;
        }

        pTrackData++;

        // skip empty bytes
        if(val == 0) {
            continue;
        }

        // skip data part of the sector marker
        if(val == CMD_DATA_PART_OF_SECTOR) {
            continue;
        }

        // current sector marker?
        if(val == CMD_CURRENT_SECTOR) {
            posStreamed.side = *pTrackData++;
            posStreamed.track = *pTrackData++;
            posStreamed.sector = *pTrackData++;
            continue;
        }

        // val is just mfm data, store it
        bfr[i] = val;
        i++;
    }

    // now we got 'len' bytes in the bfr, we just need to update data retrieval index
    uint8_t* pTrackDataStart = tracks[trackIndex].data;
    dataIndexInTrack = pTrackData - pTrackDataStart;
}

void logFailedToWrite(uint8_t side, uint8_t track, uint8_t sector)
{
    Serial.print("Failed to write side: ");
    Serial.print(side);
    Serial.print(", track: ");
    Serial.print(track);
    Serial.print(", sector: ");
    Serial.print(sector);
}

void storeWrittenSectorDataToTrackLocally(uint8_t side, uint8_t track, uint8_t sector, uint8_t* bfr, int len)
{
    // track / side / sector out of bounds?
    if(side > 1 || track >= MAX_TRACKS || sector >= 12) {
        logFailedToWrite(side, track, sector);
        Serial.println(" - TriSiSe out of bounds!");
        return;
    }

    // get pointer to start and end of track
    int trackIndex = track * 2 + side;            // track + side create index into tracks array
    uint8_t* pData = &tracks[trackIndex].data[0];
    uint8_t* pDataEnd = &tracks[trackIndex].data[READTRACKDATA_SIZE_BYTES - 1];

    uint8_t* pStreamTableItem = pData + (sector * 2);       // calc pointer to where the offset to sector is - in the stream table
    uint32_t offsetToSector = getWord(pStreamTableItem);   // get offset to sector in stream from stream table

    // make sure that offset to sector is still within the track data
    if(offsetToSector >= READTRACKDATA_SIZE_BYTES) {
        logFailedToWrite(side, track, sector);
        Serial.println(" - bad sector offset!");
        return;
    }

    // advance pointer to start of the sector - this address should contain CMD_CURRENT_SECTOR
    pData += offsetToSector;
    uint8_t* pSectorEnd = pData + ENCODED_SECTOR_MAX_SIZE - 1;  // end of sector should be exactly here, because using fixed encoded sector size

    uint8_t* pSectorDataStart = NULL;

    #define HEADER_START    0
    #define DATA_START      1
    #define NOTHING         3

    uint8_t lookingFor = HEADER_START;      // look for header, then for data start, then for data end

    // find start and end of the sector we want to overwrite
    while(pData < pDataEnd) {
        uint8_t val = *pData;

        // if data is not one of the few tags we expect, just move to next data and don't check the rest of the code
        if(val != CMD_CURRENT_SECTOR && val != CMD_DATA_PART_OF_SECTOR && val != CMD_TRACK_STREAM_END) {
            pData++;
            continue;
        }

        // we're looking for header start and this is the current_sector mark
        if(lookingFor == HEADER_START && val == CMD_CURRENT_SECTOR) {
            if(pData[1] == side && pData[2] == track && pData[3] == sector) {   // side + track + sector match? found header!
                lookingFor = DATA_START;    // header found, look for data start
            }
            pData += 4; // skip the current_sector mark - to continue after it
            continue;
        }

        // if we already found the header, but not the sector data start, and this is start of the data, we've found start now
        if(lookingFor == DATA_START && val == CMD_DATA_PART_OF_SECTOR) {
            pData++;                    // skip beyond marker
            pSectorDataStart = pData;   // store pointer to sector data start
            lookingFor = NOTHING;       // data start found, look for data end
            break;
        }
    }

    // if we didn't find everything needed, fail
    if(lookingFor != NOTHING) {
        logFailedToWrite(side, track, sector);
        Serial.println(" - no data start found");
        return;
    }

    int dataSizeBytes = pSectorEnd - pSectorDataStart;  // how many bytes are allocated in buffer for the sector data (from end to start) - should be about 1100 bytes
    int copySize = MIN(len, dataSizeBytes);         // pick smaller of these sizes, so we won't overwrite the start of next sector

    int dataSizeToClear = dataSizeBytes - copySize; // how many bytes should be cleared 

    memcpy(pData, bfr, copySize);                   // copy new data at the start of data part of the sector

    if(dataSizeToClear > 0) {
        memset(pData + len, 0, dataSizeToClear);    // clear the bytes up to end of sector data
    }
}

void processMfmWriteBuffer(uint8_t* bfr, int len)
{
    static bool receivingWriteData = false;

    for(int i=0; i<len; i++)
    {
        uint8_t val = bfr[i];

        if(val == TAG_WRITE_START) {    // on START tag found - now we're receiving write data
            // Serial.println("START");
            receivingWriteData = true;
            wrBuffer.count = 12;        // start with 12 bytes in the buffer - 10 for header, 2 for track + side + sector
            continue;
        }

        if(val == TAG_WRITE_END) {      // on END tag found - we're no longer receiving write data
            if(receivingWriteData) {    // this END tag is the first END tag found after START tag, so we're done with receiving this sector
                receivingWriteData = false;     //  not receiving anymore

                uint8_t sideTrack, sector;
                wrPosGet(&sideTrack, &sector);  // get side, track and sector number from the write-positions buffer
                wrBuffer.buffer[10] = sideTrack;
                wrBuffer.buffer[11] = sector;

                posWritten.side = ((sideTrack) & 0x80) ? 1 : 0;
                posWritten.track = sideTrack & 0x7f;
                posWritten.sector = sector;

                // Serial.println("END");
                // Serial.println(wrBuffer.count);

                sendHeaderAndDataToHost(wrBuffer.buffer, wrBuffer.count - TX_HEADER_SIZE);      // send sector to host
                storeWrittenSectorDataToTrackLocally(((sideTrack & 0x80) ? 1 : 0), (sideTrack & 0x7f), sector, wrBuffer.buffer, wrBuffer.count);
            }

            continue;
        }

        // we're not between START and END tag? ignore the data
        if(!receivingWriteData) {
            continue;
        }

        // buffer not full and the value is not a zero? store
        if(wrBuffer.count < WRITEBUFFER_SIZE) {
            // Serial.print(val, HEX);
            // Serial.print(" ");

            wrBuffer.buffer[wrBuffer.count] = val;
            wrBuffer.count++;
        }
    }
}

void refillMfmStreamer(void)
{
    #define SPI_CHUNK_SIZE  256

    uint8_t bufferOut[SPI_CHUNK_SIZE];
    uint8_t bufferIn[SPI_CHUNK_SIZE];

    // get stream data into buffer - duration: 24 us
    getMfmDataToBuffer(bufferOut, SPI_CHUNK_SIZE);

    // send data over SPI to mfm streamer - duration 262 us when SPI running at 8 MHz
    digitalWrite(PIN_CS, LOW);
    // SPI.transferBytes(bufferOut, bufferIn, SPI_CHUNK_SIZE);
    digitalWrite(PIN_CS, HIGH);

    // check bufferIn for any data, process non-zero bytes (sector written data) between start and stop tags
    processMfmWriteBuffer(bufferIn, SPI_CHUNK_SIZE);
}

void BIT_INVERT(int pin)
{
    if(pin < 32) {
        if(BIT_IS_H(pin)) {
            BIT_CLR(pin);
        } else {
            BIT_SET(pin);
        }
    }
    else
    {
        if(BIT_IS_H(pin)) {
            BIT_CLR(pin);
        } else {
            BIT_SET(pin);
        }
    }
}

uint32_t lastSendFwTime;

void sendFwReport(uint32_t now)
{
    lastSendFwTime = now;
    sendHeaderAndDataToHost(atnSendFwVersion, ATN_SENDFWVERSION_LEN_TX - TX_HEADER_SIZE);
}

void loop(void)
{
    lastSendFwTime = millis();
    timeTrackStart = millis();

    int WGatePrev = HIGH;

    while(1)
    {
        // connect to wifi, discover CE server, connect to CE server
        connectToHost();

        // handle any data incoming
        handleIncommingData();

        bool stWantsTheStream = BIT_IS_L(PIN_DRIVE_SEL) && BIT_IS_L(PIN_MOT_EN);

        // send heartbeat (fw version) once a second
        uint32_t now = millis();
        if (connected && (now - lastSendFwTime) >= 1000)
        {
            // if(stWantsTheStream) {
            //     hwPosition.side = BIT_IS_H(PIN_SIDE1) ? 0 : 1; // get the current SIDE

            //     Serial.print("S ");
            //     Serial.print(hwPosition.side);
            //     Serial.print(" ");
            //     Serial.println(hwPosition.track);
            // }

            sendFwReport(now);
        }

        // request whole image if no image loaded
        if(connected && imageState == IMAGE_NOT_LOADED)
        {
            // before requesting the whole image, send fw report, so the host will get mac address, 
            // which he will use to identify this device
            sendFwReport(now);

            // now do the image request
            imageState = IMAGE_REQUESTED;
            requestWholeImage();
        }

        // ST wants the stream? ENABLE stream
        if(stWantsTheStream) {
            BIT_CLR(PIN_FLCC_OE);
        } else {    // other cases? DISABLE stream
            BIT_SET(PIN_FLCC_OE);
        }

        // if the mfm streamer needs more data
        if(digitalRead(PIN_MFM_RXE) == HIGH) {
            refillMfmStreamer();
        }

        // when disk change happened
        if(diskChanged) {
            diskChanged = false;        // no change anymore

            BIT_INVERT(PIN_DENSITY);    // invert pins
            BIT_INVERT(PIN_WPROTECT);
            BIT_INVERT(PIN_DSKCHG);

            diskChangeEnd = now + 1000; // at this upcomming time invert back
            // Serial.println("DSK CHG start");
        }

        // after enough time passed since the disk change, need to invert pins back
        if(diskChangeEnd != 0 && (now >= diskChangeEnd)) {
            diskChangeEnd = 0;          // set this var to zero, so we don't do this until next disk change

            BIT_INVERT(PIN_DENSITY);    // invert pins
            BIT_INVERT(PIN_WPROTECT);
            BIT_INVERT(PIN_DSKCHG);
            // Serial.println("DSK CHG end");
        }

        //-------------------------------------------------
        now = millis();

        if(stWantsTheStream)
        {
            int WGateNow = BIT_LEVEL(PIN_WGATE);

            if(WGatePrev != WGateNow)   // write gate changed?
            {
                WGatePrev = WGateNow;

                if(WGateNow == LOW)     // on write start
                {
                    posStreamed.side = BIT_IS_H(PIN_SIDE1) ? 0 : 1;                // get the current SIDE
                    wrPosPut(posStreamed.side, posStreamed.track, posStreamed.sector);   // store side, track, sector into write positions buffer
                    lastPosPutTime = now;
                }
            }
        }

        // if passed enough time to get the written data from the mfm streamer, but this hasn't happened and 
        // we still have some positions stored in the wrPos FIFO, clear it and log a message
        if((now - lastPosPutTime) > 50 && wrPosCnt > 0) {
            Serial.print("Probably missed ");
            Serial.print(wrPosCnt);
            Serial.println(" written sector(s)");

            wrPosClear();       // clear the written positions, they are probably useless now
        }

        //------------
        uint32_t timeSinceTrackStart = now - timeTrackStart;

        if(timeSinceTrackStart <= 195) {  // INDEX is H for time 0-195
            BIT_SET(PIN_INDEX);
        } else {                         // INDEX is H for times 196-200
            BIT_CLR(PIN_INDEX);
        }

        if(timeSinceTrackStart >= 200) {    // track finished
            readTrackData_goToStart();      // move the pointer in the track stream to start
        }

        //---------------------------
        // check the button state and press duration
        handleAllButtons();
    }
}

void setupAtnBuffers(void)
{
    // firmware report / keep alive / heartbeat
    memset(atnSendFwVersion, 0, ATN_SENDFWVERSION_LEN_TX);
    storeHeader(atnSendFwVersion, ATN_FW_VERSION, 0);
    storeWord(atnSendFwVersion + TX_HEADER_SIZE, version[0]);
    storeWord(atnSendFwVersion + TX_HEADER_SIZE + 2, version[1]);

    // one track request
    memset(atnSendTrackRequest, 0, ATN_SENDTRACK_REQ_LEN_TX);
    storeHeader(atnSendTrackRequest, ATN_SEND_TRACK, 2);

    // all tracks request
    storeHeader(atnSendWholeImageRequest, ATN_SEND_WHOLE_IMAGE, 0);

    // configure write buffers
    storeHeader(wrBuffer.buffer, ATN_SECTOR_WRITTEN, 12);
    wrBuffer.count = 12;
}

void storeMacAddress(void)
{
    memset(atnSendFwVersion + TX_HEADER_SIZE + 6, 0, 6);
    // TODO:
    // WiFi.STA.macAddress(atnSendFwVersion + TX_HEADER_SIZE + 6);

    char msg[128];
    sprintf(msg, "mac: %02X:%02X:%02X:%02X:%02X:%02X", atnSendFwVersion[TX_HEADER_SIZE + 6], atnSendFwVersion[TX_HEADER_SIZE + 7], atnSendFwVersion[TX_HEADER_SIZE + 8], 
                                                       atnSendFwVersion[TX_HEADER_SIZE + 9], atnSendFwVersion[TX_HEADER_SIZE + 10], atnSendFwVersion[TX_HEADER_SIZE + 11]);
    Serial.println(msg);
}
