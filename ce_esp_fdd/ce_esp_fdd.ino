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

extern volatile bool ikbdEnabled;   // if true, should send data to host; otherwise just loopback ikdb data back

void handleButton(void);

TOutputFlags outFlags;
volatile uint32_t lastRequestTime;
volatile TDrivePosition drivePos;
int trackStreamedCount;
SStreamed streamed, streamingNow, streamedPrev;
uint8_t sectorsWritten;

bool sendrequestTrack;
uint32_t prevIntTime;
extern bool connected;

#define IMAGE_NOT_LOADED    0
#define IMAGE_REQUESTED     1
#define IMAGE_LOADED        2

typedef struct {
    bool loaded;
    int track;
    int side;
    uint8_t* data;
} SingleTrack;

SingleTrack tracks[2 * MAX_TRACKS];
int imageState = IMAGE_NOT_LOADED;

uint8_t singleTrackData[READTRACKDATA_SIZE_BYTES];      // TODO: remove this once the tracks.data is properly allocated from PSRAM

uint8_t *readTrackDataBfr;

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

    createIkbdTask();   // this task sends ikdb data to host and back
    displayInit();
}

void requestTrack(uint8_t forceRequest) 
{
    // drivePos

    sendrequestTrack = true;
    outFlags.weAreReceivingTrack = true;        // mark that we are receiving TRACK data, and thus shouldn't stream
}

void requestAllTracks(void)
{

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

        if(connected && imageState == IMAGE_NOT_LOADED)
        {
            imageState = IMAGE_REQUESTED;
            requestAllTracks();
        }

        if(outFlags.updatePosition) {
            outFlags.updatePosition = false;
            updateStreamPositionByFloppyPosition(timeTrackStart);     // place the read marker on the right place in the stream
        }

        outFlags.stWantsTheStream = BIT_IS_L(PIN_DRIVE_SEL) && BIT_IS_L(PIN_MOT_EN);

        // ST wants the stream and we are not receiving TRACK data? ENABLE stream
        if(outFlags.stWantsTheStream && !outFlags.weAreReceivingTrack) {
            if(!outFlags.outputsAreEnabled) {           // the outputs are not enabled yet?
                BIT_CLR(PIN_FLCC_OE);                   // enable them
                outFlags.outputsAreEnabled = true;      // mark that we enabled them
            }
        } else {    // other cases? DISABLE stream
            if(outFlags.outputsAreEnabled) {            // the outputs are enabled?
                BIT_SET(PIN_FLCC_OE);                   // disable them
                outFlags.outputsAreEnabled = false;     // mark that we disabled them
            }
        }

        // if(wrNow->readyToSend) {                                                     // not sending any ATN right now? and current write buffer has something?
        //     spiDma_txRx(wrNow->count, wrNow->buffer, 1, &fakeBuffer);

        //     wrNow->readyToSend  = false;                                                    // mark the current buffer as not ready to send (so we won't send this one again)

        //     wrNow               = wrNow->next;                                              // and now we will select the next buffer as current
        //     wrNow->readyToSend  = false;                                                    // the next buffer is not ready to send (yet)
        //     wrNow->count        = 4;                                                        // at the start we already have 4 uint16_ts in buffer - SYNC, ATN code, TX len, RX len
        // }

        //-------------------------------------------------

        // WGate = inputs & WGATE;                                         // get current WGATE value

        // if(WGate == 0) {                                                // when write gate is low, the data is written to floppy
        //     handleFloppyWrite();
        //     sectorsWritten++;                                       // one sector was written, request updated track at the end of stream
        // }

        //------------
        uint32_t now = millis();
        uint32_t timeSinceTrackStart = now - timeTrackStart;

        if(timeSinceTrackStart >= 5) {      // track time: 5m - rest - INDEX to H
            BIT_SET(PIN_INDEX);
        }

        if(timeSinceTrackStart >= 200) {    // track finished
            BIT_CLR(PIN_INDEX);             // INDEX to L
            timeTrackStart = millis();

            readTrackData_goToStart();      // move the pointer in the track stream to start

            if(sectorsWritten > 0) {        // if some sectors were written to floppy, we need to get the new stream now
                sectorsWritten = 0;         // nothing written now
                requestTrack(true);         // ask for the changed track data, but force it - get it immediatelly
            }

            //-----------
            // the following section of code should request track again if even after 2 rotations of floppy we're not streaming what we should
            trackStreamedCount++;               // increment the count of how many times we've streamed this track

            if(trackStreamedCount >= 2) {       // if since the last request 2 rotations happened
                if(streamed.track != streamingNow.track || streamed.side != streamingNow.side) {  // and we're not streaming what we really want to stream
                    requestTrack(false);        // ask for track data (again?)
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
            requestTrack(false);                // we need track from the right side
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
