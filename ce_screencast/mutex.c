#include "mutex.h"

/*
inline BYTE mutex_lock(mutex *mtx){
	mutex test=*mtx=1;
	if( test!=0 ){
		return FALSE;
	}
	return TRUE;
}
*/

inline void mutex_unlock(mutex *mtx){
	*mtx=0;
}