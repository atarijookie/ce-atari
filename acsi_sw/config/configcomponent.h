#ifndef _CONFIGCOMPONENT_H_
#define _CONFIGCOMPONENT_H_

#include <stdio.h>
#include <string>

#include "../datatypes.h"

class ConfigComponent;
typedef void (*TFonEnter)		(ConfigComponent *sender);
typedef void (*TFonChBEnter)	(int groupId, int checkboxId);

class ConfigComponent
{
public:
	enum ComponentType{ label, button, checkbox, editline };

	// maxLen is maximum length of text, that means that on screen it might have 2 more ('[' and ']')
    ConfigComponent(ComponentType type, std::string text, WORD maxLen, int x, int y);
	void setCheckboxGroupIds(int groupId, int checkboxId);
	void getCheckboxGroupIds(int& groupId, int& checkboxId);

	void setOnEnterFunction(TFonEnter onEnter);
	void setOnChBEnterFunction(TFonChBEnter onChBEnter);
	
    void getStream(bool fullNotChange, BYTE *bfr, int &len);
	void setFocus(bool hasFocus);
	void setReverse(bool isReverse);
	void setIsChecked(bool isChecked);
    void onKeyPressed(BYTE key);
	void setText(std::string text);
	
	bool isFocused(void);
	bool isChecked(void);
	bool canFocus(void);

    void terminal_addGotoCurrentCursor(BYTE *bfr, int &cnt);	// then add +cnt to bfr (might be 0 or 4)
	
private:
	bool			changed;

	bool			hasFocus;
	bool			isReverse;
	bool			checked;
	
	ComponentType	type;
	int				posX, posY;
    WORD			maxLen;
	std::string		text;

	// for editline	
    WORD			cursorPos;

	// for checkbox
	int				checkBoxGroup;
	int				checkBoxId;
	
	TFonEnter		onEnter;
	TFonChBEnter	onChBEnter;
	
    void terminal_addGoto(BYTE *bfr, int x, int y);				// then add +4 to bfr
    void terminal_addReverse(BYTE *bfr, bool onNotOff);			// then add +2 to bfr
	
    void handleEditLineKeyPress(BYTE key);
	bool isGroupCheckBox(void);
};

#endif

