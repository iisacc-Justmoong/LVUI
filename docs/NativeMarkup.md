# 레거시 마크업 변환기 기록

> 아래 내용은 과거 변환기의 보존 기록이다. 현재 LVUI 소스는
> [즉시 선언형 View](ImmediateDeclaration.md)이며 `lvui-check`와 `lvui-compile`은
> HTML형 입력을 받지 않는다. C++의 `parse_native_markup`은 전환을 위한 별도
> 레거시 API로 남아 있으며 표준 `.lvui` 컴파일 경로에는 연결되지 않는다.

레거시 변환기는 HTML형 입력을 타입 구문의 트리로 변환한다. 브라우저·웹뷰·JavaScript 실행기는 사용하지 않는다. 배치는 명시적인 `HStack`, `VStack`, `ZStack` 태그로 지정한다. `div`와 `flex-direction`을 사용한 방향 추론은 거부하며, 진단에서 스택 선택을 안내한다.

[Color·Metric](StyleTypes.md) 타입 선택을 속성의 `{Color.primary}` / `{Metric.space8}`와 인라인 스타일의 `color:Color.primary;gap:Metric.space8`에 사용할 수 있다. 색상·수치 속성에서는 `color:.primary;gap:.space8`처럼 문맥상 접두사를 생략할 수 있다. 기존 `var(--lvrs-...)` 형식도 유지한다.

```html
<script>
import vm WorkspaceBackend
var caption: String = "Ready"
</script>
<ApplicationWindow title="Workspace">
  <VStack style="gap: 12px; padding: 16px; align-items: stretch">
    <span text={vm.status} style="font-size: 16px" />
    <HStack style="gap: 8px">
      <Button on:click={vm.open}>Open</Button>
      <Button on:click={vm.save} enabled={vm.canSave}>Save</Button>
    </HStack>
  </VStack>
</ApplicationWindow>
```

`script`는 이 레거시 API에서 C++ 백엔드 가져오기와 LVUI 타입 선언만 포함한다. `vm.open`은 콜백 참조이고 `vm.open()`은 호출 표현이다. 선언 언어의 기존 호출·수명 계약을 그대로 적용한다. 속성의 따옴표 값은 문자열이며 `{}`는 타입이 있는 LVUI 표현이다. span/label은 Label, button은 Button, input은 InputField가 된다.

인라인 스타일은 `display: flex`, `gap`, `padding`, `align-items`, `width`, `height`, `flex`, `font-size`, `font-weight`, `line-height`, `font-family`, `color`, `background`/`background-color`, `border-color`, `border-width`, `border-radius`를 지원한다. 픽셀 값은 논리 픽셀이다. 지원하지 않는 스타일이나 컴포넌트 속성은 진단으로 거부한다. 임의 CSS를 조용히 무시하지 않는다. 전체 웹 표준 호환은 이 언어의 계약이 아니다.

입력은 UTF-8 1 MiB, 노드 4,096개, 깊이 64로 제한한다. 주석·표준 XML 문자 엔티티·정확한 닫기 태그를 검사한다. 네트워크 리소스나 스크립트는 파싱 중 실행하지 않는다. 일반 텍스트와 자식 컴포넌트를 섞는 경우 Label/span을 명시한다.

## 여러 줄 편집 영역

`<InputField multiline={true} height={70}/>`는 네이티브 InputStyle의 높이를 생성한다. 내용과 이벤트는 `text`와 `onEdited` 바인딩을 사용한다. Enter는 새 줄, Control/Meta+Enter는 제출이다. AOT markup 회귀 검사가 높이 생성 계약을 검증한다.
