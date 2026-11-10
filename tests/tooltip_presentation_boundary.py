from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Overlays/NativeTooltipPresentation.h").read_text()
if "class NativeTooltipPresentation final" in s or "TooltipPresentationBinding" not in s:raise SystemExit("LVUI still owns tooltip presentation")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("Tooltip presentation binding directly depends on another SDK")
