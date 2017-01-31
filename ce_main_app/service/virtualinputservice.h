// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _VIRTUALINPUTSERVICE_H_
#define _VIRTUALINPUTSERVICE_H_

class VirtualInputService
{
public:
    void start();
    void stop();
protected:
    VirtualInputService(const char * path, const char * type);
    virtual ~VirtualInputService();
private:
    void openFifo();
    void closeFifo();

protected:
    int fd;
    bool initialized;
    const char * devpath;
    const char * devtype;
};
#endif
