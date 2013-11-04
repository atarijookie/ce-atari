/*--------------------------------------------------*/
#include <tos.h>
#include "acsi.h"

#define MFP_ADDR 	0xFFFA00L      /* MFP device addres */
#define MFP     	((struct mfp_chip *) MFP_ADDR)

#define DMA_ADDR 0xFF8600L      /* DMA device addres */
#define DMA      ((struct dma_chip *) DMA_ADDR)

/* -------------------------------------- */
BYTE acsi_cmd(BYTE ReadNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer)
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

  for(i=1; i<cmdLength; i++)
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


