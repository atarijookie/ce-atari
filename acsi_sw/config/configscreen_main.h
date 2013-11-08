#ifndef _CONFIGSCREEN_MAIN_H_
#define _CONFIGSCREEN_MAIN_H_

#include "configstream.h"
#include "configcomponent.h"

void onMainMenu_acsiConfig(ConfigComponent *sender);
void onMainMenu_translatedDisks(ConfigComponent *sender);

void onMainMenu_floppyConfig(ConfigComponent *sender);
void onMainMenu_networkSettings(ConfigComponent *sender);
void onMainMenu_sharedDrive(ConfigComponent *sender);
void onMainMenu_updateSoftware(ConfigComponent *sender);

void onAcsiConfig_save(ConfigComponent *sender);
void onTranslated_save(ConfigComponent *sender);

#define COMPID_TRAN_FIRST           1
#define COMPID_TRAN_SHARED          2
#define COMPID_TRAN_CONFDRIVE       3

#endif
