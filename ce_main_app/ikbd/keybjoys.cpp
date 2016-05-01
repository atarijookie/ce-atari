#include <algorithm>
#include <string>

#include "keybjoys.h"
#include "../settings.h"
#include "../debug.h"

#define KEYBOARD_KEYS_SETTINGS0 "KEYBOARD_KEYS_JOY0"
#define KEYBOARD_KEYS_SETTINGS1 "KEYBOARD_KEYS_JOY1"

KeybJoyKeys::KeybJoyKeys(void)
{
    keyTranslator = NULL;
}

void KeybJoyKeys::setKeyTranslator(KeyTranslator *keyTrans)
{
    keyTranslator = keyTrans;
}

JoyKeys *KeybJoyKeys::getkeybJoysStruct(bool pcNotSt, int joyNumber)
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

bool KeybJoyKeys::isKeybJoyKey(bool pcNotSt, int joyNumber, int key)
{
    if(joyNumber < 0 || joyNumber > 1) {        // bad joy number? fail
        return false;
    }

    // with which structure we should compare it?
    JoyKeys *keys = getkeybJoysStruct(pcNotSt, joyNumber);

    // if got the key
    if(keys->up == key || keys->down == key || keys->left == key || keys->right == key || keys->button == key) {
        return true;
    }
 
    // if ain't got the key
    return false;
}    
    
bool KeybJoyKeys::isKeybJoyKeyPc(int joyNumber, int pcKey)
{
    return isKeybJoyKey(true, joyNumber, pcKey);            // for PC (linux) keys
}

bool KeybJoyKeys::isKeybJoyKeyAtari(int joyNumber, int stKey)
{
    return isKeybJoyKey(false, joyNumber, stKey);           // for Atari keys
}

bool KeybJoyKeys::isKeyUp(bool pcNotSt, int joyNumber, int pcKey)
{
    JoyKeys *keys = getkeybJoysStruct(pcNotSt, joyNumber);
    return (keys->up == pcKey);
}

bool KeybJoyKeys::isKeyDown(bool pcNotSt, int joyNumber, int pcKey)
{
    JoyKeys *keys = getkeybJoysStruct(pcNotSt, joyNumber);
    return (keys->down == pcKey);
}

bool KeybJoyKeys::isKeyLeft(bool pcNotSt, int joyNumber, int pcKey)
{
    JoyKeys *keys = getkeybJoysStruct(pcNotSt, joyNumber);
    return (keys->left == pcKey);
}

bool KeybJoyKeys::isKeyRight(bool pcNotSt, int joyNumber, int pcKey)
{
    JoyKeys *keys = getkeybJoysStruct(pcNotSt, joyNumber);
    return (keys->right == pcKey);
}

bool KeybJoyKeys::isKeyButton(bool pcNotSt, int joyNumber, int pcKey)
{
    JoyKeys *keys = getkeybJoysStruct(pcNotSt, joyNumber);
    return (keys->button == pcKey);
}

void KeybJoyKeys::loadKeys(int joyNumber)
{
    // prepare pointers to right things for the joyNumber
    const char      *settingsName   = (joyNumber == 0) ? KEYBOARD_KEYS_SETTINGS0    : KEYBOARD_KEYS_SETTINGS1;  // what settings name we should use?
    const char      *defaultKeys    = (joyNumber == 0) ? "ASDWQ"                    : "JKLIB";                  // what are the default settings, if we don't have anything stored?
    JoyKeysPcSt     *keysStruct     = (joyNumber == 0) ? &joyKeys[0]                : &joyKeys[1];              // to which structure we should store it?
    
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
        Debug::out(LOG_ERROR, "KeybJoyKeys::loadKeys -- keyTranslator is NULL, fail!");
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

void KeybJoyKeys::saveKeys(int joyNumber)
{
    Settings s;

    const char  *settingsName   = (joyNumber == 0) ? KEYBOARD_KEYS_SETTINGS0    : KEYBOARD_KEYS_SETTINGS1;  // what settings name we should use?
    JoyKeys     *keysStruct     = (joyNumber == 0) ? &joyKeys[0].human          : &joyKeys[1].human;        // from which structure we should save it?
    
    char keysString[6];
    
    keysString[0] = keysStruct->left;
    keysString[1] = keysStruct->down;
    keysString[2] = keysStruct->right;
    keysString[3] = keysStruct->up;
    keysString[4] = keysStruct->button;
    keysString[5] = 0;
    
    s.setString(settingsName, keysString);    
}

void KeybJoyKeys::loadKeys(void)
{
    loadKeys(0);
    loadKeys(1);
}

void KeybJoyKeys::saveKeys(void)
{
    saveKeys(0);
    saveKeys(1);
}

bool KeybJoyKeys::keybJoyHumanSettingsValidForSingleJoy(int joyNumber)
{
    JoyKeys *keysStruct = (joyNumber == 0) ? &joyKeys[0].human : &joyKeys[1].human;     // which structure we should check?
    
    // first construct a string out of individual keys
    std::string allKeys = "     ";
    allKeys[0] = keysStruct->up;
    allKeys[1] = keysStruct->down;
    allKeys[2] = keysStruct->left;
    allKeys[3] = keysStruct->right;
    allKeys[4] = keysStruct->button;
    
    // see if that key isn't used mode than once
    int c0 = std::count(allKeys.begin(), allKeys.end(), allKeys[0]);
    int c1 = std::count(allKeys.begin(), allKeys.end(), allKeys[1]);
    int c2 = std::count(allKeys.begin(), allKeys.end(), allKeys[2]);
    int c3 = std::count(allKeys.begin(), allKeys.end(), allKeys[3]);
    int c4 = std::count(allKeys.begin(), allKeys.end(), allKeys[4]);
    
    // used more than once? fail
    if(c0 > 1 || c1 > 1 || c2 > 1 || c3 > 1 || c4 > 1) {
        return false;
    }
    
    // if some key is unspecified, fail
    if(keysStruct->up == 0 || keysStruct->down == 0 || keysStruct->left == 0 || keysStruct->right == 0 || keysStruct->button == 0) {
        return false;
    }
    
    // everything fine
    return true;
}

bool KeybJoyKeys::keybJoyHumanSettingsValidBetweenJoys(void)
{
    JoyKeys *k0 = &joyKeys[0].human;
    JoyKeys *k1 = &joyKeys[1].human;
    
    // put all chars into one string
    std::string allKeys = "          ";
    allKeys[0] = k0->up;
    allKeys[1] = k0->down;
    allKeys[2] = k0->left;
    allKeys[3] = k0->right;
    allKeys[4] = k0->button;

    allKeys[5] = k1->up;
    allKeys[6] = k1->down;
    allKeys[7] = k1->left;
    allKeys[8] = k1->right;
    allKeys[9] = k1->button;
    
    // see if that key isn't used mode than once
    int c0 = std::count(allKeys.begin(), allKeys.end(), allKeys[0]);
    int c1 = std::count(allKeys.begin(), allKeys.end(), allKeys[1]);
    int c2 = std::count(allKeys.begin(), allKeys.end(), allKeys[2]);
    int c3 = std::count(allKeys.begin(), allKeys.end(), allKeys[3]);
    int c4 = std::count(allKeys.begin(), allKeys.end(), allKeys[4]);
    
    // used more than once? fail
    if(c0 > 1 || c1 > 1 || c2 > 1 || c3 > 1 || c4 > 1) {
        return false;
    }

    // everything fine
    return true;
}


    