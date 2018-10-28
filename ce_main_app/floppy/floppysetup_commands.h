#ifndef FLOPPYSETUP_COMMANDS_H
#define FLOPPYSETUP_COMMANDS_H

// commands for HOSTMOD_FDD_SETUP
#define FDD_CMD_IDENTIFY                    0
#define FDD_CMD_GETSILOCONTENT              1

#define FDD_CMD_UPLOADIMGBLOCK_START        10
#define FDD_CMD_UPLOADIMGBLOCK_FULL         11
#define FDD_CMD_UPLOADIMGBLOCK_PART         12
#define FDD_CMD_UPLOADIMGBLOCK_DONE_OK      13
#define FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL    14

#define FDD_CMD_SWAPSLOTS                   20
#define FDD_CMD_REMOVESLOT                  21
#define FDD_CMD_NEW_EMPTYIMAGE              22
#define FDD_CMD_GET_CURRENT_SLOT            25
#define FDD_CMD_SET_CURRENT_SLOT            26
#define FDD_CMD_GET_IMAGE_ENCODING_RUNNING  28

#define FDD_CMD_DOWNLOADIMG_START           30
#define FDD_CMD_DOWNLOADIMG_GETBLOCK        31
#define FDD_CMD_DOWNLOADIMG_DONE            32
#define FDD_CMD_DOWNLOADIMG_ONDEVICE        35

#define FDD_CMD_SEARCH_INIT                 40      // call this to init the image search
#define FDD_CMD_SEARCH_STRING               41      // search for a string, create vector of results
#define FDD_CMD_SEARCH_RESULTS              42      // retrieve one page of results
#define FDD_CMD_SEARCH_MARK                 43      // mark one image for download
#define FDD_CMD_SEARCH_DOWNLOAD             45      // start / check the download process
#define FDD_CMD_SEARCH_REFRESHLIST          48      // delete old image list, download a new one

#define FDD_CMD_SEARCH_DOWNLOAD2STORAGE     50      // start immediate background download

#define FDD_OK                              0
#define FDD_ERROR                           2
#define FDD_DN_LIST                         4
#define FDD_DN_WORKING                      5
#define FDD_DN_DONE                         6
#define FDD_DN_NOTHING_MORE                 7
#define FDD_RES_ONDEVICECOPY                0xDC

#endif // FLOPPYSETUP_COMMANDS_H
