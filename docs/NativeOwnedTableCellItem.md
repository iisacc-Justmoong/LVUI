# LVUI 독립 표 셀

셀의 값 검증·상태·기하 모델과 그리기·편집 필드 조합·입력·수명은 iiGUI가
소유한다. LVUI는 디자인 값, 교체 가능한 값 검증 계약과 바인딩을 제공한다. 앱 생성 코드는 두 독립 모듈을 조합한다. Qt, QML 또는 LVRS
위젯은 요구하지 않는다. C++ API와 기본 선언형 `TableCellItem`을 모두 사용할 수 있다.

```text
import vm CellModel
Window {
    TableCellItem(text: vm.value, valueType: "int", inputable: true,
                  onInputEdited: vm.edited, onInputSubmitted: vm.submitted,
                  onInputRejected: vm.rejected)
}
```

선언의 `valueValidator`는 `std::shared_ptr<const lvui::TableCellValueValidator>`를
받으며 생략하면 기본 형식 검증을 사용한다. 내용과 크기, 상태와 색상은 반응형으로
갱신할 수 있다. `textStyle`은 고정 스타일이며 반응형 크기가 필요하면 `fontSize`를
사용한다.

기본 크기는 234×24, 내용 여백은 8이며 왼쪽 구분선을 표시한다. `inputable`과
`enabled`가 모두 참이면 활성화 또는 `begin_edit()`로 편집을 시작한다. 편집할 때는
현재 셀 값과 별도 초안을 유지한다. Enter 또는 `commit_edit()`는 완성된 초안을
검사하고 성공한 값으로 셀을 갱신한 다음 `submitted`를 호출한다. 입력 중 `edited`,
거부 시 `rejected`에 값과 형식을 전달한다. Escape, 포커스 상실 또는 `cancel_edit()`는
초안을 버린다. 문자 조합이 진행 중이면 제출을 미룬다.

기본 값 형식은 `string`/`text`, `int`/`integer`, `number`/`double`/`float`,
`bool`/`boolean`이다. 사용자 정의 형식이나 다른 규칙은 `TableCellValueValidator`
구현을 주입해 교체한다. 숫자는 전체 문자열이 해당 형식이어야 하며 무한대와 NaN을
허용하지 않는다. 소수점 해석은 시스템 로캘과 무관하다. 외부 값이나 검증 조건이 편집 도중 변경되면 초안은 유지되지만
오래된 초안의 제출은 거부한다. 앱이 현재 초안과 같은 값을 다시 제공하는 경우는
정상적인 상태 반영으로 취급한다.
셀의 색상만 바뀌면 열려 있는 입력기의 색상도 갱신하며, 편집 초안·선택·포커스와
확정 전 문자 조합은 유지한다. `TableRow`가 재사용하는 셀에도 같은 규칙을 적용한다.

화면의 텍스트와 접근성 이름은 UTF-8 경계를 지켜 최대 8,192바이트로 표시하며
원본과 편집 값은 보존한다. 잘못된 설정은 마지막 정상 상태를 유지하고, 생성한
스레드에서만 위젯을 사용한다. 실제 운영체제 문자 입력기와 접근성 검증은 아직 필요하다. [표 행](NativeOwnedTableRow.md)은
이 셀을 키 기준으로 조합한다.

`LVUI.OwnedTableCellItem.Model`, `Host`, `Codegen`, `Generated`는 입력 검증, 배치, 초안·충돌·제출·취소와
수명 경계를 창을 열지 않는 환경에서 검사한다.

셀 위젯의 퇴역 시 콜백·초안·검증기 참조를 해제하며, 갱신 중 재진입으로 퇴역한 이전 셀은 새 상태를 게시하지 않는다.
