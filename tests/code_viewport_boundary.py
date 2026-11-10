from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
contract = (root / "src/Editing/CodeViewport.h").read_text()
if "code_viewport(" in contract or "CodeViewportBinding" not in contract:
    raise SystemExit("LVUI still owns visible code viewport planning")
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)', contract, re.M) or "ii::gui::" in contract:
    raise SystemExit("The code viewport contract directly depends on a graphics SDK")
host = (root.parent / "iiGUI/src/Widgets/NativeCodeEditorView.h").read_text()
if "BasicCodeViewport" not in host:
    raise SystemExit("The iiGUI code editor does not own its viewport planner")
