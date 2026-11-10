from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Collections/NativeListItemComposite.h").read_text()
if "class NativeListItemComposite final" in s or "ListItemCompositeBinding" not in s:raise SystemExit("LVUI still owns composite list item rendering")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:raise SystemExit("Composite binding directly depends on another SDK")
