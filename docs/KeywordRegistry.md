# LVUI 예약어 목록

LVUI의 예약어는 `catalog/lvui-keywords.json`이 소유한다. 이름·분류·내부 여부를
정렬된 유일한 항목으로 관리한다. 새 이름을 추가하거나 제거할지는 LVUI의 언어와
컴포넌트 계약을 기준으로 결정한다. 레거시 LVRS나 QML 소스의 변경은 이 목록을
자동으로 바꾸지 않는다.

```sh
python3 -B tools/update_keywords.py
python3 -B tools/update_keywords.py --check
python3 -B tests/test_lvui_keywords.py
```

생성기는 스키마·이름·분류·정렬·중복을 검증한 뒤 `src/LVUI/Keywords.def`를 만든다.
공개 `Keyword::source_file`과 `source_line`은 LVUI 목록의 실제 항목 위치이다.
예약된 이름은 파서의 어휘 계약을 의미한다. 이름이 등록된 사실만으로 해당
컴포넌트의 속성, 동작, 접근성 또는 플랫폼 구현이 완성된 것은 아니다.

[기존 소스 조사](ReservedWords.md)는 이 목록을 시작할 때의 역사적 자료이다.
그 조사 도구는 현재 목록과 문서를 덮어쓰지 않으며 기본 빌드에도 필요하지 않다.
