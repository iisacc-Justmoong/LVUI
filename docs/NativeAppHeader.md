# LVUI 선언과 iiGUI 앱 헤더

`AppHeader`의 압축 모드·입력 검증·자연 크기, 제목·메뉴 그리기와 액션 슬롯 수명은
`iiGUI/AppHeaderModel.h`와 `iiGUI/NativeAppHeaderView.h`가 소유한다. LVUI 생성기는
앱이 iiGUI에 디자인 값과 바인딩을 전달하는 C++을 생성한다. LVRS, Qt 또는 QML
구현을 호출하지 않는다. 앱이 추가한 액션은 제목이나 스타일이 갱신되어도 같은
위젯 세대에 남는다.

일반 모드의 가로·세로 여백은 16, 행 간격은 12, 액션 간격은 8, 메뉴 패딩은 10이다.
압축 모드에서는 각각 10, 10, 8, 6, 8이며 명시적 입력이 이 기본값보다 우선한다.
메뉴 높이는 일반 32, 압축 28이다. 자연 높이는 최소 56이며 구성된 내용의 자연 높이에
32를 더한 값보다 작지 않다. 제목은 22px/700, 부제는 12px/600으로 표현한다.
배경·텍스트·메뉴 표면 색상과 메뉴 표시 여부, 크기·활성 상태도 직접 지정할 수 있다.

직접 C++ 구성은 `ii::gui::NativeAppHeader::create`와 `actions_root()`를 사용한다.
일반·압축 치수와 메뉴 포커스 색은 `ii::gui::AppHeaderOptions`로 제공한다. 메뉴 버튼은
접근성 버튼 역할과 포커스 표시를 갖고 제공된 콜백을 호출한다. 갱신은 유효성 검사 후
적용하며, 비활성화 중 재진입한 최신 설정을 덮어쓰지 않는다. 갱신 중 취소 콜백의
예외는 원인을 가진 오류로 전달하고 이미 발생한 외부 부수 효과를 취소하지 않는다. 세대 퇴역 후에는 메뉴
콜백과 호스트 핸들을 사용할 수 없다. 활성 세대에서는 `set_menu_clicked`로 콜백을 교체할 수 있다.

기본 선언형 `AppHeader`는 이 구현을 생성하고 자식 View를 액션 슬롯에 배치한다.
`title`, `subtitle`, `enabled`, `width`, `height`, `menuVisible`, `compact`,
`contentHorizontalPadding`, `contentVerticalPadding`, `rowSpacing`, `actionSpacing`,
`menuButtonPadding`, 배경·텍스트·메뉴 색상과 `onMenuClicked` 및 공통 가시성·
불투명도·flex를 지원한다. 관찰 가능한 입력은 함께 계산하여 기존 액션을 유지하고
잘못된 입력은 바인딩 오류로 보고한다. 메뉴 패딩은 버튼 폭을 호스트 한도 이내로
유지하기 위해 0–4090이다. 기본 색상·간격·글자 크기는 LVUI 토큰에서 공급한다.
윈도우 악센트가 바뀌면 메뉴 포커스 색도 갱신하며, 색만 바뀐 경우 관련 없는 배치나
접근성을 다시 게시하지 않는다. 실제 플랫폼의 시각·접근성 검증은 아직 진행 중이다.

기존 `lvui::AppHeader` 모델과 `lvui::native::NativeAppHeader`는 제거했다. C++에서
직접 사용하던 소비자는 iiGUI의 `AppHeaderModel`, `AppHeaderOptions`와
`NativeAppHeader`를 사용한다. macOS arm64에서 iiGUI 단독 모델·호스트·설치 소비자와
LVUI 생성 코드·반응형 악센트·호스트·설치 패키지 경계 검사를 통과했다. 실제 창,
GPU 출력과 다른 운영체제 실행은 확인하지 않았다.
