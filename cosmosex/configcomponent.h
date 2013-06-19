#include <stdio.h>
#include <string>

class ConfigComponent;
typedef void (*TFonEnter)(ConfigComponent *);

class ConfigComponent
{
public:
	enum ComponentType{ label, button, checkbox, editline };

	// maxLen is maximum length of text, that means that on screen it might have 2 more ('[' and ']')
	ConfigComponent(ComponentType type, std::string text, int maxLen, int x, int y);		
	void setOnEnterFunction(TFonEnter *onEnter);
	
	void getStream(bool fullNotChange, char *bfr, int &len);
	void setFocus(bool hasFocus);
	void setReverse(bool isReverse);
	void setIsChecked(bool isChecked);
	void onKeyPressed(char vkey, char key);
	void setText(std::string text);
	
	bool isFocused(void);
	bool canFocus(void);
	
private:
	bool			changed;

	bool			hasFocus;
	bool			isReverse;
	bool			isChecked;
	
	ComponentType	type;
	int				posX, posY;
	int				maxLen;
	std::string		text;
	
	int				cursorPos;
	int				textLength;
	
	TFonEnter		*onEnter;
	
	void terminal_addGoto(char *bfr, int x, int y);				// then add +4 to bfr
	void terminal_addReverse(char *bfr, bool onNotOff);			// then add +2 to bfr
	void terminal_addGotoCurrentCursor(char *bfr, int &cnt);	// then add +cnt to bfr (might be 0 or 4)
	
	void handleEditLineKeyPress(char vkey, char key);
};

