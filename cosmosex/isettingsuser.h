#ifndef ISETTINGSUSER_H
#define ISETTINGSUSER_H

class ISettingsUser {
public:
    virtual void reloadSettings(int type) = 0;
};

#endif // ISETTINGSUSER_H
