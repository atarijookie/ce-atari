#include "settingsreloadproxy.h"

SettingsReloadProxy::SettingsReloadProxy()
{
    // init the array
    for(int i=0; i<MAX_SETTINGS_USERS; i++) {
        settUser[i].su     = 0;
        settUser[i].type   = SETTINGSUSER_NONE;
    }
}

void SettingsReloadProxy::addSettingsUser(ISettingsUser *su, int type)
{
    // find a place for new settings user and store it (if possible)
    for(int i=0; i<MAX_SETTINGS_USERS; i++) {

        if(settUser[i].su == 0) {
            settUser[i].su     = su;
            settUser[i].type   = type;
            return;
        }
    }
}

void SettingsReloadProxy::reloadSettings(int type)
{
    // go through all the settings users and make the right ones reload
    for(int i=0; i<MAX_SETTINGS_USERS; i++) {
        if(settUser[i].type == type) {
            settUser[i].su->reloadSettings(type);
        }
    }
}
