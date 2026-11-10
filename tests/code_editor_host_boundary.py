from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Editing/NativeCodeEditor.h").read_text()
if "class NativeCodeEditor final" in s or "CodeEditorHostPolicy" not in s or "CodeEditorBinding" not in s:raise SystemExit("LVUI still owns code editor host")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("Code editor binding directly depends on another SDK")
