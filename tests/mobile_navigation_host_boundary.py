from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Navigation/NativeMobileNavigation.h").read_text()
if "class NativeMobileNavigation final" in s or "MobileNavigationHostPolicy" not in s or "MobileNavigationBinding" not in s:raise SystemExit("LVUI still owns mobile navigation host")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("LVUI mobile navigation directly depends on another SDK")
