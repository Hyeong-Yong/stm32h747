/*
 * oct_realTimeCalculate.c
 *
 * - oct_calib.c가 구한 캘리브레이션 결과(a0,a1,b0,b1,eps)로 스트리밍 중인 ADC 샘플
 *   하나하나에 대해 실시간 Heydemann 보정 위상 계산 → scale factor(alpha)를 곱해
 *   전류(I_A 등) 산출
 * - 캘리브레이션(oct_calib.c), 계산 알고리즘(oct_operations.c/oct_matrix.c)과
 *   완전히 분리된 파일
 *
 * - 위상 오프셋(phi0): 이 파일이 직접 계산하지 않음
 *   - octRealTimeComputeCurrent()의 phaseOffset 입력 파라미터로만 받아
 *     I_A = alpha * (phi - phi0) 계산
 *   - 기본값 0.0f(보정 없음, OCT_RT_PHASE_OFFSET_DEFAULT), phi0 산출 방식은
 *     호출하는 쪽이 자유롭게 결정
 *
 * - scale factor(alpha): 컴파일 타임 상수 아님
 *   - CLI("oct_calib scale set <value>", oct_calib.c의 cliCalibOCT()가 처리)로
 *     UART 통해 입력
 *   - 내부 Flash(섹터5, 0x080A0000)에 저장, 모듈 초기화 시 재적재
 */

#include "oct_realTimeCalculate.h"

#ifdef _USE_HW_ADC

#include "oct_operations.h"   // matrix_extract_phase / matrix_meter_current
#include "flash.h"

#include <math.h>

// - 내부 Flash에 그대로 저장/로드하는 레코드
// - oct_calib.c의 AdcCalibParamSet과 동일 패턴(매직 + 페이로드)
// - 매직 불일치(미저장 또는 손상) 시 OCT_RT_SCALE_DEFAULT 사용
typedef struct
{
  uint32_t magic;
  float    scale;
} OctRealTimeScaleRecord;

static float s_scale = OCT_RT_SCALE_DEFAULT;

// - raw 코드값 3개(Ia0=cos, Ia1=sin, Ia2=광파워)를 광파워로 정규화해 x, y 계산
// - oct_calib.c의 calibComputeParams() 내부 정규화 로직과 계산 내용 동일
// - 차이점: 그쪽은 캡처 버퍼(rawBuf[n*stride+ch])에서 N개 순회, 여기는 실시간으로
//   매 샘플 3개 코드값을 그대로 인자로 받음
static inline void rtNormalizeSample(uint16_t Ia0, uint16_t Ia1, uint16_t Ia2,
                                      float *out_x, float *out_y)
{
  float ia0 = (float)Ia0;
  float ia1 = (float)Ia1;
  float ia2 = (float)Ia2;

  float denom = ia2 - ADC_CALIB_ZERO_CODE;
  if (fabsf(denom) < 1.0f)
  {
    denom = (denom < 0.0f) ? -1.0f : 1.0f;  // 0으로 나누기 방지 (광파워 입력 확인 필요 신호)
  }

  *out_x = (ia0 - ADC_CALIB_ZERO_CODE) / denom;
  *out_y = (ia1 - ADC_CALIB_ZERO_CODE) / denom;
}

bool octRealTimeInit(void)
{
  OctRealTimeScaleRecord record;

  if (flashRead(OCT_RT_SCALE_FLASH_ADDR, (uint8_t *)&record, sizeof(record)) == true &&
      record.magic == OCT_RT_SCALE_FLASH_MAGIC)
  {
    s_scale = record.scale;
  }
  else
  {
    s_scale = OCT_RT_SCALE_DEFAULT;
  }

  return true;
}

float octRealTimeGetScale(void)
{
  return s_scale;
}

bool octRealTimeSetScale(float scale)
{
  OctRealTimeScaleRecord record;

  // - CLI("oct_calib scale set")에 "nan"/"inf" 같은 문자열 입력 시 strtof()가
  //   그대로 NaN/Infinity 반환
  // - 미검증 시 손상된 scale factor가 Flash에 저장되어 이후 실시간 전류 계산 전체가
  //   조용히 깨짐 → 여기서 차단
  if (isfinite(scale) == 0)
  {
    return false;
  }

  record.magic = OCT_RT_SCALE_FLASH_MAGIC;
  record.scale = scale;

  // 섹터(128KB) 전체 지운 뒤 기록(oct_calib.c의 adcCalib_Param_SaveToFlash와 동일 방식).
  if (flashErase(OCT_RT_SCALE_FLASH_ADDR, sizeof(record)) == false)
  {
    return false;
  }
  if (flashWrite(OCT_RT_SCALE_FLASH_ADDR, (uint8_t *)&record, sizeof(record)) == false)
  {
    return false;
  }

  s_scale = scale;
  return true;
}

float octRealTimeComputeCurrent(const AdcCalibParam *param, uint16_t Ia0, uint16_t Ia1, uint16_t Ia2,
                                 float phaseOffset)
{
  float x, y, phase;

  if (param == NULL || param->valid == false)
  {
    return 0.0f;
  }

  rtNormalizeSample(Ia0, Ia1, Ia2, &x, &y);

  if (matrix_extract_phase(param->a0, param->a1, param->b0, param->b1, param->eps, x, y, &phase) == false)
  {
    return 0.0f;
  }

  // - phaseOffset은 이 함수가 계산하지 않고 호출자가 넘긴 값 그대로 사용
  // - I_A = scale * (phase - phaseOffset)
  return matrix_meter_current(phase, phaseOffset, s_scale);
}

#endif  // _USE_HW_ADC
