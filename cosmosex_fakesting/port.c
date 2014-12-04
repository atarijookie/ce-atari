//----------------------------------------
// CosmosEx fake STiNG - by Jookie, 2014
// Based on sources of original STiNG
//----------------------------------------

#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>
#include <stdint.h>
#include <stdio.h>

#include "globdefs.h"
#include "stdlib.h"
#include "port.h"

#define  M_YEAR    16
#define  M_MONTH   12
#define  M_DAY     2

extern CONFIG  conf;
extern int32 _pbase;

PORT    my_port   = {  "Internal", L_INTERNAL, TRUE, 0L, LOOPBACK, 0xffffffffUL, 32768, 32768, 0L, NULL, 0L, NULL, 0, NULL, NULL   };
DRIVER  my_driver = {  my_set_state, my_cntrl, NULL, NULL, "Internal", "01.00", (M_YEAR << 9) | (M_MONTH << 5) | M_DAY, "Peter Rottengatter", NULL, NULL   };

#pragma message "!!! port.c needs a lot of fixing - PORT * structure is accessed as a structure, this will fail because of gcc alignment !!!"

void  init_ports(void)
{
   my_driver.basepage   = (BASEPAGE *) _pbase;
   my_port.driver       = &my_driver;

   conf.ports = &my_port;   conf.drivers = &my_driver;
}

int16 on_port (char *port_name)
{
    //------------------------------
    // retrieve real params from stack
    getStackPointer();
    port_name = getVoidPFromSP();
    //------------------------------
    
   PORT  *this;

   if ((this = search_port (port_name)) == NULL)
        return (FALSE);

   if (this->active)   return (TRUE);

   if ((*this->driver->set_state) (this, TRUE) == FALSE)
        return (FALSE);

   this->active = TRUE;
   this->stat_sd_data = this->stat_rcv_data = this->stat_dropped = 0;

   return (TRUE);
}

void off_port (char *port_name)
{
    //------------------------------
    // retrieve real params from stack
    getStackPointer();
    port_name = getVoidPFromSP();
    //------------------------------

   PORT  *this;

   if ((this = search_port (port_name)) == NULL)
        return;

   if (! this->active)   return;

   (*this->driver->set_state) (this, FALSE);

   this->active = FALSE;
}

int16 query_port (char *port_name)
{
    //------------------------------
    // retrieve real params from stack
    getStackPointer();
    port_name = getVoidPFromSP();
    //------------------------------

   PORT  *this;

   if (port_name == NULL)   return (FALSE);

   if ((this = search_port (port_name)) == NULL)
        return (FALSE);

   return (this->active);
}

int16 cntrl_port(char *port_name, uint32 argument, int16 code)
{
    //------------------------------
    // retrieve real params from stack
    getStackPointer();
    port_name   = getVoidPFromSP();
    argument    = getDwordFromSP();
    code        = getWordFromSP();
    //------------------------------

   PORT   *this;
   PNTA   *act_pnta;
   int16  result = E_NORMAL;

   if (port_name == NULL) {
        switch (code) {
           case CTL_KERN_FIRST_PORT :
             (act_pnta = (PNTA *) argument)->opaque = conf.ports;
             strncpy (act_pnta->port_name, act_pnta->opaque->name, act_pnta->name_len);
             return (E_NORMAL);
           case CTL_KERN_NEXT_PORT :
             act_pnta = (PNTA *) argument;
             if ((act_pnta->opaque = act_pnta->opaque->next) == NULL)
                  return (E_NODATA);
             strncpy (act_pnta->port_name, act_pnta->opaque->name, act_pnta->name_len);
             return (E_NORMAL);
           }
        return (E_FNAVAIL);
      }

   if ((this = search_port (port_name)) == NULL)
        return (E_NODATA);

   switch (code) {
      case CTL_KERN_FIND_PORT :
        *((PORT **)  argument) = this;             break;
      case CTL_GENERIC_GET_IP :
        *((uint32 *) argument) = this->ip_addr;    break;
      case CTL_GENERIC_GET_MASK :
        *((uint32 *) argument) = this->sub_mask;   break;
      case CTL_GENERIC_GET_MTU :
        *(( int16 *) argument) = this->mtu;        break;
      case CTL_GENERIC_GET_MMTU :
        *(( int16 *) argument) = this->max_mtu;    break;
      case CTL_GENERIC_GET_TYPE :
        *(( int16 *) argument) = this->type;       break;
      case CTL_GENERIC_GET_STAT :
        ((int32 *) argument)[0] = this->stat_dropped;
        ((int32 *) argument)[1] = this->stat_sd_data;
        ((int32 *) argument)[2] = this->stat_rcv_data;
        break;
      case CTL_GENERIC_CLR_STAT :
        this->stat_sd_data = this->stat_rcv_data = this->stat_dropped = 0;
        break;
      default :
        result = (*this->driver->cntrl) (this, argument, code);
      }

   if (result == E_FNAVAIL) {
        switch (code) {
           case CTL_GENERIC_SET_MTU :
             if (argument > this->max_mtu)   argument = this->max_mtu;
             if (argument < 68)              argument = 68;
             this->mtu = (int16) argument;  result = E_NORMAL;   break;
           case CTL_GENERIC_SET_IP :
             this->ip_addr  = argument;     result = E_NORMAL;   break;
           case CTL_GENERIC_SET_MASK :
             this->sub_mask = argument;     result = E_NORMAL;   break;
           }
      }

   return (result);
}

PORT *search_port(char *port_name)
{
    //------------------------------
    // retrieve real params from stack
    getStackPointer();
    port_name = getVoidPFromSP();
    //------------------------------
    
   PORT  *walk;

   for (walk = conf.ports; walk; walk = walk->next)
        if (strcmp (walk->name, port_name) == 0)
             return (walk);

   return NULL;
}

int16 my_set_state (PORT *port, int16 state)
{
    return TRUE;
}

int16 my_cntrl (PORT *port, uint32 argument, int16 code)
{
    return E_FNAVAIL;
}
