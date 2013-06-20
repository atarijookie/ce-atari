#include <stdio.h>
#include <string>

class ConfigComponent;
typedef void (*TFonEnter)		(ConfigComponent *sender);
typedef void (*TFonChBEnter)	(int groupId, int checkboxId);

class ConfigComponent
{
public:
	enum ComponentType{ label, button, checkbox, editline };

	// maxLen is maximum length of text, that means that on screen it might have 2 more ('[' and ']')
	ConfigComponent(ComponentType type, std::string text, int maxLen, int x, int y);		
	void setCheckboxGroupIds(int groupId, int checkboxId);
	void getCheckboxGroupIds(int& groupId, int& checkboxId);

	void setOnEnterFunction(TFonEnter onEnter);
	void setOnChBEnterFunction(TFonChBEnter onChBEnter);
	
	void getStream(bool fullNotChange, char *bfr, int &len);
	void setFocus(bool hasFocus);
	void setReverse(bool isReverse);
	void setIsChecked(bool isChecked);
	void onKeyPressed(char vkey, char key);
	void setText(std::string text);
	
	bool isFocused(void);
	bool isChecked(void);
	bool canFocus(void);

	void terminal_addGotoCurrentCursor(char *bfr, int &cnt);	// then add +cnt to bfr (might be 0 or 4)
	
private:
	bool			changed;

	bool			hasFocus;
	bool			isReverse;
	bool			checked;
	
	ComponentType	type;
	int				posX, posY;
	int				maxLen;
	std::string		text;

	// for editline	
	int				cursorPos;
	int				textLength;

	// for checkbox
	int				checkBoxGroup;
	int				checkBoxId;
	
	TFonEnter		onEnter;
	TFonChBEnter	onChBEnter;
	
	void terminal_addGoto(char *bfr, int x, int y);				// then add +4 to bfr
	void terminal_addReverse(char *bfr, bool onNotOff);			// then add +2 to bfr
	
	void handleEditLineKeyPress(char vkey, char key);
	bool isGroupCheckBox(void);
};

