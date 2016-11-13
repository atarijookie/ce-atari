#include <algorithm>
#include <string>
#include <vector>

#include "keybjoys.h"
#include "../settings.h"
#include "../debug.h"
#include "../utils.h"

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
    const char      *settingsName   = (joyNumber == 0) ? KEYBOARD_KEYS_SETTINGS0    : KEYBOARD_KEYS_SETTINGS1;      // what settings name we should use?
    const char      *defaultKeys    = (joyNumber == 0) ? "A%S%D%W%LSHIFT"           : "LEFT%DOWN%RIGHT%UP%RSHIFT";  // what are the default settings, if we don't have anything stored?
    JoyKeysPcSt     *keysStruct     = (joyNumber == 0) ? &joyKeys[0]                : &joyKeys[1];                  // to which structure we should store it?
    
    // get the settings
    Settings s;
    char *keys = s.getString(settingsName, defaultKeys);

    // store the human readable keys directly to struct
    std::vector<std::string> keyElems;
    Utils::splitString(keys, '%', keyElems);
    if (keyElems.size() != 5) {
        // old format
        keysStruct->human.left   = keys[0];
        keysStruct->human.down   = keys[1];
        keysStruct->human.right  = keys[2];
        keysStruct->human.up     = keys[3];
        keysStruct->human.button = keys[4];
    } else {
        keysStruct->human.left   = keyElems[0];
        keysStruct->human.down   = keyElems[1];
        keysStruct->human.right  = keyElems[2];
        keysStruct->human.up     = keyElems[3];
        keysStruct->human.button = keyElems[4];
    }
    
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

    const char   *settingsName   = (joyNumber == 0) ? KEYBOARD_KEYS_SETTINGS0    : KEYBOARD_KEYS_SETTINGS1;  // what settings name we should use?
    JoyKeysHuman *keysStruct     = (joyNumber == 0) ? &joyKeys[0].human          : &joyKeys[1].human;        // from which structure we should save it?

    // using '%' as a separator should be safe as we don't really support SHIFT + KEY mapping...
    std::string keysString;
    keysString += keysStruct->left;
    keysString += "%";
    keysString += keysStruct->down;
    keysString += "%";
    keysString += keysStruct->right;
    keysString += "%";
    keysString += keysStruct->up;
    keysString += "%";
    keysString += keysStruct->button;
    
    s.setString(settingsName, keysString.c_str());
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
    JoyKeysHuman *keysStruct = (joyNumber == 0) ? &joyKeys[0].human : &joyKeys[1].human;     // which structure we should check?
    
    // first construct a vector out of individual keys
    std::vector<std::string> allKeys;
    allKeys.push_back(keysStruct->up);
    allKeys.push_back(keysStruct->down);
    allKeys.push_back(keysStruct->left);
    allKeys.push_back(keysStruct->right);
    allKeys.push_back(keysStruct->button);
    
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
    if(keysStruct->up.empty() || keysStruct->down.empty() || keysStruct->left.empty() || keysStruct->right.empty() || keysStruct->button.empty()) {
        return false;
    }
    
    // everything fine
    return true;
}

bool KeybJoyKeys::keybJoyHumanSettingsValidBetweenJoys(void)
{
    JoyKeysHuman *k0 = &joyKeys[0].human;
    JoyKeysHuman *k1 = &joyKeys[1].human;
    
    // put all chars into one vector
    std::vector<std::string> allKeys;
    allKeys.push_back(k0->up);
    allKeys.push_back(k0->down);
    allKeys.push_back(k0->left);
    allKeys.push_back(k0->right);
    allKeys.push_back(k0->button);

    allKeys.push_back(k1->up);
    allKeys.push_back(k1->down);
    allKeys.push_back(k1->left);
    allKeys.push_back(k1->right);
    allKeys.push_back(k1->button);
    
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


    
