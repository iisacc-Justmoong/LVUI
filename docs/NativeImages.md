# 네이티브 이미지와 SVG 연결

기본 LVUI 생성 경로의 `Image`는 iiGUI의 이미지 모델과 자체 그리기 위젯을
생성하고 앱이 공급한 이미지 및 반응형 값을 연결한다. 세부 배치·검증·접근성 계약은
[LVUI 독립 이미지](NativeImage.md)를 따른다. source는 C++ 백엔드가 제공하는
`shared_ptr<const ii::gui::RasterImage>`이거나 화면 서비스 또는 개별 `imageLoader`가
해석하는 파일·URL 문자열이다. 로더는 앱이 소유하며 [이미지 로딩](NativeImageLoading.md)에
설명한 iiSDK 제공자나 교체 가능한 앱 제공자를 사용할 수 있다.

```lvui
import vm ImageBackend
Window {
    Image(source: vm.preview,
          width: 64, height: 64, fit: .Contain)
}
```

파일·원격 자산을 사용하려면 앱이 로더와 수명·오류 정책을 연결한다.
import·생성자 인자의 수명 규칙은 BackendConstruction.md를 따른다. QML/JS 실행이나
뷰 내부의 네트워크 메서드는 추가하지 않는다.

source, imageLoader, width, height, accessibilityName, padding, cornerRadius, borderWidth,
backgroundColor, borderColor, preserveAspect, fit, circular, allowEmpty를 제공한다.
fit은 .Contain/.Cover/.Stretch 또는 C++ enum 값이다. 기본 크기는 32×32,
allowEmpty=true이며 비동기 소스가 없는 동안에도 같은 위젯을 유지한다. visible·opacity·
flex는 기존 공통 입력이다. 업데이트는 옵션을 검증한 뒤 반영하고 실패하면 이전 표시를
유지한다. 반응형 옵션은 한 C++ 옵션 묶음으로 읽고 반영하므로 잘못된 padding/크기
조합 이후 다른 크기가 바뀌어도 요청한 값을 잃지 않고 재검증한다.
소스·크기 변경이 스코프·입력·포커스·사용자 파일을 재생성하지 않는다.

독립 경로는 native 이미지 핸들, 문자열 주소, 빈 상태, 변경 및 잘못된 입력의 복구를 다룬다.
창을 만들지 않는 호스트 검사는 이미지 공급 메서드가 초기 활성화와 의존성 갱신에서
각각 한 번 평가되는지, 위젯과 접근성 의미 정보가 유지되는지, 이미지 배치와 잘못된
후보의 거부·복구를 확인한다. 실제 OS 창이나 GPU 실행 검증은 포함하지 않는다.
LVRS 전용 생성 경로와 과거 NativeSvg 예제는 별도 레거시 기록이다. 현재 로더의
헤드리스 검증과 실제 플랫폼 실행을 구분한다.
