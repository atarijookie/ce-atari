#ifndef _VERSION_H_
#define _VERSION_H_

#include <string>
#include <stdint.h>

#define XILINX_VERSION_FILE     "/ce/update/xilinx.current"

class Version
{
public:
    static void getAppVersion(char *bfr);

    static void getRaspberryPiInfo(void);
    static void readLineFromFile  (const char *filename, char *buffer, int maxLen, const char *defValue);

    Version();
    void fromString(char *str);
    void fromStringWithoutDashes(char *str);
    void fromInts(int y, int m, int d);
    void fromFirstLineOfFile(char *filePath, bool withDashes=true);
    void clear(void);

    void        setUrlAndChecksum(char *pUrl, char *chs);
    std::string getUrl(void);
    uint16_t        getChecksum(void);

    void toString(char *str);
    int  getYear(void);
    int  getMonth(void);
    int  getDay(void);

    volatile uint8_t downloadStatus;

private:
    int year;
    int month;
    int day;

    std::string url;
    uint16_t        checksum;
};

typedef struct {
            Version app;
            Version hans;
            Version xilinx;
            Version franz;
    } Versions;

#endif


