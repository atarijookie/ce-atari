// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <errno.h>
#include <string.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <sys/vt.h>
#include "global.h"
#include "debug.h"
#include "mediastreaming.h"
#include "mediastreaming_commands.h"
#include "acsidatatrans.h"
#include "translated/translateddisk.h"

#define Blue        0
#define Green       1
#define Red         2
#define Alpha       3

#define DEFAULT_FB  "/dev/fb0"

MediaStreaming * MediaStreaming::instance = NULL;

// MediaStream implementation
MediaStream::MediaStream(void)
: f(NULL)
{
	//constructor
}

MediaStream::~MediaStream()
{
	if(f != NULL) {
		if(isPipe)
			pclose(f);
		else
			fclose(f);
	}
}

bool MediaStream::isFree(void)
{
	return (f == NULL);
}

/* filename of url */
bool MediaStream::open(const char * filename, const MediaParams * params)
{
	char command[256];
	char escfilename[256];
	{
		// replace ' by '\''
		const char * p = filename;
		char * q = escfilename;
		/* remove file:/// */
		if(memcmp(p, "file://", 7) == 0) p += 7;
		while(*p && (q < escfilename + 255)) {
			if(*p == '\'') {
				*q++ = '\'';
				*q++ = '\\';
				*q++ = '\'';
				*q++ = '\'';
			} else {
				*q++ = *p;
			}
			p++;
		}
		*q = '\0';
	}
	snprintf(command, sizeof(command),
	         "/usr/local/bin/ffmpeg -v 0 -i '%s' -ar %u %s-f au -c pcm_s8 -", // -ac 1 => mono
	         escfilename, params->audioRate, params->forceMono ? "-ac 1 " : "");
	Debug::out(LOG_DEBUG, "MediaStream::open \"%s\"", command);
	f = popen(command, "r");
	isPipe = true;
	//f = fopen(filename, "rb");
	if(f == NULL) {
		Debug::out(LOG_ERROR, "MediaStream::open error opening %s : %s", filename, strerror(errno));
	}
	return (f != NULL);
}

void MediaStream::close(void)
{
	if(f != NULL) {
		if(isPipe)
			pclose(f);
		else
			fclose(f);
		f = NULL;
	}
}

int MediaStream::getInfos(BYTE * buffer, int bufferlen)
{
	/* read .AU header */
	size_t n;
	DWORD header_len;

	if(bufferlen < 24) return -1;
	rewind(f);
	n = fread(buffer, 1, 24, f);
	if(n != 24) {
		Debug::out(LOG_ERROR, "MediaStream::getInfos failed to read 24 bytes : %s",
		           feof(f) ? "EOF" : "ERROR");
		return -1;
	}
	header_len = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | (buffer[7]);
	if(header_len < 24) return 24;
	n = header_len;
	if((int)n > bufferlen) n = bufferlen;
	n = fread(buffer + 24, 1, n - 24, f);
	return n + 24;
}

int MediaStream::read(BYTE * buffer, int bufferlen)
{
	size_t n;
	n = fread(buffer, 1, bufferlen, f);
	return (int)n;
}

// MediaStreaming implementation
MediaStreaming::MediaStreaming(void)
{
	//constructor
    srcBlue = 0;
    srcGreen = 1;
    srcRed = 2;
    srcAlpha = 3;
}

MediaStreaming::~MediaStreaming()
{
	// destructor
}

MediaStreaming * MediaStreaming::getInstance(void)
{
    if(!instance)
        instance = new MediaStreaming();
    return instance;
}

void MediaStreaming::deleteInstance(void)
{
    delete instance;
    instance = NULL;
}

void MediaStreaming::processCommand(BYTE *command, AcsiDataTrans *dataTrans)
{
	BYTE cmd = command[4];
	BYTE arg = command[5];
	Debug::out(LOG_DEBUG, "MediaStreaming::processCommand cmd=0x%02x arg=0x%02x",
	           cmd, arg);
	dataTrans->clear();
	switch(cmd) {
	case MEDIASTREAMING_CMD_OPENSTREAM:
		openStream(dataTrans);
		break;
	case MEDIASTREAMING_CMD_GETSTREAMINFOS:
		getStreamInfo(arg, dataTrans);
		break;
	case MEDIASTREAMING_CMD_READSTREAM:
		readStream(arg, dataTrans);
		break;
	case MEDIASTREAMING_CMD_CLOSESTREAM:
		closeStream(arg, dataTrans);
		break;
    case MEDIASTREAMING_CMD_GET_SCREEN:
        getScreen(arg, dataTrans);
        break;
	default:
		dataTrans->setStatus(MEDIASTREAMING_ERR_INVALIDCOMMAND);	// ERROR
	}
	dataTrans->sendDataAndStatus();
}

void MediaStreaming::openStream(AcsiDataTrans *dataTrans)
{
    TranslatedDisk * translated = TranslatedDisk::getInstance();
	BYTE buffer[512];
	MediaParams params;
	const char * path_or_url;
	std::string hostPath;
	const char * path;

	// first, get data from Hans
	if(!dataTrans->recvData(buffer, 512)) {
		Debug::out(LOG_ERROR, "MediaStreaming::openStream failed to receive data");
		dataTrans->setStatus(MEDIASTREAMING_ERR_RX);
		return;
	}
	//Debug::out(LOG_DEBUG, "MediaStreaming::openStream %s", buffer);
	// find a free id
	int i;
	for(i = 0; i < MEDIASTREAMING_MAXSTREAMS; i++) {
		if(streams[i].isFree()) break;
	}
	if(i >= MEDIASTREAMING_MAXSTREAMS) {
		Debug::out(LOG_ERROR, "MediaStreaming::openStream no more free streams");
		dataTrans->setStatus(MEDIASTREAMING_ERR_INTERNAL);
		return;
	}

	// default params
	params.audioRate = 50066;	// STE higher sample rate
	params.forceMono = false;	// STE can do stereo !

	// parse open infos
	if(memcmp(buffer, "CEMP", 5) == 0) {
		path = NULL;
		unsigned int i = 4;
		// path is always the last param
		while((i < sizeof(buffer)) && (path == NULL)) {
			WORD id = buffer[i] << 8 | buffer[i+1];
			i += 2;
			switch(id) {
			case MEDIAPARAM_AUDIORATE:
				params.audioRate = buffer[i] << 8 | buffer[i+1];
				i += 2;
				break;
			case MEDIAPARAM_FORCEMONO:
				params.forceMono = (buffer[i] << 8 | buffer[i+1]) != 0;
				i += 2;
				break;
			case MEDIAPARAM_PATH:
				path = (char *)(buffer + i);
				break;
			default:
				Debug::out(LOG_ERROR, "MediaStreaming::openStream unknown parameter id %04X at offset %d", id, i);
				path = (char *)(buffer + i);
			}
		}
		if(path == NULL) {
			Debug::out(LOG_ERROR, "MediaStreaming::openStream mandatory parameter path not found");
			dataTrans->setStatus(MEDIASTREAMING_ERR_BADPARAM);
			return;
		}
	} else {
		// only plain path/url
		path = (char *)buffer;
	}

	Debug::out(LOG_DEBUG, "MediaStreaming::openStream path='%s'", path);
	// parse file path / url
	if(memcmp(path, "http", 4) == 0) {
		// url
		path_or_url = path;
	} else if(path[1] == ':') {
		// Full ATARI Path name
		int driveIndex;
		driveIndex = path[0] - 'A';
		std::string atariPath(path + 2); // skip drive letter and :
		bool waitingForMount;
		int zipDirNestingLevel;
		if(!translated) {
			Debug::out(LOG_ERROR, "MediaStreaming::openStream translated=%p cannot convert %s", translated, path);
			dataTrans->setStatus(MEDIASTREAMING_ERR_INTERNAL);
			return;
		}
		translated->createFullHostPath(atariPath, driveIndex, hostPath, waitingForMount, zipDirNestingLevel);
		Debug::out(LOG_DEBUG, "MediaStreaming::openStream %s => %s", path, hostPath.c_str());
		path_or_url = hostPath.c_str();
        if(access(path_or_url, R_OK) < 0) {
            Debug::out(LOG_ERROR, "MediaStreaming::openStream access(%s) : %s", path_or_url, strerror(errno));
			dataTrans->setStatus(MEDIASTREAMING_ERR_FILEACCESS);
			return;
        }
	} else {
		// relative file name
		std::string partialAtariPath(path);
		std::string fullAtariPath;
		int driveIndex = 0;
		bool waitingForMount;
		int zipDirNestingLevel;
		if(!translated->createFullAtariPathAndFullHostPath(partialAtariPath, fullAtariPath, driveIndex, hostPath, waitingForMount, zipDirNestingLevel)) {
			Debug::out(LOG_ERROR, "MediaStreaming::openStream failed to convert atariPath '%s'", partialAtariPath.c_str());
			dataTrans->setStatus(MEDIASTREAMING_ERR_INTERNAL);
			return;
		}
		Debug::out(LOG_DEBUG, "MediaStreaming::openStream %s => %s", path, hostPath.c_str());
		path_or_url = hostPath.c_str();
        if(access(path_or_url, R_OK) < 0) {
            Debug::out(LOG_ERROR, "MediaStreaming::openStream access(%s) : %s", path_or_url, strerror(errno));
			dataTrans->setStatus(MEDIASTREAMING_ERR_FILEACCESS);
			return;
        }
	}
	// now we have the path or url, open the stream :
	if(!streams[i].open(path_or_url, &params)) {
		dataTrans->setStatus(MEDIASTREAMING_ERR_INTERNAL);
		return;
	}
	dataTrans->setStatus(i + 1);
}

void MediaStreaming::closeStream(BYTE streamHandle, AcsiDataTrans *dataTrans)
{
	int index = streamHandle - 1;
	if(index >= MEDIASTREAMING_MAXSTREAMS || streams[index].isFree()) {
		Debug::out(LOG_ERROR, "MediaStreaming::%s streamHandle=0x%02x invalid",
		           "closeStream", streamHandle);
		dataTrans->setStatus(MEDIASTREAMING_ERR_INVALIDHANDLE);
		return;
	}
	streams[index].close();
	dataTrans->setStatus(MEDIASTREAMING_OK);
}

void MediaStreaming::readStream(BYTE arg, AcsiDataTrans *dataTrans)
{
	BYTE streamHandle = arg & 0x1f;
	int index = streamHandle - 1;
	if(index >= MEDIASTREAMING_MAXSTREAMS || streams[index].isFree()) {
		Debug::out(LOG_ERROR, "MediaStreaming::%s streamHandle=0x%02x invalid",
		           "readStream", streamHandle);
		dataTrans->setStatus(MEDIASTREAMING_ERR_INVALIDHANDLE);
		return;
	}
#define BUFFER_SIZE (65536)
	BYTE buffer[BUFFER_SIZE];
	unsigned int asked = 512 << (arg >> 5);
	int len = streams[index].read(buffer, asked);
	if(len < 0) {
		Debug::out(LOG_ERROR, "MediaStreaming::%s stream->read(%u) failed",
		           "readStream", asked);
		dataTrans->setStatus(MEDIASTREAMING_ERR_STREAMERROR);
		return;
	}
	Debug::out(LOG_DEBUG, "MediaStreaming::%s sending %d bytes (%u asked)",
	           "readStream", len, asked);
	dataTrans->addDataBfr(buffer, len, false);
	if(len < (int)asked) {
		dataTrans->addZerosUntilSize(asked);
		dataTrans->setStatus(MEDIASTREAMING_EOF);
	} else {
		dataTrans->setStatus(MEDIASTREAMING_OK);
	}
}

void MediaStreaming::getStreamInfo(BYTE streamHandle, AcsiDataTrans *dataTrans)
{
	int index = streamHandle - 1;
	if(index >= MEDIASTREAMING_MAXSTREAMS || streams[index].isFree()) {
		Debug::out(LOG_ERROR, "MediaStreaming::%s streamHandle=0x%02x invalid",
		           "getStreamInfo", streamHandle);
		dataTrans->setStatus(MEDIASTREAMING_ERR_INVALIDHANDLE);
		return;
	}
	BYTE buffer[512];
	int len = streams[index].getInfos(buffer, sizeof(buffer));
	if(len < 0) {
		Debug::out(LOG_ERROR, "MediaStreaming::%s stream->getInfos() failed",
		           "getStreamInfo");
		dataTrans->setStatus(MEDIASTREAMING_ERR_STREAMERROR);
		return;
	}
	Debug::out(LOG_DEBUG, "MediaStreaming::%s sending %d bytes",
	           "getStreamInfo", len);
	dataTrans->addDataBfr(buffer, len, false);
	dataTrans->addZerosUntilSize(512);
	dataTrans->setStatus(MEDIASTREAMING_OK);
}

void MediaStreaming::getScreen(BYTE arg, AcsiDataTrans *dataTrans)
{
    struct fb_var_screeninfo fb_varinfo;
    memset(&fb_varinfo, 0, sizeof(struct fb_var_screeninfo));

    get_framebufferdata((char *) DEFAULT_FB, &fb_varinfo, false);

    int bitdepth = (int) fb_varinfo.bits_per_pixel;
    int width = (int) fb_varinfo.xres;
    int height = (int) fb_varinfo.yres;
    int skip_bytes = (fb_varinfo.yoffset * fb_varinfo.xres) * (fb_varinfo.bits_per_pixel >> 3);

    int buf_size = width * height * (((unsigned int) bitdepth + 7) >> 3);
    BYTE *buf_p = (BYTE *) malloc(buf_size);

    if(buf_p == NULL) {
        //fatal_error("Not enough memory");
        return;
    }

    memset(buf_p, 0, buf_size);

    read_framebuffer((char *) DEFAULT_FB, buf_size, buf_p, skip_bytes);

    // TODO: something

	dataTrans->addZerosUntilSize(64 * 512);     // dummy data
	dataTrans->setStatus(MEDIASTREAMING_OK);
}

void MediaStreaming::get_framebufferdata(char *device, struct fb_var_screeninfo *fb_varinfo_p, int verbose)
{
    int fd;
    struct fb_fix_screeninfo fb_fixedinfo;

    /* now open framebuffer device */
    if(-1 == (fd=open(device, O_RDONLY)))
    {
    fprintf(stderr, "Error: Couldn't open %s.\n", device);
    exit(EXIT_FAILURE);
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, fb_varinfo_p) != 0) {
        Debug::out(LOG_ERROR, "ioctl FBIOGET_VSCREENINFO");
        return;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fb_fixedinfo) != 0) {
        Debug::out(LOG_ERROR, "ioctl FBIOGET_FSCREENINFO");
        return;
    }

    if (verbose)
    {
        fprintf(stderr, "frame buffer fixed info:\n");
        fprintf(stderr, "id: \"%s\"\n", fb_fixedinfo.id);
        switch (fb_fixedinfo.type)
        {
        case FB_TYPE_PACKED_PIXELS:
        fprintf(stderr, "type: packed pixels\n");
        break;
        case FB_TYPE_PLANES:
        fprintf(stderr, "type: non interleaved planes\n");
        break;
        case FB_TYPE_INTERLEAVED_PLANES:
        fprintf(stderr, "type: interleaved planes\n");
        break;
        case FB_TYPE_TEXT:
        fprintf(stderr, "type: text/attributes\n");
        break;
        case FB_TYPE_VGA_PLANES:
        fprintf(stderr, "type: EGA/VGA planes\n");
        break;
        default:
        fprintf(stderr, "type: undefined!\n");
        break;
        }
        fprintf(stderr, "line length: %i bytes (%i pixels)\n", fb_fixedinfo.line_length, fb_fixedinfo.line_length/(fb_varinfo_p->bits_per_pixel/8));

        fprintf(stderr, "\nframe buffer variable info:\n");
        fprintf(stderr, "resolution: %ix%i\n", fb_varinfo_p->xres, fb_varinfo_p->yres);
        fprintf(stderr, "virtual resolution: %ix%i\n", fb_varinfo_p->xres_virtual, fb_varinfo_p->yres_virtual);
        fprintf(stderr, "offset: %ix%i\n", fb_varinfo_p->xoffset, fb_varinfo_p->yoffset);
        fprintf(stderr, "bits_per_pixel: %i\n", fb_varinfo_p->bits_per_pixel);
        fprintf(stderr, "grayscale: %s\n", fb_varinfo_p->grayscale ? "true" : "false");
        fprintf(stderr, "red:   offset: %i, length: %i, msb_right: %i\n", fb_varinfo_p->red.offset, fb_varinfo_p->red.length, fb_varinfo_p->red.msb_right);
        fprintf(stderr, "blue:  offset: %i, length: %i, msb_right: %i\n", fb_varinfo_p->blue.offset, fb_varinfo_p->green.length, fb_varinfo_p->green.msb_right);
        fprintf(stderr, "green: offset: %i, length: %i, msb_right: %i\n", fb_varinfo_p->green.offset, fb_varinfo_p->blue.length, fb_varinfo_p->blue.msb_right);
        fprintf(stderr, "alpha: offset: %i, length: %i, msb_right: %i\n", fb_varinfo_p->transp.offset, fb_varinfo_p->transp.length, fb_varinfo_p->transp.msb_right);
        fprintf(stderr, "pixel format: %s\n", fb_varinfo_p->nonstd == 0 ? "standard" : "non-standard");
    }
    srcBlue = fb_varinfo_p->blue.offset >> 3;
    srcGreen = fb_varinfo_p->green.offset >> 3;
    srcRed = fb_varinfo_p->red.offset >> 3;

    if (fb_varinfo_p->transp.length > 0) {
    srcAlpha = fb_varinfo_p->transp.offset >> 3;
    } else {
    srcAlpha = -1; // not used
    }

    (void) close(fd);
}

void MediaStreaming::read_framebuffer(char *device, size_t bytes, unsigned char *buf_p, int skip_bytes)
{
    int fd;

    if(-1 == (fd=open(device, O_RDONLY)))
    {
    fprintf(stderr, "Error: Couldn't open %s.\n", device);
    exit(EXIT_FAILURE);
    }

    if (skip_bytes) {
        lseek(fd, skip_bytes, SEEK_SET);
    }

    if (buf_p == NULL || read(fd, buf_p, bytes) != (ssize_t) bytes) {
        Debug::out(LOG_ERROR, "Error: Not enough memory or data");
        return;
    }
}

void MediaStreaming::convert1555to32(int width, int height,
                unsigned char *inbuffer,
                unsigned char *outbuffer)
{
    unsigned int i;

    for (i=0; i < (unsigned int) height*width*2; i+=2)
    {
    /* BLUE  = 0 */
    outbuffer[(i<<1)+Blue] = (inbuffer[i+1] & 0x7C) << 1;
    /* GREEN = 1 */
        outbuffer[(i<<1)+Green] = (((inbuffer[i+1] & 0x3) << 3) |
                 ((inbuffer[i] & 0xE0) >> 5)) << 3;
    /* RED   = 2 */
    outbuffer[(i<<1)+Red] = (inbuffer[i] & 0x1f) << 3;
    /* ALPHA = 3 */
    outbuffer[(i<<1)+Alpha] = '\0';
    }
}

void MediaStreaming::convert565to32(int width, int height,
               unsigned char *inbuffer,
               unsigned char *outbuffer)
{
    unsigned int i;

    for (i=0; i < (unsigned int) height*width*2; i+=2)
    {
    /* BLUE  = 0 */
    outbuffer[(i<<1)+Blue] = (inbuffer[i] & 0x1f) << 3;
    /* GREEN = 1 */
        outbuffer[(i<<1)+Green] = (((inbuffer[i+1] & 0x7) << 3) |
                 (inbuffer[i] & 0xE0) >> 5) << 2;
        /* RED   = 2 */
    outbuffer[(i<<1)+Red] = (inbuffer[i+1] & 0xF8);
    /* ALPHA = 3 */
    outbuffer[(i<<1)+Alpha] = '\0';
    }
}

void MediaStreaming::convert888to32(int width, int height,
               unsigned char *inbuffer,
               unsigned char *outbuffer)
{
    unsigned int i;

    for (i=0; i < (unsigned int) height*width; i++)
    {
    /* BLUE  = 0 */
    outbuffer[(i<<2)+Blue] = inbuffer[i*3+srcBlue];
    /* GREEN = 1 */
        outbuffer[(i<<2)+Green] = inbuffer[i*3+srcGreen];
    /* RED   = 2 */
        outbuffer[(i<<2)+Red] = inbuffer[i*3+srcRed];
    /* ALPHA */
        outbuffer[(i<<2)+Alpha] = '\0';
    }
}

void MediaStreaming::convert8888to32(int width, int height,
               unsigned char *inbuffer,
               unsigned char *outbuffer)
{
    unsigned int i;

    for (i=0; i < (unsigned int) height*width; i++)
    {
    /* BLUE  = 0 */
    outbuffer[(i<<2)+Blue] = inbuffer[i*4+srcBlue];
    /* GREEN = 1 */
        outbuffer[(i<<2)+Green] = inbuffer[i*4+srcGreen];
    /* RED   = 2 */
        outbuffer[(i<<2)+Red] = inbuffer[i*4+srcRed];
    /* ALPHA */
        outbuffer[(i<<2)+Alpha] = srcAlpha >= 0 ? inbuffer[i*4+srcAlpha] : 0xff;
    }
}

