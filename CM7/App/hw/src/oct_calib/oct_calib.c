/*
 * oct_calib.c
 *
 * - ADC1/ADC2/ADC3의 cos/sin/광파워 채널 기반 OCT(Optical Current Transformer)
 *   self-calibration
 * - 담당 범위: ADC 도메인만(DMA 캡처, 광파워 정규화, Flash 저장/로드, CLI)
 * - Heydemann ellipse-fit 계산은 oct_operations.c(도메인 연산) /
 *   oct_matrix.c(malloc/free 기반 Matrix ADT)로 분리
 *
 * - 절차(ADC1/2/3 공통):
 *   1) 정상 스트리밍용 circular DMA 일시 정지 → 캘리브레이션 전용 버퍼로
 *      N_SAMPLES(=768)개 트리거만큼 1회성 캡처
 *   2) 정규화: x_n = (Ia0_n - ZERO) / (Ia2_n - ZERO), y_n = (Ia1_n - ZERO) / (Ia2_n - ZERO)
 *      (rank0=cos, rank1=sin, rank2=광파워. ADC3의 rank3(전압)은 미사용)
 *   3) 정규화된 x_n,y_n N개를 Matrix(N x 1, malloc)로 생성 → matrix_calibration()에
 *      일괄 전달(배치 방식) → (a0, a1, b0, b1, eps) 획득
 *      - 계산에 쓴 Matrix들은 종료 즉시 matrix_destroy()로 해제(heap 누수 방지)
 *   4) 정상 스트리밍 DMA 재개
 *
 * - 위상 오프셋 계산 및 캘리브레이션 결과(a0,a1,b0,b1,eps) 기반 실시간
 *   전류(I_A 등) 계산은 이 파일이 아니라 oct_realTimeCalculate.c 담당
 */

#include "oct_calib.h"

#ifdef _USE_HW_ADC

#include "oct_operations.h"
#include "oct_realTimeCalculate.h"   // octRealTimeSetScale/octRealTimeGetScale ("scale" 서브커맨드용)

#include "IEC61850_SV.h"   // ASDU_NUM
#include "flash.h"
#include "cmsis_os.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifdef _USE_HW_CLI
#include "cli.h"
static void cliCalibOCT(cli_args_t *args);
#endif

// - 이 파일 안에서만 쓰이는(외부 모듈 호출 없음) Flash 저장/로드 함수라 static
// - 정의 위치(파일 하단 "Flash 저장/로드" 절)보다 먼저 쓰이는 곳(persistCalibParam,
//   adcCalib)이 있어 앞에서 미리 선언
static bool adcCalib_Param_SaveToFlash(const AdcCalibParamSet *set);
static bool adcCalib_Param_LoadFromFlash(AdcCalibParamSet *set);
static bool adcCalib_RawData_SaveToFlash(void);
static bool adcCalib_RawData_LoadFromFlash(void);

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;
extern __IO uint16_t adc1Buf[];   // 3*ASDU_NUM
extern __IO uint16_t adc2Buf[];   // 3*ASDU_NUM
extern __IO uint16_t adc3Buf[];   // 4*ASDU_NUM
__attribute__((section(".non_cache"))) static __IO uint16_t calib1RawBuf[3 * ADC_CALIB_N_SAMPLES];
__attribute__((section(".non_cache"))) static __IO uint16_t calib2RawBuf[3 * ADC_CALIB_N_SAMPLES];
__attribute__((section(".non_cache"))) static __IO uint16_t calib3RawBuf[4 * ADC_CALIB_N_SAMPLES];

static volatile bool calib1CaptureActive = false;
static volatile bool calib1CaptureDone   = false;
static volatile bool calib2CaptureActive = false;
static volatile bool calib2CaptureDone   = false;
static volatile bool calib3CaptureActive = false;
static volatile bool calib3CaptureDone   = false;

volatile bool adcCalibBusy = false;

// CLI 등록.
bool octCalibInit(void)
{
#ifdef _USE_HW_CLI
  cliAdd("oct_calib", cliCalibOCT);
#endif
  return true;
}

// - adc.c의 HAL_ADC_ConvCpltCallback(ADC1/2/3분기)에서 가독성 위해 추출한 함수
// - 캘리브레이션 캡처가 방금 완료되어 처리했으면 true 반환
bool adcCalibNotifyConvCplt(ADC_HandleTypeDef *hadc)
{
  // - 캡처용 DMA도 정상 스트리밍과 동일한 Circular 모드 → 버퍼를 한 번 다 채운 뒤에도 하드웨어가 즉시 인덱스 0부터 계속 덮어씀
  // - 편법 도입 : 폴링 태스크가 osDelay(1) 주기로 뒤늦게 Stop_DMA 호출 시 그 사이 버퍼 앞부분 오염 가능 → 콜백(ISR) 컨텍스트에서 완료 감지 즉시 DMA 정지
  if (hadc->Instance == ADC1 && calib1CaptureActive == true)
  {
    HAL_ADC_Stop_DMA(hadc);
    calib1CaptureActive = false;
    calib1CaptureDone   = true;
    return true;
  }
  else if (hadc->Instance == ADC2 && calib2CaptureActive == true)
  {
    HAL_ADC_Stop_DMA(hadc);
    calib2CaptureActive = false;
    calib2CaptureDone   = true;
    return true;
  }
  else if (hadc->Instance == ADC3 && calib3CaptureActive == true)
  {
    HAL_ADC_Stop_DMA(hadc);
    calib3CaptureActive = false;
    calib3CaptureDone   = true;
    return true;
  }
  return false;
}

// - 공용 계산부: rawBuf에서 stride 간격으로 rank0(cos)/rank1(sin)/rank2(광파워) 읽어
//   x_n, y_n 정규화 → oct_operations.h 함수들로 ellipse fitting + 위상 추출 수행
// - ADC1/ADC2: stride=3, ADC3: stride=4(rank3(OVT)은 건너뜀)
static bool calibComputeParams(const __IO uint16_t *rawBuf, uint32_t stride, AdcCalibParam *out_param)
{
  Matrix x, y, params;
  bool success = false;

  // 계산 시작 시 항상 초기화 - 실패해도 "이전 성공 결과"가 최신 값처럼 오인되지 않도록
  out_param->a0    = 0.0f;
  out_param->a1    = 0.0f;
  out_param->b0    = 0.0f;
  out_param->b1    = 0.0f;
  out_param->eps   = 0.0f;
  out_param->valid = false;

  matrix_init(&x);
  matrix_init(&y);
  matrix_init(&params);

  // 1) 정규화된 x,y N개를 N x 1 Matrix(malloc)로 생성.
  if (matrix_create(&x, ADC_CALIB_N_SAMPLES, 1) == false ||
      matrix_create(&y, ADC_CALIB_N_SAMPLES, 1) == false)
  {
    goto cleanup;
  }
  for (uint32_t n = 0; n < ADC_CALIB_N_SAMPLES; n++)
  {
    // - rawBuf[n*stride + {0,1,2}](cos,sin,광파워)를 광파워로 정규화해 x_n, y_n 계산
    // - 이 정규화 로직은 여기 한 곳에서만 사용 → 별도 함수 대신 이 자리에 인라인
    float Ia0 = (float)rawBuf[n * stride + 0];
    float Ia1 = (float)rawBuf[n * stride + 1];
    float Ia2 = (float)rawBuf[n * stride + 2];

    float denom = Ia2 - ADC_CALIB_ZERO_CODE;
    if (fabsf(denom) < 1.0f)
    {
      denom = (denom < 0.0f) ? -1.0f : 1.0f;  // 0으로 나누기 방지 (광파워 입력 확인 필요 신호)
    }

    *matrix_value(&x, n, 0) = (Ia0 - ADC_CALIB_ZERO_CODE) / denom;
    *matrix_value(&y, n, 0) = (Ia1 - ADC_CALIB_ZERO_CODE) / denom;
  }

  // - 2) matrix_calibration()으로 타원 파라미터 도출
  // - 위상 오프셋은 여기서 계산 안 함(oct_realTimeCalculate.c가 실시간 계산 시점에 처리)
  if (matrix_calibration(&x, &y, &params) == false)
  {
    goto cleanup;
  }

  out_param->a0    = *matrix_const_value(&params, 0, 0);
  out_param->a1    = *matrix_const_value(&params, 1, 0);
  out_param->b0    = *matrix_const_value(&params, 2, 0);
  out_param->b1    = *matrix_const_value(&params, 3, 0);
  out_param->eps   = *matrix_const_value(&params, 4, 0);
  out_param->valid = true;
  success = true;

cleanup:
  matrix_destroy(&x);
  matrix_destroy(&y);
  matrix_destroy(&params);
  return success;
}

// - adcNum으로 요청받은 ADC가 하나뿐이어도 항상 ADC1/2/3 세 개 모두 캡처한다
//   (캡처 자체가 짧아서(~50ms) 굳이 선택적으로 하지 않아도 되고, 매번 세 채널
//   raw 데이터가 전부 최신 상태로 유지되는 부수 효과도 있음 - 어느 ADC의 결과를
//   실제로 쓸지는 호출자(adcCalib)가 계산 단계에서 고른다)
// - 세 ADC 모두 정지 → 거의 동시에 1회성 캡처 시작 → 셋 다 완료 시까지 대기
//   (공유 타임아웃 최대 1000ms) → 정상 스트리밍 복귀
static bool calibCapture(void)
{
  HAL_ADC_Stop_DMA(&hadc1);
  HAL_ADC_Stop_DMA(&hadc2);
  HAL_ADC_Stop_DMA(&hadc3);

  calib1CaptureDone = false;
  calib2CaptureDone = false;
  calib3CaptureDone = false;
  calib1CaptureActive = true;
  calib2CaptureActive = true;
  calib3CaptureActive = true;

  bool started1 = (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)calib1RawBuf, 3 * ADC_CALIB_N_SAMPLES) == HAL_OK);
  bool started2 = (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)calib2RawBuf, 3 * ADC_CALIB_N_SAMPLES) == HAL_OK);
  bool started3 = (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)calib3RawBuf, 4 * ADC_CALIB_N_SAMPLES) == HAL_OK);

  if (started1 == false) { calib1CaptureActive = false; }
  if (started2 == false) { calib2CaptureActive = false; }
  if (started3 == false) { calib3CaptureActive = false; }

  // 트리거 주기 약 65us * 768 ≈ 50ms - 여유있게 1000ms 타임아웃.
  uint32_t waited_ms = 0;
  while ((calib1CaptureDone == false || calib2CaptureDone == false || calib3CaptureDone == false)
         && waited_ms < 1000)
  {
    osDelay(1);
    waited_ms++;
  }

  HAL_ADC_Stop_DMA(&hadc1);
  HAL_ADC_Stop_DMA(&hadc2);
  HAL_ADC_Stop_DMA(&hadc3);

  // 정상 스트리밍 복귀 - 성공/실패 무관하게 항상 복구.
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1Buf, 3 * ASDU_NUM);
  HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2Buf, 3 * ASDU_NUM);
  HAL_ADC_Start_DMA(&hadc3, (uint32_t *)adc3Buf, 4 * ASDU_NUM);

  return started1 && started2 && started3
      && calib1CaptureDone && calib2CaptureDone && calib3CaptureDone;
}


// - adcCalib()(단일 또는 ALL) 종료 후 결과를 Flash에 자동 저장하기 위한 공용 헬퍼
// - Flash에 저장된 기존 AdcCalibParamSet을 먼저 읽어와, out1/out2/out3 중
//   valid==true인 항목만 해당 멤버에 덮어쓰고 다시 저장
// - valid==false(계산 실패)인 항목은 건드리지 않음 → 다른 ADC의 기존 저장값 보존
// - out1/out2/out3 중 사용하지 않는 자리는 NULL 전달 가능(adcCalib()의 단일 ADC 저장용)
// - 셋 다 valid==false면 Flash 재기록 없이 즉시 반환(불필요한 erase/write 방지)
static void persistCalibParam(const AdcCalibParam *out1, const AdcCalibParam *out2, const AdcCalibParam *out3)
{
  AdcCalibParamSet set;

  bool save1 = (out1 != NULL && out1->valid == true);
  bool save2 = (out2 != NULL && out2->valid == true);
  bool save3 = (out3 != NULL && out3->valid == true);

  if (save1 == false && save2 == false && save3 == false)
  {
    return;
  }

  // 실패해도(매직 불일치 등) set은 0으로 초기화된 상태로 반환되므로 그대로 사용 가능.
  adcCalib_Param_LoadFromFlash(&set);

  if (save1 == true) { set.adc1 = *out1; }
  if (save2 == true) { set.adc2 = *out2; }
  if (save3 == true) { set.adc3 = *out3; }

  adcCalib_Param_SaveToFlash(&set);
}

// - adcNum==ADC_CALIB_NUM_ALL: ADC1/2/3 동시 캘리브레이션(out1/out2/out3 모두 사용)
// - adcNum==ADC_CALIB_NUM_1/2/3: 대상 ADC만 다르고 나머지는 동일했던 세 함수를
//   switch-case 하나로 통합 - case별로 캡처 버퍼/stride/출력 슬롯(out1|out2|out3)/
//   그 ADC 자신의 captureDone 플래그만 선택
// - 어느 쪽이든 calibCapture()는 항상 ADC1/2/3 세 개를 모두 캡처한다(캡처가 짧아서
//   굳이 선택적으로 하지 않음) - 다만 단일 ADC 요청 시 계산/Flash 저장은 그 ADC
//   자신의 captureDone 플래그로만 게이팅해, 다른 두 ADC의 캡처 실패가 이 결과에
//   영향을 주지 않도록 함(예: "run 2"가 ADC1/3의 하드웨어 문제 때문에 실패로 잘못
//   보고되는 일이 없어야 함)
bool adcCalib(AdcCalibNum adcNum, AdcCalibParam *out1, AdcCalibParam *out2, AdcCalibParam *out3)
{
  if (adcNum == ADC_CALIB_NUM_ALL)
  {
    if (out1 == NULL || out2 == NULL || out3 == NULL)
    {
      return false;
    }
    out1->valid = false;
    out2->valid = false;
    out3->valid = false;

    // - 캡처 시작 전 true, 파라미터 계산 종료(성공/실패 무관) 후 false
    // - 이 구간 내내 adc.c는 이더넷으로 SV 패킷 미발행
    adcCalibBusy = true;

    bool ret = false;
    bool captureOk = calibCapture();

    if (captureOk == true)
    {
      bool r1 = calibComputeParams(calib1RawBuf, 3, out1);
      bool r2 = calibComputeParams(calib2RawBuf, 3, out2);
      // ADC3는 rank0,1,2(cos,sin,광파워)만 캘리브레이션 대상 - rank3(전압, ch8)은 건너뜀.
      bool r3 = calibComputeParams(calib3RawBuf, 4, out3);
      ret = r1 && r2 && r3;
    }

    adcCalibBusy = false;

    // calibCapture()는 항상 세 ADC 모두 캡처를 시도하므로 raw 데이터도 매번 저장
    // (원본 그대로 보존해 나중에 실패 원인을 오프라인으로 분석할 수 있도록).
    adcCalib_RawData_SaveToFlash();

    // 세 ADC 결과를 한 번에 Flash 자동 저장(1회 load + 1회 save) - 실패한
    // 항목(valid==false)은 persistCalibParam() 내부에서 건드리지 않고 보존됨.
    persistCalibParam(out1, out2, out3);

    return ret;
  }

  __IO uint16_t     *calibBuf      = NULL;
  uint32_t           stride        = 0;
  AdcCalibParam     *out_param     = NULL;
  volatile bool     *captureDone   = NULL;

  switch (adcNum)
  {
    case ADC_CALIB_NUM_1:
      calibBuf    = calib1RawBuf;
      stride      = 3;
      out_param   = out1;
      captureDone = &calib1CaptureDone;
      break;

    case ADC_CALIB_NUM_2:
      calibBuf    = calib2RawBuf;
      stride      = 3;
      out_param   = out2;
      captureDone = &calib2CaptureDone;
      break;

    case ADC_CALIB_NUM_3:
      // - ADC3는 rank0,1,2(cos,sin,광파워)만 캘리브레이션 대상
      // - rank3(전압, ch8)은 stride=4 덕분에 자동 건너뜀
      calibBuf    = calib3RawBuf;
      stride      = 4;
      out_param   = out3;
      captureDone = &calib3CaptureDone;
      break;

    default:
      return false;
  }

  if (out_param == NULL)
  {
    return false;
  }
  out_param->valid = false;

  // - 캡처 시작 전 true, 파라미터 계산 종료(성공/실패 무관) 후 false
  // - 이 구간 내내 adc.c는 이더넷으로 SV 패킷 미발행
  adcCalibBusy = true;

  // calibCapture()는 요청받은 ADC 하나만이 아니라 항상 ADC1/2/3 세 개를 모두 캡처한다.
  calibCapture();

  // 이 ADC 자신의 캡처 성공 여부만으로 계산 진행 여부를 결정(다른 ADC의 캡처
  // 실패는 이 결과에 영향 없음).
  bool ret = false;
  if (*captureDone == true)
  {
    ret = calibComputeParams(calibBuf, stride, out_param);
  }

  adcCalibBusy = false;

  // calibCapture()는 항상 세 ADC 모두 캡처를 시도하므로 raw 데이터도 매번 저장
  // (원본 그대로 보존해 나중에 실패 원인을 오프라인으로 분석할 수 있도록).
  adcCalib_RawData_SaveToFlash();

  // 성공 시(out_param->valid == true) 결과를 Flash에 자동 저장 - 나머지 두 ADC의
  // 기존 저장값은 persistCalibParam() 내부에서 보존됨.
  switch (adcNum)
  {
    case ADC_CALIB_NUM_1: persistCalibParam(out_param, NULL, NULL); break;
    case ADC_CALIB_NUM_2: persistCalibParam(NULL, out_param, NULL); break;
    case ADC_CALIB_NUM_3: persistCalibParam(NULL, NULL, out_param); break;
    default: break;
  }

  return ret;
}

//--------------------------------------------------------------------------
// Flash 저장/로드 (모두 static - 이 파일 내부에서만 사용, 외부 호출 없음)
//--------------------------------------------------------------------------

// - 3개 ADC 결과를 한 번에 내부 Flash(ADC_CALIB_FLASH_ADDR)에 저장
// - 직접 호출할 필요는 보통 없음: adcCalib()이 성공한 결과를 persistCalibParam()을
//   통해 자동으로 저장/병합해 줌
static bool adcCalib_Param_SaveToFlash(const AdcCalibParamSet *set)
{
  if (set == NULL)
  {
    return false;
  }

  AdcCalibParamSet toSave = *set;
  toSave.magic = ADC_CALIB_FLASH_MAGIC;

  // 대상 섹터(128KB) 전체 지운 뒤 기록.
  if (flashErase(ADC_CALIB_FLASH_ADDR, sizeof(toSave)) == false)
  {
    return false;
  }

  return flashWrite(ADC_CALIB_FLASH_ADDR, (uint8_t *)&toSave, sizeof(toSave));
}

// - Flash에서 3개 ADC 결과 묶음을 읽어옴
// - CLI의 "get_parameters"는 s_cliCalibParam[](마지막 run 결과 캐시)를 조회하므로
//   이 함수를 직접 쓰지 않음 - persistCalibParam()이 병합 저장 전 기존값을 읽을 때 사용
static bool adcCalib_Param_LoadFromFlash(AdcCalibParamSet *set)
{
  if (set == NULL)
  {
    return false;
  }

  if (flashRead(ADC_CALIB_FLASH_ADDR, (uint8_t *)set, sizeof(*set)) == false)
  {
    return false;
  }

  if (set->magic != ADC_CALIB_FLASH_MAGIC)
  {
    // 아직 저장된 적이 없거나(전부 0xFF) 손상된 데이터
    memset(set, 0, sizeof(*set));
    return false;
  }

  return true;
}

// - 마지막으로 캡처된 raw 데이터(calib1/2/3RawBuf)를 그대로 별도 섹터에 저장
// - adcCalib()(단일 또는 ALL)로 캡처를 먼저 채운 뒤 호출해야 의미 있음
// - 직접 호출할 필요는 보통 없음: adcCalib()이 캡처 성공 시(계산 성공/실패 무관)
//   자동으로 호출해 줌. CLI로는 별도 수동 저장 명령 없음(get_data로 조회만 가능)
static bool adcCalib_RawData_SaveToFlash(void)
{
  uint32_t magic = ADC_CALIB_RAW_FLASH_MAGIC;

  if (flashErase(ADC_CALIB_RAW_FLASH_ADDR, ADC_CALIB_RAW_TOTAL_SIZE) == false)
  {
    return false;
  }

  if (flashWrite(ADC_CALIB_RAW_FLASH_ADDR, (uint8_t *)&magic, sizeof(magic)) == false)
  {
    return false;
  }

  if (flashWrite(ADC_CALIB_RAW_FLASH_ADDR + ADC_CALIB_RAW_ADC1_OFFSET,
                  (uint8_t *)calib1RawBuf, ADC_CALIB_RAW_ADC1_SIZE) == false)
  {
    return false;
  }

  if (flashWrite(ADC_CALIB_RAW_FLASH_ADDR + ADC_CALIB_RAW_ADC2_OFFSET,
                  (uint8_t *)calib2RawBuf, ADC_CALIB_RAW_ADC2_SIZE) == false)
  {
    return false;
  }

  if (flashWrite(ADC_CALIB_RAW_FLASH_ADDR + ADC_CALIB_RAW_ADC3_OFFSET,
                  (uint8_t *)calib3RawBuf, ADC_CALIB_RAW_ADC3_SIZE) == false)
  {
    return false;
  }

  return true;
}

// - Flash 저장 raw 데이터를 calib1/2/3RawBuf(모듈 내부 정적 버퍼)로 재적재
// - 성공 시 CLI "get_data" 등에서 그 버퍼들을 그대로 출력 가능
static bool adcCalib_RawData_LoadFromFlash(void)
{
  uint32_t magic = 0;

  if (flashRead(ADC_CALIB_RAW_FLASH_ADDR, (uint8_t *)&magic, sizeof(magic)) == false)
  {
    return false;
  }

  if (magic != ADC_CALIB_RAW_FLASH_MAGIC)
  {
    // 아직 저장된 적이 없거나(전부 0xFF) 손상된 데이터
    return false;
  }

  if (flashRead(ADC_CALIB_RAW_FLASH_ADDR + ADC_CALIB_RAW_ADC1_OFFSET,
                (uint8_t *)calib1RawBuf, ADC_CALIB_RAW_ADC1_SIZE) == false)
  {
    return false;
  }

  if (flashRead(ADC_CALIB_RAW_FLASH_ADDR + ADC_CALIB_RAW_ADC2_OFFSET,
                (uint8_t *)calib2RawBuf, ADC_CALIB_RAW_ADC2_SIZE) == false)
  {
    return false;
  }

  if (flashRead(ADC_CALIB_RAW_FLASH_ADDR + ADC_CALIB_RAW_ADC3_OFFSET,
                (uint8_t *)calib3RawBuf, ADC_CALIB_RAW_ADC3_SIZE) == false)
  {
    return false;
  }

  return true;
}




#ifdef _USE_HW_CLI

// - run 명령 결과를 get_parameters로 재조회할 수 있도록 캐싱
// - index 0=ADC1, 1=ADC2, 2=ADC3
static AdcCalibParam s_cliCalibParam[3];

static void cliPrint_CalibParam(int32_t adcNum, const AdcCalibParam *p)
{
  cliPrintf("ADC%d calib(%s) : a0=%f a1=%f b0=%f b1=%f eps=%f\n",
            (int)adcNum, p->valid ? "OK" : "INVALID",
            p->a0, p->a1, p->b0, p->b1, p->eps);
}

// - raw 캡처 버퍼 하나(예: calib1RawBuf, stride=3, chIdx=0 => "adc1-1") 출력
// - 형식: "label\ntime value\n" 헤더 후 N_SAMPLES줄의 "time raw_value"
// - 시간값 t_n = n / (SIGNAL_FREQ * POINTS_PER_PERIOD) [s](약 65.1us 간격)
static void cliPrint_RawData(const char *label, const __IO uint16_t *buf,
                                uint32_t stride, uint32_t chIdx)
{
  const float dt = 1.0f / ((float)SIGNAL_FREQ * (float)POINTS_PER_PERIOD);

  cliPrintf("%s\n", label);
  cliPrintf("time value\n");

  for (uint32_t n = 0; n < ADC_CALIB_N_SAMPLES; n++)
  {
    cliPrintf("%f %f\n", dt * (float)n, (float)buf[n * stride + chIdx]);

    // 7680줄 가까이 UART 출력 중 다른 태스크에 실행 기회 부여.
    if ((n % 64) == 63)
    {
      osDelay(0);
    }
  }
}

void cliCalibOCT(cli_args_t *args)
{
  bool ret = false;

  // "all"인지 먼저 문자열로 검사 - args->getData()는 "all" 같은 비숫자 문자열을
  // 숫자로 못 바꾸고 0을 반환하므로, 숫자 파싱보다 반드시 먼저 검사해야 한다.
  if (args->argc == 2 && args->isStr(0, "run") == true)
  {
    if (args->isStr(1, "all") == true)
    {
      bool calibRet = adcCalib(ADC_CALIB_NUM_ALL, &s_cliCalibParam[0], &s_cliCalibParam[1], &s_cliCalibParam[2]);

      cliPrintf("ADC1/2/3 calibration(run all) %s\n", calibRet ? "OK" : "FAIL");
      cliPrint_CalibParam(1, &s_cliCalibParam[0]);
      cliPrint_CalibParam(2, &s_cliCalibParam[1]);
      cliPrint_CalibParam(3, &s_cliCalibParam[2]);
    }
    else
    {
      int32_t adcNum = args->getData(1);
      bool    calibRet;

      switch (adcNum)
      {
        case ADC_CALIB_NUM_1: calibRet = adcCalib(ADC_CALIB_NUM_1, &s_cliCalibParam[0], NULL, NULL); break;
        case ADC_CALIB_NUM_2: calibRet = adcCalib(ADC_CALIB_NUM_2, NULL, &s_cliCalibParam[1], NULL); break;
        case ADC_CALIB_NUM_3: calibRet = adcCalib(ADC_CALIB_NUM_3, NULL, NULL, &s_cliCalibParam[2]); break;

        default:
          cliPrintf("invalid adc number (1~3|all)\n");
          return;
      }

      cliPrintf("ADC%d calibration %s\n", (int)adcNum, calibRet ? "OK" : "FAIL");
    }
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "get_parameters") == true)
  {
    if (args->isStr(1, "all") == true)
    {
      cliPrint_CalibParam(1, &s_cliCalibParam[0]);
      cliPrint_CalibParam(2, &s_cliCalibParam[1]);
      cliPrint_CalibParam(3, &s_cliCalibParam[2]);
    }
    else
    {
      int32_t adcNum = args->getData(1);

      switch (adcNum)
      {
        case ADC_CALIB_NUM_1:
        case ADC_CALIB_NUM_2:
        case ADC_CALIB_NUM_3:
          cliPrint_CalibParam(adcNum, &s_cliCalibParam[adcNum - 1]);
          break;

        default:
          cliPrintf("invalid adc number (1~3|all)\n");
          return;
      }
    }
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "get_data") == true)
  {
    bool loadRet = adcCalib_RawData_LoadFromFlash();

    cliPrintf("load raw data(0x%X) %s\n",
              (unsigned int)ADC_CALIB_RAW_FLASH_ADDR, loadRet ? "OK" : "FAIL");

    if (loadRet == true)
    {
      if (args->isStr(1, "all") == true)
      {
        cliPrint_RawData("adc1-1", calib1RawBuf, 3, 0);
        cliPrint_RawData("adc1-2", calib1RawBuf, 3, 1);
        cliPrint_RawData("adc1-3", calib1RawBuf, 3, 2);

        cliPrint_RawData("adc2-1", calib2RawBuf, 3, 0);
        cliPrint_RawData("adc2-2", calib2RawBuf, 3, 1);
        cliPrint_RawData("adc2-3", calib2RawBuf, 3, 2);

        cliPrint_RawData("adc3-1", calib3RawBuf, 4, 0);
        cliPrint_RawData("adc3-2", calib3RawBuf, 4, 1);
        cliPrint_RawData("adc3-3", calib3RawBuf, 4, 2);
        // adc3-4(rank3, ch8 전압/OVT)는 캘리브레이션 대상이 아니라 출력하지 않음.
      }
      else
      {
        int32_t adcNum = args->getData(1);

        switch (adcNum)
        {
          case ADC_CALIB_NUM_1:
            cliPrint_RawData("adc1-1", calib1RawBuf, 3, 0);
            cliPrint_RawData("adc1-2", calib1RawBuf, 3, 1);
            cliPrint_RawData("adc1-3", calib1RawBuf, 3, 2);
            break;

          case ADC_CALIB_NUM_2:
            cliPrint_RawData("adc2-1", calib2RawBuf, 3, 0);
            cliPrint_RawData("adc2-2", calib2RawBuf, 3, 1);
            cliPrint_RawData("adc2-3", calib2RawBuf, 3, 2);
            break;

          case ADC_CALIB_NUM_3:
            cliPrint_RawData("adc3-1", calib3RawBuf, 4, 0);
            cliPrint_RawData("adc3-2", calib3RawBuf, 4, 1);
            cliPrint_RawData("adc3-3", calib3RawBuf, 4, 2);
            // adc3-4(rank3, ch8 전압/OVT)는 캘리브레이션 대상이 아니라 출력하지 않음.
            break;

          default:
            cliPrintf("invalid adc number (1~3|all)\n");
            return;
        }
      }
    }
    ret = true;
  }

  if (args->argc == 3 && args->isStr(0, "scale") == true && args->isStr(1, "set") == true)
  {
    float scale   = args->getFloat(2);
    bool  saveRet = octRealTimeSetScale(scale);

    cliPrintf("oct_calib scale set %f %s\n", scale, saveRet ? "OK" : "FAIL");
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "scale") == true && args->isStr(1, "get") == true)
  {
    cliPrintf("oct_calib scale = %f\n", octRealTimeGetScale());
    ret = true;
  }

  if (ret != true)
  {
    cliPrintf("oct_calib run <1|2|3|all>            : ADC 캘리브레이션 실행(개별 또는 전체 동시). 결과는 flash에 자동 저장\n");
    cliPrintf("oct_calib get_parameters <1|2|3|all> : 마지막 실행 결과(a0,a1,b0,b1,eps) 조회\n");
    cliPrintf("oct_calib get_data <1|2|3|all>       : flash의 raw 데이터를 불러와 채널별(개별 또는 전체) time/value로 출력\n");
    cliPrintf("oct_calib scale set <value>          : 실시간 전류 계산용 scale factor(alpha, 단위 A/rad) 설정 후 flash에 저장\n");
    cliPrintf("oct_calib scale get                  : 현재 scale factor 조회\n");
  }
}

#endif  // _USE_HW_CLI

#endif  // _USE_HW_ADC
