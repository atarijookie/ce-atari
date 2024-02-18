//--------------------------------------------------
#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <mint/linea.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "stdlib.h"
#include "extension.h"

#define BUFFER_SIZE (512 + 2)       // extra 2 bytes, because pBuffer will be aligned to even address
uint8_t myBuffer[BUFFER_SIZE];
uint8_t *pBuffer;

//--------------------------------------------------
int main(void)
{
    // create buffer pointer to even address 
    uint32_t toEven;
    toEven = (uint32_t) &myBuffer[0];

    if(toEven & 1) {        // not even number?
        toEven++;
    }

    pBuffer = (uint8_t*) toEven; 

    Clear_home();

    //------
    // Before calling any function from the extension, you first must successfully open the extension first.
    // It may take several seconds / minutes, because the extension might be downloaded from internet,
    // it might be compiled or other preparations might be done on first run, and they it takes a little time
    // for it to start. Make sure you either specify long enough timeout time, or try to multiple times.
    (void) Cconws("Opening extension\r\n");
    uint8_t extId = cexOpen("extension_example", "", 5);        // open extension

    if(extId > ANY_ERROR) {     // on error, fail
        (void) Cconws("Opening extension failed\r\n");
        sleep(3);
        return 0;
    }
    
    //------
    // Example call of function with no arguments and no return value. Pointer to buffer specified just to be safe.
    uint8_t res = cexCallRawWrite(extId, "no_in_no_out", 0, 0, 0, pBuffer);     // call function with no arguments and no actual return value

    if(res != STATUS_OK) {
        (void) Cconws("no_in_no_out - return value BAD\r\n");
    } else {
        (void) Cconws("no_in_no_out - return value OK\r\n");
    }

    //------
    // Example call of function with with 1 argument using RAW write call.
    // Status byte can be used for returning the success / failure / some computed value from the function.
    res = cexCallRawWrite(extId, "one_in_one_out", 69, 0, 0, pBuffer);          // call function with 1 argument, value will be returned in status

    if(res != 69) {
        (void) Cconws("one_in_one_out - return value BAD\r\n");
    } else {
        (void) Cconws("one_in_one_out - return value OK\r\n");
    }

    //------
    // Example call of function with multiple arguments using cexCallLong(), which will be placed on buffer according to function signature.
    // The response (data and status byte) will be retrieved with additional call to cexResponse().
    // If the extension needs longer time to generate response than 500 ms, then this cexResponse() function will return STATUS_NO_RESPONSE.
    // Keep calling cexResponse() until you get expected status, then the response will be in pBuffer.
    res = cexCallLong(extId, "sum_of_two", 2, 0x1234, 0x6352);        // call function with long arguments, will return sum in response

    if(res != STATUS_OK) {      // calling function failed?
        (void) Cconws("sum_of_two - call failed\r\n");
    } else {                    // calling function ok
        res = cexResponse(extId,  "sum_of_two", 512, pBuffer);

        if(res != STATUS_OK) {  // getting response failed?
            (void) Cconws("sum_of_two - cexResponse failed\r\n");
        } else {
            uint32_t* pSum = (uint32_t*) pBuffer;
            uint32_t sum = *pSum;

            if(sum != (0x1234 + 0x6352)) {      // sum value wrong?
                (void) Cconws("sum_of_two - sum BAD\r\n");
            } else {
                (void) Cconws("sum_of_two - sum OK\r\n");
            }
        }
    }

    //------
    // Example call of function with string argument using cexCallLong(). String will be placed on buffer according to function signature.
    // The response (data and status byte) will be retrieved with additional call to cexResponse().
    // If the extension needs longer time to generate response than 500 ms, then this cexResponse() function will return STATUS_NO_RESPONSE.
    // Keep calling cexResponse() until you get expected status, then the response will be in pBuffer.
    res = cexCallLong(extId, "reverse_str", 1, "This is a string");        // call function with long arguments, will return reversed string

    if(res != STATUS_OK) {      // calling function failed?
        (void) Cconws("reverse_str - call failed\r\n");
    } else {                    // calling function ok
        res = cexResponse(extId,  "reverse_str", 512, pBuffer);

        if(res != STATUS_OK) {  // getting response failed?
            (void) Cconws("reverse_str - cexResponse failed\r\n");
        } else {
            if(strncmp((char*) pBuffer, "gnirts a si sihT", 16) != 0) {     // wrong string returned?
                (void) Cconws("reverse_str - string BAD\r\n");
            } else {
                (void) Cconws("reverse_str - string OK\r\n");
            }
        }
    }

    //------
    // Example call of function with Atari path in a string argument using cexCallLong().
    // Path will be translated from Atari path to host path, so the extension will get path that is valid on host (linux).
    // The response (data and status byte) will be retrieved with additional call to cexResponse().
    // If the extension needs longer time to generate response than 500 ms, then this cexResponse() function will return STATUS_NO_RESPONSE.
    // Keep calling cexResponse() until you get expected status, then the response will be in pBuffer.
    res = cexCallLong(extId, "fun_path_in", 1, "O:\\TESTS\\CE_HDD.TOS");    // call function with long arguments, will translated path

    if(res != STATUS_OK) {      // calling function failed?
        (void) Cconws("fun_path_in - call failed\r\n");
    } else {                    // calling function ok
        res = cexResponse(extId,  "fun_path_in", 512, pBuffer);

        if(res != STATUS_OK) {  // getting response failed?
            (void) Cconws("fun_path_in - cexResponse failed\r\n");
        } else {
            if(strncmp((char*) pBuffer, "O:\\TESTS\\CE_HDD.TOS", 19) != 0) {     // wrong string returned?
                (void) Cconws("fun_path_in - string BAD\r\n");
            } else {
                (void) Cconws("fun_path_in - string OK\r\n");
            }
        }
    }

    //------
    // Example call of function using cexCallRawWrite(). This is faster than calling cexCallLong(), but there
    // are only 2 arguments of size uint8_t allowed, data from pBuffer will be sent (written) to extension and
    // response is only the status byte. No processing of data will be done before sending.

    // fill buffer with some values which will be summed together with arg1 and arg2
    memset(pBuffer, 0, 512);
    int i;

    for(i=0; i<5; i++) {
        pBuffer[i] = i;     // 0 1 2 3 4
    }

    res = cexCallRawWrite(extId, "raw_data_in", 2, 5, 512, pBuffer);        // call raw write function

    if(res != 17) {
        (void) Cconws("raw_data_in - return value BAD\r\n");
    } else {
        (void) Cconws("raw_data_in - return value OK\r\n");
    }

    //------
    // Example call of function using cexCallRawRead(). This is faster than calling cexCallLong(), but there
    // are only 2 arguments of size uint8_t allowed, and on success pBuffer will be filled by extension,
    // status byte is also available. No processing of data will be done after receiving.

    res = cexCallRawRead(extId, "raw_data_out", 10, 69, 512, pBuffer);      // call raw read function

    if(res != STATUS_OK) {
        (void) Cconws("raw_data_out - return value BAD\r\n");
    } else {
        uint8_t allOk = 1;

        for(i=0; i<512; i++) {
            if((i < 10 && pBuffer[i] != 69) || (i >= 10 && pBuffer[i] != 0)) {  // values 0-9 must be 69, other vlaues must be 0
                allOk = 0;
                break;
            }
        }

        if(allOk) {     // on success
            (void) Cconws("raw_data_out - returned data OK\r\n");
        } else {        // on failure
            (void) Cconws("raw_data_out - returned data BAD\r\n");
        }
    }

    //------
    // When you finish working with your extension, you should close it, so it doesn't consume any additional host resources.
    // On the other hand, in some specific case you might want to leave the extension running for next fast opening of extension
    // (until the host restarts and thus kills all the extensions).
    (void) Cconws("Closing extension.\r\n");
    cexClose(extId);        // close extension

    (void) Cconws("Terminatimg example.\r\n");
    sleep(3);
    return 0;
}
