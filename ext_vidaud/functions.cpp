#include "json.h"
#include "extensiondefs.h"
#include "functions.h"
#include "main.h"
#include "utils.h"
#include "recv.h"

using json = nlohmann::json;

bool streamRunning = false;

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
    if(audioChannels == AUDIO_OFF || audioRate == 0) {
        snprintf(cmdBuffer, cmdBufferLen - 1, 
            cmdFormatV,                                                         // format string
            inputFile,                                                          // input path with filename
            pixelFormat, vidFps, resolutionStr, SOCK_PATH_RECV_FFMPEG_VIDEO);   // video output: pixel format, frame rate, resolution, output to unix socket path
        return;
    } 

    // audio only?
    if(vidRes == VID_RES_OFF || vidFps == 0) {
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
    - video fps - 0 for disabled video, higher for actual fps
    - video resolution - VID_RES_ST_* value
    - audio samplerate - in Hz, 0 for disabled
    - audio - AUDIO_* value
    - filePath - path to audio / video file which will be streamed
*/
void start(json args, ResponseFromExtension* resp)
{
    createRecvThreadIfNeeded();

    uint8_t videoFps = args.at(0);
    uint8_t videoResolution = args.at(1);
    uint16_t audioRateHz = args.at(2);
    uint8_t audioChannels = args.at(3);
    std::string filePath = args.at(4);

    // bad param? fail
    if(videoFps > 30 || videoResolution > VID_RES_ST_HIGH || audioRateHz > 50000 || audioChannels > AUDIO_STEREO) {
        resp->statusByte = STATUS_BAD_ARGUMENT;
        return;
    }

    // file not found? fail
    if(!fileExists(filePath.c_str())) {
        resp->statusByte = STATUS_BAD_ARGUMENT;
        return;
    }

    char cmd[1024];
    createShellCommand(cmd, sizeof(cmd), filePath.c_str(), videoFps, videoResolution, audioRateHz, audioChannels);

    // TODO: start ffmpeg

    resp->statusByte = STATUS_OK;
}

/*
    Stop the current stream.
*/
void stop(json args, ResponseFromExtension* resp)
{


    resp->statusByte = STATUS_OK;
}

/*
    Get current video frames

args:
    - count of video frames to fetch
*/
void get_frames(json args, ResponseFromExtension* resp)
{
    uint8_t framesCount = args.at(0);

    // wait for enough frames / samples in buffer

    // convert data to expected video mode format


    uint32_t count = 0;
    responseStoreStatusAndDataLen(resp, STATUS_OK, count);
}

/*
    Get current audio samples

args:
    - count of audio frames to fetch
      (each audio frame has duration of 1 / videoFps)
*/
void get_samples(json args, ResponseFromExtension* resp)
{
    uint8_t framesCount = args.at(0);

    // wait for enough samples in buffer

    // convert data to expected audio mode format

    uint32_t count = 0;
    responseStoreStatusAndDataLen(resp, STATUS_OK, count);
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
