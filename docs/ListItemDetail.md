# 파일 목록의 상세 항목

Society와 Dreamscapes의 빈 목록 및 파일 정보 행은 제목과 설명에 더해 날짜,
폴더, 태그를 독립적으로 표시하거나 감춘다. `showDate`, `showFolders`, `showTags`는
각 영역을 제어한다. 실제 문자열이 전달된 영역만 공간을 사용한다. 숨긴 값은
그리기와 접근성 설명에서 제외한다. 앱은 `dateText`, `folderLabel1`,
`folderLabel2`, `tagLabel1`, `tagLabel2`를 전달할 수 있다.

데이터와 가시성 변경은 기존 행을 유지하고 필요한 레이아웃과 그리기만 갱신한다.
빈 화면에서 제품이 쓰는 세 플래그를 모두 끄면 기존 두 줄 항목의 크기를 유지한다.
