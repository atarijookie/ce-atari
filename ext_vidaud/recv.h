#ifndef __RECV_H__
#define __RECV_H__

#define SOCK_PATH_RECV_FFMPEG_AUDIO   "/tmp/ext_vidaud_ffmpeg_audio.sock"
#define SOCK_PATH_RECV_FFMPEG_VIDEO   "/tmp/ext_vidaud_ffmpeg_video.sock"

void createRecvThreadIfNeeded(void);

#endif
