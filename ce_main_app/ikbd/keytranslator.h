#ifndef _KEYTRANSLATOR_H_
#define _KEYTRANSLATOR_H_

#define KEY_TABLE_SIZE  256

class KeyTranslator {
    public:
        KeyTranslator();
    
        int pcKeyToSt       (int pcKey);
        int pcKeyToHuman    (int pcKey);
        int humanKeyToPc    (int humanKey);
        int humanKeyToSt    (int humanKey);
        int stKeyToPc       (int stKey);
    
    private:
        int  tableKeysPcToSt     [KEY_TABLE_SIZE];
        int  tableKeysPcToHuman  [KEY_TABLE_SIZE];

        void fillKeyTranslationTable(void);
        void addToTable(int pcKey, int stKey, int humanKey=0);
};

#endif
