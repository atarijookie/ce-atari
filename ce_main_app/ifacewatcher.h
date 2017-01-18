// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef IFACEWATCHER_H_
#define IFACEWATCHER_H_

#include <map>
#include <string>

class IfaceWatcher
{
public:
    IfaceWatcher();
    ~IfaceWatcher();

    void processMsg(bool * newIfaceUpAndRunning);
    int getFd(void) { return fd; };

private:
    int fd;
    std::map<int, unsigned int> if_flags;
    std::map<int, std::string> if_names;
};

#endif /* IFACEWATCHER_H_ */
