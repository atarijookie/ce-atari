#ifndef _CONFIGCOMPONENT_H_
#define _CONFIGCOMPONENT_H_

#include <stdio.h>
#include <string>

#include "../datatypes.h"

#define TEXT_OPTION_ALLOW_ALL                   7
#define TEXT_OPTION_ALLOW_LETTERS               1
#define TEXT_OPTION_ALLOW_NUMBERS               2
#define TEXT_OPTION_ALLOW_OTHER                 4
#define TEXT_OPTION_ALLOW_DOT                   8
#define TEXT_OPTION_LETTERS_ONLY_UPPERCASE      16

class ConfigComponent;
class ConfigStream;

class ConfigComponent
{
public:
    enum ComponentType{ label, button, checkbox, editline, editline_pass, heartBeat };

    // maxLen is maximum length of text, that means that on screen it might have 2 more ('[' and ']')
    ConfigComponent(ConfigStream *parent, ComponentType type, std::string text, unsigned int maxLen, int x, int y, int gotoOffset);
    void setCheckboxGroupIds(int groupId, int checkboxId);
    void getCheckboxGroupIds(int& groupId, int& checkboxId);
    bool isGroupCheckBox(void);

    void setOnEnterFunctionCode(int onEnter);
    void setOnChBEnterFunctionCode(int onChBEnter);

    void getStream(bool fullNotChange, BYTE *bfr, int &len);
    void setFocus(bool hasFocus);
    void setReverse(bool isReverse);
    void setIsChecked(bool isChecked);
    void onKeyPressed(BYTE key);

    void setText(std::string text);
    void getText(std::string &text);
    void setTextOptions(int newOpts);
    void setLimitedShowSize(unsigned int newShowSize);                // set how many characters we can see at one time

    bool isFocused(void);
    bool isChecked(void);
    bool canFocus(void);

    void setComponentId(int newId);
    int  getComponentId(void);

    int getComponentType(void);

    bool isEditLine(void);

    BYTE *terminal_addGotoCurrentCursor(BYTE *bfr, int &cnt);    // then add +cnt to bfr (might be 0 or 4)

    int getPosX(void);
    int getPosY(void);

    int getCol(void);
    int getRow(void);

private:
    ConfigStream    *confStream;

    bool            changed;

    bool            hasFocus;
    bool            isReverse;
    bool            checked;

    ComponentType   type;
    int             posX, posY;
    int             gotoOffset;
    unsigned int    maxLen;         // maximum number of characters that this component can contain
    std::string     text;

    int             componentId;

    // for editline
    unsigned int    cursorPos;
    int             textOptions;

    // for editline, which has scrollable content
    bool            isLimitedShowSize;
    unsigned int    showSize;           // number of characters that this editline will show (e.g. maxLen is 63 chars, but we want to show only 15 chars at any time, and scroll the rest
    unsigned int    showWindowStart;    // starting character where the shown window content is shown (e.g. when this is 20, maxLen is 63, showSize is 15, the component should show characters 20 to 35)
    
    // for checkbox
    int             checkBoxGroup;
    int             checkBoxId;

    // for heartbeat
    int             heartBeatState;
    
    int onEnter;
    int onChBEnter;

    BYTE *terminal_addGoto(BYTE *bfr, int x, int y);
    BYTE *terminal_addReverse(BYTE *bfr, bool onNotOff);
    BYTE *terminal_addCursorOn(BYTE *bfr, bool on);

    void handleEditLineKeyPress(BYTE key);

    BYTE filterTextKey(BYTE key);
    bool textOptionSet(WORD textOption);
    bool isLetter(BYTE key);
    bool isSmallLetter(BYTE key);
    bool isNumber(BYTE key);
    bool isOther(BYTE key);
    
    void updateShownWindowPositionAccordingToCursor(void);
};

#endif

