#include <stdio.h>
#include <stdlib.h>

#include "stupidvector.h"

StupidVector::StupidVector(void)
{
    count = 0;

    clear();
}

int StupidVector::size(void)
{
    return count;
}

void StupidVector::push_back(void *what)
{
    if(count >= STUPIDVECTOR_MAXITEMS) {        // out of space? quit
        return;
    }    
    
    items[count] = what;
    count++;
}

void* StupidVector::operator[] (const int index)
{
    if(index >= STUPIDVECTOR_MAXITEMS) {        // out of range? quit
        return NULL;
    }    

    return items[index];
}

void StupidVector::clear(void)
{
    for(int i=0; i<STUPIDVECTOR_MAXITEMS; i++) {
        items[i] = NULL;
    }
    
    count = 0;
}
