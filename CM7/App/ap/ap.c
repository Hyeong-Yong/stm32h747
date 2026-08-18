

#include "ap.h"
#include "cmsis_os.h"

void apInit(void)
{
  cliOpen(_DEF_UART1, 115200);
  cliLogo();


  //USB-MODE
  //ovtOct_init();

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
    
    //USB-MODE
   //ovtOct_GUI_Run();
    
    

   cliMain();
   #ifdef _USE_FREERTOS
   osDelay(1);
   #endif
  }
}
