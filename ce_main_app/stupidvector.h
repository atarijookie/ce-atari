#ifndef _STUPIDVECTOR_H_
#define _STUPIDVECTOR_H_

#define STUPIDVECTOR_MAXITEMS   1000

class StupidVector {
public:
    StupidVector(void);

    int  size(void);
    bool empty(void);
    void clear(void);

    void* front    (void);
    void  pop_front(void);
    
    void* back     (void);
    void  push_back(void *what);
    void  pop_back (void);
    
    void *operator[] (const int index);
    
private:
    int count;
    
    void *items[STUPIDVECTOR_MAXITEMS];
};

#endif