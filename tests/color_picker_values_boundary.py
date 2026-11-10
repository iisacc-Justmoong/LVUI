from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Color/NativeColorPickerValues.h").read_text()
if "class NativeColorPickerValues final" in s or "ColorPickerValuesPolicy" not in s or "ColorPickerValuesBinding" not in s:raise SystemExit("LVUI still owns color picker value view")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("LVUI color picker value directly depends on another SDK")
