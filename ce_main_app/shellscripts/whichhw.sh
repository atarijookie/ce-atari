#!/bin/sh

# test for XC9536 chip
/ce/update/flash_xilinx /ce/update/test_xc9536xl.xsvf  > /dev/null 2> /dev/null

if [ "$?" -eq "0" ]; then       # XC9536XL found, it's v1
    echo "1"
    exit
fi

# test for XC9572 chip
/ce/update/flash_xilinx /ce/update/test_xc9572xl.xsvf  > /dev/null 2> /dev/null

if [ "$?" -eq "0" ]; then       # XC9572XL found, it's v2
    echo "2"
    exit
fi

# test for 10M04 FPGA chip
/ce/update/flash_xilinx /ce/update/test_10m04.xsvf  > /dev/null 2> /dev/null

if [ "$?" -eq "0" ]; then       # 10M04 found, it's v3
    # after running and suceeding the test for 10M04 run the test for any other CPLD, 
    # as that test for 10M04 caused the chip to stop working, and running this test for CPLD
    # puts it in a working condition again.
    /ce/update/flash_xilinx /ce/update/test_xc9536xl.xsvf  > /dev/null 2> /dev/null

    echo "3"
    exit
fi

# if nothing found, return something else
echo "0"
