from pathlib import Path
import re,sys
r=Path(sys.argv[1]);model=(r/"src/Navigation/MobileNavigation.h").read_text();motion=(r/"src/Navigation/MobileNavigationMotion.h").read_text()
if "valid_mobile_navigation(" in model or "mobile_navigation_layout(" in model or "class MobileNavigationMotion final" in motion:raise SystemExit("LVUI still owns mobile navigation visual behavior")
if "MobileNavigationModelPolicy" not in model or "MobileNavigationMotionPolicy" not in motion:raise SystemExit("Mobile navigation bindings missing")
if any(re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s for s in (model,motion)):raise SystemExit("Mobile navigation depends on another SDK")
