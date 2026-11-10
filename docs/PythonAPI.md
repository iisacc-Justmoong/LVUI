# LVUI Python 컴파일 API

Python 소비자는 별도의 Qt, QML, GUI 호스트나 임시 파일 없이 LVUI 원본을 기존
C++ 컴파일러에 전달하여 네이티브 C++23 코드를 생성할 수 있다. 공개
함수는 `compile_native(source, *, name="MainView", includes=(), style_profile=None)`와
`compile_style_header(profile, *, namespace="lvui_style")`이다. 선택한 프로필은
기존 LVUI의 스타일 검증과 의미를 그대로 따른다.

입력과 결과는 UTF-8이며 진단은 메시지, `stage`와 `offset`·`line`·`column`을
갖는 `CompileError`로 전달한다. `stage`는 `source`, `style`, `header` 중 하나이다.
`offset`은 0부터 시작하는 UTF-8 바이트 위치이고 `line`과 `column`은 1부터
시작하는 컴파일러의 줄·바이트 열 위치이다.
잘못된 Python 인수와 1MiB를 넘는 원본은 명시적으로 거부한다. 각 호출의 결과와
오류는 독립적으로 소유하며 동시 호출에서도 섞이지 않는다. 네이티브 C ABI의
결과 핸들은 호출자가 `lvui_compiler_result_free`로 해제하며 데이터 포인터는 그
전까지 유효하다. 예외는 ABI 경계를 넘지 않는다.
다른 C 또는 C++ 모듈은 설치된 `LVUI/CompilerBridge.h`와 CMake 대상
`LVUI::CompilerBridge`를 사용할 수 있다.

설치된 패키지는 `<prefix>/share/LVUI/python`을 Python의 모듈 경로에 추가해
사용한다. 같은 접두사의 `lib` 또는 `bin`에서 브리지를 찾으며
`LVUI_COMPILER_LIBRARY`를 지정하면 그 파일만 사용한다. 브리지가 없거나 ABI가
맞지 않으면 조용한 대체 구현 없이 오류를 보고한다.

```python
import ii_lvui

cpp = ii_lvui.compile_native('Window { Label(text: "Hello") }', name='HelloView')
header = ii_lvui.compile_style_header('const panelWidth: Metric = 352', namespace='Example.Theme')
```

이 API는 컴파일과 스타일 헤더 생성을 대상으로 하며 Python에서 실행 중인
네이티브 위젯 전체를 생성·제어하는 API를 제공하지 않는다. 소스·진단·인수,
동시 호출과 Qt·LVRS 비의존성을 창 없이 검사한다. macOS arm64에서 C ABI와
Python 검사 및 별도 설치 경로에서의 Python 컴파일 검사가 통과했다. Windows·
Linux의 소스 경로는 제공하지만 실제 해당 플랫폼에서 실행하지는 않았다.
`compile_native`는 `localized`, `setLocale`, `setString`이 포함된 선언도 C++로
생성한다. 실행 중 앱 문자열 저장소를 Python에서 직접 제어하는 바인딩은 별도 범위이다.
