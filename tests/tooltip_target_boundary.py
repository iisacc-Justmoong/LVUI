from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Overlays/NativeTooltipTarget.h").read_text()
if "class NativeTooltipTarget final" in s or "TooltipTargetBinding" not in s:raise SystemExit("LVUI still owns tooltip target observation")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("Tooltip target binding directly depends on another SDK")
