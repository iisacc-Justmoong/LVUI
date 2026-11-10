from pathlib import Path
import re,sys
base=Path(sys.argv[1])/"src/Overlays"
for name,former,binding in (("NativeTooltip.h","class NativeTooltip final","TooltipBinding"),("NativeTooltipScheduler.h","class NativeTooltipScheduler final","TooltipSchedulerBinding")):
 s=(base/name).read_text()
 if former in s or binding not in s:raise SystemExit(f"LVUI still owns {name}")
 if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit(f"{name} directly depends on another SDK")
