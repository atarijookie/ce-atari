TARGET	= flash_stm32

CC		= gcc
HDR		= parsers/binary.h parsers/hex.h parser.h serial.h stm32.h utils.c gpio.h
CSRC	= parsers/binary.c parsers/hex.c main.c serial_common.c serial_posix.c stm32.c utils.c gpio.c
CFLAGS	= -g -Wall
LDFLAGS	= -lbcm2835

$(TARGET): $(CSRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(CSRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) *~ *.o

