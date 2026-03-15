#ifndef E937FC59_2A65_4AB8_851C_4187E9118E65
#define E937FC59_2A65_4AB8_851C_4187E9118E65
#ifndef A421C194_48A5_4053_9935_DE4FD81BEEE3
#define A421C194_48A5_4053_9935_DE4FD81BEEE3

#ifdef __cplusplus
extern "C" {
#endif

#include "hw_def.h"


#ifdef _USE_HW_UART

#define UART_MAX_CH         HW_UART_MAX_CH


bool     uartInit(void);
bool     uartOpen(uint8_t ch, uint32_t baud);
bool     uartClose(uint8_t ch);
uint32_t uartAvailable(uint8_t ch);
bool     uartFlush(uint8_t ch);
uint8_t  uartRead(uint8_t ch);
uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t length);
uint32_t uartPrintf(uint8_t ch, const char *fmt, ...);
uint32_t uartGetBaud(uint8_t ch);

#endif

#ifdef __cplusplus
}
#endif

#endif /* A421C194_48A5_4053_9935_DE4FD81BEEE3 */


#endif /* E937FC59_2A65_4AB8_851C_4187E9118E65 */
