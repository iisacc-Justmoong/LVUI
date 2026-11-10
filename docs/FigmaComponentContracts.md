# Figma 컴포넌트 선언 계약

공통 디자인 원본은 [Figma LVUI](https://www.figma.com/design/0GkItQYSNIR0lZ3iJhfJzc/LVUI)이다.
`catalog/figma-lvui-components.json`은 UI가 있는 23개 페이지의 컴포넌트 정의 119개,
속성 656개와 기본 인스턴스 참조의 이름을 기록한 스냅샷이다. 아이콘과 재질 색상은
별도 View 이름 수천 개가 아니라 아이콘 이름과 색상·재질 값으로 다룬다.

LVUI는 `{}` 안에 자식 View와 이름 붙은 속성을 함께 받을 수 있다. Figma의 선택지는
가능한 경우 접두 점 없이 쓸 수 있으며 기존의 점 표기도 유지한다.

```text
Window {
    VStack {
        PushButton { type: LabelButton, kind: accent }
        Slider { type: Filled, size: Mini }
        Checkbox { state: On }
        Icon { name: "javascript" }
        Material { type: Window }
    }
}
```

`type`, `kind`의 값이 등록된 View 이름과 같더라도 속성 위치에서는 선택지로 읽는다.
동명의 변수와 C++ 바인딩이 있을 때는 바인딩을 우선하며, `onClick`과 다른 동작은
기존 C++ 객체와 메서드 연결 계약을 따른다. 이벤트로 결정되는 포인터·포커스 상태를
백그라운드 폴링이나 고정된 애니메이션으로 대체하지 않는다.

## 현재 연결과 기본값

- `Checkbox`, `Radio`, `Toggle`, `TextField`, `GeneralSearch`, `SmallList`,
  `ColorPickerPanel`은 각각 기존 iiGUI 기반 구현의 이름에 연결된다.
  Figma 표기의 `LabelSegementedControl`, `IconSegementedControl`도 기존 구현에 연결된다.
- `PushButton`과 `DropdownButton`은 `type`과 `kind`로 Figma 변형을 선택한다.
  `accent`는 기존 네이티브 `primary`에 대응한다. 기본 버튼 종류는 Figma의
  `default`, 텍스트 버튼의 라벨은 `Button`, 드롭다운 라벨은 `Open`이며 아이콘형의
  기본 아이콘은 Figma가 참조하는 `projectStructure`이다.
- 체크박스·라디오·토글은 명시된 `checked` 또는 `state`가 없으면 Figma의 `On`을
  사용한다. `state: On`과 `state: Off`는 `checked`에 연결되며 중복 입력은 거부한다.
- 슬라이더의 기본 크기는 `Mini`, 파일 카드의 기본 크기는 `Small`, 텍스트 필드의
  기본 표면은 `Rounded`, 시트의 기본 표현은 `Mobile Fit`이다. Alert에 버튼 수나
  버튼 내용이 지정되지 않았으면 Figma의 기본 버튼 수 2를 사용한다.
- `Avatar`의 기본 크기는 `Large`, `ColorPickerButton`은 `Small`이며 각각 `size`로
  Figma 크기를 선택할 수 있다. 아이콘 세그먼트의 방향도 `orientation`으로 선택한다.
- 카드·목록 항목의 `type`, 목록 항목의 `kind`, 텍스트 필드의 `style`과 `mode`,
  색상 선택기 `type`, 스테퍼·콤보박스 `arrow`와 `tone`, 탭 `style`, 카드 `state`,
  시트 `presentation`은 지원된 Figma 값에서 기존 네이티브 입력으로 변환한다.
  `Tab`의 아이콘은 기존 계약대로 명시적으로 전달해야 한다.
- `Icon { name: "javascript" }`는 Figma 이름을 소스 아이콘 `javaScript`에 연결한다.
  `Material`의 `type`은 `Panel`, `Window`, `Surface`를 지원한다.

명시적 속성은 기본값보다 우선한다. 동일 의미의 두 입력을 함께 지정하는 등 충돌이
있으면 컴파일 오류로 보고한다. Figma 시안의 예시 파일명과 설명은 아직 모든 카드나
목록의 앱 데이터로 자동 주입하지 않는다.

## 스냅샷 갱신과 현재 한계

Figma 변경을 검토한 뒤 스냅샷을 갱신하고 `python3 -B tools/update_figma_defaults.py`를
실행한다. `--check`와 `LVUI.Figma.DefaultCatalog`는 스냅샷과 생성 표의 불일치를
검출한다. 스냅샷 저장과 코드 생성은 실행 앱이나 네트워크 접근을 요구하지 않는다.

스냅샷에 기록된 모든 속성이 아직 선언·렌더러에 연결된 것은 아니다. 특히 데스크톱
`TabBar`의 Content/Equal/Scrollable 배치, 모바일 탭 최소화, Figma `SheetContent`
예시 구성, 세그먼트의 Count, 일부 상호작용 상태 및 카드·목록의 예시 속성과 기본값은 별도 구현이
필요하다. `InputMethodGuard`, `WindowChromeInteraction`도 독립 네이티브 View로 생성되지
않는다. 미지원 이름을 빈 위젯으로 등록하지 않는다.

검사는 선언의 네이티브 C++ 생성과 생성 코드의 컴파일을 대상으로 한다. 플랫폼 창,
실제 GPU 표시, Figma의 모든 상태에 대한 픽셀 일치까지 보장하지는 않는다.
