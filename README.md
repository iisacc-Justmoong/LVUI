# LVUI

[LVRS default style tokens](docs/StyleTokens.md) preserve all 208 variables from
the LVRS Figma source, including types, modes and aliases.
[Color and Metric value types](docs/StyleTypes.md) provide concise selections such
as `Color.primary`, `Metric.space8`, and contextual `.primary` / `.space8`.
Native inline styles use the same members; `var(--lvrs-...)` and `token("...")`
remain available. Window appearance,
typography and shared layout/control defaults resolve at compile time, with
isolated per-compilation overrides and no runtime Figma or token parser.
The [installed-package example](example/StyleTokens/README.md) compiles both
syntaxes into native LVRS C++23.

[AppHeader and AppCard](docs/AppSurfaces.md) compile to retained native C++
composition with typed live inputs and ordinary child slots in FRAMEWORK LVRS.
Runtime verification remains deferred to the final whole-goal batch.

[Native Link generation](docs/NativeLinks.md) supports rich child composition,
typed target/parameter inputs, href/to aliases and Core Observable subscriptions.
Plain C++ members remain construction values; no source interpreter is shipped.


Native LVRS generation is available with `FRAMEWORK LVRS` / `--framework LVRS`.
It constructs LVRS components as iiGUI's own GPU-rendered C++ widgets. Cocoa
provides the OS boundary rather than per-component controls. The current
[LVRS native implementation](docs/LVRSNative.md) expands component generation and
retains Core-owned application and backend lifetimes. Full LVRS parity is ongoing.

[Embedded page construction](docs/EmbeddedPages.md) adds a generated `build`
entry point for existing native builders. LVRS `PageRouter(router: vm.router)`
mounts a scoped C++ controller whose compiled page factories use that entry point;
it creates no additional window or source interpreter. Execution is deferred.

`Modal { ... }` compiles its child View into a per-open native C++ content factory.
It borrows the outer backend, isolates child IDs, and supports initial open after
mounting. The [Modal contract](../LVRS/docs/native/Modal.md) records supported
geometry, actions and remaining behavior.

`Alert` and `AlertButton` generate LVRS C++ composition, including typed
`ButtonEvent` or parameterless method references, source text/image aliases and
initial open after mounting. The [native Alert contract](../LVRS/docs/native/Alert.md)
describes retained widgets, the original layouts and Metal GPU glass, together
with the remaining shadow, motion, reactive and platform work.

The native AOT path now emits instantiable C++ widget classes, styled with LVRS
appearance data. [Core owns the application runtime, entrypoint and final executable attachment](docs/NativeWidgetRuntime.md),
while iiGUI adapts Core's event loop to its own C++ widgets and native OS surfaces. The Hello World uses
that path, including Core-owned workers and C++ backend teardown after owned work.

LVUI is an independent, ahead-of-time (AOT) compiled declarative UI language.
The selected language compiles `.lvui` source into target-specific native
widget code before execution. Component layout, input behavior and drawing belong
to iiGUI; Cocoa/AppKit and Win32 provide platform windows, input and presentation.
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
earlier HTML-shaped parsing/rendering surface. It recognizes 107 LVRS visual
component keywords, three visual modifiers and eight core words, and parses
brace-delimited child `View` arguments. It now implements [native AOT code generation](docs/NativeAOT.md)
for supported LVRS components listed in the native implementation guide,
mounted as iiGUI C++ widgets on a Cocoa surface. The native Hello World compiles generated C++23
without a runtime LVUI parser. Remaining components, reactive property tracking,
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
and [complete reserved-word/source catalog](docs/ReservedWords.md).
Build, parser and installed-consumer evidence is recorded in [verification](docs/Verification.md).

The current catalog audits all 229 LVRS source files, including 91 QML file types,
25 inline components and 33 registered C++ types. LVUI builds from a checked-in
snapshot and does not depend on LVRS, Qt or QML at build time or runtime.
Tests detect snapshot drift when the sibling LVRS source is available.

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

CMake 3.31+, C++23 and iiSDKCore 0.16+ Contracts are required. Test/source-inventory
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

`TableCellItem`, `TableRow`, and `TableHeader` now generate native C++ cells,
editable rows, and interactive column headers with typed validation callbacks.
The [table contract](../LVRS/docs/native/Tables.md) distinguishes these parts from the remaining
full Table model, selection, sorting, and virtualization work.

LVRS toolbar keywords generate typed native objects, retained manual registration
and scoped model/property bindings; see [toolbar AOT](docs/Toolbars.md).

See [native Card and Avatar AOT](docs/NativeMedia.md) for typed images, retained preview factories and explicit unsupported-loader diagnostics.

See [native material AOT](docs/NativeMaterials.md) for typed density, capture domains, custom silhouettes and shared Core motion.

See [native CodeEditor AOT](docs/NativeCodeEditor.md) for a retained PlainText/NoWrap
editor, scoped document models, typed edit callbacks and Core property bindings.
RichText TextEditor remains a separate migration requirement.

See [native Tooltip AOT](docs/NativeTooltip.md) for forward target-ID attachment,
retained custom content, checked millisecond inputs and same-instance bindings.
Application and test execution remains deferred to the final whole-goal batch.

[Application style profiles](docs/StyleProfiles.md) share typed Color/Metric values between LVUI views and handwritten native C++ panels through a generated constexpr header. `STYLE_FILE` and `lvui_generate_styles` track the same build input; no runtime LVUI parser is required.
