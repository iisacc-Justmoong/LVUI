# 예약어와 LVRS 시각 컴포넌트

> 이 문서는 레거시 LVRS/QML 소스 조사 당시의 역사적 기록이다. 현재 언어의 예약어
> 등록과 갱신은 [LVUI 예약어 목록](KeywordRegistry.md)을 따른다. 아래 명령으로
> LVUI의 현재 예약어 목록을 갱신하지 않는다.

이 문서는 `tools/update_lvrs_keywords.py`로 소스 조사 결과에서 생성한다.
조사 기준 LVRS 커밋은 `c5131090ca8df583bcf52253c5082bcef1ee3833`이며, `src/`의 378개 파일을 읽고 LF 개행으로 정규화한 SHA-256을 기록했다.
시각 컴포넌트의 고유 이름 109개와 시각 효과·동작 보조 예약어 3개를 포함한다.

## 실제 언어 구현

`LVUI/Language.h`의 `keywords`, `find_keyword`, `is_reserved_identifier`, `tokenize`, `parse_view`가 이 목록을 사용한다.
시각 컴포넌트 이름은 `View` 생성 구문으로 파싱하고, `{}` 안의 자식 뷰를 소유 트리에 보관한다.
`let`·`var` 이름과 import 바인딩에 예약어를 사용하면 진단한다. 예약은 대소문자를 구분하며 문자열과 주석의 텍스트에는 적용하지 않는다.
QML 내부 타입과 인라인 컴포넌트도 포함하며, 같은 이름의 여러 원본 선언은 조사 파일에 모두 보관한다.
LVUI의 네이티브 추가 뷰는 `native_views`에 구현 헤더와 선언 위치를 별도로 기록하며 Qt 원본 선언 수와 분리한다.
iiGUI의 NativeView는 `external_source_files`에 별도 구현 해시를 기록한다. 이 추가 뷰를 기존 LVRS 선언이나 Qt 원본의 완성 수에 합산하지 않는다.
`InteractionMotion`, `SpringBehavior`, `StateColorBehavior`는 시각 보조 예약어이며 자식 `View` 생성자로 사용하지 않는다.
이 목록은 컴파일 단계의 어휘·구문 계약이다. 현재 지원 컴포넌트의 C++23 AOT와 iiGUI 자체 위젯 마운트는 [LVRS 네이티브 구현](LVRSNative.md)에 별도로 명시한다. 예약어 등록만으로 모든 속성·상태·동작이 구현되지는 않는다.

## 예약어 목록

- 예약어: `AbstractButton`; 분류: View; 원본: `src/qml/components/control/buttons/AbstractButton.qml:5`; 내부 타입: 아니오
- 예약어: `AbstractInputBar`; 분류: View; 원본: `src/qml/components/control/input/AbstractInputBar.qml:5`; 내부 타입: 아니오
- 예약어: `ActionSlot`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:99`; 내부 타입: 예
- 예약어: `AdaptiveLayoutHost`; 분류: View; 원본: `src/qml/ApplicationWindow.qml:566`; 내부 타입: 예
- 예약어: `Alert`; 분류: View; 원본: `src/qml/components/surfaces/Alert.qml:6`; 내부 타입: 아니오
- 예약어: `AlertButton`; 분류: View; 원본: `src/qml/components/surfaces/AlertButton.qml:4`; 내부 타입: 아니오
- 예약어: `AppBootstrapWindow`; 분류: View; 원본: `src/qml/AppBootstrapWindow.qml:4`; 내부 타입: 아니오
- 예약어: `AppCard`; 분류: View; 원본: `src/qml/components/surfaces/AppCard.qml:6`; 내부 타입: 아니오
- 예약어: `AppHeader`; 분류: View; 원본: `src/qml/components/layout/AppHeader.qml:6`; 내부 타입: 아니오
- 예약어: `AppShell`; 분류: View; 원본: `src/qml/AppShell.qml:4`; 내부 타입: 아니오
- 예약어: `ApplicationWindow`; 분류: View; 원본: `src/qml/ApplicationWindow.qml:8`; 내부 타입: 아니오
- 예약어: `Avatar`; 분류: View; 원본: `src/qml/components/control/display/Avatar.qml:5`; 내부 타입: 아니오
- 예약어: `BodyText`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:16`; 내부 타입: 예
- 예약어: `CaptionText`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:27`; 내부 타입: 예
- 예약어: `Card`; 분류: View; 원본: `src/qml/components/surfaces/Card.qml:6`; 내부 타입: 아니오
- 예약어: `CardImageContent`; 분류: View; 원본: `src/qml/components/surfaces/CardImageContent.qml:5`; 내부 타입: 예
- 예약어: `CardInformationContent`; 분류: View; 원본: `src/qml/components/surfaces/CardInformationContent.qml:5`; 내부 타입: 예
- 예약어: `ChannelRow`; 분류: View; 원본: `src/qml/components/control/input/ColorPicker.qml:131`; 내부 타입: 예
- 예약어: `CheckBox`; 분류: View; 원본: `src/qml/components/control/check/CheckBox.qml:4`; 내부 타입: 아니오
- 예약어: `CodeEditor`; 분류: View; 원본: `src/qml/components/control/input/CodeEditor.qml:5`; 내부 타입: 아니오
- 예약어: `Color`; 분류: 타입; 원본: LVUI 언어; 내부 타입: 아니오
- 예약어: `ColorField`; 분류: View; 원본: `src/qml/components/control/input/ColorPicker.qml:225`; 내부 타입: 예
- 예약어: `ColorInput`; 분류: View; 원본: `src/qml/components/control/input/ColorPicker.qml:71`; 내부 타입: 예
- 예약어: `ColorPicker`; 분류: View; 원본: `src/qml/components/control/input/ColorPicker.qml:5`; 내부 타입: 아니오
- 예약어: `ColorPickerButton`; 분류: View; 원본: `src/qml/components/control/buttons/ColorPickerButton.qml:5`; 내부 타입: 아니오
- 예약어: `ColorPickerSurface`; 분류: View; 원본: `src/backend/graphics/colorpickersurface.h:13`; 내부 타입: 아니오
- 예약어: `ComboBox`; 분류: View; 원본: `src/qml/components/control/buttons/ComboBox.qml:4`; 내부 타입: 아니오
- 예약어: `ContentText`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:82`; 내부 타입: 예
- 예약어: `ContextMenu`; 분류: View; 원본: `src/qml/components/navigation/ContextMenu.qml:6`; 내부 타입: 아니오
- 예약어: `ContextMenuDivider`; 분류: View; 원본: `src/qml/components/navigation/ContextMenuDivider.qml:5`; 내부 타입: 아니오
- 예약어: `ContextMenuItem`; 분류: View; 원본: `src/qml/components/navigation/ContextMenuItem.qml:5`; 내부 타입: 아니오
- 예약어: `DetailRow`; 분류: View; 원본: `src/qml/components/surfaces/CardInformationContent.qml:10`; 내부 타입: 예
- 예약어: `DropdownButton`; 분류: View; 원본: `src/qml/components/control/buttons/DropdownButton.qml:4`; 내부 타입: 아니오
- 예약어: `Endpoint`; 분류: View; 원본: `src/qml/components/control/input/Slider.qml:103`; 내부 타입: 예
- 예약어: `EventListener`; 분류: View; 원본: `src/qml/components/control/util/EventListener.qml:5`; 내부 타입: 아니오
- 예약어: `FocusRing`; 분류: View; 원본: `src/qml/components/control/util/FocusRing.qml:4`; 내부 타입: 아니오
- 예약어: `Glyph`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:41`; 내부 타입: 예
- 예약어: `HStack`; 분류: View; 원본: `src/qml/components/layout/HStack.qml:5`; 내부 타입: 아니오
- 예약어: `HelpButton`; 분류: View; 원본: `src/qml/components/control/buttons/HelpButton.qml:5`; 내부 타입: 아니오
- 예약어: `Hierarchy`; 분류: View; 원본: `src/qml/components/navigation/Hierarchy.qml:5`; 내부 타입: 아니오
- 예약어: `HierarchyItem`; 분류: View; 원본: `src/qml/components/navigation/HierarchyItem.qml:5`; 내부 타입: 아니오
- 예약어: `HierarchyList`; 분류: View; 원본: `src/qml/components/navigation/HierarchyList.qml:4`; 내부 타입: 아니오
- 예약어: `HierarchyToolbar`; 분류: View; 원본: `src/qml/components/navigation/HierarchyToolbar.qml:5`; 내부 타입: 아니오
- 예약어: `IconButton`; 분류: View; 원본: `src/qml/components/control/buttons/IconButton.qml:4`; 내부 타입: 아니오
- 예약어: `IconMenuButton`; 분류: View; 원본: `src/qml/components/control/buttons/IconMenuButton.qml:4`; 내부 타입: 아니오
- 예약어: `IconSegmentedControl`; 분류: View; 원본: `src/qml/components/control/buttons/IconSegmentedControl.qml:4`; 내부 타입: 아니오
- 예약어: `Image`; 분류: View; 원본: `src/LVRS/Image.h:6`; 내부 타입: 아니오
- 예약어: `InputField`; 분류: View; 원본: `src/qml/components/control/input/InputField.qml:7`; 내부 타입: 아니오
- 예약어: `InputMethodGuard`; 분류: View; 원본: `src/qml/components/control/util/InputMethodGuard.qml:4`; 내부 타입: 아니오
- 예약어: `InputSlot`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:227`; 내부 타입: 예
- 예약어: `InteractionMotion`; 분류: 시각 보조; 원본: `src/qml/components/control/util/InteractionMotion.qml:4`; 내부 타입: 아니오
- 예약어: `Label`; 분류: View; 원본: `src/qml/components/control/display/Label.qml:4`; 내부 타입: 아니오
- 예약어: `LabelButton`; 분류: View; 원본: `src/qml/components/control/buttons/LabelButton.qml:4`; 내부 타입: 아니오
- 예약어: `LabelMenuButton`; 분류: View; 원본: `src/qml/components/control/buttons/LabelMenuButton.qml:4`; 내부 타입: 아니오
- 예약어: `LabelSegmentedControl`; 분류: View; 원본: `src/qml/components/control/buttons/LabelSegmentedControl.qml:4`; 내부 타입: 아니오
- 예약어: `Link`; 분류: View; 원본: `src/qml/components/navigation/Link.qml:4`; 내부 타입: 아니오
- 예약어: `List`; 분류: View; 원본: `src/qml/components/navigation/List.qml:6`; 내부 타입: 아니오
- 예약어: `ListFooter`; 분류: View; 원본: `src/qml/components/navigation/ListFooter.qml:6`; 내부 타입: 아니오
- 예약어: `ListItem`; 분류: View; 원본: `src/qml/components/navigation/ListItem.qml:4`; 내부 타입: 아니오
- 예약어: `ListItemComposite`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:6`; 내부 타입: 아니오
- 예약어: `ListItemSelector`; 분류: View; 원본: `src/qml/components/navigation/ListItemSelector.qml:5`; 내부 타입: 아니오
- 예약어: `ListToolbar`; 분류: View; 원본: `src/qml/components/navigation/ListToolbar.qml:6`; 내부 타입: 아니오
- 예약어: `Material`; 분류: View; 원본: `src/qml/components/control/input/Slider.qml:140`; 내부 타입: 예
- 예약어: `MaterialSurface`; 분류: View; 원본: `src/qml/components/surfaces/MaterialSurface.qml:7`; 내부 타입: 아니오
- 예약어: `Menu`; 분류: View; 원본: `src/qml/components/navigation/Menu.qml:5`; 내부 타입: 아니오
- 예약어: `MenuDivider`; 분류: View; 원본: `src/qml/components/navigation/MenuDivider.qml:4`; 내부 타입: 아니오
- 예약어: `MenuItem`; 분류: View; 원본: `src/qml/components/navigation/MenuItem.qml:4`; 내부 타입: 아니오
- 예약어: `Metric`; 분류: 타입; 원본: LVUI 언어; 내부 타입: 아니오
- 예약어: `MobileNavigationBar`; 분류: View; 원본: `src/qml/components/navigation/MobileNavigationBar.qml:6`; 내부 타입: 아니오
- 예약어: `MobileNavigationTab`; 분류: View; 원본: `src/qml/components/navigation/MobileNavigationTab.qml:6`; 내부 타입: 아니오
- 예약어: `MobileTab`; 분류: View; 원본: `src/qml/components/navigation/MobileTab.qml:6`; 내부 타입: 아니오
- 예약어: `MobileTabBar`; 분류: View; 원본: `src/qml/components/navigation/MobileTabBar.qml:5`; 내부 타입: 아니오
- 예약어: `Modal`; 분류: View; 원본: `src/qml/components/surfaces/Modal.qml:5`; 내부 타입: 아니오
- 예약어: `MoreSlot`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:118`; 내부 타입: 예
- 예약어: `NativeView`; 분류: View; 원본: `../iiGUI/src/iiGUI/NativeView.h:20`; 내부 타입: 아니오
- 예약어: `PageRouter`; 분류: View; 원본: `src/qml/components/navigation/PageRouter.qml:5`; 내부 타입: 아니오
- 예약어: `PanelMaterial`; 분류: View; 원본: `src/qml/components/surfaces/PanelMaterial.qml:4`; 내부 타입: 아니오
- 예약어: `Popover`; 분류: View; 원본: `src/qml/components/surfaces/Popover.qml:5`; 내부 타입: 아니오
- 예약어: `PreviewSlot`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:292`; 내부 타입: 예
- 예약어: `ProgressBar`; 분류: View; 원본: `src/qml/components/control/display/ProgressBar.qml:4`; 내부 타입: 아니오
- 예약어: `PushButton`; 분류: View; 원본: `src/qml/components/control/buttons/PushButton.qml:5`; 내부 타입: 아니오
- 예약어: `QuantityControl`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:189`; 내부 타입: 예
- 예약어: `QuantitySlot`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:257`; 내부 타입: 예
- 예약어: `RadialLayer`; 분류: View; 원본: `src/qml/components/surfaces/MaterialSurface.qml:95`; 내부 타입: 예
- 예약어: `RadioButton`; 분류: View; 원본: `src/qml/components/control/check/RadioButton.qml:4`; 내부 타입: 아니오
- 예약어: `SegmentsSlot`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:263`; 내부 타입: 예
- 예약어: `SelectionSlot`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:162`; 내부 타입: 예
- 예약어: `SelectorSlot`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:175`; 내부 타입: 예
- 예약어: `Sheet`; 분류: View; 원본: `src/qml/components/surfaces/Sheet.qml:5`; 내부 타입: 아니오
- 예약어: `Slider`; 분류: View; 원본: `src/qml/components/control/input/Slider.qml:6`; 내부 타입: 아니오
- 예약어: `Slot`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:62`; 내부 타입: 예
- 예약어: `Spacer`; 분류: View; 원본: `src/qml/components/layout/Spacer.qml:4`; 내부 타입: 아니오
- 예약어: `SpringBehavior`; 분류: 시각 보조; 원본: `src/qml/components/control/util/SpringBehavior.qml:4`; 내부 타입: 아니오
- 예약어: `StateColorBehavior`; 분류: 시각 보조; 원본: `src/qml/components/control/util/StateColorBehavior.qml:4`; 내부 타입: 아니오
- 예약어: `Stepper`; 분류: View; 원본: `src/qml/components/control/buttons/Stepper.qml:4`; 내부 타입: 아니오
- 예약어: `Swatch`; 분류: View; 원본: `src/qml/components/control/input/ColorPicker.qml:102`; 내부 타입: 예
- 예약어: `Tab`; 분류: View; 원본: `src/qml/components/navigation/Tab.qml:6`; 내부 타입: 아니오
- 예약어: `TabBar`; 분류: View; 원본: `src/qml/components/navigation/TabBar.qml:6`; 내부 타입: 아니오
- 예약어: `Table`; 분류: View; 원본: `src/qml/components/control/display/Table.qml:5`; 내부 타입: 아니오
- 예약어: `TableCellItem`; 분류: View; 원본: `src/qml/components/control/display/TableCellItem.qml:4`; 내부 타입: 아니오
- 예약어: `TableHeader`; 분류: View; 원본: `src/qml/components/control/display/TableHeader.qml:5`; 내부 타입: 아니오
- 예약어: `TableRow`; 분류: View; 원본: `src/qml/components/control/display/TableRow.qml:5`; 내부 타입: 아니오
- 예약어: `TextEditor`; 분류: View; 원본: `src/qml/components/control/input/TextEditor.qml:5`; 내부 타입: 아니오
- 예약어: `ToggleSlot`; 분류: View; 원본: `src/qml/components/navigation/ListItemComposite.qml:150`; 내부 타입: 예
- 예약어: `ToggleSwitch`; 분류: View; 원본: `src/qml/components/control/check/ToggleSwitch.qml:6`; 내부 타입: 아니오
- 예약어: `ToolbarButton`; 분류: View; 원본: `src/qml/components/navigation/ToolbarButton.qml:4`; 내부 타입: 아니오
- 예약어: `Tooltip`; 분류: View; 원본: `src/qml/components/surfaces/Tooltip.qml:5`; 내부 타입: 아니오
- 예약어: `VStack`; 분류: View; 원본: `src/qml/components/layout/VStack.qml:5`; 내부 타입: 아니오
- 예약어: `View`; 분류: 타입; 원본: LVUI 언어; 내부 타입: 아니오
- 예약어: `WheelScrollGuard`; 분류: View; 원본: `src/qml/components/control/util/WheelScrollGuard.qml:4`; 내부 타입: 아니오
- 예약어: `Window`; 분류: View; 원본: `src/qml/Window.qml:5`; 내부 타입: 아니오
- 예약어: `WindowChromeInteraction`; 분류: View; 원본: `src/qml/WindowChromeInteraction.qml:5`; 내부 타입: 아니오
- 예약어: `WindowMaterial`; 분류: View; 원본: `src/qml/components/surfaces/WindowMaterial.qml:5`; 내부 타입: 아니오
- 예약어: `WindowNoiseTexture`; 분류: View; 원본: `src/backend/graphics/windownoisetexture.h:15`; 내부 타입: 아니오
- 예약어: `ZStack`; 분류: View; 원본: `src/qml/components/layout/ZStack.qml:3`; 내부 타입: 아니오
- 예약어: `false`; 분류: 리터럴; 원본: LVUI 언어; 내부 타입: 아니오
- 예약어: `import`; 분류: 선언; 원본: LVUI 언어; 내부 타입: 아니오
- 예약어: `let`; 분류: 선언; 원본: LVUI 언어; 내부 타입: 아니오
- 예약어: `true`; 분류: 리터럴; 원본: LVUI 언어; 내부 타입: 아니오
- 예약어: `var`; 분류: 선언; 원본: LVUI 언어; 내부 타입: 아니오

## 비시각 객체

다음 이름은 화면 트리의 `View`가 아닌 서비스·모델·상태 객체이므로 시각 컴포넌트 예약어에서 제외한다.

- `FontPolicy`: `src/backend/fonts/fontpolicy.h:11`, 기반 `QObject`.
- `ColorPickerModel`: `src/backend/graphics/colorpickermodel.h:13`, 기반 `QObject`.
- `SvgManager`: `src/backend/graphics/svgmanager.h:20`, 기반 `QObject`.
- `Backend`: `src/backend/io/backend.h:25`, 기반 `QObject`.
- `HierarchyModel`: `src/backend/model/hierarchymodel.h:17`, 기반 `QObject`.
- `ModelSource`: `src/backend/model/modelsource.h:15`, 기반 `QObject`.
- `ModelUndoStack`: `src/backend/model/modelundostack.h:12`, 기반 `QObject`.
- `ProgressModel`: `src/backend/model/progressmodel.h:15`, 기반 `QObject`.
- `TableHeaderModel`: `src/backend/model/tableheadermodel.h:13`, 기반 `QObject`.
- `TableModel`: `src/backend/model/tablemodel.h:19`, 기반 `QObject`.
- `ModelAdapter`: `src/backend/navigation/modeladapter.h:14`, 기반 `QObject`.
- `NavigationStackModel`: `src/backend/navigation/navigationstackmodel.h:14`, 기반 `QObject`.
- `PageMonitor`: `src/backend/navigation/pagemonitor.h:11`, 기반 `QObject`.
- `RouteMatcher`: `src/backend/navigation/routematcher.h:11`, 기반 `QObject`.
- `RouteResolver`: `src/backend/navigation/routeresolver.h:16`, 기반 `QObject`.
- `ViewStateTracker`: `src/backend/navigation/viewstatetracker.h:15`, 기반 `QObject`.
- `NativeWindowInteraction`: `src/backend/platform/nativewindowinteraction.h:16`, 기반 `QObject`.
- `NativeWindowStyle`: `src/backend/platform/nativewindowstyle.h:12`, 기반 `QObject`.
- `Platform`: `src/backend/platform/platforminfo.h:17`, 기반 `QObject`.
- `WindowSafeAreaObserver`: `src/backend/platform/windowsafeareaobserver.h:15`, 기반 `QObject`.
- `Debug`: `src/backend/runtime/debuglogger.h:13`, 기반 `QObject`.
- `GestureEvents`: `src/backend/runtime/gestureevents.h:18`, 기반 `QObject`.
- `RenderMonitor`: `src/backend/runtime/renderingmonitor.h:16`, 기반 `QObject`.
- `RenderQuality`: `src/backend/runtime/renderquality.h:16`, 기반 `QObject`.
- `RuntimeEvents`: `src/backend/runtime/runtimeevents.h:25`, 기반 `QObject`.
- `AppState`: `src/backend/state/appstate.h:13`, 기반 `QObject`.
- `StateModel`: `src/backend/state/statemodel.h:14`, 기반 `ViewModel`.
- `ViewModel`: `src/backend/state/viewmodel.h:11`, 기반 `QObject`.
- `ViewModels`: `src/backend/state/viewmodelregistry.h:18`, 기반 `QObject`.
- `TextDocumentModel`: `src/backend/text/textdocumentmodel.h:17`, 기반 `QAbstractListModel`.
- `TextMarkup`: `src/backend/text/textmarkup.h:11`, 기반 `QObject`.
- `Motion`: `src/qml/Motion.qml:4`, 기반 `QtObject`.
- `Theme`: `src/qml/Theme.qml:5`, 기반 `QtObject`.
- `ButtonMethodRegistry`: `src/qml/components/control/buttons/ButtonMethodRegistry.qml:3`, 기반 `QtObject`.
- `InteractionState`: `src/qml/components/control/util/InteractionState.qml:4`, 기반 `QtObject`.
- `Navigator`: `src/qml/components/navigation/Navigator.qml:4`, 기반 `QtObject`.
- `PageRouterTransitionDriver`: `src/qml/components/navigation/PageRouterTransitionDriver.qml:4`, 기반 `QtObject`.
- `PageTransitionController`: `src/qml/components/navigation/PageTransitionController.qml:4`, 기반 `QtObject`.

## 목록 갱신과 검증

```sh
python3 tools/update_lvrs_keywords.py --lvrs-root ../LVRS
python3 tools/update_lvrs_keywords.py --lvrs-root ../LVRS --check
python3 tools/update_lvrs_keywords.py --snapshot-only --check
```

일반 빌드와 설치 소비자는 LVRS·Qt·이 스크립트를 요구하지 않는다. 원본 조사 테스트에서 새 파일·등록·상속·내용 변경이 발견되면 목록을 다시 조사해야 한다. 내용이 같은 출력은 파일과 수정 시각을 보존한다. 비시각 소스의 내용만 바뀌었을 때 같은 예약어 헤더를 다시 써서 하위 C++ 모듈을 재컴파일시키지 않는다. 조사 도구와 회귀 검사는 Python 3.9 이상에서 동작하며, macOS와 Windows 모두 UTF-8·LF 출력과 변경 없는 파일의 수정 시각을 보존한다.
