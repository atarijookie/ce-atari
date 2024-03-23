#include "json.h"
#include "extensiondefs.h"
#include "functions.h"
#include "main.h"
#include "utils.h"
#include "recv.h"
#include "fifo.h"

using json = nlohmann::json;

extern Fifo* fifoAudio;
extern Fifo* fifoVideo;

struct {
    uint8_t videoFps;
    uint8_t videoResolution;
    uint16_t audioRateHz;
    uint8_t audioChannels;
    std::string filePath;
    bool running = false;
} stream;

uint8_t frameDataRGB[640*400*3];

const char* getResolutionString(uint8_t resolution)
{
    switch(resolution) {
        case VID_RES_ST_LOW: return "320x200";
        case VID_RES_ST_MID: return "640x200";
        case VID_RES_ST_HIGH: return "640x400";
    }

    return NULL;
}

const char* getPixelFormat(uint8_t resolution)
{
    switch(resolution) {
        case VID_RES_ST_LOW: return "rgb24";
        case VID_RES_ST_MID: return "rgb24";
        case VID_RES_ST_HIGH: return "monow";
    }

    return NULL;
}

void createShellCommand(char* cmdBuffer, int cmdBufferLen, const char* inputFile, uint8_t vidFps, uint8_t vidRes, uint16_t audioRate, uint16_t audioChannels)
{
    // these are the format strings for commands
    const char* cmdFormatAV =                                   // audio + video output
        "ffmpeg -re -i %s "                                     // input path with filename
        "-map 0:v -pix_fmt %s -r %d -s %s -f rawvideo unix:%s " // video output: pixel format, frame rate, resolution, output to unix socket path
        "-map 0:a -ar %d -ac %d -f au -c pcm_s8 unix:%s";       // audio output: audio rate, audio channels, output to unix socket path

    const char* cmdFormatA =                                    // audio only output
        "ffmpeg -re -i %s "                                     // input path with filename
        "-vn "                                                  // no video output
        "-map 0:a -ar %d -ac %d -f au -c pcm_s8 unix:%s";       // audio output: audio rate, audio channels, output to unix socket path

    const char* cmdFormatV =                                    // video only output
        "ffmpeg -re -i %s "                                     // input path with filename
        "-map 0:v -pix_fmt %s -r %d -s %s -f rawvideo unix:%s " // video output: pixel format, frame rate, resolution, output to unix socket path
        "-an";                                                  // no audio output

    // get resolution as string
    const char* resolutionStr = getResolutionString(vidRes);
    const char* pixelFormat = getPixelFormat(vidRes);

    // video only?
    if(audioChannels == AUDIO_OFF) {
        snprintf(cmdBuffer, cmdBufferLen - 1, 
            cmdFormatV,                                                         // format string
            inputFile,                                                          // input path with filename
            pixelFormat, vidFps, resolutionStr, SOCK_PATH_RECV_FFMPEG_VIDEO);   // video output: pixel format, frame rate, resolution, output to unix socket path
        return;
    } 

    // audio only?
    if(vidRes == VID_RES_OFF) {
        snprintf(cmdBuffer, cmdBufferLen - 1, 
            cmdFormatA,                                                         // format string
            inputFile,                                                          // input path with filename
            audioRate, audioChannels, SOCK_PATH_RECV_FFMPEG_AUDIO);             // audio output: audio rate, audio channels, output to unix socket path
        return;
    } 

    // audio and video output
    snprintf(cmdBuffer, cmdBufferLen - 1, 
        cmdFormatAV,                                                         // format string
        inputFile,                                                          // input path with filename
        pixelFormat, vidFps, resolutionStr, SOCK_PATH_RECV_FFMPEG_VIDEO,    // video output: pixel format, frame rate, resolution, output to unix socket path
        audioRate, audioChannels, SOCK_PATH_RECV_FFMPEG_AUDIO);             // audio output: audio rate, audio channels, output to unix socket path
}

/*
    Start audio-video streaming with specified audio and video params.

args:
    - video fps - mow hany fps the video should be, plus how large the audio chunks will be
    - video resolution - VID_RES_ST_* value (VID_RES_OFF for no video)
    - audio samplerate - in Hz
    - audio - AUDIO_* value (AUDIO_OFF for no audio)
    - filePath - path to audio / video file which will be streamed
*/
void start(json args, ResponseFromExtension* resp)
{
    createRecvThreadIfNeeded();

    stream.videoFps = args.at(0);
    stream.videoResolution = args.at(1);
    stream.audioRateHz = args.at(2);
    stream.audioChannels = args.at(3);
    stream.filePath = args.at(4);

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

    // TODO: start ffmpeg

    resp->statusByte = STATUS_OK;
}

/*
    Stop the current stream.
*/
void stop(json args, ResponseFromExtension* resp)
{
    // TODO: stop ffmpeg

    resp->statusByte = STATUS_OK;
}

bool waitForBytesInFifo(Fifo* fifo, uint32_t bytesWant, uint32_t waitFrames)
{
    uint32_t oneFrameMs = 1000 / stream.videoFps;       // how many ms one frame takes
    uint32_t waitDurationMs = waitFrames * oneFrameMs;  // how many ms we should wait before failing

    uint32_t endTime = getEndTime(waitDurationMs);      // when the waiting will be over

    while(endTime <= getCurrentMs()) {              // while not timeout
        mutexLock();

        if(fifo->usedBytes() >= bytesWant) {        // got enough data? get it and send it
            return true;
        }

        mutexUnlock();

        sleepMs(5);     // not enough data, so sleep a little to wait
    }

    // if got here, then timeout happened before buffer had enough data
    return false;
}

void convertVideoFrameToSt(uint8_t* frameDataRGB, uint32_t videoBytesPerFrame, uint8_t* stFrame)
{
    // no special conversion for ST high, just copy the data
    if(stream.videoResolution == VID_RES_ST_HIGH) {
        memset(stFrame, 0, 32);                     // clear the pallete to zeros
        memcpy(stFrame + 32, frameDataRGB, 32000);  // copy black-white pixels as-is
        return;
    }

    // TODO: convert data to expected video mode format

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

    uint8_t args1[5] = {TYPE_UINT8, TYPE_UINT8, TYPE_UINT16, TYPE_UINT8, TYPE_PATH};
    addFunctionSignature((void*) start, "start", FUNC_LONG_ARGS, args1, 5, RESP_TYPE_STATUS);

    addFunctionSignature((void*) stop, "stop", FUNC_RAW_WRITE, NULL, 0, RESP_TYPE_STATUS);

    uint8_t args3[2] = {TYPE_UINT8, TYPE_UINT8};
    addFunctionSignature((void*) get_frames, "get_frames", FUNC_RAW_READ, args3, 2, RESP_TYPE_STATUS_BIN_DATA);

    addFunctionSignature((void*) get_samples, "get_samples", FUNC_RAW_READ, args3, 2, RESP_TYPE_STATUS_BIN_DATA);
}
