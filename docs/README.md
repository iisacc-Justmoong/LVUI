# LVUI documentation

- [Getting started: native applications, state, lists and tables](GettingStarted.md)
- [LVRS native implementation migration roadmap](NativeMigrationRoadmap.md)
- [LVUI-owned components and LVRS independence status](LVUIIndependence.md)
- [LVUI-owned native motion and frame injection](NativeMotion.md)
- [LVUI-owned image layout and host adapter](NativeImage.md)
- [LVUI-owned buttons and interaction host](NativeButton.md)
- [LVUI-owned divider layout and host adapter](NativeDivider.md)
- [LVUI-owned toggle state, gesture and motion](NativeToggleSwitch.md)
- [LVUI-owned checkbox state and drawing](NativeCheckBox.md)
- [LVUI-owned radio groups and motion](NativeRadioButton.md)
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


- [Current LVRS self-rendered native implementation](LVRSNative.md)

This directory separates the current implementation contract from the selected
LVUI language and runtime design, including the native widget and acceleration
contract and the `View` type/child-argument rules clarified on 2026-10-22.

LVUI is an AOT-compiled native language. Its current LVRS path targets iiGUI's
own C++ widgets and the selected Metal, DirectX/Direct3D or Vulkan graphics path.
Platform APIs provide windows, input and presentation. Core owns the application
lifetime; native widgets and GPU resources are instantiated by compiled C++.
Its execution model does not include a source interpreter or language VM.

The selected design follows the supplied scoped-ownership proposal. The earlier
metadata-only import and per-click instance model is retired.

## Reading map

| Document | Scope | Status |
|---|---|---|
| [NativeWidgetRuntime.md](NativeWidgetRuntime.md) | Instantiable C++ widgets, LVRS-only appearance, Core application/entrypoint/attachment | Implemented macOS path; Core remains independent of GUI/DSL |
| [CoreWidgetVerification.md](CoreWidgetVerification.md) | Core entrypoint, C++ widget lifetime, native LVRS appearance and installed consumer | Verification of the Core-owned application path |
| [CompilationModel.md](CompilationModel.md) | AOT native widget compilation, Metal/DirectX/Vulkan acceleration and delivery to iiGUI | Self-rendered iiGUI C++ widgets; platform execution coverage audited separately |
| [NativeAOT.md](NativeAOT.md) | Initial C++ generation, supported inputs, scoped callbacks and CMake | Initial AOT subset; later bindings, components, functions and ownership are linked above |
| [EmbeddedPages.md](EmbeddedPages.md) | Independent view-body scopes in an existing builder and LVRS PageRouter AOT | Code generation and compilation verified; actual execution deferred |
| [NativeAOTVerification.md](NativeAOTVerification.md) | Earlier Hello World, AppKit tests, binary and relocated installed-consumer checks | Historical macOS arm64 evidence; not full current-widget execution proof |
| [Language.md](Language.md) | HTML-shaped LVUI 0.1 syntax, styles, layout and diagnostics | Current implementation contract |
| [ComponentLanguage.md](ComponentLanguage.md) | Native C++23 tokenizer, reserved identifiers, typed child View AST and lvui-check | Implemented front end; feeds the separate native AOT generator |
| [ReservedWords.md](ReservedWords.md) | All 107 visual LVRS component keywords, three modifiers and six core words with source evidence | Generated from the audited source snapshot; verified against the actual parser |
| [Verification.md](Verification.md) | Earlier source inventory, front-end tests, CLI diagnostics and staged installed consumer | Historical front-end evidence; see NativeAOTVerification for native execution |
| [DeclarativeSyntax.md](DeclarativeSyntax.md) | Common `View` type, `{}` child-view arguments, component declarations, imported instance names, bound actions and properties | Selected design; draft grammar |
| [MethodReferences.md](MethodReferences.md) | Method references versus immediate calls, typed callback inputs and component-owned event semantics | Typed event and result-valued inputs implemented; remaining general protocols are audited separately |
| [ReactiveState.md](ReactiveState.md) | Reactive values by default, the static exception and the supplied [increment/reset counter](examples/ReactiveCounter.lvui) | Scalar native tracking and static snapshots implemented; complete input/lifecycle coverage remains pending |
| [VariableDeclarations.md](VariableDeclarations.md) | `let` constants, `var` variables, typed initializers and no-semicolon syntax | Selected declaration rules; full type system remains open |
| [DeclarationAnnotations.md](DeclarationAnnotations.md) | `@Observable` and `@Static` prefix annotations; default VM binding | Selected meanings; targets, combinations and static lifetime ownership remain open |
| [TypeImports.md](TypeImports.md) | Scoped imports that create and own C++ backend instances with public-member access | Selected ownership and access semantics |
| [InvocationLifecycle.md](InvocationLifecycle.md) | Use tracking, scope teardown, asynchronous work and deterministic destruction | Selected lifecycle semantics |
| [RuntimeContracts.md](RuntimeContracts.md) | C++ access, bridge information, property observation, Core integration and resource ownership | Required design capabilities; concrete APIs remain open |
| [ViewModelQueries.md](ViewModelQueries.md) | Default VM binding, Core query value exchange, notifications, execution domains and teardown | Selected binding/communication design; LVUI adapters remain unimplemented |
| [CounterExample.md](CounterExample.md) | An application window with an observable counter and increment/decrement buttons; [source](examples/Counter.lvui) | Hypothetical scoped-backend example |

The existing filenames `TypeImports.md` and `InvocationLifecycle.md` are retained
so earlier document links continue to work. Their contents now describe scoped
instance ownership.

## Selected rules

1. LVUI describes views through declarative component-shaped syntax. MVVM is
   the recommended application pattern, with ViewModel state rules inspired
   by SwiftUI. ViewModels are bindable by default; views use their public state
   directly without a binding annotation or wrapper declaration.
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
11. `let` declares a constant, and `var` declares a mutable variable. Typed,
    initialized declarations use `let name: Type = initializer` or
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
    This does not relax `let`, read-only or C++ constness restrictions.
16. `View` is the common type for everything displayed visually in LVUI,
    including visual controls, layout containers and composed views. A view can
    take other views as child arguments through `{}`; those children can in turn
    take further child views through nested braces. The language type does not
    prescribe a shared C++ base class.

## Implementation boundary

The package implements two C++23 front ends: `parse`/`Document` for the existing
[HTML-shaped grammar](Language.md), and `tokenize`/`parse_view`/`ViewDocument` for
the [component grammar](ComponentLanguage.md). The latter uses the LVUI-owned
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

| Document | Agent JSON |
| --- | --- |
| [`AGENT.md`](../AGENT.md) | [JSON](entrypoints/AGENT.agent.json) |
| [`README.md`](../README.md) | [JSON](entrypoints/README.agent.json) |
| [`docs/ActivationWork.md`](ActivationWork.md) | [JSON](ActivationWork.agent.json) |
| [`docs/AppSurfaces.md`](AppSurfaces.md) | [JSON](AppSurfaces.agent.json) |
| [`docs/BackendConstruction.md`](BackendConstruction.md) | [JSON](BackendConstruction.agent.json) |
| [`docs/CallableFields.md`](CallableFields.md) | [JSON](CallableFields.agent.json) |
| [`docs/CallableInputs.md`](CallableInputs.md) | [JSON](CallableInputs.agent.json) |
| [`docs/CallbackValues.md`](CallbackValues.md) | [JSON](CallbackValues.agent.json) |
| [`docs/CodeDocument.md`](CodeDocument.md) | [JSON](CodeDocument.agent.json) |
| [`docs/CollectionNavigation.md`](CollectionNavigation.md) | [JSON](CollectionNavigation.agent.json) |
| [`docs/ColorSelection.md`](ColorSelection.md) | [JSON](ColorSelection.agent.json) |
| [`docs/CompilationModel.md`](CompilationModel.md) | [JSON](CompilationModel.agent.json) |
| [`docs/Completion.md`](Completion.md) | [JSON](Completion.agent.json) |
| [`docs/ComponentLanguage.md`](ComponentLanguage.md) | [JSON](ComponentLanguage.agent.json) |
| [`docs/Components.md`](Components.md) | [JSON](Components.agent.json) |
| [`docs/ConditionalViews.md`](ConditionalViews.md) | [JSON](ConditionalViews.agent.json) |
| [`docs/CoreWidgetVerification.md`](CoreWidgetVerification.md) | [JSON](CoreWidgetVerification.agent.json) |
| [`docs/CounterExample.md`](CounterExample.md) | [JSON](CounterExample.agent.json) |
| [`docs/DeclarationAnnotations.md`](DeclarationAnnotations.md) | [JSON](DeclarationAnnotations.agent.json) |
| [`docs/DeclarativeSyntax.md`](DeclarativeSyntax.md) | [JSON](DeclarativeSyntax.agent.json) |
| [`docs/EditorAppearance.md`](EditorAppearance.md) | [JSON](EditorAppearance.agent.json) |
| [`docs/EmbeddedPages.md`](EmbeddedPages.md) | [JSON](EmbeddedPages.agent.json) |
| [`docs/Expressions.md`](Expressions.md) | [JSON](Expressions.agent.json) |
| [`docs/FactoryCaptures.md`](FactoryCaptures.md) | [JSON](FactoryCaptures.agent.json) |
| [`docs/FunctionValues.md`](FunctionValues.md) | [JSON](FunctionValues.agent.json) |
| [`docs/FunctionVariables.md`](FunctionVariables.md) | [JSON](FunctionVariables.agent.json) |
| [`docs/GettingStarted.md`](GettingStarted.md) | [JSON](GettingStarted.agent.json) |
| [`docs/HierarchyDrag.md`](HierarchyDrag.md) | [JSON](HierarchyDrag.agent.json) |
| [`docs/IndependenceWorkSnapshot-2026-10-29.md`](IndependenceWorkSnapshot-2026-10-29.md) | [JSON](IndependenceWorkSnapshot-2026-10-29.agent.json) |
| [`docs/InferredValues.md`](InferredValues.md) | [JSON](InferredValues.agent.json) |
| [`docs/InvocationLifecycle.md`](InvocationLifecycle.md) | [JSON](InvocationLifecycle.agent.json) |
| [`docs/KeywordRegistry.md`](KeywordRegistry.md) | [JSON](KeywordRegistry.agent.json) |
| [`docs/LVRSNative.md`](LVRSNative.md) | [JSON](LVRSNative.agent.json) |
| [`docs/LVUIIndependence.md`](LVUIIndependence.md) | [JSON](LVUIIndependence.agent.json) |
| [`docs/Language.md`](Language.md) | [JSON](Language.agent.json) |
| [`docs/ListItemQuantity.md`](ListItemQuantity.md) | [JSON](ListItemQuantity.agent.json) |
| [`docs/ListItemSelector.md`](ListItemSelector.md) | [JSON](ListItemSelector.agent.json) |
| [`docs/LocalScopes.md`](LocalScopes.md) | [JSON](LocalScopes.agent.json) |
| [`docs/MeasuredCollections.md`](MeasuredCollections.md) | [JSON](MeasuredCollections.agent.json) |
| [`docs/MethodQueries.md`](MethodQueries.md) | [JSON](MethodQueries.agent.json) |
| [`docs/MethodReferences.md`](MethodReferences.md) | [JSON](MethodReferences.agent.json) |
| [`docs/NativeAOT.md`](NativeAOT.md) | [JSON](NativeAOT.agent.json) |
| [`docs/NativeAOTVerification.md`](NativeAOTVerification.md) | [JSON](NativeAOTVerification.agent.json) |
| [`docs/NativeAlert.md`](NativeAlert.md) | [JSON](NativeAlert.agent.json) |
| [`docs/NativeAlertButton.md`](NativeAlertButton.md) | [JSON](NativeAlertButton.agent.json) |
| [`docs/NativeAppCard.md`](NativeAppCard.md) | [JSON](NativeAppCard.agent.json) |
| [`docs/NativeAppHeader.md`](NativeAppHeader.md) | [JSON](NativeAppHeader.agent.json) |
| [`docs/NativeBindings.md`](NativeBindings.md) | [JSON](NativeBindings.agent.json) |
| [`docs/NativeButton.md`](NativeButton.md) | [JSON](NativeButton.agent.json) |
| [`docs/NativeCheckBox.md`](NativeCheckBox.md) | [JSON](NativeCheckBox.agent.json) |
| [`docs/NativeCodeEditor.md`](NativeCodeEditor.md) | [JSON](NativeCodeEditor.agent.json) |
| [`docs/NativeColorChannel.md`](NativeColorChannel.md) | [JSON](NativeColorChannel.agent.json) |
| [`docs/NativeColorPicker.md`](NativeColorPicker.md) | [JSON](NativeColorPicker.agent.json) |
| [`docs/NativeColorPickerButton.md`](NativeColorPickerButton.md) | [JSON](NativeColorPickerButton.agent.json) |
| [`docs/NativeColorSurface.md`](NativeColorSurface.md) | [JSON](NativeColorSurface.agent.json) |
| [`docs/NativeColors.md`](NativeColors.md) | [JSON](NativeColors.agent.json) |
| [`docs/NativeComboBox.md`](NativeComboBox.md) | [JSON](NativeComboBox.agent.json) |
| [`docs/NativeCoreValues.md`](NativeCoreValues.md) | [JSON](NativeCoreValues.agent.json) |
| [`docs/NativeDivider.md`](NativeDivider.md) | [JSON](NativeDivider.agent.json) |
| [`docs/NativeDocument.md`](NativeDocument.md) | [JSON](NativeDocument.agent.json) |
| [`docs/NativeDropdownButton.md`](NativeDropdownButton.md) | [JSON](NativeDropdownButton.agent.json) |
| [`docs/NativeEventListener.md`](NativeEventListener.md) | [JSON](NativeEventListener.agent.json) |
| [`docs/NativeFonts.md`](NativeFonts.md) | [JSON](NativeFonts.agent.json) |
| [`docs/NativeHelpButton.md`](NativeHelpButton.md) | [JSON](NativeHelpButton.agent.json) |
| [`docs/NativeHierarchy.md`](NativeHierarchy.md) | [JSON](NativeHierarchy.agent.json) |
| [`docs/NativeHierarchyPanel.md`](NativeHierarchyPanel.md) | [JSON](NativeHierarchyPanel.agent.json) |
| [`docs/NativeHistory.md`](NativeHistory.md) | [JSON](NativeHistory.agent.json) |
| [`docs/NativeIcons.md`](NativeIcons.md) | [JSON](NativeIcons.agent.json) |
| [`docs/NativeImage.md`](NativeImage.md) | [JSON](NativeImage.agent.json) |
| [`docs/NativeImages.md`](NativeImages.md) | [JSON](NativeImages.agent.json) |
| [`docs/NativeInputField.md`](NativeInputField.md) | [JSON](NativeInputField.agent.json) |
| [`docs/NativeLayout.md`](NativeLayout.md) | [JSON](NativeLayout.agent.json) |
| [`docs/NativeLinks.md`](NativeLinks.md) | [JSON](NativeLinks.agent.json) |
| [`docs/NativeMarkup.md`](NativeMarkup.md) | [JSON](NativeMarkup.agent.json) |
| [`docs/NativeMaterials.md`](NativeMaterials.md) | [JSON](NativeMaterials.agent.json) |
| [`docs/NativeMedia.md`](NativeMedia.md) | [JSON](NativeMedia.agent.json) |
| [`docs/NativeMenu.md`](NativeMenu.md) | [JSON](NativeMenu.agent.json) |
| [`docs/NativeMigrationRoadmap.md`](NativeMigrationRoadmap.md) | [JSON](NativeMigrationRoadmap.agent.json) |
| [`docs/NativeModal.md`](NativeModal.md) | [JSON](NativeModal.agent.json) |
| [`docs/NativeModelSources.md`](NativeModelSources.md) | [JSON](NativeModelSources.agent.json) |
| [`docs/NativeMotion.md`](NativeMotion.md) | [JSON](NativeMotion.agent.json) |
| [`docs/NativeNavigationHistory.md`](NativeNavigationHistory.md) | [JSON](NativeNavigationHistory.agent.json) |
| [`docs/NativeOpacity.md`](NativeOpacity.md) | [JSON](NativeOpacity.agent.json) |
| [`docs/NativeOwnedCard.md`](NativeOwnedCard.md) | [JSON](NativeOwnedCard.agent.json) |
| [`docs/NativeOwnedHierarchyItem.md`](NativeOwnedHierarchyItem.md) | [JSON](NativeOwnedHierarchyItem.agent.json) |
| [`docs/NativeOwnedHierarchyList.md`](NativeOwnedHierarchyList.md) | [JSON](NativeOwnedHierarchyList.agent.json) |
| [`docs/NativeOwnedHierarchyToolbar.md`](NativeOwnedHierarchyToolbar.md) | [JSON](NativeOwnedHierarchyToolbar.agent.json) |
| [`docs/NativeOwnedListFooter.md`](NativeOwnedListFooter.md) | [JSON](NativeOwnedListFooter.agent.json) |
| [`docs/NativeOwnedListItem.md`](NativeOwnedListItem.md) | [JSON](NativeOwnedListItem.agent.json) |
| [`docs/NativeOwnedListItemSelector.md`](NativeOwnedListItemSelector.md) | [JSON](NativeOwnedListItemSelector.agent.json) |
| [`docs/NativeOwnedListPanel.md`](NativeOwnedListPanel.md) | [JSON](NativeOwnedListPanel.agent.json) |
| [`docs/NativeOwnedListToolbar.md`](NativeOwnedListToolbar.md) | [JSON](NativeOwnedListToolbar.agent.json) |
| [`docs/NativeOwnedListView.md`](NativeOwnedListView.md) | [JSON](NativeOwnedListView.agent.json) |
| [`docs/NativeOwnedTable.md`](NativeOwnedTable.md) | [JSON](NativeOwnedTable.agent.json) |
| [`docs/NativeOwnedTableCellItem.md`](NativeOwnedTableCellItem.md) | [JSON](NativeOwnedTableCellItem.agent.json) |
| [`docs/NativeOwnedTableHeader.md`](NativeOwnedTableHeader.md) | [JSON](NativeOwnedTableHeader.agent.json) |
| [`docs/NativeOwnedTableRow.md`](NativeOwnedTableRow.md) | [JSON](NativeOwnedTableRow.agent.json) |
| [`docs/NativeOwnedToolbarButton.md`](NativeOwnedToolbarButton.md) | [JSON](NativeOwnedToolbarButton.agent.json) |
| [`docs/NativePageRouter.md`](NativePageRouter.md) | [JSON](NativePageRouter.agent.json) |
| [`docs/NativePlatform.md`](NativePlatform.md) | [JSON](NativePlatform.agent.json) |
| [`docs/NativePopover.md`](NativePopover.md) | [JSON](NativePopover.agent.json) |
| [`docs/NativeProgress.md`](NativeProgress.md) | [JSON](NativeProgress.agent.json) |
| [`docs/NativeRadioButton.md`](NativeRadioButton.md) | [JSON](NativeRadioButton.agent.json) |
| [`docs/NativeReadiness.md`](NativeReadiness.md) | [JSON](NativeReadiness.agent.json) |
| [`docs/NativeReadinessVerification.md`](NativeReadinessVerification.md) | [JSON](NativeReadinessVerification.agent.json) |
| [`docs/NativeRendering.md`](NativeRendering.md) | [JSON](NativeRendering.agent.json) |
| [`docs/NativeRouteModels.md`](NativeRouteModels.md) | [JSON](NativeRouteModels.agent.json) |
| [`docs/NativeRoutes.md`](NativeRoutes.md) | [JSON](NativeRoutes.agent.json) |
| [`docs/NativeSegmentedControl.md`](NativeSegmentedControl.md) | [JSON](NativeSegmentedControl.agent.json) |
| [`docs/NativeSheet.md`](NativeSheet.md) | [JSON](NativeSheet.agent.json) |
| [`docs/NativeSlider.md`](NativeSlider.md) | [JSON](NativeSlider.agent.json) |
| [`docs/NativeStepper.md`](NativeStepper.md) | [JSON](NativeStepper.agent.json) |
| [`docs/NativeTabs.md`](NativeTabs.md) | [JSON](NativeTabs.agent.json) |
| [`docs/NativeToggleSwitch.md`](NativeToggleSwitch.md) | [JSON](NativeToggleSwitch.agent.json) |
| [`docs/NativeTooltip.md`](NativeTooltip.md) | [JSON](NativeTooltip.agent.json) |
| [`docs/NativeTooltipModel.md`](NativeTooltipModel.md) | [JSON](NativeTooltipModel.agent.json) |
| [`docs/NativeViews.md`](NativeViews.md) | [JSON](NativeViews.agent.json) |
| [`docs/NativeWidgetRuntime.md`](NativeWidgetRuntime.md) | [JSON](NativeWidgetRuntime.agent.json) |
| [`docs/NativeWindow.md`](NativeWindow.md) | [JSON](NativeWindow.agent.json) |
| [`docs/ObjectInputs.md`](ObjectInputs.md) | [JSON](ObjectInputs.agent.json) |
| [`docs/ObjectMethodReferences.md`](ObjectMethodReferences.md) | [JSON](ObjectMethodReferences.agent.json) |
| [`docs/OwnedHierarchyModel.md`](OwnedHierarchyModel.md) | [JSON](OwnedHierarchyModel.agent.json) |
| [`docs/OwnedListModel.md`](OwnedListModel.md) | [JSON](OwnedListModel.agent.json) |
| [`docs/OwnedTableModel.md`](OwnedTableModel.md) | [JSON](OwnedTableModel.agent.json) |
| [`docs/ReactiveState.md`](ReactiveState.md) | [JSON](ReactiveState.agent.json) |
| [`docs/ReadonlyMembers.md`](ReadonlyMembers.md) | [JSON](ReadonlyMembers.agent.json) |
| [`docs/Repetition.md`](Repetition.md) | [JSON](Repetition.agent.json) |
| [`docs/ReservedWords.md`](ReservedWords.md) | [JSON](ReservedWords.agent.json) |
| [`docs/ReturnedCallbacks.md`](ReturnedCallbacks.md) | [JSON](ReturnedCallbacks.agent.json) |
| [`docs/RuntimeContracts.md`](RuntimeContracts.md) | [JSON](RuntimeContracts.agent.json) |
| [`docs/ScalarInputs.md`](ScalarInputs.md) | [JSON](ScalarInputs.agent.json) |
| [`docs/SourceLayout.md`](SourceLayout.md) | [JSON](SourceLayout.agent.json) |
| [`docs/StyleProfiles.md`](StyleProfiles.md) | [JSON](StyleProfiles.agent.json) |
| [`docs/StyleTokenMembers.md`](StyleTokenMembers.md) | [JSON](StyleTokenMembers.agent.json) |
| [`docs/StyleTokens.md`](StyleTokens.md) | [JSON](StyleTokens.agent.json) |
| [`docs/StyleTypes.md`](StyleTypes.md) | [JSON](StyleTypes.agent.json) |
| [`docs/TableClipboard.md`](TableClipboard.md) | [JSON](TableClipboard.agent.json) |
| [`docs/TableEditing.md`](TableEditing.md) | [JSON](TableEditing.agent.json) |
| [`docs/TableSorting.md`](TableSorting.md) | [JSON](TableSorting.agent.json) |
| [`docs/TableStructure.md`](TableStructure.md) | [JSON](TableStructure.agent.json) |
| [`docs/TextInput.md`](TextInput.md) | [JSON](TextInput.agent.json) |
| [`docs/Toolbars.md`](Toolbars.md) | [JSON](Toolbars.agent.json) |
| [`docs/TypeImports.md`](TypeImports.md) | [JSON](TypeImports.agent.json) |
| [`docs/VariableDeclarations.md`](VariableDeclarations.md) | [JSON](VariableDeclarations.agent.json) |
| [`docs/Verification.md`](Verification.md) | [JSON](Verification.agent.json) |
| [`docs/ViewModelQueries.md`](ViewModelQueries.md) | [JSON](ViewModelQueries.agent.json) |
| [`docs/ViewSequences.md`](ViewSequences.md) | [JSON](ViewSequences.agent.json) |
| [`docs/ViewValues.md`](ViewValues.md) | [JSON](ViewValues.agent.json) |
| [`docs/WindowsValidation.md`](WindowsValidation.md) | [JSON](WindowsValidation.agent.json) |
| [`docs/documentation-structure.md`](documentation-structure.md) | [JSON](documentation-structure.agent.json) |
| [`example/README.md`](../example/README.md) | [JSON](mirrors/example/README.agent.json) |
| [`example/StyleTokens/README.md`](../example/StyleTokens/README.md) | [JSON](mirrors/example/StyleTokens/README.agent.json) |
<!-- workspace-docs:document-map:end -->
