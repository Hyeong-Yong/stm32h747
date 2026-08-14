## 프로젝트 구조

```text
calc/
├── CMakeLists.txt
├── include/
│   ├── csv.h
│   ├── matrix.h
│   └── operations.h
└── src/
    ├── csv.c
    ├── main.c
    ├── matrix.c
    └── operations.c
```

각 모듈의 역할은 다음과 같습니다.

- `matrix`: 행렬 메모리, 인덱싱과 복사
- `csv`: CSV 파일 읽기와 쓰기
- `operations`: 행렬 복사·제곱, Vandermonde 행렬, 덧셈과 최소제곱 계산
- `main`: 명령행 인자 처리

## CMake 빌드

Visual Studio Code에서 CMake Tools를 사용하거나 다음 명령을 실행합니다.

```cmd
cmake -S . -B build
cmake --build build --config Debug
```

Windows 멀티 구성 생성기에서는 실행 파일이 보통 `build\Debug\calc.exe`에 생성됩니다. 단일 구성 생성기에서는 `build\calc.exe`에 생성될 수 있습니다.

## 입력 파일 형식

한 줄이 한 행이며, 값은 쉼표로 구분합니다. 쉼표 주변 공백은 허용됩니다.

```text
1, 2, 3, 4
5, 6, 7, 8
```

## Command-line operations

```cmd
test.exe add A.txt B.txt C.txt
test.exe copy A.txt B.txt
test.exe sqr A.txt B.txt
test.exe vandermonde x.txt y.txt V.txt
test.exe calibration x.txt y.txt params.txt
test.exe extract_phase x.txt y.txt params.txt phase.txt
test.exe meter_current phase.txt 17000 current.txt
```
`add`는 `A + B`의 결과를 `C.txt`에 저장됩니다.

`copy`는 `A.txt`의 행렬을 `B.txt`에 복사합니다. `sqr`는 입력 행렬의 각 원소를 제곱해 출력합니다. `vandermonde`는 `x.txt`, `y.txt`의 `N x 1` 열 벡터로 `[y^2, xy, x, y, 1]` 형태의 `N x 5` 행렬을 생성합니다.

`calibration`은 `x.txt`, `y.txt`의 `N x 1` 열 벡터를 Heydemann Correction으로 보정해 `[a0, a1, b0, b1, phi0]` 형태의 `5 x 1` `params` 행렬을 `params.txt`에 저장합니다. 입력 데이터는 최소 5개 샘플이 필요합니다.

`extract_phase`는 `x.txt`, `y.txt`, `params.txt`를 읽어 보정된 위상을 `phase.txt`에 `N x 1` 행렬로 저장합니다.

`meter_current`는 `phase.txt`의 평균을 `phase_offset`으로 계산해 CLI에 출력하고, `(phase - phase_offset) * scale`을 `current.txt`에 저장합니다. `scale`은 행렬 파일이 아닌 일반 실수 인자이며, 예시 값은 `17000 A/rad`입니다.

## Operations API

행렬 기본 함수는 `include/matrix.h`, 계산 함수는 `include/operations.h`에 있습니다. 출력 행렬은 호출 전에 `matrix_init()`으로 초기화해야 합니다.

```c
Matrix x;
Matrix y;
Matrix v;
matrix_init(&x);
matrix_init(&y);
matrix_init(&v);

matrix_vandermonde(&x, &y, &v);
```

- `matrix_copy(A, B)`: `A`와 같은 크기의 행렬을 새로 할당해 `B`에 복사합니다. `B`가 기존 행렬이면 기존 메모리를 해제합니다. 이 함수는 `matrix.h`에 있습니다.
- `matrix_sqr(x)`: `x`를 in-place로 제곱해 `x[i][j] = x[i][j] * x[i][j]`로 변경합니다.
- `matrix_vandermonde(x, y, V)`: `x`, `y`가 같은 크기의 `N x 1` 열 벡터일 때 `N x 5` 행렬 `[y^2, xy, x, y, 1]`을 생성합니다.

함수 사용이 끝난 뒤에는 모든 행렬에 `matrix_destroy()`를 호출합니다.

`A*x = b`를 최소제곱 방식으로 풉니다.

```cmd
test.exe lsm A.txt b.txt x.txt
```

`A`가 `m x n` 행렬이면 `b`는 `m`개 값을 가진 `m x 1` 또는 `1 x m` 벡터여야 합니다. 출력 `x.txt`는 `n x 1` 열 벡터입니다. 계산에는 `A^T A x = A^T b` 정상방정식과 부분 피벗 가우스 소거를 사용하며, 유일해가 없는 특이 행렬은 오류로 처리합니다.

예시:

```text
A.txt      b.txt
1, 0       3
0, 1       -2
```

```cmd
test.exe lsm A.txt b.txt x.txt
```

결과 `x.txt`:

```text
-3
2
```
=======
