# C++ 플랫폼 서비스와 프로필 표시

> 아래는 과거 LVRS 통합을 설명한 기록이다. LVUI는 LVRS 플랫폼 타입을 제공하거나
> 필요로 하지 않는다. 앱은 원하는 플랫폼 서비스를 일반 C++ import로 직접 주입할 수
> 있으며, LVUI 소유의 독립 플랫폼 프로필 API는 아직 확정되지 않았다.

`lvrs::PlatformInfo`를 일반 scoped C++ import로 사용한다. OS별 분기·타깃 별칭·메타데이터
구성은 네이티브 모듈이 수행하고 LVUI는 반환된 C++ 문자열·Bool·Core Value를 표시한다.

```lvui
import platform lvrs.PlatformInfo
Window {
    VStack {
        Label(text: platform.os())
        Label(text: platform.runtimeProfile("iphoneos")["target"])
        HStack(width: platform.runtimeProfile("ios")["adaptiveNavWidth"]) {
            Label(text: "Navigation")
        }
    }
}
```

메서드는 현재 활성화의 C++ 객체에 직접 호출한다. runtimeProfiles는 순서가 있는 C++
목록이며, 각 항목과 runtimeProfile은 Core의 소유된 읽기 전용 Value 객체이다. 문자열 키
조회는 기존 NativeValues 경계를 사용한다. 값을 표시하기 위해 원본 객체에 키를 삽입하거나
QVariant·자바스크립트 객체로 변환하지 않는다.

서로 다른 문맥을 선택하는 앱은 C++에서 `Observable<shared_ptr<const PlatformInfo>>`를
제공할 수 있다. 선택 변경은 기존 메서드/멤버 관측으로 갱신하고 InputField·포커스·콘텐츠
세대를 보존한다. `@Static`은 활성화 시의 프로필 스냅샷을 유지한다. 뷰 문맥 선택은 파일
편집이나 현재 OS·GPU 설정 변경을 의미하지 않는다.

당시 프로필 필드와 기능 게이트의 의미는
[레거시 PlatformInfo](../../LVRS/docs/native/PlatformInfo.md)에 보존되어 있다. 이 예제는
현재 LVUI의 패키지 의존성이나 기본 플랫폼 서비스 계약을 나타내지 않는다.
