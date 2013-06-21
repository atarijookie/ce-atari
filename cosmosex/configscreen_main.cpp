#include "configscreen_main.h"
#include "globaldefs.h"
#include "settings.h"

void onMainMenu_acsiConfig(ConfigComponent *sender)
{
	ConfigStream::instance().createScreen_acsiConfig();
}

void onMainMenu_floppyConfig(ConfigComponent *sender)
{

}

void onMainMenu_networkSettings(ConfigComponent *sender)
{

}

void onMainMenu_sharedDrive(ConfigComponent *sender)
{

}

void onMainMenu_updateSoftware(ConfigComponent *sender)
{

}

void onAcsiConfig_save(ConfigComponent *sender)
{
	int devTypes[8];
	
	bool somethingActive = false;
	bool somethingInvalid = false;
	int sharCnt = 0, netCnt = 0, confCnt = 0;
	
	for(int id=0; id<8; id++) {								// get all selected types from checkbox groups
		devTypes[id] = ConfigStream::instance().checkboxGroup_getCheckedId(id);

		if(devTypes[id] != DEVTYPE_OFF) {					// if found something which is not OFF
			somethingActive = true;
		}
		
		switch(devTypes[id]) {								// count the shared drives, network adapters, config drives
			case DEVTYPE_SHARED_DRIVE:	sharCnt++;					break;
			case DEVTYPE_NET_ADAPTER:	netCnt++;					break;
			case DEVTYPE_CONFIG_DRIVE:	confCnt++;					break;
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
	
	if(sharCnt > 1) {										// more than 1 of this type?
		ConfigStream::instance().showMessageScreen((char *) "Warning", (char *) "You have more than 1 shared drives selected.\nUnselect some to leave only 1 active.");
		return;
	}
	
	if(netCnt > 1) {										// more than 1 of this type?
		ConfigStream::instance().showMessageScreen((char *) "Warning", (char *) "You have more than 1 network adapters selected.\nUnselect some to leave only 1 active.");
		return;
	}
	
	if(confCnt > 1) {										// more than 1 of this type?
		ConfigStream::instance().showMessageScreen((char *) "Warning", (char *) "You have more than 1 config drives selected.\nUnselect some to leave only 1 active.");
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

