CC = iccavr
CFLAGS =  -IC:\icc\include\ -e -DATMEGA -DATMega16  -l -g -Mavr_enhanced 
ASFLAGS = $(CFLAGS)  -Wa-g
LFLAGS =  -LC:\icc\lib\ -g -ucrtatmega.o -bfunc_lit:0x54.0x4000 -dram_end:0x45f -bdata:0x60.0x45f -dhwstk_size:16 -beeprom:1.512 -fihx_coff -S2
FILES = init.o main.o sd_raw.o 

sd_tos:	$(FILES)
	$(CC) -o sd_tos $(LFLAGS) @sd_tos.lk   -lcatmega
init.o: C:/icc/include/iom16v.h C:/icc/include/macros.h Z:\LinuxShare\SdTos/global.h
init.o:	Z:\LinuxShare\SdTos\init.c
	$(CC) -c $(CFLAGS) Z:\LinuxShare\SdTos\init.c
main.o: C:/icc/include/stdio.h C:/icc/include/stdarg.h C:/icc/include/_const.h Z:\LinuxShare\SdTos/init.h C:/icc/include/iom16v.h C:/icc/include/macros.h Z:\LinuxShare\SdTos/global.h Z:\LinuxShare\SdTos/sd_raw.h Z:\LinuxShare\SdTos/sd_raw_config.h\
 Z:\LinuxShare\SdTos/global.h Z:\LinuxShare\SdTos/sd_raw_config.h
main.o:	Z:\LinuxShare\SdTos\main.c
	$(CC) -c $(CFLAGS) Z:\LinuxShare\SdTos\main.c
sd_raw.o: C:/icc/include/string.h C:/icc/include/_const.h C:/icc/include/iom16v.h C:/icc/include/macros.h Z:\LinuxShare\SdTos/global.h Z:\LinuxShare\SdTos/sd_raw.h Z:\LinuxShare\SdTos/sd_raw_config.h Z:\LinuxShare\SdTos/global.h
sd_raw.o:	Z:\LinuxShare\SdTos\sd_raw.c
	$(CC) -c $(CFLAGS) Z:\LinuxShare\SdTos\sd_raw.c
