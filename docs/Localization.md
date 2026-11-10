# 앱 문자열 저장소와 현지화 계약

사용자에게 표시할 문자열은 `.lvui`에서 `localized("키")`로 참조하고 앱의
`AppStrings`에 언어별 키·값으로 보관한다. 각 앱은 별도 저장소를 소유하며 하나의
앱에 속한 Scene과 뷰 활성화는 같은 저장소를 공유한다. 명시적으로 같은 저장소를
주입한 다른 호스트는 의도적으로 공유할 수 있다. 지역화 대상이 아닌 리터럴,
사용자가 입력한 값, 식별자는 자동 번역하거나 임의로 추출하지 않는다.

저장소는 선택 언어의 정확한 태그, 상위 태그, 기본 언어의 같은 순서로 키를 찾는다.
예를 들어 `ko-KR`은 `ko-kr`, `ko`, 기본 언어의 순서이다. 태그는 ASCII 대소문자를
구분하지 않으며 단순 하이픈 구분 태그를 받는다. 키가 없으면 키 자체를 표시하고,
빈 문자열은 번역이 존재하는 값이다. 복수형·문법 성별·날짜/숫자 형식 변환이나
운영체제 언어 자동 감지는 이 문자열 카탈로그가 추측하지 않는다.

언어 교체와 특정 키의 실행 중 대입은 UI 소유 스레드에서 수행하고 같은 앱의 활성
바인딩에 반영한다. `const title: String = localized("title")`은 반응형이며
`@Static const`와 저장형 `var`는 기존 뷰 세션 계약에 따라 초기값을 보존한다. 이벤트의
`setLocale("ko")`와 `setString("title", "새 제목")`은 각각 현재 언어와 그 언어의
문자열을 변경한다. C++ 앱은 주입한 저장소의 같은 메서드를 직접 사용할 수 있다.

외부 UTF-8 JSON 파일은 `{"locale":"en","strings":{"title":"Title"}}` 형태이다.
키는 각 카탈로그에서 고유하고 값은 문자열이어야 한다. 앱은 형식과 저장소를
`StringCatalogCodec` 및 `StringCatalogStorage` 콜백으로 제공한다. 예제 앱은
직접 iiJson과 iiFileProvider를 조합하며 잘못된 JSON 전체를 거부한다. LVUI는
반환된 언어·키·값을 다시 검증하며 실패할 때 기존 카탈로그를 유지한다. 저장은 명시적으로
호출할 때만 수행하고, 언어 전환이나 뷰 갱신이 원본 파일을 자동으로 수정하지 않는다.
파일 경로와 배포 위치, 권한 및 파일 변경을 언제 다시 읽을지는 앱이 결정한다.

조회·파일 크기 및 카탈로그 수는 유한하며, 오류와 누락은 빈 성공값으로 위장하지
않는다. 구체적인 사용 예제, 빌드 의존성 및 검증 범위는 구현과 함께 유지한다.

## 선언과 외부 파일

```lvui
Window {
    const title: String = localized("counter.heading")
    var count: Int = 0
    VStack {
        Label(text: title)
        Label(text: count)
        PushButton(text: localized("counter.increment"), onClick: { count = count + 1 })
        PushButton(text: localized("language.korean"), onClick: setLocale("ko-KR"))
        PushButton(text: localized("counter.replace"), onClick: {
            setString("counter.heading", localized("counter.replacement"))
        })
    }
}
```

`localized`의 키는 문자열 식이다. 키 자체도 반응형 값일 수 있다.
[전체 예제](../example/GettingStarted/LocalizedCounter.lvui)와
[영어](../example/GettingStarted/resources/strings/en.json),
[한국어](../example/GettingStarted/resources/strings/ko.json) 카탈로그는 실제 생성
코드와 같은 계약을 사용한다. 지역화된 문자열은 Label 이외의 String 입력과 창
제목에도 사용할 수 있다. 식별자 등 컴파일 시 고정되어야 하는 입력은 예외이다.

`setString`은 현재 선택한 정확한 태그의 카탈로그를 갱신한다. 예를 들어 `ko-KR`에서
부모 `ko`의 값을 읽고 있다면 `ko-kr`에 덮어쓰기를 만들며 `ko` 자체는 바꾸지 않는다.
`save_string_catalog(strings, "ko-kr", path, storage, codec)`는 해당 카탈로그만 저장한다.
선택 언어에 해당 파일이 없더라도 부모·기본 언어가 검색된다.

## 앱 시작과 파일 IO

기본 `BackendFactories`로 생성한 Application은 빈 `AppStrings`를 만들고 그 Scene에
전파한다. 카탈로그가 없을 때는 키가 표시된다. 여러 별도 생성 뷰를 한 앱 저장소에
연결하거나 시작 전에 파일을 읽으려면 다음과 같이 같은 인스턴스를 주입한다.

```cpp
auto strings = std::make_shared<lvui::native::AppStrings>();
auto storage = lvui_example::catalog_storage(); // 앱이 파일 공급자를 조합한다
auto codec = lvui_example::catalog_codec();     // 앱이 JSON 구현을 조합한다
if (auto loaded = lvui::native::load_string_catalog(*strings, resources / "en.json", storage, codec); !loaded)
    return loaded;
if (auto loaded = lvui::native::load_string_catalog(*strings, resources / "ko.json", storage, codec); !loaded)
    return loaded;
auto factories = lvui::native::BackendFactories{}.with_app_strings(strings);
return GeneratedApp::configure_application(app, {}, factories);
```

완전한 시작 코드는 [LocalizedApplication.cpp](../example/GettingStarted/App/LocalizedApplication.cpp),
선언은 [LocalizedApplication.lvui](../example/GettingStarted/LocalizedApplication.lvui)이다.
`lvui_add_application`의 `CONFIGURE_SOURCE`에 이와 같은 C++ 시작 파일을 전달하면
기본 시작 코드 대신 이 파일을 사용한다. 예제 빌드는 원본 `resources/strings`를
참조하며 배포할 때 앱이 실제 번들·사용자 저장소의 경로를 제공해야 한다.

문자열 저장소 자체는 `LVUI::Native`에 포함된다. 선택적 `LVUI::Localization`은
저장·형식 콜백 계약만 노출하며 iiSDKCore 외 SDK를 링크하거나 검색하지 않는다.
앱은 [예제 어댑터](../example/GettingStarted/App/StringCatalogAdapters.h)처럼
iiJson과 iiFileProvider를 직접 조합하거나 다른 구현을 제공한다. 설치 소비자는
`find_package(LVUI CONFIG REQUIRED COMPONENTS Native Localization)` 후
`LVUI::Localization`을 링크한다.
파일을 바깥에서 수정한 뒤 `load_string_catalog`를 다시 호출하면 활성 뷰가 갱신된다.
`save_string_catalog`는 전달된 쓰기 콜백을 사용하며 원자적 저장과 충돌 정책은
앱의 파일 공급자가 보장한다. 외부 편집자와의 내용 충돌은 자동 병합하지 않는다. 충돌 감지가 필요하면
`encode_string_catalog`의 결과와 iiFileProvider의 명시적 `File::update`를 사용한다.

## 경계와 검사

- 언어 태그는 최대 63바이트, 하이픈으로 구분된 ASCII 영숫자 항목은 각각 최대 8자이다.
  키는 최대 512바이트, 값은 최대 65536바이트이며 NUL과 잘못된 UTF-8을 거부한다.
- 카탈로그당 최대 10000개 키와 8MiB, 앱당 최대 128개 언어와 총 32MiB이다.
  원본과 출력도 각각 8MiB 이하여야 한다. 예제 JSON 어댑터는 중복 키, 예상 밖
  필드와 비문자열 값을 거부한다. LVUI는 언어·키·값 및 UTF-8을 독립적으로 검증한다.
- 저장소와 그에 연결된 뷰는 같은 UI 소유 스레드에서 수정한다. 파일 읽기·저장은
  동기식이다. 큰 파일은 작업자에서 읽고 `decode_string_catalog`로 검증한 다음
  UI 영역에서 `replace_catalog`를 호출할 수 있다. 변경 알림을 받는 사용자 코드가
  예외를 던진 경우 저장소의 변경은 이미 확정되었을 수 있다.
- 별도 앱이 같은 `AppStrings` 포인터를 명시적으로 받으면 상태도 공유한다.
  서로 독립적인 앱에는 다른 인스턴스를 전달한다. 수신 중인 뷰가 퇴역하면 기존
  바인딩 수명 계약에 따라 갱신이 중단된다.
- `LVUI.Localization.Storage`, `LVUI.Localization.Ports`, `LVUI.Localization.Catalog`,
  `LVUI.Localization.Generated`, `LVUI.ViewSessions.localization`은 언어 대체,
  외부 파일 왕복·재적재, 잘못된 파일의 롤백, 여러 창·Scene 및 실제 생성 C++를
  헤드리스로 검사한다. OS 언어 선택기와 플랫폼 화면 실행을 의미하지 않는다.
