/*
 * uart.c
 *
 *  Created on: Apr 19, 2021
 *      Author: baram
 */




#include "uart.h"

#ifdef _USE_HW_UART
#include "qbuffer.h"
#if HW_USB_CDC == 1
#include "cdc.h"
#endif

#define UART_RX_BUF_LENGTH      512



typedef struct
{
  bool     is_open;
  uint32_t baud;


  uint8_t  q_rx_buf[UART_RX_BUF_LENGTH];
  qbuffer_t q_rx;

  UART_HandleTypeDef *p_huart;
  DMA_HandleTypeDef  *p_hdma_rx;

} uart_tbl_t;



//외부 참조 데이터
extern UART_HandleTypeDef huart4;
extern DMA_HandleTypeDef hdma_uart4_rx;

// 내부 참조 데이터
static bool is_init = false;
static __attribute__((section(".non_cache"))) uart_tbl_t uart_tbl[1];






bool uartInit(void)
{
    uart_tbl[0].is_open   = false;
    uart_tbl[0].baud      = 115200;
    uart_tbl[0].p_huart   = NULL;

  is_init = true;

  return is_init;
}


bool uartDeInit(void)
{
  is_init = false;
  return true;
}

bool uartIsInit(void)
{
  return is_init;
}

bool uartOpen(uint8_t ch, uint32_t baud)
{
  bool ret = false;


  switch(ch)
  {
    case _DEF_UART1:
      uart_tbl[ch].p_huart   = &huart4;
      uart_tbl[ch].p_hdma_rx = &hdma_uart4_rx;

      uart_tbl[ch].p_huart->Instance    = UART4;
      uart_tbl[ch].p_huart->Init.BaudRate    = baud;
      uart_tbl[ch].p_huart->Init.WordLength  = UART_WORDLENGTH_8B;
      uart_tbl[ch].p_huart->Init.StopBits    = UART_STOPBITS_1;
      uart_tbl[ch].p_huart->Init.Parity      = UART_PARITY_NONE;
      uart_tbl[ch].p_huart->Init.Mode        = UART_MODE_TX_RX;
      uart_tbl[ch].p_huart->Init.HwFlowCtl   = UART_HWCONTROL_NONE;
      uart_tbl[ch].p_huart->Init.OverSampling= UART_OVERSAMPLING_16;
      uart_tbl[ch].p_huart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
      uart_tbl[ch].p_huart->Init.ClockPrescaler = UART_PRESCALER_DIV1;
      uart_tbl[ch].p_huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

      HAL_UART_DeInit(uart_tbl[ch].p_huart);

      qbufferCreate(&uart_tbl[ch].q_rx, &uart_tbl[ch].q_rx_buf[0], UART_RX_BUF_LENGTH);

      __HAL_RCC_DMA1_CLK_ENABLE();

      HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
      HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);

      if (HAL_UART_Init(uart_tbl[ch].p_huart) != HAL_OK)
      {
        ret = false;
      }
      else
      {
        ret = true;
        uart_tbl[ch].is_open = true;

        if(HAL_UART_Receive_DMA(uart_tbl[ch].p_huart, (uint8_t *)&uart_tbl[ch].q_rx_buf[0], UART_RX_BUF_LENGTH) != HAL_OK)
        {
          ret = false;
        }

        uart_tbl[ch].q_rx.in  = uart_tbl[ch].q_rx.len - ((DMA_Stream_TypeDef *)uart_tbl[ch].p_huart->hdmarx->Instance)->NDTR;
        uart_tbl[ch].q_rx.out = uart_tbl[ch].q_rx.in;
      }
      break;

    case _DEF_UART2:
      uart_tbl[ch].is_open = true;
      ret = true;
      break;
  }

  return ret;
}

bool uartIsOpen(uint8_t ch)
{
  if (ch >= UART_MAX_CH)
    return false;

  return uart_tbl[ch].is_open; 
}

bool uartClose(uint8_t ch)
{
  if (ch >= UART_MAX_CH)
    return false;

  uart_tbl[ch].is_open = false;
  
  return true;
}


uint32_t uartAvailable(uint8_t ch)
{
  uint32_t ret = 0;

  switch(ch)
  {
    case _DEF_UART1:
      uart_tbl[ch].q_rx.in = (uart_tbl[ch].q_rx.len - ((DMA_Stream_TypeDef *)uart_tbl[ch].p_hdma_rx->Instance)->NDTR);
      ret = qbufferAvailable(&uart_tbl[ch].q_rx);
      break;
      case _DEF_UART2:
      ret = cdcAvailable();
      break;
  }

  return ret;
}

bool uartFlush(uint8_t ch)
{
  uint32_t pre_time;

  pre_time = millis();
  while(uartAvailable(ch))
  {
    if (millis()-pre_time >= 10)
    {
      break;
    }

    switch (ch)
    {
    case _DEF_UART1:
      uartRead(ch);
      break;
    case _DEF_UART2:
      cdcRead();
      break;
    }
  }

  return true;
}

uint8_t uartRead(uint8_t ch)
{
  uint8_t ret = 0;

  switch(ch)
  {
    case _DEF_UART1:
      qbufferRead(&uart_tbl[ch].q_rx, &ret, 1);
      break;
      case _DEF_UART2:
      ret = cdcRead();
      break;
  }

  return ret;
}

uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t length)
{
  uint32_t ret = 0;

  switch(ch)
  {
    case _DEF_UART1:
      if (HAL_UART_Transmit(uart_tbl[ch].p_huart, p_data, length, 100) == HAL_OK)
      {
        ret = length;
      }
      break;
      case _DEF_UART2:
        ret = cdcWrite(p_data, length);
      break;
  }

  return ret;
}

uint32_t uartPrintf(uint8_t ch, const char *fmt, ...)
{
  char buf[256];
  va_list args;
  int len;
  uint32_t ret;

  va_start(args, fmt);
  len = vsnprintf(buf, 256, fmt, args);

  ret = uartWrite(ch, (uint8_t *)buf, len);

  va_end(args);


  return ret;
}

uint32_t uartGetBaud(uint8_t ch)
{
  uint32_t ret = 0;


  switch(ch)
  {
    case _DEF_UART1:
      ret = uart_tbl[ch].baud;
      break;
      case _DEF_UART2:
      ret = cdcGetBaud();
      break;
  }

  return ret;
}



#endif
