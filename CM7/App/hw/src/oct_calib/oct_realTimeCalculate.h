/*
 * oct_realTimeCalculate.h
 *
 * - oct_calib.c에서 얻은 캘리브레이션 결과(a0,a1,b0,b1,eps)를 이용해, 스트리밍 중인
 *   ADC 샘플 하나하나에 대해 실시간으로 Heydemann 보정 위상을 뽑고 scale
 *   factor(alpha)를 곱해 물리량(전류, 예: I_A)으로 변환하는 모듈
 *
 *   X = (x-a0)/a1, Y = (y-b0)/b1
 *   Ycor = (Y - X*sin(eps)) / cos(eps)
 *   phi  = atan2(Ycor, X)
 *   I_A  = alpha * (phi - phi0)
 *
 * - scale factor(alpha, 단위: A/rad): 컴파일 타임 상수 아님
 *   - CLI("oct_calib scale", oct_calib.c의 cliCalibOCT()가 처리)로 UART 통해 입력
 *   - 내부 Flash(섹터5, 0x080A0000)에 저장, 모듈 초기화 시 재적재
 *   - 사유: 표준 전류원 등으로 실측해 맞추는 값 → 재컴파일 불필요하도록
 *
 * - 위상 오프셋(phi0): 이 모듈이 직접 계산하지 않음
 *   - octRealTimeComputeCurrent()의 입력 파라미터로만 받음, 기본값 0.0f(보정 없음)
 *   - phi0 산출 방식(예: N개 위상을 큐에 모아 평균, CLI 수동 입력, 기타 추정 등)은
 *     호출하는 쪽(다른 모듈)이 자유롭게 결정해서 계산 후 전달
 */

#ifndef SRC_HW_INCLUDE_OCT_REALTIMECALCULATE_H_
#define SRC_HW_INCLUDE_OCT_REALTIMECALCULATE_H_

#include "hw_def.h"
#include "oct_calib.h"   // AdcCalibParam, ADC_CALIB_ZERO_CODE

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _USE_HW_ADC

// scale factor(alpha) 기본값 - 내부 Flash에 저장된 값 없을 때(한 번도
// "oct_calib scale set" 안 했을 때)만 사용.
#define OCT_RT_SCALE_DEFAULT       1.0f

// - scale factor 저장용 내부 Flash 위치
// - oct_calib.h의 ADC_CALIB_FLASH_ADDR(섹터7, 0x080E0000)/
//   ADC_CALIB_RAW_FLASH_ADDR(섹터6, 0x080C0000)와 겹치지 않는 빈 섹터5 사용
#define OCT_RT_SCALE_FLASH_ADDR    0x080A0000
#define OCT_RT_SCALE_FLASH_MAGIC   0x5343414Cu  // "SCAL"

// - 모듈 초기화 - 내부 Flash 저장 scale factor 읽어와 캐싱(없으면 OCT_RT_SCALE_DEFAULT)
// - CLI 명령("oct_calib scale set/get")은 이 파일이 아니라 oct_calib.c의
//   cliCalibOCT()가 등록/처리 → 이 함수는 CLI 별도 등록 안 함
// - hwInit()에서 octCalibInit() 이후 호출
bool octRealTimeInit(void);

// 현재 캐싱된 scale factor(alpha) 반환.
float octRealTimeGetScale(void);

// scale factor(alpha)를 RAM에 반영하고 즉시 내부 Flash(섹터5)에 저장.
bool octRealTimeSetScale(float scale);

// phaseOffset(phi0) 인자 기본값 상수 - "보정 없음" 의미.
#define OCT_RT_PHASE_OFFSET_DEFAULT   0.0f

// - ADC 원시 코드값 3개(Ia0=cos, Ia1=sin, Ia2=광파워), 캘리브레이션 결과(param),
//   위상 오프셋(phaseOffset, phi0)으로부터 Heydemann 보정 위상 추출 →
//   I_A = scale * (phase - phaseOffset) 계산
// - phaseOffset은 이 함수가 계산하지 않고 호출자가 넘긴 값 그대로 사용
//   - 별도로 구한 값 없으면 OCT_RT_PHASE_OFFSET_DEFAULT(0.0f) 전달
// - 0.0f 반환 조건: param이 NULL/invalid, 또는 위상 추출 불가(a1/b1/cos(eps)가 0에 가까움)
float octRealTimeComputeCurrent(const AdcCalibParam *param, uint16_t Ia0, uint16_t Ia1, uint16_t Ia2,
                                 float phaseOffset);

#endif  // _USE_HW_ADC

#ifdef __cplusplus
}
#endif

#endif /* SRC_HW_INCLUDE_OCT_REALTIMECALCULATE_H_ */
