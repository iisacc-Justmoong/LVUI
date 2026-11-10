# 목록 항목 선택 모델

LVUI는 옵션과 선언 설정, `ListItemSelectorModelPolicy`를 제공한다.
선택 상태·이동·유효성 검사는 iiGUI의 `BasicListItemSelector`가 소유한다. `choose()`는 사용 가능한 다른 옵션만 선택하고, `navigate()`는 방향으로
이동하면서 비활성 옵션을 건너뛰며 끝에서 멈춘다. 빈 목록은 기본 문구를 표시하고
선택 이벤트를 발생시키지 않는다. 옵션은 최대 400개이며 텍스트 길이와 UTF-8을
검증한다. 설정이 유효하지 않으면 기존 상태를 유지한다.

이 모델은 독립 [선택기 화면](NativeOwnedListItemSelector.md)과 목록 항목의 선언형
선택 유형에 연결된다.
`LVUI.ListItemSelector.ModelIntegration`은 LVUI 값과 iiGUI 모델의 빈 목록, 비활성 옵션, 이동, 경계와 잘못된 입력을 검증한다.
