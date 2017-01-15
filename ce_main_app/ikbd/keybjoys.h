#ifndef _KEYBJOYS_H_
#define _KEYBJOYS_H_

#include <string>

#include "keytranslator.h"

typedef struct {
    int up;
    int down;
    int left;
    int right;
    int button;
} JoyKeys;

typedef struct {
    std::string up;
    std::string down;
    std::string left;
    std::string right;
    std::string button;
} JoyKeysHuman;

typedef struct {
    JoyKeysHuman human;             // human readable keys, e.g. "A" or "LCTRL"
    JoyKeys      linuxx;            // linux key event codes, like KEY_A
    JoyKeys      atari;             // atari key codes, like 0x1e
} JoyKeysPcSt;

class KeybJoyKeys {
public:
    KeybJoyKeys(void);
    void setKeyTranslator(KeyTranslator *keyTrans);
    
    bool isKeybJoyKey       (bool pcNotSt, int joyNumber, int key);
    bool isKeybJoyKeyPc     (int joyNumber, int pcKey);
    bool isKeybJoyKeyAtari  (int joyNumber, int pcKey);

    bool isKeyUp    (bool pcNotSt, int joyNumber, int pcKey);
    bool isKeyDown  (bool pcNotSt, int joyNumber, int pcKey);
    bool isKeyLeft  (bool pcNotSt, int joyNumber, int pcKey);
    bool isKeyRight (bool pcNotSt, int joyNumber, int pcKey);
    bool isKeyButton(bool pcNotSt, int joyNumber, int pcKey);
    
    void loadKeys(void);
    void saveKeys(void);
    
    bool keybJoyHumanSettingsValidForSingleJoy(int joyNumber);
    bool keybJoyHumanSettingsValidBetweenJoys(void);
    
    JoyKeysPcSt   joyKeys[2];
private:    
    KeyTranslator *keyTranslator;

    void loadKeys(int joyNumber);
    void saveKeys(int joyNumber);
    
    JoyKeys *getkeybJoysStruct(bool pcNotSt, int joyNumber);
};

#endif
