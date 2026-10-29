# LVUI

LVUI owns its components and inherits the visual language and design parameters
of legacy LVRS. LVRS is not the intended implementation or runtime dependency.
The independent [icon catalog](docs/NativeIcons.md) embeds LVUI-authored
SVG designs and accepts a replaceable application source; remaining icon assets
and platform appearance verification are still in progress.
The existing `FRAMEWORK LVRS` generator is a legacy dependency that has **not yet
been fully replaced**. The default native path now owns its ProgressBar geometry,
paint recipe, reactive visual configuration and frame-driven motion. The
[motion model](docs/NativeMotion.md) is standard C++ and accepts host frame times.
The default [Image path](docs/NativeImage.md) owns validation, fitting, cropping
and paint recipes in LVUI while the application supplies immutable pixels and a host.
The default [button path](docs/NativeButton.md) owns its tone, state-dependent
painting and content geometry; the host supplies generic input and text services.
The default [divider path](docs/NativeDivider.md) owns line layout and style;
the application host supplies only a non-interactive drawing surface.
The default [menu path](docs/NativeMenu.md) owns its entries, selection,
keyboard navigation, rendering and dismissal policy, including direct items,
dividers and nested menus; dynamically generated children remain in progress.
The default [popover path](docs/NativePopover.md) owns validated design inputs
and reactive updates while the host supplies generic popup placement and input.
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
selection and expansion from document edits. Its [virtual list](docs/NativeOwnedHierarchyList.md)
retains visible rows, supports keyboard navigation, and sends revision-checked
drag moves to a replaceable editor. Its [list toolbar](docs/NativeOwnedListToolbar.md)
owns its three icon slots and reactive behavior. The [hierarchy panel layout](docs/NativeHierarchyPanel.md)
owns toolbar, list, and footer placement, composition, and declarative generation.
The separate [flat list model](docs/OwnedListModel.md) provides replaceable data
and edit ports. Its [virtual list view](docs/NativeOwnedListView.md) owns visible rows,
and the [list panel](docs/NativeOwnedListPanel.md) composes its controls and declarative path.
The independent [table](docs/NativeOwnedTable.md)
provides virtualized two-axis drawing, selection, revision-checked cell editing,
[range clipboard operations](docs/TableClipboard.md), replaceable [row sorting](docs/TableSorting.md)
and [structure edits and resizing](docs/TableStructure.md); merged cells and other
advanced table operations remain in progress.
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
indicator and dot motion, including nested view publication and retirement.
The [document adapter](docs/NativeDocument.md) is also assembled by the application;
iiGUI does not import the LVUI package or document type.
See the [independence status](docs/LVUIIndependence.md) for the
actual coverage and remaining work. Historical references below describe the
previous implementation and do not establish LVUI independence.

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
[Native colors](docs/NativeColors.md) use iiColorManager's header-only value contract
for typed view inputs, mutable state, static snapshots, channels, and reactive drawing.
[Native layout](docs/NativeLayout.md) folds constant geometry at build time and
updates dynamic stack spacing, insets, dimensions, alignment and flex atomically
without replacing retained widgets or their local input state.
[Native Window properties](docs/NativeWindow.md) bind titles, geometry, minimums,
appearance and LVRS accents to the owning native activation, preserve user resizing
and editing state, and commit settings only after candidate construction succeeds.
[Readonly C++ members](docs/ReadonlyMembers.md) preserve native observation through
const objects, shared selections and standard references, retain temporary owners
during evaluation, and support noncomparable native record values.
[Native backend construction](docs/BackendConstruction.md) injects application
runtimes and services without copying nonmovable backend objects.
[Activation-owned work](docs/ActivationWork.md) uses the optional `LVUI::Runtime`
to cancel pending deliveries, retain active receivers, and retire them on the UI domain.
Dynamic composition and the full lifecycle remain incomplete.

[LVRS default style tokens](docs/StyleTokens.md) preserve all 208 variables from
the LVRS Figma source, including types, modes and aliases.
[Color and Metric value types](docs/StyleTypes.md) provide concise selections such
as `Color.primary`, `Metric.space8`, and contextual `.primary` / `.space8`.
Native inline styles use the same members; `var(--lvrs-...)` and `token("...")`
remain available. Window appearance,
typography and shared layout/control defaults resolve at compile time, with
isolated per-compilation overrides and no runtime Figma or token parser.
The [style example](example/StyleTokens/README.md) compiles both syntaxes into
native C++23 without linking LVRS.

The default [AppCard](docs/NativeAppCard.md) and [AppHeader](docs/NativeAppHeader.md)
use LVUI-owned styling, layout and retained child composition. Their legacy
[generation](docs/AppSurfaces.md) remains available through FRAMEWORK LVRS during
the transition.

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
The default [Avatar](docs/NativeMedia.md) uses LVUI-owned sizing and circular
image composition with application-supplied pixels.
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
rendering and events; additional named icon assets remain to be migrated.
The default [InputField](docs/NativeInputField.md) owns text editing, selection,
IME composition and drawing with replaceable Unicode and platform services.
Android and Web text shaping, automatic wrapping, and actual device verification
remain open work.
The [Tooltip model](docs/NativeTooltipModel.md) owns placement and timing, with
an application-side popup adapter and automatic target observation. The C++
facade assembles these with the application's event loop. Default declarative
generation supports target IDs, text or inline content, callbacks, and static
placement and style inputs; remaining inputs and platform verification are still
being migrated.


The legacy `FRAMEWORK LVRS` / `--framework LVRS` generator still emits LVRS
components and is being replaced. The default path creates LVUI-owned components
on an application-supplied iiGUI host. See the [independence status](docs/LVUIIndependence.md)
for implemented controls and the remaining migration work.

[Embedded page construction](docs/EmbeddedPages.md) adds a generated `build`
entry point for existing native builders. LVRS `PageRouter(router: vm.router)`
mounts a scoped C++ controller whose compiled page factories use that entry point;
it creates no additional window or source interpreter. Execution is deferred.

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
earlier HTML-shaped parsing/rendering surface. It recognizes 109 native visual
component keywords, three visual modifiers and eight core words, and parses
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
historical evidence and do not determine current vocabulary. The remaining
legacy framework generator is tracked separately in the independence status.

## Current 0.1 parser and rendering integration

The existing `.lvui` parser accepts familiar HTML-shaped tags and inline
styles, similar to the markup portion of a Svelte component. It does not load
an HTML document in a browser.

```html
<main style="padding:24px; gap:16px; background:#101827; color:#ffffff">
  <h1>Native LVUI</h1>
  <div style="flex-direction:row; gap:12px">
    <section style="padding:16px; background:#20304a; border-radius:12px">
      <p>Parsed, laid out and drawn by iiGUI.</p>
    </section>
    <section style="padding:16px; background:#274638; border-radius:12px">
      <p>One C++23 application, native desktop graphics.</p>
    </section>
  </div>
</main>
```

The `LVUI::LVUI` C++23 package owns syntax, validated styles, source diagnostics,
immutable document trees and build-time C++ code generation. Its sole SDK dependency is
iiSDKCore Contracts (UTF-8 validation). It does not depend on iiGUI, a window
system, Qt, a WebView, JavaScript or a browser parser. iiGUI consumes this tree,
measures native fonts, lays out logical boxes and submits native graphics.
This dependency goes in one direction:
application → iiGUI → LVUI → Core Contracts.

```cpp
#include <LVUI/Document.h>
auto document = lvui::parse("<view><p>Hello</p></view>");
if (!document) {
    // document.error(): offset, line, column, message
}
```

The initial 0.1 language surface is deliberately bounded. See the
[language specification](docs/Language.md) for exact supported syntax and
layout semantics. The selected AOT language is documented separately. Event
handling, data binding and application state remain outside this first
parser/rendering implementation; no scripting engine is embedded. Unsupported
input fails with a diagnostic instead of being ignored.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DiiSDKCore_DIR=/path/to/iiSDKCore/lib/cmake/iiSDKCore \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/install"
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/lvui-check docs/examples/ReservedComponents.lvui
./build/lvui-compile --name HelloWorld /path/to/HelloWorld.lvui
cmake --install build
```

CMake 3.31+, C++23 and iiSDKCore 0.18.1+ Contracts are required. Test/source-inventory
checks additionally use Python 3; `BUILD_TESTING=OFF` needs no Python. The parser tests
do not open windows or require GPU hardware. Installation above is staged under
`build/`; it does not replace a user's installed SDK. Source and test results
describe this implementation, not full HTML/CSS or Svelte compatibility.

## Source header layout

Public headers and implementations live together in `src/LVUI/`.
There is no separate authored `include/` tree. Existing public `#include` names
and exported CMake targets are unchanged; installation still places only public
headers in the package include directory. Generated export headers belong to
`build/`, not to the authored source tree.

Validate this layout from the SDK workspace with
`python3 -B tests/test_source_header_layout.py`. Reconfigure, build, run CTest,
and verify a staged installed consumer after changing source/header paths.

The standalone `TableCellItem`, `TableRow`, and `TableHeader` components still
need independent LVUI implementations. The default `Table` uses the independent
model and host described in [its contract](docs/NativeOwnedTable.md).

Default toolbar keywords generate LVUI-owned native objects and scoped bindings;
the [toolbar AOT history](docs/Toolbars.md) separately describes the remaining legacy path.

See [native Card and Avatar AOT](docs/NativeMedia.md) for typed images, retained preview factories and explicit unsupported-loader diagnostics.

The default [native material AOT](docs/NativeMaterials.md) uses LVUI-owned material
models, radial gradients, validated SVG outlines, reactive styles, and motion on
generic iiGUI host services. Platform GPU parity remains incomplete; the optional legacy
`FRAMEWORK LVRS` material path still depends on LVRS.

See [native CodeEditor AOT](docs/NativeCodeEditor.md) for a retained PlainText/NoWrap
editor, scoped document models, typed edit callbacks and Core property bindings.
RichText TextEditor remains a separate migration requirement.

See [native Tooltip AOT](docs/NativeTooltip.md) for forward target-ID attachment,
retained custom content, checked millisecond inputs and same-instance bindings.
Application and test execution remains deferred to the final whole-goal batch.

[Application style profiles](docs/StyleProfiles.md) share typed Color/Metric values between LVUI views and handwritten native C++ panels through a generated constexpr header. `STYLE_FILE` and `lvui_generate_styles` track the same build input; no runtime LVUI parser is required.

[NativeView](docs/NativeViews.md) embeds typed C++ visual providers in AOT layouts.
The [Dreamscapes readiness contract](docs/NativeReadiness.md) separates proven
native authoring capabilities, final execution, installed-package verification,
and the later product migration.

The independent [list item](docs/NativeOwnedListItem.md) shares its row layout and drawing with the owned list view and provides toggle, checkbox, action, stepper, and selector compositions; other compound variants are still being implemented.

The independent [card model and native host](docs/NativeOwnedCard.md) expose replaceable image inputs and actions through C++ and declarative views; custom components and detailed visual parity remain in progress.

An independent [table header](docs/NativeOwnedTableHeader.md) reuses the LVUI table viewport model through C++ and declarative views.

The independent [table cell](docs/NativeOwnedTableCellItem.md) provides replaceable validation and conflict-aware editing through C++ and declarative views.

An independent [table row](docs/NativeOwnedTableRow.md) composes retained keyed cells through C++ and declarative views.

<!-- workspace-docs:readme:start -->
## Documentation entry points

- Human overview: this English `README.md`.
- Agent instructions: [AGENT.md](AGENT.md).
- Human documentation: [documentation index](docs/README.md).
- Structured agent documentation: [agent index](docs/agent-index.json).
<!-- workspace-docs:readme:end -->
