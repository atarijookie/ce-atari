#include "json.h"
#include "extensiondefs.h"
#include "main.h"

using json = nlohmann::json;

// Function with no arguments in and no return value.
// Even though args and resp are present, args will be empty and resp will be ignored.
void no_in_no_out(json args, ResponseFromExtension* resp)
{
    printf("no_in_no_out was called\n");
}

// Function with 1 in argument, returned back as status byte.
void one_in_one_out(json args, ResponseFromExtension* resp)
{
    printf("one_in_one_out was called\n");

    resp->statusByte = args.at(0);
}

// Add two numbers together, store the result in buffer.
void sum_of_two(json args, ResponseFromExtension* resp)
{
    printf("sum_of_two was called\n");

    int16_t a = args.at(0);         // 0th argument
    int16_t b = args.at(1);         // 1st argument
    int32_t sum = a + b;            // add those together
    storeDword(resp->data, sum);    // store in buffer in expected endian

    responseStoreStatusAndDataLen(resp, STATUS_OK, 4);  // OK status, 4 bytes of response data
}

// String going in, being reversed and returned as binary data buffer
void reverse_str(json args, ResponseFromExtension* resp)
{
    printf("reverse_str was called\n");

    std::string str = args.at(0);               // input string as 0th argument 

    std::reverse(str.begin(), str.end());       // reverse it
    strcpy((char*) resp->data, str.c_str());    // store it to response data

    responseStoreStatusAndDataLen(resp, STATUS_OK, str.length() + 1);   // set OK status and length of response
}

// Path supplied will be returned as path again.
void fun_path_in(json args, ResponseFromExtension* resp)
{
    printf("fun_path_in was called\n");

    std::string str = args.at(0);               // get 0th argument
    strcpy((char*) resp->data, str.c_str());    // copy it to response buffer

    responseStoreStatusAndDataLen(resp, STATUS_OK, str.length() + 1);   // set OK status and length of response
}

/*
Function called for raw data input. 
Function arguments ARE FIXED - don't change their count. 
RAW WRITE always sends:
  args: [TYPE_UINT8, TYPE_UINT8, TYPE_BIN_DATA]
  response: RESP_TYPE_STATUS
*/
void raw_data_in(json args, ResponseFromExtension* resp)
{
    printf("raw_data_in was called\n");

    uint8_t a = args.at(0);     // 0th argument - cmd4 (uint8_t)
    uint8_t b = args.at(1);     // 1st argument - cmd5 (uint8_t)

    uint8_t sum = a + b;        // add those together first

    // The binary buffer was passed to this extension before sending the JSON, we currently just copied it to latestData
    for(int i=0; i<latestDataLen; i++) {            // add all the latest received data to result
        sum += latestData[i];
    }

    responseStoreStatusAndDataLen(resp, sum, 0);    // instead of status we return sum of args and buffer, no response data
}

/*
Function called for raw data output - will add cmd4 count of bytes to response with value specified in cmd5
Function arguments ARE FIXED - don't change their count. 
RAW WRITE always sends:
  args: [TYPE_UINT8, TYPE_UINT8]
  response: RESP_TYPE_STATUS_BIN_DATA
*/
void raw_data_out(json args, ResponseFromExtension* resp)
{
    printf("raw_data_out was called\n");

    uint8_t count = args.at(0);     // 0th argument - cmd4 - count of items that should be placed on buffer
    uint8_t value = args.at(1);     // 1st argument - cmd5 - value that should be placed on buffer count-times

    for(int i=0; i<count; i++) {    // generate cmd4 count of bytes with value stored in cmd5
        resp->data[i] = value;
    }

    responseStoreStatusAndDataLen(resp, STATUS_OK, count);  // return OK status and how many bytes we've placed on buffer
}

/*
Place the functions which should be exported in this function. 
You must specify for each function:
    - exported function name
    - function call type (where the args are, also read / write direction when calling)
    - argument types (what argument will be stored and later retrieved from the buffer / cmd[4] cmd[5])
    - arguments count
    - return value type
*/
void exportFunctionSignatures(void)
{
    // this function has no arguments, returns nothing
    addFunctionSignature((void*) no_in_no_out, "no_in_no_out", FUNC_RAW_WRITE, NULL, 0, RESP_TYPE_NONE);
 
    // function expects 1 uint8 argument, returns uint8
    uint8_t args1[1] = {TYPE_UINT8};
    addFunctionSignature((void*) one_in_one_out, "one_in_one_out", FUNC_RAW_WRITE, args1, 1, RESP_TYPE_STATUS);

    // 2 arguments in, int16 out
    uint8_t args2[2] = {TYPE_INT16, TYPE_INT16};
    addFunctionSignature((void*) sum_of_two, "sum_of_two", FUNC_LONG_ARGS, args2, 2, RESP_TYPE_STATUS_BIN_DATA);

    // string as argument, string as return value
    uint8_t args3[1] = {TYPE_CSTRING};
    addFunctionSignature((void*) reverse_str, "reverse_str", FUNC_LONG_ARGS, args3, 1, RESP_TYPE_STATUS_BIN_DATA);

    // string path in, string as return value
    uint8_t args4[1] = {TYPE_PATH};
    addFunctionSignature((void*) fun_path_in, "fun_path_in", FUNC_LONG_ARGS, args4, 1, RESP_TYPE_STATUS_PATH);

    // raw data in can have 2 input bytes (from cmd4 and cmd5) and bin data, returns just status
    uint8_t args5[3] = {TYPE_UINT8, TYPE_UINT8, TYPE_BIN_DATA};
    addFunctionSignature((void*) raw_data_in, "raw_data_in", FUNC_RAW_WRITE, args5, 3, RESP_TYPE_STATUS);

    // raw data out can have 2 input bytes (from cmd4 and cmd5), returns bin data and status
    uint8_t args6[2] = {TYPE_UINT8, TYPE_UINT8};
    addFunctionSignature((void*) raw_data_out, "raw_data_out", FUNC_RAW_READ, args6, 2, RESP_TYPE_STATUS_BIN_DATA);
}
