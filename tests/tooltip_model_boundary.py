from pathlib import Path
import re,sys
root=Path(sys.argv[1]);m=(root/"src/Overlays/Tooltip.h").read_text();c=(root/"src/Overlays/TooltipController.h").read_text()
if "class Tooltip final" in m or "TooltipModelBinding" not in m or "class TooltipController final" in c or "TooltipControllerBinding" not in c:raise SystemExit("LVUI still owns tooltip state or controller")
for s in (m,c):
 if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("Tooltip binding directly depends on another SDK")
