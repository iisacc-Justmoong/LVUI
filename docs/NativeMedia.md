# Card·Avatar AOT

기본 `Avatar`는 LVUI의 독립 C++ 모델과 이미지 어댑터로 생성한다. `Card`는 아직
`FRAMEWORK LVRS`의 레거시 경로에 있다. AppCard는 별개이다.
Card의 type(.File/.FilePreview/.Folder/.Project/.Device/.Model/.Member/.Link),
size(.Small/.Medium/.Large), detail(.Brief/.Detailed), displayState(.Automatic/
.DefaultState/.HoverState/.SelectedState)를 typed enum으로 생성한다. Avatar.size도 같은
크기 이름을 받는다. enum·이미지·행·색상·데이터는 scoped C++ 멤버 또는 Core Observable로
연결할 수 있다. filename/title, icon/iconSource는 각각 동시에 지정할 수 없는 별칭이다.

```lvui
import vm MediaBackend
Window {
 HStack {
  Avatar(size: .Small, source: vm.image)
  Card(type: .File, title: vm.title, previewSource: vm.image,
       selected: vm.selected, onSelectedChanged: vm.select)
  Card(type: .Project, rows: vm.rows, progress: vm.progress,
       onMenuRequested: vm.menu, onActionTriggered: vm.action)
 }
}
```

source/previewSource/icon/iconSource는 준비된 shared_ptr<const RasterImage>이다. 문자열 URL,
asynchronous/cache 플래그는 지원하지 않으며 명시적으로 진단한다. rows는 vector<CardRow>,
previewComponent/iconComponent는 WidgetFactory 또는 불변 CardComponent를 받는다.
반응형 팩토리는 Observable<CardComponent>를 사용한다. previewComponent와 중괄호 프리뷰
자식은 동시에 지정할 수 없다. 커스텀 아이콘/프리뷰는 네이티브 세대 수명에 속한다.

Avatar는 원형으로 중앙 자르며 Small·Medium·Large 자연 크기는 각각 24·40·64 논리 px이다.
폭이나 높이를 0으로 두면 해당 축의 자연 크기를 사용한다. 준비된 픽셀, 크기, 색상,
사용 가능 여부와 접근성 이름은 반응형으로 바꿀 수 있다. 잘못된 값은 마지막 유효
설정을 유지한다. 빈 이미지는 배경만 그린다. 로딩과 디코딩은 앱이 제공한다.
입력 모델과 생성 코드는 Qt·QML·LVRS 타입이나 라이브러리에 의존하지 않는다.
소유 스레드의 접근성 알림이 설정을 다시 변경하면 나중 설정과 사용 가능 상태를 유지한다.

레거시 Card의 필드별 바인딩은 기존 래퍼의 set_options/set_*_content로 갱신한다. 선택 콜백은 bool,
메뉴·액션·클릭 콜백은 인자 없이 실행한다. scope 백엔드는 한 번 생성되어 유지된다.
기타 visible/flex는 공통 위젯 계약을 따른다. 알 수 없는 입력, 잘못된 enum·수치·별칭,
즉시 호출 형태의 콜백, Avatar 자식은 생성 오류이다. generated C++에 LVUI 파서/VM은 없다.

독립 Avatar의 모델·생성 코드·호스트 검사와 레거시 Card의 컴파일 계약은 별도로 다룬다.
원본 전체 상속 API,
URL 비동기 로딩·캐시, 모션·글로벌 Theme·일부 clip/말줄임과 플랫폼 검증은 남아 있다.

선행 점 뒤의 enum 경로에서는 시각 컴포넌트 예약어도 값으로 사용할 수 있다.
따라서 Card.type의 .Link는 허용하지만 let/import/var 이름으로 Link를 선언하는 것은
계속 금지된다. 선언·리터럴 예약어의 enum 우회도 허용하지 않는다.
