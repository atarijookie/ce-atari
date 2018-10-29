// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
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
#include "../ce_hdd_if/stdlib.h"
#include "../ce_hdd_if/hdd_if.h"
#include "port.h"

#define  M_YEAR    16
#define  M_MONTH   12
#define  M_DAY     2

extern CONFIG  conf;
extern int32 _pbase;

PORT    my_port   = {  "Internal", L_INTERNAL, TRUE, 0L, LOOPBACK, 0xffffffffUL, 32768, 32768, 0L, NULL, 0L, NULL, 0, NULL, NULL   };
DRIVER  my_driver = {  my_set_state, my_cntrl, NULL, NULL, "Internal", "01.00", (M_YEAR << 9) | (M_MONTH << 5) | M_DAY, "Peter Rottengatter", NULL, NULL   };

void  init_ports(void)
{
   my_driver.basepage   = (BASEPAGE *) _pbase;
   my_port.driver       = &my_driver;

   conf.ports   = &my_port;
   conf.drivers = &my_driver;
}

int16 on_port (char *port_name)
{
    PORT  *this;

    if ((this = search_port (port_name)) == NULL) {
        return FALSE;
    }

    if ( getWordByByteOffset(this, PO_active) ) {
        return TRUE;
    }

    /*
    // TODO: turn this port on

    if ((*this->driver->set_state) (this, TRUE) == FALSE)
        return (FALSE);
    */

   setWordByByteOffset (this, PO_active,        TRUE);
   setDwordByByteOffset(this, PO_stat_sd_data,  0);
   setDwordByByteOffset(this, PO_stat_rcv_data, 0);
   setWordByByteOffset (this, PO_stat_dropped,  0);

   return (TRUE);
}

void off_port (char *port_name)
{
    PORT *this;
    this = search_port(port_name);

    if (this == NULL) {
        return;
    }

    if (! getWordByByteOffset(this, PO_active) ) {
        return;
    }

    /*
    // TODO: turn this port OFF
    (*this->driver->set_state) (this, FALSE);
    */

    setWordByByteOffset (this, PO_active, FALSE);
}

int16 query_port (char *port_name)
{
    PORT  *this;

    if (port_name == NULL) {
        return FALSE;
    }

    if ((this = search_port (port_name)) == NULL) {
        return FALSE;
    }

   return getWordByByteOffset(this, PO_active);
}

int16 cntrl_port(char *port_name, uint32 argument, int16 code)
{
    PORT   *this;
    int16  result = E_NORMAL;
    PORT   *pOpaque, *pNext;
    char   *pPName, *pOName;

   if (port_name == NULL) {
        switch (code) {
            case CTL_KERN_FIRST_PORT :
                setDwordByByteOffset((void *) argument, PO_opaque, (DWORD) conf.ports);

                pPName      = getVoidpByByteOffset((void *) argument, PO_port_name);    // get pointer to name in PTNA

                pOpaque     = getVoidpByByteOffset((void *) argument, PO_opaque);       // first get pointer to port
                pOName      = getVoidpByByteOffset(pOpaque,  PO_name);                  // then get pointer to name from PORT

                strncpy(pPName, pOName, getWordByByteOffset((void *) argument, PO_name_len));
                return E_NORMAL;

            case CTL_KERN_NEXT_PORT :
                pOpaque     = getVoidpByByteOffset((void *) argument, PO_opaque);       // first get pointer to port
                pNext       = getVoidpByByteOffset(pOpaque,  PO_next);                  // then get pointer to next port

                if (pNext == NULL) {                                                    // no next port? fail
                    return E_NODATA;
                }

                pPName      = getVoidpByByteOffset((void *) argument, PO_port_name);    // get pointer to name in PNTA
                pOName      = getVoidpByByteOffset(pOpaque, PO_name);                   // then get pointer to name from PORT

                strncpy (pPName, pOName, getWordByByteOffset((void *) argument, PO_name_len));
                return E_NORMAL;
           }

        return E_FNAVAIL;
    }

    this = search_port(port_name);

    if (this == NULL) {
        return E_NODATA;
    }

    switch (code) {
      case CTL_KERN_FIND_PORT :
        *((PORT **)  argument) = this;                                      break;

      case CTL_GENERIC_GET_IP :
        *((uint32 *) argument) = getDwordByByteOffset(this, PO_ip_addr);    break;

      case CTL_GENERIC_GET_MASK :
        *((uint32 *) argument) = getDwordByByteOffset(this, PO_sub_mask);   break;

      case CTL_GENERIC_GET_MTU :
        *(( int16 *) argument) = getWordByByteOffset(this, PO_mtu);         break;

      case CTL_GENERIC_GET_MMTU :
        *(( int16 *) argument) = getWordByByteOffset(this, PO_max_mtu);     break;

      case CTL_GENERIC_GET_TYPE :
        *(( int16 *) argument) = getWordByByteOffset(this, PO_type);        break;

      case CTL_GENERIC_GET_STAT :
        ((int32 *) argument)[0] = getWordByByteOffset (this, PO_stat_dropped);
        ((int32 *) argument)[1] = getDwordByByteOffset(this, PO_stat_sd_data);
        ((int32 *) argument)[2] = getDwordByByteOffset(this, PO_stat_rcv_data);
        break;

      case CTL_GENERIC_CLR_STAT :
            setDwordByByteOffset(this, PO_stat_sd_data,  0);
            setDwordByByteOffset(this, PO_stat_rcv_data, 0);
            setWordByByteOffset (this, PO_stat_dropped,  0);
        break;

      default :
      /*
        result = (*this->driver->cntrl) (this, argument, code);
        */
        break;
    }

   if (result == E_FNAVAIL) {
        switch (code) {
           case CTL_GENERIC_SET_MTU :
             if (argument > getWordByByteOffset(this, PO_max_mtu)) {
                argument = getWordByByteOffset(this, PO_max_mtu);
             }

             if (argument < 68) {
                argument = 68;
             }
             setWordByByteOffset(this, PO_mtu, (int16) argument);
             result = E_NORMAL;
             break;

            case CTL_GENERIC_SET_IP :
                setDwordByByteOffset(this, PO_ip_addr, argument);
                result = E_NORMAL;
                break;

            case CTL_GENERIC_SET_MASK :
                setDwordByByteOffset(this, PO_sub_mask, argument);
                result = E_NORMAL;
                break;
           }
      }

   return (result);
}

PORT *search_port(char *port_name)
{
    PORT *walk;

    for (walk = conf.ports; walk; walk = (PORT *) getDwordByByteOffset(walk, PO_next)) {
        char *pToName = ((char *) walk) + PO_name;

        if (strcmp (pToName, port_name) == 0) {
            return walk;
        }
    }

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
