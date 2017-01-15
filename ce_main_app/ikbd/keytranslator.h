#ifndef _KEYTRANSLATOR_H_
#define _KEYTRANSLATOR_H_

#include <string>

#define KEY_TABLE_SIZE  256

class KeyTranslator {
public:
    KeyTranslator();

    int pcKeyToSt                   (int pcKey) const;
    const std::string& pcKeyToHuman (int pcKey) const;
    int humanKeyToPc                (const std::string& humanKey) const;
    int humanKeyToSt                (const std::string& humanKey) const;
    int stKeyToPc                   (int stKey) const;

private:
    int         tableKeysPcToSt     [KEY_TABLE_SIZE];
    std::string tableKeysPcToHuman  [KEY_TABLE_SIZE];

    void fillKeyTranslationTable(void);
    void addToTable(int pcKey, int stKey, const std::string& humanKey);
};

#endif
