#include "WiFi.h"
#include <Preferences.h>

#include "defs.h"
#include "utils.h"
#include "connection.h"
#include "captive_portal.h"
#include "display.h"
#include "ikbd.h"

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

TOutputFlags outFlags;
volatile uint32_t lastRequestTime;
volatile TDrivePosition drivePos;
int trackStreamedCount;
SStreamed streamed, streamingNow, streamedPrev;
uint8_t sectorsWritten;

uint32_t prevIntTime;
extern bool connected;

SingleTrack tracks[2 * MAX_TRACKS];
int imageState = IMAGE_NOT_LOADED;
uint8_t imgTracks, imgSides, imgSectorsPerTrack;
char imageFileName[32];
bool diskChanged;

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
        if(streamingNow.track > 0) {
            streamingNow.track--;
        }
    } else  {                // direction is Low? track++
        if(streamingNow.track < 85) {
            streamingNow.track++;
        }
    }
}

void setup(void)
{
    Serial.begin(115200);   // uart0 for debug strings
    Serial1.begin(7812, SERIAL_8N1, /* rxd pin */ PIN_KEYB_TX_ORIG, /* txd pin */ PIN_KEYB_TX); // uart1 for IKBD
    Serial2.begin(7812, SERIAL_8N1, /* rxd pin */ PIN_KEYB_RX, /* txd pin */ PIN_TXD2);         // uart2 for IKBD

    Serial.println("setup() starting");

    #define INPUTS_COUNT 8
    int inputs[INPUTS_COUNT] = {PIN_SDA, PIN_DRIVE_SEL, PIN_MOT_EN, PIN_DIR, PIN_STEP, PIN_WDATA, PIN_WGATE, PIN_SIDE1};

    for (int i = 0; i < INPUTS_COUNT; i++)
    {
        pinMode(inputs[i], INPUT);
    }

    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);

    #define OUTPUTS_COUNT 8
    int outputs[OUTPUTS_COUNT] = {PIN_SCL, PIN_DENSITY, PIN_INDEX, PIN_TRACK00, PIN_WPROTECT, PIN_RDATA, PIN_DSKCHG, PIN_FLCC_OE};

    for (int i = 0; i < OUTPUTS_COUNT; i++)
    {
        pinMode(outputs[i], OUTPUT);
    }

    // init floppy signals
    BIT_CLR(PIN_TRACK00);
    BIT_CLR(PIN_DSKCHG);
    BIT_SET(PIN_FLCC_OE);       // disable output
    BIT_SET(PIN_WPROTECT);

    // init track and side vars for the floppy position
    drivePos.side = 0;
    drivePos.track = 0;
    
    lastRequestTime = 0;

    for(int trackNo=0; trackNo<MAX_TRACKS; trackNo++) {
        for(int sideNo=0; sideNo<2; sideNo++) {
            int index = trackNo*2 + sideNo;
            tracks[index].loaded = false;
            tracks[index].track = trackNo;
            tracks[index].side = sideNo;
            // tracks[index].data = (uint8_t*) malloc(READTRACKDATA_SIZE_BYTES);        // TODO: uncomment this once the PSRAM is working
            tracks[index].data = singleTrackData;
        }
    }
    readTrackDataBfr = tracks[0].data;

    preferences.begin("ikbd", PREFERENCES_RO_MODE);
    ikbdEnabled = preferences.getUChar("enabled", 1);
    preferences.end();

    setupAtnBuffers();

    createIkbdTask();   // this task sends ikdb data to host and back
    displayInit();

    attachInterrupt(PIN_STEP, floppyStepISR, FALLING);
}

void requestTrack(uint8_t side, uint8_t track)
{
    atnSendFwVersion[TX_HEADER_SIZE + 0] = side;
    atnSendFwVersion[TX_HEADER_SIZE + 1] = track;
    sendHeaderAndDataToHost(atnSendTrackRequest, ATN_SENDTRACK_REQ_LEN_TX - TX_HEADER_SIZE);
}

void requestWholeImage(void)
{
    sendHeaderAndDataToHost(atnSendWholeImageRequest, 0);
}

void readTrackData_goToStart(void)
{

}

void loop(void)
{
    uint32_t lastSendFwTime = millis();

    uint8_t indexCount = 0;
    uint32_t timeTrackStart = millis();

    prevIntTime = 0;
    sectorsWritten = 0;         // nothing written yet

    while(1)
    {
        // connect to wifi, discover CE server, connect to CE server
        connectToHost();

        // handle any data incoming
        handleIncommingData();

        uint32_t now = millis();
        if (connected && (now - lastSendFwTime) >= 1000)
        {
            lastSendFwTime = now;
            sendHeaderAndDataToHost(atnSendFwVersion, ATN_SENDFWVERSION_LEN_TX - TX_HEADER_SIZE);
        }

        if(connected && imageState == IMAGE_NOT_LOADED)
        {
            imageState = IMAGE_REQUESTED;
            requestWholeImage();
        }

        if(outFlags.updatePosition) {
            outFlags.updatePosition = false;
            updateStreamPositionByFloppyPosition(timeTrackStart);     // place the read marker on the right place in the stream
        }

        bool stWantsTheStream = BIT_IS_L(PIN_DRIVE_SEL) && BIT_IS_L(PIN_MOT_EN);

        // ST wants the stream and we are not receiving TRACK data? ENABLE stream
        if(stWantsTheStream && !outFlags.weAreReceivingTrack) {
            BIT_CLR(PIN_FLCC_OE);
        } else {    // other cases? DISABLE stream
            BIT_SET(PIN_FLCC_OE);
        }

        if(streamingNow.track == 0) {        // if track is 0, TRACK00 is L
            BIT_CLR(PIN_TRACK00);
        } else {                    // if track is not 0, TRACK00 to H
            BIT_SET(PIN_TRACK00);
        }

        // can send this write buffer?
        if(wrNow->readyToSend) {
            uint32_t dataSize = (wrNow->count > TX_HEADER_SIZE) ? (wrNow->count - TX_HEADER_SIZE) : 0;
            sendHeaderAndDataToHost(wrNow->buffer, dataSize);
            wrNow->readyToSend = false;     // mark the current buffer as not ready to send (so we won't send this one again)

            wrNow = (TWriteBuffer*) wrNow->next;    // and now we will select the next buffer as current
            wrNow->readyToSend = false;     // the next buffer is not ready to send (yet)
            wrNow->count = 10;              // at the start we already have header there
        }

        /*
            // on write start
            wrNow->buffer[10] = streamed.track | ((streamed.side != 0) ? 0x80 : 0);
            wrNow->buffer[11] = streamed.sector;
            wrNow->count = 12;          // 10 for header, 2 for track + side + sector
        */

        //-------------------------------------------------

        // WGate = inputs & WGATE;                                         // get current WGATE value

        // if(WGate == 0) {                                                // when write gate is low, the data is written to floppy
        //     handleFloppyWrite();
        //     sectorsWritten++;                                       // one sector was written, request updated track at the end of stream
        // }

        //------------
        now = millis();
        uint32_t timeSinceTrackStart = now - timeTrackStart;

        if(timeSinceTrackStart < 5) {    // INDEX is L for time 0-4
            BIT_CLR(PIN_INDEX);
        } else {                         // INDEX is H for times 5-200
            BIT_SET(PIN_INDEX);
        }

        if(timeSinceTrackStart >= 200) {    // track finished
            BIT_CLR(PIN_INDEX);             // INDEX to L
            timeTrackStart = millis();

            readTrackData_goToStart();      // move the pointer in the track stream to start

            if(sectorsWritten > 0) {        // if some sectors were written to floppy, we need to get the new stream now
                sectorsWritten = 0;         // nothing written now
                // requestTrack(true);         // ask for the changed track data, but force it - get it immediatelly
            }

            //-----------
            // the following section of code should request track again if even after 2 rotations of floppy we're not streaming what we should
            trackStreamedCount++;               // increment the count of how many times we've streamed this track

            if(trackStreamedCount >= 2) {       // if since the last request 2 rotations happened
                if(streamed.track != streamingNow.track || streamed.side != streamingNow.side) {  // and we're not streaming what we really want to stream
                    // requestTrack(false);        // ask for track data (again?)
                }
            }
            streamed.track = 0xff;        // after the end of track mark that we're not streaming anything
            streamed.side = 0xff;
        }

        //--------
        // NOTE! Handling of STEP and SIDE only when MOTOR is ON, but the drive doesn't have to be selected and it must handle the control anyway
        if(BIT_IS_H(PIN_MOT_EN)) {             // motor not enabled? Skip the following code.
            continue;
        }

        //------------
        // update SIDE var
        streamingNow.side = BIT_IS_H(PIN_SIDE1) ? 0 : 1; // get the current SIDE
        if(streamedPrev.side != streamingNow.side) {             // side changed?
            // requestTrack(false);                // we need track from the right side
            streamedPrev.side = streamingNow.side;
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

void updateStreamPositionByFloppyPosition(uint32_t timeTrackStart)
{
    // uint32_t streamSize = readTrackDataBfr[STREAM_TABLE_OFFSET];       // get stream size (in bytes) from stream table at index 0

    // if(streamSize >= (READTRACKDATA_SIZE_BYTES - 1)) {              // if stream size is invalid (is bigger than where we store read track data)
    //     inIndexGet = STREAM_START_OFFSET;                           // just go to the start of stream
    //     return;
    // }

    // // read the current position - from 0 to 200
    // uint32_t timeSinceTrackStart = millis() - timeTrackStart;

    // // calculate index where we should place sream reading index -
    // // current position is between 0 and 200, that is from 0 to 100%, so place it between 0 and LENGTH OF STREAM position
    // inIndexGet = (streamSize * timeSinceTrackStart) / 200;
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
