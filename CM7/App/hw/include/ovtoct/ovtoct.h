/*
 * galvano.h
 *
 *  Created on: 2024. 9. 18.
 *      Author: User
 */

 #ifndef SRC_COMMON_HW_INCLUDE_GALVANO_H_
 #define SRC_COMMON_HW_INCLUDE_GALVANO_H_
 
 #include "hw_def.h"
 
 #ifdef _USE_HW_OVTOCT


 
 #define OVTOCT_PKT_BUF         10
 
 
 
 
 enum
 {
   ovtOct_INST_STATUS = 0x01,
   ovtOct_INST_READ = 0x02,
   ovtOct_INST_WRITE = 0x03,
 };
 
 typedef struct
 {
   uint16_t length;
   uint8_t inst;
   int8_t param_1; 
   int8_t param_2; 
   int8_t param_3; 
   int8_t param_4; 
   uint16_t crc;
 } ovtOct_packet_t;
 
 
 
 typedef struct _ovtOct_driver_t
 {
   bool isInit;
   bool isOpen;
 
   bool (*open)(uint8_t ch, uint32_t baud);
   bool (*close)(uint8_t ch);
   uint32_t (*available)(uint8_t ch);
   uint32_t (*write)(uint8_t ch, uint8_t* p_data, uint32_t length);
   uint8_t (*read)(uint8_t ch);
   bool (*flush)(uint8_t ch);
 } ovtOct_driver_t;
 
 typedef struct _ovtOct_t
 {
   ovtOct_driver_t driver;
 
   bool 		 isOpen;
   uint8_t  	 ch;
   uint32_t      baud;
   uint32_t 	 pre_time;
   uint32_t      state;
 
   ovtOct_packet_t packet;
   uint16_t 		   param_length;
   uint16_t 		   param_current_pos;
   uint8_t        packet_buf[OVTOCT_PKT_BUF];
 } ovtOct_t;
 
 
 
 bool ovtOctLoadDriver(ovtOct_t* p_OvtOct, bool (*load_func)(ovtOct_driver_t*));
 
 bool ovtOct_init();
 bool ovtOct_Open(uint8_t ch, uint32_t baud);
 bool ovtOct_IsOpen(ovtOct_t *p_OvtOct);
 
 
 void runScan();
 
 void ovtOct_GUI_Run();
 
 #endif
 
 
 #endif /* SRC_COMMON_HW_INCLUDE_GALVANO_H_ */