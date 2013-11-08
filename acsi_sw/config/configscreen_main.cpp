#include "configscreen_main.h"
#include "../global.h"
#include "../settings.h"

void onMainMenu_acsiConfig(ConfigComponent *sender)
{
	ConfigStream::instance().createScreen_acsiConfig();
}

void onMainMenu_translatedDisks(ConfigComponent *sender)
{
    ConfigStream::instance().createScreen_translated();
}

void onMainMenu_sharedDrive(ConfigComponent *sender)
{

}

void onMainMenu_floppyConfig(ConfigComponent *sender)
{

}

void onMainMenu_networkSettings(ConfigComponent *sender)
{
    ConfigStream::instance().createScreen_network();
}

void onMainMenu_updateSoftware(ConfigComponent *sender)
{

}

void onAcsiConfig_save(ConfigComponent *sender)
{
	int devTypes[8];
	
	bool somethingActive = false;
	bool somethingInvalid = false;
	int tranCnt = 0;
	
	for(int id=0; id<8; id++) {								// get all selected types from checkbox groups
		devTypes[id] = ConfigStream::instance().checkboxGroup_getCheckedId(id);

		if(devTypes[id] != DEVTYPE_OFF) {					// if found something which is not OFF
			somethingActive = true;
		}
		
		switch(devTypes[id]) {								// count the shared drives, network adapters, config drives
			case DEVTYPE_TRANSLATED:	tranCnt++;					break;
			case -1:					somethingInvalid = true;	break;
		}
	}

	if(somethingInvalid) {									// if everything is set to OFF
		ConfigStream::instance().showMessageScreen((char *) "Warning", (char *) "Some ACSI ID has no selected type.\nGo and select something!");
		return;
	}

	if(!somethingActive) {									// if everything is set to OFF
		ConfigStream::instance().showMessageScreen((char *) "Warning", (char *) "All ACSI IDs are set to 'OFF', this is invalid and would brick the device.\nSelect at least one active ACSI ID.");
		return;
	}
	
	if(tranCnt > 1) {										// more than 1 of this type?
		ConfigStream::instance().showMessageScreen((char *) "Warning", (char *) "You have more than 1 translated drives selected.\nUnselect some to leave only 1 active.");
		return;
	}
	
	// if we got here, everything seems to be fine and we can write values to settings
	Settings s;
	char key[32];
	
	for(int id=0; id<8; id++) {								// write all the ACSI IDs to settings
		sprintf(key, "ACSI_DEVTYPE_%d", id);				// create settings KEY, e.g. ACSI_DEVTYPE_0
		s.setInt(key, devTypes[id]);		
	}
	
	ConfigStream::instance().createScreen_homeScreen();		// now back to the home screen
}

void onTranslated_save(ConfigComponent *sender)
{
    std::string value;
    char letter1, letter2, letter3;

    ConfigStream::instance().getTextByComponentId(COMPID_TRAN_FIRST, value);

    if(value.length() < 1) {    // no drive letter
        letter1 = 0;
    } else {
        letter1 = value[0];
    }

    ConfigStream::instance().getTextByComponentId(COMPID_TRAN_SHARED, value);

    if(value.length() < 1) {    // no drive letter
        letter2 = 0;
    } else {
        letter2 = value[0];
    }

    ConfigStream::instance().getTextByComponentId(COMPID_TRAN_CONFDRIVE, value);

    if(value.length() < 1) {    // no drive letter
        letter3 = 0;
    } else {
        letter3 = value[0];
    }

    if(letter1 == 0 && letter2 == 0 && letter3 == 0) {
        ConfigStream::instance().showMessageScreen((char *) "Info", (char *) "No drive letter assigned, this is OK,\nbut the translated disk will be\nunaccessible.");
    }

    // now save the settings
    Settings s;
    s.setChar((char *) "DRIVELETTER_FIRST",      letter1);
    s.setChar((char *) "DRIVELETTER_SHARED",     letter2);
    s.setChar((char *) "DRIVELETTER_CONFDRIVE",  letter3);

    ConfigStream::instance().createScreen_homeScreen();		// now back to the home screen
}

void onNetwork_save(ConfigComponent *sender)
{


}
