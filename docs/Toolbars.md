# 목록·계층 도구 막대 AOT

기본 생성 경로는 LVUI가 소유한 [목록 도구 모음](NativeOwnedListToolbar.md),
[목록 푸터](NativeOwnedListFooter.md), [도구 버튼](NativeOwnedToolbarButton.md),
[계층 도구 모음](NativeOwnedHierarchyToolbar.md)을 사용한다. 각 문서는 현재 지원하는
입력과 남은 기능을 설명한다. 아래는 아직 제거하지 않은 `FRAMEWORK LVRS` 호환
경로의 역사적 계약이며 LVUI의 독립 구현을 뜻하지 않는다.

FRAMEWORK LVRS에서 ListToolbar·ListFooter·HierarchyToolbar·ToolbarButton을 실제
C++ create 호출로 생성한다. 일반·Observable C++ 멤버와 지연 메서드 참조를 사용한다.
HierarchyToolbar의 직접 ToolbarButton 자식은 같은 WidgetBuilder의 기본 manual 슬롯에
구성하고 자동 등록한 뒤 정규화한다. 모델이 비어 있어도 수동 자식의 백엔드는 재생성하지 않는다.

ListFooter의 button1/2/3은 ListFooterSlot, HierarchyToolbar의 buttonItems는
vector<HierarchyToolbarEntry>, activeButtonId는 ToolbarId인 C++ 멤버이다. 반응형 구성은 Observable<HierarchyToolbarItems>와
Observable<shared_ptr<const ListFooterSlot>>처럼 불변 구성의 공유 포인터를 교체한다.
plain vector/slot과 함께 typed toolbar_items/footer_slot으로 변환한다. Core Observable
갱신은 같은 BindingScope로 전달하며 종료하면 구독도 종료한다. buttonId 상수는 문자열
또는 정수이고 임의 사전/JS 표현식이나 호출식을 실행하지 않는다.

ListToolbar onIconClicked는 (size_t,string), ListFooter onButtonClicked는
(size_t,const ListFooterSlot&), 계층 onActiveChanged는 const ToolbarSelection&,
onButtonTriggered는 const ToolbarTrigger&, onButtonEventTriggered는
(const ToolbarEvent&,const ToolbarTrigger&)를 받는 메서드 참조이다.

지원하지 않는 입력·자식·잘못된 상수는 진단한다. 실제 앱·생성 검사·self-test의 실행은
전체 작업 최종 일괄 단계까지 보류한다. 빌드와 실행 결과를 구분한다.
