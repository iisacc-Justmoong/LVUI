from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
contracts = [
    root / "src/Collections/NativeVirtualCollection.h",
    root / "src/Collections/NativeMeasuredCollection.h",
    root / "src/Editing/NativeLineIndex.h",
]
texts = [path.read_text() for path in contracts]
for path, text in zip(contracts, texts):
    if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', text, re.M) or "ii::gui::" in text:
        raise SystemExit(f"LVUI contract directly depends on another SDK: {path.name}")
if "virtual_collection_frame(" in texts[0] or "VirtualCollectionPlannerBinding" not in texts[0]:
    raise SystemExit("LVUI still owns virtual collection geometry")
if "class VirtualMeasurements" in texts[1] or "VirtualMeasurementsBinding" not in texts[1]:
    raise SystemExit("LVUI still owns virtual measurement state")
if "class VirtualLineIndex" in texts[2]:
    raise SystemExit("LVUI still owns visible-row geometry indexing")

compiler = (root / "src/Compiler/Compiler.cpp").read_text()
for name in ("iiGUI/VirtualCollectionPlanner.h", "iiGUI/VirtualMeasurements.h", "BasicVirtualMeasurements"):
    if name not in compiler:
        raise SystemExit(f"Generated application does not compose its iiGUI implementation: {name}")
