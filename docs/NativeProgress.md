# 네이티브 진행 상태 뷰

> 2026-10-29: 이 문서의 `FRAMEWORK LVRS` 생성 경로와 LVRS 팩터리 검사 설명은
> 제거된 경로에 대한 역사적 기록이다. 현재 지원 범위와 남은 이식은
> [독립 구현 현황](LVUIIndependence.md)과 [이전 우선순위](NativeMigrationRoadmap.md)를 따른다.

## LVUI 독립 구현

기본 네이티브 생성 경로의 `ProgressBar`는 `LVUI/ProgressBar.h`의 독립 상태와
도형 계산을 사용한다. 앱에 생성하는 호스트 접착 코드가 사각형 세 개를 그리며,
상태 변경은 기존 위젯을 유지하고 다시 그리기를 요청한다. 기본 크기는 100×6이고
시작값과 현재값의 정규화 구간을 채운다. 직접 C++ API는 역방향 범위, 유효하지 않은
값의 원자적 거부, 0 크기, 작은 크기의 반경과 테두리를 처리한다.

독립 경로는 `minimumValue`, `maximumValue` 또는 `endValue`, `startValue`,
`currentValue`, `width`, `height`, `largeHeight`, `regularHeight`, `size`,
`shapeStyle`, `cornerRadius`, `trackColor`, `fillColor`, `displayedStart`,
`displayedProgress`를 지원한다. 각각 상수 또는 C++ 반응형 값으로 지정할 수 있다.
정방향·역방향 범위를 처리하며, 두 maximum 이름을 동시에 지정하면 진단한다.
표시 시작과 길이는 화면에 맞게 0–1 범위로 제한한다.

독립 경로의 `stateModel`은 LVUI의 `ProgressState` 스냅샷 또는
`ProgressStateSource` 인터페이스를 사용한다. 앱이 `ii::core::Observable`로 스냅샷을
제공하면 상태 변경과 반응형 키 변경을 함께 관찰한다. 네 개의 `...StateKey`는 기본
키를 바꾸며 앞뒤 공백을 제거한다. 비어 있는 키는 기본 키로, 존재하지 않는 값이나
비어 있는 선택적 스냅샷은 직접 입력값으로 대체한다. 소스 값이 유한하지 않거나
허용 범위를 벗어나면 전체 후보를 거부한다. 레거시 LVRS 상태 모델은 이 인터페이스와
별개이다. 객체 포인터를 공급하는 앱은 내부 변경을 관찰 가능한 스냅샷 변경으로
전달해야 하며, 포인터 내부의 일반 C++ 변경은 자동 감지되지 않는다.

동적 값은 하나의 LVUI 구성 후보로 계산·검증한 뒤 기존 위젯에 반영한다. 잘못된
색상이나 크기는 이전 상태를 유지하며, 정상 값으로 복구할 수 있다. 기본 독립 경로는
`motionEnabled`와 LVUI `Motion` 값을 받아 보간하며, 프레임 시각은 호스트에서
주입한다. 프레임 소스가 없는 상태에서 애니메이션을 시작하면 목표에 정착하고 오류를
보고한다. 자세한 정책과 직접 C++ API는 [네이티브 모션](NativeMotion.md)을 따른다.
아래 레거시 경로의 전체 동작 대체 여부는 [독립 구현 현황](LVUIIndependence.md)에서 관리한다.

독립 모델 검사는 정방향·역방향·경계값·잘못된 입력과 작은 크기의 도형을 확인한다.
범용 호스트의 의미 정보 접점에 `progress` 역할, `Progress` 이름, 0–1로 정규화한
현재 목표값을 전달한다. 역방향 및 동일한 범위도 같은 정규화 규칙을 사용한다.
애니메이션 중 접근성 값은 화면의 중간 프레임이 아니라 모델의 현재 목표값이다.
이 경로는 `set_accessibility_semantics`를 제공하는 iiGUI 호스트가 필요하다.
생성된 C++는 LVRS 헤더 경로 없이 컴파일한다. 창을 만들지 않는 호스트 검사는 초기
track·fill 그리기, 여러 반응형 입력의 일괄 변경, 잘못된 후보의 거부·복구,
상태 스냅샷과 키의 일괄 변경, 누락 값 대체, 위젯의 동일성 및 종료 시 수명 정리를 확인한다.
이는 실제 OS 창이나 GPU 렌더링 검증은 아니다.
검사에 사용한 iiGUI content host는 최상위 자식을 표시 영역 전체로 배치하므로,
명시한 폭을 자연 크기로 검사할 때는 진행 표시기를 스택의 자식으로 배치한다.

## 기존 LVRS 생성 경로

`FRAMEWORK LVRS`의 `ProgressBar`는 LVRS의 유지되는 C++ `ProgressBar`와
`ProgressModel`을 생성한다. 기본 iiGUI 프레임워크의 간단한 progress API와 구분한다.
계산·상태 제공자 관측·그리기·모션은 네이티브 SDK가 수행하며 QML이나 런타임 문자열 해석을 사용하지 않는다.

```lvui
import state lvrs.StateModel
Window { ProgressBar(stateModel: state, size: .regular, shapeStyle: .shapeCylinder) }
```

직접 scoped import는 객체를 복사하지 않고 활성화의 수명을 차용한다. 사용자 C++ 백엔드의
shared/weak StateSource, 변경 가능한 제공자 선택, 타입 지정 View 입력 전달도 지원한다.
제공자의 내부 revision 변경은 LVRS 모델 관측 경로로 처리한다.

## 입력

| 영역 | 입력 |
|---|---|
| 직접 값 | minimumValue, maximumValue 또는 endValue, startValue, currentValue |
| 상태 제공자 | stateModel, minimumValueStateKey, maximumValueStateKey, startValueStateKey, currentValueStateKey |
| 크기·모양 | size, shapeStyle, width, height, largeHeight, regularHeight, cornerRadius |
| 색상 | trackColor, fillColor |
| 모션 | motionEnabled, motion |
| 표시값 직접 제어 | displayedStart, displayedProgress |

`size`는 `.large`/`.regular` 또는 정수 0/1이며 `shapeStyle`은
`.shapeRoundRect`/`.shapeCylinder` 또는 정수 0/1이다. C++ 제공자는 대응하는 native enum
또는 정수를 반환한다. `motion`은 공유 native Motion이다. maximumValue와 endValue를
동시에 지정하면 소스 위치가 포함된 진단이다. 읽기 전용 progress 같은 계산값을 입력으로
허용하지 않는다.

모든 입력은 상수 또는 C++ 반응형 식을 받는다. 동적인 여러 입력은 하나의 native 옵션
후보로 계산하여 한 번에 적용한다. 따라서 minimum·maximum·상태 키·제공자가 함께
바뀌어도 중간 구성이 모델 수신자에게 노출되지 않는다. 값과 스타일 변경은 같은 위젯·모델을
유지한다. 실패한 후보의 오류와 모델 반영 이후의 수신자·프레임 접수 오류는 구분한다.

스코프 수명, 기본 크기·색상·OutBack 모션과 남은 원본 계약은
[LVRS ProgressBar](../../LVRS/docs/native/ProgressBar.md)를 따른다.
위젯 ID로 읽기 전용 상태를 참조하는 일반 언어 계약과 전체 Item 상속 API를 완료했다고
간주하지 않는다.

## 검증

`tests/progress.cpp`는 기본 경로·원본 입력·반응형 생성·scoped import·진단을 검사한다.
`NativeProgress`, `NativeProgressScoped`, `NativeProgressForwarded`는 실제 생성 C++의
컴파일·링크 대상이다. LVRS 실행 검사는 전체 목표의 최종 일괄 단계에 수행한다.
Dreamscapes는 관측 기준으로 유지하며 제품 소스를 변환하거나 설치하지 않는다.

2026-10-26에는 일반·ASan/UBSan 구성에서 LVUI 컴파일러 검사 34개씩을 통과했다.
기존 호출을 기대한 회귀 검사 한 개는 새 네이티브 컨트롤 호출을 검증하도록 갱신한 뒤
다시 통과했다. LVRS 팩터리 62개를 각 구성에서 생성·컴파일했고 생성 코드는 동일하다.
이 기능을 포함한 재배치 소비자의 컴파일·링크 및 아직 실행하지 않은 LVRS 검사 38개의
등록은 `SDK/LVRS/build/lvui-completion/progress-bar/verification-20261026.json`에 기록한다.
