from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Color/NativeColorPicker.h").read_text()
if "class NativeColorPicker final" in s or "ColorPickerHostPolicy" not in s or "ColorPickerBinding" not in s:raise SystemExit("LVUI still owns color picker visual host")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("LVUI color picker directly depends on another SDK")
