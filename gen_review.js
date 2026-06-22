const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
  HeadingLevel, BorderStyle, WidthType, ShadingType
} = require('/sessions/nice-blissful-galileo/mnt/outputs/lib/node_modules/docx');
const fs = require('fs');

const border = { style: BorderStyle.SINGLE, size: 1, color: "CCCCCC" };
const borders = { top: border, bottom: border, left: border, right: border };

function h1(text) {
  return new Paragraph({ heading: HeadingLevel.HEADING_1, children: [new TextRun(text)] });
}
function h2(text) {
  return new Paragraph({ heading: HeadingLevel.HEADING_2, children: [new TextRun(text)] });
}
function p(text) {
  return new Paragraph({ children: [new TextRun(text)] });
}
function pBold(text) {
  return new Paragraph({ children: [new TextRun({ text, bold: true })] });
}
function pCode(text) {
  return new Paragraph({
    children: [new TextRun({ text, font: "Courier New", size: 18, color: "C0392B" })],
    indent: { left: 720 }
  });
}
function sp() {
  return new Paragraph({ children: [new TextRun("")], spacing: { after: 200 } });
}

function issueTable(rows) {
  const colWidths = [1200, 1600, 5000, 1560];
  return new Table({
    width: { size: 9360, type: WidthType.DXA },
    columnWidths: colWidths,
    rows: [
      new TableRow({
        tableHeader: true,
        children: ["심각도", "파일", "문제", "위치"].map((t, ci) =>
          new TableCell({
            borders,
            width: { size: colWidths[ci], type: WidthType.DXA },
            shading: { fill: "2E4057", type: ShadingType.CLEAR },
            margins: { top: 80, bottom: 80, left: 120, right: 120 },
            children: [new Paragraph({ children: [new TextRun({ text: t, bold: true, color: "FFFFFF" })] })]
          })
        )
      }),
      ...rows.map(([sev, file, issue, loc], idx) => {
        const sevColor = sev === "버그" ? "C0392B" : sev === "경고" ? "E67E22" : "2980B9";
        const bg = idx % 2 === 0 ? "F8F9FA" : "FFFFFF";
        return new TableRow({
          children: [sev, file, issue, loc].map((val, ci) =>
            new TableCell({
              borders,
              width: { size: colWidths[ci], type: WidthType.DXA },
              shading: { fill: bg, type: ShadingType.CLEAR },
              margins: { top: 80, bottom: 80, left: 120, right: 120 },
              children: [new Paragraph({
                children: [new TextRun({
                  text: val,
                  color: ci === 0 ? sevColor : "000000",
                  bold: ci === 0
                })]
              })]
            })
          )
        });
      })
    ]
  });
}

const doc = new Document({
  styles: {
    default: { document: { run: { font: "Arial", size: 22 } } },
    paragraphStyles: [
      { id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 32, bold: true, font: "Arial", color: "2E4057" },
        paragraph: { spacing: { before: 320, after: 160 }, outlineLevel: 0 } },
      { id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 26, bold: true, font: "Arial", color: "048A81" },
        paragraph: { spacing: { before: 240, after: 120 }, outlineLevel: 1 } },
    ]
  },
  sections: [{
    properties: {
      page: {
        size: { width: 11906, height: 16838 },
        margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 }
      }
    },
    children: [
      new Paragraph({
        children: [new TextRun({ text: "STM32H747 CM7 코드 리뷰", bold: true, size: 44, font: "Arial", color: "2E4057" })],
        spacing: { after: 120 }
      }),
      new Paragraph({
        children: [new TextRun({ text: "작성일: 2026-06-22  |  대상: CM7/App (hw / ap / common / Modules)", size: 20, color: "666666" })],
        spacing: { after: 480 }
      }),

      h1("1. 요약"),
      p("총 14개 항목을 발견했습니다. 버그 5건, 경고 4건, 개선 5건입니다. IEC61850_SV.c와 ovtoct.c에 실제 동작에 영향을 주는 버그가 집중되어 있으며, 우선 처리를 권장합니다."),
      sp(),
      issueTable([
        ["버그",   "IEC61850_SV.c", "모든 ASDU에 동일한 svId[0] 사용 — svId[i]여야 함",                          "line 71"],
        ["버그",   "IEC61850_SV.c", "smpCnt가 루프 내에서 증가 — 같은 프레임 내 ASDU마다 카운터가 달라짐",        "line 138~139"],
        ["버그",   "IEC61850_SV.c", "SVPublisher_create 실패 시 태스크 return — vTaskDelete(NULL) 필요",          "line 61"],
        ["버그",   "ovtoct.c",      "DATA 루프 상한 오류 — 마지막 샘플(buf_index=1999) 누락",                     "line 127/172"],
        ["버그",   "adc.c",         "adcBuf_3(uint16_t)를 uint32_t*로 DMA — DMA 설정 불일치 시 메모리 오염",      "line 98/143"],
        ["경고",   "IEC61850_SV.c", "B·C·N 채널 전류·전압 모두 0 하드코딩 — 미구현 상태",                        "line 120~135"],
        ["경고",   "adc.c",         "#define _USE_ADC_TEST 소스 내 하드코딩 — 항상 활성화됨",                     "line 152"],
        ["경고",   "cli.c",         "cliPrintf(cmd_str) — format string 직접 전달, %s/%d 포함 시 UB",             "line 762"],
        ["경고",   "cli.c",         "cliParseArgs: argc++ 시 CLI_ARGS_MAX 초과 체크 없음 — 스택 오버플로 가능",   "line 568~571"],
        ["경고",   "galvano.c",     "스캔 종료 조건 == 비교 — interval이 약수가 아니면 무한 루프",                 "line 183/193"],
        ["개선",   "galvano.c",     "RunScanOnce / RunScanContinous 코드 중복 — 공통 함수로 리팩토링 권장",        "line 143/228"],
        ["개선",   "galvano.c",     "RunScanOnce에서 osDelay 없이 while 루프 — FreeRTOS 태스크 스타브 위험",       "line 156"],
        ["개선",   "def.h",         "이중 include guard (UUID + SRC_COMMON_DEF_H_) 중복 정리 필요",               "line 8~12"],
        ["개선",   "def.h",         "map 매크로: 정수 입력 시 정수 나눗셈 정밀도 손실, 괄호 불충분",               "line 125"],
      ]),
      sp(),

      h1("2. 버그 상세"),

      h2("2-1. IEC61850_SV.c — ASDU ID 버그 (line 71)"),
      p("SVPublisher_addASDU 호출 시 8개 ASDU 모두 svId[0](\"OCT_DATA_1\")을 사용합니다. IEC61850에서 각 ASDU는 고유 ID를 가져야 합니다."),
      pBold("현재 코드:"),
      pCode("ASDUs[i].asdu = SVPublisher_addASDU(svPublisher, svId[0], NULL, 1);"),
      pBold("수정 코드:"),
      pCode("ASDUs[i].asdu = SVPublisher_addASDU(svPublisher, svId[i], NULL, 1);"),
      sp(),

      h2("2-2. IEC61850_SV.c — smpCnt 공유 버그 (line 138)"),
      p("smpCnt가 ASDU 루프 내부에서 증가하여 한 프레임에서 ASDU 0 = N, ASDU 1 = N+1, ... ASDU 7 = N+7이 됩니다. 동일 프레임 내 모든 ASDU는 같은 smpCnt를 가져야 합니다."),
      pBold("수정 방향 — smpCnt 증가를 루프 바깥으로 이동:"),
      pCode("for (ASDU_num = 0; ASDU_num < ASDU_NUM; ASDU_num++) {"),
      pCode("    SVPublisher_ASDU_setSmpCnt(ASDUs[ASDU_num].asdu, smpCnt);"),
      pCode("}"),
      pCode("smpCnt++;"),
      pCode("if (smpCnt >= smpCntMax) smpCnt = 0;"),
      sp(),

      h2("2-3. IEC61850_SV.c — FreeRTOS 태스크 비정상 종료 (line 61)"),
      p("SVPublisher_create가 NULL을 반환하면 while(1) 블록에 진입하지 않고 태스크 함수가 return됩니다. FreeRTOS에서 태스크 함수의 return은 정의되지 않은 동작입니다."),
      pBold("수정 코드:"),
      pCode("if (svPublisher == NULL) {"),
      pCode("    logPrintf(\"[ERROR] SVPublisher_create failed\\n\");"),
      pCode("    vTaskDelete(NULL);"),
      pCode("    return;"),
      pCode("}"),
      sp(),

      h2("2-4. ovtoct.c — DATA 루프 상한 오류 (line 127, 172)"),
      p("ADC_BUF_SIZE=2000, 헤더 이후 i=4일 때, 루프 조건 count < ADC_BUF_SIZE*3(=6000)은 count=4~5998, buf_index=0~1998로 마지막 샘플(buf_index=1999)이 누락됩니다."),
      pBold("현재 코드:"),
      pCode("for (int count = i; count < ADC_BUF_SIZE * 3; count += 3)"),
      pBold("수정 코드:"),
      pCode("for (int count = i; count < i + ADC_BUF_SIZE * 3; count += 3)"),
      sp(),

      h2("2-5. adc.c — adcBuf_3 DMA 타입 불일치 (line 98)"),
      p("adcBuf_3는 uint16_t[ASDU_NUM] 배열이지만 HAL_ADC_Start_DMA에 (uint32_t*)로 캐스팅됩니다. STM32CubeMX에서 hadc3의 DMA DataWidth가 Word(32bit)로 설정되어 있다면 16비트 배열에 32비트씩 기록되어 인접 메모리를 오염시킵니다. hadc3 DMA 설정을 Half-Word로 맞추거나 버퍼를 uint32_t로 변경해야 합니다."),
      sp(),

      h1("3. 경고 상세"),

      h2("3-1. cli.c — Format String 취약점 (line 762)"),
      p("cliShowList에서 cliPrintf(cmd_str)와 같이 format string 자리에 사용자 데이터를 직접 전달합니다. cmd_str에 %s, %d 등이 포함되면 스택 손상이 발생합니다."),
      pBold("수정 코드:"),
      pCode("cliPrintf(\"%s\", p_cli->cmd_list[i].cmd_str);"),
      sp(),

      h2("3-2. cli.c — cliParseArgs 버퍼 오버플로 (line 568)"),
      p("strtok_r 루프에서 argc가 CLI_ARGS_MAX(32)를 넘어도 argv 배열에 계속 씁니다."),
      pBold("수정 코드:"),
      pCode("for (tok = strtok_r(cmdline, delim, &next_ptr);"),
      pCode("     tok && argc < CLI_ARGS_MAX;"),
      pCode("     tok = strtok_r(NULL, delim, &next_ptr))"),
      sp(),

      h2("3-3. galvano.c — 스캔 종료 무한 루프 위험 (line 183, 193)"),
      p("current_position_x == end_x 조건은 interval_x가 (end_x - start_x)의 약수일 때만 성립합니다. 그렇지 않으면 end_x를 지나쳐 무한 루프가 됩니다."),
      pBold("수정 코드:"),
      pCode("if (current_position_x >= end_x) { state = y_stepIncrease; }"),
      pCode("if (current_position_x <= start_x) { state = y_stepIncrease; }"),
      sp(),

      h2("3-4. IEC61850_SV.c — B·C·N 채널 미구현 (line 120~135)"),
      p("CurrentB, CurrentC, CurrentN 및 모든 Voltage 채널이 0으로 하드코딩되어 있습니다. 실제 측정값 연결이 필요합니다. 주석이나 TODO를 명시하여 미완성임을 표시해야 합니다."),
      sp(),

      h1("4. 개선 권장사항"),

      h2("4-1. galvano.c — 코드 중복 제거"),
      p("RunScanOnce와 RunScanContinous가 동일한 상태머신 코드를 갖습니다. runScanCore(bool continuous) 내부 함수로 분리하면 버그 수정을 한 곳에서 반영할 수 있습니다."),
      sp(),

      h2("4-2. galvano.c — RunScanOnce FreeRTOS 친화성"),
      p("RunScanOnce의 while 루프에 osDelay가 없어 루프 실행 중 낮은 우선순위 태스크가 실행되지 못합니다. 최소 osDelay(1)을 각 스텝 후 추가하거나, 타이머 ISR 기반 스텝 제어로 재설계를 권장합니다."),
      sp(),

      h2("4-3. adc.c — _USE_ADC_TEST 관리"),
      p("#define _USE_ADC_TEST를 adc.c 내에 하드코딩하지 말고 hw_def.h에서 조건부로 정의하여 빌드 설정(Debug/Release)에 따라 제어할 수 있도록 합니다."),
      sp(),

      h2("4-4. def.h — 이중 include guard"),
      p("UUID 기반 guard(E2D091EB...)와 SRC_COMMON_DEF_H_ guard가 중첩되어 있습니다. 하나만 남기고 정리하세요."),
      sp(),

      h2("4-5. def.h — map 매크로 개선"),
      p("현재 map 매크로는 정수 인수에서 나눗셈이 먼저 일어나 정밀도를 잃습니다. 또한 복합 표현식 인수를 안전하게 처리하려면 각 인수를 괄호로 묶어야 합니다."),
      pBold("개선안:"),
      pCode("#define map(v,i0,i1,o0,o1) \\"),
      pCode("    ((float)((v)-(i0)) * ((o1)-(o0)) / ((i1)-(i0)) + (o0))"),
    ]
  }]
});

Packer.toBuffer(doc).then(buf => {
  fs.writeFileSync('/sessions/nice-blissful-galileo/mnt/stm32h747/CM7_코드리뷰.docx', buf);
  console.log('done');
}).catch(e => { console.error(e); process.exit(1); });
});
