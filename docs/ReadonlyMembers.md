# 읽기 전용 C++ 멤버와 네이티브 값

일반 LVUI 값은 반응형이며 읽기 전용 접근을 스냅샷과 동일하게 취급하지 않는다.
`let current = backend.selected`가 읽기 전용 객체를 가리키더라도 그 객체의 Observable
멤버는 생산자의 변경을 계속 관측한다. `const_cast`, 문자열 필드 검색, 전역 버스나
프레임별 폴링으로 이를 구현하지 않는다.

```text
import model ReviewModel
let current = model.selected
let alias = current
@Static let initialTitle: String = current.title
Window {
    VStack {
        Label(text: current.title)
        Label(text: alias.metrics.count)
        Label(text: initialTitle)
    }
}
```

`selected`가 `Observable<std::shared_ptr<const Item>>`을 제공하면 항목을 교체할 때
선택 의존성과 멤버 의존성을 다시 연결한다. 같은 항목의 title·metrics.count 변경은 해당
소비자를 갱신하며, 이전 항목의 변경은 새 선택을 갱신하지 않는다. 위 예의 initialTitle은
활성화 시 복사한 String 값이다. 객체 전체나 View 전체의 깊은 Static 스냅샷을 이 예의
증거로 대신하지 않는다.

## 타입·접근·발행

- Core의 `Observable::observe()`는 const 참조에서도 사용할 수 있다. 소유 스레드,
  구독 lease, 종료와 이동 규칙을 유지하며 const 접근에 수정 권한을 주지 않는다.
  사용자 C++ 관측자가 mutable observe만 제공하면 읽기 전용 멤버에서 조용히 스냅샷으로
  바꾸지 않고 빌드 진단을 낸다. 고정값은 명시적인 Static 또는 스냅샷 인터페이스로 선택한다.
- 네이티브 멤버 선택은 값·원시 포인터·표준 스마트 포인터 및 `reference_wrapper`의
  실제 C++ const 자격을 보존한다. nullptr은 명시적인 오류이다.
- 일반 member 경로와 `(model.selected).metrics.count` 형태의 member AST 모두 같은
  `member(context, object, selector)` 및 `read_source()` 경로로 생성한다.
- private 멤버·private 조상·없는 멤버는 생성된 C++의 정상 접근 제어로 거부한다.
- 동등 비교가 없는 C++ 구조체도 네이티브 let·추론한 var의 값으로 사용할 수 있다.
  Core Observable 값은 복사 생성과 예외 없는 이동 대입을 제공해야 한다. 비교 가능한
  값은 기존 no-op 규칙을 따르며, 비교가 없는 값은 각 set을 명시적인 발행으로 처리한다.
- 일반 public 멤버의 직접 수정은 자동으로 가로채지 않는다. 소유자가 제공하는
  Observable 또는 명시적인 스코프 발행을 사용한다. var는 초기화 이후 자체 상태이며
  초기화 식의 객체 변경을 자동으로 따라가지 않는다.

읽기 전용 참조의 수정을 막는 것과 값의 복사·소유권을 구분한다. 표준 참조 래퍼와 원시
포인터의 대상은 기존 소유자가 계속 소유해야 한다. 선택한 shared_ptr은 공유 소유권을
제공하며 이를 값 복사와 혼동하지 않는다. 상위 UI가 Core에 역의존하지 않는다.

## 계산 중 소유권

C++ getter가 값을 읽은 뒤 선택 객체를 교체할 수 있다. 과거에는 이전 객체가 getter가
끝나기 전에 파괴되어, 의존 구독 또는 revision 검사에서 해제된 Observable에 접근했다.
이 경로는 ASan으로 재현했다.

ReadContext는 읽은 shared_ptr과 멤버 경로에서 만난 shared_ptr의 소유자를 해당 계산·
의존성 검사·적용이 끝날 때까지 보존한다. 소유권 보존은 한 번의 평가에 한정하며 별도
전역 레지스트리나 영속 객체 캐시를 만들지 않는다. 정상 선택 교체·오류·재진입·계산 종료
후에는 임시 보존을 해제한다. 이전 객체를 계속 필요로 하는 별도 사용자 소유자는 유지한다.

같은 control block의 보존은 중복하지 않는다. 평가 하나에서 최대 4096개 소유자를
보존하며 한도 초과는 명시적인 실패이다. 실패는 마지막 정상 출력을 보존하고 다음
유효한 변경에서 복구한다. 구독 전환 자체가 실패하면 이전 정상 연결을 보존한다. 이 수명 보호를 임의 원시 포인터나 사용자 정의
불투명 핸들의 소유권 자동 발견으로 확대하지 않는다.

## 검사와 범위

- `LVUI.NativeBindings.readonly*`: const 관측·선택 교체·재진입 수명·임시 소유자 퇴역·
  보존 한도·표준 참조의 네이티브 연결을 실행한다.
- `LVUI.NativeReadonlyValidation`: 실제 생성 팩터리에서 값·포인터·shared_ptr·참조를
  컴파일하고 private 멤버·조상·없는 멤버·잘못된 객체를 거부한다.
- `iiGUI.native_readonly.*`: 생성 C++를 실제 창 없는 위젯 상태에 연결하여 별칭·지역
  스코프·스칼라 Static·선택·참조·구조체 let/var·널 복구·퇴역·조건부 재활성화·member
  AST와 입력·포커스·파일 상태 보존을 실행한다.
- `iiGUI.layout_package`와 `LVUI.NativeInstalled`: 재배치한 설치 패키지의 공개 const
  관측·비교 없는 값·AOT 팩터리를 컴파일·링크하고 제한된 소비자를 실행한다.

이는 현재 지원하는 C++ 값과 참조의 계약이다. 일반 C++ 타입을 명시한 모든 View 입력,
사용자 정의 불투명 핸들의 보존·복제·비교 프로토콜, 가변 View·깊은 Static, LVRS 전체
동작과 실제 OS 입력·GPU·플랫폼 실행의 완료를 뜻하지 않는다. 전체 조건은
[완료 감사](Completion.md)에 유지한다.

## 2026-10-26 검증 기록

macOS arm64 현재 작업 트리에서 다음을 확인했다. 각 구성의 반복 실행을 서로 다른 기능
수로 합산하지 않으며, Core 전체 회귀 검사를 관측 테스트의 성공으로 대신하지 않는다.

| 검사 | 확인 결과 |
|---|---|
| Core Observable | 기본 build와 독립 일반·ASan·UBSan 구성의 해당 검사 각각 통과 |
| Core 새 패키지 | 일반·메모리 검사 구성에서 현재 Runtime·Embedded 빌드 및 스테이징 설치 |
| Core 구조 경계 | 두 독립 구성의 의존성 경계·sealed 공개 타입·다중 상속 검사 통과 |
| LVUI Release 전체 | 210/210 통과 |
| LVUI 관련 ASan·UBSan | 139/139 통과 |
| iiGUI 일반 창 없는 검사·패키지 | 150/150 통과 |
| iiGUI ASan·UBSan | 150/150 통과 |
| 새 생성 위젯 검사 | 각 구성 9건, 참조·선택·값·널 복구·퇴역·조건부 구성·입력 보존 포함 |
| 생성 C++ 접근·타입 | 각 구성 유효 4개 컴파일, 부적합 5개 거부 |
| 재배치 소비자 | LVUI 3건과 iiGUI의 일반·메모리 검사 구성 소비자 통과 |
| 생성 위젯 헤더 | 일반·메모리 검사 구성의 41개 내용 일치 |
| LVRS | 팩터리 41개를 두 구성에서 컴파일, 실행 미실시 |
| 제품 보존 | Dreamscapes QML 81개 해시 유지, 제품 변환·재설치 없음 |

먼저 읽기 전용 값의 갱신 중단을 평면·중첩 두 검사에서 재현했다. const 관측을 연결한 뒤
재진입 선택 교체에서 ASan heap-use-after-free를 확인했고, 평가 소유권 보존을 추가한
후 같은 경로를 포함한 메모리 검사를 통과했다. 기존 함수 값의 함수 포인터 소유권도
control block의 별칭으로 보존하며, 한 평가가 끝나면 해당 보존을 해제한다.

이는 생성 C++·창 없는 실제 위젯 상태·제한된 설치 소비자의 증거이다. OS 창이나 GPU
표면을 만들지 않았고 LVRS 앱·테스트는 저장소 지침의 최종 일괄 단계에 남겨 두었다.
iiGUI 메모리 검사에서 차용하는 일반 NativeBindings 아카이브의 구현은 별도 LVUI
ASan·UBSan 검사에 포함한다. 모든 상위 SDK나 플랫폼의 실행 완료로 확대하지 않는다.
