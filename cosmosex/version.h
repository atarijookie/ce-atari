#ifndef _VERSION_H_
#define _VERSION_H_

#include <string>
#include "datatypes.h"

#define XILINX_VERSION_FILE		"/ce/update/xilinx_current.txt"
#define IMAGELIST_FILE      	"/ce/update/imagelist.csv"
#define APP_VERSION         	"2014-03-04"

class Version
{
public:
    Version();
    void fromString(char *str);
    void fromInts(int y, int m, int d);
    void fromFirstLineOfFile(char *filePath);
    void clear(void);

    void        setUrlAndChecksum(char *pUrl, char *chs);
    std::string getUrl(void);
    WORD        getChecksum(void);

    void toString(char *str);

    bool isOlderThan(const Version &other);
    bool isEqualTo(const Version &other);

private:
    int year;
    int month;
    int day;

    std::string url;
    WORD        checksum;
};

typedef struct {
        struct {
            Version app;
            Version hans;
            Version xilinx;
            Version franz;
            Version imglist;
        } current;

        struct {
            Version app;
            Version hans;
            Version xilinx;
            Version franz;
            Version imglist;
        } onServer;

        bool updateListWasProcessed;
        bool gotUpdate;
    } Versions;

#endif


