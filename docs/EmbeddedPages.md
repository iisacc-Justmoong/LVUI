# 기존 네이티브 트리에 뷰 구성

AOT 생성 클래스의 `build(WidgetBuilder&, Widget parent = {}, configure, scope_owner)`
는 새 Host나 마운트를 만들지 않고 현재 빌더에 뷰 본문을 구성한다. import나
반응형 var가 없는 뷰는 configure와 scope_owner 인자를 생성하지 않는다.

```cpp
return lvui_generated::ReportPage::build(widgets, parent,
    [&](auto& scope) { scope.backend_0.title.set("Report 42"); });
```

최상위 LVUI 문서는 기존처럼 Window 또는 ApplicationWindow와 자식 한 개이다.
`build`에서는 창 옵션을 사용하지 않고 자식 본문만 구성한다. 부모를 생략하면
독립 콘텐츠 빌더의 루트가 되며, 명시하면 해당 빌더 안의 부모 아래에 붙는다.
각 호출은 독립된 Scope와 BindingScope를 생성하여 해당 위젯 세대에 retain한다.
정적 `mount`와 인스턴스 `attach`도 같은 생성 함수를 재사용한다.

LVRS PageRouter는 C++ 백엔드의 `std::shared_ptr<lvrs::PageRouter>`를 LVUI 입력
`router: vm.router`로 받는다. 다른 경로의 페이지는 각각 빌드 시 생성한 C++의
`build`를 호출한다. 런타임 소스 파싱, 인터프리터, 추가 OS 위젯 계층은 없다.

임베디드 페이지에서 초기 `open: true` 팝업은 `unsupported`이다. 페이지 활성화
이후 명시적으로 열어야 한다. `build` 자체는 상위 트랜잭션을 소유하지 않으므로
교체 원자성이 필요하면 iiGUI content_host 같은 독립 세대 호스트 안에서 호출한다.
직접 같은 빌더에 여러 번 구성할 때 ID 유일성·루트 하나 계약은 호출자 책임이다.

`tests/compiler.cpp`에는 생성 진입점·부모 전달·스코프 유지·초기 팝업 거부와
PageRouter 입력 진단을 추가했다. 실제 생성 코드의 C++ 컴파일은 LVRS의 독립
페이지 예제와 설치 소비자 빌드에서 확인한다. 모든 테스트와 앱의 실행은 사용자가
요청한 전체 작업의 최종 일괄 단계까지 보류한다.
