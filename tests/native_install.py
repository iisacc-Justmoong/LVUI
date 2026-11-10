"""Test a relocated native consumer using resource inputs and isolated build outputs."""
from pathlib import Path
from contextlib import contextmanager
from package_workspace import cleanup_owned_temporary, validate_install_dirs
from workspace_support import checked_workspace
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile


@contextmanager
def native_build_directory(build):
    temporary = tempfile.TemporaryDirectory(prefix="native-consumer-build-", dir=build)
    try:
        yield temporary.name
    finally:
        cleanup_owned_temporary(temporary)


def install_paths(cache_text):
    validate_install_dirs(cache_text)
    values = dict(re.findall(r'^([^/#][^:\n]*):[^=\n]*=(.*)$', cache_text, re.M))
    return {name: Path(values.get(key) or default) for name, key, default in (
        ("lib", "CMAKE_INSTALL_LIBDIR", "lib"),
        ("include", "CMAKE_INSTALL_INCLUDEDIR", "include"),
        ("bin", "CMAKE_INSTALL_BINDIR", "bin"))}


def image_io_consumer_required(cache, installed):
    value = cache.get("LVUI_BUILD_IMAGE_IO", "OFF").upper()
    if value in {"ON", "TRUE", "YES", "Y", "1"}:
        enabled = True
    elif value in {"OFF", "FALSE", "NO", "N", "0", "", "IGNORE", "NOTFOUND"} or value.endswith("-NOTFOUND"):
        enabled = False
    else:
        raise ValueError("Invalid LVUI_BUILD_IMAGE_IO value")
    if enabled != installed:
        raise RuntimeError("The installed ImageIO component does not match the build configuration")
    return enabled


def image_io_consumer_options(cache):
    required = ("iiGUI_DIR", "iiFileProviderNative_DIR", "iiNetwork_DIR", "iiVisualCodec_DIR")
    for name in required:
        if not cache.get(name) or cache[name].endswith("-NOTFOUND"):
            raise ValueError("ImageIO consumer requires " + name)
    names = (*required, "iiJson_DIR", "iiFileProviderCoreIO_DIR", "Vulkan_INCLUDE_DIR", "Vulkan_LIBRARY",
             "OPENSSL_ROOT_DIR", "OPENSSL_INCLUDE_DIR", "OPENSSL_CRYPTO_LIBRARY", "OPENSSL_SSL_LIBRARY",
             "CURL_DIR", "CURL_INCLUDE_DIR", "CURL_LIBRARY_RELEASE", "CURL_LIBRARY_DEBUG",
             "JPEG_INCLUDE_DIR", "JPEG_LIBRARY_RELEASE", "JPEG_LIBRARY_DEBUG",
             "PNG_PNG_INCLUDE_DIR", "PNG_LIBRARY_RELEASE", "PNG_LIBRARY_DEBUG",
             "TIFF_INCLUDE_DIR", "TIFF_LIBRARY_RELEASE", "TIFF_LIBRARY_DEBUG", "WebP_DIR",
             "ZLIB_INCLUDE_DIR", "ZLIB_LIBRARY_RELEASE", "ZLIB_LIBRARY_DEBUG")
    return [f"-D{name}={cache[name]}" for name in names
            if cache.get(name) and not cache[name].endswith("-NOTFOUND")]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--core", type=Path, required=True)
    parser.add_argument("--color", type=Path, required=True)
    parser.add_argument("--cmake", required=True)
    args = parser.parse_args()
    build = args.build.resolve()
    root = checked_workspace(args.workspace)
    manifests = [build / "install_manifest.txt", build / "install_local_manifest.txt"]
    original_manifests = [path.read_bytes() if path.is_file() else None for path in manifests]
    cache_text = (build / "CMakeCache.txt").read_text()
    paths = install_paths(cache_text)
    cache = dict(re.findall(r'^([^/#][^:\n]*):[^=\n]*=(.*)$', cache_text, re.M))
    gui_package = cache.get("iiGUI_DIR", "")
    gui_options = [f"-D{name}={cache[name]}" for name in ("Vulkan_INCLUDE_DIR", "Vulkan_LIBRARY")
                   if cache.get(name) and not cache[name].endswith("-NOTFOUND")]
    consumer_options=[f'-D{name}={cache[name]}' for name in ('CMAKE_CXX_COMPILER','CMAKE_CXX_FLAGS','CMAKE_EXE_LINKER_FLAGS','CMAKE_OSX_DEPLOYMENT_TARGET') if cache.get(name)]
    # Do not accidentally satisfy a public dependency through the developer's
    # global SDK search paths. The consumer supplies its dependencies explicitly.
    environment = os.environ.copy()
    for name in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "LIBRARY_PATH", "CMAKE_PREFIX_PATH",
                 "DYLD_LIBRARY_PATH", "DYLD_FALLBACK_LIBRARY_PATH", "LD_LIBRARY_PATH"):
        environment.pop(name, None)
    isolated_packages = ["-DCMAKE_DISABLE_FIND_PACKAGE_LVRS=ON",
                         "-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON",
                         "-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON"]
    with native_build_directory(build) as temporary:
        outputs = Path(temporary)
        def run(*command):
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    env=environment)
            print(result.stdout, end="", flush=True)
            result.check_returncode()
            return result.stdout
        installed = outputs / "stage"
        # Keep the caller's build/install_manifest.txt unchanged during this
        # isolated consumer test, even when another install has already run.
        script = (build / "cmake_install.cmake").read_text(encoding="utf-8")
        original = f'{build.as_posix()}/${{CMAKE_INSTALL_MANIFEST}}'
        if original not in script:
            raise RuntimeError("Unable to isolate the generated install manifest")
        local_script = root / "cmake_install.cmake"
        local_script.write_text(script.replace(original, f'{root.as_posix()}/${{CMAKE_INSTALL_MANIFEST}}'), encoding="utf-8")
        run(args.cmake, f"-DCMAKE_INSTALL_PREFIX={installed}", "-P", str(local_script))
        relocated = outputs / "relocated"
        installed.rename(relocated)
        profile = cache.get("LVUI_CORE_PROFILE_HINT", "")
        if profile:
            profile_path = outputs / "iiSDKCore" / profile
            profile_path.parent.mkdir(parents=True, exist_ok=True)
            profile_path.symlink_to(args.core.resolve().parents[2], target_is_directory=True)
        package_dir = relocated / paths["lib"] / "cmake/LVUI"
        builtin_icons = (relocated / paths["include"] / "LVUI/BuiltinIconAssets.h").read_text(encoding="utf-8")
        symbol_entry = re.search(r'"sliderSymbolSun", R"(?P<delimiter>LVUI\d+)\((?P<svg>.*?)\)(?P=delimiter)"',
                                 builtin_icons, re.DOTALL)
        source_root = Path(__file__).resolve().parents[1]
        sys.path.insert(0, str(source_root / "tools"))
        from embed_icons import resolve_svg_tokens
        raw_symbol = (source_root / "resources/icons/sliderSymbolSun.svg").read_text(encoding="utf-8")
        token_catalog = json.loads((source_root / "catalog/lvui-style-tokens.json").read_text(encoding="utf-8"))
        expected_symbol = resolve_svg_tokens(raw_symbol, token_catalog)
        if not symbol_entry or symbol_entry.group("svg") != expected_symbol:
            raise RuntimeError("The relocated LVUI package changed or omitted its Figma slider symbol")
        cli_source = root / "Portable.lvui"
        cli_source.write_text('Window { Label(text: "Portable") }\n')
        suffix = ".exe" if sys.platform == "win32" else ""
        checked = run(str(relocated / paths["bin"] / ("lvui-check" + suffix)), str(cli_source))
        compiled = run(str(relocated / paths["bin"] / ("lvui-compile" + suffix)), "--name", "Portable", str(cli_source))
        if "syntax valid" not in checked or "struct Portable final" not in compiled:
            raise RuntimeError("Relocated LVUI command-line tools did not parse and generate the source")
        outdated=root/'outdated-core';(outdated/'include').mkdir(parents=True)
        (outdated/'iiSDKCoreConfig.cmake').write_text('add_library(iiSDKCore::Contracts INTERFACE IMPORTED)\nset(iiSDKCore_Contracts_FOUND TRUE)\nset(iiSDKCore_FOUND TRUE)\n')
        (outdated/'iiSDKCoreConfigVersion.cmake').write_text('set(PACKAGE_VERSION "0.20.0")\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\n')
        outdated_project=root/'outdated-project';outdated_project.mkdir()
        (outdated_project/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.31)\nproject(OutdatedCore LANGUAGES CXX)\nfind_package(LVUI CONFIG REQUIRED COMPONENTS Native)\n')
        rejected=subprocess.run([args.cmake,'-S',str(outdated_project),'-B',str(outputs/'outdated-project'/'build'),'-G','Ninja',f'-DLVUI_DIR={package_dir}',f'-DiiSDKCore_DIR={outdated}',*isolated_packages],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=environment)
        if not rejected.returncode or 'LVUI_CORE_OBSERVATION_REQUIRED' not in rejected.stdout:raise RuntimeError('An outdated Core package bypassed the native observation capability check: '+rejected.stdout)
        project = root / "project"
        shutil.copytree(Path(__file__).parent / "native_consumer", project)
        target = outputs / "consumer" / "build"
        gui_consumer_options = (["-DLVUI_VERIFY_GUI_COMPONENTS=ON", f"-DiiGUI_DIR={gui_package}", *gui_options]
                                if gui_package and Path(gui_package).is_dir() else [])
        run(args.cmake, "-S", str(project), "-B", str(target), "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release", f"-DLVUI_DIR={package_dir}",
            f"-DiiSDKCore_DIR={args.core}", f"-DiiColorManager_DIR={args.color}",*gui_consumer_options,*consumer_options,*isolated_packages)
        consumer_cache = (target / "CMakeCache.txt").read_text()
        for result in ("LVUI_INVALID_VOID_CALLBACK_ACCEPTED", "LVUI_INVALID_SCALAR_CALLBACK_ACCEPTED",
                       "LVUI_INVALID_TYPED_CALLBACK_RETURN_ACCEPTED", "LVUI_INVALID_TYPED_CALLBACK_ARITY_ACCEPTED",
                       "LVUI_INVALID_TYPED_CALLBACK_TYPE_ACCEPTED", "LVUI_INVALID_FUNCTION_BOOL_RESULT_ACCEPTED",
                       "LVUI_INVALID_COPIED_COMPONENT_STORAGE_ACCEPTED", "LVUI_INVALID_ESCAPED_COMPONENT_INPUT_ACCEPTED"):
            if f"{result}:INTERNAL=\n" not in consumer_cache:
                raise RuntimeError(f"Invalid returned callable was not rejected: {result}")
        run(args.cmake, "--build", str(target), "-j", "2")
        run("ctest", "--test-dir", str(target), "--output-on-failure")
        query_project = root / "query-project"
        shutil.copytree(Path(__file__).parent / "native_query_install_consumer", query_project)
        query_target = outputs / "query-consumer" / "build"
        query_core = [] if profile else [f"-DiiSDKCore_DIR={args.core}"]
        run(args.cmake, "-S", str(query_project), "-B", str(query_target), "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release", f"-DLVUI_DIR={package_dir}",
            *query_core, f"-DiiColorManager_DIR={args.color}", *consumer_options, *isolated_packages)
        run(args.cmake, "--build", str(query_target), "-j", "2")
        run("ctest", "--test-dir", str(query_target), "--output-on-failure")
        if cache.get("LVUI_BUILD_LOCALIZATION_IO", "OFF").upper() in {"ON", "TRUE", "YES", "Y", "1"}:
            localization_project = root / "localization-project"
            shutil.copytree(Path(__file__).parent / "native_localization_consumer", localization_project)
            localization_target = outputs / "localization-consumer" / "build"
            localization_options = ["-DCMAKE_DISABLE_FIND_PACKAGE_iiJson=ON",
                                    "-DCMAKE_DISABLE_FIND_PACKAGE_iiFileProviderNative=ON"]
            run(args.cmake, "-S", str(localization_project), "-B", str(localization_target), "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", f"-DLVUI_DIR={package_dir}",
                f"-DiiSDKCore_DIR={args.core}", f"-DiiColorManager_DIR={args.color}",
                *localization_options, *consumer_options, *isolated_packages)
            run(args.cmake, "--build", str(localization_target), "-j", "2")
            run("ctest", "--test-dir", str(localization_target), "--output-on-failure")
        if cache.get("LVUI_BUILD_COLOR_ADAPTER", "OFF").upper() in {"ON", "TRUE", "YES", "Y", "1"}:
            color_project = root / "color-contract-project"
            shutil.copytree(Path(__file__).parent / "native_color_contract_consumer", color_project)
            color_target = outputs / "color-contract-consumer" / "build"
            run(args.cmake, "-S", str(color_project), "-B", str(color_target), "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", f"-DLVUI_DIR={package_dir}",
                f"-DiiSDKCore_DIR={args.core}", "-DCMAKE_DISABLE_FIND_PACKAGE_iiColorManager=ON",
                *consumer_options, *isolated_packages)
            run(args.cmake, "--build", str(color_target), "-j", "2")
            run("ctest", "--test-dir", str(color_target), "--output-on-failure")
        menu_consumer_verified = False
        application_consumer_verified = False
        if gui_package and Path(gui_package).is_dir():
            menu_project = root / "menu-project"
            shutil.copytree(Path(__file__).parent / "native_menu_install_consumer", menu_project)
            for name in ("NativeMenuBundles.lvui", "NativeMenuNestedComposition.lvui", "NativeMenuBundlesFixture.h", "NativeTableStructureControls.lvui", "NativeOwnedTableFixture.h", "NativeTabDynamic.lvui", "NativeTabDynamicFixture.h", "NativeOwnedSearchField.lvui", "NativeOwnedSearchFieldFixture.h", "NativeOwnedSlider.lvui", "NativeOwnedSliderFixture.h", "NativeFocusRing.lvui", "NativeFocusRingFixture.h", "NativeAbstractInputBar.lvui", "NativeAbstractInputBarFixture.h", "NativeExplicitListItemComposite.lvui", "NativeMobileTabs.lvui", "NativeMobileTabsFixture.h", "NativeWindowNoise.lvui", "NativeWindowNoiseFixture.h", "NativeWheelGuard.lvui", "NativeWheelGuardFixture.h"):
                shutil.copy2(Path(__file__).parent / "fixtures" / name, menu_project / name)
            menu_target = outputs / "menu-consumer" / "build"
            run(args.cmake, "-S", str(menu_project), "-B", str(menu_target), "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", f"-DLVUI_DIR={package_dir}",
                f"-DiiSDKCore_DIR={args.core}", f"-DiiColorManager_DIR={args.color}",
                f"-DiiGUI_DIR={gui_package}", *consumer_options, *gui_options, *isolated_packages)
            run(args.cmake, "--build", str(menu_target), "-j", "2")
            menu_consumer_verified = True
            app_project = root / "application-project"
            shutil.copytree(Path(__file__).parent / "native_application_consumer", app_project)
            for name in ("NativeApplication.lvui", "NativeApplicationMarkup.lvui", "NativeApplicationForeground.lvui", "NativeApplicationForegroundMarkup.lvui", "NativeForegroundBackend.h", "NativeLaunchApplication.lvui", "NativeLaunchMarkup.lvui", "NativeLaunchScenes.lvui", "NativeApplicationClose.lvui", "NativeApplicationCloseMarkup.lvui", "NativeApplicationCloseScenes.lvui", "NativeApplicationQuitOnly.lvui", "NativeMultiWindow.lvui", "NativeMultiWindowMarkup.lvui", "NativeMixedScenes.lvui", "NativeNamedWindows.lvui", "NativeNamedWindowsMarkup.lvui", "NativeScenes.lvui", "NativeDynamicWindows.lvui", "NativeDynamicWindowsMarkup.lvui", "NativeDynamicWindowsBackend.h", "NativeWindowClose.lvui", "NativeSingleWindowClose.lvui", "NativeApplicationBackend.h", "WindowCloseFixtureBackend.h"):
                shutil.copy2(Path(__file__).parent / "fixtures" / name, app_project / name)
            app_options = [f"-DLVUI_DIR={package_dir}",
                           f"-DiiSDKCore_DIR={args.core}", f"-DiiColorManager_DIR={args.color}",
                           f"-DiiGUI_DIR={gui_package}", *consumer_options, *gui_options, *isolated_packages]
            if cache.get("LVUI_BUILD_LOCALIZATION_IO", "OFF").upper() in {"ON", "TRUE", "YES", "Y", "1"}:
                example = source_root / "example/GettingStarted"
                shutil.copy2(example / "LocalizedApplication.lvui", app_project)
                shutil.copy2(example / "App/LocalizedApplication.cpp", app_project)
                shutil.copy2(example / "App/StringCatalogAdapters.h", app_project)
                application_adapters = [f"-D{name}={cache[name]}" for name in
                    ("iiJson_DIR", "iiFileProviderNative_DIR")
                    if cache.get(name) and not cache[name].endswith("-NOTFOUND")]
                app_options.extend(["-DLOCALIZED_APPLICATION=ON", *application_adapters])
            invalid = subprocess.run([args.cmake, "-S", str(app_project), "-B",
                                      str(outputs / "invalid-application" / "build"), "-G", "Ninja",
                                      "-DBAD_APPLICATION_TARGET=ON", *app_options],
                                     text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=environment)
            if not invalid.returncode or "requires an identifier target" not in invalid.stdout:
                raise RuntimeError("Invalid application target was not rejected: " + invalid.stdout)
            invalid_source = subprocess.run([args.cmake, "-S", str(app_project), "-B",
                                            str(outputs / "invalid-configure-source" / "build"), "-G", "Ninja",
                                            "-DBAD_CONFIGURE_SOURCE=ON", *app_options],
                                           text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=environment)
            if not invalid_source.returncode or "CONFIGURE_SOURCE must name an existing source file" not in invalid_source.stdout:
                raise RuntimeError("Invalid application configure source was not rejected: " + invalid_source.stdout)
            app_target = outputs / "application-consumer" / "build"
            run(args.cmake, "-S", str(app_project), "-B", str(app_target), "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", *app_options)
            run(args.cmake, "--build", str(app_target), "-j", "2")
            application_consumer_verified = True
        runtime_verified = False
        if (package_dir / "LVUIRuntimeTargets.cmake").exists():
            runtime_project = root / "runtime-project"
            shutil.copytree(Path(__file__).parent / "runtime_consumer", runtime_project)
            runtime_target = outputs / "runtime-consumer" / "build"
            run(args.cmake, "-S", str(runtime_project), "-B", str(runtime_target), "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", f"-DLVUI_DIR={package_dir}",
                f"-DiiSDKCore_DIR={args.core}", f"-DiiColorManager_DIR={args.color}",*consumer_options,*isolated_packages)
            run(args.cmake, "--build", str(runtime_target), "-j", "2")
            run("ctest", "--test-dir", str(runtime_target), "--output-on-failure")
            runtime_verified = True
        image_verified = False
        image_export = package_dir / "LVUIImageIOTargets.cmake"
        if image_io_consumer_required(cache, image_export.is_file()):
            image_contract_project = root / "image-contract-project"
            shutil.copytree(Path(__file__).parent / "native_imageio_contract_consumer", image_contract_project)
            image_contract_target = outputs / "image-contract-consumer" / "build"
            peer_free = [f"-DCMAKE_DISABLE_FIND_PACKAGE_{name}=ON" for name in
                         ("iiGUI", "iiNetwork", "iiFileProviderNative", "iiVisualCodec")]
            run(args.cmake, "-S", str(image_contract_project), "-B", str(image_contract_target), "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", f"-DLVUI_DIR={package_dir}",
                f"-DiiSDKCore_DIR={args.core}", *peer_free, *consumer_options, *isolated_packages)
            run(args.cmake, "--build", str(image_contract_target), "-j", "2")
            run("ctest", "--test-dir", str(image_contract_target), "--output-on-failure")
            image_project = root / "image-project"
            shutil.copytree(Path(__file__).parent / "native_image_install_consumer", image_project)
            for name in ("NativeImageLoader.cpp", "NativeImageSource.cpp", "NativeImageAsync.h", "NativeImageLoader.h"):
                shutil.copy2(source_root / "example/Integrations/App" / name, image_project)
            image_target = outputs / "image-consumer" / "build"
            run(args.cmake, "-S", str(image_project), "-B", str(image_target), "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", f"-DLVUI_DIR={package_dir}",
                f"-DiiSDKCore_DIR={args.core}", f"-DiiColorManager_DIR={args.color}",
                *image_io_consumer_options(cache), *consumer_options, *isolated_packages)
            run(args.cmake, "--build", str(image_target), "-j", "2")
            run("ctest", "--test-dir", str(image_target), "--output-on-failure")
            image_verified = True
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
        report["relocated_variable_hierarchy_and_view_revision_verified"] = bool(gui_consumer_options)
        report["relocated_table_merge_contract_and_codegen_verified"] = True
        report["relocated_menu_sequence_codegen_verified"] = True
        report["relocated_conditional_and_repeated_menu_bundles_codegen_verified"] = True
        report["relocated_menu_bundles_consumer_compiled_linked"] = menu_consumer_verified
        report["relocated_nested_menu_composition_consumer_compiled_linked"] = menu_consumer_verified
        report["relocated_search_field_consumer_compiled_linked"] = menu_consumer_verified
        report["relocated_slider_decorations_consumer_compiled_linked"] = menu_consumer_verified
        report["relocated_slider_symbol_embedded"] = True
        report["relocated_declarative_application_compiled_linked_not_executed"] = application_consumer_verified
        report["relocated_foreground_application_compiled_linked_not_executed"] = application_consumer_verified
        report["relocated_dynamic_window_application_compiled_linked_not_executed"] = application_consumer_verified
        report["relocated_table_structure_controls_consumer_compiled_linked"] = menu_consumer_verified
        report["relocated_initial_table_structure_actions_compiled_linked"] = menu_consumer_verified
        report["outdated_core_without_scoped_observation_rejected"] = True
        report["optional_runtime_consumer_compiled_linked_executed"] = runtime_verified
        report["optional_image_io_consumer_compiled_linked_executed"] = image_verified
        if [path.read_bytes() if path.is_file() else None for path in manifests] != original_manifests:
            raise RuntimeError("The existing install manifests changed during consumer verification")
        print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
