# LVUI documentation

- [LVRS default style tokens and Figma snapshot](StyleTokens.md)
- [Color and Metric value types](StyleTypes.md)
- [Complete Color and Metric member catalog](StyleTokenMembers.md)


- [Current LVRS self-rendered native implementation](LVRSNative.md)

This directory separates the current implementation contract from the selected
LVUI language and runtime design, including the native widget and acceleration
contract and the `View` type/child-argument rules clarified on 2026-10-22.

LVUI is an AOT-compiled native language. At compile time it targets OS-native
widgets, including Cocoa/AppKit on macOS and Win32 on Windows, and the selected
Metal, DirectX/Direct3D or Vulkan acceleration path. The resulting native code
is handed to iiGUI for hosting, events, lifecycle and presentation in the iiSDK
runtime. Native objects and GPU resources are instantiated at runtime by the
compiled code. Its execution model does not include a language interpreter,
JVM or .NET/CLR layer.

The selected design follows the supplied scoped-ownership proposal. The earlier
metadata-only import and per-click instance model is retired.

## Reading map

| Document | Scope | Status |
|---|---|---|
| [NativeWidgetRuntime.md](NativeWidgetRuntime.md) | Instantiable C++ widgets, LVRS-only appearance, Core application/entrypoint/attachment | Implemented macOS path; Core remains independent of GUI/DSL |
| [CoreWidgetVerification.md](CoreWidgetVerification.md) | Core entrypoint, C++ widget lifetime, native LVRS appearance and installed consumer | Verification of the Core-owned application path |
| [CompilationModel.md](CompilationModel.md) | AOT native widget compilation, Metal/DirectX/Vulkan acceleration and delivery to iiGUI | Architecture; bounded C++23/Cocoa implementation now available |
| [NativeAOT.md](NativeAOT.md) | Actual C++ generation, supported inputs, scoped callbacks, CMake and Cocoa mount lifecycle | Implemented for ApplicationWindow, stacks, Label and PushButton |
| [EmbeddedPages.md](EmbeddedPages.md) | 기존 빌더에 독립 뷰 본문·스코프 구성, LVRS PageRouter AOT 연결 | 코드 생성·컴파일 검증, 실행 보류 |
| [NativeAOTVerification.md](NativeAOTVerification.md) | Native Hello World, AppKit tests, binary and relocated installed-consumer checks | macOS arm64 execution evidence |
| [Language.md](Language.md) | HTML-shaped LVUI 0.1 syntax, styles, layout and diagnostics | Current implementation contract |
| [ComponentLanguage.md](ComponentLanguage.md) | Native C++23 tokenizer, reserved identifiers, typed child View AST and lvui-check | Implemented front end; feeds the separate native AOT generator |
| [ReservedWords.md](ReservedWords.md) | All 107 visual LVRS component keywords, three modifiers and six core words with source evidence | Generated from the audited source snapshot; verified against the actual parser |
| [Verification.md](Verification.md) | Earlier source inventory, front-end tests, CLI diagnostics and staged installed consumer | Historical front-end evidence; see NativeAOTVerification for native execution |
| [DeclarativeSyntax.md](DeclarativeSyntax.md) | Common `View` type, `{}` child-view arguments, component declarations, imported instance names, bound actions and properties | Selected design; draft grammar |
| [MethodReferences.md](MethodReferences.md) | Method references versus immediate calls, typed callback inputs and component-owned event semantics | Selected callable semantics; concrete callable/component APIs remain open |
| [ReactiveState.md](ReactiveState.md) | Reactive values by default, the static exception and the supplied [increment/reset counter](examples/ReactiveCounter.lvui) | Selected general value semantics; native tracking remains unimplemented |
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
the [component grammar](ComponentLanguage.md). The latter uses the audited LVRS
reserved-word table and owns typed child Views, inputs, scalar declarations and
import syntax. The native `lvui-check` tool validates component source files.
The package depends only on iiSDKCore Contracts.

Parsing alone does not activate C++ objects or execute a UI. The separate
`compile_native`/`lvui-compile` path generates C++23 for the documented native
subset. iiGUI mounts actual Cocoa widgets, owns imported scopes and connects
compiled method-reference callbacks. Runtime UI property changes are explicit
native C++ calls. General reactive field bindings, annotations, View-valued
declarations, custom component definitions, other native widget backends and
GPU code generation remain outside that implementation.

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
