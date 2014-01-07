#include <stdio.h>

#include "config/configstream.h"
#include "settings.h"
#include "global.h"

int main()
 {
    printf("CosmosEx starting...\n");

	//-----------------	 
	// read the settings and create devices as needed
	Settings s;
	 
	char key[32];
	int devTypes[8];
	bool somethingActive = false;						// flag to see if we got at least one device active
	 
	for(int id=0; id<8; id++) {						// read the list of device types from settings
		sprintf(key, "ACSI_DEVTYPE_%d", id);			// create settings KEY, e.g. ACSI_DEVTYPE_0
		devTypes[id] = s.getInt(key, DEVTYPE_OFF);		
		
		if(devTypes[id] < 0) {
			devTypes[id] = DEVTYPE_OFF;
		}
		
		if(devTypes[id] != DEVTYPE_OFF) {				// if we found something active, set the flag
			somethingActive = true;
		}
	}
	 
	if(!somethingActive) {								// if no device is activated, activate DEVTYPE_TRANSLATED on ACSI ID 0 to avoid bricking the device
		devTypes[0] = DEVTYPE_TRANSLATED;
	}



	

    return 0;
 }

