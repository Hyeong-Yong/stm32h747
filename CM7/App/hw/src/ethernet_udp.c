#include "ethernet_udp.h"


#ifdef _USE_HW_ETHERNET_UDP
#include "cli.h"
#include "qbuffer.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"


#define ETNET_UDP_RX_LENGTH   4096



static void cliCmd(cli_args_t *args);
static void etnetUdpReceiveCallback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
static bool etnetUdpSend(const void *p_data, uint32_t length);

static struct udp_pcb *upcb;

static bool      is_open = false;
static ip_addr_t dest_ip_addr;
static uint16_t  dest_port = 5100;
static bool      is_ip_received = false;

static uint8_t   rx_buf[ETNET_UDP_RX_LENGTH];
static qbuffer_t rx_q;





bool etnetUdpInit(void)
{
  bool ret = false;

  logPrintf("[%s] udpInit()\n", ret ? "OK":"E_");

  qbufferCreate(&rx_q, rx_buf, ETNET_UDP_RX_LENGTH);

  ret = etnetUdpOpen(IP_ADDR_ANY, 5100);

  cliAdd("udp", cliCmd);

  return ret;
}

bool etnetUdpOpen(const ip_addr_t *ip_addr, uint16_t port)
{
  bool ret = false;


  is_ip_received = false;

  upcb = udp_new();
    if (upcb == NULL)
    {
      logPrintf("[E_] udp_new()\n");
      return false;
    }

  if (upcb != NULL)
  {
    err_t err;

    err = udp_bind(upcb, IP_ADDR_ANY, port);
    if (err != ERR_OK)
    {
      logPrintf("[E_] udp_bind() : %d\n", (int)err);    
    }
    if (err == ERR_OK)
    {
      udp_recv(upcb, etnetUdpReceiveCallback, NULL);  
      ret = true;
    }
  }

  is_open = ret;

  logPrintf("[%s] etnetUdpOpen()\n", ret ? "OK":"E_");

  return ret;
}

bool etnetUdpIsOpen(void)
{
  return is_open;
}

bool etnetUdpClose(void)
{
  if (upcb != NULL)
  {
    udp_remove(upcb);
  }

  is_open = false;
  return true;
}

uint32_t etnetUdpAvailable(void)
{
  uint32_t ret;

  ret = qbufferAvailable(&rx_q);

  return ret;
}

bool etnetUdpFlush(void)
{
  qbufferFlush(&rx_q);
  return true;
}

bool etnetUdpRead(uint8_t *p_data, uint32_t length)
{
  bool ret;

  ret = qbufferRead(&rx_q, p_data, length);
  return ret;
}

uint32_t etnetUdpWrite(uint8_t *p_data, uint32_t length)
{
  uint32_t ret = 0;

  if (!is_ip_received)
  {
    return 0;
  }

  ret = etnetUdpSend((const void *)p_data, length);

  return ret;
}


bool etnetUdpSend(const void *p_data, uint32_t length)
{
  bool ret = false;
  struct pbuf *p;
  

  /* allocate pbuf from pool*/
  p = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_POOL);

  if (p != NULL)
  {    
    /* copy data to pbuf */
    pbuf_take(p, (char*)p_data, length);
    
    /* send udp data */
    // if (udp_send(upcb, p) == ERR_OK)
    if (udp_sendto(upcb, p, &dest_ip_addr, dest_port) == ERR_OK)
    {
      ret = true;
    }
    
    /* free pbuf */
    pbuf_free(p);
  }

  return ret;
}

void etnetUdpReceiveCallback(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
  is_ip_received = true;
  dest_ip_addr   = *addr;
  dest_port      = port;

  qbufferWrite(&rx_q, (uint8_t *)p->payload, p->len);

  #if 1
  logPrintf("[  ] udp receive\n");
  logPrintf("[  ]   len : %d\n", p->len);
  logPrintf("[  ]   ip  : %s\n", ip4addr_ntoa(addr));
  logPrintf("[  ]   port: %d\n", port);
  #endif

  pbuf_free(p);
}

void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    cliPrintf("is_open : %d\n", is_open);
    cliPrintf("is_ip_received : %d\n", is_ip_received);
    if (is_ip_received)
    {
      logPrintf("ip dest : %s\n", ip4addr_ntoa(&dest_ip_addr));      
    }
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "rx_q"))
  {
    uint16_t cnt = 0;

    while(qbufferAvailable(&rx_q))
    {
      uint8_t rxd;

      qbufferRead(&rx_q, &rxd, 1);
      cliPrintf("rx_q %d:0x%02X\n", cnt++, rxd);
    }
    ret = true;
  }


  if (args->argc == 1 && args->isStr(0, "send"))
  {
    bool udp_ret;
    const char *send_str = "this is test\n";

    udp_ret = etnetUdpSend(send_str, strlen(send_str));
    cliPrintf("etnetUdpSend() : %s\n", udp_ret ? "OK":"E_");

    ret = true;
  }


  if (!ret)
  {
    cliPrintf("udp info\n");
    cliPrintf("udp rx_q\n");
    cliPrintf("udp send\n");
  }
}

#endif