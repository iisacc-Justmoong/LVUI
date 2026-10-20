# LVUI

LVUI is an independent, ahead-of-time (AOT) compiled declarative UI language.
The selected language compiles `.lvui` source into native machine code before
execution. The compiled UI is mounted in the iiSDK runtime as an iiGUI drawing
target, without a language interpreter, JVM or .NET/CLR execution layer.

The [AOT compilation and mounting design](docs/CompilationModel.md) specifies
this architecture. The [documentation index](docs/README.md) records the
selected component syntax, C++ imports, declarations and state annotations.
Values are reactive and bound by default unless declared `@Static`; views
follow state changes without observation annotations. ViewModels follow this
[general value model](docs/ReactiveState.md).
Component interfaces declare callback inputs; method references and explicit
calls follow the [callable semantics](docs/MethodReferences.md).
The current 0.1 package below implements an earlier parsing/rendering surface;
it does not yet implement the AOT compiler or compiled-target mounting API.

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

The current `LVUI::LVUI` C++23 package owns only syntax, validated styles,
diagnostics and an immutable document tree. Its sole SDK dependency is
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
cmake --install build
```

CMake 3.31+, C++23 and iiSDKCore 0.16+ Contracts are required. The parser tests
do not open windows or require GPU hardware. Installation above is staged under
`build/`; it does not replace a user's installed SDK. Source and test results
describe this implementation, not full HTML/CSS or Svelte compatibility.
