/*
 * galvano.c
 *
 *  Created on: 2024. 9. 18.
 *      Author: User
 */

#include "galvano.h"
#include "galvano_uart.h"

#include "cli.h"
#include "dac8562.h"
#include "pwm.h"
#include "tim.h"

extern float voltage_A;
extern float voltage_B;

galvano_packet_t packetGalvano;
galvano_t galvano;
galvano_status_t galvano_status;

#ifdef _USE_HW_CLI
static void cliGalvano(cli_args_t *args);
#endif

static void parserPacket(galvano_t *p_galvano);
static uint16_t updateCrc(uint16_t crc_accum, uint8_t *data_blk_ptr,
                          uint16_t data_blk_size);
// static bool galvanoSendPacket(galvano_t *p_galvano, uint8_t inst, uint8_t
// *param);
static bool galvanoReceivePacket(galvano_t *p_galvano);
static bool galvanoProcessPKT(galvano_t *p_galvano, uint8_t rx_data);

// Instruction function
static void RunScanOnce(void);
static void SetAngle(galvano_status_t *p_motor, uint32_t angle,
                     axisType axisType); // 파라미터 값만큼 회전하며, 반환값은
                                         // 현재각도+이동한 값이다.
static uint32_t GetAngle(galvano_status_t *p_motor,
                         axisType axisType); // 반환값이 현재 각도의 값이다.
static void SetZeroAngle(galvano_status_t *p_motor); // 현재 각도를 0으로 설정.

typedef enum _motionState {
  init = 0,
  x_startToEnd,
  y_stepIncrease,
  x_endToStart,
} motionState;

enum state {
  STATE_HEADER = 0,
  STATE_LENGTH,
  STATE_INST,
  STATE_PARAM,
  STATE_CRC_H,
  STATE_CRC_L,
};
enum index {
  INDEX_HEADER = 0,
  INDEX_LENGTH,
  INDEX_INST,
  INDEX_PARAM,
};
enum instruction {
  INST_SET_ANGLE_X = 0x01,
  INST_SET_ANGLE_Y = 0x02,
  INST_SCAN_CONFIG = 0x03,
  INST_SCAN_ONCE = 0x04,
  INST_SCAN_CONTINOUS = 0x05,
  INST_SCAN_STOP = 0x06,
  INST_SET_ANGLE_ZERO = 0x07,
};

bool galvanoInit(void) {
  bool ret = true;
  dac8562_init();

  galvano_status.current_voltageA = voltage_A;

  galvano_status.current_voltageB = voltage_B;

  galvanoLoadDriver(&galvano, galvanoUartDriver);

#ifdef _USE_HW_CLI
  cliAdd("galvano", cliGalvano);
#endif
  return ret;
}

void galvanoGUIRun(void) {
  /*INSTUCTION
   0x01 : set_angle_X (A)
   0x02 : set angle_Y (B)
   0x03 : scan run
   0x04 : scan stop
   0x05 : set zero angle
   0x55 : open */

  if (galvano.driver.available(_DEF_GALVANO1) > 0) {
    if (galvanoReceivePacket(&galvano) == true) {
      switch (packetGalvano.inst) {
      case INST_SET_ANGLE_X:
        galvano_status.current_position_x = packetGalvano.param_1;
        SetAngle(&galvano_status, galvano_status.current_position_x, X_Axis);
        break;
      case INST_SET_ANGLE_Y:
        galvano_status.current_position_y = packetGalvano.param_1;
        SetAngle(&galvano_status, galvano_status.current_position_y, Y_Axis);
        break;
      case INST_SCAN_CONFIG:
        // TODO
        galvano_status.start_x = packetGalvano.param_1;
        galvano_status.start_y = packetGalvano.param_2;
        galvano_status.end_x = packetGalvano.param_3;
        galvano_status.end_y = packetGalvano.param_4;
        galvano_status.interval_x = packetGalvano.param_5;
        galvano_status.interval_y = packetGalvano.param_6;
        galvano_status.timeDelay = packetGalvano.param_7;
        break;
      case INST_SCAN_ONCE:
        // TODO
        RunScanOnce();
        break;
      case INST_SCAN_CONTINOUS:
        // TODO
        break;
      case INST_SCAN_STOP:
        galvano_status.keepScan = false;
        // TODO
        break;
      case INST_SET_ANGLE_ZERO:
        SetZeroAngle(&galvano_status);
        galvano_status.current_position_x = 0;
        galvano_status.current_position_y = 0;

        break;
      }
    }
  }
}

void RunScanOnce(void) {
  bool keepScan = true;

  int current_position_x = galvano_status.current_position_x,
      current_position_y = galvano_status.current_position_y,
      start_x = galvano_status.start_x, start_y = galvano_status.start_y,
      end_x = galvano_status.end_x, end_y = galvano_status.end_y,
      interval_x = galvano_status.interval_x,
      interval_y = galvano_status.interval_y;
  uint32_t timeDelay = galvano_status.timeDelay;

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
      delay_us(timeDelay);
      // onePulseTrigger();

      // Go to next "x_start => x_end" step
      state = x_startToEnd;

      break;

    case x_startToEnd: // "x_start => x_end"
      current_position_x += interval_x;

      dac8562_writeVoltage_A(current_position_x);
      delay_us(timeDelay);
      // onePulseTrigger();

      if (end_x == current_position_x) {
        state = y_stepIncrease;
      }
      break;

    case x_endToStart: // "x_end => x_start"
      current_position_x -= interval_x;
      dac8562_writeVoltage_A(current_position_x);
      delay_us(timeDelay);
      // onePulseTrigger();

      if (start_x == current_position_x) {
        state = y_stepIncrease;
      }

      break;

    case y_stepIncrease: // "y => y + y_interval"

      if (end_x == current_position_x && end_y == current_position_y) {
        keepScan = false;
        break;
      }

      current_position_y += interval_y;
      dac8562_writeVoltage_B(current_position_y);
      delay_us(timeDelay);
      // onePulseTrigger();

      if (current_position_x == end_x) {
        state = x_endToStart;
      } else if (current_position_x == start_x) {
        state = x_startToEnd;
      }
      break;

    default:
      break;
    }
  }
  dac8562_writeVoltage_AB(0);
  galvano_status.current_position_x = 0;
  galvano_status.current_position_y = 0;
}

void RunScanContinous(void) {
  bool keepScan = true;
  int current_position_x = galvano_status.current_position_x,
      current_position_y = galvano_status.current_position_y,
      start_x = galvano_status.start_x, start_y = galvano_status.start_y,
      end_x = galvano_status.end_x, end_y = galvano_status.end_y,
      interval_x = galvano_status.interval_x,
      interval_y = galvano_status.interval_y;
  uint32_t timeDelay = galvano_status.timeDelay;

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
      delay_us(timeDelay);
      // onePulseTrigger();

      // Go to next "x_start => x_end" step
      state = x_startToEnd;

      break;

    case x_startToEnd: // "x_start => x_end"
      current_position_x += interval_x;

      dac8562_writeVoltage_A(current_position_x);
      delay_us(timeDelay);
      // onePulseTrigger();

      if (end_x == current_position_x) {
        state = y_stepIncrease;
      }
      break;

    case x_endToStart: // "x_end => x_start"
      current_position_x -= interval_x;
      dac8562_writeVoltage_A(current_position_x);
      delay_us(timeDelay);
      // onePulseTrigger();

      if (start_x == current_position_x) {
        state = y_stepIncrease;
      }

      break;

    case y_stepIncrease: // "y => y + y_interval"

      if (end_x == current_position_x && end_y == current_position_y) {
        if (galvano.driver.available(_DEF_GALVANO1) > 0) {
          if (galvanoReceivePacket(&galvano) == true) {
            if (packetGalvano.inst == INST_SCAN_STOP) {
              keepScan = false;
            }
          }
        }
        break;
      }

      current_position_y += interval_y;
      dac8562_writeVoltage_B(current_position_y);
      delay_us(timeDelay);
      // onePulseTrigger();

      if (current_position_x == end_x) {
        state = x_endToStart;
      } else if (current_position_x == start_x) {
        state = x_startToEnd;
      }
      break;

    default:
      break;
    }
  }
  dac8562_writeVoltage_AB(0);
  galvano_status.current_position_x = 0;
  galvano_status.current_position_y = 0;
}

void SetAngle(galvano_status_t *p_galvano, uint32_t angle, axisType axisType) {
  if (axisType == X_Axis) {
    dac8562_writeVoltage_A(angle);
    p_galvano->current_voltageA = angle;
  } else if (axisType == Y_Axis) {
    dac8562_writeVoltage_B(angle);
    p_galvano->current_voltageB = angle;
  }
}

uint32_t GetAngle(galvano_status_t *p_galvano,
                  axisType axisType) // 반환값이 현재 각도의 값이다.
{
  if (axisType == X_Axis) {
    return p_galvano->current_voltageA;
  } else if (axisType == Y_Axis) {
    return p_galvano->current_voltageB;
  }
  return 0;
}

void SetZeroAngle(galvano_status_t *p_galvano) // 현재 각도를 0으로 설정.
{
  dac8562_writeVoltage_AB(0);
  p_galvano->current_voltageA = 0;
  p_galvano->current_voltageB = 0;
}

bool galvanoLoadDriver(galvano_t *p_galvano,
                       bool (*load_func)(galvano_driver_t *)) {
  bool ret;

  ret = load_func(&p_galvano->driver);
  return ret;
}
bool galvanoOpen(uint8_t ch, uint32_t baud) {
  bool ret = false;
  if (galvano.driver.isInit == false) {
    return false;
  }

  galvano.ch = ch;
  galvano.baud = baud;
  galvano.isOpen = galvano.driver.open(ch, baud);
  galvano.pre_time = millis();
  galvano.state = STATE_HEADER;

  ret = galvano.isOpen;
  return ret;
}
bool galvanoIsOpen(galvano_t *p_galvano) { return p_galvano->isOpen; }
bool galvanoClose(galvano_t *p_galvano) {
  bool ret = true;
  ret = p_galvano->driver.close(p_galvano->ch);
  p_galvano->isOpen = false;

  return ret;
}

// cli 다시 검토 및 send함수 작성
// 항상 galvanoUart 오픈 또는 선택시 galvanoUart 오픈
// bool galvanoSendPacket(galvano_t *p_galvano, uint8_t inst, uint8_t *param);
bool galvanoReceivePacket(galvano_t *p_galvano) {
  bool ret = false;
  uint8_t rx_data;
  uint32_t pre_time;

  if (p_galvano->isOpen == false) {
    return ret;
  }

  pre_time = millis();
  while (p_galvano->driver.available(p_galvano->ch) > 0) {
    rx_data = p_galvano->driver.read(p_galvano->ch);
    ret = galvanoProcessPKT(p_galvano, rx_data);

    if (ret == true) {
      break;
    }

    if (millis() - pre_time >= 50) {
      break;
    }
  }
  return ret;
}
bool galvanoProcessPKT(galvano_t *p_galvano, uint8_t rx_data) {
  bool ret = false;

  if (millis() - p_galvano->pre_time > 100) {
    p_galvano->state = STATE_HEADER;
  }
  p_galvano->pre_time = millis();

  switch (p_galvano->state) {
  case STATE_HEADER:

    if (rx_data == STX) {
      memset(p_galvano->packet_buf, 0, sizeof(uint8_t) * GALVANO_PKT_BUF_MAX);
      p_galvano->packet_buf[INDEX_HEADER] = rx_data;
      p_galvano->state = STATE_LENGTH;
    }
    break;

  case STATE_LENGTH:
    p_galvano->packet_buf[INDEX_LENGTH] = rx_data;
    // PARAM_LEN = TOTAL_LEN - (STX[1] +LENGTH[1]+INST[1] + CRC[2])
    p_galvano->param_length = rx_data - 5;
    p_galvano->param_current_pos = 0;
    p_galvano->state = STATE_INST;
    break;

  case STATE_INST:
    p_galvano->packet_buf[INDEX_INST] = rx_data;
    p_galvano->state = STATE_PARAM;
    break;

  case STATE_PARAM:
    p_galvano->packet_buf[INDEX_PARAM + p_galvano->param_current_pos] = rx_data;
    p_galvano->param_current_pos++;

    if (p_galvano->param_current_pos == p_galvano->param_length) {
      p_galvano->state = STATE_CRC_L;
    }
    break;

  case STATE_CRC_L:
    p_galvano->packet.crc = rx_data;
    p_galvano->state = STATE_CRC_H;
    break;

  case STATE_CRC_H:
    p_galvano->packet.crc |= rx_data << 8;
    uint16_t crc = updateCrc(0, p_galvano->packet_buf,
                             p_galvano->packet_buf[INDEX_LENGTH] - 2);

    if (crc == p_galvano->packet.crc) {
      ret = true;
      parserPacket(p_galvano);
    }

    p_galvano->state = STATE_HEADER;

    break;
  }
  return ret;
}

void parserPacket(galvano_t *p_galvano) {
  packetGalvano.length = p_galvano->packet_buf[INDEX_LENGTH];
  packetGalvano.inst = p_galvano->packet_buf[INDEX_INST];
  int index = INDEX_INST;
  switch (packetGalvano.inst) {
  case INST_SCAN_CONFIG:
    packetGalvano.param_1 = 0;
    for (int i = 0; i < 4; i++) {
      index++;
      packetGalvano.param_1 |= p_galvano->packet_buf[index] << 8 * i;
    }

    packetGalvano.param_2 = 0;
    for (int i = 0; i < 4; i++) {
      index++;
      packetGalvano.param_2 |= p_galvano->packet_buf[index] << 8 * i;
    }

    packetGalvano.param_3 = 0;
    for (int i = 0; i < 4; i++) {
      index++;
      packetGalvano.param_3 |= p_galvano->packet_buf[index] << 8 * i;
    }

    packetGalvano.param_4 = 0;
    for (int i = 0; i < 4; i++) {
      index++;
      packetGalvano.param_4 |= p_galvano->packet_buf[index] << 8 * i;
    }

    packetGalvano.param_5 = 0;
    for (int i = 0; i < 4; i++) {
      index++;
      packetGalvano.param_5 |= p_galvano->packet_buf[index] << 8 * i;
    }

    packetGalvano.param_6 = 0;
    for (int i = 0; i < 4; i++) {
      index++;
      packetGalvano.param_6 |= p_galvano->packet_buf[index] << 8 * i;
    }

    packetGalvano.param_7 = 0;
    for (int i = 0; i < 4; i++) {
      index++;
      packetGalvano.param_7 |= p_galvano->packet_buf[index] << 8 * i;
    }
    break;
  case INST_SET_ANGLE_X:
    packetGalvano.param_1 = 0;
    for (int i = 0; i < 4; i++) {
      index++;
      packetGalvano.param_1 |= p_galvano->packet_buf[index] << 8 * i;
    }
    break;
  case INST_SET_ANGLE_Y:
    packetGalvano.param_1 = 0;
    for (int i = 0; i < 4; i++) {
      index++;
      packetGalvano.param_1 |= p_galvano->packet_buf[index] << 8 * i;
    }
    break;
  default:
    break;
  }
}

uint16_t updateCrc(uint16_t crc_accum, uint8_t *data_blk_ptr,
                   uint16_t data_blk_size) {
  uint16_t i, j;
  const uint16_t crc_table[256] = {
      0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011, 0x8033,
      0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022, 0x8063, 0x0066,
      0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072, 0x0050, 0x8055, 0x805F,
      0x005A, 0x804B, 0x004E, 0x0044, 0x8041, 0x80C3, 0x00C6, 0x00CC, 0x80C9,
      0x00D8, 0x80DD, 0x80D7, 0x00D2, 0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB,
      0x00EE, 0x00E4, 0x80E1, 0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE,
      0x00B4, 0x80B1, 0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087,
      0x0082, 0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
      0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1, 0x01E0,
      0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1, 0x81D3, 0x01D6,
      0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2, 0x0140, 0x8145, 0x814F,
      0x014A, 0x815B, 0x015E, 0x0154, 0x8151, 0x8173, 0x0176, 0x017C, 0x8179,
      0x0168, 0x816D, 0x8167, 0x0162, 0x8123, 0x0126, 0x012C, 0x8129, 0x0138,
      0x813D, 0x8137, 0x0132, 0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E,
      0x0104, 0x8101, 0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317,
      0x0312, 0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
      0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371, 0x8353,
      0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342, 0x03C0, 0x83C5,
      0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1, 0x83F3, 0x03F6, 0x03FC,
      0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2, 0x83A3, 0x03A6, 0x03AC, 0x83A9,
      0x03B8, 0x83BD, 0x83B7, 0x03B2, 0x0390, 0x8395, 0x839F, 0x039A, 0x838B,
      0x038E, 0x0384, 0x8381, 0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E,
      0x0294, 0x8291, 0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7,
      0x02A2, 0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
      0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1, 0x8243,
      0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252, 0x0270, 0x8275,
      0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261, 0x0220, 0x8225, 0x822F,
      0x022A, 0x823B, 0x023E, 0x0234, 0x8231, 0x8213, 0x0216, 0x021C, 0x8219,
      0x0208, 0x820D, 0x8207, 0x0202};

  for (j = 0; j < data_blk_size; j++) {
    i = ((uint16_t)(crc_accum >> 8) ^ data_blk_ptr[j]) & 0xFF;
    crc_accum = (crc_accum << 8) ^ crc_table[i];
  }

  return crc_accum;
}

#ifdef _USE_HW_CLI
void cliGalvano(cli_args_t *args) {
  UNUSED(args);

  cliPrintf("	INST1: SET_VOLTAGE_X\n");
  cliPrintf("	INST2: SET_VOLTAGE_Y\n");
  cliPrintf("	INST3: SCAN_CONFIG, start_x start_y end_x end_y interval_x "
            "interval_y timeDelay\n");
  cliPrintf("	INST4: SCAN_ONCE\n");
  cliPrintf("	INST5: SCAN_CONTINOUS\n");
  cliPrintf("	INST6: SCAN_STOP\n");
  cliPrintf("	INST7: SET_ANGLE_ZERO\n");
}
#endif
