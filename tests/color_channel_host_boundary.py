from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Color/NativeColorChannel.h").read_text()
if "class NativeColorChannel final" in s or "ColorChannelHostPolicy" not in s or "ColorChannelBinding" not in s:raise SystemExit("LVUI still owns color channel view")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("LVUI color channel directly depends on another SDK")
