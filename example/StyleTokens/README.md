# LVRS style tokens

This installed-package consumer compiles both component and HTML-shaped `.lvui`
sources into native LVRS C++23. The final view has no runtime token lookup or
LVUI source parser. A separate catalog probe exercises the public LVUI API.

The examples use `Color.window`, `Metric.space12`, contextual `.space24` and
typed `let`/`var` declarations. The inline view also verifies that a Metric
Observable updates the same native slider after mounting.

Build under the LVUI repository's `build/` directory with a Qt-free native SDK kit:

```sh
cmake --install build --prefix "$PWD/build/style-token-prefix"
cmake -S example/StyleTokens -B build/style-token-consumer/build -G Ninja \
  -DCMAKE_PREFIX_PATH="$PWD/build/style-token-prefix;/path/to/native-sdk/prefix" \
  -DLVUI_STYLE_TOKEN_NATIVE_TESTS=ON
cmake --build build/style-token-consumer/build --parallel 4
ctest --test-dir build/style-token-consumer/build --output-on-failure
```

The two optional GPU tests currently check Metal and Vulkan on macOS. Disable
them on other hosts; the C++ sources, token catalog and generated code remain
portable. They create a short-lived window and verify background, border and
mounted font properties. See [the token contract](../../docs/StyleTokens.md).
