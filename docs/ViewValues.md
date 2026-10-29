# 재사용 가능한 View 값

`let`의 시각 초기값은 `View`로 명시하거나 추론할 수 있다. 선언은 빌드 시의 구성
정의이며, 소스 파싱이나 선언 자체가 위젯·지역 백엔드를 생성하지 않는다.

```lvui
import document Backend
let header: View = HStack {
    Label(text: document.title)
    PushButton(document.save, text: "Save")
}
Window {
    VStack {
        header()
        header()
    }
}
```

각 사용 위치에서 정적인 구성을 C++로 생성한다. 정의에 있는 값·import는 정의한
어휘 스코프에서 찾으며, 사용한 위치에서 같은 이름을 가려도 다른 객체로 바뀌지 않는다.
정의 안에 지역 import가 있으면 각 활성화가 독립된 객체를 소유한다. 같은 부모 import를
참조하면 기존 부모 인스턴스를 빌린다.

`let copied = header`는 같은 구성 정의를 다른 이름에 연결한다. `copied()`를 사용해도
위젯 객체를 공유하지 않는다. 명시한 자식 ID에는 사용 위치별 이름 영역을 부여하여 두
활성화가 서로 충돌하지 않게 한다. 메뉴·툴팁의 지역 `targetId`도 같은 영역에서 해석한다.
`TabBar` 같은 타입 제한 자식 입력은 정의가 실제로 구성하는 뷰의 타입으로 검사한다.

창을 값으로 정의한 뒤 루트에서 사용할 수도 있다.

```lvui
let app: View = Window(width: 720) { Label(text: "Native app") }
app()
```

호출 위치에서 이름이 있는 입력과 기본 자식 콘텐츠를 덮어쓸 수 있다.

```lvui
import document Backend
let heading: View = Label(text: "Untitled")
let save: View = PushButton(document.save, text: "Save")
let panel: View = VStack { Label(text: "Default content") }
Window {
    import document OtherBackend
    VStack {
        heading(text: document.title)
        save(onClick: document.save)
        panel() { Label(text: document.title) }
    }
}
```

전달한 입력·콜백·자식 콘텐츠는 **호출 위치**의 어휘 스코프를 사용한다. 정의 내부의
import가 같은 이름을 가려도 전달한 수신자가 바뀌지 않는다. 입력은 반응형 C++ 식으로
생성하며 콜백은 지연 참조·종료 접수 경계를 유지한다. 자식에 호출 위치에서만 보이는 View
값을 사용할 수 있으며, 그 View 자체가 포획한 정의 스코프도 유지한다. 입력은 원래 루트
컴포넌트의 계약으로 검증한다. 자식 블록은 기본 자식 목록을 대체하며 선언 자체를 삭제하지
않는다. 창을 값으로 사용한 루트도 이름 있는 입력과 기본 콘텐츠를 받을 수 있다.

명시적인 [타입 매개변수와 이름별 슬롯](Components.md)은 `let Name(...): View` 서명으로
별도 제공한다. 위의 서명 없는 View 값 덮어쓰기와 구분한다. 전달한 자식의
지역 선언은 `VStack { import ... }`처럼 자체 View 안에 둔다. 호출 본문의 직접 지역 선언,
이름 없는 호출 입력, 조건부 View 값의 분기 입력 덮어쓰기는 명시적으로 거부한다.
변경 가능한 `var View`와 `@Static View`의 깊은 스냅샷은 남아 있다.
[여러 뷰의 구성](ViewSequences.md)은 `[View]` 값·매개변수로 전달할 수 있으며,
빈 목록과 중첩 목록도 원래 부모에 직접 확장한다. 단일 View 슬롯과 타입을 구분한다.
안정적인 키를 유지하는 구성은 [네이티브 반복](Repetition.md)을 따른다.
지원하지 않는 형식을 조용히 무시하지 않는다.
확장은 64단계, 생성 위젯은 기존 4,096개 한계를 유지한다.

`LVUI.ViewValues.*`는 AST·포획·이름 가림·ID·자식 타입·루트·진단을 검사한다.
`LVUI.NativeViewValues`는 실제 생성된 C++를 컴파일·링크하고 UI 호스트 없이 두 지역
객체의 독립 상태·파생값·스냅샷·종료를 실행한다. LVRS 위젯의 실행은 최종 일괄 단계이다.

`iiGUI.native_view_inputs`는 실제 AOT 결과를 창 없는 iiGUI 위젯 상태에 연결하여 전달한
값·콜백·자식·호출 위치의 지역 View 정의·반응형 갱신·퇴역 후 해제를 검사한다. 이는 실제
창·입력·그리기나 LVRS 전체 컴포넌트의 최종 실행 검증과 구분한다.
