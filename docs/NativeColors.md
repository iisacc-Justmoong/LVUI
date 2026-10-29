# 네이티브 반응형 Color

Color 입력·일반 let/var·Static 스냅샷을 C++ 값과 Core Observable로 연결한다.
공통 RGBA 값 검증·HEX 해석·encoded 공간 합성은 iiColorManager::Values를 사용하며
Picker 공유 라이브러리나 GUI를 LVUI::Native의 의존성으로 추가하지 않는다.

직접 C++ 위젯 경계의 Color는 기존 WidgetColor이며,
iiColorManager Rgba와 명시적으로 encoded sRGB 계약을 제공한 값 사이의 채널을 검증한다.
이 값 계약은 네 개의 정규화된 부동소수점 채널을 요구하며 float·double 사이의 명시적인
변환을 지원한다. RGB 바이트 구조를 정규화된 색상으로 잘못 해석하지 않는다.
선형광 GPU Color는 자동으로 encoded 값으로 받아들이지 않는다.
채널 생성에서는 `Color(vm.red, .2, .3, 1)`과 같은 선행 소수점 수치도 허용하며,
`Color.primary`와 문맥상 `.primary` 선택자를 수치로 재해석하지 않는다.

```lvui
import vm ColorModel
var selected: Color = Color.primary
let current: Color = vm.ink
@Static let initial: Color = current
let sample: (Double) -> Color = vm.sample
let Swatch(ink: Color = .primary): View = VStack(backgroundColor: ink) {
    Label(text: "Color", color: ink)
}
Window {
    VStack {
        Swatch(ink: current)
        Swatch(ink: sample(vm.amount))
        Label(text: current.alpha)
    }
}
```

## 값·입력과 갱신

타입이 있는 입력은 정의 위치 기본값과 호출 위치 수신자를 유지하며, 지역 import가 같은
이름을 가려도 전달한 색상의 원본을 바꾸지 않는다. `red`, `green`, `blue`, `alpha` 채널을
읽을 수 있다. 백엔드의 기존 멤버·반복 행 관찰 경로는 유지한다. C++ SDK Rgba 입력은
명시적인 encoded 계약과 유한한 0~1 범위를 확인한 뒤 위젯 색상으로 변환한다.

가변 값의 초기화는 기존 LVUI 변수와 같이 활성화 때 한 번 선택한다. 재대입은 생성 C++
setter와 Core Observable을 통하며 유효하지 않은 Color 대입은 기존 값을 보존한다.
일반 let은 의존성을 추적하고 Static은 해당 활성화의 최초 값만 유지한다. 조건부 제거 후
다시 활성화하면 새 지역 객체와 새 Static 값을 만든다. 생성된 스칼라 setter는 위젯 수명과
네이티브 활성화의 접수 상태를 함께 확인한다.

`Color(value)`는 encoded 값 또는 색상 문자열, 네 인자는 0~1 RGBA 채널이다.
문자열은 `#RGB`, `#RGBA`, `#RRGGBB`, `#RRGGBBAA`, 기존 6·8자리 bare HEX,
`black`, `white`, `transparent`, `rgba(r,g,b,a)`를 지원한다. rgba 문자열의 RGB는
0~255이고 alpha는 0~1이다. 이 값 해석은 LVUI 소스·AST를 실행하는 해석기가 아니다.
토큰 이름·테마 모드·프로필은 계속 AOT에서 확정한다.

일반 조건식은 선택한 분기의 값과 의존성을 따르며 `composite`는 기존 UI 토큰과 동일한
encoded 공간 source-over이다. 선형광 이미지 합성과 혼동하지 않는다. 선언한 Color 함수
인자·결과는 같은 인코딩·범위 검사를 거친다. 임의 숫자와 GPU 선형 Color를 Color 함수
결과로 자동 해석하지 않으며 생성 C++에서 거부한다.

## 표시와 수명

Label 갱신은 색상만 바꾸고 텍스트·글꼴·기하·정체성을 유지한다. 패널은 공통 C++ 그리기
데이터를 사용하며, 최초에 설치한 paint provider의 상태만 바꾸고 다시 그리기를 요청한다.
그리기 데이터의 RGB는 선형광으로 변환하고 독립 알파와 보더·크기를 유지한다.
변경이 없는 값은 기존 네이티브 바인딩 비교로 갱신하지 않는다. 색상과 크기가 같으면
이전의 불변 그리기 데이터를 재사용하며 hover 같은 무관한 상태로 새 데이터를 만들지
않는다. 캐시는 해당 UI 실행 영역만 접근하고 색상·크기 변경 때만 다시 만든다.

잘못된 반응형 입력은 기존의 마지막 유효 표시를 보존하고 바인딩 실패에 기록한다.
정상 값으로 복구할 수 있으며 후보 활성화의 초기 입력이 실패하면 기존 화면을 보존한다.
제거·종료 후에는 구독·지역 객체를 퇴역한다. 색상 표시와 채널 읽기는 파일 리비전을
발행하지 않는다. 임의 C++ 쓰기는 기존 모델의 변경 발행 계약을 따른다.

창 생성 옵션의 appearance는 뷰 활성화보다 먼저 결정된다. ApplicationWindow 배경은
현재 상수 Color를 요구하며 동적 값을 정적 C++ 함수에 잘못 생성하지 않고 소스 진단으로
거부한다. 동적 창 appearance 계약은 전체 완료 감사의 남은 범위이다.

## 검증

`tests/colors.cpp`는 타입 입력·가변 상태·스냅샷·채널·정의 환경·View 목록·진단을
검사한다. `tests/native_colors.cpp`는 값·범위·선택적 의존성·실패 복구·함수·종료·스레드
경계를 GUI 없이 실행한다. `iiGUI/tests/native_colors.cpp`는 실제 생성 C++의 갱신·지역
수명·후보 실패 롤백·종료와 Label의 필드 보존, 패널 그리기 데이터를 검사한다.
메모리 검사와 별도로 생성 C++의 잘못된 인자·반환 타입도 컴파일에서 거부한다.

LVRS의 Link·AppCard·AppHeader·Toolbar·TableCell·Tooltip·Material·CodeEditor·Modal·
Sheet에 SDK 색상을 전달하는 팩터리는 중간 단계에서 컴파일만 한다. 실제 LVRS 앱·테스트
실행과 실제 창·GPU·접근성·IME·플랫폼 검증은 전체 작업의 최종 일괄 단계에 남긴다.

## 2026-10-26 검증 기록

macOS arm64의 현재 작업 트리에서 다음 결과를 확인했다. 범위가 겹치는 검사를 합산하여
독립 기능 수나 전체 플랫폼 지원 수로 표시하지 않는다.

| 범위 | 결과 |
|---|---|
| LVUI 전체 Release CTest | 193/193 통과 |
| 관련 언어·네이티브 구현 AddressSanitizer + UndefinedBehaviorSanitizer | 122/122 통과 |
| 작업·콜백·함수·색상 ThreadSanitizer | 32/32 통과, 빌린 Core Runtime 라이브러리는 비계측 |
| iiGUI 창 없는 기존 동작·생성 C++ 통합 | 105/105 통과 |
| iiGUI 구현·생성 C++·LVUI Runtime·Core AddressSanitizer + UndefinedBehaviorSanitizer | 105/105 통과, NativeBindings 구현은 별도 LVUI 검사 |
| LVUI 설치·재배치 독립 소비자 | Native 색상 값·공개 컴파일러·선택적 Runtime 3/3 통과 |
| iiColorManager 일반 검사와 메모리 검사 | 각각 8/8 통과, 설치 소비자 자동 검사 포함 |
| iiColorManager 값 전용·기존 Picker 설치/재배치 소비자 | 각각 1/1 통과 |
| 생성 C++ 타입 계약 | 유효 4개 컴파일, 잘못된 14개 거부 |
| LVRS 생성 C++ | 총 23개 팩터리 컴파일, 실행하지 않음 |

일반·메모리 검사 빌드의 생성 C++ 헤더 26개가 동일함을 확인했다. 색상 전용 패키지는
Picker 라이브러리 없이 설치·재배치하고 소비자가 컴파일·링크·실행한다. 전체 패키지의
기존 기본 검색과 Picker 연결도 별도로 확인했다. 색상 표시가 파일 리비전을 바꾸지 않는지,
관측 대상 Dreamscapes QML이 보존되는지도 검사한다.

로그·소스 해시·의존성·제품 비변경 기록은
`build/lvui-completion/native-colors-verification-20261026.json`에 보관한다. 이는
[전체 LVUI·LVRS 완성](Completion.md)이나 제품의 UI 지연 시간 개선율을 입증한 기록은
아니다. 동적 창 appearance와 남은 전체 타입·인터페이스·자원·플랫폼 계약은 유지한다.
