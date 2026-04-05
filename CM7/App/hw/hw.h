/*
 * hw.h
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */

#ifndef SRC_HW_HW_H_
#define SRC_HW_HW_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"

#include "adc.h"

#include "led.h"
#include "gpio.h"
#include "cli.h"
#include "uart.h"
#include "usb.h"
#include "cdc.h"
#include "ovtoct.h"
//#include "qspi.h"
#include "spi.h"

#include "galvano.h"
//#include "dac8562.h"

#include "flash.h"
#include "sdram.h"
#include "tim.h"

//#include "ethernet.h"
//#include "ethernet_udp.h"

void hwInit(void);

#ifdef __cplusplus
}
#endif

#endif /* SRC_HW_HW_H_ */
