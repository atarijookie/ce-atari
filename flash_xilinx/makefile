TARGET	= flash_xilinx.elf

OBJS	= gpio.o micro.o lenval.o ports.o
HDRS	= gpio.h micro.h lenval.h ports.h datatypes.h raspberry.h

$(TARGET): $(OBJS) $(HDRS)
	gcc $(OBJS) -lbcm2835 -lrt -o $(TARGET)

%.o: %.c
	gcc -c $<

clean:
	rm -rf *.o $(TARGET)

