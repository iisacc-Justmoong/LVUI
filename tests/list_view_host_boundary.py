from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Collections/NativeListView.h").read_text()
if "class NativeListView final" in s or "ListViewHostPolicy" not in s or "ListViewBinding" not in s:raise SystemExit("LVUI still owns list view host")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("List view binding directly depends on another SDK")
