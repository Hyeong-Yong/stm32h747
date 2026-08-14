/*
 * oct_matrix.h
 *
 * - rows, columns, values(1차원 float 버퍼)로 이루어진 범용 행렬 ADT
 * - double 대신 float 사용(STM32 펌웨어 환경, "double 금지" 원칙) 외에는
 *   표준적인 malloc/calloc/free 기반 가변 크기 행렬 구현
 * - rows*columns 크기가 매 호출마다 달라질 수 있다는 전제로 설계
 * - 이 프로젝트 사용 패턴: 캘리브레이션 1회 실행(ADC1/2/3 각각, N=768 고정)마다
 *   Matrix를 몇 개 생성 → 계산 종료 즉시 matrix_destroy()로 해제 필수
 *   (해제 누락 시 실행할 때마다 힙 누적 → 결국 malloc 실패)
 */

#ifndef APP_HW_SRC_OCT_CALIB_OCT_MATRIX_H_
#define APP_HW_SRC_OCT_CALIB_OCT_MATRIX_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  size_t rows;
  size_t columns;
  float *values;
} Matrix;

// - rows=columns=0, values=NULL로 초기화만(할당 없음)
// - Matrix 선언 직후 항상 제일 먼저 호출 필수
//   (그래야 create 이전 상태에서 matrix_destroy() 호출해도 안전)
void matrix_init(Matrix *matrix);

// - values를 free() 후 matrix_init() 상태로 복귀
// - values==NULL(미생성 또는 이미 destroy됨)이어도 free(NULL) 안전 → 그냥 호출 가능
void matrix_destroy(Matrix *matrix);

// - rows*columns개의 float을 calloc(0 초기화)
// - false 조건: rows==0, columns==0, rows*columns 오버플로, malloc 실패
bool matrix_create(Matrix *matrix, size_t rows, size_t columns);

// - source 값을 destination으로 복사(새로 할당)
// - source==destination: 아무 동작 없이 true
// - 그 외: destination 먼저 destroy → 새로 create → 복사
bool matrix_copy(const Matrix *source, Matrix *destination);

// - row-major 인덱싱(values[row*columns+column])으로 원소 포인터 반환
// - 범위 체크 없음 - 호출자가 rows/columns 준수 책임
float *matrix_value(Matrix *matrix, size_t row, size_t column);
const float *matrix_const_value(const Matrix *matrix, size_t row, size_t column);

// - rows*columns(총 원소 개수) 반환
size_t matrix_element_count(const Matrix *matrix);

#ifdef __cplusplus
}
#endif

#endif /* APP_HW_SRC_OCT_CALIB_OCT_MATRIX_H_ */
