#ifndef _VERSION_H_
#define _VERSION_H_

#include <string>

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

#endif


