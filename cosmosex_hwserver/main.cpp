#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <queue>          

#include "gpio.h"
#include "socks.h"

/*
void bcm2835_gpio_write(int, int)
void spi_tx_rx(int whichSpiCS, int count, BYTE *txBuf, BYTE *rxBuf) 
bool spi_atn(int whichSpiAtn)
*/

#define SYNC1           0xab
#define SYNC2           0xcd

#define FUN_GPIO_WRITE  1
#define FUN_SPI_TX_RX   2
#define FUN_SPI_ATN     3


int main(int argc, char *argv[])
 {
    printf("\n\nCosmosEx HW server starting...\n");
 
	if(!gpio_open()) {									        // try to open GPIO and SPI on RPi
		return 0;
	}

    serverSocket_setParams(1111);

    BYTE *bfrRead   = new BYTE[1024*1024];
    BYTE *bfrWrite  = new BYTE[1024*1024];
    
    while(1) {
        int res;
        
        res = serverSocket_read(bfrRead, 6);
        
        if(res != 6) {
            continue;
        }
        
        if(bfrRead[0] != SYNC1 || bfrRead[1] != SYNC2) {
            continue;
        }
        
        switch(bfrRead[2]) {
            case FUN_GPIO_WRITE:
            bcm2835_gpio_write(bfrRead[3], bfrRead[4]);
            break;
            //----------------------------------------------------------
            
            case FUN_SPI_TX_RX:
            {
                int whichCs = bfrRead[3];
                int count;
                count = bfrRead[4];
                count = count << 8;
                count = count | ((WORD) bfrRead[5]);

                res = serverSocket_read(bfrRead + 6, count);
            
                if(res != count) {
                    continue;
                }
            
                bfrWrite[0] = SYNC1;
                bfrWrite[1] = SYNC2;
                spi_tx_rx(whichCs, count, bfrRead + 6, bfrWrite + 2);
            
                serverSocket_write(bfrWrite, count + 2);
            }
            break;
            //----------------------------------------------------------
        
            case FUN_SPI_ATN:
            int whichAtn = bfrRead[3];
            bfrWrite[0] = SYNC1;
            bfrWrite[1] = SYNC2;
            bfrWrite[2] = spi_atn(whichAtn);
            
            serverSocket_write(bfrWrite, 3);
            break;
        }
    }

    delete []bfrRead;
    delete []bfrWrite;
    
    gpio_close();
    printf("\n\nCosmosEx HW server terminated.\n");

    return 0;
 }

