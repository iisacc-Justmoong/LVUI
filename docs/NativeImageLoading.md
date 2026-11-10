# 네이티브 이미지 로딩

`LVUI::ImageIO`는 이미지 자료형에 독립적인 `ImageResolver<Image>` 계약이다.
파일·네트워크·이미지 코덱 SDK는 패키지에 연결되지 않는다. 설치된 앱 예제의
`ApplicationImageIOAdapter`는 iiGUI 래스터와 앱이 선택한 파일 제공자, 네트워크,
코덱을 명시적으로 조합한다. 앱은 자체 공급자로 교체할 수 있다.

```cmake
find_package(LVUI CONFIG REQUIRED COMPONENTS ImageIO)
add_subdirectory(/path/to/LVUI/examples/Integrations integrations) # APPLICATION_IMAGE_ADAPTER=ON
target_link_libraries(app PRIVATE LVUI::ImageIO ApplicationImageIOAdapter)
```

SDK를 만들 때 `LVUI_BUILD_IMAGE_IO=ON`을 지정한다. 위 앱 어댑터를 사용할 때는
`APPLICATION_IMAGE_ADAPTER=ON`과 `APPLICATION_COLOR_ADAPTER=OFF`를 지정한다.
해당 앱 조합에는 iiGUI, iiSDKCore Runtime·Observation, iiFileProviderNative,
iiNetwork, iiVisualCodec Image, CURL과 zlib의 플랫폼별 패키지가 필요하다.
구현 헤더는 앱 어댑터의 `NativeImageLoader.h`이며 LVUI 공개 헤더가 아니다.
이미지 로더와 작업 실행기는 앱이 소유한다. 화면 활성화의 이미지 서비스에 로더를
한 번 연결하면 해당 화면의 이미지와 자식 스코프에서 함께 사용한다.

```cpp
// loop와 workers는 loader보다 오래 살아야 한다. loop는 UI 스레드에서 펌프한다.
ii::runtime::Runtime workers({.workers = 2, .capacity = 64});
auto loader = std::make_shared<lvui::native::ImageLoader>(loop, workers);
auto services = std::make_shared<lvui::native::ImageServices<ii::gui::RasterImage>>();
services->resolver.set(loader);
auto mounted = PreviewView::mount(host, [&](auto& scope) {
    scope.image_services = services; // 활성화 전에 공유 서비스로 교체
});
```

여러 창에 같은 앱 정책을 전달한다면 서비스 등록을 `BackendFactories`에 한 번
넣을 수도 있다. 명시적 `imageLoader` 없이 이미지 소스를 쓰는 각 화면은 활성화할 때
이 정책을 사용하므로 각 화면의 `configure`에서 서비스를 다시 지정할 필요가 없다.

```cpp
auto factories = lvui::native::BackendFactories{}
    .with<std::shared_ptr<lvui::native::ImageServices<ii::gui::RasterImage>>>([services] {
        return services;
    });
auto mounted = PreviewView::mount(host, {}, nullptr, factories);
```

기존 백엔드 생성 정책이 있으면 그 정책에 `with`를 호출해 결합한다. 팩토리는
최상위 화면 활성화마다 한 번 호출되며, 위 예처럼 같은 서비스를 반환해 캐시를
공유하거나 새 서비스를 반환해 화면을 분리할 수 있다. 지역·조건부·반복 자식은
팩토리를 다시 호출하지 않고 부모의 서비스를 상속한다. 팩토리가 빈 포인터를
반환하거나 등록되지 않았다면 독립적인 빈 서비스가 생성된다. `configure`에서
서비스를 명시하면 앱 정책보다 우선한다. 팩토리 예외가 발생하면 후보 화면을
버리고 기존 화면을 유지한다. `BackendRequest`를 받는 팩토리의 위치는
`type = "lvui::native::ImageServices"`, `binding = "image_services"`이며,
앱이 활성화 문맥을 제공하면 import가 없는 이미지 전용 화면에도 이를 전달하고
화면과 함께 연결·퇴역한다. 생성 정책과 화면은 서비스를 소유할 수 있으므로
`loop`와 `workers`는 해당 정책과 화면이 모두 폐기된 뒤에도 실행 중인 이미지
작업이 끝날 때까지 살아 있어야 한다.

```lvui
import vm PreviewModel
Window {
    VStack {
        Image(source: vm.url, width: 120, height: 80)
        Avatar(source: vm.url, size: .Small)
        Card(type: .File, previewSource: vm.url)
        ListItem(type: .Media, previewSource: vm.url)
    }
}
```

`source`와 `previewSource`는 문자열 리터럴, 반응형 문자열 또는 앱이 준비한
`shared_ptr<const RasterImage>`를 받는다. 각 화면 활성화에는 독립적인
`image_services`가 기본 생성되며 위 예처럼 여러 화면에 같은 서비스를 전달할 수 있다.
중첩 스코프는 부모의 서비스를 공유한다. 활성화 후 공급자를 바꾸거나 제거할 때는
`services->resolver.set(...)`을 사용한다. `image_services` 포인터를 활성화 후에
대입하는 것은 반응형 갱신이 아니므로 활성화 전의 `configure`에서만 교체한다.

공급자가 없는 동안 비어 있지 않은 문자열도 빈 이미지로 표시되고, 나중에 공급자를
설정하면 기존 위젯이 갱신된다. 공급자를 제거하면 이전 이미지를 비운다. 빈 주소와
기존 래스터 핸들은 공급자를 사용하지 않는다. 서비스가 기본 생성된다는 말은 파일이나
네트워크 로더가 자동으로 생성된다는 뜻은 아니다. 앱은 필요한 리소스 정책에 따라
`ImageLoader` 또는 자체 `ImageResolver`를 한 번 연결해야 한다.

개별 컴포넌트에 `imageLoader: vm.loader`를 지정하면 화면 서비스보다 우선한다. 이
명시적 공급자가 비어 있는데 주소가 비어 있지 않으면 오류다. 사용자 공급자는
`ImageResolver<ii::gui::RasterImage>::image`를 구현한다. 비동기 갱신을 선언에 반영하려면 해당 메서드가
iiSDKCore Observable을 읽어야 한다. 원격 결과, 주소 및 공급자 교체는 기존 위젯의
입력을 갱신한다.
`Image`의 기본 `allowEmpty: true`는 원격 이미지가 준비되는 동안 빈 자리를 유지한다.
`allowEmpty: false`에서는 빈 후보를 거부하므로 준비 전의 초기 생성도 실패할 수 있다.

기본 소스는 UTF-8 로컬 경로, `file:` URL, percent/base64 `data:` URL, HTTP와
HTTPS를 지원한다. 과거 `qrc:/qt/qml/LVRS/resources/iconset/` 및 대응 `:/` 경로는
LVUI가 보유한 아이콘 카탈로그에서 읽는다. 다른 URL 스킴과 앱 리소스는
`ImageSourceOptions::resource`로 앱이 해석할 수 있다. 사용자명·암호가 포함된 HTTP
주소와 HTTPS에서 HTTP로 낮아지는 리다이렉트는 거부한다. 리다이렉트는 최대 여덟 번이며
응답 크기와 시간도 제한한다. 파일을 읽기만 하며 변경하지 않는다.

iiVisualCodec이 지원하는 PNG·JPEG·BMP·GIF 첫 이미지·TGA·합성 PSD·WebP·TIFF는
원본 크기로 디코딩하고 색상 프로필이 있으면 sRGB로 변환한다. SVG와 gzip 압축
SVG는 iiGUI가 요청된 크기로 래스터화한다. SVG의 외부 리소스·스크립트·애니메이션과
일부 효과는 iiGUI의 [이미지 계약](../../iiGUI/docs/Images.md)에 따른다. 동영상,
애니메이션 프레임과 편집 가능한 PSD 계층을 이 이미지 API로 보존하지 않는다.

기본 입력은 최대 16 MiB, 압축 해제 후에도 같은 제한을 적용한다. 래스터는 변마다
8192픽셀 이하이면서 최대 16 Mi 픽셀이다. 기본 캐시는 원본 64 MiB, 래스터 64 MiB,
항목 256개, 동시 원격 요청 64개로 제한되며 `ImageCacheLimits`로 조정할 수 있다.
한 주소의 여러 크기는 원본과 원격 요청을 공유하고 SVG 래스터는 크기별로 보관한다.
사용하지 않는 항목과 래스터부터 제거한다. 외부가 계속 보유하는 이미지 메모리는
캐시 상한에 포함되지 않는다. 화면 크기에 맞게 `Image`·`Avatar`·`Card`는 각 모델의
크기를, 목록은 `previewSize` 또는 48을 SVG 래스터 크기로 전달한다.

`load(address, width, height)`는 `empty`, `loading`, `ready`, `failed`와 이미지 또는
오류 문구를 돌려준다. `image()`는 선언의 간편 입력용이며 준비되지 않았거나 실패하면
빈 핸들을 반환한다. 성공과 실패 완료는 관찰 가능한 리비전을 발행한다. 실패한 주소를
매 렌더링마다 자동 재요청하지 않으므로 재시도나 파일 갱신에는 `invalidate(address)`를
호출한다. 같은 정규 키의 별칭도 함께 무효화한다. `clear()`는 호출 당시의 요청을
취소하고 취소 콜백이 새로 시작한 요청은 보존한다. 소멸은 남은 요청을 취소하며
이전 요청의 늦은 완료를 차단한다. 로더·공급자 호출은 생성한 스레드에서 이루어져야 하며,
주입한 소스의 완료도 같은 스레드에 지연 전달해야 한다.

## 작업 스레드와 수명

위의 `EventLoop`와 `Runtime`을 함께 전달하는 생성자는 로컬 파일 읽기, 데이터 URL
디코딩과 압축 해제, 원격 gzip 본문 압축 해제, 이미지 디코딩·SVG 래스터화를 작업
스레드에 보낸다. 원격 전송은 iiNetwork가 담당한다. HTTP 압축은 gzip을 요청하며
압축된 SVG 파일이 전송 압축까지 된 경우 두 계층을 각각 해제한다. 지원하지 않는
HTTP 콘텐츠 인코딩은 오류다. 주소 검사·정규화, 기본 아이콘 카탈로그 조회,
`ImageSourceOptions::resource` 콜백과 사용자 `ImageResourceSource::read/fetch`는
소유 스레드에서 실행된다. 이 콜백에서 오래 걸리는 일을 직접 수행하지 않아야 한다.

`ImageFileReader`와 `ImageDecoder`를 각각 주입해 파일 접근과 디코더를 교체할 수
있다. 비동기 생성자에 넘긴 구현은 둘 이상의 작업 스레드에서 호출되어도 안전해야
한다. 사용자 소스를 쓰는 경우 `ImageLoader(source, loop, workers, limits, decoder)`로
디코딩을 백그라운드에 보낼 수 있다. 사용자 소스 자체의 완료 전달 계약은 바뀌지
않는다. 주입한 파일 리더가 한도를 넘겨 반환한 데이터도 로더가 거부한다.

로더 하나는 대기 중인 파일·디코딩 작업을 `pending_requests` 이하로 유지하고 동시에
최대 두 작업을 실행한다. 화면에 표시할 래스터가 준비되도록 디코딩 작업을 대기 중인
새 파일 읽기보다 먼저 처리한다. 모든 완료의 전달을 위해 소유 이벤트 루프에 신호
슬롯 한 개를 예약한다. 작업이 완료되거나 실행 전 취소되면 작업 측이 이 슬롯을 깨우며,
작업 대기 중에는 반복 확인 기한을 만들지 않는다. 결과를 받은 뒤 슬롯을 반납한다.
소유 루프가 포화되어 슬롯을
확보할 수 없거나 중단되면 실패한다. 앱은 루프를 계속 펌프하고, 로더를 폐기한 뒤에도
진행 중인 작업이 끝날 때까지 `Runtime`을 유지해야 한다.

완료 리비전을 관찰하는 콜백이 예외를 던져도 다른 이미지 완료는 계속 전달한다.
첫 예외는 소유 루프의 별도 작업으로 다시 던진다. 루프가 포화되어 새 작업을 받을 수
없으면 남은 이미지 작업을 전달한 뒤 루프 호출에 다시 던진다. 같은 시기에 전달을
기다리는 추가 예외는 첫 예외로 합친다. 루프를 중단해 더 이상 펌프하지 않으면 대기
중인 예외나 완료가 전달된다고 보장하지 않는다. 남은 작업이 모두 취소된 경우에는
실행 중인 작업이 끝나기를 기다리지 않고 예외를 전달한다. 그 작업이 돌아오기 전에도
나중에 새 신호 슬롯을 예약해 로더를 다시 사용할 수 있다. 기존에 실행 중이던 취소된
작업도 새 슬롯에 안전하게 다시 연결하여 완료와 자원 회수를 놓치지 않는다.

취소는 대기 작업을 제거하거나 실행 중인 작업의 결과를 버린다. 이미 실행 중인 파일
시스템 호출이나 코덱 함수를 강제로 중단하지 않으며 로더 소멸자는 그 반환을 기다리지
않는다. 실행 중인 작업이 보유한 입력과 앱이 보유한 이미지 핸들은 캐시 예산 밖이다.
예산보다 큰 래스터는 영구 캐시에 두지 않고 소비 대기 중인 결과를 최대 두 개까지
임시 보관한다. 이 결과는 읽히면 제거되며 다른 완료에 의해 먼저 밀려날 수 있다.

`ImageLoader()`, `ImageLoader(source)` 및 `ImageLoader(loop)`는 기존 동기 호환
생성자다. 이 경로에서는 로컬 읽기와 디코딩이 소유 스레드에서 수행되므로 큰 이미지에
대해서는 위의 작업 실행기 생성자를 사용한다. 화면 상태별 로딩 UI와 실제 플랫폼
표시·접근성 동등성은 별도 작업이다. 숨겨진 목록 미리보기와 파일형이 아닌 카드
미리보기는 요청하지 않는다.

## 재배치된 설치 소비자 계약

`LVUI_BUILD_IMAGE_IO=ON`인 빌드의 설치 검사는 옮겨진 LVUI 패키지에서
`find_package(LVUI COMPONENTS ImageIO)`와 `LVUI::ImageIO`만을 사용하고 다른 SDK
탐색을 차단한 계약 소비자와, 필요한 SDK를 명시적으로 연결한 앱 어댑터 소비자를
각각 구성·컴파일·링크한다. 앱 소비자는 이미지 소스의 메모리 SVG를
로딩하고 요청한 래스터 크기를 확인한다. 이 검사는 창·GPU·네트워크를 열지 않는다.

소비자 구성에는 원래 빌드가 명시한 iiSDK와 코덱 의존성의 패키지 위치를 전달한다.
Qt·LVRS 검색을 비활성화하며 사용자의 전역 검색 경로를 소비자에 전달하지 않는다.
ImageIO 구성이 켜져 있는데 설치된 구성요소가 없으면 검사는 실패한다. 구성이 꺼진
경우에는 검사 대상에서 제외했다고 보고하며, 설치·링크 성공으로 기록하지 않는다.
설치 검사에 지정한 경로는 소유한 스테이징 디렉터리 안에 머물러야 한다. 절대 경로와
상위 디렉터리 이동, Windows 드라이브 상대 경로와 루트 상대 경로는 거부한다.
`lib64`처럼 허용된 상대 라이브러리·헤더·실행 파일 디렉터리는 소비자와
패키지 탐색에 그대로 반영한다.

2026-11-04 macOS arm64에서 Qt·LVRS SDK 경로를 차단하고 ImageIO·색상 어댑터·
작업 런타임을 켠 새 빌드를 수행했다. 설치본을 옮긴 뒤 기본 소비자와 생성 앱,
선택형 Runtime·ImageIO 소비자를 구성·빌드했으며 Runtime과 ImageIO의 헤드리스
소비자 검사를 통과했다. `lib64`, `include/sdk`, `tools`로 설치 경로를 변경한
빌드도 같은 검사를 통과했다. ImageIO 소비자는 앱 제공 SVG를 12×6 래스터로
변환했으며 창·GPU·네트워크는 사용하지 않았다. 최종 기본 빌드의 이미지 모델·
비동기·설치·패키지 경계 및 작업 공간 검사 다섯 건도 통과했다.

기본 빌드의 LVUI 컴파일러·Native·Color·ImageIO 정적 라이브러리 네 개에서
미해결 심볼 723개를 역변환해 이름을 검사했으며 Qt·LVRS C++ 이름과 일치하는
심볼은 없었다. 이는 검사한 라이브러리의 미해결 심볼에 대한 결과이며 라이브러리에
포함된 모든 코드나 간접 의존성 전체를 분석한 결과는 아니다.

격리 설정에서는 LVUI 리소스, 설치된 `qt.svg` 아이콘과 iiGUI의
`QtGuiComparison` 문서 읽기를 예외로 허용했다. SDK 의존성은 작업 공간의
스테이징 패키지와 기존 iiFileProvider 설치본을 명시했다. 이 결과는 전역 LVUI
설치본을 교체하거나 모든 간접 라이브러리의 독립성을 증명하지 않으며 Windows·Linux
실행이나 실제 운영체제 창 표시 검증도 포함하지 않는다.
