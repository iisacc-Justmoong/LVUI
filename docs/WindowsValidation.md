# Windows 검증

Windows에서도 C++23 AOT와 언어 검사를 동일하게 실행한다. Avatar는 기본 경로에서
LVUI 자체 구현으로 생성되어야 하며, 프레임워크 옵션 없이도 성공해야 한다.
생성 코드에서 LVRS 헤더와 타입이 없는지 별도로 검사한다. 실제 Windows에서의
설치·화면·접근성 검증은 컴파일 검증과 구분해 기록한다.

예약어 조사 검사의 임시 소스는 UTF-8과 LF로 작성한 뒤 명시적으로 CRLF로 변환한다.
Windows 텍스트 출력의 자동 CRLF 변환과 테스트의 바이트 변환이 중첩되어 CR이
중복되는 fixture 오류를 방지한다. 실제 내용 변경과 조사 스냅샷의 drift 검사는
유지한다. LVRS 원본을 갱신하면 `tools/update_lvrs_keywords.py --lvrs-root ../LVRS`로
조사 문서와 스냅샷을 재생성한 뒤 `--check`로 다시 검증한다.
생성 문서·목록·헤더는 UTF-8/LF 바이트로 저장하며 내용이 같은 파일의 수정 시각은
보존한다. 생성 결과의 실제 바이트와 수정 시각을 회귀 검사한다.

전체 구성·빌드·CTest 출력은 `build/`에 저장한다. 공통 Windows 빌드 환경과
검증 명령은 Workspace의 `SDK/Windows/README.md`를 기준으로 한다.
컴파일 성공과 실제 테스트·설치 성공을 각각 기록한다.
