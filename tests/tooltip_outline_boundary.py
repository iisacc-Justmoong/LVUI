from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Overlays/TooltipOutline.h").read_text()
if "tooltip_outline(" in s or "TooltipOutlineBinding" not in s:raise SystemExit("LVUI still owns tooltip outline geometry")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("Tooltip outline binding directly depends on another SDK")
