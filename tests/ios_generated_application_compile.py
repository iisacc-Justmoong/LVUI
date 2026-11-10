"""Compile a generated multi-window LVUI entry for iPhoneOS without running it."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--gui-include", type=Path, required=True)
    parser.add_argument("--core-includes", required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    build = args.build.resolve()
    env = os.environ.copy()
    for key in list(env):
        if key in {"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH", "CMAKE_PREFIX_PATH"} or key.startswith(("QT_", "QML", "LVRS_", "DYLD_", "LD_")):
            env.pop(key, None)
    sdk = subprocess.check_output(["xcrun", "--sdk", "iphoneos", "--show-sdk-path"], text=True, env=env).strip()
    compiler = subprocess.check_output(["xcrun", "--sdk", "iphoneos", "--find", "clang++"], text=True, env=env).strip()
    includes = [build, build / "include", root / "src", root / "tests/fixtures", args.gui_include]
    includes += [Path(value) for value in args.core_includes.split("|") if value]
    source = build / "LVUINativeMultiWindowFixture.application.cpp"
    if not source.is_file() or not (build / "LVUINativeMultiWindowFixture.generated.h").is_file():
        raise RuntimeError("The generated LVUI multi-window fixture has not been built")
    with tempfile.TemporaryDirectory(prefix="ios-generated-application-", dir=build) as output:
        obj = Path(output) / "Application.o"
        command = [compiler, "-std=c++23", "-target", "arm64-apple-ios18.0", "-isysroot", sdk,
                   "-DIIGUI_STATIC_DEFINE"]
        for path in includes:
            command += ["-I", str(path)]
        subprocess.run([*command, "-c", str(source), "-o", str(obj)], check=True, env=env)
        symbols = subprocess.check_output(["xcrun", "nm", "-u", str(obj)], text=True, env=env)
        symbols = subprocess.run(["c++filt"], input=symbols, text=True, stdout=subprocess.PIPE, check=True, env=env).stdout
        for symbol in ("supports_application_driver", "set_application_driver", "ii::gui::Host::show"):
            if symbol not in symbols:
                raise RuntimeError("Generated iOS app did not reference " + symbol)
    print("Generated multi-window LVUI application compiled for iPhoneOS; no app executed")


if __name__ == "__main__":
    main()
