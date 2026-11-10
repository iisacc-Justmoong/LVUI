"""The installed LVUI catalog contains only LVUI-owned public data."""
import os
from contextlib import contextmanager
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from package_workspace import checked_paths, cleanup_owned_temporary, redirect_manifest, validate_install_dirs
from workspace_support import provider


@contextmanager
def package_temporary_directory(workspace):
    temporary = provider.TemporaryDirectory(prefix="lvui-package-boundary-", dir=workspace)
    try:
        yield temporary.name
    finally:
        cleanup_owned_temporary(temporary)


def main():
    source = Path(__file__).resolve().parents[1]
    build, workspace = checked_paths(source, sys.argv[1], sys.argv[2])
    validate_install_dirs((build / "CMakeCache.txt").read_text(encoding="utf-8"))
    manifests = [build / "install_manifest.txt", build / "install_local_manifest.txt"]
    previous = [p.read_bytes() if p.is_file() else None for p in manifests]
    with package_temporary_directory(workspace) as temporary:
        prefix = Path(temporary)
        # Preserve the build's existing install manifest: run the same generated
        # install rules with only their manifest destination redirected here.
        script = (build / "cmake_install.cmake").read_text(encoding="utf-8")
        script = redirect_manifest(script, build, prefix)
        local_script = prefix / "cmake_install.cmake"
        provider.write_text(local_script, script, encoding="utf-8")
        subprocess.run([os.environ.get("CMAKE_COMMAND", "cmake"), f"-DCMAKE_INSTALL_PREFIX={prefix}", "-P", str(local_script)],
                       check=True, stdout=subprocess.DEVNULL)
        if [p.read_bytes() if p.is_file() else None for p in manifests] != previous:
            raise AssertionError("The existing build install manifests were modified")
        ios_manifests = list(prefix.glob("**/cmake/LVUI/ios/LVUIInfo.plist.in"))
        if (len(ios_manifests) != 1 or ios_manifests[0].read_bytes() !=
                (source / "resources/platform/ios/LVUIInfo.plist.in").read_bytes()):
            raise AssertionError("Installed iOS scene bundle template is missing or stale")
        catalog = prefix / "share" / "LVUI" / "catalog"
        names = {entry.name for entry in catalog.iterdir()}
        expected = {"lvui-keywords.json", "lvui-style-tokens.json"}
        if names != expected:
            raise AssertionError(f"Unexpected installed catalog: {sorted(names)}")
        for name in expected:
            if (catalog / name).read_bytes() != (source / "catalog" / name).read_bytes():
                raise AssertionError(f"Installed catalog differs from the LVUI source: {name}")
        headers = list((prefix / "include" / "LVUI").rglob("*.h"))
        if not headers:
            raise AssertionError("No LVUI public headers were installed")
        for name in ("InputField.h", "NativeInputField.h",
                     "NativeTextFieldMaterial.h"):
            installed = prefix / "include" / "LVUI" / name
            original = source / "src" / "Editing" / name
            if not installed.is_file() or installed.read_bytes() != original.read_bytes():
                raise AssertionError(f"Installed input field header is missing or differs: {name}")
        for name in ("AuthoredIcons.h", "NativeAuthoredIcons.h", "NativeIconCache.h"):
            installed = prefix / "include" / "LVUI" / name
            original = source / "src" / "Media" / name
            if not installed.is_file() or installed.read_bytes() != original.read_bytes():
                raise AssertionError(f"Installed media header is missing or differs: {name}")
        for name in ("NativeAuthoredIcons.h", "NativeIconCache.h", "NativeCheckBox.h", "NativeToggleSwitch.h", "NativeRadioButton.h"):
            asset_adapter = (prefix / "include/LVUI" / name).read_text(encoding="utf-8")
            if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', asset_adapter, re.M) or "ii::gui::" in asset_adapter:
                raise AssertionError(f"Installed icon asset contract depends directly on another SDK: {name}")
        avatar_assets = (prefix / "include/LVUI/NativeAvatarAssets.h").read_text(encoding="utf-8")
        if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', avatar_assets, re.M) or "ii::gui::" in avatar_assets:
            raise AssertionError("Installed Avatar asset contract depends directly on another SDK")
        for name in ("NativeAvatarAssets.h", "Button.h", "NativeButton.h", "NativeCheckBox.h", "NativeToggleSwitch.h", "NativeRadioButton.h", "Slider.h", "NativeSlider.h"):
            installed = prefix / "include" / "LVUI" / name
            original = source / "src" / "Controls" / name
            if not installed.is_file() or installed.read_bytes() != original.read_bytes():
                raise AssertionError(f"Installed control header is missing or differs: {name}")
        for name in ("Image.h", "NativeImage.h", "Avatar.h", "NativeAvatar.h", "Sheet.h", "NativeSheet.h", "NativeInputFieldClear.h", "NativeTextLayout.h", "NativeSearchFieldMaterial.h", "RadioGroup.h", "NativeAppCard.h", "AppHeader.h", "NativeAppHeader.h", "EventListener.h", "NativeEventListener.h", "NativeIcon.h"):
            if (prefix / "include/LVUI" / name).exists():
                raise AssertionError(f"Migrated visual implementation leaked into LVUI: {name}")
        for name, directory, forbidden in (("ToolbarButton.h", "Controls", "class ToolbarButton final"),
                                           ("HierarchyToolbarLayout.h", "Collections", "hierarchy_toolbar_geometry("),
                                           ("Stepper.h", "Controls", "class Stepper final"),
                                           ("ToggleSwitch.h", "Controls", "class ToggleSwitch final"),
                                           ("CheckBox.h", "Controls", "class CheckBox final"),
                                           ("RadioButton.h", "Controls", "class RadioButton final"),
                                           ("Button.h", "Controls", "class Button final"),
                                           ("SegmentedControl.h", "Controls", "class SegmentedControl final"),
                                           ("DropdownButton.h", "Controls", "class DropdownButton final"),
                                           ("Material.h", "Style", "class Material final")):
            path = prefix / "include/LVUI" / name
            source_text = path.read_text(encoding="utf-8")
            if path.read_bytes() != (source / "src" / directory / name).read_bytes() or forbidden in source_text:
                raise AssertionError(f"Installed value contract contains a visual model or is stale: {name}")
            if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', source_text, re.M) or "ii::gui::" in source_text:
                raise AssertionError(f"Installed value contract depends directly on another SDK: {name}")
        alert_values = prefix / "include/LVUI/Alert.h"
        alert_text = alert_values.read_text(encoding="utf-8")
        if alert_values.read_bytes() != (source / "src/Overlays/Alert.h").read_bytes() or "class Alert final" in alert_text:
            raise AssertionError("Installed alert values include a visual model or are stale")
        if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', alert_text, re.M) or "ii::gui::" in alert_text:
            raise AssertionError("Installed alert values depend directly on another SDK")
        modal_values = prefix / "include/LVUI/Modal.h"
        modal_text = modal_values.read_text(encoding="utf-8")
        if modal_values.read_bytes() != (source / "src/Overlays/Modal.h").read_bytes() or "class Modal final" in modal_text:
            raise AssertionError("Installed modal values include a visual model or are stale")
        if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', modal_text, re.M) or "ii::gui::" in modal_text:
            raise AssertionError("Installed modal values depend directly on another SDK")
        list_item_values = prefix / "include/LVUI/ListItem.h"
        list_item_text = list_item_values.read_text(encoding="utf-8")
        if list_item_values.read_bytes() != (source / "src/Collections/ListItem.h").read_bytes() or "class ListItem final" in list_item_text:
            raise AssertionError("Installed list-item values include a visual model or are stale")
        if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', list_item_text, re.M) or "ii::gui::" in list_item_text:
            raise AssertionError("Installed list-item values depend directly on another SDK")
        footer_values = prefix / "include/LVUI/ListFooter.h"
        footer_text = footer_values.read_text(encoding="utf-8")
        if footer_values.read_bytes() != (source / "src/Collections/ListFooter.h").read_bytes() or "class ListFooter final" in footer_text:
            raise AssertionError("Installed list-footer values include a visual model or are stale")
        if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', footer_text, re.M) or "ii::gui::" in footer_text:
            raise AssertionError("Installed list-footer values depend directly on another SDK")
        toolbar_values = prefix / "include/LVUI/ListToolbar.h"
        toolbar_text = toolbar_values.read_text(encoding="utf-8")
        if toolbar_values.read_bytes() != (source / "src/Collections/ListToolbar.h").read_bytes() or "class ListToolbar final" in toolbar_text:
            raise AssertionError("Installed list-toolbar values include a visual model or are stale")
        if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', toolbar_text, re.M) or "ii::gui::" in toolbar_text:
            raise AssertionError("Installed list-toolbar values depend directly on another SDK")
        alert_values = prefix / "include/LVUI/AlertButton.h"
        alert_text = alert_values.read_text(encoding="utf-8")
        if alert_values.read_bytes() != (source / "src/Overlays/AlertButton.h").read_bytes() or "class AlertButton final" in alert_text:
            raise AssertionError("Installed AlertButton values include a visual model or are stale")
        if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', alert_text, re.M) or "ii::gui::" in alert_text:
            raise AssertionError("Installed AlertButton values depend directly on another SDK")
        sheet_assets = prefix / "include/LVUI/NativeSheetAssets.h"
        if sheet_assets.exists():
            raise AssertionError("Installed LVUI package still contains a sheet graphical adapter")
        for name in ("NativePopupOpen.h", "NativeStylePort.h", "NativeInputEvents.h", "NativeSection.h", "NativeWidgetColors.h", "NativeTextBoundary.h", "NativeWidgetLayout.h", "NativeVisual.h", "NativeTextFieldMaterial.h", "NativeListItemDrawing.h", "NativeListItem.h", "NativeTableViewGroup.h", "NativeTableHeaderPresentation.h", "NativeSegmentedControl.h", "NativeListToolbar.h", "NativeToolbarButton.h", "NativeListFooter.h", "NativeButtonMethods.h", "NativeButton.h", "NativeStepper.h", "NativeDropdownButton.h", "NativeAlertButton.h", "NativeAlert.h", "NativeModal.h", "NativeMenuItem.h", "NativeMenu.h", "NativeMaterialGradient.h", "NativeMaterial.h", "NativePopover.h", "NativePageRouter.h", "NativeColorPickerButton.h", "NativeComboBox.h", "NativeLink.h", "NativeEditorTimeline.h", "NativeDocument.h", "NativeCollectionWidgets.h", "NativeColorSurface.h", "NativeTab.h", "NativeTabBar.h", "NativeCard.h", "ListItemSegments.h", "NativeListItemSegments.h", "ListItemSelector.h", "NativeListItemSelector.h", "ColorChannelSurface.h", "TextInput.h", "NativeInputField.h", "TextEditor.h", "NativeTextEditor.h", "Slider.h", "NativeSlider.h", "NativeSearchField.h", "NativeTableCellItem.h", "TableCellItem.h", "TableRowView.h", "NativeTableRow.h", "NativeColorPickerValues.h", "NativeColorChannel.h", "ColorPickerLayout.h", "NativeColorPicker.h", "MobileNavigation.h", "MobileNavigationMotion.h", "NativeMobileNavigation.h", "NativeHierarchyItem.h", "HierarchyItem.h", "HierarchyItemProjection.h", "HierarchyToolbar.h", "NativeHierarchyToolbar.h", "NativeTableHeader.h", "TableHeader.h", "NativeHierarchyList.h", "PanelRegions.h", "HierarchyPanel.h", "ListPanel.h", "NativeHierarchyPanel.h", "NativeListPanel.h", "ListViewValues.h", "NativeListView.h", "NativeCodeEditor.h", "Tooltip.h", "TooltipController.h", "TooltipOutline.h", "NativeTooltipPresentation.h", "NativeTooltipTarget.h", "NativeTooltipScheduler.h", "NativeTooltip.h", "NativeTable.h", "NativeListItemComposite.h", "ListItemDetailActions.h", "ListItemDetailControls.h", "ListItemTask.h", "ListItemPreviewRow.h", "ListItemForm.h", "ListItemQuantity.h", "ListViewport.h", "ListDrag.h", "HierarchyViewport.h", "HierarchyDrag.h", "TableViewport.h", "ListItemMenu.h", "NativeCollectionNavigation.h", "NativeVirtualCollection.h"):
            contract = (prefix / "include/LVUI" / name).read_text(encoding="utf-8")
            if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', contract, re.M) or "ii::gui::" in contract:
                raise AssertionError(f"Installed binding contract depends directly on a graphics SDK: {name}")
        for name, forbidden in (("NativeVirtualCollection.h", "virtual_collection_frame("),
                                ("NativeMeasuredCollection.h", "class VirtualMeasurements"),
                                ("NativeLineIndex.h", "class VirtualLineIndex"),
                                ("CodeViewport.h", "code_viewport(")):
            contract = (prefix / "include/LVUI" / name).read_text(encoding="utf-8")
            if forbidden in contract or re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', contract, re.M) or "ii::gui::" in contract:
                raise AssertionError(f"Installed LVUI header retains graphics implementation or dependency: {name}")
        mobile_tab = (prefix / "include/LVUI/NativeMobileTab.h").read_text(encoding="utf-8")
        if "MobileTabBinding" not in mobile_tab or re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', mobile_tab, re.M) or "ii::gui::" in mobile_tab:
            raise AssertionError("Installed mobile-tab contract depends directly on a graphics SDK")
        retired_headers = ("NativeCompiledView.h", "NativeInputFieldClear.h", "NativeTextLayout.h", "NativeSearchFieldMaterial.h", "RadioGroup.h")
        for header in (source / "src").rglob("*.h"):
            content = header.read_text(encoding="utf-8")
            for retired in retired_headers:
                if f"#include <LVUI/{retired}>" in content:
                    raise AssertionError(f"Retired graphics header is still referenced: {header}: {retired}")
        compiled_view = prefix / "include/LVUI/NativeCompiledView.h"
        if compiled_view.exists():
            raise AssertionError("Native view host implementation leaked into LVUI")
        for name in ("NativeInputEvents.h",):
            installed = prefix / "include" / "LVUI" / name
            original = source / "src" / "Runtime" / name
            if not installed.is_file() or installed.read_bytes() != original.read_bytes():
                raise AssertionError(f"Installed input contract header is missing or stale: {name}")
        commands = prefix / "include/LVUI/ApplicationCommands.h"
        if not commands.is_file() or commands.read_bytes() != (source / "src/Runtime/ApplicationCommands.h").read_bytes():
            raise AssertionError("Installed application commands header is missing or stale")
        scenes = prefix / "include/LVUI/SceneWindowSet.h"
        if not scenes.is_file() or scenes.read_bytes() != (source / "src/Runtime/SceneWindowSet.h").read_bytes():
            raise AssertionError("Installed independent scene lifecycle header is missing or stale")
        scene_descriptor = prefix / "include/LVUI/Scene.h"
        if not scene_descriptor.is_file() or scene_descriptor.read_bytes() != (source / "src/Runtime/Scene.h").read_bytes():
            raise AssertionError("Installed Scene descriptor header is missing or stale")
        guard = prefix / "include/LVUI/WindowCloseGuard.h"
        if not guard.is_file() or guard.read_bytes() != (source / "src/Runtime/WindowCloseGuard.h").read_bytes():
            raise AssertionError("Installed window close policy header is missing or stale")
        for name in ("Popover.h", "NativePopover.h"):
            installed = prefix / "include" / "LVUI" / name
            original = source / "src" / "Overlays" / name
            if not installed.is_file() or installed.read_bytes() != original.read_bytes():
                raise AssertionError(f"Installed popover header is missing or differs: {name}")
        for name in ("NativeMaterial.h", "NativeMaterialGradient.h", "StyleTokens.h"):
            installed = prefix / "include" / "LVUI" / name
            original = source / "src" / "Style" / name
            if not installed.is_file() or installed.read_bytes() != original.read_bytes():
                raise AssertionError(f"Installed material header is missing or differs: {name}")
        avatar_asset = prefix / "share/LVUI/resources/design-icons/avatar-placeholder.svg"
        if not avatar_asset.is_file() or avatar_asset.read_bytes() != (source / "resources/design-icons/avatar-placeholder.svg").read_bytes():
            raise AssertionError("Installed Avatar placeholder is missing or stale")
        geometry = prefix / "share/LVUI/resources/lvui-design-icon-geometry.json"
        if not geometry.is_file() or geometry.read_bytes() != (source / "catalog/lvui-design-icon-geometry.json").read_bytes():
            raise AssertionError("Installed design icon geometry snapshot is missing or stale")
        radial = prefix / "share/LVUI/resources/lvui-material-radial.json"
        if not radial.is_file() or radial.read_bytes() != (source / "catalog/lvui-material-radial.json").read_bytes():
            raise AssertionError("Installed material radial snapshot is missing or stale")
        button_motion = prefix / "share/LVUI/resources/lvui-button-motion.json"
        if not button_motion.is_file() or button_motion.read_bytes() != (source / "catalog/lvui-button-motion.json").read_bytes():
            raise AssertionError("Installed button motion snapshot is missing or stale")
        slider_center = prefix / "share/LVUI/resources/lvui-slider-center.json"
        if not slider_center.is_file() or slider_center.read_bytes() != (source / "catalog/lvui-slider-center.json").read_bytes():
            raise AssertionError("Installed center slider snapshot is missing or stale")
        slider_filled = prefix / "share/LVUI/resources/lvui-slider-filled.json"
        if not slider_filled.is_file() or slider_filled.read_bytes() != (source / "catalog/lvui-slider-filled.json").read_bytes():
            raise AssertionError("Installed filled slider snapshot is missing or stale")
        slider_segmented = prefix / "share/LVUI/resources/lvui-slider-labeled-segmented.json"
        if not slider_segmented.is_file() or slider_segmented.read_bytes() != (source / "catalog/lvui-slider-labeled-segmented.json").read_bytes():
            raise AssertionError("Installed labeled/segmented slider snapshot is missing or stale")
        slider_symbol = prefix / "share/LVUI/resources/lvui-slider-symbol.json"
        symbol_header = prefix / "include/LVUI/SliderSymbol.generated.h"
        if not slider_symbol.is_file() or slider_symbol.read_bytes() != (source / "catalog/lvui-slider-symbol.json").read_bytes():
            raise AssertionError("Installed slider symbol snapshot is missing or stale")
        if not symbol_header.is_file() or symbol_header.read_bytes() != (source / "src/Style/SliderSymbol.generated.h").read_bytes():
            raise AssertionError("Installed slider symbol header is missing or stale")
        segments = prefix / "share/LVUI/resources/lvui-segmented-geometry.json"
        if not segments.is_file() or segments.read_bytes() != (source / "catalog/lvui-segmented-geometry.json").read_bytes():
            raise AssertionError("Installed segmented control snapshot is missing or stale")
        button_methods = prefix / "include" / "LVUI" / "NativeButtonMethods.h"
        bridge_header = prefix / "include" / "LVUI" / "CompilerBridge.h"
        if not bridge_header.is_file() or bridge_header.read_bytes() != (source / "src/Python/CompilerBridge.h").read_bytes():
            raise AssertionError("Installed compiler bridge header is missing or stale")
        python_package = prefix / "share/LVUI/python/ii_lvui/__init__.py"
        if not python_package.is_file() or python_package.read_bytes() != (source / "src/Python/ii_lvui/__init__.py").read_bytes():
            raise AssertionError("Installed LVUI Python API is missing or stale")
        python_env = os.environ.copy()
        python_env.pop("LVUI_COMPILER_LIBRARY", None)
        python_env["PYTHONPATH"] = str(python_package.parent.parent)
        subprocess.run([sys.executable, "-B", "-c",
                        "import ii_lvui; assert 'struct InstalledView final' in "
                        "ii_lvui.compile_native('Window { Label() }', name='InstalledView'); "
                        "ii_lvui.compile_native('VStack { Label(text: \\\"Embedded\\\") }'); "
                        "ii_lvui.compile_native('ListItemSelector(options: [\\\"One\\\", \\\"Two\\\"])'); "
                        "assert 'configure_application(ii::runtime::Application&' in "
                        "ii_lvui.compile_native('Application { Window { Label() } }'); "
                        "assert 'ii::gui::IconView::create' in "
                        "ii_lvui.compile_native('Window { Image(iconName: \"brush\") }'); "
                        "assert 'lvui::icon_segmented_configuration()' in "
                        "ii_lvui.compile_native('Window { IconSegmentedControl() }'); "
                        "assert 'ii::gui::NativeAvatar::create' in "
                        "ii_lvui.compile_native('Window { EventListener() { Avatar() } }'); "
                        "assert 'value.placeholder_color = ii::gui::avatar_color' in "
                        "ii_lvui.compile_native('Window { Avatar() }'); "
                        "assert 'lvui::SliderType::center_biased' in "
                        "ii_lvui.compile_native('Window { Slider(type: .centerBiased) }'); "
                        "assert 'lvui::SliderType::filled' in "
                        "ii_lvui.compile_native('Window { Slider(type: .filled) }'); "
                        "assert 'lvui::SliderType::min_max_labels' in "
                        "ii_lvui.compile_native('Window { Slider(type: .minMaxLabels) }'); "
                        "assert 'value.segment_count = lvui::slider_segment_count(' in "
                        "ii_lvui.compile_native('Window { Slider(type: .segmented, segmentCount: 3) }'); "
                        "assert 'value.palette.label = lvui::native::slider_color(ii::gui::WidgetColor{1, 0, 0,' in "
                        "ii_lvui.compile_native('Window { Slider(labelColor: \"#ff000080\") }'); "
                        "assert 'value.palette.track = lvui::native::slider_color(ii::gui::WidgetColor{0.1, 0.2, 0.3, 0.4})' in "
                        "ii_lvui.compile_native('Window { Slider() }', style_profile='let surfaceSolid: Color = Color(0.1, 0.2, 0.3, 0.4)'); "
                        "assert 'value.palette.symbol = lvui::native::slider_color(' in "
                        "ii_lvui.compile_native('Window { Slider(showSymbol: true) }'); "
                        "assert 'value.configuration.background = ii::gui::avatar_color(ii::gui::WidgetColor{0.1, 0.2, 0.3, 0.4})' in "
                        "ii_lvui.compile_native('Window { Avatar() }', style_profile='let surfaceRaised: Color = Color(0.1, 0.2, 0.3, 0.4)'); "
                        "assert '.size_to_content = true' in "
                        "ii_lvui.compile_native('Window { EventListener(sizeToContent: true) { Avatar() } }'); "
                        "assert 'danger_hover' in "
                        "ii_lvui.compile_native('Window { PushButton(dangerHoverColor: \"#ff0000\") }'); "
                        "assert 'value.configuration.palette.danger_pressed_default = lvui::StyleColor{' in "
                        "ii_lvui.compile_native('Window { PushButton(tone: .destructive) }'); "
                        "assert 'ButtonSelectionStyle::segmented' in "
                        "ii_lvui.compile_native('Window { LabelButton(selectionStyle: .segmented) }'); "
                        "assert 'widgets.scroll_view(' in "
                        "ii_lvui.compile_native('Window { ScrollView(width: 100, height: 60) { Label() } }'); "
                        "assert 'ii::gui::TextRole::body' in "
                        "ii_lvui.compile_native('Window { Label(style: .body, text: \"Brush\") }'); "
                        "assert 'value.blur_enabled = false' in "
                        "ii_lvui.compile_native('Window { Popover(blurEnabled: false) }'); "
                        "assert 'value.primary_color = lvui::native::popover_color' in "
                        "ii_lvui.compile_native('Window { Popover(primaryColor: \"#ff0000\") }'); "
                        "assert 'value.motion_enabled = false' in "
                        "ii_lvui.compile_native('Window { Popover(motionEnabled: false) }')"],
                       check=True, env=python_env, stdout=subprocess.DEVNULL)
        for name in ("IconButtonDefaults.generated.h", "ButtonMotionDefaults.generated.h", "DesignIconDefaults.generated.h", "MaterialRadialDefaults.generated.h", "SegmentedDefaults.generated.h", "SliderCenter.generated.h", "SliderFilled.generated.h", "SliderSegmented.generated.h", "SearchFieldTokens.generated.h",
                     "SearchFieldMaterial.generated.h"):
            installed = prefix / "include" / "LVUI" / name
            original = source / "src" / "Style" / name
            if not installed.is_file() or installed.read_bytes() != original.read_bytes():
                raise AssertionError(f"Installed generated component defaults are missing or stale: {name}")
        if not button_methods.is_file() or button_methods.read_bytes() != (source / "src/Controls/NativeButtonMethods.h").read_bytes():
            raise AssertionError("Installed button method registry is missing or stale")
        include = re.compile(
            r'^\s*#\s*include\s*[<"](?:LVRS/|Qt[A-Za-z0-9_]*/|'
            r'Qt[A-Za-z0-9_]*\b|Q[A-Z][A-Za-z0-9_]*\b)',
            re.M)
        for header in headers:
            text = header.read_text(encoding="utf-8")
            if include.search(text) or re.search(r'\blvrs::', text):
                raise AssertionError(f"Installed header exposes a legacy UI dependency: {header}")
        packages = list(prefix.glob("**/cmake/LVUI/*.cmake"))
        if not packages:
            raise AssertionError("No LVUI CMake package was installed")
        targets = next((path for path in packages if path.name == "LVUITargets.cmake"), None)
        if targets is None or "LVUI::CompilerBridge" not in targets.read_text(encoding="utf-8"):
            raise AssertionError("Installed LVUI package does not expose the compiler bridge target")
        dependency = re.compile(r'find_(?:package|dependency)\s*\(\s*(?:LVRS\b|Qt[56]?[A-Za-z0-9_]*\b)|LVRS::|Qt[56]?::', re.I)
        for package in packages:
            if dependency.search(package.read_text(encoding="utf-8")):
                raise AssertionError(f"Installed package exposes a legacy UI dependency: {package}")
        cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
        if sys.platform == "darwin":
            compiler = re.search(r'^CMAKE_CXX_COMPILER:(?:FILEPATH|STRING)=(.+)$', cache, re.M)
            if not compiler:
                raise AssertionError("C++ compiler is missing from the existing build configuration")
            # Check the installed header and its generated center defaults without
            # creating a second build tree or an executable.
            consumer = ('#include <LVUI/Slider.h>\n#include <LVUI/Button.h>\n#include <LVUI/DropdownButton.h>\n#include <LVUI/Stepper.h>\n#include <LVUI/AlertButton.h>\n#include <LVUI/ListToolbar.h>\n#include <LVUI/ListFooter.h>\n#include <LVUI/HierarchyToolbarLayout.h>\n#include <LVUI/ListItem.h>\n#include <LVUI/Modal.h>\n#include <LVUI/Alert.h>\n'
                        'static_assert(std::string_view(lvui::slider_symbol_defaults::legacy_icon) == "sun");\n'
                        'template<class P> struct SliderContract { using Policy = P; };\n'
                        'static_assert(std::is_same_v<lvui::SliderBinding<SliderContract>::Policy, lvui::SliderModelPolicy>);\n'
                        'void check_slider() { lvui::SliderConfiguration c; '
                        'c.palette.label = lvui::StyleColor{.25, .5, 1, .75}; '
                        'c.palette.symbol = lvui::StyleColor{.75, .5, .25, 1}; '
                        'lvui::apply_slider_type(c, lvui::SliderType::center_biased_ticks); '
                        'lvui::apply_slider_type(c, lvui::SliderType::filled); '
                        'lvui::apply_slider_type(c, lvui::SliderType::segmented); c.segment_count = 3; '
                        '(void)c; }\n'
                        'void check_alert_values() { lvui::AlertButtonConfiguration c; (void)c; lvui::ListToolbarConfiguration bar; (void)bar; lvui::ListFooterConfiguration footer; (void)footer; lvui::HierarchyToolbarAppearance appearance; (void)appearance; lvui::ListItemConfiguration row; (void)row; lvui::ModalConfiguration modal; (void)modal; lvui::AlertConfiguration alert; (void)alert; }\n'
                        'void check_button() { lvui::ButtonConfiguration c; '
                        'c.palette.danger_reference = c.palette.danger; '
                        'lvui::DropdownConfiguration d; lvui::StepperConfiguration s; '
                        '(void)(c == c); (void)(d == d); (void)(s == s); }\n')
            subprocess.run([compiler.group(1), '-std=c++23', '-fsyntax-only', '-x', 'c++',
                            '-I', str(prefix / 'include'), '-'], input=consumer, text=True,
                           check=True, env=python_env, stdout=subprocess.DEVNULL)
            core_package = re.search(r'^iiSDKCore_DIR:[^=]+=(.+)$', cache, re.M)
            if not core_package:
                raise AssertionError("The configured iiSDKCore package is missing")
            core_include = Path(core_package.group(1)).parents[2] / 'include'
            icon_consumer = ('#include <LVUI/NativeAuthoredIcons.h>\n#include <LVUI/NativeIconCache.h>\n#include <LVUI/NativeAvatarAssets.h>\n#include <LVUI/NativeCheckBox.h>\n#include <LVUI/NativeToggleSwitch.h>\n#include <LVUI/NativeVisual.h>\n#include <LVUI/NativeTextFieldMaterial.h>\n#include <LVUI/NativeRadioButton.h>\n#include <LVUI/NativeListItemDrawing.h>\n#include <LVUI/NativeListItem.h>\n#include <LVUI/NativeTableViewGroup.h>\n#include <LVUI/NativeTableHeaderPresentation.h>\n'
                             'struct Decoder { using Image=int; '
                             'static ii::core::Result<std::shared_ptr<const int>> decode('
                             'std::shared_ptr<const void>, std::string_view, std::uint32_t) '
                             '{ return std::make_shared<const int>(1); } };\n'
                             'template<class P> struct FakeModel {}; template<class P> struct FakeHost {};\n'
                             'using CheckBox = lvui::native::CheckBoxBinding<FakeModel,FakeHost,Decoder>;\n'
                             'using Toggle = lvui::native::ToggleSwitchBinding<FakeModel,FakeHost>;\n'
                             'using Radio = lvui::native::RadioButtonBinding<FakeModel,FakeHost>;\n'
                             'using ListModel = lvui::native::ListItemModelBinding<FakeModel>;\n'
                             'using ListInput = lvui::native::ListItemInputValue<int>;\n'
                             'using TablePolicy = lvui::native::TableViewGroupPolicy<int,int>;\n'
                             'using HeaderPolicy = lvui::native::TableHeaderPresentationPolicy<Decoder>;\n'
                             'using Material = lvui::native::text_field_material::MaterialBinding<FakeModel>;\n'
                             'void compose(std::shared_ptr<const lvui::IconSource> source) '
                             '{ (void)lvui::native::icon_image<Decoder>(source, "x", 16); '
                             'lvui::native::NativeIconCache<Decoder> cache; (void)cache.resolve("x", source, 16); '
                             '(void)lvui::native::avatar_placeholder_mask_catalog(); (void)sizeof(CheckBox); (void)sizeof(Toggle); (void)sizeof(Radio); (void)sizeof(ListModel); (void)sizeof(ListInput); (void)sizeof(TablePolicy); (void)sizeof(HeaderPolicy); (void)sizeof(Material); (void)lvui::native::native_display_text("test"); }\n')
            subprocess.run([compiler.group(1), '-std=c++23', '-fsyntax-only', '-x', 'c++',
                            '-I', str(prefix / 'include'), '-isystem', str(core_include), '-'],
                           input=icon_consumer, text=True, check=True, env=python_env,
                           stdout=subprocess.DEVNULL)
        image_io = bool(re.search(r'^LVUI_BUILD_IMAGE_IO:BOOL=ON$', cache, re.M))
        image_header = prefix / "share" / "LVUI" / "examples" / "Integrations" / "App" / "NativeImageLoader.h"
        resolver_header = prefix / "include" / "LVUI" / "NativeImageResolver.h"
        config = next((path for path in packages if path.name == "LVUIConfig.cmake"), None)
        if config is None:
            raise AssertionError("Installed LVUIConfig.cmake is missing")
        if 'set(LVUI_CompilerBridge_FOUND TRUE)' not in config.read_text(encoding="utf-8"):
            raise AssertionError("Installed package does not advertise the CompilerBridge component")
        if 'set(LVUI_IIGUI_MIN_VERSION "0.5.0")' not in config.read_text(encoding="utf-8"):
            raise AssertionError("Installed LVUI package does not publish its required iiGUI API version")
        image_targets = config.parent / "LVUIImageIOTargets.cmake"
        if not image_header.is_file() or not resolver_header.is_file():
            raise AssertionError("Image application adapter or independent resolver contract was not installed")
        if image_targets.is_file() != image_io:
            raise AssertionError("Installed image service target disagrees with LVUI_BUILD_IMAGE_IO")
        if image_io:
            target_text = image_targets.read_text(encoding="utf-8")
            if "LVUI::ImageIO" not in target_text:
                raise AssertionError("Installed image contract target is missing")
            for name in ("iiGUI::iiGUI", "iiNetwork::iiNetwork", "iiVisualCodec::Image", "iiFileProvider::Native"):
                if name in target_text:
                    raise AssertionError(f"Image contract unexpectedly depends on {name}")
            if (prefix / "include" / "LVUI" / "NativeImageLoader.h").exists():
                raise AssertionError("Application-specific image loader leaked into the LVUI SDK")


if __name__ == "__main__":
    main()
