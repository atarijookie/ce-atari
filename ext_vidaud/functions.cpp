#include "json.h"
#include "extensiondefs.h"
#include "functions.h"
#include "main.h"
#include "utils.h"
#include "recv.h"
#include "fifo.h"
#include "stream.h"

using json = nlohmann::json;

extern Fifo* fifoAudio;
extern Fifo* fifoVideo;

TStream stream;
uint8_t frameDataRGB[RGB_FRAME_SIZE_BYTES];

/*
    Start audio-video streaming with specified audio and video params.

args:
    - video fps - mow hany fps the video should be, plus how large the audio chunks will be
    - video resolution - VID_RES_ST_* value (VID_RES_OFF for no video)
    - video palette type - VID_PALETTE_* value (to generate different palette for ST and STE)
    - audio samplerate - in Hz
    - audio - AUDIO_* value (AUDIO_OFF for no audio)
    - filePath - path to audio / video file which will be streamed
*/
void start(json args, ResponseFromExtension* resp)
{
    createRecvThreadIfNeeded();
    stopStream();

    stream.videoFps = args.at(0);
    stream.videoResolution = args.at(1);
    stream.videoPaletteType = args.at(2);
    stream.audioRateHz = args.at(3);
    stream.audioChannels = args.at(4);
    stream.filePath = args.at(5);

    // bad param? fail
    if(stream.videoFps > 30 || stream.videoResolution > VID_RES_ST_HIGH || stream.audioRateHz > 50000 || stream.audioChannels > AUDIO_STEREO) {
        resp->statusByte = STATUS_BAD_ARGUMENT;
        return;
    }

    // file not found? fail
    if(!fileExists(stream.filePath.c_str())) {
        resp->statusByte = STATUS_BAD_ARGUMENT;
        return;
    }

    // clear the FIFOs from anything that's left in them
    fifoAudio->clear();
    fifoVideo->clear();

    char cmd[1024];
    createShellCommand(cmd, sizeof(cmd), stream.filePath.c_str(), stream.videoFps, stream.videoResolution, stream.audioRateHz, stream.audioChannels);

    // start ffmpeg
    stream.pipe = popen(cmd, "r");
    stream.running = stream.pipe != NULL;    // running if got valid handle

    resp->statusByte = stream.running ? STATUS_OK : STATUS_EXT_ERROR;
}

/*
    Stop the current stream.
*/
void stop(json args, ResponseFromExtension* resp)
{
    stopStream();
    resp->statusByte = STATUS_OK;
}

/*
    Get current video frames

args:
    - count of video frames to fetch
*/
void get_frames(json args, ResponseFromExtension* resp)
{
    uint32_t framesCount = args.at(0);
    uint32_t videoBytesPerFrame = 0;

    if(stream.videoFps > 0) {
        switch(stream.videoResolution) {
            case VID_RES_ST_LOW:    videoBytesPerFrame = 320*200*3; break;      // ST low * 3 RGB bytes per pixel
            case VID_RES_ST_MID:    videoBytesPerFrame = 640*200*3; break;      // ST mid * 3 RGB bytes per pixel
            case VID_RES_ST_HIGH:   videoBytesPerFrame = (640*400)/8; break;    // ST high / 8 black-white pixels fit into byte
        }
    }

    uint32_t bytesWant = framesCount * videoBytesPerFrame;  // how many bytes we want transfer now

    // stream is not running? update the bytesWant to only what remains in FIFO
    if(!stream.running) {
        mutexLock();
        bytesWant = fifoAudio->usedBytes();
        mutexUnlock();

        // stream not running and no more data in FIFO? no more frames!
        if(bytesWant == 0) {
            resp->statusByte = STATUS_NO_MORE_FRAMES;
            return;
        }
    }

    // wait for enough data in buffer
    bool canGetBytes = waitForBytesInFifo(fifoVideo, bytesWant, framesCount);

    // not engouh bytes in FIFO? don't send data now
    if(!canGetBytes) {
        resp->statusByte = STATUS_NO_RESPONSE;
        return;
    }

    framesCount = bytesWant / videoBytesPerFrame;               // update received frames to count of how many frames we can get from the data in FIFO

    // fetch and process video data by each frame
    for(uint32_t i=0; i<framesCount; i++) {
        mutexLock();
        fifoVideo->getBfr(frameDataRGB, videoBytesPerFrame);    // get one frame in the buffer
        mutexUnlock();

        // convert data to expected video mode format
        uint32_t stFrameOffset = i * 32032;
        convertVideoFrameToSt(frameDataRGB, videoBytesPerFrame, resp->data + stFrameOffset);
    }

    uint32_t respSizeBytes = framesCount * 32032;
    responseStoreStatusAndDataLen(resp, framesCount, respSizeBytes);    // the status holds how many frames we are returning to ST
}

/*
    Get current audio samples

args:
    - count of audio frames to fetch
      (each audio frame has duration of 1 / videoFps)
*/
void get_samples(json args, ResponseFromExtension* resp)
{
    uint32_t framesCount = args.at(0);
    uint32_t audioBytesPerFrame = 0;

    if(stream.videoFps > 0) {
        // calculate how many audio bytes we get for each video frame, like:
        // (25000 Hz / 10 fps) * 2 channels = 5000 bytes per each video frame
        audioBytesPerFrame = (stream.audioRateHz / stream.videoFps) * stream.audioChannels;
    }

    uint32_t bytesWant = framesCount * audioBytesPerFrame;  // how many bytes we want transfer now

    if(bytesWant > MAX_RESPONSE_DATA_SIZE) {        // the bytes we want couldn't fit in the reponse, fail here
        resp->statusByte = STATUS_BAD_ARGUMENT;
        return;
    }

    // stream is not running? update the bytesWant to only what remains in FIFO
    if(!stream.running) {
        mutexLock();
        bytesWant = fifoAudio->usedBytes();
        mutexUnlock();

        // stream not running and no more data in FIFO? no more frames!
        if(bytesWant == 0) {
            resp->statusByte = STATUS_NO_MORE_FRAMES;
            return;
        }
    }

    // wait for enough samples in buffer
    bool canGetBytes = waitForBytesInFifo(fifoAudio, bytesWant, framesCount);

    if(!canGetBytes) {  // not engouh bytes in FIFO? don't send data now
        resp->statusByte = STATUS_NO_RESPONSE;
        return;
    }

    // got enough data? get it and send it
    mutexLock();
    fifoAudio->getBfr(resp->data, bytesWant);   // get the data into response
    mutexUnlock();

    // how many frames we were able to fetch from FIFO (e.g. at the end of stream)
    uint32_t framesReceived = bytesWant / audioBytesPerFrame;
    uint32_t bytesInLastFrame = bytesWant % audioBytesPerFrame; // see if last frame is full (== remaining bytes are 0)

    // if not a full frame was in the buffer, increase received frames count
    if(bytesInLastFrame != 0) {
        framesReceived++;

        uint32_t paddBytes = audioBytesPerFrame - bytesInLastFrame;
        memset(resp->data + bytesWant, 0, paddBytes);   // clear the padding bytes
        bytesWant += paddBytes;                         // increase the received bytes to full frame
    }

    responseStoreStatusAndDataLen(resp, framesReceived, bytesWant);     // the status holds how many frames we are returning to ST
}

/*
Place the functions which should be exported in this function. 
You must specify for each function:
    - exported function name
    - function call type (where the args are, also read / write direction when calling)
    - argument types (what argument will be stored and later retrieved from the buffer / cmd[4] cmd[5])
    - arguments count
    - return value type
*/
void exportFunctionSignatures(void)
{
    createRecvThreadIfNeeded();

    uint8_t args1[6] = {TYPE_UINT8, TYPE_UINT8, TYPE_UINT8, TYPE_UINT16, TYPE_UINT8, TYPE_PATH};
    addFunctionSignature((void*) start, "start", FUNC_LONG_ARGS, args1, 6, RESP_TYPE_STATUS);

    addFunctionSignature((void*) stop, "stop", FUNC_RAW_WRITE, NULL, 0, RESP_TYPE_STATUS);

    uint8_t args3[2] = {TYPE_UINT8, TYPE_UINT8};
    addFunctionSignature((void*) get_frames, "get_frames", FUNC_RAW_READ, args3, 2, RESP_TYPE_STATUS_BIN_DATA);

    addFunctionSignature((void*) get_samples, "get_samples", FUNC_RAW_READ, args3, 2, RESP_TYPE_STATUS_BIN_DATA);
}
