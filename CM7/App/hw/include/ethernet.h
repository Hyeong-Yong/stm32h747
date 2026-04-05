#ifndef ETNET_H_
#define ETNET_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "hw_def.h"

#ifdef _USE_HW_ETHERNET

    typedef struct
    {
        uint8_t mac[6]; ///< Source Mac Address
        uint8_t ip[4];  ///< Source IP Address
        uint8_t sn[4];  ///< Subnet Mask
        uint8_t gw[4];  ///< Gateway IP Address
        uint8_t dns[4]; ///< DNS server IP Address
        bool dhcp;
    } etnet_info_t;

    bool etnetInit(void);
    bool etnetIsInit(void);
    bool etnetIsLink(void);
    void etnetUpdate(void);
    bool etnetIsGetIP(void);
    bool etnetGetInfo(etnet_info_t *p_info);

#endif

#ifdef __cplusplus
}
#endif

#endif