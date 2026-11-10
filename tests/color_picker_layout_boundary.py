from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Color/ColorPickerLayout.h").read_text()
if "color_picker_layout(" in s or "ColorPickerLayoutPolicy" not in s:raise SystemExit("LVUI still owns color picker geometry")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("LVUI picker geometry binding directly imports another SDK")
