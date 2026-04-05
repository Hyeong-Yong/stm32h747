/*
 * ethernet_stm32.c
 *
 *  Created on: 28 сент. 2023 г.
 *      Author: Professional
 */

#include "hal_ethernet.h"
#include "stm32h7xx_hal.h"				// Include hal header for your STM32 MCU
#include "lwip.h"

 // DMA malloc for lwip, not creating memory in the DTCM
__attribute__((section(".lwip_sec"))) static uint8_t dma_heap[4 * 1024];

static size_t dma_heap_offset = 0;

void* dma_malloc(size_t size)
{
    void* ptr = &dma_heap[dma_heap_offset];
    dma_heap_offset += size;
    return ptr;
}

void* dma_calloc(size_t n, size_t size)
{
    void* ptr = dma_malloc(n * size);
    memset(ptr, 0, n * size);
    return ptr;
}



struct sEthernetSocket {
	struct netif * netif;
	uint8_t* destAddress;
};


void Ethernet_getInterfaceMACAddress(const char* interfaceId, uint8_t* addr)
{

	struct netif * netif = netif_find(interfaceId);
	for (int i = 0; i < 6; ++i) {
		addr[i] = netif->hwaddr[i];
	}

}


EthernetSocket Ethernet_createSocket(const char* interfaceId, uint8_t* destAddress)
{

	EthernetSocket ethernetSocket = (EthernetSocket) calloc(1, sizeof(struct sEthernetSocket));
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
