#ifndef _VERSION_H_
#define _VERSION_H_

#include <string>

#define UPDATE_REMOTEURL    "http://joo.kie.sk/cosmosex/update/updatelist.csv"
#define UPDATE_LOCALPATH    "update"
#define UPDATE_LOCALLIST    "update/updatelist.csv"

#define XILINX_VERSION_FILE "update/xilinx_current.txt"
#define IMAGELIST_FILE      "imagelist.csv"
#define APP_VERSION         "2014-03-04"

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

    void toString(char *str);

    bool isOlderThan(const Version &other);
private:
    int year;
    int month;
    int day;

    std::string url;
    int         checksum;
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


