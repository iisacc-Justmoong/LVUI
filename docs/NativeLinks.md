# 네이티브 Link 코드 생성

FRAMEWORK LVRS의 Link는 lvrs::Link::create를 직접 호출하는 C++를 생성한다.
자식은 같은 빌더와 기존 스코프에 구성하며 별도의 OS 컨트롤·소스 해석기를 만들지 않는다.
href/to 별칭, typed 경로/컴포넌트 대상과 Core Value, 네 가지 색상·밑줄을 지원한다.

```text
import vm Backend
Window {
    Link(text: vm.text, to: vm.href, navigator: vm.navigator,
         replace: vm.replace, underline: vm.underline, linkColor: vm.color,
         method: vm.log, methods: [vm.log, vm.after]) {
        HStack { Label(text: "Custom child") }
    }
}
```

href와 to 동시 사용, 잘못된 색상·tone·메서드 호출식은 소스 진단이다. router,
navigator, targetComponent, params 등은 C++ 공개 멤버와 직접 연결해 C++ 컴파일러가
타입을 검사한다. method/methods는 ButtonEvent 인자 또는 인자 없는 메서드 참조이다.
onNavigated는 bool, onNavigationFailed는 const ii::core::Error&를 받는다.

iiGUI binding_value/bind_member는 plain 멤버와 Core Observable을 구분한다. plain 값은
구성 때 읽고 Observable은 BindingScope에 구독한다. shared_ptr의 get을 관찰 모델로
취급하지 않는다. href/to·params·router·navigator·targetComponent·replace·underline·
tone·색상·text·enabled·visible와 typed methods 멤버는 해당 setter에 연결한다.
새 위젯 세대의 구독은 종료 시 해제하며 오래된 Link의 closed 갱신은 무시한다.

반응형 치수·패딩과 일반 상속 속성은 별도 이식 범위이다. 지원하지 않는 입력을
초기 상수로 동결해 성공 처리하지 않는다. 모든 Link 경계는
[LVRS 네이티브 Link](../../LVRS/docs/native/Link.md)에 정리한다.

tests/compiler.cpp의 lvrs_link 진단/생성 사례와 실제 두 페이지 예제의 C++ 빌드·설치
소비자 빌드로 생성 코드를 확인한다. 실제 테스트·앱·self-test 실행은 전체 작업의
마지막 일괄 단계로 보류한다.
