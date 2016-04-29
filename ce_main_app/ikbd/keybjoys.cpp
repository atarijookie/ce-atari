#include "keybjoys.h"
#include "../settings.h"
#include "../debug.h"

#define KEYBOARD_KEYS_SETTINGS0 "KEYBOARD_KEYS_JOY0"
#define KEYBOARD_KEYS_SETTINGS1 "KEYBOARD_KEYS_JOY1"

TKeybJoyKeys::TKeybJoyKeys(void)
{
    keyTranslator = NULL;
}

void TKeybJoyKeys::setKeyTranslator(KeyTranslator *keyTrans)
{
    keyTranslator = keyTrans;
}

TJoyKeys *TKeybJoyKeys::getkeybJoysStruct(bool pcNotSt, int joyNumber)
{
    if(joyNumber < 0 || joyNumber > 1) {        // bad joy number? fail
        return NULL;
    }

    if(pcNotSt) {   // for PC (linux) keys
        return &joyKeys[joyNumber].linuxx;
    } else {        // for Atari keys
        return &joyKeys[joyNumber].atari;
    }
}

bool TKeybJoyKeys::isKeybJoyKey(bool pcNotSt, int joyNumber, int key)
{
    if(joyNumber < 0 || joyNumber > 1) {        // bad joy number? fail
        return false;
    }

    // with which structure we should compare it?
    TJoyKeys *keys = getkeybJoysStruct(pcNotSt, joyNumber);

    // if got the key
    if(keys->up == key || keys->down == key || keys->left == key || keys->right == key || keys->button == key) {
        return true;
    }
 
    // if ain't got the key
    return false;
}    
    
bool TKeybJoyKeys::isKeybJoyKeyPc(int joyNumber, int pcKey)
{
    return isKeybJoyKey(true, joyNumber, pcKey);            // for PC (linux) keys
}

bool TKeybJoyKeys::isKeybJoyKeyAtari(int joyNumber, int stKey)
{
    return isKeybJoyKey(false, joyNumber, stKey);           // for Atari keys
}

bool TKeybJoyKeys::isKeyUp(bool pcNotSt, int joyNumber, int pcKey)
{
    TJoyKeys *keys = getkeybJoysStruct(pcNotSt, joyNumber);
    return (keys->up == pcKey);
}

bool TKeybJoyKeys::isKeyDown(bool pcNotSt, int joyNumber, int pcKey)
{
    TJoyKeys *keys = getkeybJoysStruct(pcNotSt, joyNumber);
    return (keys->down == pcKey);
}

bool TKeybJoyKeys::isKeyLeft(bool pcNotSt, int joyNumber, int pcKey)
{
    TJoyKeys *keys = getkeybJoysStruct(pcNotSt, joyNumber);
    return (keys->left == pcKey);
}

bool TKeybJoyKeys::isKeyRight(bool pcNotSt, int joyNumber, int pcKey)
{
    TJoyKeys *keys = getkeybJoysStruct(pcNotSt, joyNumber);
    return (keys->right == pcKey);
}

bool TKeybJoyKeys::isKeyButton(bool pcNotSt, int joyNumber, int pcKey)
{
    TJoyKeys *keys = getkeybJoysStruct(pcNotSt, joyNumber);
    return (keys->button == pcKey);
}

void TKeybJoyKeys::loadKeys(int joyNumber)
{
    // prepare pointers to right things for the joyNumber
    const char      *settingsName   = (joyNumber == 0) ? KEYBOARD_KEYS_SETTINGS0    : KEYBOARD_KEYS_SETTINGS1;  // what settings name we should use?
    const char      *defaultKeys    = (joyNumber == 0) ? "ASDWQ"                    : "JKLIB";                  // what are the default settings, if we don't have anything stored?
    TJoyKeysPcSt    *keysStruct     = (joyNumber == 0) ? &joyKeys[0]                : &joyKeys[1];              // to which structure we should store it?
    
    // get the settings
    Settings s;
    char *keys = s.getString(settingsName, defaultKeys);
    
    // store the human readable keys directly to struct
    keysStruct->human.left   = keys[0];
    keysStruct->human.down   = keys[1];
    keysStruct->human.right  = keys[2];
    keysStruct->human.up     = keys[3];
    keysStruct->human.button = keys[4];
    
    // no key translator? fail
    if(!keyTranslator) {
        Debug::out(LOG_ERROR, "TKeybJoyKeys::loadKeys -- keyTranslator is NULL, fail!");
        return;
    }

    // convert human readable keys to linux key event
    keysStruct->linuxx.left   = keyTranslator->humanKeyToPc(keysStruct->human.left);
    keysStruct->linuxx.down   = keyTranslator->humanKeyToPc(keysStruct->human.down);
    keysStruct->linuxx.right  = keyTranslator->humanKeyToPc(keysStruct->human.right);
    keysStruct->linuxx.up     = keyTranslator->humanKeyToPc(keysStruct->human.up);
    keysStruct->linuxx.button = keyTranslator->humanKeyToPc(keysStruct->human.button);
    
    // convert human readable keys to atari key down values
    keysStruct->atari.left   = keyTranslator->humanKeyToSt(keysStruct->human.left);
    keysStruct->atari.down   = keyTranslator->humanKeyToSt(keysStruct->human.down);
    keysStruct->atari.right  = keyTranslator->humanKeyToSt(keysStruct->human.right);
    keysStruct->atari.up     = keyTranslator->humanKeyToSt(keysStruct->human.up);
    keysStruct->atari.button = keyTranslator->humanKeyToSt(keysStruct->human.button);
}

void TKeybJoyKeys::saveKeys(int joyNumber)
{
    Settings s;

    const char      *settingsName   = (joyNumber == 0) ? KEYBOARD_KEYS_SETTINGS0    : KEYBOARD_KEYS_SETTINGS1;  // what settings name we should use?
    TJoyKeysPcSt    *keysStruct     = (joyNumber == 0) ? &joyKeys[0]                : &joyKeys[1];              // from which structure we should save it?
    
    char keys[6];
    
    keys[0] = keysStruct->human.left;
    keys[1] = keysStruct->human.down;
    keys[2] = keysStruct->human.right;
    keys[3] = keysStruct->human.up;
    keys[4] = keysStruct->human.button;
    keys[5] = 0;
    
    s.setString(settingsName, keys);    
}

void TKeybJoyKeys::loadKeys(void)
{
    loadKeys(0);
    loadKeys(1);
}

void TKeybJoyKeys::saveKeys(void)
{
    saveKeys(0);
    saveKeys(1);
}
