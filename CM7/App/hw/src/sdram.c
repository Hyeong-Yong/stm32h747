#include "sdram.h"


#ifdef _USE_HW_SDRAM
#include "cli.h"
//#include "fmc.h"


#define SDRAM_TIMEOUT                    ((uint32_t)0xFFFF)

  //
  // refresh rate = (COUNT + 1) * SDRAM clock freq (120Mhz)
  //                
  // COUNT = (SDRAM refresh period/Number of rows) - 20
#define REFRESH_COUNT                    ((uint32_t)0x1539)   /* SDRAM refresh counter (100MHz SDRAM clock) */



/**
  * @brief  FMC SDRAM Mode definition register defines
  */
#define SDRAM_MODEREG_BURST_LENGTH_1             ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2             ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4             ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8             ((uint16_t)0x0004)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL      ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED     ((uint16_t)0x0008)
#define SDRAM_MODEREG_CAS_LATENCY_2              ((uint16_t)0x0020)
#define SDRAM_MODEREG_CAS_LATENCY_3              ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD    ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE     ((uint16_t)0x0200)

#ifdef _USE_HW_CLI
static void cliSdram(cli_args_t *args);
#endif
static bool sdramMemInit(SDRAM_HandleTypeDef *p_sdram);

extern void HAL_FMC_MspInit(void);
extern void HAL_FMC_MspDeInit(void);

static bool is_init = false;
static SDRAM_HandleTypeDef hsdram1;






bool sdramInit(void)
{
  bool ret = true;;

  
  ret = sdramMemInit(&hsdram1);
  
  logPrintf("[%s] sdramInit()\n", ret ? "OK":"NG");
  if (ret == true)
  {
    logPrintf("     addr : 0x%X\n", SDRAM_MEM_ADDR);
    logPrintf("     size : %dMB\n", (int)(SDRAM_MEM_SIZE/1024/1024));
  }

#ifdef _USE_HW_CLI
  cliAdd("sdram", cliSdram);
#endif

  is_init = ret;

  return ret;
}

bool sdramIsInit(void)
{
  return is_init;
}

uint32_t sdramGetAddr(void)
{
  return SDRAM_MEM_ADDR;
}

uint32_t sdramGetLength(void)
{
  return SDRAM_MEM_SIZE;
}

static FMC_SDRAM_CommandTypeDef Command;

bool sdramMemInit(SDRAM_HandleTypeDef *p_sdram)
{
  FMC_SDRAM_TimingTypeDef SdramTiming= {0};
  // SDRAM-CLK = 240Mhz/2 = 120MHz
  //
  p_sdram->Instance                 = FMC_SDRAM_DEVICE;
  /* p_sdram->Init */
  p_sdram->Init.SDBank              = FMC_SDRAM_BANK1;
  p_sdram->Init.ColumnBitsNumber    = FMC_SDRAM_COLUMN_BITS_NUM_9;
  p_sdram->Init.RowBitsNumber       = FMC_SDRAM_ROW_BITS_NUM_13;
  p_sdram->Init.MemoryDataWidth     = FMC_SDRAM_MEM_BUS_WIDTH_16;
  p_sdram->Init.InternalBankNumber  = FMC_SDRAM_INTERN_BANKS_NUM_4;
  p_sdram->Init.CASLatency          = FMC_SDRAM_CAS_LATENCY_3;
  p_sdram->Init.WriteProtection     = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
  p_sdram->Init.SDClockPeriod       = FMC_SDRAM_CLOCK_PERIOD_2;
  p_sdram->Init.ReadBurst           = FMC_SDRAM_RBURST_ENABLE;
  p_sdram->Init.ReadPipeDelay       = FMC_SDRAM_RPIPE_DELAY_0;

  /* Timing configuration for 120Mhz/8.33ns as SDRAM clock frequency*/

  /* SdramTiming */
  SdramTiming.LoadToActiveDelay     = 2;  // tMRD 2*tCK
  SdramTiming.ExitSelfRefreshDelay  = 9;  // tXSR 72ns / 8.33ns = 9
  SdramTiming.SelfRefreshTime       = 6;  // tRAS 42ns / 8.33ns = 6
  SdramTiming.RowCycleDelay         = 8;  // tRC 60ns / 8.33ns = 8
  SdramTiming.WriteRecoveryTime     = 2;  // tWR 2*tCK
  SdramTiming.RPDelay               = 2;  // tRP 15ns / 8.33ns = 2
  SdramTiming.RCDDelay              = 2;  // tRCD 15ns / 8.33ns = 2

  if (HAL_SDRAM_Init(p_sdram, &SdramTiming) != HAL_OK)
  {
    return false;
  }


  // Step 1: Configure a clock configuration enable command 
  //
  Command.CommandMode            = FMC_SDRAM_CMD_CLK_ENABLE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = 0;
  if(HAL_SDRAM_SendCommand(p_sdram, &Command, SDRAM_TIMEOUT) != HAL_OK) 
  {
    logPrintf("[  ] sdram step 1 fail\n");
    return false;
  }

  // Step 2: Insert 100 us minimum delay */ 
  // Inserted delay is equal to 1 ms due to systick time base unit (ms)
  //
  delay(1);  
    
  // Step 3: Configure a PALL (precharge all) command 
  //
  Command.CommandMode            = FMC_SDRAM_CMD_PALL;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = 0;  
  if(HAL_SDRAM_SendCommand(p_sdram, &Command, SDRAM_TIMEOUT) != HAL_OK) 
  {
    logPrintf("[  ] sdram step 3 fail\n");
    return false;
  }

  // Step 4: Configure a Refresh command 
  //
  Command.CommandMode            = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber      = 8;
  Command.ModeRegisterDefinition = 0; 
  if(HAL_SDRAM_SendCommand(p_sdram, &Command, SDRAM_TIMEOUT) != HAL_OK) 
  {
    logPrintf("[  ] sdram step 4 fail\n");
    return false;
  }

  // Step 5: Program the external memory mode register
  //
  volatile uint32_t tmpmrd = 0;

  tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1          |\
                     SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   |\
                     SDRAM_MODEREG_CAS_LATENCY_3           |\
                     SDRAM_MODEREG_OPERATING_MODE_STANDARD |\
                     SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

  Command.CommandMode            = FMC_SDRAM_CMD_LOAD_MODE;
  Command.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
  Command.AutoRefreshNumber      = 1;
  Command.ModeRegisterDefinition = tmpmrd;

  if(HAL_SDRAM_SendCommand(p_sdram, &Command, SDRAM_TIMEOUT) != HAL_OK) 
  {
    logPrintf("[  ] sdram step 5 fail\n");
    return false;
  }

  // Step 6: Set the refresh rate counter
  if(HAL_SDRAM_ProgramRefreshRate(p_sdram, REFRESH_COUNT) != HAL_OK)
  {
    logPrintf("[  ] sdram step 6 fail\n");
    return false;
  }

  return true;
}

#ifdef _USE_HW_CLI
void cliSdram(cli_args_t *args)
{
  bool ret = false;
  uint8_t number;
  uint32_t i;
  uint32_t pre_time;
  uint32_t exe_time;


  if(args->argc == 1 && args->isStr(0, "info") == true)
  {
    cliPrintf("sdram init : %s\n", is_init ? "True":"False");
    cliPrintf("sdram addr : 0x%X\n", SDRAM_MEM_ADDR);
    cliPrintf("sdram size : %dMB\n", SDRAM_MEM_SIZE/1024/1024);
    ret = true;
  }

  if(args->argc == 2 && args->isStr(0, "test") == true)
  {
    uint32_t *p_data = (uint32_t *)SDRAM_MEM_ADDR;

    number = (uint8_t)args->getData(1);

    while(number > 0)
    {
      pre_time = millis();
      for (i=0; i<SDRAM_MEM_SIZE/4; i++)
      {
        p_data[i] = i;
      }
      exe_time = millis()-pre_time;
      cliPrintf("Write : %d MB/s, %d ms\n", ((SDRAM_MEM_SIZE/1024/1024) * 1000 / exe_time), exe_time);


      volatile uint32_t data_sum = 0;
      pre_time = millis();
      for (i=0; i<SDRAM_MEM_SIZE/4; i++)
      {
        data_sum += p_data[i];
      }
      exe_time = millis()-pre_time;
      cliPrintf("Read  : %d MB/s, %d ms\n", ((SDRAM_MEM_SIZE/1024/1024) * 1000 / exe_time), exe_time);


      for (i=0; i<SDRAM_MEM_SIZE/4; i++)
      {
        if (p_data[i] != i)
        {
          cliPrintf( "%d : %d fail\n", i, p_data[i]);
          break;
        }
      }

      if (i == SDRAM_MEM_SIZE/4)
      {
        cliPrintf( "Count %d\n", number);
        cliPrintf( "Sdram %d MB OK\n\n", SDRAM_MEM_SIZE/1024/1024);
        for (i=0; i<SDRAM_MEM_SIZE/4; i++)
          {
            p_data[i] = 0x5555AAAA;
          }
      }

      number--;

      if (cliAvailable() > 0)
      {
        cliPrintf( "Stop test...\n");
        break;
      }
    }
   
    ret = true;
  }
  
  
  if (ret == false)
  {
    cliPrintf("sdram info\n");
    cliPrintf("sdram test 1~100 \n");
  }
}
#endif
#endif