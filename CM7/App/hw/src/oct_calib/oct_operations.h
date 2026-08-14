/*
 * oct_operations.h
 *
 * - Heydemann ellipse-fit 캘리브레이션의 도메인 연산
 *   (matrix_sqr, matrix_vandermonde, matrix_least_squares, matrix_calibration)
 * - oct_matrix.h의 Matrix(malloc/free 기반, float)를 입출력으로 쓰는 배치(batch) 함수들
 *
 * - matrix_extract_phase/matrix_meter_current의 batch 버전(N개 샘플 일괄 처리)도
 *   한때 존재했으나 실호출자 없어 제거
 *   - 사유: oct_realTimeCalculate.c는 ADC 샘플 1개마다 실시간 위상/전류 계산 필요
 *     → 매 샘플 malloc/free 비용·힙 단편화 회피 필요 → batch 버전은 용도 부적합
 *   - 대안: 샘플 1개를 받는 스칼라 버전(matrix_extract_phase/matrix_meter_current)으로 충분
 *
 * - 이 파일의 함수: 광파워 정규화 등 OCT 도메인 지식 없는 순수 계산 함수
 *   - 전처리는 oct_calib.c 담당
 *
 * - 배치(batch) 함수(Matrix 주고받는 것들): 호출마다 malloc/calloc으로 새 버퍼 생성
 *   - 호출자는 사용 후 반드시 matrix_destroy()로 해제(누락 시 힙 누수)
 */

#ifndef APP_HW_SRC_OCT_CALIB_OCT_OPERATIONS_H_
#define APP_HW_SRC_OCT_CALIB_OCT_OPERATIONS_H_

#include <stdbool.h>

#include "oct_matrix.h"

#ifdef __cplusplus
extern "C" {
#endif

// matrix의 모든 원소 제곱(제자리 연산).
bool matrix_sqr(Matrix *matrix);

// - x, y(둘 다 N x 1 열벡터, N>=1, 크기 동일 필요)로부터 N x 5 Vandermonde 행렬 v 생성(malloc)
//   v[row] = [y_row^2, x_row*y_row, x_row, y_row, 1]
// - 실패 시 v 미변경(=matrix_init 상태)
bool matrix_vandermonde(const Matrix *x, const Matrix *y, Matrix *v);

// - a(N x M), b(N개짜리 행/열 벡터)로 최소자승 문제 a*x=b 풀이
//   - 방식: 정규방정식(A^T A x = A^T b), 부분피벗 가우스 소거, FLT_EPSILON 기준 특이행렬 판정
//   - 결과: x(M x 1, 새로 malloc)
// - a의 열 개수(M)에 대해 완전 일반 동작(5 고정 아님)
bool matrix_least_squares(const Matrix *a, const Matrix *b, Matrix *x);

// - x, y(N x 1, N>=5)로부터 Vandermonde 행렬과 -x^2 생성 → 최소자승법으로 [B,C,D,F,G] 도출
// - 상대허용오차 기반 특이/퇴화 판정 통과 시 Heydemann 타원 파라미터(a0,a1,b0,b1,eps)를
//   params(5 x 1, 새로 malloc)에 순서대로 저장
// - 실패 시 params 미변경(=matrix_init 상태)
bool matrix_calibration(const Matrix *x, const Matrix *y, Matrix *params);

// - 타원 파라미터(a0,a1,b0,b1,eps)와 정규화 샘플 (x,y) 1개로 Heydemann 보정 위상 계산
// - 매 ADC 샘플마다 malloc 없이 쓰는 실시간 처리 전용 함수
// - a1/b1/cos(eps)가 0에 가까우면 false 반환
bool matrix_extract_phase(float a0, float a1, float b0, float b1, float eps,
                         float x, float y, float *out_phase);

// - 위상에서 phaseOffset을 뺀 뒤 scale을 곱해 물리량(전류 등)으로 변환
// - phaseOffset은 이 함수가 계산하지 않고 호출자가 넘긴 값 그대로 사용
// - 실시간 처리를 위한 스칼라 버전
float matrix_meter_current(float phase, float phaseOffset, float scale);

#ifdef __cplusplus
}
#endif

#endif /* APP_HW_SRC_OCT_CALIB_OCT_OPERATIONS_H_ */
