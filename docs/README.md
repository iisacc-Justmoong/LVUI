# LVUI documentation

- [Declarative language and graphics framework responsibility](GraphicsBoundary.md)
- [GPU scene composition and platform boundaries](../../iiGUI/docs/GpuSceneComposition.md)
- [Getting started: native applications, state, lists and tables](GettingStarted.md)
- [뷰 세션의 값, 객체, 이벤트와 타입 지정 쿼리](ViewSessions.md)
- [View declarations and imported C++ view models](ViewOnlyArchitecture.md)
- [앱 문자열 저장소와 실행 중 현지화](Localization.md)
- [앱 Scene, 창 수명과 렌더 트리](Scenes.md)
- [독립 View의 네이티브 컴파일과 런타임 부착](NativeEmbeddedViews.md)
- [Declarative application entry and single-window lifetime](NativeApplication.md)
- [LVRS native implementation migration roadmap](NativeMigrationRoadmap.md)
- [Society and Dreamscapes product migration plan](SocietyDreamscapesMigrationPlan.md)
- [Native plain-text editor](NativeTextEditor.md)
- [Native mobile navigation](NativeMobileNavigation.md)
- [Mobile tab materials, badges and motion](MobileTabEffects.md)
- [File card selection and opening](NativeCardOpen.md)
- [Navigation list item values](ListItemValue.md)
- [Detailed file list metadata](ListItemDetail.md)
- [Editor canvas query and input](NativeEditorCanvas.md)
- [Editor timeline query and input](NativeEditorTimeline.md)
- [Native label presentation](NativeLabel.md)
- [Society and Dreamscapes component coverage](ProductComponentCoverage.md)
- [LVUI-owned components and LVRS independence status](LVUIIndependence.md)
- [LVUI-owned native motion and frame injection](NativeMotion.md)
- [LVUI-owned image layout and host adapter](NativeImage.md)
- [LVUI-owned buttons and interaction host](NativeButton.md)
- [LVUI-owned divider layout and host adapter](NativeDivider.md)
- [LVUI-owned toggle state, gesture and motion](NativeToggleSwitch.md)
- [LVUI-owned checkbox state and drawing](NativeCheckBox.md)
- [LVUI-owned radio groups and motion](NativeRadioButton.md)
- [Table merge data and revision-checked editing](TableMerge.md)
- [LVUI-owned materials and custom outlines](NativeMaterials.md)
- [LVUI-owned links and navigation interface](NativeLinks.md)
- [LVUI-owned route matching and resolver](NativeRoutes.md)
- [LVUI-owned transactional navigation history](NativeNavigationHistory.md)
- [LVUI-owned page routing and native host](NativePageRouter.md)
- [LVUI-owned AppCard model and host adapter](NativeAppCard.md)
- [LVUI-owned AppHeader model and host adapter](NativeAppHeader.md)
- [LVUI-owned event listener and host boundary](NativeEventListener.md)
- [LVUI-owned Avatar and legacy Card media contracts](NativeMedia.md)
- [LVUI-owned Tooltip placement and timing model](NativeTooltipModel.md)
- [LVUI document layout and application-side host adapter](NativeDocument.md)
- [LVUI-owned keyword registry](KeywordRegistry.md)
- [Whole-language and native-module completion audit](Completion.md)
- [Value expressions and compiler-only constant folding](Expressions.md)
- [Native dependency tracking and value operations](NativeBindings.md)
- [Native reactive stack layout, constant folding and atomic geometry updates](NativeLayout.md)
- [Nested view declarations and scoped imports](LocalScopes.md)
- [Reusable visual values and lexical composition](ViewValues.md)
- [Typed reusable components, borrowed backends and named View slots](Components.md)
- [Shared scalar component inputs, native evaluation and dependent defaults](ScalarInputs.md)
- [Ordered View sequences and transparent content slots](ViewSequences.md)
- [Keyed native repetition and bounded row projections](Repetition.md)
- [Native natural-height measurements and sparse virtual rows](MeasuredCollections.md)
- [Native virtual collection navigation and selection](CollectionNavigation.md)
- [Returned C++ callbacks](ReturnedCallbacks.md)
- [Typed native event callback inputs](CallableInputs.md)
- [Activation-owned shared callback values](CallbackValues.md)
- [Native value-returning functions and readonly declarations](FunctionValues.md)
- [Mutable native function values and typed C++ setters](FunctionVariables.md)
- [Native reactive colors and iiColorManager value contracts](NativeColors.md)
- [Native backend construction inputs](BackendConstruction.md)
- [Activation-owned asynchronous work](ActivationWork.md)
- [Conditional view activations](ConditionalViews.md)

- [LVRS default style tokens and Figma snapshot](StyleTokens.md)
- [Color and Metric value types](StyleTypes.md)
- [Complete Color and Metric member catalog](StyleTokenMembers.md)


- [Historical LVRS native code generation](LVRSNative.md)

This directory separates the current implementation contract from the selected
LVUI language and runtime design, including the native widget and acceleration
contract and the `View` type/child-argument rules clarified on 2026-10-22.

LVUI is an AOT-compiled native language. Its current generation path targets iiGUI's
own C++ widgets and the selected Metal, DirectX/Direct3D or Vulkan graphics path. The legacy `FRAMEWORK LVRS` generation path has been removed; [LVRSNative.md](LVRSNative.md) is a historical record of that path.
Platform APIs provide windows, input and presentation. Core owns the application
lifetime; native widgets and GPU resources are instantiated by compiled C++.
Its execution model does not include a source interpreter or language VM.

The selected design follows the supplied scoped-ownership proposal. The earlier
metadata-only import and per-click instance model is retired.

## Reading map

- Document: [NativeApplication.md](NativeApplication.md); Scope: `Application` root, Scene and window scopes, generated entry and failure contract; Status: AOT and headless multi-window coverage; target-platform execution is separate
- Document: [NativeWidgetRuntime.md](NativeWidgetRuntime.md); Scope: Instantiable C++ widgets, LVRS-only appearance, Core application/entrypoint/attachment; Status: Implemented macOS path; Core remains independent of GUI/DSL
- Document: [CoreWidgetVerification.md](CoreWidgetVerification.md); Scope: Core entrypoint, C++ widget lifetime, native LVRS appearance and installed consumer; Status: Verification of the Core-owned application path
- Document: [CompilationModel.md](CompilationModel.md); Scope: AOT native widget compilation, Metal/DirectX/Vulkan acceleration and delivery to iiGUI; Status: Self-rendered iiGUI C++ widgets; platform execution coverage audited separately
- Document: [NativeAOT.md](NativeAOT.md); Scope: Initial C++ generation, supported inputs, scoped callbacks and CMake; Status: Initial AOT subset; later bindings, components, functions and ownership are linked above
- Document: [EmbeddedPages.md](EmbeddedPages.md); Scope: Independent view-body scopes in an existing builder and LVRS PageRouter AOT; Status: Code generation and compilation verified; actual execution deferred
- Document: [NativeAOTVerification.md](NativeAOTVerification.md); Scope: Earlier Hello World, AppKit tests, binary and relocated installed-consumer checks; Status: Historical macOS arm64 evidence; not full current-widget execution proof
- Document: [Language.md](Language.md); Scope: Earlier HTML-shaped `parse`/`Document` API; Status: Legacy C++ API; not current `.lvui` syntax
- Document: [ComponentLanguage.md](ComponentLanguage.md); Scope: Native C++23 tokenizer, reserved identifiers, typed child View AST and lvui-check; Status: Implemented front end; feeds the separate native AOT generator
- Document: [ReservedWords.md](ReservedWords.md); Scope: All 107 visual LVRS component keywords, three modifiers and six core words with source evidence; Status: Generated from the audited source snapshot; verified against the actual parser
- Document: [Verification.md](Verification.md); Scope: Earlier source inventory, front-end tests, CLI diagnostics and staged installed consumer; Status: Historical front-end evidence; see NativeAOTVerification for native execution
- Document: [DeclarativeSyntax.md](DeclarativeSyntax.md); Scope: Common `View` type, `{}` child-view arguments, component declarations, imported instance names, bound actions and properties; Status: Selected design; draft grammar
- Document: [MethodReferences.md](MethodReferences.md); Scope: Method references versus immediate calls, typed callback inputs and component-owned event semantics; Status: Typed event and result-valued inputs implemented; remaining general protocols are audited separately
- Document: [ReactiveState.md](ReactiveState.md); Scope: Reactive values by default, the static exception and the supplied [increment/reset counter](Examples/ReactiveCounter.lvui); Status: Scalar native tracking and static snapshots implemented; complete input/lifecycle coverage remains pending
- Document: [VariableDeclarations.md](VariableDeclarations.md); Scope: `const` constants, `var` variables, typed initializers and no-semicolon syntax; Status: Selected declaration rules; full type system remains open
- Document: [DeclarationAnnotations.md](DeclarationAnnotations.md); Scope: `@Observable` and `@Static` prefix annotations; default VM binding; Status: Selected meanings; targets, combinations and static lifetime ownership remain open
- Document: [TypeImports.md](TypeImports.md); Scope: Scoped imports that create and own C++ backend instances with public-member access; Status: Selected ownership and access semantics
- Document: [InvocationLifecycle.md](InvocationLifecycle.md); Scope: Use tracking, scope teardown, asynchronous work and deterministic destruction; Status: Selected lifecycle semantics
- Document: [RuntimeContracts.md](RuntimeContracts.md); Scope: C++ access, bridge information, property observation, Core integration and resource ownership; Status: Required design capabilities; concrete APIs remain open
- Document: [ViewModelQueries.md](ViewModelQueries.md); Scope: Default VM binding, Core query value exchange, notifications, execution domains and teardown; Status: Typed request and stream connections implemented; automatic ordinary-field observation remains open
- Document: [QueryConnections.md](QueryConnections.md); Scope: Typed QueryStream-to-View connections, UI delivery, bounds and lifecycle; Status: Native AOT and host-owned query registry
- Document: [CounterExample.md](CounterExample.md); Scope: An application window with an observable counter and increment/decrement buttons; [source](Examples/Counter.lvui); Status: Hypothetical scoped-backend example

The existing filenames `TypeImports.md` and `InvocationLifecycle.md` are retained
so earlier document links continue to work. Their contents now describe scoped
instance ownership.

## Selected rules

1. LVUI declares views, layout, styles, and bindings. Application objects,
   methods, and domain rules belong to imported C++ view models, with
   SwiftUI-inspired ownership and state observation. Existing local objects and
   executable event blocks still compile and need migration; compiler enforcement
   is not implemented. See [view-only architecture](ViewOnlyArchitecture.md).
2. An imported C++ object's public members are accessible without `@export`.
   C++ accessibility and constness apply; private and protected members are
   not exposed by import. Native observation support implements the default
   reactive value contract.
3. Activating a view or component scope activates its imports and creates the
   owned backend instances. The syntax is `import bindingName CppObjectName`,
   with the view's use name first and the C++ backend type second.
   For example, `import cpp Class` exposes the created instance as `cpp`.
   Parsing or compiling a source does not construct those instances.
4. An imported name identifies the created instance. Each separate import
   activation creates an independent instance.
5. Child views borrow an explicitly passed instance. Passing it to a child does
   not create another instance or transfer ownership.
6. Event connections and property bindings use the existing instance. Repeated
   clicks, property evaluation and redraw do not recreate it.
7. Destruction becomes eligible when no future-use path remains and all active
   uses have ended. A live import binding, event connection, subscription or
   instance-using task can constitute a use.
8. Scope removal closes admission, removes connections, cancels pending owned
   work and lets active uses retire. The object is destroyed immediately once
   safe, and its owned resources are returned.
9. Shared runtime infrastructure has a separate owner. Work that must outlive
   the view requires an explicit transfer to a longer-lived owner.
10. ViewModels send and receive values through iiSDKCore object-query contracts.
    Queries support reads, writes, commands, responses and change notifications.
11. `const` declares a constant, and `var` declares a mutable variable. Typed,
    initialized declarations use `const name: Type = initializer` or
    `var name: Type = initializer`, such as `var variable: Int = 6`.
    LVUI does not use semicolon terminators.
12. Declarations can be prefixed by `@Observable` or `@Static`.
    `@Observable` explicitly indicates the existing default observation behavior
    and is optional. `@Static` prevents value changes
    during the lifetime established for that declaration; its precise owner
    remains to be specified.
13. LVUI is compiled ahead of time into OS-native widget code, including
    Cocoa/AppKit and Win32, together with the selected Metal, DirectX/Direct3D
    or Vulkan acceleration path. The compiled UI and graphics integration are
    handed to iiGUI and mounted in the iiSDK runtime. Native widgets and GPU
    resources are created when this code runs. Execution does not interpret
    LVUI source or bytecode and does not use a JVM or .NET/CLR layer.
14. `vm.method` references a method bound to its receiver; `vm.method()`
    explicitly invokes it when evaluated. Component interfaces declare callback
    parameter types, and component implementations decide when to invoke them.
    The language does not hardcode button-click semantics into member access
    or argument passing.
15. Values use state-change tracking and binding by default, including ordinary
    LVUI variables. `Label(text: vm.value)` follows source changes without an
    observation annotation. `@Static` explicitly fixes a value for its lifetime.
    This does not relax `const`, read-only or C++ constness restrictions.
16. `View` is the common type for everything displayed visually in LVUI,
    including visual controls, layout containers and composed views. A view can
    take other views as child arguments through `{}`; those children can in turn
    take further child views through nested braces. The language type does not
    prescribe a shared C++ base class.

## Implementation boundary

The package retains the `parse`/`Document` C++ API for its separate
[legacy grammar](Language.md). Current `.lvui` source uses
`tokenize`/`parse_view`/`ViewDocument` and the [component grammar](ComponentLanguage.md).
The current front end uses the LVUI-owned
reserved-word catalog and owns typed child Views, inputs, scalar declarations and
import syntax. The native `lvui-check` tool validates component source files.
The language and native-binding targets depend on iiSDKCore Contracts; the
optional `LVUI::Runtime` target also uses the existing Core Runtime.

Parsing alone does not activate C++ objects or execute a UI. The separate
`compile_native`/`lvui-compile` path generates C++23 for the documented native
subset. LVUI is replacing legacy components with its own C++ models and paint
recipes on application-supplied iiGUI host primitives. The legacy
`FRAMEWORK LVRS` generation path has been removed; missing component features
remain in the migration roadmap. The
generated activation owns imported C++ backends, reactive
bindings, typed reusable views, function inputs, conditional content and keyed
or virtual collections. Runtime property reads, calls and updates execute as
compiled native C++; there is no LVUI source evaluator or QML dependency in this
path. OS integration stays at the window, surface and input boundaries.

The optional activation runtime uses the existing Core workers, serial queues
and task scopes for explicitly registered work and retirement. Complete type
and slot protocols, all inherited LVRS properties and behavior, rendering and
platform execution remain subject to the [completion audit](Completion.md).
Headless generated-widget execution, compile-only LVRS factories and actual
window/GPU execution are distinct verification boundaries.

## Remaining design work

- [Native code generation and compiled-target mounting](CompilationModel.md#open-questions).
- [Grammar, component definitions and expressions](DeclarativeSyntax.md#open-questions).
- [Callable types and component callback interfaces](MethodReferences.md#open-questions).
- [Type rules, variable scopes and initialization](VariableDeclarations.md#open-questions).
- [Annotation targets, binding projections and static lifetimes](DeclarationAnnotations.md#open-questions).
- [Constructor inputs and import activation details](TypeImports.md#open-questions).
- [Cancellation, teardown scheduling and invocation policies](InvocationLifecycle.md#open-questions).
- [C++ bridge information, execution domains and runtime adapters](RuntimeContracts.md#open-questions).
- [Default ViewModel binding, query mapping and value delivery](ViewModelQueries.md).
- [General reactive values and the static exception](ReactiveState.md).

The ownership model and native AOT execution model are selected. The C++23/Cocoa
subset has concrete code generation and mounting APIs. Full language syntax,
reactive binding formats, asynchronous scope ownership and remaining backends
still require implementation work.

- [Shared application style profiles](StyleProfiles.md): build-time Color/Metric members for LVUI and native C++ panels.

- [Native Window properties](NativeWindow.md): activation-owned geometry, appearance, candidate commit and retained state.

- [Readonly C++ members](ReadonlyMembers.md): const observation, member ownership, standard references and noncomparable values.
- [Typed C++ component inputs](ObjectInputs.md): native value types, shared or borrowed handles, dependent defaults and ownership.
- [Native object method queries](MethodQueries.md): nested receivers, private Observable reads, source identity and reentrant evaluation.
- [Object method references](ObjectMethodReferences.md): deferred native receivers, retained shared parents, immovable temporaries and typed callbacks.
- [Callable fields](CallableFields.md): native function publications, retained handler versions, borrowed functors and typed field inputs.
- [Returned factory captures](FactoryCaptures.md): selected receiver and temporary argument ownership with native overloads and mandatory copy elision.
- [Inferred native values](InferredValues.md): C++ type based value/function storage, aliases, fixed selections and variable replacement.

<!-- workspace-docs:document-map:start -->
## Human and agent document map

Markdown is the maintained human source. Agent JSON is a structured, source-traceable projection.

- Document: [`AGENT.md`](../AGENT.md); Agent JSON: [JSON](Entrypoints/AGENT.agent.json)
- Document: [`README.md`](../README.md); Agent JSON: [JSON](Entrypoints/README.agent.json)
- Document: [`docs/ActivationWork.md`](ActivationWork.md); Agent JSON: [JSON](ActivationWork.agent.json)
- Document: [`docs/AppSurfaces.md`](AppSurfaces.md); Agent JSON: [JSON](AppSurfaces.agent.json)
- Document: [`docs/BackendConstruction.md`](BackendConstruction.md); Agent JSON: [JSON](BackendConstruction.agent.json)
- Document: [`docs/BuildLayout.md`](BuildLayout.md); Agent JSON: [JSON](BuildLayout.agent.json)
- Document: [`docs/CallableFields.md`](CallableFields.md); Agent JSON: [JSON](CallableFields.agent.json)
- Document: [`docs/CallableInputs.md`](CallableInputs.md); Agent JSON: [JSON](CallableInputs.agent.json)
- Document: [`docs/CallbackValues.md`](CallbackValues.md); Agent JSON: [JSON](CallbackValues.agent.json)
- Document: [`docs/CodeDocument.md`](CodeDocument.md); Agent JSON: [JSON](CodeDocument.agent.json)
- Document: [`docs/CollectionNavigation.md`](CollectionNavigation.md); Agent JSON: [JSON](CollectionNavigation.agent.json)
- Document: [`docs/ColorSelection.md`](ColorSelection.md); Agent JSON: [JSON](ColorSelection.agent.json)
- Document: [`docs/ColorSurfaceRasterReuse.md`](ColorSurfaceRasterReuse.md); Agent JSON: [JSON](ColorSurfaceRasterReuse.agent.json)
- Document: [`docs/CompilationModel.md`](CompilationModel.md); Agent JSON: [JSON](CompilationModel.agent.json)
- Document: [`docs/Completion.md`](Completion.md); Agent JSON: [JSON](Completion.agent.json)
- Document: [`docs/ComponentLanguage.md`](ComponentLanguage.md); Agent JSON: [JSON](ComponentLanguage.agent.json)
- Document: [`docs/Components.md`](Components.md); Agent JSON: [JSON](Components.agent.json)
- Document: [`docs/ConditionalViews.md`](ConditionalViews.md); Agent JSON: [JSON](ConditionalViews.agent.json)
- Document: [`docs/CoreWidgetVerification.md`](CoreWidgetVerification.md); Agent JSON: [JSON](CoreWidgetVerification.agent.json)
- Document: [`docs/CounterExample.md`](CounterExample.md); Agent JSON: [JSON](CounterExample.agent.json)
- Document: [`docs/CustomWindowChrome.md`](CustomWindowChrome.md); Agent JSON: [JSON](CustomWindowChrome.agent.json)
- Document: [`docs/DeclarationAnnotations.md`](DeclarationAnnotations.md); Agent JSON: [JSON](DeclarationAnnotations.agent.json)
- Document: [`docs/DeclarativeSyntax.md`](DeclarativeSyntax.md); Agent JSON: [JSON](DeclarativeSyntax.agent.json)
- Document: [`docs/DesignIconCatalog.md`](DesignIconCatalog.md); Agent JSON: [JSON](DesignIconCatalog.agent.json)
- Document: [`docs/DynamicLinking.md`](DynamicLinking.md); Agent JSON: [JSON](DynamicLinking.agent.json)
- Document: [`docs/EditorAppearance.md`](EditorAppearance.md); Agent JSON: [JSON](EditorAppearance.agent.json)
- Document: [`docs/EmbeddedPages.md`](EmbeddedPages.md); Agent JSON: [JSON](EmbeddedPages.agent.json)
- Document: [`docs/Expressions.md`](Expressions.md); Agent JSON: [JSON](Expressions.agent.json)
- Document: [`docs/FactoryCaptures.md`](FactoryCaptures.md); Agent JSON: [JSON](FactoryCaptures.agent.json)
- Document: [`docs/FigmaComponentContracts.md`](FigmaComponentContracts.md); Agent JSON: [JSON](FigmaComponentContracts.agent.json)
- Document: [`docs/FunctionValues.md`](FunctionValues.md); Agent JSON: [JSON](FunctionValues.agent.json)
- Document: [`docs/FunctionVariables.md`](FunctionVariables.md); Agent JSON: [JSON](FunctionVariables.agent.json)
- Document: [`docs/GettingStarted.md`](GettingStarted.md); Agent JSON: [JSON](GettingStarted.agent.json)
- Document: [`docs/GraphicsBoundary.md`](GraphicsBoundary.md); Agent JSON: [JSON](GraphicsBoundary.agent.json)
- Document: [`docs/HierarchyDrag.md`](HierarchyDrag.md); Agent JSON: [JSON](HierarchyDrag.agent.json)
- Document: [`docs/ImmediateDeclaration.md`](ImmediateDeclaration.md); Agent JSON: [JSON](ImmediateDeclaration.agent.json)
- Document: [`docs/IndependenceWorkSnapshot-2026-10-29.md`](IndependenceWorkSnapshot-2026-10-29.md); Agent JSON: [JSON](IndependenceWorkSnapshot-2026-10-29.agent.json)
- Document: [`docs/InferredValues.md`](InferredValues.md); Agent JSON: [JSON](InferredValues.agent.json)
- Document: [`docs/InvocationLifecycle.md`](InvocationLifecycle.md); Agent JSON: [JSON](InvocationLifecycle.agent.json)
- Document: [`docs/KeywordRegistry.md`](KeywordRegistry.md); Agent JSON: [JSON](KeywordRegistry.agent.json)
- Document: [`docs/LVRSNative.md`](LVRSNative.md); Agent JSON: [JSON](LVRSNative.agent.json)
- Document: [`docs/LVUIIndependence.md`](LVUIIndependence.md); Agent JSON: [JSON](LVUIIndependence.agent.json)
- Document: [`docs/LVUI_LVRS_Replacement_Audit_2026-11-01.md`](LVUI_LVRS_Replacement_Audit_2026-11-01.md); Agent JSON: [JSON](LVUI_LVRS_Replacement_Audit_2026-11-01.agent.json)
- Document: [`docs/Language.md`](Language.md); Agent JSON: [JSON](Language.agent.json)
- Document: [`docs/ListItemDetail.md`](ListItemDetail.md); Agent JSON: [JSON](ListItemDetail.agent.json)
- Document: [`docs/ListItemQuantity.md`](ListItemQuantity.md); Agent JSON: [JSON](ListItemQuantity.agent.json)
- Document: [`docs/ListItemSelector.md`](ListItemSelector.md); Agent JSON: [JSON](ListItemSelector.agent.json)
- Document: [`docs/ListItemValue.md`](ListItemValue.md); Agent JSON: [JSON](ListItemValue.agent.json)
- Document: [`docs/LocalScopes.md`](LocalScopes.md); Agent JSON: [JSON](LocalScopes.agent.json)
- Document: [`docs/Localization.md`](Localization.md); Agent JSON: [JSON](Localization.agent.json)
- Document: [`docs/MeasuredCollections.md`](MeasuredCollections.md); Agent JSON: [JSON](MeasuredCollections.agent.json)
- Document: [`docs/MethodQueries.md`](MethodQueries.md); Agent JSON: [JSON](MethodQueries.agent.json)
- Document: [`docs/MethodReferences.md`](MethodReferences.md); Agent JSON: [JSON](MethodReferences.agent.json)
- Document: [`docs/MobileTabEffects.md`](MobileTabEffects.md); Agent JSON: [JSON](MobileTabEffects.agent.json)
- Document: [`docs/NativeAOT.md`](NativeAOT.md); Agent JSON: [JSON](NativeAOT.agent.json)
- Document: [`docs/NativeAOTVerification.md`](NativeAOTVerification.md); Agent JSON: [JSON](NativeAOTVerification.agent.json)
- Document: [`docs/NativeAlert.md`](NativeAlert.md); Agent JSON: [JSON](NativeAlert.agent.json)
- Document: [`docs/NativeAlertButton.md`](NativeAlertButton.md); Agent JSON: [JSON](NativeAlertButton.agent.json)
- Document: [`docs/NativeAppCard.md`](NativeAppCard.md); Agent JSON: [JSON](NativeAppCard.agent.json)
- Document: [`docs/NativeAppHeader.md`](NativeAppHeader.md); Agent JSON: [JSON](NativeAppHeader.agent.json)
- Document: [`docs/NativeApplication.md`](NativeApplication.md); Agent JSON: [JSON](NativeApplication.agent.json)
- Document: [`docs/NativeBindings.md`](NativeBindings.md); Agent JSON: [JSON](NativeBindings.agent.json)
- Document: [`docs/NativeButton.md`](NativeButton.md); Agent JSON: [JSON](NativeButton.agent.json)
- Document: [`docs/NativeCardOpen.md`](NativeCardOpen.md); Agent JSON: [JSON](NativeCardOpen.agent.json)
- Document: [`docs/NativeCheckBox.md`](NativeCheckBox.md); Agent JSON: [JSON](NativeCheckBox.agent.json)
- Document: [`docs/NativeCodeEditor.md`](NativeCodeEditor.md); Agent JSON: [JSON](NativeCodeEditor.agent.json)
- Document: [`docs/NativeColorChannel.md`](NativeColorChannel.md); Agent JSON: [JSON](NativeColorChannel.agent.json)
- Document: [`docs/NativeColorPicker.md`](NativeColorPicker.md); Agent JSON: [JSON](NativeColorPicker.agent.json)
- Document: [`docs/NativeColorPickerButton.md`](NativeColorPickerButton.md); Agent JSON: [JSON](NativeColorPickerButton.agent.json)
- Document: [`docs/NativeColorSurface.md`](NativeColorSurface.md); Agent JSON: [JSON](NativeColorSurface.agent.json)
- Document: [`docs/NativeColors.md`](NativeColors.md); Agent JSON: [JSON](NativeColors.agent.json)
- Document: [`docs/NativeComboBox.md`](NativeComboBox.md); Agent JSON: [JSON](NativeComboBox.agent.json)
- Document: [`docs/NativeCompilerCoverage.md`](NativeCompilerCoverage.md); Agent JSON: [JSON](NativeCompilerCoverage.agent.json)
- Document: [`docs/NativeCoreValues.md`](NativeCoreValues.md); Agent JSON: [JSON](NativeCoreValues.agent.json)
- Document: [`docs/NativeDivider.md`](NativeDivider.md); Agent JSON: [JSON](NativeDivider.agent.json)
- Document: [`docs/NativeDocument.md`](NativeDocument.md); Agent JSON: [JSON](NativeDocument.agent.json)
- Document: [`docs/NativeDropdownButton.md`](NativeDropdownButton.md); Agent JSON: [JSON](NativeDropdownButton.agent.json)
- Document: [`docs/NativeEditorCanvas.md`](NativeEditorCanvas.md); Agent JSON: [JSON](NativeEditorCanvas.agent.json)
- Document: [`docs/NativeEditorTimeline.md`](NativeEditorTimeline.md); Agent JSON: [JSON](NativeEditorTimeline.agent.json)
- Document: [`docs/NativeEmbeddedViews.md`](NativeEmbeddedViews.md); Agent JSON: [JSON](NativeEmbeddedViews.agent.json)
- Document: [`docs/NativeEventListener.md`](NativeEventListener.md); Agent JSON: [JSON](NativeEventListener.agent.json)
- Document: [`docs/NativeFonts.md`](NativeFonts.md); Agent JSON: [JSON](NativeFonts.agent.json)
- Document: [`docs/NativeHelpButton.md`](NativeHelpButton.md); Agent JSON: [JSON](NativeHelpButton.agent.json)
- Document: [`docs/NativeHierarchy.md`](NativeHierarchy.md); Agent JSON: [JSON](NativeHierarchy.agent.json)
- Document: [`docs/NativeHierarchyPanel.md`](NativeHierarchyPanel.md); Agent JSON: [JSON](NativeHierarchyPanel.agent.json)
- Document: [`docs/NativeHistory.md`](NativeHistory.md); Agent JSON: [JSON](NativeHistory.agent.json)
- Document: [`docs/NativeIcons.md`](NativeIcons.md); Agent JSON: [JSON](NativeIcons.agent.json)
- Document: [`docs/NativeImage.md`](NativeImage.md); Agent JSON: [JSON](NativeImage.agent.json)
- Document: [`docs/NativeImageLoading.md`](NativeImageLoading.md); Agent JSON: [JSON](NativeImageLoading.agent.json)
- Document: [`docs/NativeImages.md`](NativeImages.md); Agent JSON: [JSON](NativeImages.agent.json)
- Document: [`docs/NativeInputField.md`](NativeInputField.md); Agent JSON: [JSON](NativeInputField.agent.json)
- Document: [`docs/NativeLabel.md`](NativeLabel.md); Agent JSON: [JSON](NativeLabel.agent.json)
- Document: [`docs/NativeLayout.md`](NativeLayout.md); Agent JSON: [JSON](NativeLayout.agent.json)
- Document: [`docs/NativeLinks.md`](NativeLinks.md); Agent JSON: [JSON](NativeLinks.agent.json)
- Document: [`docs/NativeMarkup.md`](NativeMarkup.md); Agent JSON: [JSON](NativeMarkup.agent.json)
- Document: [`docs/NativeMaterials.md`](NativeMaterials.md); Agent JSON: [JSON](NativeMaterials.agent.json)
- Document: [`docs/NativeMedia.md`](NativeMedia.md); Agent JSON: [JSON](NativeMedia.agent.json)
- Document: [`docs/NativeMenu.md`](NativeMenu.md); Agent JSON: [JSON](NativeMenu.agent.json)
- Document: [`docs/NativeMigrationRoadmap.md`](NativeMigrationRoadmap.md); Agent JSON: [JSON](NativeMigrationRoadmap.agent.json)
- Document: [`docs/NativeMobileNavigation.md`](NativeMobileNavigation.md); Agent JSON: [JSON](NativeMobileNavigation.agent.json)
- Document: [`docs/NativeModal.md`](NativeModal.md); Agent JSON: [JSON](NativeModal.agent.json)
- Document: [`docs/NativeModelSources.md`](NativeModelSources.md); Agent JSON: [JSON](NativeModelSources.agent.json)
- Document: [`docs/NativeMotion.md`](NativeMotion.md); Agent JSON: [JSON](NativeMotion.agent.json)
- Document: [`docs/NativeNavigationHistory.md`](NativeNavigationHistory.md); Agent JSON: [JSON](NativeNavigationHistory.agent.json)
- Document: [`docs/NativeOpacity.md`](NativeOpacity.md); Agent JSON: [JSON](NativeOpacity.agent.json)
- Document: [`docs/NativeOwnedCard.md`](NativeOwnedCard.md); Agent JSON: [JSON](NativeOwnedCard.agent.json)
- Document: [`docs/NativeOwnedHierarchyItem.md`](NativeOwnedHierarchyItem.md); Agent JSON: [JSON](NativeOwnedHierarchyItem.agent.json)
- Document: [`docs/NativeOwnedHierarchyList.md`](NativeOwnedHierarchyList.md); Agent JSON: [JSON](NativeOwnedHierarchyList.agent.json)
- Document: [`docs/NativeOwnedHierarchyToolbar.md`](NativeOwnedHierarchyToolbar.md); Agent JSON: [JSON](NativeOwnedHierarchyToolbar.agent.json)
- Document: [`docs/NativeOwnedListFooter.md`](NativeOwnedListFooter.md); Agent JSON: [JSON](NativeOwnedListFooter.agent.json)
- Document: [`docs/NativeOwnedListItem.md`](NativeOwnedListItem.md); Agent JSON: [JSON](NativeOwnedListItem.agent.json)
- Document: [`docs/NativeOwnedListItemSelector.md`](NativeOwnedListItemSelector.md); Agent JSON: [JSON](NativeOwnedListItemSelector.agent.json)
- Document: [`docs/NativeOwnedListPanel.md`](NativeOwnedListPanel.md); Agent JSON: [JSON](NativeOwnedListPanel.agent.json)
- Document: [`docs/NativeOwnedListToolbar.md`](NativeOwnedListToolbar.md); Agent JSON: [JSON](NativeOwnedListToolbar.agent.json)
- Document: [`docs/NativeOwnedListView.md`](NativeOwnedListView.md); Agent JSON: [JSON](NativeOwnedListView.agent.json)
- Document: [`docs/NativeOwnedTable.md`](NativeOwnedTable.md); Agent JSON: [JSON](NativeOwnedTable.agent.json)
- Document: [`docs/NativeOwnedTableCellItem.md`](NativeOwnedTableCellItem.md); Agent JSON: [JSON](NativeOwnedTableCellItem.agent.json)
- Document: [`docs/NativeOwnedTableHeader.md`](NativeOwnedTableHeader.md); Agent JSON: [JSON](NativeOwnedTableHeader.agent.json)
- Document: [`docs/NativeOwnedTableRow.md`](NativeOwnedTableRow.md); Agent JSON: [JSON](NativeOwnedTableRow.agent.json)
- Document: [`docs/NativeOwnedToolbarButton.md`](NativeOwnedToolbarButton.md); Agent JSON: [JSON](NativeOwnedToolbarButton.agent.json)
- Document: [`docs/NativePageRouter.md`](NativePageRouter.md); Agent JSON: [JSON](NativePageRouter.agent.json)
- Document: [`docs/NativePlatform.md`](NativePlatform.md); Agent JSON: [JSON](NativePlatform.agent.json)
- Document: [`docs/NativePopover.md`](NativePopover.md); Agent JSON: [JSON](NativePopover.agent.json)
- Document: [`docs/NativePopupOpen.md`](NativePopupOpen.md); Agent JSON: [JSON](NativePopupOpen.agent.json)
- Document: [`docs/NativeProgress.md`](NativeProgress.md); Agent JSON: [JSON](NativeProgress.agent.json)
- Document: [`docs/NativeRadioButton.md`](NativeRadioButton.md); Agent JSON: [JSON](NativeRadioButton.agent.json)
- Document: [`docs/NativeReadiness.md`](NativeReadiness.md); Agent JSON: [JSON](NativeReadiness.agent.json)
- Document: [`docs/NativeReadinessVerification.md`](NativeReadinessVerification.md); Agent JSON: [JSON](NativeReadinessVerification.agent.json)
- Document: [`docs/NativeRendering.md`](NativeRendering.md); Agent JSON: [JSON](NativeRendering.agent.json)
- Document: [`docs/NativeRouteModels.md`](NativeRouteModels.md); Agent JSON: [JSON](NativeRouteModels.agent.json)
- Document: [`docs/NativeRoutes.md`](NativeRoutes.md); Agent JSON: [JSON](NativeRoutes.agent.json)
- Document: [`docs/NativeScrollView.md`](NativeScrollView.md); Agent JSON: [JSON](NativeScrollView.agent.json)
- Document: [`docs/NativeSearchField.md`](NativeSearchField.md); Agent JSON: [JSON](NativeSearchField.agent.json)
- Document: [`docs/NativeSegmentedControl.md`](NativeSegmentedControl.md); Agent JSON: [JSON](NativeSegmentedControl.agent.json)
- Document: [`docs/NativeSheet.md`](NativeSheet.md); Agent JSON: [JSON](NativeSheet.agent.json)
- Document: [`docs/NativeSlider.md`](NativeSlider.md); Agent JSON: [JSON](NativeSlider.agent.json)
- Document: [`docs/NativeStepper.md`](NativeStepper.md); Agent JSON: [JSON](NativeStepper.agent.json)
- Document: [`docs/NativeTableCardComposition.md`](NativeTableCardComposition.md); Agent JSON: [JSON](NativeTableCardComposition.agent.json)
- Document: [`docs/NativeTabs.md`](NativeTabs.md); Agent JSON: [JSON](NativeTabs.agent.json)
- Document: [`docs/NativeTextEditor.md`](NativeTextEditor.md); Agent JSON: [JSON](NativeTextEditor.agent.json)
- Document: [`docs/NativeToggleSwitch.md`](NativeToggleSwitch.md); Agent JSON: [JSON](NativeToggleSwitch.agent.json)
- Document: [`docs/NativeTooltip.md`](NativeTooltip.md); Agent JSON: [JSON](NativeTooltip.agent.json)
- Document: [`docs/NativeTooltipModel.md`](NativeTooltipModel.md); Agent JSON: [JSON](NativeTooltipModel.agent.json)
- Document: [`docs/NativeViews.md`](NativeViews.md); Agent JSON: [JSON](NativeViews.agent.json)
- Document: [`docs/NativeWidgetRuntime.md`](NativeWidgetRuntime.md); Agent JSON: [JSON](NativeWidgetRuntime.agent.json)
- Document: [`docs/NativeWindow.md`](NativeWindow.md); Agent JSON: [JSON](NativeWindow.agent.json)
- Document: [`docs/ObjectInputs.md`](ObjectInputs.md); Agent JSON: [JSON](ObjectInputs.agent.json)
- Document: [`docs/ObjectMethodReferences.md`](ObjectMethodReferences.md); Agent JSON: [JSON](ObjectMethodReferences.agent.json)
- Document: [`docs/OwnedHierarchyModel.md`](OwnedHierarchyModel.md); Agent JSON: [JSON](OwnedHierarchyModel.agent.json)
- Document: [`docs/OwnedListModel.md`](OwnedListModel.md); Agent JSON: [JSON](OwnedListModel.agent.json)
- Document: [`docs/OwnedTableModel.md`](OwnedTableModel.md); Agent JSON: [JSON](OwnedTableModel.agent.json)
- Document: [`docs/ProductComponentCoverage.md`](ProductComponentCoverage.md); Agent JSON: [JSON](ProductComponentCoverage.agent.json)
- Document: [`docs/PythonAPI.md`](PythonAPI.md); Agent JSON: [JSON](PythonAPI.agent.json)
- Document: [`docs/QueryConnections.md`](QueryConnections.md); Agent JSON: [JSON](QueryConnections.agent.json)
- Document: [`docs/ReactiveState.md`](ReactiveState.md); Agent JSON: [JSON](ReactiveState.agent.json)
- Document: [`docs/ReadonlyMembers.md`](ReadonlyMembers.md); Agent JSON: [JSON](ReadonlyMembers.agent.json)
- Document: [`docs/Repetition.md`](Repetition.md); Agent JSON: [JSON](Repetition.agent.json)
- Document: [`docs/ReservedWords.md`](ReservedWords.md); Agent JSON: [JSON](ReservedWords.agent.json)
- Document: [`docs/ReturnedCallbacks.md`](ReturnedCallbacks.md); Agent JSON: [JSON](ReturnedCallbacks.agent.json)
- Document: [`docs/RuntimeContracts.md`](RuntimeContracts.md); Agent JSON: [JSON](RuntimeContracts.agent.json)
- Document: [`docs/ScalarInputs.md`](ScalarInputs.md); Agent JSON: [JSON](ScalarInputs.agent.json)
- Document: [`docs/Scenes.md`](Scenes.md); Agent JSON: [JSON](Scenes.agent.json)
- Document: [`docs/SocietyDreamscapesMigrationPlan.md`](SocietyDreamscapesMigrationPlan.md); Agent JSON: [JSON](SocietyDreamscapesMigrationPlan.agent.json)
- Document: [`docs/SourceLayout.md`](SourceLayout.md); Agent JSON: [JSON](SourceLayout.agent.json)
- Document: [`docs/StyleProfiles.md`](StyleProfiles.md); Agent JSON: [JSON](StyleProfiles.agent.json)
- Document: [`docs/StyleTokenMembers.md`](StyleTokenMembers.md); Agent JSON: [JSON](StyleTokenMembers.agent.json)
- Document: [`docs/StyleTokens.md`](StyleTokens.md); Agent JSON: [JSON](StyleTokens.agent.json)
- Document: [`docs/StyleTypes.md`](StyleTypes.md); Agent JSON: [JSON](StyleTypes.agent.json)
- Document: [`docs/TableClipboard.md`](TableClipboard.md); Agent JSON: [JSON](TableClipboard.agent.json)
- Document: [`docs/TableEditing.md`](TableEditing.md); Agent JSON: [JSON](TableEditing.agent.json)
- Document: [`docs/TableMerge.md`](TableMerge.md); Agent JSON: [JSON](TableMerge.agent.json)
- Document: [`docs/TableSorting.md`](TableSorting.md); Agent JSON: [JSON](TableSorting.agent.json)
- Document: [`docs/TableStructure.md`](TableStructure.md); Agent JSON: [JSON](TableStructure.agent.json)
- Document: [`docs/TextInput.md`](TextInput.md); Agent JSON: [JSON](TextInput.agent.json)
- Document: [`docs/TextWrapFallback.md`](TextWrapFallback.md); Agent JSON: [JSON](TextWrapFallback.agent.json)
- Document: [`docs/Toolbars.md`](Toolbars.md); Agent JSON: [JSON](Toolbars.agent.json)
- Document: [`docs/TypeImports.md`](TypeImports.md); Agent JSON: [JSON](TypeImports.agent.json)
- Document: [`docs/VariableDeclarations.md`](VariableDeclarations.md); Agent JSON: [JSON](VariableDeclarations.agent.json)
- Document: [`docs/Verification.md`](Verification.md); Agent JSON: [JSON](Verification.agent.json)
- Document: [`docs/ViewModelQueries.md`](ViewModelQueries.md); Agent JSON: [JSON](ViewModelQueries.agent.json)
- Document: [`docs/ViewOnlyArchitecture.md`](ViewOnlyArchitecture.md); Agent JSON: [JSON](ViewOnlyArchitecture.agent.json)
- Document: [`docs/ViewSequences.md`](ViewSequences.md); Agent JSON: [JSON](ViewSequences.agent.json)
- Document: [`docs/ViewSessions.md`](ViewSessions.md); Agent JSON: [JSON](ViewSessions.agent.json)
- Document: [`docs/ViewValues.md`](ViewValues.md); Agent JSON: [JSON](ViewValues.agent.json)
- Document: [`docs/WindowsValidation.md`](WindowsValidation.md); Agent JSON: [JSON](WindowsValidation.agent.json)
- Document: [`docs/documentation-structure.md`](documentation-structure.md); Agent JSON: [JSON](documentation-structure.agent.json)
- Document: [`example/README.md`](../example/README.md); Agent JSON: [JSON](Mirrors/Example/README.agent.json)
- Document: [`example/HelloAndroid/README.md`](../example/HelloAndroid/README.md); Agent JSON: [JSON](Mirrors/Example/HelloAndroid/README.agent.json)
- Document: [`example/HelloHosts/README.md`](../example/HelloHosts/README.md); Agent JSON: [JSON](Mirrors/Example/HelloHosts/README.agent.json)
- Document: [`example/StyleTokens/README.md`](../example/StyleTokens/README.md); Agent JSON: [JSON](Mirrors/Example/StyleTokens/README.agent.json)
<!-- workspace-docs:document-map:end -->

## Editor panels

- [App-provided menu and editor panel contracts](EditorPanels.md)
