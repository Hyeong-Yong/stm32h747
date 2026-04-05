#include "ethernet.h"


#ifdef _USE_HW_ETHERNET
#include "cli.h"
#include "ethernet_udp.h"

#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/mem.h"
#include "lwip/memp.h"
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "lwip/netif.h"

#include "lwip/timeouts.h"
#include "ethernetif.h"


#define DHCP_OFF                   (uint8_t) 0
#define DHCP_START                 (uint8_t) 1
#define DHCP_WAIT_ADDRESS          (uint8_t) 2
#define DHCP_ADDRESS_ASSIGNED      (uint8_t) 3
#define DHCP_TIMEOUT               (uint8_t) 4
#define DHCP_LINK_DOWN             (uint8_t) 5

#define MAX_DHCP_TRIES              4

/*Static IP ADDRESS: IP_ADDR0.IP_ADDR1.IP_ADDR2.IP_ADDR3 */
#define IP_ADDR0                    ((uint8_t)192U)
#define IP_ADDR1                    ((uint8_t)168U)
#define IP_ADDR2                    ((uint8_t)50U)
#define IP_ADDR3                    ((uint8_t)200U)

/*NETMASK*/
#define NETMASK_ADDR0               ((uint8_t)255U)
#define NETMASK_ADDR1               ((uint8_t)255U)
#define NETMASK_ADDR2               ((uint8_t)255U)
#define NETMASK_ADDR3               ((uint8_t)0U)

/*Gateway Address*/
#define GW_ADDR0                    ((uint8_t)192U)
#define GW_ADDR1                    ((uint8_t)168U)
#define GW_ADDR2                    ((uint8_t)50U)
#define GW_ADDR3                    ((uint8_t)1U)


extern void MX_LWIP_Process(void);
extern bool lwipIsLink(void);

extern struct netif gnetif;

static uint8_t dhcp_state = DHCP_OFF;
static uint8_t dhcp_state_req = DHCP_OFF;
static bool    dhcp_get_ip_flag = false;




bool etnetInit(void)
{
  bool ret = true;


  logPrintf("[OK] etnetInit()\n");

  ret &= etnetUdpInit();

  return ret;
}

bool etnetIsInit(void)
{
  return true;
}

bool etnetIsLink(void)
{
  return lwipIsLink();
}

bool etnetUpdateLink(void)
{
  static bool is_link_pre = false;


  if (etnetIsLink() != is_link_pre)
  {
    if (etnetIsLink())
    {
      dhcp_state_req = DHCP_START; 
    }
    else
    {
      dhcp_state_req = DHCP_LINK_DOWN;      
    }
    is_link_pre = etnetIsLink();
  }

  return true;
}

bool etnetUpdateDHCP(void)
{
  static uint32_t pre_time = 0;

  if (millis() - pre_time < DHCP_FINE_TIMER_MSECS)
  {
    return false;
  }  
  pre_time = millis();


  ip_addr_t     ipaddr;
  ip_addr_t     netmask;
  ip_addr_t     gw;
  struct dhcp  *dhcp;
  struct netif *netif = &gnetif;

  if (dhcp_state_req != dhcp_state)
  {
    logPrintf("[  ] etnet dhcp s%d -> s%d\n", dhcp_state, dhcp_state_req);
    dhcp_state = dhcp_state_req;    
  }

  switch (dhcp_state)
  {
    case DHCP_START:
      {
        int err_dhcp;

        printf("[  ] etnet dhcp : Looking for DHCP server ...\n");

        ip_addr_set_zero_ip4(&netif->ip_addr);
        ip_addr_set_zero_ip4(&netif->netmask);
        ip_addr_set_zero_ip4(&netif->gw);

        dhcp_get_ip_flag = false;
        
        dhcp_stop(netif);
        err_dhcp = dhcp_start(netif);
        if (err_dhcp == ERR_OK)
        {
          dhcp_state_req = DHCP_WAIT_ADDRESS;
        }
      }
      break;

    case DHCP_WAIT_ADDRESS:
      {
        if (dhcp_supplied_address(netif))
        {
          dhcp_get_ip_flag = true;
          dhcp_state_req = DHCP_ADDRESS_ASSIGNED;
          printf("[  ] etnet DHCP IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
          printf("[  ] etnet DHCP IP address assigned by a DHCP server: \n");

        }
        else
        {
          dhcp = (struct dhcp *)netif_get_client_data(netif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);

          /* DHCP timeout */
          if (dhcp->tries > MAX_DHCP_TRIES)
          {
            dhcp_state_req = DHCP_TIMEOUT;

            /* Static address used */
            IP_ADDR4(&ipaddr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
            IP_ADDR4(&netmask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
            IP_ADDR4(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
            netif_set_addr(netif, &ipaddr, &netmask, &gw);

            printf("[  ] etnet DHCP IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
            printf("[  ] etnet DHCP Timeout !! \n");
            printf("[  ] etnet DHCP Static IP address: \n");
          }
        }
      }
      break;
    case DHCP_LINK_DOWN:
      {
        dhcp_get_ip_flag = false;
        dhcp_state_req = DHCP_OFF;
        printf("[  ] enet DHCP : dhcp_off \n");
      }
      break;
    default:
      break;
  }


  return true;
}

void etnetUpdate(void)
{
  MX_LWIP_Process();
  etnetUpdateLink();
  etnetUpdateDHCP();
}

bool etnetIsGetIP(void)
{
  return dhcp_get_ip_flag;;
}

bool etnetGetInfo(etnet_info_t *p_info)
{
  struct netif *netif = &gnetif;

  memcpy(p_info->ip, &netif->ip_addr, sizeof(netif->ip_addr));
  memcpy(p_info->gw, &netif->gw, sizeof(netif->gw));

  return true;
}



#endif