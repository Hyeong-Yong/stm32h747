/*
 * ethernet_stm32.c
 *
 *  Created on: 28 сент. 2023 г.
 *      Author: Professional
 */

#include "hal_ethernet.h"
#include "stm32h7xx_hal.h"				// Include hal header for your STM32 MCU
#include "lwip.h"

#define SV_HEAP_SIZE   (48 * 1024)

__attribute__((section(".sv_heap"), aligned(32)))
static uint8_t sv_heap[SV_HEAP_SIZE];

static size_t sv_heap_offset = 0;

void* sv_malloc(size_t size)
{
    size = (size + 31U) & ~31U;

    if ((sv_heap_offset + size) > SV_HEAP_SIZE)
        return NULL;

    void* ptr = &sv_heap[sv_heap_offset];
    sv_heap_offset += size;
    return ptr;
}

void* sv_calloc(size_t n, size_t size)
{
    size_t total = n * size;
    void* ptr = sv_malloc(total);

    if (ptr)
        memset(ptr, 0, total);

    return ptr;
}


struct sEthernetSocket {
	struct netif * netif;
	uint8_t* destAddress;
};


void Ethernet_getInterfaceMACAddress(const char* interfaceId, uint8_t* addr)
{
	struct netif * netif;
	netif = netif_find(interfaceId);
	for (int i = 0; i < 6; ++i) {
		addr[i] = netif->hwaddr[i];
	}

}


EthernetSocket Ethernet_createSocket(const char* interfaceId, uint8_t* destAddress)
{
	EthernetSocket ethernetSocket = (EthernetSocket) sv_calloc(1, sizeof(struct sEthernetSocket));

	ethernetSocket->netif = netif_find(interfaceId);
	ethernetSocket->destAddress = destAddress;
	return ethernetSocket;

}


void Ethernet_destroySocket(EthernetSocket ethSocket)
{

	free(ethSocket);

}

void Ethernet_sendPacket(EthernetSocket ethSocket, uint8_t* buffer, int packetSize)
{
	struct pbuf bufToSend;
	bufToSend.next = NULL;
	bufToSend.payload = (void *)buffer;
	bufToSend.tot_len = packetSize;
	bufToSend.len = packetSize;
	bufToSend.flags = 117;
	ethSocket->netif->linkoutput(ethSocket->netif, &bufToSend);

}
