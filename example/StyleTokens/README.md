# LVUI style tokens

This installed-package consumer compiles both component and HTML-shaped `.lvui`
sources into native C++23 using LVUI-owned style values and ProgressBar geometry.
The application supplies iiGUI as the host. It does not find or link LVRS.
The final view has no runtime token lookup or LVUI source parser. A separate
catalog probe exercises the public LVUI API.

The examples use `Color.window`, `Metric.space12`, contextual `.space24` and
typed `let`/`var` declarations. The inline view also verifies that a Metric
Observable updates the same native slider after mounting.

Build from the LVUI repository into its existing `build/` directory with a
Qt-free native iiGUI kit available to CMake:

```sh
cmake -S . -B build -DLVUI_BUILD_STYLE_EXAMPLE=ON \
  -DCMAKE_PREFIX_PATH="/path/to/native-sdk/prefix"
cmake --build build --target LVUIStyleTokenConsumer LVUITokenCatalogConsumer
ctest --test-dir build --output-on-failure -R '^LVUI.StyleTokens.(InstalledCatalog|CompiledAppearance)$'
```

`LVUI_STYLE_TOKEN_NATIVE_TESTS=ON` additionally registers two GPU tests for
Metal and Vulkan on macOS. They create a short-lived window and verify the
background, border and mounted font properties. Leave them disabled unless
native execution is requested. See [the token contract](../../docs/StyleTokens.md).
