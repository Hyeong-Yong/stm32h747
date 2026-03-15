/*
 * dac8562.c
 *
 *  Created on: 2023. 12. 9.
 *      Author: User
 */

 #include "spi.h"
 #include "dac8562.h"
 #include "gpio.h"
 #include "cli.h"
 #include "tim.h"
 
 #ifdef _USE_HW_DAC8562
 
#define _USE_DAC8562_DMA	1

 static const uint8_t spi_ch = _DEF_SPI1;
 float voltage_A;
 float voltage_B;
 
 __attribute__((section(".non_cache"))) uint8_t buf[3];

 const float ResistorRatio = 4.166667; // OPA2277 Circuit : 100 [k-ohm] / 24 [k-ohm] = 4.16667
 const float Vref = 2.5; // internal reference voltage : 2.5 [V]
 uint16_t DAC_in = 32768;
 
static void dac8562_writeReg(uint8_t cmd_byte, uint16_t data_byte );
static void dac8562_writeData(uint8_t cmd_byte, uint16_t input);
static uint16_t dac8562_voltageConvert(int32_t voltage);

 #ifdef _USE_HW_CLI
 static void cliDAC8562(cli_args_t *args);
 #endif
 
 #define _GPIO_DAC8562_SPI_SYNC     0
 


 void dac8562_init(void) {
    gpioPinWrite(_GPIO_DAC8562_SPI_SYNC, _DEF_RESET);
	dac8562_writeReg(CMD_RESET_ALL_REG, DATA_RESET_ALL_REG);      // reset
	dac8562_writeReg(CMD_PWR_UP_A_B, DATA_PWR_UP_A_B);    	      // power up
	dac8562_writeReg(CMD_INTERNAL_REF_EN, DATA_INTERNAL_REF_EN); // enable internal reference
	dac8562_writeReg(CMD_GAIN, DATA_GAIN_A1_B1);            // set multiplier
	dac8562_writeReg(CMD_LDAC_DIS, DATA_LDAC_DIS);          // update the caches

    voltage_A=0;
	voltage_B=0;

 #ifdef _USE_HW_CLI
     cliAdd("dac8562", cliDAC8562);
 #endif
 }
 
static void dac8562_writeReg(uint8_t cmd_byte, uint16_t data_byte) {
     gpioPinWrite(_GPIO_DAC8562_SPI_SYNC, _DEF_SET);
     spiTransfer8(spi_ch, cmd_byte);
     spiTransfer8(spi_ch, (data_byte >> 8) & 0xFF); // Mid_byte
     spiTransfer8(spi_ch, data_byte & 0xFF);  //Last
     gpioPinWrite(_GPIO_DAC8562_SPI_SYNC, _DEF_RESET);
 }

static void dac8562_writeData(uint8_t cmd_byte, uint16_t input) {

	buf[0]= cmd_byte;
	buf[1] = (input >> 8) & 0xFF; // Mid_byte
	buf[2] = input & 0xFF;  //Last_byte
	gpioPinWrite(_GPIO_DAC8562_SPI_SYNC, _DEF_SET);
#ifdef _USE_DAC8562_DMA
	spiDmaTxStart(spi_ch, buf, 3);
#endif
#ifdef _USE_DAC8562_POLLING
	spiTransfer8(spi_ch, buf[0]);
	spiTransfer8(spi_ch, buf[1]);
	spiTransfer8(spi_ch, buf[2]);
	gpioPinWrite(_GPIO_DAC8562_SPI_SYNC, _DEF_RESET);
#endif
 }
 
 
 
 void dac8562_writeVoltage_A(int32_t voltage) {
	 dac8562_writeData(CMD_SETA_UPDATEA, dac8562_voltageConvert(voltage));
     //voltage_A = (float)voltage/1000.0f;
 }
  
 void dac8562_writeVoltage_B(int32_t voltage) {
     dac8562_writeData(CMD_SETB_UPDATEB, dac8562_voltageConvert(voltage));
     //voltage_B = (float)voltage/1000.0f;
 }
  
 
 void dac8562_writeVoltage_AB(int32_t voltage) {
     dac8562_writeVoltage_A(voltage);
     dac8562_writeVoltage_B(voltage);
 }
  
  
 
 uint16_t dac8562_voltageConvert(int32_t voltage) {
 
     if (voltage > 10*1000 || voltage < -10*1000) {
         return 32768;
     }
     float f_voltage = (float)voltage/1000;
     // float 연산시 소수점 에러를 없애기 위해 int 변환(*1000)을 했기 때문에, 여기서 계산기 원래 값 (/1000)
     //Voltage = Resistor * Vref * (2 * Din/65536 -1 )
     DAC_in = 32768 * f_voltage / (ResistorRatio * Vref) + 32768;
 
     return DAC_in;
 }
 
 
 #ifdef _USE_HW_CLI
 
 typedef enum _motionState {
     init = 1, x_startToEnd, y_stepIncrease, x_endToStart,
 } motionState;
 
 void cliDAC8562(cli_args_t *args) {
     bool ret = true;
 
     if (args->argc == 1) {
         if (args->isStr(0, "get_value") == true) {
             cliPrintf(
                     "dac8562, Vout of A channel: %.3f [V], Vout of B channel: %.3f [V]\n",
                     voltage_A, voltage_B);
         } else {
             ret = false;
         }
     }
 
     else if (args->argc == 2) {
         if (args->isStr(0, "scan") == true) {
             int32_t us = (int32_t) args->getData(1);
             bool keepScan = true;
 
 
                 float f_start_x = -5.00f, f_start_y = -5.00f, f_end_x = 5.00f,
                         f_end_y = 5.00f, f_interval_x = 0.1f, f_interval_y =
                                 0.1f;
 
                 int current_position_x, current_position_y, start_x =
                         (int) (f_start_x * 1000), start_y = (int) (f_start_y
                         * 1000), end_x = (int) (f_end_x * 1000), end_y =
                         (int) (f_end_y * 1000), interval_x = (int) (f_interval_x
                         * 1000), interval_y = (int) (f_interval_y * 1000);
 
                 /*
                  *  <packet에서 가져오기>
                  *  start_x    = uartRead(
                  *  start_y    = uartRead(
                  *  end_x      = uartRead(
                  *  end_y      = uartRead(
                  *  interval_x = uartRead(
                  *  interval_y = uartRead(
                  *
                  */
 
                 motionState state = init;
 
                 while (keepScan) // 끝점에 도달하면 종료
                 {
                     // 상태머신 구현 하자
                     switch (state) {
                     case init: //"Init"
                         // go to x=-5 , starting point x
                         // go to y=-5 , starting point y
 
                         current_position_x = start_x;
                         current_position_y = start_y;
                         dac8562_writeVoltage_A(current_position_x);
                         dac8562_writeVoltage_B(current_position_y);
                         delay_us(us);
                         //onePulseTrigger(_DEF_PWM1);


                         // timer_trigger() -> Alazartech
 
                         // Go to next "x_start => x_end" step
                         state = x_startToEnd;
 
                         break;
 
                     case x_startToEnd: // "x_start => x_end"
                         current_position_x += interval_x;
 
                         dac8562_writeVoltage_A(current_position_x);
                         delay_us(us);
                         //onePulseTrigger(_DEF_PWM1);
 
                         // timer_trigger() -> Alazartech
 
                         if (end_x == current_position_x) {
                             state = y_stepIncrease;
                         } else {
                             //Console.WriteLine("Error, case 2: x_start => x_end");
                         }
 
                         break;
 
                     case x_endToStart: // "x_end => x_start"
                         current_position_x -= interval_x;
                         dac8562_writeVoltage_A(current_position_x);
                         delay_us(us);
                         //onePulseTrigger(_DEF_PWM1);
 
                         // timer_trigger() -> Alazartech
 
                         if (start_x == current_position_x) {
                             state = y_stepIncrease;
                         } else {
                             //Console.WriteLine("Error, case 3: x_end => x_start");
                         }
 
                         break;
 
                     case y_stepIncrease: // "y => y + y_interval"
 
                         if (end_x == current_position_x
                                 && end_y == current_position_y) {
                             keepScan = false;
                             break;
                         }
 
                         current_position_y += interval_y;
                         dac8562_writeVoltage_B(current_position_y);
                         delay_us(us);
                         //onePulseTrigger(_DEF_PWM1);
 
                         // timer_trigger() -> Alazartech
 
                         if (current_position_x == end_x) {
                             state = x_endToStart;
                         } else if (current_position_x == start_x) {
                             state = x_startToEnd;
                         } else {
                             delay_us(us);
                             //Console.WriteLine("Error, case 4: y => y+y_interval");
                         }
                         break;
 
                     default:
                         break;
                     }
                 }
         }
     }
 
     else if (args->argc == 3) {
         if (args->isStr(0, "set_value") == true) {
             char channel = *(char*) args->getStr(1);
             float f_voltage = (float) args->getData(2);
             int32_t voltage = (int) (f_voltage * 1000);
             if (channel == 0x41) { //"A"
                 dac8562_writeVoltage_A(voltage);
             } else if (channel == 0x42) { //"B"
                 dac8562_writeVoltage_B(voltage);
             } else if (channel == 0x43) { //"C" A and B
                 dac8562_writeVoltage_AB(voltage);
             } else {
                 ret = false;
             }
         } else {
             ret = false;
         }
     }
     else {
         ret = false;
     }
 
     if (ret == false) {
         cliPrintf("dac8562 scan delay[ms] \n");
         cliPrintf("dac8562 get_value \n");
         cliPrintf(
                 "dac8562 set_value [channel] [voltage] \n [channel]: A, B, C(AB) \n [voltage]: -10~10V");
     }
 }
 
 #endif
 
 #endif
