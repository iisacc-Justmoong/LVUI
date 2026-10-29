# 네이티브 공통 표시 속성 opacity

Dreamscapes의 레이어 행·비활성 도구·타임라인 항목은 `opacity`를 사용한다. 이 관측을
바탕으로 LVUI의 네이티브 컴포넌트에 공통 입력 `opacity`를 추가한다. Dreamscapes의
QML이나 파일 상태 모델을 수정하거나 자동 변환하는 기능은 아니다.

```lvui
import vm LayerModel
Window {
    VStack(opacity: vm.visible() ? 1 : 0.5) {
        Label(text: vm.name)
        PushButton(text: "Select", onClick: vm.select)
    }
}
```

상수는 빌드 시 범위와 타입을 검사하며, 반응형 값은 iiSDKCore 관측과 기존 NativeBindings를
통해 C++ `Widget::set_opacity`로 전달한다. 허용 범위는 0부터 1까지이고 기본값은 1이다.
잘못된 반응형 값은 기존 표시를 유지한 채 binding 실패를 기록하며 이후 정상값으로 복구한다.
일반 숫자 입력·C++ 조회·공유 구성 입력과 `@Static`의 기존 의미를 유지한다.

`VStack`, `Label`, 버튼·입력·진행 컨트롤 및 LVRS 계층 행을 포함한 네이티브 컴포넌트의
루트 위젯에 적용한다. 계층 `itemDelegate`와 `countView`도 동일한 표시 트리로 연결한다.
사용자가 선언한 View에는 필요한 매개변수를 정의하여 전달한다. `Window`의 불투명도는
위젯 투명도와 운영체제 창 합성이 다르므로 이 입력으로 받지 않는다.

부모와 자식의 투명도는 곱한다. 색상 alpha나 파일의 레이어 opacity를 다시 쓰지 않는다.
0은 표시만 생략하며 입력·포커스·선택·콘텐츠 수명을 유지한다. 따라서 비활성 정책이
필요하면 `enabled`를 별도로 지정한다. 투명도 변경을 위해 뷰를 교체하거나 C++ 명령을
호출하지 않는다. 원본의 전체 상속·모션·플랫폼 계약 완료를 의미하지 않는다.

검사는 컴파일러 8개, iiGUI 자체 위젯 9개, 동일한 생성 C++를 사용하는 iiGUI/LVRS 사례
각 8개로 구성하며, LVRS에는 계층 행과 count 슬롯의 투명도 보존 1개를 추가한다.
LVRS 실행은 저장소 지침에 따라 전체 작업의 최종 일괄 단계에 모은다.
창 없는 C++ 그리기 검사는 실제 OS 창·GPU 픽셀 또는 플랫폼 동등성 증거와 구분한다.
