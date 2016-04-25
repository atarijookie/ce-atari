// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include "global.h"
#include "debug.h"
#include "mediastreaming.h"
#include "mediastreaming_commands.h"
#include <string>

// MediaStream implementation
MediaStream::MediaStream(void)
: f(NULL)
{
	//constructor
}

MediaStream::~MediaStream()
{
	if(f != NULL) {
		fclose(f);
	}
}

bool MediaStream::isFree(void)
{
	return (f == NULL);
}

bool MediaStream::open(const char * filename)
{
	f = fopen(filename, "rb");
	if(f == NULL) {
		Debug::out(LOG_ERROR, "MediaStream::open error opening %s", filename);
	}
	return (f != NULL);
}

void MediaStream::close(void)
{
	if(f != NULL) {
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
	if(n != 24) return -1;
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
}

MediaStreaming::~MediaStreaming()
{
	// destructor
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
	default:
		dataTrans->setStatus(MEDIASTREAMING_ERR_INVALIDCOMMAND);	// ERROR
	}
	dataTrans->sendDataAndStatus();
}

void MediaStreaming::openStream(AcsiDataTrans *dataTrans)
{
	BYTE buffer[512];
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
		dataTrans->setStatus(0xfd);
		return;
	}
	char * path;
	int driveIndex;
	path = (char *)buffer + 2;	// XXX
	driveIndex = buffer[0] - 'A';
	std::string atariPath(path);
	std::string hostPath;
	bool waitingForMount;
	int zipDirNestingLevel;
	translated->createFullHostPath(atariPath, driveIndex, hostPath, waitingForMount, zipDirNestingLevel);
	Debug::out(LOG_DEBUG, "MediaStreaming::openStream %s => %s", buffer, hostPath.c_str());
	//if(!streams[i].open("/mnt/sda/ATARI/STEAUPLY/BILLJE12.au")) {
	if(!streams[i].open("/home/root/nnd/THRILLER.au")) {
		dataTrans->setStatus(0xfc);
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

