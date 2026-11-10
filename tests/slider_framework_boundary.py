from pathlib import Path
import re,sys
text=(Path(sys.argv[1])/"src/Controls/NativeSlider.h").read_text()
if "class NativeSlider final" in text or "SliderHostPolicy" not in text or "SliderViewBinding" not in text:
    raise SystemExit("LVUI must expose a slider binding rather than its visual host")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",text,re.M) or "ii::gui::" in text:
    raise SystemExit("LVUI slider depends directly on another SDK")
