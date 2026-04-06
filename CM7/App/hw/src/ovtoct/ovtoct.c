

#include "cli.h"
#include "adc.h"
#include "uart.h"
#include "ovtoct.h"
#include "ovtoct_uart.h"
#include "crc.h"

#ifdef _USE_FREERTOS
#include "cmsis_os.h"
#endif


ovtOct_packet_t packet;
ovtOct_t ovtOct;

#ifdef _USE_HW_CLI
static void cliOvtOct(cli_args_t *args);
#endif

static void parserPacket(ovtOct_t *p_OvtOct);
static bool ovtOctReceivePacket(ovtOct_t *p_OvtOct);
static bool ovtOctProcessPKT(ovtOct_t *p_OvtOct, uint8_t rx_data);

static void ovt_Acquire();
static void ovt_Run();
static bool KeepLoop();

enum state
{
    STATE_HEADER = 0,
    STATE_LENGTH_L,
    STATE_LENGTH_H,
    STATE_INST,
    STATE_DATA,
    STATE_CRC_H,
    STATE_CRC_L,
    STATE_END,
};
enum index
{
    INDEX_HEADER = 0,
    INDEX_LENGTH_L,
    INDEX_LENGTH_H,
    INDEX_INST,
    INDEX_DATA,
    INDEX_CRC_L,
    INDEX_CRC_H,
    INDEX_END,
};
enum instruction
{
    INST_OVT_ACQUIRE = 0x01,
    INST_OVT_RUN = 0x02,
    INST_OVT_STOP = 0x03,
    INST_OCT_ACQUIRE = 0x04
};

bool ovtOct_init()
{
    bool ret = true;

    ovtOctLoadDriver(&ovtOct, ovtOctUartDriver);
    ovtOct.isOpen = true;
#ifdef _USE_HW_CLI
    cliAdd("OvtOct", cliOvtOct);
#endif
    return ret;
}

void ovtOct_GUI_Run()
{
    /*INSTUCTION
     0x01 : OVT_ACQUIRE
     0x02 : OVT_RUN
     0x03 : OVT_STOP
     0x04 : OCT_ACQUIRE */

    if (ovtOct.driver.available(_DEF_OVTOCT1) > 0)
    {
        if (ovtOctReceivePacket(&ovtOct) == true)
        {
            switch (packet.inst)
            {
            case INST_OVT_ACQUIRE:
                ovt_Acquire();
                break;
            case INST_OVT_RUN:
                ovt_Run();
                break;
            case INST_OVT_STOP:
                //Nothing, because keepLoop() excute during OVT_RUN()
                break;
            case INST_OCT_ACQUIRE:
                break;
            }
        }
    }
}

extern __IO uint32_t adcBuf_OVT[];
extern bool adc_OVT_Finished;
uint8_t sendPacketBuf[ADC_BUF_SIZE * 3 + 7];
bool keepLoop = true;

void ovt_Acquire()
{
    adcOvtMeasure();
    keepLoop = true;
    while (keepLoop)
    {
        if (adc_OVT_Finished == true)
        {
            keepLoop = false;
            adc_OVT_Finished = false;
            int i = 0;
            // HEADER
            sendPacketBuf[i++] = STX;
            // LENGTH = LENGTH(2)+INSTRUCTION(1)+DATA(N=ADC_BUF_SIZE*3)
            sendPacketBuf[i++] = (uint8_t)((ADC_BUF_SIZE*3+3) & 0xFF);
            sendPacketBuf[i++] = (uint8_t)(((ADC_BUF_SIZE*3+3) >> 8) & 0xFF);
            // INSTUCTION
            sendPacketBuf[i++] = (uint8_t)(INST_OVT_ACQUIRE & 0xFF);
            // DATA
            int buf_index = 0;
            for (int count = i; count < ADC_BUF_SIZE * 3; count = count + 3)
            {
                sendPacketBuf[count] = (uint8_t)(adcBuf_OVT[buf_index] & 0xFF);
                sendPacketBuf[count + 1] = (uint8_t)(adcBuf_OVT[buf_index] >> 8 & 0xFF);
                sendPacketBuf[count + 2] = (uint8_t)(adcBuf_OVT[buf_index] >> 16 & 0xFF);
                buf_index++;
            }
            // STX(1)+LENGTH(2)+INSTRUCTION(1)+DATA(N=ADC_BUF_SIZE*3) excluding crc [2bit] +etx[1bit]
            uint16_t crc = crc16_compute(sendPacketBuf, ADC_BUF_SIZE * 3 + 4);
            sendPacketBuf[ADC_BUF_SIZE * 3 + i] = (uint8_t)(crc & 0xFF);
            i++;
            sendPacketBuf[ADC_BUF_SIZE * 3 + i] = (uint8_t)((crc >> 8) & 0xFF);
            i++;
            sendPacketBuf[ADC_BUF_SIZE * 3 + i] = ETX;
            i++;
            uartWrite(_DEF_UART2, (uint8_t *)sendPacketBuf, ADC_BUF_SIZE * 3 + i);
        }
           #ifdef _USE_FREERTOS
              osDelay(1);
           #endif
    }
}

void ovt_Run()
{
    adcOvtMeasure();
    keepLoop = true;
    bool isTimeout = false;
    ovtOct.pre_time = millis();
    while (KeepLoop() & !isTimeout)
    {
        if (adc_OVT_Finished == true)
        {
            adc_OVT_Finished = false;

            int i = 0;
            // HEADER
            sendPacketBuf[i++] = STX;
            // LENGTH = LENGTH(2)+INSTRUCTION(1)+DATA(N=ADC_BUF_SIZE*3)
            sendPacketBuf[i++] = (uint8_t)((ADC_BUF_SIZE*3+3) & 0xFF);
            sendPacketBuf[i++] = (uint8_t)(((ADC_BUF_SIZE*3+3) >> 8) & 0xFF);
            // INSTUCTION
            sendPacketBuf[i++] = (uint8_t)(INST_OVT_RUN & 0xFF);
            // DATA
            int buf_index = 0;
            for (int count = i; count < ADC_BUF_SIZE * 3; count = count + 3)
            {
                sendPacketBuf[count] = (uint8_t)(adcBuf_OVT[buf_index] & 0xFF);
                sendPacketBuf[count + 1] = (uint8_t)(adcBuf_OVT[buf_index] >> 8 & 0xFF);
                sendPacketBuf[count + 2] = (uint8_t)(adcBuf_OVT[buf_index] >> 16 & 0xFF);
                buf_index++;
            }
            // STX(1)+LENGTH(2)+INSTRUCTION(1)+DATA(N=ADC_BUF_SIZE*3) excluding crc [2bit] +etx[1bit]
            uint16_t crc = crc16_compute(sendPacketBuf, ADC_BUF_SIZE * 3 + 4);
            sendPacketBuf[ADC_BUF_SIZE * 3 + i] = (uint8_t)(crc & 0xFF);
            i++;
            sendPacketBuf[ADC_BUF_SIZE * 3 + i] = (uint8_t)((crc >> 8) & 0xFF);
            i++;
            sendPacketBuf[ADC_BUF_SIZE * 3 + i] = ETX;
            i++;
            uartWrite(_DEF_UART2, (uint8_t *)sendPacketBuf, ADC_BUF_SIZE * 3 + i);

            adcOvtMeasure();
            ovtOct.pre_time = millis();
        }

        if (millis() - ovtOct.pre_time >= 1500)
        {
            isTimeout = true;
            logPrintf("OVT_RUN timeout\n");
        }
    }
}

bool KeepLoop()
{

    if (ovtOct.driver.available(_DEF_OVTOCT1) > 0)
    {
        if (ovtOctReceivePacket(&ovtOct) == true)
        {
            if (packet.inst == 0x03)
            {
                keepLoop = false;
            }
        }
    }

    return keepLoop;
}

bool ovtOctLoadDriver(ovtOct_t *p_ovtOct, bool (*load_func)(ovtOct_driver_t *))
{
    bool ret;

    ret = load_func(&p_ovtOct->driver);
    return ret;
}

bool ovtOctOpen(uint8_t ch, uint32_t baud)
{
    bool ret = false;
    if (ovtOct.driver.isInit == false)
    {
        return false;
    }

    ovtOct.ch = ch;
    ovtOct.baud = baud;
    ovtOct.isOpen = ovtOct.driver.open(ch, baud);
    ovtOct.pre_time = millis();
    ovtOct.state = STATE_HEADER;
    ret = ovtOct.isOpen;
    return ret;
}

bool ovtOctIsOpen(ovtOct_t *p_ovtOct)
{
    return p_ovtOct->isOpen;
}

// cli 다시 검토 및 send함수 작성
// 항상 OvtOctUart 오픈 또는 선택시 OvtOctUart 오픈
bool ovtOctReceivePacket(ovtOct_t *p_ovtOct)
{
    bool ret = false;
    uint8_t rx_data;
    uint32_t pre_time;

    if (p_ovtOct->isOpen == false)
    {
        return ret;
    }

    pre_time = millis();
    while (p_ovtOct->driver.available(p_ovtOct->ch) > 0)
    {
        rx_data = p_ovtOct->driver.read(p_ovtOct->ch);
        ret = ovtOctProcessPKT(p_ovtOct, rx_data);

        if (ret == true)
        {
            break;
        }

        if (millis() - pre_time >= 50)
        {
            break;
        }
    }
    return ret;
}
bool ovtOctProcessPKT(ovtOct_t *p_ovtOct, uint8_t rx_data)
{
    bool ret = false;

    if (millis() - p_ovtOct->pre_time > 100)
    {
        p_ovtOct->state = STATE_HEADER;
    }
    p_ovtOct->pre_time = millis();

    switch (p_ovtOct->state)
    {
    case STATE_HEADER:

        if (rx_data == STX)
        {
            memset(p_ovtOct->packet_buf, 0, sizeof(uint8_t) * OVTOCT_PKT_BUF);
            p_ovtOct->packet_buf[INDEX_HEADER] = rx_data;
            p_ovtOct->state = STATE_LENGTH_L;
        }
        break;

    case STATE_LENGTH_L:
        p_ovtOct->packet_buf[INDEX_LENGTH_L] = rx_data;
        p_ovtOct->state = STATE_LENGTH_H;
        break;
    case STATE_LENGTH_H:
        p_ovtOct->packet_buf[INDEX_LENGTH_H] = rx_data;
        p_ovtOct->state = STATE_INST;
        break;
    case STATE_INST:
        p_ovtOct->packet_buf[INDEX_INST] = rx_data;
        p_ovtOct->state = STATE_DATA;
        break;
    case STATE_DATA:
        p_ovtOct->packet_buf[INDEX_DATA] = rx_data;
        p_ovtOct->state = STATE_CRC_L;
        break;
    case STATE_CRC_L:
        p_ovtOct->packet_buf[INDEX_CRC_L] = rx_data;
        p_ovtOct->packet.crc = rx_data;
        p_ovtOct->state = STATE_CRC_H;
        break;
    case STATE_CRC_H:
        p_ovtOct->packet_buf[INDEX_CRC_H] = rx_data;
        p_ovtOct->packet.crc |= rx_data << 8;
        p_ovtOct->state = STATE_END;
        break;
    case STATE_END:
        p_ovtOct->packet_buf[INDEX_END] = rx_data;
        p_ovtOct->packet.length = (p_ovtOct->packet_buf[INDEX_LENGTH_L]) | (p_ovtOct->packet_buf[INDEX_LENGTH_H] << 8);
        // crc packet[2]
        p_ovtOct->packet.crc = p_ovtOct->packet_buf[INDEX_CRC_L] | p_ovtOct->packet_buf[INDEX_CRC_H] << 8;

        // crc calculation for total packets : <length + STX>
        uint16_t crc = crc16_compute(p_ovtOct->packet_buf, p_ovtOct->packet.length + 1);

        // Comparsion of CRC
        if (crc == p_ovtOct->packet.crc)
        {
            ret = true;

            parserPacket(p_ovtOct);
        }

        p_ovtOct->state = STATE_HEADER;
        break;
    }
    return ret;
}

void parserPacket(ovtOct_t *p_ovtOct)
{
    packet.inst = p_ovtOct->packet_buf[INDEX_INST];

    // inst에 따른 data 정의 // 이 OVTOCT에서는 시작과 정지 명령만 존재하여 의미가 없음
    switch (packet.inst)
    {
    case INST_OVT_RUN:
        break;
    default:
        break;
    }
}

// #define _USE_HW_USB_OCTOVT

#ifdef _USE_HW_USB_OCTOVT
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (AdcHandle->Instance == ADC1)
    {
        adc_OVT_Finished = true;
    }
}
#endif

#ifdef _USE_HW_CLI
void cliOvtOct(cli_args_t *args)
{
    bool ret = false;

    if (args->argc == 1)
    {
        if (args->isStr(0, "ovt_acquire") == true)
        {
            ovt_Acquire();
            cliPrintf("OVT_ACQUIRE\n");
            ret = true;
        }
    }

    if (ret == false)
    {
        cliPrintf("=== \n");
        cliPrintf("ovtoct ovt_run\n");
    }
}
#endif