/*
 * OvtOct.c
 *
 *  Created on: 2024. 9. 18.
 *      Author: User
 */

 #include "uart.h"
 
 #ifdef _USE_HW_OVTOCT
 #include "ovtoct_uart.h"
 #include "ovtoct.h"
 
 static bool open(uint8_t ch, uint32_t baud);
 static bool close(uint8_t ch);
 static uint32_t available(uint8_t ch);
 static uint32_t write(uint8_t ch, uint8_t* p_data, uint32_t length);
 static uint8_t read(uint8_t ch);
 static bool flush(uint8_t ch);
 
 
 
 bool ovtOctUartDriver(ovtOct_driver_t* p_driver)
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
                     //_DEF_OvtOct=> UART2
 static const uint8_t ovtOct_ch_tbl[]={_DEF_UART2};
 
 bool open(uint8_t ch, uint32_t baud)
 {
   bool ret = false;
 
   switch (ch)
   {
   case _DEF_OVTOCT1:
     ret=uartOpen(ovtOct_ch_tbl[ch], baud);
     break;
   }
   return ret;
 }
 
 bool close(uint8_t ch)
 {
   bool ret = false;
   switch (ch)
   {
     case _DEF_OVTOCT1:
     ret = uartClose(ovtOct_ch_tbl[ch]);
     break;
   }
   return ret;
 }
 
 uint32_t available(uint8_t ch)
 {
   uint32_t ret=0;
   switch (ch)
   {
     case _DEF_OVTOCT1:
     ret=uartAvailable(ovtOct_ch_tbl[ch]);
     break;
   }
     return ret;
 }
 uint32_t write(uint8_t ch, uint8_t *p_data, uint32_t length)
 {
   uint32_t ret=0;
 
   switch (ch)
   {
   case _DEF_OVTOCT1:
     ret=uartWrite(ovtOct_ch_tbl[ch], p_data, length);
     break;
   }
   return ret;
 }
 
 uint8_t read(uint8_t ch)
 {
   uint8_t ret=0;
   switch(ch)
   {
     case _DEF_OVTOCT1:

     ret=uartRead(ovtOct_ch_tbl[ch]);
     break;
   }
   return ret;
 }
 bool flush(uint8_t ch)
 {
   bool ret = false;
 switch(ch)
 {
   case _DEF_OVTOCT1:
     ret=uartFlush(ovtOct_ch_tbl[ch]);
     break;
 }
 return ret;
 }
 
 #endif