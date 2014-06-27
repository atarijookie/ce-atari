#ifndef _STUPIDVECTOR_H_
#define _STUPIDVECTOR_H_

#define STUPIDVECTOR_MAXITEMS   100

class StupidVector {
public:
    StupidVector(void);

    int size(void);
    void clear(void);
    void push_back(void *what);
    void *operator[] (const int index);
    
private:
    int count;
    
    void *items[STUPIDVECTOR_MAXITEMS];
};

#endif