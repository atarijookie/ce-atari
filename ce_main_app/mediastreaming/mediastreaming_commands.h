#ifndef MEDIASTREAMING_COMMANDS_H
#define MEDIASTREAMING_COMMANDS_H

/* Mediastreaming format :
 * 00 id << 5
 * 01 'C'
 * 02 'E'
 * 03 HOSTMOD_MEDIA_STREAMING
 * 04 command
 * 05 argument
 *
 * MEDIASTREAMING_CMD_OPENSTREAM : 512 byte data
 * 4 first bytes : 'CEMP'
 * followed by a parameter list (see ids below)
 * MEDIAPARAM_PATH should be the last in the list
 *
 * MEDIASTREAMING_CMD_GETSTREAMINFOS : arg = stream_id
 *
 * MEDIASTREAMING_CMD_READSTREAM : arg = asked_size_code << 5 | stream_id
 * asked_size_code : 0 => 512, 1 => 1024, 2 => 2048, 3 => 4096, 4 => 8192
 *  5 => 16384, 6 => 32768, 7 => 65536
 *
 * MEDIASTREAMING_CMD_CLOSESTREAM : arg = stream_id
 */
#define MEDIASTREAMING_CMD_OPENSTREAM	1
#define MEDIASTREAMING_CMD_GETSTREAMINFOS	2
#define MEDIASTREAMING_CMD_READSTREAM	3
#define MEDIASTREAMING_CMD_CLOSESTREAM	4

/* parameters for MEDIASTREAMING_CMD_OPENSTREAM */
#define MEDIAPARAM_AUDIORATE	0x0001		// uint16_t value : 50066, 25033, etc.
#define MEDIAPARAM_FORCEMONO	0x0002		// uint16_t value : 1 for true / 0 false

#define MEDIAPARAM_PATH			0x00ff		// null terminated ascii string

/* error codes */
#define MEDIASTREAMING_OK					0
#define MEDIASTREAMING_ERR_INVALIDCOMMAND	0xff
#define MEDIASTREAMING_ERR_INVALIDHANDLE	0xfe
#define MEDIASTREAMING_ERR_STREAMERROR		0xfd
#define MEDIASTREAMING_ERR_RX				0xfc
#define MEDIASTREAMING_ERR_INTERNAL			0xfb
#define MEDIASTREAMING_ERR_BADPARAM			0xfa
#define MEDIASTREAMING_ERR_FILEACCESS		0xf9
#define MEDIASTREAMING_EOF					0xf0

#endif // MEDIASTREAMING_COMMANDS_H
