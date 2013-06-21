#ifndef _CONFIGSCREEN_MAIN_H_
#define _CONFIGSCREEN_MAIN_H_

#include "configstream.h"
#include "configcomponent.h"

void onMainMenu_acsiConfig(ConfigComponent *sender);
void onMainMenu_floppyConfig(ConfigComponent *sender);
void onMainMenu_networkSettings(ConfigComponent *sender);
void onMainMenu_sharedDrive(ConfigComponent *sender);
void onMainMenu_updateSoftware(ConfigComponent *sender);

void onAcsiConfig_save(ConfigComponent *sender);

#endif
