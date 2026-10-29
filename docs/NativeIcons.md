# LVUI 독립 아이콘 자산

아이콘은 LVUI가 소유한 벡터 디자인 자산과 앱이 교체 가능한 `IconSource`로
공급한다. `IconCatalog`는 이름과 불변 SVG 텍스트를 보유하는 표준 C++ 목록이다.
중복 이름, 잘못된 UTF-8, 빈 내용 및 자산 수·바이트 한도를 검증하고, 알 수 없는
이름은 오류를 반환한다. 불변 카탈로그는 동일 자산의 공유 소유권을 제공한다.

기본 자산은 `resources/icons/`에 보관하고 빌드할 때 결정적으로 임베드한다.
현재 기본 이름은 `generaladd`, `home`, `nodesfolder`, `projectStructure`, `procedure`,
`generalchevronDown`, `generalchevronUp`, `generalchevronUpDown`, `generalchevronRight`,
`generalchevronLeft`, `alertAdjustments`, `alertFileText`, `colorEyedropper`이다. 경고창은 액션 개수에
따라 뒤의 두 자산 중 하나를 기본으로 사용한다. 나머지
이름은 앱이 별도 `IconSource`를 제공해야 한다. 변경 가능한 공급자는 자산 변경마다
`revision()`을 갱신해야 한다. `iconSource`에 앱의 `shared_ptr<const IconSource>`를
넘기고 `iconRevision`에 관찰 가능한 0 이상의 정수를 넘기면 앱이 공급자를 갱신할 때
반응형 값도 함께 바꾸어 아이콘 재조회가 일어난다. 같은 공급자의 내부 변경은
자동으로 관찰되지 않는다. `iconName`과 `iconRevision`도 반응형 값일 수 있다.

`NativeIconCache`는 앱이 공급한 범용 iiGUI SVG 디코더를 이용해 메모리에서만
래스터화한다. 이름·공급자·revision·출력 크기가 같으면 불변 픽셀을 재사용한다.
잘못된 SVG나 없는 자산은 오류를 반환하며 이전 캐시를 보존한다. 공급자가 조회 중
계속 바뀌면 재시도 횟수를 제한한다. 캐시는 생성한 스레드에서만 조회할 수 있다.
현재 픽셀 크기는 논리 크기의 두 배이다.

기본 버튼과 드롭다운은 이름, 공급자, revision을 검증한 뒤 기존 컨트롤에 반영한다.
잘못된 후보는 기존 픽셀을 유지하고 정상 후보로 복구한다. 공급자 조회 중 새로운
설정이 들어오면 오래된 후보가 이를 덮지 않는다. 내장 SVG는 프로젝트가 직접 작성한
디자인이며 이전 아이콘과 픽셀 단위로 같다는 의미는 아니다. 더 많은 디자인 자산,
배율 변화 대응과 실제 플랫폼 시각 검증이 남아 있다.
