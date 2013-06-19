/*--------------------------------------------------*/
#include <tos.h>
#include <stdio.h>
#include <screen.h>
#include <string.h>
#include <stdlib.h>

#include "dma.h"
#include "system.h"
/*--------------------------------------------------*/
#define BYTE  unsigned char
#define WORD  unsigned int
#define DWORD unsigned long int

/* -------------------------------------- */
 struct mfp_chip {
                  char reg[48]; /* MFP registers are on odd bytes */
                 };
/* -------------------------------------- */
typedef long (*func)();         /* pointer to function returning a long */

/* Logical Drive Info */
struct hd_drv {
            short dev_addr;     /* physical unit ASCI address */
            long  part_start;   /* start sector logical unit */
               } ;
/*--------------------------------------------------*/
BYTE CE_ReadRunningFW(BYTE ACSI_id, BYTE *buffer);

void getScreenStream(void);
void sendKeyDown(BYTE vkey, BYTE key);
/*--------------------------------------------------*/
BYTE      deviceID;

BYTE myBuffer[520];
BYTE *pBuffer;
/*--------------------------------------------------*/
void main(void)
{
  DWORD scancode;
  BYTE key, vkey, res;
  BYTE i;
  WORD timeNow, timePrev;
  DWORD toEven;
 
  /* ---------------------- */
  toEven = (DWORD) &myBuffer[0];
  
  if(toEven & 0x0001)       /* not even number? */
    toEven++;
  
  pBuffer = (BYTE *) toEven; 
  /* ---------------------- */
  /* search for device on the ACSI bus */
  deviceID = 0;

	Clear_home();
	printf("Looking for CosmosEx:\n");

	while(1) {
		for(i=0; i<8; i++) {
			printf("%d", i);
      
			res = CE_ReadRunningFW(i, pBuffer);      /* try to read FW name */
      
			if(res == 1) {                           	/* if found the US */
				deviceID = i;                     		/* store the ACSI ID of device */
				break;
			}
		}
  
		if(res == 1) {                             /* if found, break */
			break;
		}
      
		printf(" - not found.\nPress any key to retry or 'Q' to quit.\n");
		key = Cnecin();
    
		if(key == 'Q' || key=='q') {
			return;
		}
	}
  
	printf("\n\nCosmosEx ACSI ID: %d\nFirmWare: %s", (int) deviceID, (char *)pBuffer);
	/* ----------------- */
	/* use Ctrl + C to quit */
	timePrev = Tgettime();
	
	while(1) {
		res = Bconstat(2);						/* see if there's something waiting from keyboard */
		
		if(res == 0) {							/* nothing waiting from keyboard? */
			timeNow = Tgettime();
			
			if((timeNow - timePrev) > 0) {		/* check if time changed (2 seconds passed) */
				timePrev = timeNow;
				
				getScreenStream();				/* display a new stream (if something changed) */
			}
			
			continue;							/* try again */
		}
	
		scancode = Cnecin();					/* get char form keyboard, no echo on screen */

		vkey	= (scancode>>16)	& 0xff;
		key		=  scancode			& 0xff;

		sendKeyDown(vkey, key);					/* send this key to device */
		getScreenStream();						/* and display the screen */
	}
}
/*--------------------------------------------------*/
void sendKeyDown(BYTE vkey, BYTE key)
{
	/* todo: send a command with these two bytes to CosmosEx device */
	
}
/*--------------------------------------------------*/
void getScreenStream(void)
{
	/* todo: send command over ACSI and receive screen stream to pBuffer */
	
	
	/* now display the buffer */
	Cconws(pBuffer);	
}
/*--------------------------------------------------*/
BYTE CE_ReadRunningFW(BYTE ACSI_id, BYTE *buffer)
{
  WORD res;
  BYTE cmd[] = {0x1f, 0x20, 'U', 'S', 'C', 'u', 'r', 'n', 't', 'F', 'W'};
  
  cmd[0] = 0x1f | (ACSI_id << 5);  
  memset(buffer, 0, 512);               /* clear the buffer */
  
  res = LongRW(1, cmd, buffer);         /* read name and version of current FW */
    
  if(res != OK)                         /* if failed, return FALSE */
    return 0;
    
  return 1;                             /* success */
}
/*--------------------------------------------------*/
BYTE LongRW(BYTE ReadNotWrite, BYTE *cmd, BYTE *buffer)
{
 DWORD status;
 WORD i;
 void *OldSP;

 OldSP = (void *) Super((void *)0);  			/* supervisor mode */ 

 FLOCK = -1;                            /* disable FDC operations */
 setdma((DWORD) buffer);                      /* setup DMA transfer address */

 DMA->MODE = NO_DMA | HDC;              /* write 1st byte (0) with A1 low */
 DMA->DATA = cmd[0];
 DMA->MODE = NO_DMA | HDC | A0;         /* A1 high again */

  for(i=1; i<10; i++)
  {
    if (qdone() != OK)                  /* wait for ack */
    {
      hdone();                          /* restore DMA device to normal */
      Super((void *)OldSP);  			      /* user mode */
      return ERROR;
    }
    
    DMA->DATA = cmd[i];
    DMA->MODE = NO_DMA | HDC | A0;
  }

  if (qdone() != OK)                  /* wait for ack */
  {
    hdone();                          /* restore DMA device to normal */
    Super((void *)OldSP);  			      /* user mode */
    return ERROR;
  }

  if(ReadNotWrite==1)
  {
    DMA->MODE = DMA_WR | NO_DMA | SC_REG;  /* clear FIFO = toggle R/W bit */
    DMA->MODE = NO_DMA | SC_REG;           /* and select sector count reg */ 

    DMA->SECT_CNT = 1;                     /* write sector cnt to DMA device */
    DMA->MODE = NO_DMA | HDC | A0;         /* select DMA data register again */

    DMA->DATA = cmd[10];                   
    DMA->MODE = 0;                         /* start DMA transfer */

    status = endcmd(NO_DMA | HDC | A0);    /* wait for DMA completion */
  }
  else
  {
    DMA->MODE = NO_DMA | SC_REG;           /* clear FIFO = toggle R/W bit */
    DMA->MODE = DMA_WR | NO_DMA | SC_REG;  /* and select sector count reg */

    DMA->SECT_CNT = 1;                     /* write sector cnt to DMA device */
    DMA->MODE = DMA_WR | NO_DMA | HDC | A0;/* select DMA data register again */

    DMA->DATA = cmd[10];                   
    DMA->MODE = DMA_WR;                    /* start DMA transfer */

    status = endcmd(DMA_WR | NO_DMA | HDC | A0); /* wait for DMA completion */
  }

  hdone();                                /* restore DMA device to normal */
  Super((void *)OldSP);  			            /* user mode */
  return status;
}
/****************************************************************************/
long endcmd(short mode)
{
 if (fdone() != OK)                   /* wait for operation done ack */
    return(ERRORL);

 DMA->MODE = mode;                    /* write mode word to mode register */
 return((long)(DMA->DATA & 0x00FF));  /* return completion byte */
}
/****************************************************************************/
long hdone(void)
{
 DMA->MODE = NO_DMA;        /* restore DMA mode register */
 FLOCK = 0;                 /* FDC operations may get going again */
 return((long)DMA->STATUS); /* read and return DMA status register */
}
/****************************************************************************/
void setdma(DWORD addr)
{
 DMA->ADDR[LOW]  = (BYTE)(addr);
 DMA->ADDR[MID]  = (BYTE)(addr >> 8);
 DMA->ADDR[HIGH] = (BYTE)(addr >> 16);
}
/****************************************************************************/
long qdone(void)
{
 return(wait_dma_cmpl(STIMEOUT));
}
/****************************************************************************/
long fdone(void)
{
 return(wait_dma_cmpl(LTIMEOUT));
}
/****************************************************************************/
long wait_dma_cmpl(unsigned long t_ticks)
{
 unsigned long to_count;

 to_count = t_ticks + HZ_200;   /* calc value timer must get to */

 do
 {
    if ( (MFP->GPIP & IO_DINT) == 0) /* Poll DMA IRQ interrupt */
         return(OK);                 /* got interrupt, then OK */

 }  while (HZ_200 <= to_count);      /* check timer */

 return(ERROR);                      /* no interrupt, and timer expired, */
}
/****************************************************************************/


