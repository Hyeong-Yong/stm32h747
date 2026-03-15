/*
 * ap.cpp
 *
 *  Created on: Apr 19, 2021
 *      Author: baram
 */




#include "ap.h"


void apInit(void)
{
  cliOpen(_DEF_UART1, 115200);
  cliLogo();
  ovtOct_init();

  galvanoInit();
  //dac8562_init();
  timStart();
}

void apMain(void)
{
  uint32_t pre_time;


  pre_time = millis();
  while(1)
  {
    if (millis()-pre_time >= 500)
    {
      pre_time = millis();
      ledToggle(_DEF_LED1);
    }

    ovtOct_GUI_Run();


   cliMain();
   
  }
}
