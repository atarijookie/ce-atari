#ifndef _DEVCHANGEHANDLER_H_
#define _DEVCHANGEHANDLER_H_

class DevChangesHandler {
public:
	virtual void onDevAttached(std::string devName, bool isAtariDrive) = 0;
	virtual void onDevDetached(std::string devName) = 0;
	
};

#endif

