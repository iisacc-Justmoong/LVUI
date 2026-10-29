# 중첩 뷰의 지역 스코프

뷰 본문의 지역 `let`·`var`·import는 해당 뷰를 구성하는 C++ 활성화에 속한다.
AOT는 이를 별도 스코프와 구성 함수로 생성한다. 부모의 값과 import는 같은 객체를
빌려 읽고, 지역 import는 자체 객체를 생성한다. 지역 이름은 부모 이름을 가릴 수 있다.

```lvui
import document Backend
Window {
    VStack {
        VStack {
            import inspector Backend
            let status: String = document.ready ? "Ready" : "Waiting"
            Label(text: status)
            PushButton(inspector.refresh, text: "Refresh")
        }
        Label(text: document.title)
    }
}
```

새로 그리거나 속성을 갱신할 때 import를 다시 만들지 않는다. 일반 정적 자식은 기존
위젯 세대와 함께 유지된다. 모달 콘텐츠의 구성 함수 안에 있으면 실제 콘텐츠 활성화 때
생성하여 닫을 때 그 세대의 연결과 함께 해제한다. 부모 객체를 자식의 새 import로 복사하지 않는다.

지역 ID의 메뉴·툴팁 연결은 그 지역 컴포넌트 안에서 정의한다. 바깥 지역의 임시 C++
변수 이름을 다른 구성 함수 안에서 참조하도록 생성하지 않는다. 같은 실제 빌더에 구성하는
정적 자식들의 ID는 계속 유일해야 한다. 독립 모달 세대는 기존의 ID 격리를 유지한다.

루트 Window 본문에서도 지역 선언과 import를 사용한다. 모듈 선언과 창 본문에서 같은
이름을 사용하면 본문의 이름이 가리되, 이미 생성한 모듈 파생값의 원본은 바뀌지 않는다.
타입 청사진을 가져온 뒤 `import instance Model`처럼 짧은 타입 이름으로 객체를 생성할 수 있다.

[조건](ConditionalViews.md)·[반복](Repetition.md)·[사용자 View 값](ViewValues.md)은 각
문서의 현재 생성·실행 범위를 따른다. 취소 가능한 비동기 작업과 종료 접수 게이트 전체의
수명 계약은 후속 검증 범위이다.
지역 스코프의 성공을 이 전체 수명 기능의 완료로 간주하지 않는다.

`LVUI.NativeNested`는 실제 생성된 C++를 컴파일·링크하고 UI 호스트 없이 세 단계의
부모 값 추적, 지역 객체 분리, 이름 가림과 독립 바인딩 종료를 검사한다. 위젯 실행은
전체 작업의 최종 일괄 검증 단계에 유지한다.
