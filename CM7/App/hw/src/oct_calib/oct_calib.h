/*
 * oct_calib.h
 *
 * - ADC1/ADC2/ADC3의 cos/sin/광파워 3채널 신호 기반 self-calibration
 *   (Heydemann ellipse fitting) 모듈 (OCT = Optical Current Transformer)
 *
 * - 계산 코어: oct_operations.h/.c의 matrix_vandermonde, matrix_least_squares,
 *   matrix_calibration, matrix_extract_phase, matrix_meter_current가 담당
 * - STM32 펌웨어 환경(메모리/연산 부담) 고려해 double 대신 float 정밀도 사용
 * - x,y 신호만으로는 부족 → 광파워 채널(rank2)로 x,y 정규화하는 전처리 단계가
 *   이 파일에 별도로 존재
 *
 * - 채널 구성: 채널0=cos, 채널1=sin, 채널2=광파워(정규화 기준)
 * - N_SAMPLES(=POINTS_PER_PERIOD*3, 60Hz 3주기)번 수신 후 정규화:
 *     x_n = (Ia0_n - ZERO) / (Ia2_n - ZERO)
 *     y_n = (Ia1_n - ZERO) / (Ia2_n - ZERO)
 * - 아래 타원(ellipse) 모델에 최소자승법으로 피팅 → (a0, a1, b0, b1, eps) 5개 파라미터 도출:
 *     x_n = a0 + a1*cos(phi_n)
 *     y_n = b0 + b1*sin(phi_n + eps)
 *
 * - ADC1, ADC2: 3채널 모두 사용
 * - ADC3: 4채널 중 앞 3개(ch2,ch6,ch7)만 캘리브레이션 대상, 4번째(ch8, 전압)는 제외
 *
 * - 캘리브레이션 결과(a0,a1,b0,b1,eps)로 실시간 위상/전류(I_A 등) 계산하는 부분은
 *   이 파일이 아니라 oct_realTimeCalculate.h/.c 담당
 *   (위상 오프셋 계산 없이, UART/CLI로 ROM에 저장해 둔 scale factor를 곱하는 방식)
 */

#ifndef SRC_HW_INCLUDE_OCT_CALIB_H_
#define SRC_HW_INCLUDE_OCT_CALIB_H_

#include "hw_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _USE_HW_ADC

// - 60Hz 기준 3주기 = POINTS_PER_PERIOD(IEC61850_SV.h) * 3
// - 슬라이드 예시(N=768)와 동일하게 맞춤
#define ADC_CALIB_N_SAMPLES   (256 * 3)   // = 768

// 0V 입력 시 ADC 코드값 - 16bit 단극성 기준 2^16/2 = 32768 추정치.
#define ADC_CALIB_ZERO_CODE   32768.0f

typedef struct
{
  bool  valid;
  float a0;
  float a1;
  float b0;
  float b1;
  float eps;          // phi0 (Heydemann skew 위상)
} AdcCalibParam;

// - 캘리브레이션 대상 ADC 번호
// - 사용처: adcCalib() 인자, s_cliCalibParam[]/CLI 인자(1~3|all) 변환, switch-case 분기 등
// - ADC 번호를 다루는 모든 곳에서 매직넘버(1,2,3) 대신 이 enum 사용
// - ADC_CALIB_NUM_ALL: adcCalib()에 "ADC1/2/3 동시 실행"을 지정할 때만 사용(단일 ADC
//   전용 코드에서는 등장하지 않음 - switch-case의 default로 자연히 걸러짐)
typedef enum
{
  ADC_CALIB_NUM_ALL = 0,
  ADC_CALIB_NUM_1   = 1,
  ADC_CALIB_NUM_2   = 2,
  ADC_CALIB_NUM_3   = 3,
} AdcCalibNum;

// 3개 ADC의 캘리브레이션 결과 묶음 - Flash 저장/로드 단위.
typedef struct
{
  uint32_t      magic;   // 유효성 판별용 매직넘버
  AdcCalibParam adc1;
  AdcCalibParam adc2;
  AdcCalibParam adc3;
} AdcCalibParamSet;

// 모듈 초기화 - CLI 명령("oct_calib") 등록. hwInit()에서 adcInit() 이후 호출.
bool octCalibInit(void);

// - adcNum==ADC_CALIB_NUM_1/2/3: 해당 ADC 하나만 N_SAMPLES번 캡처 → RAM 저장 → 파라미터
//   계산. 결과는 out1/out2/out3 중 그 ADC 번호에 대응하는 자리에만 기록(나머지 자리는
//   NULL 가능)
// - adcNum==ADC_CALIB_NUM_ALL: ADC1/2/3 동시(세 ADC 모두 거의 같은 시점에 캡처 시작)
//   캘리브레이션 - out1/out2/out3 모두 사용(NULL 불가). 개별 3회 실행 대비 총
//   소요시간 단축(약 50ms, 개별 실행 시 약 150ms), 세 ADC raw 데이터가 같은
//   시간대에 캡처됨
// - out1/out2/out3는 항상 ADC1/ADC2/ADC3에 고정 대응(persistCalibParam()과 동일한 관례)
// - 반드시 태스크(스레드) 컨텍스트에서 호출(내부에서 osDelay로 대기)
// - ADC3(ADC_CALIB_NUM_3)는 ch2,ch6,ch7(rank0~2)만 사용
// - 계산 성공(valid == true) 시 결과를 내부 Flash에 자동 저장(adcCalib_Param_SaveToFlash,
//   나머지 ADC의 기존 저장값은 보존) - ALL 실행 시 1회 저장으로 일괄 반영
// - 캡처 성공 시(계산 성공/실패 무관) raw 데이터도 자동 저장
//   (adcCalib_RawData_SaveToFlash, calib1/2/3RawBuf 전체를 그대로 저장)
bool adcCalib(AdcCalibNum adcNum, AdcCalibParam *out1, AdcCalibParam *out2, AdcCalibParam *out3);

// - adc.c의 HAL_ADC_ConvCpltCallback(ADC1/2/3 분기)에서 호출하는 훅
// - 해당 ADC의 캘리브레이션 캡처가 방금 완료됐으면 true 반환
// - 호출자(adc.c)는 true 반환 시 그 콜백 안에서 "정상 스트리밍" 처리 건너뛰어야 함
//   (캘리브레이션 중엔 그 ADC의 스트리밍 버퍼 갱신 안 됨)
bool adcCalibNotifyConvCplt(ADC_HandleTypeDef *hadc);

// - ADC1/ADC2/ADC3 중 하나라도 캘리브레이션(캡처+파라미터 계산) 진행 중이면 true
// - adc.c는 이 플래그 true인 동안 SV 패킷 조립/큐 전달(=이더넷 발행) 전면 건너뛰어야 함
// - adcCalib()이 캡처 시작 전 true로, calibComputeParams() 종료 후
//   (성공/실패 무관) false로 복귀
extern volatile bool adcCalibBusy;

// - 3개 ADC 결과를 한 번에 내부 Flash에 저장/로드하는 단위
// - 저장 위치: FLASH_BANK_1의 마지막 섹터(0x080E0000, 128KB)
//   - 코드/데이터 영역(현재 ~0x08036224까지 사용)과 충분히 이격
// - 매직 "CAL3": AdcCalibParam에서 phaseOffset 필드 제거로 레이아웃 변경 →
//   예전 "CAL2" 데이터를 새 구조체로 오인 해석하지 않도록 매직 상향
// - 실제 저장/로드 함수(adcCalib_Param_SaveToFlash/LoadFromFlash)는 oct_calib.c
//   내부에서만 쓰여 static - 필요하면 oct_calib.c의 persistCalibParam()이나
//   cliCalibOCT()를 통해 간접적으로 이용
#define ADC_CALIB_FLASH_ADDR   0x080E0000
#define ADC_CALIB_FLASH_MAGIC  0x43414C33u  // "CAL3"

// - 마지막으로 캡처된 raw 데이터(calib1/2/3RawBuf 내용)를 그대로 저장하는 섹터
// - 저장 위치: FLASH_BANK_1 섹터6(0x080C0000, 128KB) - 파라미터 저장용 섹터7과 별개
//   → 파라미터 저장/로드해도 raw dump 유지(반대도 마찬가지)
// - 크기: (3+3+4)*768*2 = 15,360 bytes(~15KB, 섹터 용량의 12% 정도만 사용)
// - 실제 저장/로드 함수(adcCalib_RawData_SaveToFlash/LoadFromFlash)도 oct_calib.c
//   내부에서만 쓰여 static
#define ADC_CALIB_RAW_FLASH_ADDR    0x080C0000
#define ADC_CALIB_RAW_FLASH_MAGIC   0x52415731u  // "RAW1"

#define ADC_CALIB_RAW_HDR_SIZE      32u  // magic + 여유(32바이트 flash word 정렬)
#define ADC_CALIB_RAW_ADC1_SIZE     (3u * ADC_CALIB_N_SAMPLES * (uint32_t)sizeof(uint16_t))
#define ADC_CALIB_RAW_ADC2_SIZE     (3u * ADC_CALIB_N_SAMPLES * (uint32_t)sizeof(uint16_t))
#define ADC_CALIB_RAW_ADC3_SIZE     (4u * ADC_CALIB_N_SAMPLES * (uint32_t)sizeof(uint16_t))
#define ADC_CALIB_RAW_ADC1_OFFSET   (ADC_CALIB_RAW_HDR_SIZE)
#define ADC_CALIB_RAW_ADC2_OFFSET   (ADC_CALIB_RAW_ADC1_OFFSET + ADC_CALIB_RAW_ADC1_SIZE)
#define ADC_CALIB_RAW_ADC3_OFFSET   (ADC_CALIB_RAW_ADC2_OFFSET + ADC_CALIB_RAW_ADC2_SIZE)
#define ADC_CALIB_RAW_TOTAL_SIZE    (ADC_CALIB_RAW_ADC3_OFFSET + ADC_CALIB_RAW_ADC3_SIZE)

#endif  // _USE_HW_ADC

#ifdef __cplusplus
}
#endif

#endif /* SRC_HW_INCLUDE_OCT_CALIB_H_ */
