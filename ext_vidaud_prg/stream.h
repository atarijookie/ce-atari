#ifndef __STREAM_H__
#define __STREAM_H__

#define RGB_FRAME_SIZE_BYTES    (640*400*3)

#define VID_PALETTE_NONE    0
#define VID_PALETTE_ST      1       // 9 bits per color
#define VID_PALETTE_STE     2       // 12 bits per color

#define VID_RES_OFF     0
#define VID_RES_ST_LOW  1
#define VID_RES_ST_MID  2
#define VID_RES_ST_HIGH 3

#define AUDIO_OFF       0
#define AUDIO_MONO      1
#define AUDIO_STEREO    2

#define STATUS_NO_MORE_FRAMES   0xF0

#endif
