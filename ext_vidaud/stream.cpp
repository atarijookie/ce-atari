#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <cstring>

#include "extensiondefs.h"
#include "main.h"
#include "utils.h"
#include "recv.h"
#include "fifo.h"
#include "stream.h"

#include <GraphicsMagick/Magick++.h>
using namespace Magick;

extern TStream stream;
extern uint8_t frameDataRGB[RGB_FRAME_SIZE_BYTES];

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

void getWidthHeightBpp(uint8_t resolution, unsigned int& width, unsigned int& height, unsigned int& bitsPerPixel)
{
    width = 0;
    height = 0;
    bitsPerPixel = 0;

    switch(resolution) {
        case VID_RES_ST_LOW: width = 320; height = 200; bitsPerPixel = 4; break;
        case VID_RES_ST_MID: width = 640; height = 200; bitsPerPixel = 2; break;
        case VID_RES_ST_HIGH: width = 640; height = 400; bitsPerPixel = 1; break;
    }
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

void save_palette(uint8_t* pPalette, const Image& image)
{
    // for ST shift 5 down (leaving 3 bits), for STE shift 4 down (leaving 4 bits)
    int shift = (stream.videoPaletteType == VID_PALETTE_ST) ? 5 : 4;

    size_t colorMapSize = const_cast<Image&>(image).colorMapSize();
    for (size_t i = 0; i <colorMapSize; ++i) {
        const Color color = image.colorMap(i);

        uint16_t r = (color.redQuantum()   >> shift);
        uint16_t g = (color.greenQuantum() >> shift);
        uint16_t b = (color.blueQuantum()  >> shift);

        uint16_t stPalValue;

        /*
        colors in palette:  xxxx xRRR xGGG xBBB
                      ST :  xxxx x210 x210 x210
                      STE:  xxxx 0321 0321 0321
        */

        // for STE move bits 321 down by 1 bit, and move bit 0 up 3 bits
        if(stream.videoPaletteType == VID_PALETTE_STE) {
            r = (r >> 1) | ((r & 1) << 3);
            g = (g >> 1) | ((g & 1) << 3);
            b = (b >> 1) | ((b & 1) << 3);
        }
        stPalValue = (r << 8) | (g << 4) | b;   // merge colors to 16 bits

        storeWord(pPalette, stPalValue);    // store to palette space
        pPalette += 2;                      // advance to next palette value point
    }
}

void c2p(uint8_t* buffer, const Image& image, int bitsPerPixel)
{
    // unfortunately, we really need to call this one even if it's useless
    image.getConstPixels(0, 0, image.columns(), image.rows());
    const IndexPacket* pIndexPackets = image.getConstIndexes();

    uint16_t planes[4];

    // convert image to planes, 16 pixels at the time
    for (const IndexPacket* pIndexPacketsEnd = pIndexPackets + image.columns() * image.rows();
         pIndexPackets != pIndexPacketsEnd; pIndexPackets += 16) {

        // clear planes
        for (int j=0; j<bitsPerPixel; j++) {
            planes[j] = 0;
        }

        // construct planes
        for (size_t i=0; i<16; i++) {
            uint8_t paletteIndex = pIndexPackets[i];

            for (int j=0; j<bitsPerPixel; j++) {
                planes[j] |= ((paletteIndex >> j) & 1) << (15 - i);
            }
        }

        // store planes
        for (int i=0; i<bitsPerPixel; i++) {
            *buffer = planes[i] >> 8;    // MSB
            buffer++;
            *buffer = planes[i];         // LSB
            buffer++;
        }
    }
}

/*
    Convert input image to ST format of frame.
    For ST high the frameDataRGB are black-white pixels packed into byte.
    For ST mid and ST low the frameDataRGB are RGB pixels (8 bits each).
*/
void convertVideoFrameToSt(uint8_t* frameDataRGB, uint32_t videoBytesPerFrame, uint8_t* stFrame)
{
    // no special conversion for ST high, just copy the data
    if(stream.videoResolution == VID_RES_ST_HIGH) {
        memset(stFrame, 0, 32);                     // clear the pallete to zeros
        memcpy(stFrame + 32, frameDataRGB, 32000);  // copy black-white pixels as-is
        return;
    }

    // convert data to expected video mode format

    // create ImageMagick image object
    unsigned int width, height, bitsPerPixel;
    getWidthHeightBpp(stream.videoResolution, width, height, bitsPerPixel);
    Magick::Image image({width, height}, {0, 0, 0});
    image.classType(Magick::DirectClass);   // Image is composed of pixels which represent literal color values.
    image.type(Magick::TrueColorType);      // Truecolor image (RGB)

    // fill the image with actual RGB data
    Magick::PixelPacket* pPixelPackets = image.getPixels(0, 0, image.columns(), image.rows());

    for(unsigned int y=0; y<height; y++) {
        for(unsigned int x=0; x<width; x++) {
            pPixelPackets->red   = *frameDataRGB++;
            pPixelPackets->green = *frameDataRGB++;
            pPixelPackets->blue  = *frameDataRGB++;

            pPixelPackets++;
        }
    }
    image.syncPixels();

    // image color reduction
    image.quantizeDither(true);
    image.quantizeColors(1u << bitsPerPixel);
    image.quantize();

    if(image.type() != PaletteType) {
        printf("convertVideoFrameToSt - image.type() != PaletteType\n");
        return;
    }

    if (image.colorMapSize() > (1u << bitsPerPixel)) {
        printf("convertVideoFrameToSt - Too few bpp for %d colos\n", image.colorMapSize());
        return;
    }
    
    // save palette on start of ST frame, then the pixels
    save_palette(stFrame, image);
    c2p(stFrame + 32, image, bitsPerPixel);
}
