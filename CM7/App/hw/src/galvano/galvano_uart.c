/*
 * galvano.c
 *
 *  Created on: 2024. 9. 18.
 *      Author: User
 */

#include "galvano_uart.h"
#include "uart.h"

#ifdef _USE_HW_GALVANO


static bool open(uint8_t ch, uint32_t baud);
static bool close(uint8_t ch);
static uint32_t available(uint8_t ch);
static uint32_t write(uint8_t ch, uint8_t* p_data, uint32_t length);
static uint8_t read(uint8_t ch);
static bool flush(uint8_t ch);



bool galvanoUartDriver(galvano_driver_t* p_driver)
{
  p_driver->isInit    = true;
  p_driver->open      = open;
  p_driver->close     = close;
  p_driver->write     = write;
  p_driver->read      = read;
  p_driver->available = available;
  p_driver->flush     = flush;
  return true;
}
				    //_DEF_GALVANO=> UART2
static const uint8_t galvano_ch_tbl[]={_DEF_UART2};

bool open(uint8_t ch, uint32_t baud)
{
  bool ret = false;

  switch (ch)
  {
  case _DEF_GALVANO1:
    ret=uartOpen(galvano_ch_tbl[ch], baud);
    break;
  }
  return ret;
}

bool close(uint8_t ch)
{
  bool ret = false;
  switch (ch)
  {
    case _DEF_GALVANO1:
    ret = uartClose(galvano_ch_tbl[ch]);
    break;
  }
  return ret;
}

uint32_t available(uint8_t ch)
{
  uint32_t ret=0;
  switch (ch)
  {
    case _DEF_GALVANO1:

    ret=uartAvailable(galvano_ch_tbl[ch]);
    break;
  }
    return ret;
}
uint32_t write(uint8_t ch, uint8_t *p_data, uint32_t length)
{
  uint32_t ret=0;

  switch (ch)
  {
  case _DEF_GALVANO1:
    ret=uartWrite(galvano_ch_tbl[ch], p_data, length);
    break;
  }
  return ret;
}

uint8_t read(uint8_t ch)
{
  uint8_t ret=0;
  switch(ch)
  {
    case _DEF_GALVANO1:

    ret=uartRead(galvano_ch_tbl[ch]);
    break;
  }
  return ret;
}
bool flush(uint8_t ch)
{
  bool ret = false;
switch(ch)
{
  case _DEF_GALVANO1:
    ret=uartFlush(galvano_ch_tbl[ch]);
    break;
}
return ret;
}

#endif
