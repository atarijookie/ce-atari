#include <stdio.h>
#include <stdlib.h>

#include "stupidvector.h"

StupidVector::StupidVector(void)
{
    count       = 0;

    clear();
}

int StupidVector::size(void)
{
    return count;
}

bool StupidVector::empty(void)
{
    return (count == 0);
}

void* StupidVector::front(void)
{
    if(count <= 0) {
        return 0;
    }
    
    return items[0];
}

void StupidVector::pop_front(void)
{
    if(count <= 0) {                // nothing in vector? quit
        return;
    }
    
    count--;
    for(int i=0; i<count; i++) {    // move items one back
        items[i] = items[i+1];
    }
    items[count] = NULL;            // remove previous last, because it's a copy of current last
}

void* StupidVector::back(void)
{
    if(count <= 0) {
        return 0;
    }
    
    return items[count - 1];
}

void StupidVector::pop_back(void)
{
    if(count <= 0) {                // nothing in vector? quit
        return;
    }
    
    items[count - 1] = NULL;        // remove last item
    count--;
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
