#ifndef _KEYBJOYS_H_
#define _KEYBJOYS_H_

#include "keytranslator.h"

typedef struct {
    int up;
    int down;
    int left;
    int right;
    int button;
} TJoyKeys;

typedef struct {
    TJoyKeys human;             // human readable keys, e.g. 'A' 
    TJoyKeys linuxx;            // linux key event codes, like KEY_A
    TJoyKeys atari;             // atari key codes, like 0x1e
} TJoyKeysPcSt;

class TKeybJoyKeys {
public:
    TKeybJoyKeys(void);
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
    
private:    
    TJoyKeysPcSt   joyKeys[2];
    KeyTranslator *keyTranslator;

    void loadKeys(int joyNumber);
    void saveKeys(int joyNumber);
    
    TJoyKeys *getkeybJoysStruct(bool pcNotSt, int joyNumber);
};

#endif
