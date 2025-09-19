#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/timer.h"
#include "pico/cyw43_arch.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"

#include "connection.h"
#include "display.h"
#include "ikbd.h"
#include "buttons.h"
#include "mfm.h"
#include "psram.h"
#include "defs.h"
#include "utils.h"
#include "serial_config.h"

uint16_t version[2] = {0xf025, 0x0901}; // this means: Franz, 2025-09-01
uint8_t atnSendFwVersion[ATN_SENDFWVERSION_LEN_TX];
uint8_t atnSendTrackRequest[ATN_SENDTRACK_REQ_LEN_TX]; 
uint8_t atnSendWholeImageRequest[TX_HEADER_SIZE];

void setupAtnBuffers(void);
void requestTrack(uint8_t side, uint8_t track);
void requestWholeImage(void);

SStreamed posStreamed, hwPosition;

uint8_t trackData0[READTRACKDATA_SIZE_BYTES];
uint8_t trackData1[READTRACKDATA_SIZE_BYTES];

extern bool connectedToHost;
uint32_t timeTrackStart;

int imageState = IMAGE_NOT_LOADED;
uint8_t imgTracks, imgSides, imgSectorsPerTrack;
char imageFileName[32];
bool diskChanged = false;
uint32_t diskChangeEnd;
uint32_t dataIndexInTrack = STREAM_START_OFFSET;

extern Settings_t Settings;

TWriteBuffer wrBuffer;  // buffer for written sectors

// interrupt handler for STEP signal
void floppyStepISR(uint gpio, uint32_t event_mask)
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
        if(hwPosition.track < MAX_TRACKS) {
            hwPosition.track++;
        }
    }

    if(hwPosition.track == 0) {   // if track is 0, TRACK00 is L
        gpio_put(PIN_TRACK00, 0);
    } else {                        // if track is not 0, TRACK00 to H
        gpio_put(PIN_TRACK00, 1);
    }
}

void readTrackData_goToStart(void)
{
    dataIndexInTrack = STREAM_START_OFFSET;     // stream index to start
    timeTrackStart = millis();                  // time of track start to now
}

void setup(void)
{
    stdio_init_all();
    xprintf("\n\nsetup() starting\n");

    #define INPUTS_COUNT 7
    int inputs[INPUTS_COUNT] = {PIN_DRIVE_SEL, PIN_MOT_EN, PIN_DIR, PIN_STEP, PIN_WGATE, PIN_SIDE1};

    for (int i = 0; i < INPUTS_COUNT; i++)
    {
        gpio_set_dir(inputs[i], GPIO_IN);
    }

    #define OUTPUTS_COUNT 7
    int outputs[OUTPUTS_COUNT] = {PIN_DENSITY, PIN_INDEX, PIN_TRACK00, PIN_WPROTECT, PIN_DSKCHG, PIN_FLCC_OE, PIN_CS};
    int levels[OUTPUTS_COUNT]  = {          0,         0,           0,            1,          0,           1,      1};

    for (int i = 0; i < OUTPUTS_COUNT; i++)
    {
        gpio_set_dir(outputs[i], GPIO_OUT);
        gpio_put(outputs[i], 1);
    }

    displayInit();

    // SPI initialisation.
    spi_init(SPI_PORT, 16000000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    // init PSRAM, read ID, test read and write
    // psramTest();

    loadSettingsFromEeprom();

    // If the flag to enter config mode is set, or there is no ssid stored, enter the config mode.
    // In order for flashing to work, we must ensure that only 1 core is writing to flash and running, so
    // we must enter config mode and storing to flash before we call cyw43_arch_init(), 
    // which runs on other core.
    if(psramConfigFlagGet() || strlen(Settings.ssid) == 0) {
        xprintf("Starting serial config - configFlag: %d, SSID length: %d\n", psramConfigFlagGet(), strlen(Settings.ssid));
        displayMessage("...now running...", "SERIAL CONFIG");
        psramConfigFlagClear();
        serialConfigLoop();
    }

    readTrackData_goToStart();

    setupAtnBuffers();

    // start mfm output
    setupPwmOutput();
    setupDmaToPwm();

    gpio_set_irq_enabled_with_callback(PIN_STEP, GPIO_IRQ_EDGE_FALL, true, floppyStepISR);

    // Initialise UART 1
    gpio_set_function(PIN_KEYB_TX, UART_FUNCSEL_NUM(uart1, PIN_KEYB_TX));
    gpio_set_function(PIN_KEYB_TX_ORIG, UART_FUNCSEL_NUM(uart1, PIN_KEYB_TX_ORIG));
    uart_init(uart1, 7812);

/*  // TODO:
    Serial1.begin(7812, SERIAL_8N1, PIN_KEYB_TX_ORIG, PIN_KEYB_TX); // uart1 for IKBD - RXD PIN, TXD PIN
    Serial2.begin(7812, SERIAL_8N1, PIN_KEYB_RX, PIN_TXD2);         // uart2 for IKBD - RXD PIN, TXD PIN
*/
    createIkbdTask();   // this task sends ikdb data to host and back

    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) {
        xprintf("Wi-Fi init failed\n");
        while(1);
    }

    // Enable wifi station
    cyw43_arch_enable_sta_mode();
}
    
void requestTrack(uint8_t side, uint8_t track)
{
    atnSendTrackRequest[TX_HEADER_SIZE + 0] = side;
    atnSendTrackRequest[TX_HEADER_SIZE + 1] = track;
    sendHeaderAndDataToHost(atnSendTrackRequest, ATN_SENDTRACK_REQ_LEN_TX - TX_HEADER_SIZE);
}

void requestWholeImage(void)
{
    xprintf("requestWholeImage");
    sendHeaderAndDataToHost(atnSendWholeImageRequest, 0);
}

void storeWrittenSectorDataToTrackLocally(uint8_t side, uint8_t track, uint8_t sector, uint8_t* bfr, int len)
{
    // track / side / sector out of bounds?
    if(side > 1 || track >= MAX_TRACKS || sector >= 12) {
        xprintf("Failed to write side: %d, track: %d, sector: %d - TriSiSe out of bounds!\n", side, track, sector);
        return;
    }

    uint8_t* pTrackStart = (side == 0) ? trackData0 : trackData1;
    uint8_t* pDataEnd = pTrackStart + READTRACKDATA_SIZE_BYTES - 1;
    uint8_t* pData = pTrackStart;

    uint8_t* pStreamTableItem = pData + (sector * 2);       // calc pointer to where the offset to sector is - in the stream table
    uint32_t offsetToSector = getWord(pStreamTableItem);    // get offset to sector in stream from stream table

    // make sure that offset to sector is still within the track data
    if(offsetToSector >= READTRACKDATA_SIZE_BYTES) {
        xprintf("Failed to write side: %d, track: %d, sector: %d - bad sector offset!\n", side, track, sector);
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
        xprintf("Failed to write side: %d, track: %d, sector: %d - no data start found!\n", side, track, sector);
        return;
    }

    int dataSizeBytes = pSectorEnd - pSectorDataStart;  // how many bytes are allocated in buffer for the sector data (from end to start) - should be about 1100 bytes
    int copySize = MIN(len, dataSizeBytes);         // pick smaller of these sizes, so we won't overwrite the start of next sector

    int dataSizeToClear = dataSizeBytes - copySize; // how many bytes should be cleared
    dataSizeToClear = MAX(dataSizeToClear, 0);      // make sure it's not negative

    memcpy(pSectorDataStart, bfr, copySize);        // copy new data at the start of data part of the sector

    if(dataSizeToClear > 0) {
        memset(pSectorDataStart + copySize, 0, dataSizeToClear);    // clear the bytes up to end of sector data
    }

    // copy the new data to psram
    int byteOffsetFromTrackStart = pSectorDataStart - pTrackStart;  // calculate the byte offset of the written sector data from start of the track
    psramStoreSector(track, side, byteOffsetFromTrackStart, bfr, copySize, dataSizeToClear);
}

void processMfmWriteBuffer(uint8_t* bfr, int len)
{
    static bool receivingWriteData = false;

    for(int i=0; i<len; i++)
    {
        uint8_t val = bfr[i];

        if(val == TAG_WRITE_START) {    // on START tag found - now we're receiving write data
            wrBuffer.buffer[10] = (((posStreamed.side > 0) ? 0x80 : 0) | posStreamed.track);
            wrBuffer.buffer[11] = posStreamed.sector;

            receivingWriteData = true;
            wrBuffer.count = 12;        // start with 12 bytes in the buffer - 10 for header, 2 for track + side + sector

            // xprintf("START");
            continue;
        }

        if(val == TAG_WRITE_END) {      // on END tag found - we're no longer receiving write data
            if(receivingWriteData) {    // this END tag is the first END tag found after START tag, so we're done with receiving this sector
                receivingWriteData = false;     //  not receiving anymore

                sendHeaderAndDataToHost(wrBuffer.buffer, wrBuffer.count - TX_HEADER_SIZE);      // send sector to host
                storeWrittenSectorDataToTrackLocally(posStreamed.side, posStreamed.track, posStreamed.sector, wrBuffer.buffer, wrBuffer.count);

                // xprintf("END");
            }

            continue;
        }

        // we're not between START and END tag? ignore the data
        if(!receivingWriteData) {
            continue;
        }

        // buffer not full and the value is not a zero? store
        if(wrBuffer.count < WRITEBUFFER_SIZE) {
            wrBuffer.buffer[wrBuffer.count] = val;
            wrBuffer.count++;
        }
    }
}

void BIT_INVERT(int pin)
{
    if(BIT_IS_H(pin)) {
        gpio_put(pin, 0);
    } else {
        gpio_put(pin, 1);
    }
}

uint32_t lastSendFwTime;

void sendFwReport(uint32_t now)
{
    lastSendFwTime = now;
    sendHeaderAndDataToHost(atnSendFwVersion, ATN_SENDFWVERSION_LEN_TX - TX_HEADER_SIZE);
}

int main()
{
    setup();

    lastSendFwTime = millis();
    timeTrackStart = millis();

    int WGatePrev = 1;

    while(1)
    {
        int key = getchar_timeout_us(0);
        if(key == '\n' || key == '\r') {
            // In order for flashing to work, we must ensure that only 1 core is writing to flash and running, so
            // we must enter config mode and storing to flash before we call cyw43_arch_init(), 
            // which runs on other core. For this to happen we set a flag in the external PSRAM
            // which will not get cleared on pico restart, and we do the restart. The config then happens after restart.
            psramConfigFlagSet();
            watchdog_reboot(0, 0, 0);
        }

        // connect to wifi, discover CE server, connect to CE server
        connectToHost();

        // handle any data incoming
        handleIncommingData();

        // MFM read buffer should be refilled?
        if(fillWhat != FILL_NONE) {
            fillHalfMfmBuffer();
        }

        bool stWantsTheStream = BIT_IS_L(PIN_DRIVE_SEL) && BIT_IS_L(PIN_MOT_EN);

        // send heartbeat (fw version) once a second
        uint32_t now = millis();
        if (connectedToHost && (now - lastSendFwTime) >= 1000)
        {
            // if(stWantsTheStream) {
            //     hwPosition.side = BIT_IS_H(PIN_SIDE1) ? 0 : 1; // get the current SIDE

            //     xprintf("S %d %d\n", hwPosition.side, hwPosition.track);
            // }

            sendFwReport(now);
        }

        // request whole image if no image loaded
        if(connectedToHost && imageState == IMAGE_NOT_LOADED)
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
            gpio_put(PIN_FLCC_OE, 0);
        } else {    // other cases? DISABLE stream
            gpio_put(PIN_FLCC_OE, 1);
        }

        // when disk change happened
        if(diskChanged) {
            diskChanged = false;        // no change anymore

            BIT_INVERT(PIN_DENSITY);    // invert pins
            BIT_INVERT(PIN_WPROTECT);
            BIT_INVERT(PIN_DSKCHG);

            diskChangeEnd = now + 1000; // at this upcomming time invert back
            // xprintf("DSK CHG start");
        }

        // after enough time passed since the disk change, need to invert pins back
        if(diskChangeEnd != 0 && (now >= diskChangeEnd)) {
            diskChangeEnd = 0;          // set this var to zero, so we don't do this until next disk change

            BIT_INVERT(PIN_DENSITY);    // invert pins
            BIT_INVERT(PIN_WPROTECT);
            BIT_INVERT(PIN_DSKCHG);
            // xprintf("DSK CHG end");
        }

        //-------------------------------------------------
        now = millis();

        if(stWantsTheStream)
        {
            int WGateNow = BIT_LEVEL(PIN_WGATE);

            if(WGatePrev != WGateNow)   // write gate changed?
            {
                WGatePrev = WGateNow;

                if(WGateNow == 0)     // on write start
                {
                    posStreamed.side = BIT_IS_H(PIN_SIDE1) ? 0 : 1;                // get the current SIDE
                    // posStreamed.side, posStreamed.track, posStreamed.sector   // store side, track, sector into write positions buffer
                }
            }
        }

        //------------
        uint32_t timeSinceTrackStart = now - timeTrackStart;

        if(timeSinceTrackStart <= 195) {  // INDEX is H for time 0-195
            gpio_put(PIN_INDEX, 1);
        } else {                         // INDEX is H for times 196-200
            gpio_put(PIN_INDEX, 0);
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
    uint8_t* pMac = atnSendFwVersion + TX_HEADER_SIZE + 6;
    memset(pMac, 0, 6);
    cyw43_hal_get_mac(CYW43_HAL_MAC_WLAN0, pMac);
    xprintf("mac: %02X:%02X:%02X:%02X:%02X:%02X\n", pMac[0], pMac[1], pMac[2], pMac[3], pMac[4], pMac[5]);
}
