# Card·Avatar AOT

기본 `Avatar`의 모델·기하·입력·접근성·그리기는 iiGUI가 소유한다. LVUI는
디자인 토큰과 빈 사진의 SVG 자산 공급자를 주입한다. `Card`는 아직 LVUI의
C++ 모델과 어댑터를 사용하며 AppCard는 별개이다. 카드의 현재 상세 계약은 [독립 카드](NativeOwnedCard.md)를 따른다.
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

`source`와 `previewSource`는 준비된 `shared_ptr<const RasterImage>` 또는 화면
서비스나 개별 `imageLoader`가 해석할 문자열이다. `icon`은 래스터이고 `iconSource`는 교체 가능한
아이콘 공급자다. `rows`는 `vector<CardRow>`를 받는다. 카드 전체 콘텐츠 또는
미리보기·아이콘 영역에 사용자 View를 지정할 수 있으며 세부 계약은
[독립 카드](NativeOwnedCard.md)를 따른다. 로더 정책과 원격 요청은
[이미지 로딩](NativeImageLoading.md)을 따른다.

Avatar는 원형으로 중앙 자르며 Small·Medium·Large 자연 크기는 각각 24·40·64 논리 px이다.
폭이나 높이를 0으로 두면 해당 축의 자연 크기를 사용한다. 준비된 픽셀, 크기, 색상,
사용 가능 여부와 접근성 이름은 반응형으로 바꿀 수 있다. 잘못된 값은 마지막 유효
설정을 유지한다. 이미지가 없거나 지워지면 Figma LVUI Avatar의 사용자 심벌을
원 지름의 절반 크기로 중앙에 그린다. 사진이 있으면 원형 중앙 자르기로 대체하고,
사진이 지워지면 같은 심벌로 돌아간다. 심벌의 원본 형상은 인스턴스에서 가져온
`avatar-placeholder` SVG이다. 기본 아이콘 래스터는 실제 할당된 원의 지름과 그리기 배율을
기준으로 공통 캐시에서 선택한다. 최소 2배 표본화를 유지하고 최대 512픽셀로
제한한다. 심벌의 여백도 실제 할당된 지름을 기준으로 하므로 호스트가 배치
크기를 바꾸면 비율과 다음 그리기의 래스터 크기가 함께 바뀐다.
로더는 앱이 선택하고 소유한다.
입력 모델과 생성 코드는 Qt·QML·LVRS 타입이나 라이브러리에 의존하지 않는다.
소유 스레드의 접근성 알림이 설정을 다시 변경하면 나중 설정과 사용 가능 상태를 유지한다.

### Avatar 심벌의 공통 색상 계약

Figma의 `Text/bodyColor`를 Avatar의 빈 사진 상태 심벌에만 적용한다. LVUI
컴파일러는 선택한 공통 스타일 토큰을 생성 코드에 전달하며 RGB와 알파를 모두
사용한다. 원형 배경에는 선택한 `Panel/surfaceRaised`를 적용하고 명시된
`backgroundColor`가 있으면 그 값을 우선한다. 원본 SVG의 두 흰 도형에서
캡처 당시의 0.8 알파를 제거한 마스크를
파생해 크기별로 공통 캐시하고, iiGUI의 이미지 색상 입력으로 알파 0과 1을
포함하는 색을 적용한다. 원본 SVG 자체는 변경하지 않으며 캡처한 두 색상 속성이
달라지면 오류로 처리한다. 사진에는 심벌 색상을 적용하지 않는다. 공통 네이티브
위젯의 `ii::gui::AvatarInput.placeholder_color` 갱신은 기존 위젯과 접근성을 유지하고,
유효하지 않은 색상은 마지막 유효 설정을 보존한다. 선언형 Avatar에는 별도의
색상 재정의 속성을 추가하지 않았다. 정적 컴파일 토큰과 네이티브 설정 갱신은
실행 중 전역 테마 구독을 의미하지 않는다. `AvatarInput`의 필드 구성이 바뀌므로
C++ 소비자는 새 헤더로 다시 빌드해야 한다.

### Avatar 심벌의 크기와 배율 계약

기본 심벌의 논리 크기는 원의 짧은 지름 절반이다. 배치 크기나 배율이 바뀐 뒤
그릴 때 래스터를 다시 선택하며, 입력 사진이 있으면 사진을 그대로 사용하고
지우면 심벌을 복원한다. 뷰의 크기·원형 마스크·접근성 및 기존 이미지 로딩
계약은 유지한다. 크기 계산·캐시와 그리기 장면 검사는 운영체제 창을 열지
않는다. 실제 기기의 화면 배율 전환과 GPU 출력은 별도 검증 대상이다.

[Congregation Figma의 Avatar](https://www.figma.com/design/r4E0PNUdZuo6dEiMEitF2I/Congregation?node-id=268-2666)
심벌은 `Text/bodyColor` 변수에 연결되어 있다. 공통 토큰의 빌드 시 선택과
네이티브 입력 갱신을 지원하지만 실행 중 전역 테마 변경에 대한 구독은 아직
제공하지 않는다. 원본 형상·알파, 캐시, 토큰 전달과 사진 분리는 창 없는
모델·컴파일러·위젯 검사로 확인했다. 위젯의 그리기 명령은 테스트 전용 iiGUI
소프트웨어 합성 경계에 전달하여 선택 색상의 불투명 픽셀과 완전 투명 픽셀도
확인했다. 이는 실제 GPU 출력이나 기기 표시를 확인한 결과가 아니다.

Card의 필드별 바인딩은 자체 어댑터의 `set_options`로 갱신한다. 선택 콜백은 bool,
메뉴·액션·클릭 콜백은 인자 없이 실행한다. scope 백엔드는 한 번 생성되어 유지된다.
기타 visible/flex는 공통 위젯 계약을 따른다. 알 수 없는 입력, 잘못된 enum·수치·별칭,
즉시 호출 형태의 콜백, Avatar 자식은 생성 오류이다. generated C++에 LVUI 파서/VM은 없다.

독립 Avatar와 Card의 모델·생성 코드·호스트 검사는 별도로 다룬다.
원본 전체 상속 API,
백그라운드 파일 읽기·디코딩, 모션·글로벌 Theme·일부 clip/말줄임과 플랫폼 검증은 남아 있다.

선행 점 뒤의 enum 경로에서는 시각 컴포넌트 예약어도 값으로 사용할 수 있다.
따라서 Card.type의 .Link는 허용하지만 const/import/var 이름으로 Link를 선언하는 것은
계속 금지된다. 선언·리터럴 예약어의 enum 우회도 허용하지 않는다.

### 자산과 그래픽 연결 경계

`NativeAvatarAssets.h`는 SVG 마스크 자산만 제공하며 iiGUI를 포함하지 않는다.
생성된 앱은 `ii::gui::make_avatar_placeholder_source`에 선택한 디코더를 연결한다.
이전 LVUI의 `avatar_placeholder_image`, `avatar_placeholder_mask_image`,
`avatar_placeholder_source` C++ 헬퍼는 제거되었다. 직접 소비자는
`avatar_placeholder_mask_catalog()`과 iiGUI의 이미지 공급자 계약을 조합한다.
macOS arm64 헤드리스 검사에서 생성된 아바타의 크기·색상·마스크와 공통 캐시,
독립 설치 소비자 및 Core만 포함한 LVUI 자산 소비자를 확인했다. 실제 창·GPU와
다른 운영체제는 이 검증에 포함되지 않는다.
