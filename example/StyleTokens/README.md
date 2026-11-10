# LVUI style tokens

This installed-package consumer compiles both component and HTML-shaped `.lvui`
sources into native C++23 using LVUI-owned style values and ProgressBar geometry.
The application supplies iiGUI as the host. It does not find or link LVRS.
The final view has no runtime token lookup or LVUI source parser. A separate
catalog probe exercises the public LVUI API, including the independent search
field tokens and legacy lookup compatibility.

The expected accent and window values follow LVUI's checked-in Figma color
defaults; native pixel checks derive their background from the same window token.

The examples use `Color.window`, `Metric.space12`, contextual `.space24` and
typed `const`/`var` declarations. The inline view also verifies that a Metric
Observable updates the same native slider after mounting.

Build from the LVUI repository into its existing `build/` directory with a
Qt-free native iiGUI kit available to CMake.

Use matching iiGUI headers and libraries from a revision that provides native
accessibility semantics and widget font families. An older installed iiGUI kit
may not compile generated views from the current LVUI sources:

```sh
cmake -S . -B build -DLVUI_BUILD_STYLE_EXAMPLE=ON \
  -DCMAKE_PREFIX_PATH="/path/to/native-sdk/prefix"
cmake --build build --target LVUIStyleTokenConsumer LVUITokenCatalogConsumer
ctest --test-dir build --output-on-failure -R '^LVUI.StyleTokens.(InstalledCatalog|CompiledAppearance)$'
```

When the sibling iiGUI source checkout is present, headless host fixtures use a
private forwarding header in this example's build directory to inspect widget
state. This is test infrastructure, not an installed or public iiGUI API.

`LVUI_STYLE_TOKEN_NATIVE_TESTS=ON` additionally registers two GPU tests for
Metal and Vulkan on macOS. They create a short-lived window and verify the
background, border and mounted font properties. Leave them disabled unless
native execution is requested. See [the token contract](../../docs/StyleTokens.md).
