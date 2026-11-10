"""Generated apps and windows diagnose missing Core and host contracts."""
from pathlib import Path
import re
import subprocess
import sys

from workspace_support import checked_workspace, provider

workspace = checked_workspace(sys.argv[1])
compiler, cxx = sys.argv[2:4]
source = workspace / "guard.lvui"
provider.write_text(source, 'Window { Label(text: "Guard") }\n', encoding="utf-8")
generated = subprocess.run([compiler, "--name", "Guard", str(source)], text=True,
                           capture_output=True, check=True).stdout
headers = workspace / "iiGUI"
provider.mkdir(headers)
for name in ("NativeWidgets.h", "Bindings.h", "Host.h"):
    provider.write_text(headers / name, "#pragma once\n", encoding="utf-8")

def preprocess(source=generated):
    return subprocess.run([cxx, "-std=c++23", "-E", "-x", "c++", "-I", str(workspace), "-"],
                          input=source, text=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

old = preprocess()
if old.returncode == 0 or "requires iiGUI 0.5.0" not in old.stderr:
    raise AssertionError(f"Old host was not diagnosed: {old.stderr}")
provider.write_text(headers / "NativeWidgets.h", "#pragma once\n#define IIGUI_TEXT_WEIGHT_RULES_API 1\n", encoding="utf-8")
supported = preprocess()
if supported.returncode != 0:
    raise AssertionError(f"A host advertising the required API was rejected: {supported.stderr}")

application_source = workspace / "application-guard.lvui"
provider.write_text(application_source, 'Application { Window { Label(text: "Guard") } }\n', encoding="utf-8")
application = subprocess.run([compiler, "--name", "ApplicationGuard", str(application_source)],
                             text=True, capture_output=True, check=True).stdout
lvui_headers = workspace / "LVUI"
provider.mkdir(lvui_headers)
for name in re.findall(r'^#include <LVUI/([^/>]+)>', application, re.M):
    provider.write_text(lvui_headers / name, "#pragma once\n", encoding="utf-8")
provider.write_text(headers / "Application.h", "#pragma once\n", encoding="utf-8")
old_application = preprocess(application)
if old_application.returncode == 0 or "requires iiSDKCore atomic application configuration" not in old_application.stderr:
    raise AssertionError(f"An old Core application API was not diagnosed: {old_application.stderr}")
provider.write_text(headers / "Application.h", "#pragma once\n#define IISDKCORE_APPLICATION_CONFIGURATION_TRANSACTION_API 1\n", encoding="utf-8")
old_scope = preprocess(application)
if old_scope.returncode == 0 or "requires iiSDKCore before-driver attachment" not in old_scope.stderr:
    raise AssertionError(f"A Core without before-driver ownership was not diagnosed: {old_scope.stderr}")
provider.write_text(headers / "Application.h", "#pragma once\n#define IISDKCORE_APPLICATION_CONFIGURATION_TRANSACTION_API 1\n#define IISDKCORE_APPLICATION_BEFORE_DRIVER_API 1\n", encoding="utf-8")
new_application = preprocess(application)
if new_application.returncode != 0:
    raise AssertionError(f"A Core advertising both application contracts was rejected: {new_application.stderr}")

activity_source = workspace / "application-activity-guard.lvui"
provider.write_text(activity_source, 'Application(onActive: vm.run) {\n import vm Backend\n Window { Label(text: "Guard") }\n}\n', encoding="utf-8")
activity = subprocess.run([compiler, "--name", "ActivityGuard", str(activity_source)],
                          text=True, capture_output=True, check=True).stdout
for name in re.findall(r'^#include <LVUI/([^/>]+)>', activity, re.M):
    provider.write_text(lvui_headers / name, "#pragma once\n", encoding="utf-8")
old_activity = preprocess(activity)
if old_activity.returncode == 0 or "requires iiGUI application activity support" not in old_activity.stderr:
    raise AssertionError(f"An old activity host was not diagnosed: {old_activity.stderr}")
provider.write_text(headers / "Host.h", "#pragma once\n#define IIGUI_HOST_APPLICATION_ACTIVITY_API 1\n", encoding="utf-8")
new_activity = preprocess(activity)
if new_activity.returncode != 0:
    raise AssertionError(f"A host advertising activity support was rejected: {new_activity.stderr}")

scenes_source = workspace / "application-scenes-guard.lvui"
provider.write_text(scenes_source, 'Application { Window { Label() }\n Window { Label() } }\n', encoding="utf-8")
scenes = subprocess.run([compiler, "--name", "ScenesGuard", str(scenes_source)],
                        text=True, capture_output=True, check=True).stdout
for name in re.findall(r'^#include <LVUI/([^/>]+)>', scenes, re.M):
    provider.write_text(lvui_headers / name, "#pragma once\n", encoding="utf-8")
old_scenes = preprocess(scenes)
if old_scenes.returncode == 0 or "require iiGUI host failure notifications" not in old_scenes.stderr:
    raise AssertionError(f"An old host without failure notifications was not diagnosed: {old_scenes.stderr}")
provider.write_text(headers / "Host.h", "#pragma once\n#define IIGUI_HOST_APPLICATION_ACTIVITY_API 1\n#define IIGUI_HOST_FAILURE_HANDLER_API 1\n", encoding="utf-8")
old_driver = preprocess(scenes)
if old_driver.returncode == 0 or "require an independent iiGUI application driver" not in old_driver.stderr:
    raise AssertionError(f"An old host without an independent application driver was not diagnosed: {old_driver.stderr}")
provider.write_text(headers / "Host.h", "#pragma once\n#define IIGUI_HOST_APPLICATION_ACTIVITY_API 1\n#define IIGUI_HOST_FAILURE_HANDLER_API 1\n#define IIGUI_HOST_APPLICATION_DRIVER_API 1\n", encoding="utf-8")
new_scenes = preprocess(scenes)
if new_scenes.returncode != 0:
    raise AssertionError(f"A host advertising failure notifications was rejected: {new_scenes.stderr}")

scene_event_source = workspace / "scene-event-guard.lvui"
provider.write_text(scene_event_source, 'Application {\n import vm Backend\n Window(onOpened: vm.opened) { Label() }\n}\n', encoding="utf-8")
scene_event = subprocess.run([compiler, "--name", "SceneEventGuard", str(scene_event_source)],
                             text=True, capture_output=True, check=True).stdout
for name in re.findall(r'^#include <LVUI/([^/>]+)>', scene_event, re.M):
    provider.write_text(lvui_headers / name, "#pragma once\n", encoding="utf-8")
old_scene_event = preprocess(scene_event)
if old_scene_event.returncode == 0 or "requires iiGUI committed window publication" not in old_scene_event.stderr:
    raise AssertionError(f"An old window publication host was not diagnosed: {old_scene_event.stderr}")
provider.write_text(headers / "NativeWidgets.h", "#pragma once\n#define IIGUI_TEXT_WEIGHT_RULES_API 1\n#define IIGUI_COMMITTED_PUBLICATION_API 1\n", encoding="utf-8")
old_binding = preprocess(scene_event)
if old_binding.returncode == 0 or "requires LVUI presentation retirement" not in old_binding.stderr:
    raise AssertionError(f"Old LVUI bindings without presentation retirement were not diagnosed: {old_binding.stderr}")
provider.write_text(lvui_headers / "NativeBindings.h", "#pragma once\n#define LVUI_BINDINGS_RETIRE_EFFECTS_API 1\n", encoding="utf-8")
new_scene_event = preprocess(scene_event)
if new_scene_event.returncode != 0:
    raise AssertionError(f"A host advertising committed window publication was rejected: {new_scene_event.stderr}")
provider.write_text(headers / "NativeWidgets.h", "#pragma once\n#define IIGUI_TEXT_WEIGHT_RULES_API 1\n", encoding="utf-8")

popup_source = workspace / "popup-guard.lvui"
provider.write_text(popup_source, 'Window { Modal(open: true, motionEnabled: false) {} }\n', encoding="utf-8")
popup = subprocess.run([compiler, "--name", "PopupGuard", str(popup_source)], text=True,
                       capture_output=True, check=True).stdout
prefix = popup.split("#include <LVUI/", 1)[0]
if "IIGUI_COMMITTED_PUBLICATION_API" not in prefix:
    raise AssertionError("A popup binding did not require the committed-publication contract")
old_popup = subprocess.run([cxx, "-std=c++23", "-E", "-x", "c++", "-I", str(workspace), "-"],
                           input=prefix, text=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
if old_popup.returncode == 0 or "committed popup publication" not in old_popup.stderr:
    raise AssertionError(f"An old publication host was not diagnosed: {old_popup.stderr}")
provider.write_text(headers / "NativeWidgets.h", "#pragma once\n#define IIGUI_TEXT_WEIGHT_RULES_API 1\n#define IIGUI_COMMITTED_PUBLICATION_API 1\n", encoding="utf-8")
new_popup = subprocess.run([cxx, "-std=c++23", "-E", "-x", "c++", "-I", str(workspace), "-"],
                           input=prefix, text=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
if new_popup.returncode != 0:
    raise AssertionError(f"A host advertising committed publication was rejected: {new_popup.stderr}")

# Popover rendering and its older-host compatibility guards live in iiGUI.
# The generated application must explicitly request that framework surface.
provider.write_text(popup_source, 'Window { Popover(motionEnabled: false) }\n', encoding="utf-8")
popover_code = subprocess.run([compiler, "--name", "PopoverGuard", str(popup_source)], text=True,
                              capture_output=True, check=True).stdout
if "#include <iiGUI/NativePopoverView.h>" not in popover_code or "PopoverBinding<" not in popover_code:
    raise AssertionError("Popover did not compose the framework host explicitly")

def material_guard(guard, defines):
    return subprocess.run([cxx, "-std=c++23", "-E", "-x", "c++", "-"],
                          input=defines + guard.group(), text=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

# A slider must reject an older host header instead of silently losing the
# accessible absolute-value contract introduced by the native adapter.
slider = (Path(__file__).resolve().parents[1] / "src/Controls/NativeSlider.h").read_text(encoding="utf-8")
slider_guard = re.search(r"#if !defined\(IIGUI_ACCESSIBILITY_VALUE_HANDLER_API\).*?\n#endif", slider, re.S)
if slider_guard is None:
    raise AssertionError("Slider lacks an accessible-value host compatibility guard")
old_slider = material_guard(slider_guard, "")
if old_slider.returncode == 0 or "requires iiGUI accessible value support" not in old_slider.stderr:
    raise AssertionError(f"An old slider host was not diagnosed: {old_slider.stderr}")
new_slider = material_guard(slider_guard, "#define IIGUI_ACCESSIBILITY_VALUE_HANDLER_API 1\n")
if new_slider.returncode != 0:
    raise AssertionError(f"A slider host advertising accessible values was rejected: {new_slider.stderr}")

