# 모바일 탐색 컴포넌트

Society의 모바일 탭과 Dreamscapes의 모바일 탐색은 안정적인 키를 가진 항목,
접근성 이름과 아이콘, 현재 선택, 사용자 활성화 및 하단 안전 영역을 사용한다.
Dreamscapes는 독립된 검색 동작도 사용한다. 선택은 앱이 소유할 수 있으며
자동 선택을 끄면 활성화가 기존 선택을 바꾸지 않는다. 외부 선택 변경은 사용자
활성화 이벤트를 발생시키지 않는다. `MobileNavigationBar`와 `MobileTabBar`는
애플리케이션에서 제공한 모델을 구독하고 실제 변경된 항목만 갱신한다.

시각적 기본값은 Figma LVUI의 모바일 탐색 항목과 공통 패널·텍스트 토큰을 따른다.
아이콘은 LVUI의 내장 아이콘 카탈로그를 사용한다. 좁은 화면에서도 터치 영역을
유지하며 항목 영역을 가로로 스크롤한다. 창의 안전 영역 값은 호스트 또는 앱이
전달한다. 키보드 포커스 표시는 현재 창의 악센트 토큰을 따른다. 모델은
애플리케이션의 C++ `std::vector<MobileNavigationItem>`이며
검색은 별도의 선택적 항목이다. 항목은 최대 5개이며 키와 아이콘 이름, 표시 문구
또는 접근성 이름이 필요하다. 인접 항목의 활성화, 외부 선택과 선택 항목의 가시성은
주기 검사 없이 갱신된다.

```lvui
import vm NavigationModel
Window {
    MobileNavigationBar(model: vm.items, search: vm.searchItem,
        currentIndex: vm.current, autoSelect: false, width: vm.width,
        bottomSafeInset: vm.safeInset, onActivated: vm.activate,
        onSearchRequested: vm.search)
}
```

뷰모델은 `ii::core::Observable<std::vector<lvui::MobileNavigationItem>>`과
`ii::core::Observable<std::optional<lvui::MobileNavigationItem>>`을 제공한다. 선택을
외부에서 관리할 때 `activate(int)`는 요청만 처리하고 실제 선택을 `current`에
반영한다. `MobileTabBar`도 동일한 타입의 모델을 받으며 검색을 생략할 수 있다.
Python에서는 `ii_lvui.compile_native`로 동일한 선언을 AOT 컴파일한다.

두 선언형 타입의 배치·입력·배지·선택 모션과 위젯 수명은 iiGUI가 소유하며,
LVUI는 Figma 디자인 토큰과 독립 바인딩을 제공한다. 반투명 재질·배지와 움직이는
선택 표시는 [모션 계약](MobileTabEffects.md)을 따른다. 최소화와 원본 앱의 테스트용
객체 이름 계약은 아직 구현하지 않았다. 항목의 키가 이동하면 포커스는 같은 키를 따라가지만 물리적
위젯 슬롯은 인덱스로 재사용한다. 플랫폼 실기기 실행이나 모든 애니메이션 상태는
검증하지 않았다.

퇴역하면 선택 콜백과 아이콘 캐시를 반환하며 진행 중인 변경은 오래된 상태를 게시하지 않는다. macOS arm64의 헤드리스 검사와 독립 설치 소비자 검사는 모바일 실기기 표시 검증을 뜻하지 않는다.

## 독립 모바일 탭

`MobileTab`과 `MobileNavigationTab`은 앱이 개별적으로 배치할 수 있는 iiGUI 탭이다.
두 타입은 탐색 막대와 아이콘·레이블·배지 그리기 구현을 공유하며 `iconName`은
필수이다. `text`, `badge`, `badgeDot`, `badgeAccessibilityName`, `selected`, `enabled`,
`showLabel`, `drawSelection`, `platformStyle`, `width`, `height`, `iconSource`,
`iconRevision`, `focusColor`, `motionEnabled`, `accessibilityName`, `onClick`을 받는다.
선택 여부는 바인딩한 C++ 뷰모델이 소유하고 클릭은 요청으로 전달한다. 아이콘만
표시할 때 접근성 이름을 생략하면 아이콘 이름을 사용한다.

```lvui
import vm NavigationModel
Window {
    MobileTab(iconName: "home", text: vm.label, selected: vm.selected,
              badge: vm.badge, onClick: vm.activate)
}
```

상태가 바뀐 동안에만 전환 프레임을 요청하며 비활성화하면 예약된 프레임을 취소한다.
시스템이 동작을 줄이도록 설정하면 전환을 중단한다. 입력 변경은 기존 위젯의 상태를 갱신하고,
퇴역 시 아이콘·콜백·모션 참조를 해제한다. macOS arm64의 창 없는 생성·입력·배지·
수명·프레임 검사만 수행했으며 모바일 실기기 외형 검증은 아직 없다.
아이콘 원천의 조회 중 새 입력이 재진입하면 이전 조회 결과를 적용하지 않으며,
조회 중 위젯이 퇴역하면 결과를 게시하지 않고 닫힘을 반환한다.
