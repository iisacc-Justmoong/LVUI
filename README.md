# LVUI

LVUI is a declarative UI language whose `.lvui` files generate native C++ view
construction and bindings. iiGUI is the graphics framework that owns visual
components, layout, input, accessibility and GPU scene rendering. Their roles
correspond to QML and QtQuick respectively without requiring either technology
or compatibility with it. See the [responsibility boundary](docs/GraphicsBoundary.md).
The design-system tokens and assets remain shared visual inputs. The operating
system supplies window, input, font and accessibility services; desktop apps do
not embed a browser or construct component trees from OS controls. See
[GPU scene composition](../iiGUI/docs/GpuSceneComposition.md) for current limits.

The implementation has not completed that separation: several components and
their drawing and interaction code still live in LVUI. The component details
below describe the current implementation and the work to migrate it to iiGUI.
Existing builds must not depend on LVRS or Qt/QML.
LVUI declares views, layout, styles, and bindings. Application objects and method
implementations belong to imported C++ view models, whose lifetime and observable
state are tied to their owning view activation. See the
[view-only architecture](docs/ViewOnlyArchitecture.md) and its C++-backed counter.
Values have `@State` and `@Bindable` semantics by default; see the
[reactive value contract](docs/ReactiveState.md) for ownership, writable sources,
explicit edit commands, and fixed-value exceptions.
Application state can be exposed through iiSDKCore typed queries and
`QueryValue`; editable controls keep their default two-way binding. Temporary
view state remains scoped to its activation. See [view-model queries](docs/ViewModelQueries.md).
Existing local Object and executable event-block features predate this rule and
remain implemented pending migration; the compiler does not yet enforce the rule.
An application can declare named [Scenes](docs/Scenes.md), each with a native
window and its own view session. [The Scenes example](example/GettingStarted/App/Scenes.lvui)
opens a second scene and demonstrates independent reactive state.
[App-scoped strings](docs/Localization.md) keep translated text in external
UTF-8 catalogs and update active views when the locale or a value changes.
The optional `LVUI::Localization` component loads and saves JSON catalogs.
The independent [icon catalog](docs/NativeIcons.md) retains the original
LVUI-authored defaults and also embeds 2,860 migrated design SVGs and their
lookup aliases without a runtime LVRS or QML dependency. Applications can supply
a replaceable source or explicitly request the migrated snapshot; platform
appearance and unsupported SVG effects still require separate verification.
Standalone named icons use the Figma-derived LVUI geometry through `NativeIcon`
or `<Image iconName="brush"/>`, with replaceable sources and reactive updates.
The former `FRAMEWORK LVRS` generator has been removed; explicitly selecting it
is rejected. Component migration and independent package verification remain in
progress. The native path now owns its ProgressBar geometry,
paint recipe, reactive visual configuration and frame-driven motion. The
[motion model](docs/NativeMotion.md) is standard C++ and accepts host frame times.
The default [Image path](docs/NativeImage.md) owns validation, fitting, cropping
and paint recipes in LVUI. Applications supply immutable pixels or a replaceable
resolver shared by a view activation; the optional [iiSDK image loader](docs/NativeImageLoading.md)
handles files, URLs, decoding and bounded caches without LVRS or Qt. An injected
iiSDK runtime moves file reads and decoding off the owning UI thread. When ImageIO
is enabled, the installed-package check builds a relocated consumer and verifies
in-memory image decoding without opening a window or making a network request.
The default [button path](docs/NativeButton.md) owns its tone, state-dependent
painting and content geometry; the host supplies generic input and text services.
The default [divider path](docs/NativeDivider.md) owns line layout and style;
the application host supplies only a non-interactive drawing surface.
The default [menu path](docs/NativeMenu.md) owns its entries, selection,
keyboard navigation, rendering and dismissal policy, including direct items,
dividers and nested menus; dynamically generated children remain in progress.
The default [popover path](docs/NativePopover.md) owns validated design inputs
and reactive updates while the host supplies generic popup placement and input.
The declarative [ScrollView](docs/NativeScrollView.md) mounts one retained
content view in an iiGUI viewport with reactive sizing and native scrolling.
The default [color picker button](docs/NativeColorPickerButton.md) owns its
state and design and accepts a replaceable LVUI color source.
The independent [color selection model](docs/ColorSelection.md) supports shared,
transactional editing with stale-session protection.
The default [color picker](docs/NativeColorPicker.md) composes LVUI-owned
surfaces, channels, values and actions around a replaceable edit session.
Its native platform presentation and accessibility still need device verification.
Its [color surface interface](docs/NativeColorSurface.md) also supports replaceable
color calculations, native declarative surfaces, and an optional iiColorManager adapter.
The independent [modal](docs/NativeModal.md) owns its layout, actions, and
per-open content lifetime through a replaceable host boundary.
The default [alert button](docs/NativeAlertButton.md) and [alert](docs/NativeAlert.md)
own their styling, reactive inputs and event lifetime; some visual assets and
platform verification remain in progress.
The independent [sheet](docs/NativeSheet.md) owns adaptive placement, retained
content and drag policy through a generic host boundary.
The independent [hierarchy model](docs/OwnedHierarchyModel.md) separates view
selection, expansion and per-row height from document edits. Its [virtual list](docs/NativeOwnedHierarchyList.md)
retains visible variable-height rows, supports keyboard navigation, and sends revision-checked
drag moves to a replaceable editor. A standalone [hierarchy item](docs/NativeOwnedHierarchyItem.md)
can display an application-provided, reactive count View. Generated rows can use
reactive count Views or custom content with per-row data while preserving list
selection and navigation. Its [list toolbar](docs/NativeOwnedListToolbar.md)
owns its three icon slots and reactive behavior. The [hierarchy panel layout](docs/NativeHierarchyPanel.md)
owns toolbar, list, and footer placement, composition, and declarative generation.
The separate [flat list model](docs/OwnedListModel.md) provides replaceable data
and edit ports. Its [virtual list view](docs/NativeOwnedListView.md) owns visible rows,
and the [list panel](docs/NativeOwnedListPanel.md) composes its controls and declarative path.
The independent [table](docs/NativeOwnedTable.md)
provides virtualized two-axis drawing, selection, revision-checked cell editing,
[range clipboard operations](docs/TableClipboard.md), replaceable [row sorting](docs/TableSorting.md),
[structure edits and resizing](docs/TableStructure.md), and [merged cells](docs/TableMerge.md).
Insertion and deletion controls and per-cell accessibility remain in progress.
The independent [toolbar button](docs/NativeOwnedToolbarButton.md) owns its identity,
visuals, icon and label layout, and reactive actions. The [hierarchy toolbar](docs/NativeOwnedHierarchyToolbar.md)
retains keyed buttons and selection; direct child registration and named events remain open.
The independent [list footer](docs/NativeOwnedListFooter.md) composes three LVUI buttons
with replaceable slot commands and local icon assets.
The independent [code editor](docs/NativeCodeEditor.md) uses immutable shared text chunks,
virtualized visible lines, replaceable document ports, and a separate input session;
richer editing features and platform validation remain in progress.
The default [toggle switch](docs/NativeToggleSwitch.md) owns its state, drag,
layout and frame-driven motion, using a generic input and accessibility host.
The default [check box](docs/NativeCheckBox.md) owns its selection, geometry,
palette and generated check mark, with input and accessibility supplied by the host.
The default [radio button](docs/NativeRadioButton.md) owns its group selection,
arrow-key navigation, independent indicator-color and dot motion, including nested view publication and retirement.
The [document adapter](docs/NativeDocument.md) is also assembled by the application;
iiGUI does not import the LVUI package or document type.
See the [independence status](docs/LVUIIndependence.md) for the
actual coverage and remaining work. Historical references below describe the
previous implementation and do not establish LVUI independence.

An [`Application` root](docs/NativeApplication.md) declares one or more windows,
generating the iiSDKCore/iiGUI entry through
`lvui_add_application`. Its application scope starts before the native driver and
is shared by the windows; each window owns its local state. On Android, the same
helper builds a NativeActivity shared library with the
iiGUI Android entry; the application supplies its manifest and APK packaging.
A plain `Window` remains usable as an embedded view. `onStart`, `onReady`, and `onStop`
run in application-scope lifetime order, with readiness after all initially open
windows are attached. In a multi-window application, named windows can be opened,
closed, and reopened through deferred `openWindow` and `closeWindow` commands;
their target can be a string expression evaluated when the event occurs, and the
selected window is fixed before the request is queued. The first window starts
open, and other named windows can start closed. Reopening
creates a new window scope while preserving application state. An independent
desktop application driver keeps other windows alive when the first window closes.
`onLaunchArguments` delivers the full Core argument vector once after `onStart`
and before the native driver; the application interprets its options and paths.
`Application(onCloseRequested: ...)` can veto closing the last window or the application.
Each `Window` can also declare its own `onCloseRequested` policy; application quit
asks each open window in declaration order before asking the application, without
closing any window when approval is denied. `quitApplication()` requests another
close on a later event-loop turn after an asynchronous decision.
`onActive` and `onInactive` receive the primary host's platform
activity after readiness.
`onForeground` and `onBackground` separately receive Cocoa, Win32 and X11 window
aggregation, UIKit application, Android NativeActivity, or Web document visibility
transitions after readiness. Wayland currently rejects these handlers with
`unsupported`; the events do not grant background execution time.
`onOpenFiles` delivers native file-open batches to application state after
readiness, with bounded reentrant delivery and shutdown cancellation. Native
file-open events are currently supplied by the Cocoa and UIKit hosts.
Independent-window support depends on the native host; an internally recorded
secondary-host failure stops the loop and survives window
cleanup. Application windows can also declare `onOpened` and `onClosed` callbacks
that run within their own scope when the view is published and retired. Callback
failures stop the application loop and are returned after cleanup. Immediate API
errors remain the caller's responsibility. Wayland independent-window support is
implemented in source and checked with a mock host; live Linux execution remains
unverified. The UIKit independent-scene driver is implemented and linked into a
generated iPhoneOS app, but device execution and process-restart restoration remain
unverified or unsupported.
Independent Android windows and foreground lifecycle support on the Wayland host
remain unfinished.
Failed configuration rolls back atomically.

`lvui_install_application(MyApp DESTINATION .)` adds a desktop installation
rule that collects declared non-system runtime dependencies without modifying the
build-tree app. macOS can optionally use `ADHOC_SIGN` for local verification;
Linux requires `patchelf`. System libraries, drivers and dynamically selected
plugins remain the application's deployment responsibility. Windows and Linux
installation code has not been verified on those hosts.

The latest [native authoring verification record](docs/NativeReadinessVerification.md) distinguishes executed checks, installed-package proof, and platform limits from the historical implementation notes below.

The [whole-language completion audit](docs/Completion.md) retains the full LVRS,
language, scoped-runtime and native-module requirements. Dreamscapes QML is a
read-only observation corpus, not an application conversion. The current
[expression front end](docs/Expressions.md) adds typed operator ASTs, declaration
annotations and compiler-only constant folding. [Native bindings](docs/NativeBindings.md)
compile reactive scalar formulas, dynamic indices and activation-owned static
snapshots into C++23. [Lexical scopes](docs/LocalScopes.md) support declarations and
imports in Window bodies and nested views, with parent borrowing and independent
local objects. [Reusable View values](docs/ViewValues.md) expand lexical visual
definitions into native C++, with independent local activations and child IDs.
[Typed reusable components](docs/Components.md) add named scalar parameters,
borrowed C++ backends, defaults and lexical View slots without runtime ASTs.
[Shared scalar inputs](docs/ScalarInputs.md) evaluate each dynamic argument once
per activation or dependency update and share its native result across consumers,
forwarding views and dependent defaults, including calls to earlier function inputs.
[Typed C++ inputs](docs/ObjectInputs.md) preserve record values, owning or borrowed
object handles and const observation across reusable Views with shared native storage.
[Native method queries](docs/MethodQueries.md) call nested, indexed and passed C++
objects, track private Observable reads through Core's synchronous read scope,
and preserve receiver lifetime, editing state and failure recovery.
[Object method references](docs/ObjectMethodReferences.md) defer nested and selected
receivers without cloning them, retaining shared parent and temporary lifetimes
through replacement, failure recovery and activation retirement.
[Callable fields](docs/CallableFields.md) select native function publications,
shared move-only handlers and borrowed functors through the same typed function
and activation contracts, with C++ deciding fields versus methods.
[Returned factory captures](docs/FactoryCaptures.md) retain selected receivers and
temporary argument storage with each callable version, while preserving native
reference categories and mandatory copy elision for value parameters.
[Ordered View sequences](docs/ViewSequences.md) pass `[View]` slots and compose
children directly in their receiving parent, with per-use state and expanded child contracts.
[Keyed repetition](docs/Repetition.md) compiles stable row identity, native
reactive payloads and bounded display ranges into C++ widgets. Fixed-size native
list/grid virtualization derives its visible range from the scroll viewport,
reuses validated model snapshots, and preserves keyed anchors on insertion and resize.
[Natural-height measurement](docs/MeasuredCollections.md) connects native content
metrics, prefix indexing and sparse visible rows without view-side scripts.
[Native collection navigation](docs/CollectionNavigation.md) keeps keyed focus
and selection independent of mounted rows and preserves native editing shortcuts.
[Returned C++ callbacks](docs/ReturnedCallbacks.md) evaluate native factories,
refresh their dependencies, and retire move-only handlers with their activation.
[Typed event callback inputs](docs/CallableInputs.md) forward native method references
and returned callables through reusable views with explicit argument contracts.
[Activation-owned callback values](docs/CallbackValues.md) share one current
move-only handler across consumers and retire active versions safely.
[Inferred native values](docs/InferredValues.md) select value or function storage
from C++ types while retaining reactive aliases, fixed versions and native setters.
[Native function values](docs/FunctionValues.md) return typed results into reactive
view expressions, support readonly declarations and static factory selection,
and preserve selected function versions for static aliases without copying handlers.
[Mutable function values](docs/FunctionVariables.md) select an initial version and
replace it through typed native setters while retaining consumer identity.
[Native colors](docs/NativeColors.md) use an LVUI-owned encoded sRGB value contract
for typed view inputs, mutable state, static snapshots, channels, and reactive drawing.
Application-provided encoded color values can be converted without a color SDK dependency.
[Native layout](docs/NativeLayout.md) folds constant geometry at build time and
updates dynamic stack spacing, insets, dimensions, alignment and flex atomically
without replacing retained widgets or their local input state.
[Native Window properties](docs/NativeWindow.md) bind titles, geometry, minimums,
background colors and fonts to the owning native activation, preserve user resizing
and editing state, and commit settings only after candidate construction succeeds.
LVUI-owned Window accents also update the host and basic button palettes; other
components do not yet inherit that accent automatically.
[Readonly C++ members](docs/ReadonlyMembers.md) preserve native observation through
const objects, shared selections and standard references, retain temporary owners
during evaluation, and support noncomparable native record values.
[Native backend construction](docs/BackendConstruction.md) injects application
runtimes and services without copying nonmovable backend objects.
[Activation-owned work](docs/ActivationWork.md) uses the optional `LVUI::Runtime`
to cancel pending deliveries, retain active receivers, and retire them on the UI domain.
Dynamic composition and the full lifecycle remain incomplete.

[LVUI default style tokens](docs/StyleTokens.md) preserve 242 LVUI-owned variables
from the Figma snapshot, including compatibility aliases for the original 237.
The independent LVUI library was read back on 2026-11-02.
[Color and Metric value types](docs/StyleTypes.md) provide concise selections such
as `Color.primary`, `Metric.space8`, and contextual `.primary` / `.space8`.
Native inline styles use the same members; `var(--lvrs-...)` and `token("...")`
remain available. Window appearance,
typography and shared layout/control defaults resolve at compile time, with
isolated per-compilation overrides and no runtime Figma or token parser.
The [style example](example/StyleTokens/README.md) compiles both syntaxes into
native C++23 without linking LVRS.

The default [AppCard](docs/NativeAppCard.md) and [AppHeader](docs/NativeAppHeader.md)
use LVUI-owned styling, layout and retained child composition. Their former legacy
[generation](docs/AppSurfaces.md) is preserved as historical documentation.

[Native Link generation](docs/NativeLinks.md) uses LVUI-owned appearance, state and
navigation interfaces with child composition, typed targets, href/to aliases and
Core Observable subscriptions. Applications can supply a navigator or use LVUI's
[independent PageRouter](docs/NativePageRouter.md) with application-owned page factories.
Plain C++ members use a generated object revision.
The [route matcher and resolver](docs/NativeRoutes.md) provide LVUI-owned path
normalization, parameter capture and bounded caching without a legacy runtime.
The [navigation history](docs/NativeNavigationHistory.md) prepares and commits
bounded changes independently of a page host; PageRouter joins it to candidate-first
native page publication and shared navigation context.
The default [EventListener](docs/NativeEventListener.md) owns its bounds and
callback lifecycle while using the host's generic input and layout services.
The default [Avatar](docs/NativeMedia.md) uses LVUI-owned sizing, circular
image composition and a Figma-derived placeholder when no photo is supplied.
The default [Slider](docs/NativeSlider.md) owns range, geometry, input policy
and painting while using generic host services for input and accessibility.
The default [Stepper](docs/NativeStepper.md) owns its direction and visual policy,
with application-owned numeric state and generic host accessibility actions.
The default [ComboBox](docs/NativeComboBox.md) owns its appearance and input while
the application supplies its menu and selection.
The default [segmented controls](docs/NativeSegmentedControl.md) own their layout
and temporary child styling, including nested, conditional and repeated buttons.
The default [HelpButton](docs/NativeHelpButton.md) uses an LVUI-owned circular
button preset with a generic host action surface.
The default [dropdown buttons](docs/NativeDropdownButton.md) own their geometry,
rendering and events. The authored catalog supplies the migrated named SVG assets;
one SVG filter remains unsupported by the current decoder.
The default [InputField](docs/NativeInputField.md) owns text editing, selection,
IME composition and drawing with replaceable Unicode and platform services.
Android labels and editable `TextLine` use iiGUI's system text layout, covered
by mock and NDK link checks. Hardware keyboard input follows iiGUI's Android
bridge; basic Android IME selection and composition use an iiGUI platform
bridge, with incomplete operations and no device validation. Web editor text shaping
also remains open.
The [SearchField](docs/NativeSearchField.md) composes the same editing model with
an LVUI-owned search icon and clear action, including composition and undo.
Its Figma material uses native backdrop blur and an approximated inner shadow;
pixel parity and platform-specific input and accessibility remain unverified.
The [Tooltip model](docs/NativeTooltipModel.md) owns placement and timing, with
an application-side popup adapter and automatic target observation. The C++
facade assembles these with the application's event loop. Default declarative
generation supports target IDs or typed widget targets, text or inline content,
callbacks, and reactive available area, placement, timing, anchor, scrolling, motion, and style inputs;
remaining inputs and platform verification are still
being migrated.


The former `FRAMEWORK LVRS` / `--framework LVRS` generator has been removed;
explicit legacy selection produces a diagnostic. The native path creates
LVUI-owned components on an application-supplied iiGUI host. See the
[independence status](docs/LVUIIndependence.md) for coverage and remaining work.

[Embedded page construction](docs/EmbeddedPages.md) adds a generated `build`
entry point for existing native builders. The independent LVUI `PageRouter` can
mount application-owned page factories; no additional window or source interpreter
is required.

The independent `Modal { ... }` compiles its child View into a per-open native
C++ content factory. It borrows the outer backend, isolates child IDs, and
supports initial open after mounting; see the [LVUI contract](docs/NativeModal.md).
The default `AlertButton` and `Alert` use LVUI's independent models and adapters.
The [LVUI Alert contract](docs/NativeAlert.md) defines the currently implemented
behavior and limits.

The native AOT path now emits instantiable C++ widget classes, styled with LVRS
appearance data. [Core owns the application runtime, entrypoint and final executable attachment](docs/NativeWidgetRuntime.md),
while iiGUI adapts Core's event loop to its own C++ widgets and native OS surfaces. The Hello World uses
that path, including Core-owned workers and C++ backend teardown after owned work.

LVUI is an independent, ahead-of-time (AOT) compiled declarative UI language.
The selected language compiles `.lvui` source into target-specific native
widget code before execution. Migrated components own their layout, behavior and
drawing in LVUI, while iiGUI supplies generic hosting and platform services;
remaining legacy generation paths are still being replaced. Cocoa/AppKit and
Win32 provide platform windows, input and presentation.
The build also compiles and links the selected Metal, DirectX/Direct3D or
Vulkan graphics acceleration path for the target. The resulting native UI and
graphics code are handed to iiGUI for hosting, event delivery, lifecycle and
presentation within the iiSDK runtime. Native widget instances and GPU devices
are created at runtime by that compiled code; LVUI source is not interpreted.

The [AOT compilation and mounting design](docs/CompilationModel.md) specifies
this architecture. The [documentation index](docs/README.md) records the
selected component syntax, C++ imports, declarations and state annotations.
`View` is the common type for everything displayed visually. A view can accept
other views as child arguments through `{}`, including nested compositions;
see [View and child arguments](docs/DeclarativeSyntax.md#view-type-and-child-arguments).
Values are reactive and bound by default unless declared `@Static`; views
follow state changes without observation annotations. ViewModels follow this
[general value model](docs/ReactiveState.md).
Component interfaces declare callback inputs; method references and explicit
calls follow the [callable semantics](docs/MethodReferences.md).
The package now implements a C++23 component-language front end alongside the
earlier HTML-shaped parsing/rendering surface. It recognizes the native component
registry, three visual modifiers and core language words, and parses
brace-delimited child `View` arguments. It now implements [native AOT code generation](docs/NativeAOT.md)
for supported LVUI-owned components listed in the independence guide,
mounted as iiGUI C++ widgets on a Cocoa surface. The native Hello World compiles generated C++23
without a runtime LVUI parser. Remaining components, complete reactive input coverage,
Windows/Linux widget input adapters and additional GPU code generation remain separate work.

## Implemented component language

```cpp
#include <LVUI/Language.h>
auto source = "VStack { Label(text: \"Hello\") }";
auto document = lvui::parse_view(source);
// document->root().children contains a typed Label View.
```

`keywords()`, `find_keyword()` and `is_reserved_identifier()` expose the actual
reserved-word table used by `tokenize()` and `parse_view()`. The native
`lvui-check` tool validates component files at build time; it does not execute
the UI or imported backend objects. See the [implemented grammar and API](docs/ComponentLanguage.md)
and [LVUI-owned reserved-word registry](docs/KeywordRegistry.md).
Build, parser and installed-consumer evidence is recorded in [verification](docs/Verification.md).

The language owns its checked-in keyword registry. Legacy source observations are
historical evidence and do not determine current vocabulary. Features still to be
ported to LVUI are tracked in the independence status.

## Immediate View declarations

LVUI declares native Views directly in the form `Name(constructor inputs) {
body arguments }`. The body may contain named inputs and nested Views, separated
by commas or newlines. It is not an HTML or CSS document language.
For example, `HStack() { VStack() }` passes a `VStack` View to an `HStack`.
Empty constructor parentheses or an empty body may be omitted; a View with neither
inputs nor a body can be written as just its name, such as `Label`. Each component
validates the content types and number of child Views it supports.

```text
import vm ToolbarModel
const RandomString: () -> String = vm.randomString
const random = vm.icon
Window() {
    HStack(id: toolbar) {
        gap: Metric.space8,
        IconButton(label: RandomString(), icon: random) { onClick: vm.activate() },
    },
}
```

`ToolbarModel` provides the application values and operations. Value calls are
evaluated when their bindings are read; an event call is performed when that
event occurs. `lvui-check` and native AOT compilation accept View declarations,
not HTML tags. [Immediate declarations](docs/ImmediateDeclaration.md) describe
the syntax and lifetime. The older C++ `Document` parser remains a separate
legacy API for existing consumers; it is not the LVUI source language.

LVUI generates native C++ and iiGUI renders it. Application state is provided
by imported C++ objects and iiSDKCore queries and bindings; no browser or
language interpreter is needed by the resulting application.
Objects and methods are defined in C++ and imported. LVUI accepts variables,
constants, C++ method references, View declarations and inline event blocks;
it does not declare object literals or named script bodies.

Event handler bodies can contain statements directly, without a `<script>` tag.
They run only when the event is delivered and can update local View state and
call imported C++ methods. Native LVUI `if` and collection `for` statements
are compiled ahead of time. See [tagless event scripts](docs/ImmediateDeclaration.md).

Compose layout with `HStack` for horizontal placement, `VStack` for vertical
placement, and `ZStack` for overlapping children. The stacks can be nested and
are lowered to iiGUI; an HTML `div` is not a built-in LVUI View.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DiiSDKCore_DIR=/path/to/iiSDKCore/lib/cmake/iiSDKCore \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local/SDK/LVUI"
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/lvui-check docs/examples/ReservedComponents.lvui
./build/lvui-compile --name HelloWorld /path/to/HelloWorld.lvui
cmake --install build
```

CMake 3.31+, C++23 and iiSDKCore 0.18.1+ Contracts are required. Test/source-inventory
checks additionally use Python 3. The parser tests do not open windows or
require GPU hardware. The install command above updates the user's SDK and is
a separate action from building. The [build layout](docs/BuildLayout.md) explains
temporary resources, optional native validation and remaining legacy tests.
Source and test results describe this implementation, not full HTML/CSS or
Svelte compatibility.

## Python compiler API

The installed `ii_lvui` Python package invokes the native LVUI compiler without
Qt, a GUI host, or temporary source files. Add `<prefix>/share/LVUI/python` to
Python's module path, then use `compile_native` or `compile_style_header`:

```python
import ii_lvui

cpp = ii_lvui.compile_native('Window { Label(text: "Hello") }', name='HelloView')
```

Diagnostics carry source locations in `CompileError`. The API currently covers
compilation and style-header generation, not live native widget control. See
[the Python API contract](docs/PythonAPI.md) for installation and limitations.

## Source header layout

Public headers and implementations live together in responsibility directories below `src/`.
There is no separate authored `include/` tree. Existing public `#include` names
and exported CMake targets are unchanged; installation still places only public
headers in the package include directory. Generated export headers belong to
`build/`, not to the authored source tree.

Validate this layout from the SDK workspace with
`ctest --test-dir build -R '^LVUI.source_layout$' --output-on-failure`. Reconfigure, build, run CTest,
and verify a staged installed consumer after changing source/header paths.

The standalone `TableCellItem`, `TableRow`, and `TableHeader` components use
LVUI-owned models and native composition; their remaining limitations are described
in their respective contracts. The default `Table` uses the independent model and
host described in [its contract](docs/NativeOwnedTable.md).

Default toolbar keywords generate LVUI-owned native objects and scoped bindings;
the [toolbar AOT history](docs/Toolbars.md) describes the former legacy path.

See the [native Card contract](docs/NativeOwnedCard.md) and [image contract](docs/NativeImage.md)
for typed images, retained preview factories, and supported image loading.
An application can register shared [image services](docs/NativeImageLoading.md) once in its
backend construction policy for automatic delivery to each generated view activation.
Conditional branches can also own local imports, values, and reusable views with the same
activation and retirement boundaries as their selected content; see [conditional views](docs/ConditionalViews.md).

The default [native material AOT](docs/NativeMaterials.md) uses LVUI-owned material
models, radial gradients, validated SVG outlines, reactive styles, and motion on
generic iiGUI host services. Platform GPU parity remains incomplete; the former
LVRS material generation path is no longer available.

See [native CodeEditor AOT](docs/NativeCodeEditor.md) for a retained PlainText/NoWrap
editor, scoped document models, typed edit callbacks and Core property bindings.
RichText TextEditor remains a separate migration requirement.

See [native Tooltip AOT](docs/NativeTooltip.md) for forward target-ID attachment,
retained custom content, checked millisecond inputs and same-instance bindings.
Headless tests cover the supported contracts; native application execution and
platform visual verification remain separate.

[Application style profiles](docs/StyleProfiles.md) share typed Color/Metric values between LVUI views and handwritten native C++ panels through a generated constexpr header. `STYLE_FILE` and `lvui_generate_styles` track the same build input; no runtime LVUI parser is required.

[NativeView](docs/NativeViews.md) embeds typed C++ visual providers in AOT layouts.
The [Dreamscapes readiness contract](docs/NativeReadiness.md) separates proven
native authoring capabilities, final execution, installed-package verification,
and the later product migration.

The independent [list item](docs/NativeOwnedListItem.md) shares its row layout and drawing with the owned list view and provides toggle, checkbox, action, stepper, selector, and inline-edit compositions; other compound variants are still being implemented.

The independent [card model and native host](docs/NativeOwnedCard.md) expose replaceable image inputs and actions through C++ and declarative views; custom components and detailed visual parity remain in progress.

An independent [table header](docs/NativeOwnedTableHeader.md) reuses the LVUI table viewport model through C++ and declarative views.

The independent [table cell](docs/NativeOwnedTableCellItem.md) provides replaceable validation and conflict-aware editing through C++ and declarative views.

An independent [table row](docs/NativeOwnedTableRow.md) composes retained keyed cells through C++ and declarative views.

Sources and adjacent headers are grouped by responsibility under `src/`, while installed `<LVUI/...>` include paths remain stable. See [source layout](docs/SourceLayout.md).

<!-- workspace-docs:readme:start -->
## Documentation entry points

- Human overview: this English `README.md`.
- Agent instructions: [AGENT.md](AGENT.md).
- Human documentation: [documentation index](docs/README.md).
- Structured agent documentation: [agent index](docs/agent-index.json).
<!-- workspace-docs:readme:end -->
