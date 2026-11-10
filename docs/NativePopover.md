# LVUI 독립 팝오버

기본 선언형 `Popover`는 LVUI가 제공하는 디자인 값과 바인딩을 앱에서
`iiGUI/PopoverModel.h`, `iiGUI/NativePopoverView.h`와 조합한다. iiGUI가 검증·
기하·표면·입력·모션·팝업 수명을 소유하며 LVUI 바인딩은 Core 외 SDK를 직접
참조하지 않는다. LVRS, Qt 또는 QML 구현을 호출하지 않는다.

```lvui
import vm EditorBackend
Window {
    Popover(id: "details", width: vm.width, surfaceOpacity: 0.25,
            onOpened: vm.opened, onClosed: vm.closed) {
        VStack { Label(text: vm.description) }
    }
}
```

`width`와 `height`의 0은 콘텐츠에 따른 자동 크기이다. 기본 여백은 12, 화면 가장자리
여백은 8, 반경은 12, 표면 채색 강도는 0.25이다. `surfaceColor`, `primaryColor`, `blurEnabled`, `motionEnabled`, `modal`, `dim`,
`focus`, `clip`, `closeOnEscape`, `closeOnPressOutside`, `closeOnContextRequest`,
`accessibilityName`과 열림·닫힘·취소 콜백을 설정할 수 있다. 문맥 요청 정책을
생략하면 일반 외부 클릭 정책을 따른다. 콘텐츠는 하나의 직접 View이며 여러 요소는
스택에 담는다. 별도 팝업은 중첩할 수 있다.

C++ `PopoverConfiguration::placement`는 기본 앵커 배치 또는 창 중앙 배치를
선택한다. 잘못된 열거값은 거부하며, `NativePopover`가 선택한 배치를 호스트에
전달한다. 선언형 `Popover`의 배치 입력은 아직 제공하지 않는다.

크기·색상·정책 입력은 반응형으로 변경할 수 있으며 잘못된 후보는 기존 상태와
콘텐츠를 유지하고 바인딩 오류를 전달한다. 생성된 위젯 핸들의 `open_at`, `open_for`,
`close_popup`으로 표시 수명을 제어한다. 닫힘 콜백이 새 팝오버를 열었다면 이전
취소 이벤트는 새 열린 상태에 전달하지 않는다. 팝오버가 제거되면 핸들도 만료된다.

기본 배경은 원본 LVUI의 `Panel/surfaceSolid`와 공통 유리 패널 재질을 사용한다.
범용 호스트에는 16px 배경 흐림, 공통 방사형 악센트, 1px 유리 테두리,
0.14 불투명도와 1px 오프셋의 안쪽 광택, 아래로 8px 이동한 24px 그림자와
그림자 불투명도 0.22를 전달한다. 악센트 래스터는 실제 논리 크기와 배율에 맞추어
생성한다. 입력이 같으면 래스터를, 기하가 같으면 도형 기여도 계산을 재사용한다.
`surfaceColor`와 `surfaceOpacity`는 재질 틴트에
적용하며 반응형으로 갱신할 수 있다. `blurEnabled`를 끄면 불투명 기본 표면으로
폴백하고 다시 켜면 흐림을 복원한다. C++에서는 같은 설정을
`PopoverConfiguration::blur_enabled`로 지정한다. iiGUI 재질 모델이 주입된 정책으로 공통 패널 수치를 계산한다.

공통 재질의 다색 방사형 정지점과 투명도를 채색된 밑그림으로 전달한다.
블러가 켜진 경우 경계 밖 악센트도 공통 재질의 흐림 입력으로 제공한다.
흐림을 끈 불투명 폴백에도 악센트와 안쪽 광택을 유지하고 이전 화면은 캡처하지
않는다. 이 합성은 GPU 재질 경로를 사용한다. 플랫폼별 실제 표시와 보조 기술
동작은 별도 검증이 필요하다.
헤드리스 상태 검사는 창이나 GPU를 실행한 결과가 아니다.

## 창 강조색 이식 계약

`primaryColor`를 생략한 선언형 팝오버는 정적·반응형 창 `accent`의 공통 기본색을
상속한다. 재사용 View의 팝오버도 같은 범위를 따르며, 명시한 `primaryColor`는
창 값보다 우선한다. 창에 강조색이 없으면 기본 파랑을 사용한다. C++에서는
`PopoverConfiguration::primary_color`로 지정할 수 있다. 열린 팝오버의 강조색
변경은 팝오버 인스턴스나 콘텐츠를 교체하지 않고 반영하며, RGB만 바뀌면
기존 기하 계산을 재사용하고 채색된 래스터는 다시 만든다. 유효하지 않은 색상이나 거부된 창 변경은 마지막
유효 색상을 보존한다. 관련 단위 및 생성 위젯 검사는 운영체제 창과 GPU를 열지
않는다. `PopoverConfiguration`에 선택적 색상 필드가 추가되었으므로 이전 헤더로
컴파일한 C++ 소비자는 새 헤더와 함께 다시 빌드해야 한다. 설치된 Python 컴파일
API에서도 선언형 `primaryColor` 입력이 생성 코드로 이어지는지 검사한다.

## 팝오버 모션

기존 공통 팝오버의 동작을 기준으로, 열릴 때 불투명도는 160ms 동안 선형으로
증가하고 크기는 중앙을 기준으로 0.92에서 1까지 420ms `OutBack` 곡선과
1.45의 오버슈트로 변한다. 닫힐 때는 당시 크기를 유지하고 불투명도만 150ms
동안 줄인다. 각 시간은 공통 모션 설정의 속도와 축소 모션을 따른다. 닫힘 즉시
입력을 차단하되 퇴장 프레임이 끝날 때까지 그리기 상태를 유지한다. 닫히는 중
다시 열리면 현재 표시값에서 연속적으로 이어진다.

선언형 `motionEnabled`와 C++ `motion_enabled`는 기본 활성화이며 각 팝오버에
개별 적용한다. 실행 중 끄면 진행 중인 전환을 즉시 완료하고 다른 팝오버나 공통
환경설정은 바꾸지 않는다. 프레임 소스가 없는 호스트에서는 생성과 입력을
실패시키지 않고 전환을 즉시 완료한다. 명시적으로 끈 팝오버는 프레임 공급기
오류가 있어도 동작하며, 이후 전환을 켜려 할 때 오류를 전달한다. 프레임 소스
이외의 오류와 애니메이션을 지원하지 않는 동적 콘텐츠 오류는 호출자에게
전달하며, 속성 갱신 실패 시 마지막 유효 설정과 콘텐츠를 보존한다. 재질과
그림자도 콘텐츠와 함께 변환되며 퇴장 중 유지된다. 이 동작은 iiGUI의
`IIGUI_POPOVER_COMBINED_MATERIAL_MOTION_API`와 새 심볼이 필요하다. 채색된
밑그림에는 `IIGUI_POPOVER_UNTINTED_UNDERLAY_API`와 해당 심볼이 필요하다. 헤드리스
검사는 프레임을 모사하며 실제 운영체제 창이나 GPU 표시를 검증하지 않는다.

팝오버가 폐기되면 콜백·배경 공급자와 그라데이션 캐시를 해제한다. macOS arm64의
독립 iiGUI 설치 소비자와 LVUI 생성·모션·윈도우 악센트 검사를 창 없이 통과했다.
구형 표면 API에 대한 호환성 검사는 iiGUI에서 유지한다.
