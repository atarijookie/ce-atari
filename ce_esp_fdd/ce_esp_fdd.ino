#include "WiFi.h"
#include <Preferences.h>
#include <SPI.h>

#include "defs.h"
#include "utils.h"
#include "connection.h"
#include "captive_portal.h"
#include "display.h"
#include "ikbd.h"

/*
    Arduino IDE: 2.3.6
    Board: 'ESP32S3 Dev Module'
    PSRAM: OPI PSRAM
*/

Preferences preferences;

uint16_t version[2] = {0xf025, 0x0616}; // this means: Franz, 2025-06-16
uint8_t atnSendFwVersion[ATN_SENDFWVERSION_LEN_TX];
uint8_t atnSendTrackRequest[ATN_SENDTRACK_REQ_LEN_TX]; 
uint8_t atnSendWholeImageRequest[TX_HEADER_SIZE];

extern volatile bool ikbdEnabled;   // if true, should send data to host; otherwise just loopback ikdb data back

void handleButton(void);
void setupAtnBuffers(void);
void requestTrack(uint8_t side, uint8_t track);
void requestWholeImage(void);

SStreamed streamed, hwPosition;
uint8_t sectorsWritten;

extern bool connected;

SingleTrack tracks[2 * MAX_TRACKS];
int imageState = IMAGE_NOT_LOADED;
uint8_t imgTracks, imgSides, imgSectorsPerTrack;
char imageFileName[32];
bool diskChanged = false;
uint32_t diskChangeEnd;
uint32_t dataIndexInTrack = STREAM_START_OFFSET;

uint8_t singleTrackData[READTRACKDATA_SIZE_BYTES];      // TODO: remove this once the tracks.data is properly allocated from PSRAM

uint8_t *readTrackDataBfr;

TWriteBuffer wrBuffer[2];                           // two buffers for written sectors
TWriteBuffer *wrNow;

// interrupt handler for STEP signal
void IRAM_ATTR floppyStepISR(void)
{
    static uint32_t lastStepTime = 0;

    uint32_t now = millis();

    if((now - lastStepTime) < 2) {  // last step ISR was less than 2 ms ago? this is a glitch, ignore it
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
    Serial1.begin(7812, SERIAL_8N1, /* rxd pin */ PIN_KEYB_TX_ORIG, /* txd pin */ PIN_KEYB_TX); // uart1 for IKBD
    Serial2.begin(7812, SERIAL_8N1, /* rxd pin */ PIN_KEYB_RX, /* txd pin */ PIN_TXD2);         // uart2 for IKBD

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

    readTrackDataBfr = tracks[0].data;

    readTrackData_goToStart();

    preferences.begin("ikbd", PREFERENCES_RO_MODE);
    ikbdEnabled = preferences.getUChar("enabled", 1);
    preferences.end();

    setupAtnBuffers();

    createIkbdTask();   // this task sends ikdb data to host and back
    displayInit();

    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

    attachInterrupt(PIN_STEP, floppyStepISR, FALLING);
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
        if(pTrackData >= pTrackDataEnd || val == CMD_TRACK_STREAM_END_BYTE) {
            break;
        }

        pTrackData++;

        // skip empty bytes
        if(val == 0) {
            continue;
        }

        // current sector marker?
        if(val == CMD_CURRENT_SECTOR) {
            streamed.side = *pTrackData++;
            streamed.track = *pTrackData++;
            streamed.sector = *pTrackData++;
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

void processMfmWriteBuffer(uint8_t* bfr, int len)
{
    uint8_t* pStore = &wrNow->buffer[wrNow->count];

    for(int i=0; i<len; i++)
    {
        // buffer full? quit
        if(wrNow->count >= WRITEBUFFER_SIZE) {
            break;
        }

        uint8_t val = *bfr;
        bfr++;

        // non-zero value gets stored
        if(val != 0) {
           *pStore = val;   // store value
           pStore++;

           wrNow->count++;  // increment count of data in buffer
        }
    }
}

void refillMfmStreamer(void)
{
    uint8_t bufferOut[32];
    uint8_t bufferIn[32];

    while(digitalRead(PIN_MFM_RXE) == HIGH) {       // while still can send data to mfm streamer
        // get stream data into buffer
        getMfmDataToBuffer(bufferOut, 32);

        // send data over SPI to mfm streamer
        digitalWrite(PIN_CS, LOW);
        SPI.transferBytes(bufferOut, bufferIn, 32);
        digitalWrite(PIN_CS, HIGH);

        // // check bufferIn for any data, process non-zero bytes (sector written data)
        // processMfmWriteBuffer(bufferIn, 32);
    }
}

void BIT_INVERT(int pin)
{
    if(BIT_IS_H(pin)) {
        BIT_CLR(pin);
    } else {
        BIT_SET(pin);
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
    sectorsWritten = 0;         // nothing written yet

    while(1)
    {
        // connect to wifi, discover CE server, connect to CE server
        connectToHost();

        // handle any data incoming
        handleIncommingData();

        // send heartbeat (fw version) once a second
        uint32_t now = millis();
        if (connected && (now - lastSendFwTime) >= 1000)
        {
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

        bool stWantsTheStream = BIT_IS_L(PIN_DRIVE_SEL) && BIT_IS_L(PIN_MOT_EN);

        // ST wants the stream? ENABLE stream
        if(stWantsTheStream) {
            BIT_CLR1(PIN_FLCC_OE);
        } else {    // other cases? DISABLE stream
            BIT_SET1(PIN_FLCC_OE);
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

        // // can send this write buffer?
        // if(wrNow->readyToSend) {
        //     uint32_t dataSize = (wrNow->count > TX_HEADER_SIZE) ? (wrNow->count - TX_HEADER_SIZE) : 0;
        //     sendHeaderAndDataToHost(wrNow->buffer, dataSize);
        //     wrNow->readyToSend = false;     // mark the current buffer as not ready to send (so we won't send this one again)

        //     wrNow = (TWriteBuffer*) wrNow->next;    // and now we will select the next buffer as current
        //     wrNow->readyToSend = false;     // the next buffer is not ready to send (yet)
        //     wrNow->count = 10;              // at the start we already have header there
        // }

        //-------------------------------------------------
        // if(stWantsTheStream)
        // {
        //     int WGateNow = BIT_LEVEL(PIN_WGATE);

        //     if(WGatePrev != WGateNow)   // write gate changed?
        //     {
        //         WGatePrev = WGateNow;

        //         if(WGateNow == LOW)     // on write start
        //         {
        //             hwPosition.side = BIT_IS_H(PIN_SIDE1) ? 0 : 1; // get the current SIDE
        //             wrNow->buffer[10] = hwPosition.track | ((hwPosition.side != 0) ? 0x80 : 0);
        //             wrNow->buffer[11] = hwPosition.sector;
        //             wrNow->count = 12;          // 10 for header, 2 for track + side + sector
        //         }
        //         else                    // on write end
        //         {
        //             sectorsWritten++;   // one sector was written, request updated track at the end of stream

        //         }
        //     }
        // }

        //------------
        now = millis();
        uint32_t timeSinceTrackStart = now - timeTrackStart;

        if(timeSinceTrackStart <= 195) {  // INDEX is H for time 0-195
            BIT_SET(PIN_INDEX);
        } else {                         // INDEX is H for times 196-200
            BIT_CLR(PIN_INDEX);
        }

        if(timeSinceTrackStart >= 200) {    // track finished
            readTrackData_goToStart();      // move the pointer in the track stream to start

            // if(sectorsWritten > 0) {        // if some sectors were written to floppy, we need to get the new stream now
            //     sectorsWritten = 0;         // nothing written now
            //     // requestTrack(true);         // ask for the changed track data, but force it - get it immediatelly
            // }

            streamed.track = 0xff;        // after the end of track mark that we're not streaming anything
            streamed.side = 0xff;
            streamed.sector = 0xff;
        }

        //---------------------------
        // check the button state and press duration
        handleButton();
    }
}

#define BTN_PRESS_SHORT     500
#define BTN_PRESS_SAVE      2000
#define BTN_PRESS_CAPTIVE   5000

// This gets called on button pressed (current button state LOW) or released (current button state HIGH)
void onButtonStateChanged(int buttonState, uint32_t now, uint32_t& buttonPressTime)
{
    // button state change to low, so button just pressed - store time, nothing more to do
    if(buttonState == LOW)
    {
        buttonPressTime = now;
        return;
    }

    //-------
    // button state change to high, so button released
    uint32_t pressDuration = now - buttonPressTime;

    if(pressDuration < BTN_PRESS_SHORT)     // on short press, ikbd enable / disable
    {
        ikbdEnabled = !ikbdEnabled;
    }

    // on longer press, save ikbd enabled flag
    if(pressDuration >= BTN_PRESS_SAVE && pressDuration < BTN_PRESS_CAPTIVE)
    {
        preferences.begin("ikbd", PREFERENCES_RW_MODE);
        preferences.putUChar("enabled", ikbdEnabled);
        preferences.end();
    }

    // on longest press, run captive portal
    if(pressDuration >= BTN_PRESS_CAPTIVE)
    {
        runCaptivePortal();
    }

    showRunningStateOnDisplay();
}

// Gets called during the button is pressed down, used to show stuff on display for long press.
void duringButtonPressed(uint32_t now, uint32_t& buttonPressTime)
{
    uint32_t pressDuration = now - buttonPressTime;

    // press too short? nothing to show on display
    if(pressDuration < BTN_PRESS_SAVE)
    {
        return;
    }

    // longer press? ask about saving ikbd settings
    if(pressDuration >= BTN_PRESS_SAVE && pressDuration < BTN_PRESS_CAPTIVE)
    {
        displayMessage(NULL, "Store IKDB enabled?", NULL);
    }

    // longest press? ask about running captive portal
    if(pressDuration >= BTN_PRESS_CAPTIVE)
    {
        displayMessage(NULL, "Run captive portal?", NULL);
    }
}

// Check the button pressed / released state, check if button has been just pressed, released, 
// or is being held down. Show stuff on display, handle button actions.
void handleButton(void)
{
    static uint32_t lastCheck = millis();
    static int lastButtonState = HIGH;
    static uint32_t buttonPressTime = 0;

    uint32_t now = millis();

    if(now - lastCheck < 100)      // check for button change only every now and then
    {
        return;
    }
    lastCheck = now;

    int buttonState = digitalRead(PIN_BOOT_BTN);    // read button

    bool buttonStateChanged = (lastButtonState != buttonState);
    lastButtonState = buttonState;

    if(buttonStateChanged)      // button state changed? (e.g. pressed, released)
    {
        onButtonStateChanged(buttonState, now, buttonPressTime);
    }
    else        // button state not changed (stayed released, stayed pressed)
    {
        if(buttonState == LOW)
        {
            duringButtonPressed(now, buttonPressTime);
        }
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
    for(int i=0; i<2; i++) {
        storeHeader(wrBuffer[i].buffer, ATN_SECTOR_WRITTEN, 10);
        wrBuffer[i].count = 10;
        wrBuffer[i].readyToSend = false;
        wrBuffer[i].next = (i == 0) ? &wrBuffer[1] : &wrBuffer[0];
    }

    wrNow = &wrBuffer[0];
}

void storeMacAddress(void)
{
    WiFi.STA.macAddress(atnSendFwVersion + TX_HEADER_SIZE + 6);

    char msg[128];
    sprintf(msg, "mac: %02X:%02X:%02X:%02X:%02X:%02X", atnSendFwVersion[TX_HEADER_SIZE + 6], atnSendFwVersion[TX_HEADER_SIZE + 7], atnSendFwVersion[TX_HEADER_SIZE + 8], 
                                                       atnSendFwVersion[TX_HEADER_SIZE + 9], atnSendFwVersion[TX_HEADER_SIZE + 10], atnSendFwVersion[TX_HEADER_SIZE + 11]);
    Serial.println(msg);
}
