"""Install and relocate a native-only C++ consumer entirely under build/."""
from pathlib import Path
import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--core", type=Path, required=True)
    parser.add_argument("--color", type=Path, required=True)
    parser.add_argument("--cmake", required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    cache=dict(re.findall(r'^([^/#][^:\n]*):[^=\n]*=(.*)$',(build/'CMakeCache.txt').read_text(),re.M))
    consumer_options=[f'-D{name}={cache[name]}' for name in ('CMAKE_CXX_COMPILER','CMAKE_CXX_FLAGS','CMAKE_EXE_LINKER_FLAGS','CMAKE_OSX_DEPLOYMENT_TARGET') if cache.get(name)]
    evidence = build / "lvui-completion"
    evidence.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="native-consumer-", dir=build) as temporary:
        root = Path(temporary)
        def run(*command):
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            print(result.stdout, end="", flush=True)
            result.check_returncode()
            return result.stdout
        installed = root / "stage"
        run(args.cmake, "--install", str(build), "--prefix", str(installed))
        relocated = root / "relocated"
        installed.rename(relocated)
        cli_source = root / "Portable.lvui"
        cli_source.write_text('Window { Label(text: "Portable") }\n')
        suffix = ".exe" if sys.platform == "win32" else ""
        checked = run(str(relocated / ("bin/lvui-check" + suffix)), str(cli_source))
        compiled = run(str(relocated / ("bin/lvui-compile" + suffix)), "--name", "Portable", str(cli_source))
        if "syntax valid" not in checked or "struct Portable final" not in compiled:
            raise RuntimeError("Relocated LVUI command-line tools did not parse and generate the source")
        outdated=root/'outdated-core';(outdated/'include').mkdir(parents=True)
        (outdated/'iiSDKCoreConfig.cmake').write_text('add_library(iiSDKCore::Contracts INTERFACE IMPORTED)\nset(iiSDKCore_Contracts_FOUND TRUE)\nset(iiSDKCore_FOUND TRUE)\n')
        (outdated/'iiSDKCoreConfigVersion.cmake').write_text('set(PACKAGE_VERSION "0.20.0")\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\n')
        outdated_project=root/'outdated-project';outdated_project.mkdir()
        (outdated_project/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.31)\nproject(OutdatedCore LANGUAGES CXX)\nfind_package(LVUI CONFIG REQUIRED COMPONENTS Native)\n')
        rejected=subprocess.run([args.cmake,'-S',str(outdated_project),'-B',str(outdated_project/'build'),'-G','Ninja',f'-DLVUI_DIR={relocated / "lib/cmake/LVUI"}',f'-DiiSDKCore_DIR={outdated}'],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        if not rejected.returncode or 'LVUI_CORE_OBSERVATION_REQUIRED' not in rejected.stdout:raise RuntimeError('An outdated Core package bypassed the native observation capability check: '+rejected.stdout)
        project = root / "project"
        shutil.copytree(Path(__file__).parent / "native_consumer", project)
        target = project / "build"
        run(args.cmake, "-S", str(project), "-B", str(target), "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release", f"-DLVUI_DIR={relocated / 'lib/cmake/LVUI'}",
            f"-DiiSDKCore_DIR={args.core}", f"-DiiColorManager_DIR={args.color}",*consumer_options)
        cache = (target / "CMakeCache.txt").read_text()
        for result in ("LVUI_INVALID_VOID_CALLBACK_ACCEPTED", "LVUI_INVALID_SCALAR_CALLBACK_ACCEPTED",
                       "LVUI_INVALID_TYPED_CALLBACK_RETURN_ACCEPTED", "LVUI_INVALID_TYPED_CALLBACK_ARITY_ACCEPTED",
                       "LVUI_INVALID_TYPED_CALLBACK_TYPE_ACCEPTED", "LVUI_INVALID_FUNCTION_BOOL_RESULT_ACCEPTED",
                       "LVUI_INVALID_COPIED_COMPONENT_STORAGE_ACCEPTED", "LVUI_INVALID_ESCAPED_COMPONENT_INPUT_ACCEPTED"):
            if f"{result}:INTERNAL=\n" not in cache:
                raise RuntimeError(f"Invalid returned callable was not rejected: {result}")
        run(args.cmake, "--build", str(target), "-j", "2")
        run("ctest", "--test-dir", str(target), "--output-on-failure")
        runtime_verified = False
        if (relocated / "lib/cmake/LVUI/LVUIRuntimeTargets.cmake").exists():
            runtime_project = root / "runtime-project"
            shutil.copytree(Path(__file__).parent / "runtime_consumer", runtime_project)
            runtime_target = runtime_project / "build"
            run(args.cmake, "-S", str(runtime_project), "-B", str(runtime_target), "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", f"-DLVUI_DIR={relocated / 'lib/cmake/LVUI'}",
                f"-DiiSDKCore_DIR={args.core}", f"-DiiColorManager_DIR={args.color}",*consumer_options)
            run(args.cmake, "--build", str(runtime_target), "-j", "2")
            run("ctest", "--test-dir", str(runtime_target), "--output-on-failure")
            runtime_verified = True
        report = {"target": "LVUI::Native", "installed": True, "relocated": True,
                  "consumer_compiled_linked_executed": True,
                  "component_compiler_consumer_compiled_linked_executed": True, "ui_host_executed": False}
        report["core_package_dir"] = str(args.core.resolve())
        report["observation_execution_component_required"] = True
        report["relocated_check_and_compile_tools_executed"] = True
        report["invalid_void_and_scalar_callback_results_rejected_by_cpp"] = True
        report["typed_callback_inputs_compiled_and_invalid_contracts_rejected"] = True
        report["shared_callback_values_compiled_linked_executed"] = True
        report["native_function_values_and_result_contracts_verified"] = True
        report["static_function_version_snapshots_verified"] = True
        report["mutable_function_variables_verified"] = True
        report["view_sequence_slots_verified"] = True
        report["native_color_values_and_inputs_verified"] = True
        report["shared_scalar_component_inputs_and_callable_defaults_verified"] = True
        report["native_layout_values_and_aot_verified"] = True
        report["native_window_values_and_aot_verified"] = True
        report["readonly_observation_and_noncomparable_values_verified"] = True
        report["native_object_input_values_ownership_and_null_validation_executed"] = True
        report["native_method_private_observable_reads_executed"] = True
        report["native_object_method_reference_selection_and_retirement_executed"] = True
        report["native_callable_field_versions_and_retirement_executed"] = True
        report["native_factory_receiver_and_argument_capture_executed"] = True
        report["native_inferred_values_and_function_versions_executed"] = True
        report["outdated_core_without_scoped_observation_rejected"] = True
        report["optional_runtime_consumer_compiled_linked_executed"] = runtime_verified
        (evidence / "native-installed-consumer.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
