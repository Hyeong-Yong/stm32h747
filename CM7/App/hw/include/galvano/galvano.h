/*
 * galvano.h
 *
 *  Created on: 2024. 9. 18.
 *      Author: User
 */

#ifndef EA176120_11DE_42EC_B90F_27E6D04ADBD2
#define EA176120_11DE_42EC_B90F_27E6D04ADBD2

#include "hw_def.h"


#define GALVANO_PKT_BUF_MAX        HW_GALVANO_PKT_BUF_MAX




typedef enum  {
	X_Axis = 0,
	Y_Axis = 1,
} axisType;

enum
{
  GALVANO_INST_STATUS = 0x01,
  GALVANO_INST_READ = 0x02,
  GALVANO_INST_WRITE = 0x03,
};

typedef struct
{
  uint8_t length;
  uint8_t inst;
  uint32_t param_1; //when scan mode, start_x
  uint32_t param_2; //start_y
  uint32_t param_3; //end_x
  uint32_t param_4; //end_y
  uint32_t param_5; //interval_x
  uint32_t param_6; //interval_y
  uint32_t param_7; //us
  uint16_t crc;
} galvano_packet_t;



typedef struct _galvano_driver_t
{
  bool isInit;
  bool isOpen;

  bool (*open)(uint8_t ch, uint32_t baud);
  bool (*close)(uint8_t ch);
  uint32_t (*available)(uint8_t ch);
  uint32_t (*write)(uint8_t ch, uint8_t* p_data, uint32_t length);
  uint8_t (*read)(uint8_t ch);
  bool (*flush)(uint8_t ch);
} galvano_driver_t;

typedef struct _galvano_t
{
  galvano_driver_t driver;

  bool 		 isOpen;
  uint8_t	 ch;
  uint32_t       baud;
  uint32_t 	 pre_time;
  uint32_t       state;

  galvano_packet_t packet;
  uint8_t 		 param_length;
  uint8_t 		 param_current_pos;
  uint8_t        packet_buf[GALVANO_PKT_BUF_MAX];
} galvano_t;


typedef struct _galvano_status_t
{
  uint32_t       current_voltageA;
  uint32_t       current_voltageB;
  int current_position_x;
  int current_position_y;

  int start_x;
  int start_y;
  int end_x;
  int end_y;
  int interval_x;
  int interval_y;
  uint32_t timeDelay; //us 단위

  bool		 	 keepScan;
  bool 			 flagGUI;
}galvano_status_t;

bool galvanoLoadDriver(galvano_t* p_galvano, bool (*load_func)(galvano_driver_t*));

bool galvanoInit(void);
bool galvanoOpen(uint8_t ch, uint32_t baud);
bool galvanoIsOpen(galvano_t *p_galvano);
bool galvanoClose(galvano_t *p_galvano);


void galvanoGUIRun(void);


#endif /* EA176120_11DE_42EC_B90F_27E6D04ADBD2 */
