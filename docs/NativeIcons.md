# LVUI 독립 아이콘 자산

## 독립 아이콘 뷰 계약

현재 LVUI 원본의 독립 아이콘 인스턴스는 기본적으로 18×18 논리 픽셀이다.
이 값과 원본 컴포넌트 키는 `catalog/lvui-design-icon-geometry.json`에 보관하고
검증된 공통 헤더를 생성한다. 설치 패키지에도 원본 스냅샷을 포함한다.
iiGUI `IconView`는 이미지 배치·접근성·갱신과 위젯 수명을 소유한다. LVUI 생성
코드는 기본 크기와 디자인 자산을 앱에서 연결한다. 명시적으로 크기를 전달할 수 있으며, 현재 공통 아이콘 캐시의
정책과 같이 논리 크기의 두 배 픽셀로 래스터화한다. 이 고정 배율은 기기별
화면 배율에 대한 실제 검증이나 자동 대응을 뜻하지 않는다.

이름, 크기, 선택적 자산 공급자와 접근성 이름을 변경할 수 있다. 잘못된 후보는
기존 아이콘을 보존하며 다른 스레드의 변경과 폐기된 뷰의 변경을 거절한다.
공급자를 지정하지 않으면 현재 디자인 카탈로그와 그 이전 자산 폴백을 사용한다.
런타임 위젯의 입력·접근성·크기 변경과 오류 복구는 창 없이 검사한다.
동일한 입력은 공급자를 다시 조회하지 않고, 공급자를 해석하는 중 최신 요청이
들어오면 이전 결과가 이를 덮지 않는다. 화면 퇴역 시 위젯이 보유한 공급자와
캐시를 해제한다. 공급자 조회 도중 화면과 위젯 핸들이 함께 폐기되는 경우에도
진행 중인 조회가 퇴역한 캐시에 공급자를 다시 남기지 않는 것을 검사했다.
선언형 화면은 `Image(iconName: "brush")`로 같은 기본값을 사용하고,
`iconSize`, `iconSource`, `iconRevision`, `accessibilityName`을 선택적으로
반응형으로 공급할 수 있다. 일반 이미지의 `source`와 아이콘 입력은 동시에
지정할 수 없다.

독립 C++ 앱은 `iiGUI/NativeIconView.h`의 `IconView`와 `IconViewInput`을 사용하고
래스터를 반환하는 `ButtonIconSource`를 제공한다. 기존 `LVUI/NativeIcon.h` 위젯은
제거했다. LVUI 자산 계약 `NativeAuthoredIcons.h`는 앱이 디코더를 주입받도록
바뀌어 iiGUI를 직접 참조하지 않는다. 다른 위젯 어댑터의 SDK 직접 참조는 남아 있다.
macOS arm64에서 독립 iiGUI 호스트와 설치 소비자, LVUI 생성·공급자·캐시·Python
API·설치 경계 검사를 통과했다. 실제 창과 다른 플랫폼은 이 검사에 포함되지 않는다.

아이콘은 LVUI가 소유한 벡터 디자인 자산과 앱이 교체 가능한 `IconSource`로
공급한다. `IconCatalog`는 이름과 불변 SVG 텍스트를 보유하는 표준 C++ 목록이다.
중복 이름, 잘못된 UTF-8, 빈 내용 및 자산 수·바이트 한도를 검증하고, 알 수 없는
이름은 오류를 반환한다. 불변 카탈로그는 동일 자산의 공유 소유권을 제공한다.

기본 자산은 `resources/icons/`에 보관하고 빌드할 때 결정적으로 임베드한다.
강조색 기본 아이콘은 SVG의 `data-lvui-fill-token` 또는 `data-lvui-stroke-token`으로
LVUI 스냅샷의 토큰에 연결된다. 생성기는 기본 모드의 실제 값을 속성에 반영하며
토큰이 없거나 색상이 불투명하지 않으면 빌드를 거절한다. 현재 파랑 아이콘 네 개는
`Accent/Blue/Hover`, 노랑 폴더는 `Accent/Yellow/Hover`를 사용한다. 이는 각 아이콘이
이전에 쓰던 기본 강조색에서 한 단계 어두운 역할이다. 나머지 기본 자산의 도형과
중립색 및 앱이 제공하는 `IconSource`는 그대로 유지한다. 연결은 빌드 시 확정되며
실시간 테마 변환이나 앱 제공 SVG의 자동 변경을 뜻하지 않는다.
LVUI 기본 이름은 `addFile`, `alertAdjustments`, `alertFileText`, `checkboxFigmaMark`,
`colorEyedropper`, `generaladd`, `generalchevronDown`, `generalchevronLeft`,
`generalchevronRight`, `generalchevronUp`, `generalchevronUpDown`, `generaldelete`,
`home`, `nodesfolder`, `procedure`, `projectStructure`, `searchField`, `settings`이다. 검색
아이콘은 Dreamscapes의 `TextField/Search`에서 제공된 12×12 SVG 원본의 경로를 보존하고
두 선을 독립 LVUI의 `Icon/search` 색상에 연결한다. 원본 SVG 바이트와 토큰 변경 시
색상 변경을 생성기 검사에서 확인한다. 경고창은 액션 개수에
따라 `alertAdjustments` 또는 `alertFileText`를 기본으로 사용한다. 앱이 공급자를
별도로 지정하지 않은 경우 이 이름이 아니면 아래의 독립 카탈로그에서 검색한다.
앱이 별도 `IconSource`를 제공할 수도 있다. 변경 가능한 공급자는 자산 변경마다
`revision()`을 갱신해야 한다. `iconSource`에 앱의 `shared_ptr<const IconSource>`를
넘기고 `iconRevision`에 관찰 가능한 0 이상의 정수를 넘기면 앱이 공급자를 갱신할 때
반응형 값도 함께 바꾸어 아이콘 재조회가 일어난다. 같은 공급자의 내부 변경은
자동으로 관찰되지 않는다. `iconName`과 `iconRevision`도 반응형 값일 수 있다.

`NativeIconCache<Decoder>`는 앱이 공급한 디코더를 이용해 메모리에서만
래스터화한다. 기존 `NativeIconCache` 직접 사용자는 `iiGUI/SvgImageCache.h`를
포함하고 `NativeIconCache<ii::gui::CachedSvgDecoder>`를 명시해야 한다. LVUI 헤더는
그래픽 SDK를 포함하지 않으며, 다른 디코더도 같은 `Image`와 `decode` 계약을
제공할 수 있다. SVG는 최대 1 MiB의 NUL 없는 UTF-8이어야 한다. 이름·공급자·revision·출력 크기가 같으면 불변 픽셀을 재사용한다.
잘못된 SVG나 없는 자산은 오류를 반환하며 이전 캐시를 보존한다. 공급자가 조회 중
계속 바뀌면 재시도 횟수를 제한한다. 캐시는 생성한 스레드에서만 조회할 수 있다.
현재 픽셀 크기는 논리 크기의 두 배이다.

기본 버튼과 드롭다운은 이름, 공급자, revision을 검증한 뒤 기존 컨트롤에 반영한다.
잘못된 후보는 기존 픽셀을 유지하고 정상 후보로 복구한다. 공급자 조회 중 새로운
설정이 들어오면 오래된 후보가 이를 덮지 않는다. 직접 작성한 기본 SVG는 이전
아이콘과 픽셀 단위로 같다는 의미는 아니다. 배율 변화 대응과 실제 플랫폼 시각
검증이 남아 있다.

## 이전 디자인 자산의 독립 카탈로그

LVRS의 2026-11-01 작업 트리에서 SVG 2,860개와 리소스 별칭 310개, 이름 별칭
10개를 LVUI의 `resources/authored-icons/`와 `authored-icon-manifest.json`으로
복제했다. 각 원본 바이트의 SHA-256과 크기를 보존하며 MIT 고지를
`resources/licenses/LVRS-MIT.txt`에 포함한다. 빌드·설치·실행은 LVRS 저장소,
QML 파일 또는 외부 리소스를 읽지 않는다. `embed_authored_icons.py`는 고정된
매니페스트와 파일을 대조해 정렬된 C++ 자료를 생성한다.

`authored_icon_catalog()`은 교체 가능한 `IconSource`이며 요청된 SVG만 불변
공유 객체로 만든다. 원본 리소스 별칭과 공백·슬래시·`.svg` 및 기존 이름 별칭을
해석한다. 동일한 원본의 별칭은 같은 객체를 사용하며 여러 스레드의 조회를
직렬화한다. `authored_icon_names()`와 `authored_icon_svg()`는 이름 정규화를
거치지 않는 정확한 원본 목록과 프로세스 수명의 문자열 뷰를 제공한다.
예를 들어 기존 규칙에서 `projectStructure`는 `generalprojectStructure`로
정규화되지만 원본 `projectStructure` SVG도 정확한 조회로 확인할 수 있다.

기존 LVUI 기본 아이콘은 우선하며, 기본 `NativeIconCache`는 이름을 찾지 못하면
아래의 디자인 카탈로그를 거쳐 이전 자산에서 검색한다. 앱이 명시적으로 공급한 `IconSource`는 이
폴백을 사용하지 않는다. 완전히 동일한 이전 자산이 필요한 앱은
`authored_icon_catalog()`을 명시적으로 공급할 수 있다. 기존 아이콘의 색상이나
형태를 조용히 덮어쓰지 않는다. 이전 경로 문자열을 입력으로 받아도 해당 문자열은
호환 식별자일 뿐 실제 QML 파일이나 Qt 리소스를 열지 않는다.

iiGUI를 조합하는 C++ 앱은 `LVUI/NativeAuthoredIcons.h`와 `iiGUI/SvgImageCache.h`를
가져와 `lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>(name, pixels)`로
동일한 원본을 1~512픽셀 정사각형 비트맵으로 요청할 수 있다. 기존 비템플릿 호출은
디코더를 명시하도록 변경해야 한다. LVUI 계약은 자산·리비전과 1 MiB 입력 한도를
검사하며, 기본 디코더와 최대 128개·8 MiB의 공유 캐시는 iiGUI가 소유한다. 같은
원본의 별칭은 캐시 항목을 공유한다. 이름이나 크기가 잘못되었거나 디코더가 효과를
지원하지 않으면 오류를 반환한다. 다른 디코더를 공급하는 앱은 iiGUI 없이 LVUI
자산 계약을 사용할 수 있다.

내부 fragment 참조와 포함된 PNG는 자산으로 보존하고 외부 URL·스크립트·활성
요소는 생성 단계에서 거부한다. 한 SVG는 최대 1MiB, 전체는 16MiB로 제한한다.
원본 조회와 별칭·동시성, 생성기의 안전성은 자동 검사한다.

필터를 사용하는 `checkboxCheckedDisabled`는 원본 SVG를 변경하지 않고,
원본을 512×512 투명 이미지로 미리 렌더링한 LVUI 소유 대체 자산을 사용한다.
`resources/authored-icon-fallbacks/manifest.json`은 원본·대체 SVG·포함된 PNG와
비교 이미지의 해시와 크기를 고정한다. 생성기는 이를 원본 목록과 대조하고 단일 PNG를 포함한
안전한 SVG 구조만 임베드한다. 대체 이름이 별칭 해석 후 다른 원본을 가리키면 생성을
거부한다. 대체 SVG와 비교 이미지에도 합계 16MiB 제한을 적용하고,
긴 SVG는 큰 문자열 리터럴 대신 바이트 배열로 생성한다. 같은 카탈로그의 불변 원본 객체에만 대체 자산을
적용하므로, 이름이나 SVG 텍스트가 같은 별도 앱 공급 객체를 바꾸지 않는다.
`authored_icon_svg()`는 항상 필터가 포함된 원본을 반환한다. 정적 이미지는 자산
제작 과정에서 Chrome 156으로 생성했고 빌드와 앱 실행에는 브라우저가 필요하지 않다.

macOS 헤드리스 검사에서 원본 2,860개 중 2,859개는 iiGUI의 24×24 SVG 디코더가
직접 출력하고, 나머지 한 개는 범용 필터 미지원으로 `unsupported`를 반환한다.
LVUI의 자산 어댑터로는 2,860개를 모두 출력한다. 해당 대체 자산의 24×24와
64×64 결과는 각각 원본의 독립 렌더링과 알파가 적용된 채널의 평균 오차를 비교하며,
기준 이미지는 `reference-24.png`, `reference-64.png`의 128×128 투명 화면 왼쪽
위에 위치한다. 이는 일반 SVG 필터 구현이나 모든 크기·플랫폼의 픽셀 동등성,
GPU 표시 검증을 뜻하지 않는다. 사용자 제공 SVG의 미지원 필터는 여전히 오류다.

남은 SVG의 필터는 Gaussian blur뿐 아니라 색상 행렬, 합성, 오프셋과 블렌드도
사용한다. [ThorVG의 SVG 지원 목록](https://github.com/thorvg/thorvg/wiki/SVG-Support)은
Gaussian blur를 명시하지만 이 자산의 색상 행렬과 합성은 해당 목록에서 확인되지
않으므로 교체만으로 동등해진다고 판단할 수 없다. [resvg의 필터 계약](https://github.com/linebender/resvg/blob/main/crates/usvg/docs/spec.adoc)은
필요한 연산을 문서화하고 [라이선스와 C 인터페이스](https://github.com/linebender/resvg)는
MIT 또는 Apache-2.0 및 C API를 제공한다. 다만 Rust 빌드 도구가 추가되며 해당 SVG의
실제 출력, 자원 한도, 플랫폼별 빌드와 전이 의존성 라이선스는 따로 검증해야 한다.
또한 resvg의 [기본 이미지 참조 처리](https://github.com/linebender/resvg/blob/v0.49.0/crates/usvg/src/parser/image.rs)는
로컬 파일 접근을 허용하므로, 임의 SVG에 적용하려면 이를 명시적으로 차단하거나
허용 범위를 제한하고 입력·중간 이미지의 자원 한도를 확인해야 한다. 현재의 C API를
그대로 연결하는 것만으로 기존 iiGUI 입력 정책이 보장되는 것은 아니다.
어느 대안도 현재 iiGUI의 디코더에 연결하거나 사용자 환경에서 검증한 것은 아니다.

## 현재 디자인의 독립 카탈로그

`design_icon_catalog()`는 현재 Figma 디자인에서 캡처한 아이콘을 우선하고, 목록에
없는 이름은 기존 `authored_icon_catalog()`에 위임한다. 기존 카탈로그와 정확한
바이트 조회는 변경하지 않는다. 두 카탈로그는 이름 정규화·기존 별칭을 공유하며,
같은 자산은 동시 조회와 별칭 사이에서 불변 객체를 공유한다. `design_icon_names()`와
`design_icon_svg()`는 디자인 목록과 정확한 SVG를 제공한다.

현재 목록은 Congregation Main에 사용된 100종과 LVUI Avatar의 기본 사용자 심벌
1종이다. Figma에서 확인한 18×18 벡터를
`resources/design-icons/`에 보관하며 원본 노드와 SHA-256을 매니페스트에 기록한다.
기존 자산 61종의 뷰박스는 현재 Figma와 달랐으므로 디자인 목록은 기존 파일을
수정하지 않고 별도 자산으로 보관한다. 생성기는 목록과 해시, SVG 안전성 및
뷰박스를 검증해 임베드한다. 런타임은 Figma·Qt·LVRS에 접근하지 않는다.

`lvui::native::design_icon_image()`는 디자인 목록과 기존 자산 폴백을 1~512픽셀로
래스터화한다. 범용 `icon_image()`는 다른 `IconSource`도 받으며, 조회 또는 래스터화
중 원본 버전이 바뀌면 최대 네 번 재시도한 뒤 `busy`를 반환한다. 캐시는 불변 자산과
픽셀 크기를 기준으로 공유하며 기존 필터 대체 이미지는 원본 자산에만 적용한다.
앱이 직접 지정한 공급자를 기본 목록으로 대체하지 않는다. 자세한 계약은
[디자인 아이콘](DesignIconCatalog.md)을 따른다.

앞선 macOS 헤드리스 검사에서는 Main의 100종 생성·래스터화와 기존 2,860종의 원본 보존을
확인했다. `brush`, `previous-frame`, `inspectionsEye`를 Figma에서 별도 내보낸 36px
PNG와 비교한 알파 적용 평균 채널 오차는 각각 0.982, 0.505, 0.907/255였다.
이는 세 자산과 해당 디코더에 대한 검사이며 모든 아이콘의 픽셀 동등성이나 실제 GPU
표시를 검증한 결과는 아니다.

2026-11-02 macOS 헤드리스 탐색에서는 resvg 0.49.0의 C API를 텍스트 기능 없이,
래스터 이미지 기능만 켜 정적 라이브러리로 빌드했다. 필터가 들어 있는 원본
`checkboxCheckedDisabled`를 직접 그려 기존 독립 기준 PNG의 왼쪽 위 픽셀과 비교한 결과,
알파를 적용한 RGBA 채널 평균 절대 오차는 24×24에서 0.688/255, 64×64에서
0.326/255였다. 최대 채널 오차는 각각 36/255와 80/255이므로 평균값만으로 완전한
픽셀 일치를 뜻하지 않는다. 같은 실험에서 임시로 만든 단색 PNG의 절대 경로를 SVG
`image`에 넣자 기본 C API가 파일을 읽어 출력하는 것도 확인했다. 이 결과는 macOS의
단일 원본과 제한된 기능 구성에 대한 탐색이며, 앱 통합·다른 플랫폼·글꼴·메모리
한도나 임의 SVG 전반의 동작을 검증하지 않는다. 실험용 소스와 빌드·캐시는 정리한다.
실험용 C API에서 외부 이미지 문자열 리졸버를 비활성화하면 같은 로컬 참조는
표시되지 않았고, 내장 PNG와 위 필터 아이콘은 계속 표시됐다. 이는 외부 리소스
접근을 막는 현재 iiGUI 계약의 한 부분만 확인한 것이다. 외부 참조를 조용히 생략해
결과 이미지가 일부 빠질 수 있으므로, 통합 시 진단 정책을 정해야 한다. 필터 중간
버퍼와 연산 수의 한도도 별도로 설계·검증해야 한다.
