# SDK Core 값의 네이티브 표시

SDK 모델이 제공하는 `ii::core::Value`는 실행 중 소스를 해석하는 동적 언어 값이 아니라
소유된 C++ 데이터이다. NativeValues는 이 값을 표시용 문자열과 명시적인 수치·Bool 입력에
연결한다. Qt QVariant나 JS 객체를 경유하지 않는다.

- text는 문자열·bool·정수·유한 실수를 표시한다. 64비트 정수 텍스트는 double로 바꾸지 않는다.
- null과 없는 optional Value의 텍스트는 빈 문자열이다.
- 배열·객체·바이트를 레이블에 넘기면 형식 오류이며, 유효한 기존 바인딩 값은 유지한다.
- number/range/integer는 실제 숫자 Value만 받는다. 숫자 문자열이나 Bool을 수치로 바꾸지 않는다.
- truth는 실제 Bool Value만 받는다. 임의 값의 truthiness를 사용하지 않는다.
- 네이티브 map의 문자열 키 조회는 find를 사용한다. 없는 키는 오류이며 데이터를 삽입하지 않는다.
- Core Value 객체의 문자열 키와 배열의 숫자 인덱스도 같은 읽기 전용 경계로 조회한다.
  Core Value가 키를 제공하면 실제 text/number 종류만 받는다. Bool·null·복합 키나 숫자
  문자열을 배열 인덱스로 바꾸지 않는다. 범위/종류 오류는 기존 표시값을 보존한다.
- 조회 결과는 소유된 Value 복사본이다. 메서드가 반환한 임시 객체·배열·중첩 데이터나
  C++ 목록 속의 Core Value에서도 부모가 사라진 뒤 유효한 결과를 유지한다.

Core Value는 일반 Numeric/String C++ 타입으로 선언되지 않는다. 이 표시·추출 경계를 넘어서
임의 variant 산술이나 구조체 문자열화를 암묵적으로 추가하지 않는다. 필요한 데이터 변환과
업무 메서드는 C++ 모델에 둔다.

```lvui
import vm NativeRegistryBackend
Window { VStack {
    Label(text: vm.registry.read_property("Editor", "displayName"))
    Label(text: vm.registry.descriptor("Canvas")["className"])
    PushButton(text: "Toggle", enabled: vm.registry.can_write("Editor"), onClick: vm.toggle_busy)
} }
```

메서드 내부의 Core 관측은 NativeMethods가 수집하며 모델 교체와 퇴역은 기존 스코프
수명 규약을 따른다. 레지스트리의 키별 쓰기 권한·별칭·모델 제공자 규약은
[LVRS ViewModelRegistry](../../LVRS/docs/native/ViewModelRegistry.md)를 따른다.

`tests/native_core_values.cpp`는 scalar 표시, 엄격한 수치·Bool 추출, 키 조회 비변경,
잘못된 데이터 이후 바인딩 복구를 검사한다. 이 검사는 LVRS를 실행하지 않는다.
실제 Registry/RegistryScoped 뷰의 생성 C++ 컴파일과 LVRS 위젯 조작 검사는 별도로 관리한다.

2026-10-27에는 관련 LVUI 검사 68개가 일반·ASan/UBSan 구성에서 각각 통과했다.
Core 값 표시·키 조회·오류 복구 검사 네 개도 여기에 포함한다. LVRS 실행 검사를 포함한
컴파일·링크·등록 상태는 `SDK/LVRS/build/lvui-completion/model-registry/verification-20261027.json`에
구분해 기록한다. Dreamscapes 관측 파일 81개는 변경하지 않았다.

플랫폼 서비스 연결에서 발견한 중첩 조회 누락을 추가로 보완했다. 현재 전용 검사는 기존
4개와 객체/배열·Core 키·임시 부모 소유권·중첩 바인딩 오류 복구 4개, 총 8개이다.
실행 결과와 전체 LVUI/LVRS 검사 경계는 플랫폼 단계의 검증 기록에 구분한다.


## 이름으로 조회하는 객체 속성

Core Value 객체와 문자열 키 C++ 맵은 `snapshot.frameCount` 같은 점 표기로 읽을 수 있다.
생성 코드는 속성 이름과 C++ 멤버 선택자를 함께 전달한다. 실제 C++ 객체는 원래 멤버의
참조·관측 계약을 유지하고, 맵은 존재하는 키를 조회해 소유 값을 복사한다. 없는 키를
삽입하거나 null·배열을 객체로 강제 변환하지 않는다. 중간 임시 스냅샷이 사라져도 선택한
값이 유효하며, 의존 값의 종류·키 오류는 마지막 유효 표시를 보존하고 다음 갱신에서 복구한다.

파생 let/var와 @Static의 자동 타입 판정도 같은 이름 조회를 사용한다. Core 데이터는
C++ 메서드 포인터로 해석하지 않으며 기존 C++ 메서드·callable 필드의 타입 및 수명 판정은
공유된 경계를 유지한다. 없는 키의 오류와 이후 복구를 저장 모드별로 검사한다.

이름 조회가 반환한 임시 데이터는 initial/read/read_source에서도 소유 값으로 전달한다.
기존 C++ lvalue의 참조 종류는 변경하지 않으며 임시 Observable을 소유 문맥 없이 자동
관측하는 경로는 허용하지 않는다. 임시 텍스트 뷰도 저장 가능한 문자열로 구체화한다.

임시 값의 반응형 읽기는 그래프에 등록된 실제 원천 수와 revision 발행 후의 재평가로
검사한다. ReadContext는 내부 관측 수집을 소유하므로 바깥쪽 수집 콜백의 호출 수를
구독 여부로 간주하지 않는다. 최초 검사에서 이 두 경계를 혼동한 실패 로그는 보존한다.
