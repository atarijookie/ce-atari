#include <string.h>
#include <linux/input.h>
#include "keytranslator.h"
#include "../debug.h"

KeyTranslator::KeyTranslator(void)
{
    fillKeyTranslationTable();
}

void KeyTranslator::fillKeyTranslationTable(void)
{
    memset(tableKeysPcToSt,     0, sizeof(tableKeysPcToSt));
    memset(tableKeysPcToHuman,  0, sizeof(tableKeysPcToHuman));
    
    addToTable(KEY_ESC,         0x01);
    addToTable(KEY_1,           0x02, '1');
    addToTable(KEY_2,           0x03, '2');
    addToTable(KEY_3,           0x04, '3');
    addToTable(KEY_4,           0x05, '4');
    addToTable(KEY_5,           0x06, '5');
    addToTable(KEY_6,           0x07, '6');
    addToTable(KEY_7,           0x08, '7');
    addToTable(KEY_8,           0x09, '8');
    addToTable(KEY_9,           0x0a, '9');
    addToTable(KEY_0,           0x0b, '0');
    addToTable(KEY_MINUS,       0x0c, '-');
    addToTable(KEY_EQUAL,       0x0d, '=');
    addToTable(KEY_BACKSPACE,   0x0e);
    addToTable(KEY_TAB,         0x0f);
    addToTable(KEY_Q,           0x10, 'Q');
    addToTable(KEY_W,           0x11, 'W');
    addToTable(KEY_E,           0x12, 'E');
    addToTable(KEY_R,           0x13, 'R');
    addToTable(KEY_T,           0x14, 'T');
    addToTable(KEY_Y,           0x15, 'Y');
    addToTable(KEY_U,           0x16, 'U');
    addToTable(KEY_I,           0x17, 'I');
    addToTable(KEY_O,           0x18, 'O');
    addToTable(KEY_P,           0x19, 'P');
    addToTable(KEY_LEFTBRACE,   0x1a, '(');
    addToTable(KEY_RIGHTBRACE,  0x1b, ')');
    addToTable(KEY_ENTER,       0x1c);
    addToTable(KEY_LEFTCTRL,    0x1d);
    addToTable(KEY_A,           0x1e, 'A');
    addToTable(KEY_S,           0x1f, 'S');
    addToTable(KEY_D,           0x20, 'D');
    addToTable(KEY_F,           0x21, 'F');
    addToTable(KEY_G,           0x22, 'G');
    addToTable(KEY_H,           0x23, 'H');
    addToTable(KEY_J,           0x24, 'J');
    addToTable(KEY_K,           0x25, 'K');
    addToTable(KEY_L,           0x26, 'L');
    addToTable(KEY_SEMICOLON,   0x27, ';');
    addToTable(KEY_APOSTROPHE,  0x28, '\'');
    addToTable(KEY_GRAVE,       0x2b);
    addToTable(KEY_LEFTSHIFT,   0x2a);
    addToTable(KEY_BACKSLASH,   0x60, '\\');
    addToTable(KEY_Z,           0x2c, 'Z');
    addToTable(KEY_X,           0x2d, 'X');
    addToTable(KEY_C,           0x2e, 'C');
    addToTable(KEY_V,           0x2f, 'V');
    addToTable(KEY_B,           0x30, 'B');
    addToTable(KEY_N,           0x31, 'N');
    addToTable(KEY_M,           0x32, 'M');
    addToTable(KEY_COMMA,       0x33, ',');
    addToTable(KEY_DOT,         0x34, '.');
    addToTable(KEY_SLASH,       0x35, '/');
    addToTable(KEY_RIGHTSHIFT,  0x36);
    addToTable(KEY_KPASTERISK,  0x66, '*');
    addToTable(KEY_LEFTALT,     0x38);
    addToTable(KEY_SPACE,       0x39, ' ');
    addToTable(KEY_CAPSLOCK,    0x3a);
    addToTable(KEY_F1,          0x3b);
    addToTable(KEY_F2,          0x3c);
    addToTable(KEY_F3,          0x3d);
    addToTable(KEY_F4,          0x3e);
    addToTable(KEY_F5,          0x3f);
    addToTable(KEY_F6,          0x40);
    addToTable(KEY_F7,          0x41);
    addToTable(KEY_F8,          0x42);
    addToTable(KEY_F9,          0x43);
    addToTable(KEY_F10,         0x44);
    addToTable(KEY_KP7,         0x67);
    addToTable(KEY_KP8,         0x68);
    addToTable(KEY_KP9,         0x69);
    addToTable(KEY_KPMINUS,     0x4a, '-');
    addToTable(KEY_KP4,         0x6a);
    addToTable(KEY_KP5,         0x6b);
    addToTable(KEY_KP6,         0x6c);
    addToTable(KEY_KPPLUS,      0x4e, '+');
    addToTable(KEY_KP1,         0x6d);
    addToTable(KEY_KP2,         0x6e);
    addToTable(KEY_KP3,         0x6f);
    addToTable(KEY_KP0,         0x70);
    addToTable(KEY_KPDOT,       0x71, '.');
    addToTable(KEY_KPENTER,     0x72);
    addToTable(KEY_RIGHTCTRL,   0x1d);
    addToTable(KEY_KPSLASH,     0x65, '/');
    addToTable(KEY_RIGHTALT,    0x38);
    addToTable(KEY_UP,          0x48);
    addToTable(KEY_LEFT,        0x4b);
    addToTable(KEY_RIGHT,       0x4d);
    addToTable(KEY_DOWN,        0x50);
    addToTable(KEY_HOME,        0x62);
    addToTable(KEY_PAGEUP,      0x61);
    addToTable(KEY_PAGEDOWN,    0x47);
    addToTable(KEY_INSERT,      0x52);
    addToTable(KEY_DELETE,      0x53);
}

void KeyTranslator::addToTable(int pcKey, int stKey, int humanKey)
{
    if(pcKey >= KEY_TABLE_SIZE) {
        Debug::out(LOG_ERROR, "addToTable -- Can't add pair %d - %d - out of range.", pcKey, stKey);
        return;
    }

    tableKeysPcToSt[pcKey]      = stKey;
    tableKeysPcToHuman[pcKey]   = humanKey;
}

int KeyTranslator::pcKeyToSt(int pcKey)
{
    if(pcKey < 0 || pcKey >= KEY_TABLE_SIZE) {      // invalid / too big key? 
        return 0;
    }
    
    return tableKeysPcToSt[pcKey];                  // return the key translated through table
}

int KeyTranslator::pcKeyToHuman(int pcKey)
{
    if(pcKey < 0 || pcKey >= KEY_TABLE_SIZE) {      // invalid / too big key? 
        return ' ';
    }
    
    return tableKeysPcToHuman[pcKey];               // return the key translated through table
}

int KeyTranslator::humanKeyToPc(int humanKey)
{
    int i;
    for(i=0; i<KEY_TABLE_SIZE; i++) {               // go through pcToHuman table, and find which pcKey (index) matches the human key, and return it
        if(tableKeysPcToHuman[i] == humanKey) {
            return i;
        }
    }
    
    return 0;                                       // not found, return space
}

int KeyTranslator::humanKeyToSt(int humanKey)
{
    int pcKey = humanKeyToPc(humanKey);
    int stKey = tableKeysPcToSt[pcKey];
    return stKey;
}

int KeyTranslator::stKeyToPc(int stKey)
{
    int i;
    for(i=0; i<KEY_TABLE_SIZE; i++) {               // go through pcToSt table, and find which pcKey (index) matches the st key, and return it
        if(tableKeysPcToSt[i] == stKey) {
            return i;
        }
    }
    
    return 0;
}
