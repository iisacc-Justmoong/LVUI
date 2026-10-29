# C++ 렌더링 상태를 표시하는 LVUI

RenderMonitor의 getter와 명령을 일반 C++ 호출로 연결한다. getter의 Core 관측을 읽으므로
숨겨진 C++ 통계가 바뀌어도 필요한 표시가 갱신된다. @Static은 활성화 시 값을 고정한다.
프레임 수·평균·백분위·종료·오류 처리는 C++에 있으며 LVUI에는 계산식을 복제하지 않는다.

애플리케이션은 기존 iiSDKCore 수명 안에서 Host의 presentation_source를 가져와 C++
백엔드 생성자로 전달하거나 RenderMonitor.attachWindow(host)를 호출한다. 원천은 창을
소유하지 않으며 scoped import 퇴역 시 모니터의 lease만 제거한다. 공유된 다른 모니터나
파일/생성/저장 상태를 변경하지 않는다.

NativeRender와 NativeRenderSelection fixture는 기본 import 및 중첩 C++ 백엔드의
상태·스냅샷·명령을 실제 AOT C++로 생성한다. Frame 이벤트 후 입력·포커스·노드·Static
값과 파일 revision 유지, 소스 교체·종료·퇴역을 검사 대상으로 둔다. fixture의 프레임은
결정적 C++ 시험 데이터이며 실제 장치 성능 측정으로 표시하지 않는다.

모델의 원본 수치와 소유 계약은 [RenderMonitor 문서](../../LVRS/docs/native/RenderMonitor.md),
원천의 전달·종료와 플랫폼 경계는 [Presentation 문서](../../iiGUI/docs/Presentation.md)를
참조한다. LVRS 모델·AOT 실행은 전체 작업의 최종 일괄 단계에 수행한다.
