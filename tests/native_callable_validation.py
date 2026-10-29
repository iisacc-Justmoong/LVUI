"""Compile generated callback adapters and reject incompatible C++ contracts."""
from pathlib import Path
import argparse
import json
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--cxx", required=True)
    parser.add_argument("--core", type=Path, required=True)
    parser.add_argument("--color", type=Path, required=True)
    parser.add_argument("--gui", type=Path, required=True)
    args = parser.parse_args()
    source_root = Path(__file__).resolve().parents[1]
    build = args.build.resolve()
    cases = {
        "valid": ("void run() {}", "()", "PushButton(action)", "vm.run", None),
        "return_type": ("int run() { return 1; }", "()", "PushButton(action)", "vm.run", "A callback declared Void must return void"),
        "event_arity": ("void run(std::string) {}", "(String)", "PushButton(action)", "vm.run", ("Callback event arity does not match", "constraints not satisfied")),
        "event_type": ("void run(std::int64_t) {}", "(Int)", "InputField(onEdited: action)", "vm.run", "Callback Int input requires an integral event payload"),
        "missing_argument": ("void run(int) {}", "()", "PushButton(action)", "vm.run", "run"),
        # Field/method selection uses constrained C++ accessors. Access rejection
        # may surface at invoke rather than spelling "private" in diagnostics.
        "private_member": ("private: void run() {}", "()", "PushButton(action)", "vm.run", "run"),
        "immediate_void": ("void run() {}", "()", "PushButton(action)", "vm.run()", "void"),
        "valid_function": ('std::string run(std::int64_t) { return "Native"; }', "(Int)", "Label(text: action(2))", "vm.run", None),
        "wrong_bool_result": ("int run() { return 1; }", "()", "Label(text: action())", "vm.run", "Callback Bool input requires Bool"),
        "wrong_string_result": ("int run() { return 1; }", "()", "Label(text: action())", "vm.run", "Callback String input requires native text"),
        "void_function_result": ("void run() {}", "()", "Label(text: action())", "vm.run", "A value-returning callable requires a native result"),
        "variable_valid": ('std::string run(std::int64_t) { return "Native"; }', "(Int)", "Label(text: action(2))", "vm.run", None),
        "variable_wrong_result": ("int run() { return 1; }", "()", "Label(text: action())", "vm.run", "Callback Bool input requires Bool"),
        "variable_wrong_replacement": ('std::string run(std::int64_t) { return "Native"; }', "(Int)", "Label(text: action(2))", "vm.run", "Callback String input requires native text"),
        "valid_color_function": ('iiColorManager::Rgba run(double) { return {.2,.4,.6,1}; }', "(Double)", "Label(color: action(.5))", "vm.run", None),
        "wrong_color_result": ('double run() { return .5; }', "()", "Label(color: action())", "vm.run", "Callback Color requires an encoded sRGB color value"),
        "linear_color_result": ('ii::gui::Color run() { return {.2,.4,.6,1}; }', "()", "Label(color: action())", "vm.run", "Callback Color requires an encoded sRGB color value"),
        "wrong_color_argument": ('std::string run(ii::gui::WidgetColor) { return "Native"; }', "(Color)", "Label(text: action(true))", "vm.run", "Callback Color requires an encoded sRGB color value"),
    }
    results = {"valid_function":"String","wrong_bool_result":"Bool","wrong_string_result":"String","void_function_result":"String"}
    results.update(variable_valid="String",variable_wrong_result="Bool",variable_wrong_replacement="String")
    results.update(valid_color_function="Color",wrong_color_result="Color",linear_color_result="Color",wrong_color_argument="String")
    result = {}
    with tempfile.TemporaryDirectory(prefix="callback-types-", dir=build) as temporary:
        root = Path(temporary)
        for name, (body, signature, view, action, expected) in cases.items():
            fixture = root / "Fixture.h"
            fixture.write_text("#pragma once\n#include <cstdint>\n#include <string>\nstruct Backend final {" + body + "};\n")
            source = root / "View.lvui"
            source.write_text("import vm Backend\nlet Control(action: " + signature + " -> " + results.get(name,"Void") + "): View = " + view + "\nWindow { Control(action: " + action + ") }\n")
            if name.startswith("variable_"):
                source.write_text("import vm Backend\nvar action: " + signature + " -> " + results[name] + " = " + action + "\nWindow { " + view + " }\n")
            generated = root / "View.generated.h"
            command = [str(args.compiler), "--name", "CallbackContract", "--include", "Fixture.h", str(source)]
            generation = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if generation.returncode:
                raise RuntimeError(f"AOT generation failed for {name}: {generation.stderr}")
            generated.write_text(generation.stdout)
            probe = root / "probe.cpp"
            probe.write_text('#include "View.generated.h"\nauto factory = &lvui_generated::CallbackContract::build;\n')
            if name=="variable_wrong_replacement":
                probe.write_text(probe.read_text()+'void rejected(lvui_generated::CallbackContract& view) { (void)view.set_action_native([] { return [](std::int64_t) { return 7; }; }); }\n')
            includes = [root, source_root / "src", args.build / "lvui-generated-icons", (args.core / "../../../include").resolve(),(args.color / "../../../include").resolve(),
                        args.gui / "src", args.gui / "build/install/include"]
            msvc = Path(args.cxx).name.lower() in ("cl", "cl.exe")
            compile_command = ([args.cxx, "/std:c++latest", "/Zs", "/utf-8"] if msvc else
                               [args.cxx, "-std=c++23", "-fsyntax-only"])
            for include in includes:
                compile_command.extend(["/I" if msvc else "-I", str(include)])
            compile_command.append(str(probe))
            compiled = subprocess.run(compile_command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if expected is None:
                if compiled.returncode:
                    raise RuntimeError(f"Valid generated callback failed: {compiled.stdout}")
            elif compiled.returncode == 0 or not any(message in compiled.stdout for message in (expected if isinstance(expected, tuple) else (expected,))):
                raise RuntimeError(f"Invalid generated callback was not rejected for {name}: {compiled.stdout}")
            result[name] = {"compiled": compiled.returncode == 0, "expected_rejection": expected}
    report = build / "lvui-completion/native-callable-validation.json"
    report.parent.mkdir(exist_ok=True)
    report.write_text(json.dumps(result, indent=2) + "\n")
    valid = sum(value[4] is None for value in cases.values())
    print(f"Generated native callables: {valid} valid, {len(cases)-valid} rejected; report {report}")


if __name__ == "__main__":
    main()
