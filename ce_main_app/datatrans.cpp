#include "debug.h"
#include "utils.h"
#include "gpio.h"
#include "datatrans.h"

DataTrans::DataTrans()
{
    retryMod = new RetryModule();
}

DataTrans::~DataTrans()
{
    if(retryMod) {
        delete retryMod;
        retryMod = NULL;
    }
}
