# 타입 지정 쿼리 연결

Society의 파일 작업 완료, 모델 변경, 계정·장치 알림처럼 하나의 생산자가 여러 뷰에
전달하는 사건은 iiSDKCore `QueryStream`으로 표현한다. 기존 `query()`는 단일 요청과
응답을 담당하며, 스트림은 명시적으로 선택한 타입 지정 값의 다중 수신을 담당한다.
별도의 전역 이벤트 버스나 Qt 런타임에 의존하지 않는다.

## 두 객체 노드의 양방향 쿼리

C++ ViewModel이 `ii::runtime::QueryPeer`를 멤버로 가지면 한 쪽은 `.lvui`의 기존
`query(vm.peer, vm.request())`로 상대에게 쿼리를 시작할 수 있다. 상대도 자신의 피어에서
별도 쿼리를 시작해 ViewModel의 수신 처리기를 호출한다. 수신 처리기가 관찰 가능한
ViewModel 상태를 변경하면 LVUI 바인딩이 화면을 갱신한다. 요청·응답 타입은 두 방향이
서로 달라도 되며, 한 방향의 응답을 역방향 요청으로 취급하지 않는다.

[`NativeQueryPeer.lvui`](../tests/fixtures/NativeQueryPeer.lvui)와
[`NativeQueryPeerFixture.h`](../tests/fixtures/NativeQueryPeerFixture.h)는 양방향 피어와
ViewModel을 함께 보여준다. `LVUI.QueryPeer.Generated`는 버튼의 송신·응답, 상대가
독립적으로 시작한 수신과 뷰 바인딩, 뷰 퇴역 시 대기 쿼리 취소를 검사한다.
Python의 `ii_lvui.compile_native`도 같은 선언을 컴파일한다. 이는 Python에서 Core
피어 객체를 생성·구독하는 별도 런타임 바인딩이 있다는 뜻은 아니다.

이 예제의 `query()`는 같은 실행 영역의 동기 호출이다. 서로 다른 객체의 `SerialQueue`를
넘는 운영 코드는 ViewModel C++에서 피어의 비동기 `post`를 사용하고 결과를 UI 실행
영역에 전달해야 한다. 피어 연결·종료·큐 전달의 상세 계약은
[Core 양방향 쿼리](../../iiSDKCore/docs/query.md#대등한-양방향-연결)를 따른다.

## 뷰 선언

```lvui
import vm NativeQueryBackend
Window {
    var count: Int = 0
    VStack {
        QueryConnection(source: vm.events, query: vm.selection, value: "message",
                        capacity: 2, deliveryBatch: 1, overflow: .dropOldest,
                        onValue: { count = message.number })
        Label(text: count)
        PushButton(text: "+", onClick: { count = count + 1 })
        QueryConnection(source: vm.stream(), query: vm.selection, onValue: vm.changed)
    }
}
```

`source`는 스코프가 소유하거나 안전하게 차용한 C++ 객체의 직접 공개
`QueryStream` 멤버 또는 유효한 스트림 참조를 반환하는 인자 없는 접근자이다.
구독이 유지되는 동안 같은 스트림과 그 소유자가 살아 있어야 한다. 값이 교체될 수 있는
반복 행의 객체를 스트림 소유자로 직접 지정하는 것은 거부하며, 안정적인 ViewModel
스트림에서 행 식별자를 선택하거나 객체 교체 시 연결을 소유한 뷰를 다시 활성화한다.
`query`는 스트림이 선언한 선택 타입이다. `Observable` 등으로 선택이 바뀌면 기존
구독의 선택을 교체하고, 이전 선택으로 아직 전달되지 않은 값은 버린다. 각 선언은
별도 구독이므로 하나의 생산자에 여러 수신자를 연결할 수 있다. `value`에는 이벤트
블록에서 읽을 페이로드 이름을 지정한다. 이름 없이 `onValue: vm.changed`를 사용하면
메서드가 `const Output&`를 받거나 인자를 받지 않을 수 있다. `() -> Void`로 선언한
콜백에도 페이로드를 전달하지 않는다. 여러 인자는 소유권이 명확한 Output 구조체로
묶는다. 페이로드의 일반 C++ 필드는 읽을 수 있으나 뷰에서 C++ 객체 필드를 직접
수정하지 않는다. C++ 수신 함수의 `const Output&`는 호출 중에만 유효하므로 이후
작업에 필요하면 값을 복사하거나 불변 공유 소유권을 명시적으로 전달한다.

Core 스트림의 구독 슬롯은 기본 16개이며 `QueryStreamOptions.max_subscriptions`로
최대 4096개까지 설정한다. 한도를 넘으면 새 연결은 실패한다. 호스트 관리자의 전체
연결 한도는 기본 4096개이며 생성 시 최대 65536개까지 정할 수 있다.

각 선언의 `capacity`와 `deliveryBatch`는 1–65536의 정수이며 기본값은 각각 64이다.
`overflow`는 기본 `.reject` 또는 가장 오래된 대기 항목을 버리는 `.dropOldest`이다.
이는 생산자·수신자 사이의 명시적 유한 자원 계약이다. `source`, `query`, `onValue`는
필수이다. `QueryConnection`은 보이는 위젯을 만들지 않으며 `VStack` 등의 자식이나
단독 조건부 분기에 둘 수 있다. 전체 컴파일·실행 경로의 실제 예제와 백엔드는
[`NativeQueryConnections.lvui`](../tests/fixtures/NativeQueryConnections.lvui)와
[`NativeQueryConnectionsFixture.h`](../tests/fixtures/NativeQueryConnectionsFixture.h)이다.
[`NativeQueryScene.lvui`](../tests/fixtures/NativeQueryScene.lvui)는 애플리케이션 소유
백엔드와 Scene 지역 상태의 연결을 보여준다. Python에서는 기존
`ii_lvui.compile_native(source)`로 같은 선언을 네이티브 C++로 생성할 수 있다.
Core `QueryStream` 자체의 Python 런타임 구독 바인딩은 이 구현에 포함되지 않는다.

## 호스트, 수명과 복구

```cpp
auto registry = lvui::native::QueryConnections::create(loop);
if (!registry) return std::unexpected(registry.error());
auto factories = lvui::native::BackendFactories{}
    .with_query_connections(*registry);
auto mounted = GeneratedView::mount(host, {}, nullptr, factories);
if (!mounted) return std::unexpected(mounted.error());
// 호스트가 위젯을 제거하고 생산자를 종료한 뒤 (*registry)->close()의 완료를 확인한다.
```

`loop`는 해당 호스트가 사용하는 `ii::runtime::EventLoop`와 같은 인스턴스여야 한다.
생성된 Application 경로에서는 `app.event_loop()`로 같은 관리자를 만든 뒤
`configure_application(app, {}, factories)`에 정책을 전달할 수 있다. 네이티브 앱은
`LVUI::Native`와 `iiSDKCore::Runtime` 및 iiGUI를 링크한다. 관리자 생성·연결·선택
변경·통계·재시도·퇴역은 UI 소유 스레드에서 호출한다. 생산자의 `publish()`는
다른 스레드에서 호출할 수 있으며 선택기는 그 발행 스레드에서 실행된다.
C++의 `connect` 수신 함수는 이동 전용 캡처를 소유할 수 있으며 빈 함수는 거부한다.

뷰 연결은 생성된 C++에서 생산자의 공개 스트림과 선택 쿼리를 사용해 UI EventLoop에
구독한다. 수신자는 현재 뷰 세션과 바인딩의 수명을 따르며 입력된 값은 쿼리 계약이
소유한다. 분기 제거 또는 부모 퇴역 후 대기 중인 이벤트는 사용자 콜백을 호출하지
않는다. 처리 중 뷰가 스스로 제거되거나 다른 스레드에서 선택기가 실행 중인 경우에도
UI 스레드에서 수신 종료를 기다리지 않으며, 구독의 종료 장벽이 완료될 때까지 필요한
소유자와 캡처를 보존한다. 제한된 큐의 수용·거부와 재시도는 Core 계약을 따른다.

호스트는 UI EventLoop마다 `QueryConnections`를 소유하고 뷰의
`BackendFactories`에 전달한다. 관리자는 정리 신호 슬롯 하나를 미리 확보한다.
이벤트 루프의 남은 용량과 각 구독 큐는 여전히 유한하다. 생산자는 `publish()` 결과의
`rejected`, `dispatch_failed`, `projection_failed`를 확인해야 한다. UI 호스트는
`QueryConnections::stats()`에서 활성 구독별 통계와 마지막 콜백 예외를 확인하고
`resume()`으로 대기 중인 구독을 일괄 재시도할 수 있다. 반환 순서는 활성 등록
순서이며 뷰 교체 후 안정적인 식별자가 아니다. C++에서 직접 연결한 구독은 해당
핸들의 `stats()`와 `resume()`도 사용할 수 있다. 생산이 중단된 채 EventLoop의 접수가
실패하면 공간이 생긴 후 명시적 재시도가 필요하다. 거부되거나 버린 값은 재생하지
않는다. 앱 종료 시 뷰를 제거하고 `close()`가 완료될 때까지
루프를 유지해야 한다. 생산자가 아직 실행 중이면 `close()`는 기다리지 않고 false를
돌려주며, 호스트는 생산자를 종료하고 `poll()` 또는 루프 전달로 정리를 완료한다.
루프가 중단된 뒤에는 예약된 신호를 전달할 수 없으므로 호스트가 직접 `poll()`한다.
관리자가 연결을 보유한 채 파괴되면 수명 계약 위반으로 프로세스를 종료하므로,
종료 결과가 false인 관리자를 생산자보다 먼저 파괴해서는 안 된다.

전달은 각 수신자의 UI EventLoop에서 순차적으로 수행된다. Qt의 직접 연결처럼 발행 호출 안에서
동기 실행된다고 가정해서는 안 된다. 선택기와 접근 정책은 생산자가 정의하고, 송신자
식별이나 객체 전체를 자동으로 페이로드에 추가하지 않는다. 요청·응답과 도메인 오류는
기존 `QueryPort` 계약으로 구분한다.

기존 QObject 시그널을 자동으로 변환하거나 등록하는 기능은 없다. Society의 각 생산자는
도메인 소유 스트림과 선택기를 제공하고 뷰는 필요한 연결을 선언해야 한다. 구독 이전의
이벤트를 재생하지 않으며 초기 화면 값은 별도의 상태·읽기 쿼리로 구성한다. 이 계약은
Qt `DirectConnection`의 동기 재진입이나 무제한 이벤트 큐와 동등하지 않다.
예를 들어 Society `FilesBrowser.qml`의 활성화·폴더 변경·실패 알림은 각각 타입 지정
스트림 또는 명시적인 구분자를 갖는 공통 Output으로 매핑할 수 있다. `Connections`의
`enabled`는 조건부 분기의 연결 수명으로 표현하고, 교체되는 디렉터리 객체 자체 대신
안정적인 ViewModel 스트림과 경로 선택 쿼리를 사용한다. ViewModel이 실제 소스 변경과
오류를 발행하도록 옮기는 작업은 해당 앱의 책임이다.

검사는 `LVUI.QueryConnections.Native`, `LVUI.QueryConnections.Generated`,
`LVUI.ViewSessions.connections`, `iiSDKCore.QueryStream`, `iiSDKCore.Query`에서
다중 수신, 선택 변경, 제한과 재시도, 다른 스레드의 발행, 뷰 퇴역 및 자기 제거를 다룬다.
`LVUI.NativeInstalled`는 위치를 옮긴 LVUI 패키지의 공개 헤더와 Core Runtime만으로
별도 소비자가 연결을 빌드하고 송수신·퇴역하는 경로를 검사한다.
