#!/bin/sh

# write hans or franz firmware
# $1 - 0 for Hans, 1 for Franz

echo "update_stm32         : START"

[ "$1" != "0" ] && [ "$1" != "1" ] && echo "update_stm32         : Franz or Hans selection not made, fail!" && exit 1

[ -f /dev/serial0 ] && PORT="/dev/serial0" || PORT="/dev/ttyAMA0"   # if symlink serial0 exists, use it, otherwise try to use ttyAMA0
echo "update_stm32 port    : $PORT"

[ "$1" = "0" ] && SWITCH='-x' || SWITCH='-y'                                        # -x for Hans, -y for Franz
[ "$1" = "0" ] && HEXFILE='/ce/update/hans.hex' || HEXFILE='/ce/update/franz.hex'   # hans.hex or franz.hex

CMD="/ce/update/flash_stm32.elf $SWITCH -w $HEXFILE $PORT"
echo "update_stm32 command : $CMD"
eval $CMD

echo "update_stm32         : END"
