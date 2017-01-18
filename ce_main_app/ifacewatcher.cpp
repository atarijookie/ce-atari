// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <arpa/inet.h>

#include "ifacewatcher.h"
#include "debug.h"

#define CLASSFUNC "IfaceWatcher::",__FUNCTION__

IfaceWatcher::IfaceWatcher()
{
    fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if(fd < 0) {
        Debug::out(LOG_ERROR, "%s%s: socket : %s", CLASSFUNC, strerror(errno));
    } else {
        struct sockaddr_nl addr;
        memset(&addr, 0, sizeof(addr));
        addr.nl_family = AF_NETLINK;
        addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;
        if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            Debug::out(LOG_ERROR, "%s%s: bind: %s", CLASSFUNC, strerror(errno));
            close(fd);
            fd = -1;
        }
    }
}

IfaceWatcher::~IfaceWatcher()
{
    if(fd >= 0) {
        close(fd);
        fd = -1;
    }
}

void IfaceWatcher::processMsg(bool * newIfaceUpAndRunning)
{
    char buffer[4096];
    ssize_t len;
    struct iovec iov;
    struct msghdr hdr;
    struct nlmsghdr *nlhdr;

    iov.iov_base = buffer;
    iov.iov_len = sizeof(buffer);

    memset(&hdr, 0, sizeof(hdr));
    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;

    len = recvmsg(fd, &hdr, 0);
    if(len < 0) {
        Debug::out(LOG_ERROR, "%s%s: recvmsg: %s", CLASSFUNC, strerror(errno));
        return;
    }

    for(nlhdr = (struct nlmsghdr *)buffer;  NLMSG_OK (nlhdr, (unsigned int)len); nlhdr = NLMSG_NEXT (nlhdr, len)) {
        bool is_del = false;
        switch(nlhdr->nlmsg_type) {
        case NLMSG_DONE:
            return;
        case RTM_DELLINK:
            is_del = true;
        case RTM_NEWLINK:
            {
                struct rtattr *rta;
                int rta_len;
                struct ifinfomsg * ifi = (struct ifinfomsg *)NLMSG_DATA(nlhdr);
                Debug::out(LOG_DEBUG, "%s%s %s type=%hu index=%d flags=0x%08x",
                          CLASSFUNC, is_del ? "RTM_DELLINK" : "RTM_NEWLINK",
                          ifi->ifi_type, ifi->ifi_index, ifi->ifi_flags);
                /* flags : /usr/include/linux/if.h
              IFF_UP            Interface is running.
              IFF_BROADCAST     Valid broadcast address set.
              IFF_DEBUG         Internal debugging flag.
              IFF_LOOPBACK      Interface is a loopback interface.

              IFF_POINTOPOINT   Interface is a point-to-point link.
              IFF_RUNNING       Resources allocated.
              IFF_NOARP         No arp protocol, L2 destination address not
                                set.
              IFF_PROMISC       Interface is in promiscuous mode.
              IFF_NOTRAILERS    Avoid use of trailers.
              IFF_ALLMULTI      Receive all multicast packets.
              IFF_MASTER        Master of a load balancing bundle.
              IFF_SLAVE         Slave of a load balancing bundle.
              IFF_MULTICAST     Supports multicast
              IFF_PORTSEL       Is able to select media type via ifmap.
              IFF_AUTOMEDIA     Auto media selection active.
              IFF_DYNAMIC       The addresses are lost when the interface
                                goes down.
              IFF_LOWER_UP      Driver signals L1 up (since Linux 2.6.17)
              IFF_DORMANT       Driver signals dormant (since Linux 2.6.17)
              IFF_ECHO          Echo sent packets (since Linux 2.6.25)
                */
                if(is_del) {
                    if_names.erase(ifi->ifi_index);
                    if_flags.erase(ifi->ifi_index);
                } else {
                    if_flags[ifi->ifi_index] = ifi->ifi_flags;
                }
                for(rta = IFLA_RTA(ifi), rta_len = IFLA_PAYLOAD(nlhdr); rta_len >= 0 && RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len)) {
                    switch(rta->rta_type) {
#if 0
                    case IFLA_ADDRESS:   //  hardware address   interface L2 address
                    case IFLA_BROADCAST: //  hardware address   L2 broadcast address.
#endif
                    case IFLA_IFNAME:    //  asciiz string      Device name.
                        Debug::out(LOG_DEBUG, "%s%s IFLA_IFNAME=%s", CLASSFUNC, RTA_DATA(rta));
                        if(!is_del) if_names[ifi->ifi_index] = std::string((const char *)RTA_DATA(rta));
                        break;
#if 0
                    case IFLA_MTU:       //  unsigned int       MTU of the device.
                    case IFLA_LINK:      //  int                Link type.
                    case IFLA_QDISC:     //  asciiz string      Queueing discipline.
                    case IFLA_STATS:     //  see below          Interface Statistics.
#endif
                    }
                }
            }
            break;
        case RTM_DELADDR:
            is_del = true;
        case RTM_NEWADDR:
            {
                bool hasIPv4Address = false;
                struct rtattr *rta;
                int rta_len;
                struct ifaddrmsg * ifa = (struct ifaddrmsg *)NLMSG_DATA(nlhdr);
                char addrtmp[48];
                memset(addrtmp, 0, sizeof(addrtmp));

                Debug::out(LOG_DEBUG, "%s%s %s family=%hhu prefixlen=%hhu flags=0x%02x scope=%hhu index=%d",
                           CLASSFUNC, is_del ? "RTM_DELADDR" : "RTM_NEWADDR", ifa->ifa_family,
                           ifa->ifa_prefixlen, ifa->ifa_flags, ifa->ifa_scope, ifa->ifa_index);
                for(rta = IFA_RTA(ifa), rta_len = IFA_PAYLOAD(nlhdr); rta_len >= 0 && RTA_OK(rta, rta_len); rta = RTA_NEXT(rta, rta_len)) {
                    switch(rta->rta_type) {
                    case IFA_ADDRESS:   //  raw protocol address   interface address
                        if(ifa->ifa_family == AF_INET)
                            hasIPv4Address = true;
                    case IFA_LOCAL:     //  raw protocol address   local address
                    case IFA_BROADCAST: //  raw protocol address   broadcast address.
                    case IFA_ANYCAST:   //  raw protocol address   anycast address
                        if(inet_ntop(ifa->ifa_family, RTA_DATA(rta), addrtmp, sizeof(addrtmp)) != NULL) {
                            Debug::out(LOG_DEBUG, "%s%s type=%d %s", CLASSFUNC, rta->rta_type, addrtmp);
                        }
                        break;
                    case IFA_LABEL:     //  asciiz string          name of the interface
                        Debug::out(LOG_DEBUG, "%s%s IFA_LABEL=%s", CLASSFUNC, RTA_DATA(rta));
                        break;
#if 0
                    case IFA_CACHEINFO: //  struct ifa_cacheinfo   Address information.
#endif
                    }
                }
                if(!is_del && hasIPv4Address && newIfaceUpAndRunning) {
                    Debug::out(LOG_DEBUG, "%s%s flags for %s are 0x%08x", CLASSFUNC,
                               if_names[ifa->ifa_index].c_str(), if_flags[ifa->ifa_index]);
                    //if(if_flags[ifa->ifa_index] & IFF_UP) // let's suppose for now it is up if it has an address
                        *newIfaceUpAndRunning = true;
                }
            }
            break;
        default:
            Debug::out(LOG_ERROR, "%s%s unknown MSG type %d", CLASSFUNC, nlhdr->nlmsg_type);
        }
    }
}
