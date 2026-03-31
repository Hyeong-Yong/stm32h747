#include "spi.h"


#ifdef _USE_HW_SPI
#include "stm32h7xx_ll_spi.h"
#include "gpio.h"

#define SPI_TX_DMA_MAX_LENGTH   0xFFFF




typedef struct
{
  bool is_open;
  bool is_tx_done;
  bool is_rx_done;
  bool is_error;

  void (*func_tx)(void);

  SPI_HandleTypeDef *h_spi;
} spi_t;

extern SPI_HandleTypeDef hspi2;

spi_t spi_tbl[SPI_MAX_CH];


bool spiInit(void)
{
  bool ret = true;


  for (int i=0; i<SPI_MAX_CH; i++)
  {
    spi_tbl[i].is_open    = false;
    spi_tbl[i].is_tx_done = true;
    spi_tbl[i].is_rx_done = true;
    spi_tbl[i].is_error   = false;
    spi_tbl[i].func_tx    = NULL;
  }

  return ret;
}

bool spiBegin(uint8_t ch)
{
  bool ret = false;
  spi_t *p_spi = &spi_tbl[ch];

  switch(ch)
  {
    case _DEF_SPI1:
      p_spi->h_spi = &hspi2;


      //HAL_SPI_DeInit(p_spi->h_spi);
      //if (HAL_SPI_Init(p_spi->h_spi) == HAL_OK)
      //{
      //  p_spi->is_open = true;
        ret = true;
      //}
      
      break;
  }

  return ret;
}

bool spiIsBegin(uint8_t ch)
{
  return spi_tbl[ch].is_open;
}

void spiSetDataMode(uint8_t ch, uint8_t dataMode)
{
  spi_t  *p_spi = &spi_tbl[ch];

  //if (p_spi->is_open == false) return;
  switch( dataMode )
  {
    // CPOL=0, CPHA=0
    case SPI_MODE0:
      p_spi->h_spi->Init.CLKPolarity = SPI_POLARITY_LOW;
      p_spi->h_spi->Init.CLKPhase    = SPI_PHASE_1EDGE;
      HAL_SPI_Init(p_spi->h_spi);
      break;

    // CPOL=0, CPHA=1
    case SPI_MODE1:
      p_spi->h_spi->Init.CLKPolarity = SPI_POLARITY_LOW;
      p_spi->h_spi->Init.CLKPhase    = SPI_PHASE_2EDGE;
      HAL_SPI_Init(p_spi->h_spi);
      break;

    // CPOL=1, CPHA=0
    case SPI_MODE2:
      p_spi->h_spi->Init.CLKPolarity = SPI_POLARITY_HIGH;
      p_spi->h_spi->Init.CLKPhase    = SPI_PHASE_1EDGE;
      HAL_SPI_Init(p_spi->h_spi);
      break;

    // CPOL=1, CPHA=1
    case SPI_MODE3:
      p_spi->h_spi->Init.CLKPolarity = SPI_POLARITY_HIGH;
      p_spi->h_spi->Init.CLKPhase    = SPI_PHASE_2EDGE;
      HAL_SPI_Init(p_spi->h_spi);
      break;
  }
}

void spiSetBitWidth(uint8_t ch, uint8_t bit_width)
{
  spi_t  *p_spi = &spi_tbl[ch];

  //if (p_spi->is_open == false) return;

  

  switch(bit_width)
  {
    case 16:
      p_spi->h_spi->Init.DataSize = SPI_DATASIZE_16BIT;
      LL_SPI_SetDataWidth(p_spi->h_spi->Instance, LL_SPI_DATAWIDTH_16BIT);
      break;

    default:
      p_spi->h_spi->Init.DataSize = SPI_DATASIZE_8BIT;
      LL_SPI_SetDataWidth(p_spi->h_spi->Instance, LL_SPI_DATAWIDTH_8BIT);
      break;
  }
}

uint8_t spiTransfer8(uint8_t ch, uint8_t data)
{
  uint8_t ret = 0;
  spi_t  *p_spi = &spi_tbl[ch];

  //if (p_spi->is_open == false) return 0;

  HAL_SPI_TransmitReceive(p_spi->h_spi, &data, &ret, 1, 10);
  //HAL_SPI_Transmit(p_spi->h_spi, &data, 1, 10);
  //HAL_SPI_Transmit_DMA(p_spi->h_spi, &data, 1);
  return ret;
}

uint16_t spiTransfer16(uint8_t ch, uint16_t data)
{
  uint8_t tBuf[2];
  uint8_t rBuf[2];
  uint16_t ret;
  spi_t  *p_spi = &spi_tbl[ch];


  //if (p_spi->is_open == false) return 0;

  if (p_spi->h_spi->Init.DataSize == SPI_DATASIZE_8BIT)
  {
    tBuf[1] = (uint8_t)data;
    tBuf[0] = (uint8_t)(data>>8);
    //HAL_SPI_Transmit(p_spi->h_spi, (uint8_t *)&tBuf, 2, 10);
    HAL_SPI_TransmitReceive(p_spi->h_spi, (uint8_t *)&tBuf, (uint8_t *)&rBuf, 2, 10);
     ret = rBuf[0];
     ret <<= 8;
     ret += rBuf[1];

    //HAL_SPI_Transmit_DMA(p_spi->h_spi, (uint8_t *)&tBuf, 2);

  }
  else
  {
    HAL_SPI_TransmitReceive(p_spi->h_spi, (uint8_t *)&data, (uint8_t *)&ret, 1, 10);
    //HAL_SPI_Transmit_DMA(p_spi->h_spi, (uint8_t *)&data, 1);
  }

  return ret;
}

bool spiTransfer(uint8_t ch, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t length, uint32_t timeout)
{
  bool ret = true;
  HAL_StatusTypeDef status;
  spi_t  *p_spi = &spi_tbl[ch];

  //if (p_spi->is_open == false) return false;

  if (rx_buf == NULL)
  {
    status =  HAL_SPI_Transmit(p_spi->h_spi, tx_buf, length, timeout);
  }
  else if (tx_buf == NULL)
  {
    status =  HAL_SPI_Receive(p_spi->h_spi, rx_buf, length, timeout);
  }
  else
  {
    status =  HAL_SPI_TransmitReceive(p_spi->h_spi, tx_buf, rx_buf, length, timeout);
  }

  if (status != HAL_OK)
  {
    return false;
  }

  return ret;
}

bool spiTransferDMA(uint8_t ch, uint8_t *tx_buf, uint8_t *rx_buf, uint32_t length, uint32_t timeout)
{
  bool ret = false;
  HAL_StatusTypeDef status;
  spi_t  *p_spi = &spi_tbl[ch];
  bool is_dma = false;

  //if (p_spi->is_open == false) return false;

  if (rx_buf == NULL)
  {
    status = HAL_SPI_Transmit(p_spi->h_spi, tx_buf, length, timeout);
  }
  else if (tx_buf == NULL)
  {
    p_spi->is_rx_done = false;
    status = HAL_SPI_Receive_DMA(p_spi->h_spi, rx_buf, length);
    is_dma = true;
  }
  else
  {
    status = HAL_SPI_TransmitReceive(p_spi->h_spi, tx_buf, rx_buf, length, timeout);
  }

  if (status == HAL_OK)
  {
    uint32_t pre_time;

    ret = true;
    pre_time = millis();
    if (is_dma == true)
    {
      while(1)
      {
        if(p_spi->is_rx_done == true)
          break;

        if((millis()-pre_time) >= timeout)
        {
          ret = false;
          break;
        }
      }
    }
  }

  return ret;
}

void spiDmaTxStart(uint8_t spi_ch, uint8_t *p_buf, uint32_t length)
{
  spi_t  *p_spi = &spi_tbl[spi_ch];

  //if (p_spi->is_open == false) return;

  p_spi->is_tx_done = false;
  HAL_SPI_Transmit_DMA(p_spi->h_spi, p_buf, length);
}

bool spiDmaTxTransfer(uint8_t ch, void *buf, uint32_t length, uint32_t timeout)
{
  bool ret = true;
  uint32_t t_time;


  spiDmaTxStart(ch, (uint8_t *)buf, length);

  t_time = millis();

  if (timeout == 0) return true;

  while(1)
  {
    if(spiDmaTxIsDone(ch))
    {
      break;
    }
    if((millis()-t_time) > timeout)
    {
      ret = false;
      break;
    }
  }

  return ret;
}

bool spiDmaTxIsDone(uint8_t ch)
{
  spi_t  *p_spi = &spi_tbl[ch];

  //if (p_spi->is_open == false)     return true;

  return p_spi->is_tx_done;
}

void spiAttachTxInterrupt(uint8_t ch, void (*func)())
{
  spi_t  *p_spi = &spi_tbl[ch];


  //if (p_spi->is_open == false)     return;

  p_spi->func_tx = func;
}


void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
#define GPIO_DAC8562_SYNC 0
  if(hspi->Instance == spi_tbl[_DEF_SPI1].h_spi->Instance){
		gpioPinWrite(GPIO_DAC8562_SYNC, _DEF_RESET);
  }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
  for (int i=0; i<SPI_MAX_CH; i++)
  {
    if (hspi->Instance == spi_tbl[i].h_spi->Instance)
    {
      spi_tbl[i].is_rx_done = true;
    }  
  }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  for (int i=0; i<SPI_MAX_CH; i++)
  {
    if (hspi->Instance == spi_tbl[i].h_spi->Instance)
    {
      spi_tbl[i].is_error = true;
    }
  }
}



#endif