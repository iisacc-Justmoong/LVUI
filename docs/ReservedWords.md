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

| 예약어 | 분류 | 원본 | 내부 타입 |
|---|---|---|---|
| `AbstractButton` | View | `src/qml/components/control/buttons/AbstractButton.qml:5` | 아니오 |
| `AbstractInputBar` | View | `src/qml/components/control/input/AbstractInputBar.qml:5` | 아니오 |
| `ActionSlot` | View | `src/qml/components/navigation/ListItemComposite.qml:99` | 예 |
| `AdaptiveLayoutHost` | View | `src/qml/ApplicationWindow.qml:566` | 예 |
| `Alert` | View | `src/qml/components/surfaces/Alert.qml:6` | 아니오 |
| `AlertButton` | View | `src/qml/components/surfaces/AlertButton.qml:4` | 아니오 |
| `AppBootstrapWindow` | View | `src/qml/AppBootstrapWindow.qml:4` | 아니오 |
| `AppCard` | View | `src/qml/components/surfaces/AppCard.qml:6` | 아니오 |
| `AppHeader` | View | `src/qml/components/layout/AppHeader.qml:6` | 아니오 |
| `AppShell` | View | `src/qml/AppShell.qml:4` | 아니오 |
| `ApplicationWindow` | View | `src/qml/ApplicationWindow.qml:8` | 아니오 |
| `Avatar` | View | `src/qml/components/control/display/Avatar.qml:5` | 아니오 |
| `BodyText` | View | `src/qml/components/navigation/ListItemComposite.qml:16` | 예 |
| `CaptionText` | View | `src/qml/components/navigation/ListItemComposite.qml:27` | 예 |
| `Card` | View | `src/qml/components/surfaces/Card.qml:6` | 아니오 |
| `CardImageContent` | View | `src/qml/components/surfaces/CardImageContent.qml:5` | 예 |
| `CardInformationContent` | View | `src/qml/components/surfaces/CardInformationContent.qml:5` | 예 |
| `ChannelRow` | View | `src/qml/components/control/input/ColorPicker.qml:131` | 예 |
| `CheckBox` | View | `src/qml/components/control/check/CheckBox.qml:4` | 아니오 |
| `CodeEditor` | View | `src/qml/components/control/input/CodeEditor.qml:5` | 아니오 |
| `Color` | 타입 | LVUI 언어 | 아니오 |
| `ColorField` | View | `src/qml/components/control/input/ColorPicker.qml:225` | 예 |
| `ColorInput` | View | `src/qml/components/control/input/ColorPicker.qml:71` | 예 |
| `ColorPicker` | View | `src/qml/components/control/input/ColorPicker.qml:5` | 아니오 |
| `ColorPickerButton` | View | `src/qml/components/control/buttons/ColorPickerButton.qml:5` | 아니오 |
| `ColorPickerSurface` | View | `src/backend/graphics/colorpickersurface.h:13` | 아니오 |
| `ComboBox` | View | `src/qml/components/control/buttons/ComboBox.qml:4` | 아니오 |
| `ContentText` | View | `src/qml/components/navigation/ListItemComposite.qml:82` | 예 |
| `ContextMenu` | View | `src/qml/components/navigation/ContextMenu.qml:6` | 아니오 |
| `ContextMenuDivider` | View | `src/qml/components/navigation/ContextMenuDivider.qml:5` | 아니오 |
| `ContextMenuItem` | View | `src/qml/components/navigation/ContextMenuItem.qml:5` | 아니오 |
| `DetailRow` | View | `src/qml/components/surfaces/CardInformationContent.qml:10` | 예 |
| `DropdownButton` | View | `src/qml/components/control/buttons/DropdownButton.qml:4` | 아니오 |
| `Endpoint` | View | `src/qml/components/control/input/Slider.qml:103` | 예 |
| `EventListener` | View | `src/qml/components/control/util/EventListener.qml:5` | 아니오 |
| `FocusRing` | View | `src/qml/components/control/util/FocusRing.qml:4` | 아니오 |
| `Glyph` | View | `src/qml/components/navigation/ListItemComposite.qml:41` | 예 |
| `HStack` | View | `src/qml/components/layout/HStack.qml:5` | 아니오 |
| `HelpButton` | View | `src/qml/components/control/buttons/HelpButton.qml:5` | 아니오 |
| `Hierarchy` | View | `src/qml/components/navigation/Hierarchy.qml:5` | 아니오 |
| `HierarchyItem` | View | `src/qml/components/navigation/HierarchyItem.qml:5` | 아니오 |
| `HierarchyList` | View | `src/qml/components/navigation/HierarchyList.qml:4` | 아니오 |
| `HierarchyToolbar` | View | `src/qml/components/navigation/HierarchyToolbar.qml:5` | 아니오 |
| `IconButton` | View | `src/qml/components/control/buttons/IconButton.qml:4` | 아니오 |
| `IconMenuButton` | View | `src/qml/components/control/buttons/IconMenuButton.qml:4` | 아니오 |
| `IconSegmentedControl` | View | `src/qml/components/control/buttons/IconSegmentedControl.qml:4` | 아니오 |
| `Image` | View | `src/LVRS/Image.h:6` | 아니오 |
| `InputField` | View | `src/qml/components/control/input/InputField.qml:7` | 아니오 |
| `InputMethodGuard` | View | `src/qml/components/control/util/InputMethodGuard.qml:4` | 아니오 |
| `InputSlot` | View | `src/qml/components/navigation/ListItemComposite.qml:227` | 예 |
| `InteractionMotion` | 시각 보조 | `src/qml/components/control/util/InteractionMotion.qml:4` | 아니오 |
| `Label` | View | `src/qml/components/control/display/Label.qml:4` | 아니오 |
| `LabelButton` | View | `src/qml/components/control/buttons/LabelButton.qml:4` | 아니오 |
| `LabelMenuButton` | View | `src/qml/components/control/buttons/LabelMenuButton.qml:4` | 아니오 |
| `LabelSegmentedControl` | View | `src/qml/components/control/buttons/LabelSegmentedControl.qml:4` | 아니오 |
| `Link` | View | `src/qml/components/navigation/Link.qml:4` | 아니오 |
| `List` | View | `src/qml/components/navigation/List.qml:6` | 아니오 |
| `ListFooter` | View | `src/qml/components/navigation/ListFooter.qml:6` | 아니오 |
| `ListItem` | View | `src/qml/components/navigation/ListItem.qml:4` | 아니오 |
| `ListItemComposite` | View | `src/qml/components/navigation/ListItemComposite.qml:6` | 아니오 |
| `ListItemSelector` | View | `src/qml/components/navigation/ListItemSelector.qml:5` | 아니오 |
| `ListToolbar` | View | `src/qml/components/navigation/ListToolbar.qml:6` | 아니오 |
| `Material` | View | `src/qml/components/control/input/Slider.qml:140` | 예 |
| `MaterialSurface` | View | `src/qml/components/surfaces/MaterialSurface.qml:7` | 아니오 |
| `Menu` | View | `src/qml/components/navigation/Menu.qml:5` | 아니오 |
| `MenuDivider` | View | `src/qml/components/navigation/MenuDivider.qml:4` | 아니오 |
| `MenuItem` | View | `src/qml/components/navigation/MenuItem.qml:4` | 아니오 |
| `Metric` | 타입 | LVUI 언어 | 아니오 |
| `MobileNavigationBar` | View | `src/qml/components/navigation/MobileNavigationBar.qml:6` | 아니오 |
| `MobileNavigationTab` | View | `src/qml/components/navigation/MobileNavigationTab.qml:6` | 아니오 |
| `MobileTab` | View | `src/qml/components/navigation/MobileTab.qml:6` | 아니오 |
| `MobileTabBar` | View | `src/qml/components/navigation/MobileTabBar.qml:5` | 아니오 |
| `Modal` | View | `src/qml/components/surfaces/Modal.qml:5` | 아니오 |
| `MoreSlot` | View | `src/qml/components/navigation/ListItemComposite.qml:118` | 예 |
| `NativeView` | View | `../iiGUI/src/iiGUI/NativeView.h:20` | 아니오 |
| `PageRouter` | View | `src/qml/components/navigation/PageRouter.qml:5` | 아니오 |
| `PanelMaterial` | View | `src/qml/components/surfaces/PanelMaterial.qml:4` | 아니오 |
| `Popover` | View | `src/qml/components/surfaces/Popover.qml:5` | 아니오 |
| `PreviewSlot` | View | `src/qml/components/navigation/ListItemComposite.qml:292` | 예 |
| `ProgressBar` | View | `src/qml/components/control/display/ProgressBar.qml:4` | 아니오 |
| `PushButton` | View | `src/qml/components/control/buttons/PushButton.qml:5` | 아니오 |
| `QuantityControl` | View | `src/qml/components/navigation/ListItemComposite.qml:189` | 예 |
| `QuantitySlot` | View | `src/qml/components/navigation/ListItemComposite.qml:257` | 예 |
| `RadialLayer` | View | `src/qml/components/surfaces/MaterialSurface.qml:95` | 예 |
| `RadioButton` | View | `src/qml/components/control/check/RadioButton.qml:4` | 아니오 |
| `SegmentsSlot` | View | `src/qml/components/navigation/ListItemComposite.qml:263` | 예 |
| `SelectionSlot` | View | `src/qml/components/navigation/ListItemComposite.qml:162` | 예 |
| `SelectorSlot` | View | `src/qml/components/navigation/ListItemComposite.qml:175` | 예 |
| `Sheet` | View | `src/qml/components/surfaces/Sheet.qml:5` | 아니오 |
| `Slider` | View | `src/qml/components/control/input/Slider.qml:6` | 아니오 |
| `Slot` | View | `src/qml/components/navigation/ListItemComposite.qml:62` | 예 |
| `Spacer` | View | `src/qml/components/layout/Spacer.qml:4` | 아니오 |
| `SpringBehavior` | 시각 보조 | `src/qml/components/control/util/SpringBehavior.qml:4` | 아니오 |
| `StateColorBehavior` | 시각 보조 | `src/qml/components/control/util/StateColorBehavior.qml:4` | 아니오 |
| `Stepper` | View | `src/qml/components/control/buttons/Stepper.qml:4` | 아니오 |
| `Swatch` | View | `src/qml/components/control/input/ColorPicker.qml:102` | 예 |
| `Tab` | View | `src/qml/components/navigation/Tab.qml:6` | 아니오 |
| `TabBar` | View | `src/qml/components/navigation/TabBar.qml:6` | 아니오 |
| `Table` | View | `src/qml/components/control/display/Table.qml:5` | 아니오 |
| `TableCellItem` | View | `src/qml/components/control/display/TableCellItem.qml:4` | 아니오 |
| `TableHeader` | View | `src/qml/components/control/display/TableHeader.qml:5` | 아니오 |
| `TableRow` | View | `src/qml/components/control/display/TableRow.qml:5` | 아니오 |
| `TextEditor` | View | `src/qml/components/control/input/TextEditor.qml:5` | 아니오 |
| `ToggleSlot` | View | `src/qml/components/navigation/ListItemComposite.qml:150` | 예 |
| `ToggleSwitch` | View | `src/qml/components/control/check/ToggleSwitch.qml:6` | 아니오 |
| `ToolbarButton` | View | `src/qml/components/navigation/ToolbarButton.qml:4` | 아니오 |
| `Tooltip` | View | `src/qml/components/surfaces/Tooltip.qml:5` | 아니오 |
| `VStack` | View | `src/qml/components/layout/VStack.qml:5` | 아니오 |
| `View` | 타입 | LVUI 언어 | 아니오 |
| `WheelScrollGuard` | View | `src/qml/components/control/util/WheelScrollGuard.qml:4` | 아니오 |
| `Window` | View | `src/qml/Window.qml:5` | 아니오 |
| `WindowChromeInteraction` | View | `src/qml/WindowChromeInteraction.qml:5` | 아니오 |
| `WindowMaterial` | View | `src/qml/components/surfaces/WindowMaterial.qml:5` | 아니오 |
| `WindowNoiseTexture` | View | `src/backend/graphics/windownoisetexture.h:15` | 아니오 |
| `ZStack` | View | `src/qml/components/layout/ZStack.qml:3` | 아니오 |
| `false` | 리터럴 | LVUI 언어 | 아니오 |
| `import` | 선언 | LVUI 언어 | 아니오 |
| `let` | 선언 | LVUI 언어 | 아니오 |
| `true` | 리터럴 | LVUI 언어 | 아니오 |
| `var` | 선언 | LVUI 언어 | 아니오 |

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
