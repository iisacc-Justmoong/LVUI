from pathlib import Path
import re,sys
s=(Path(sys.argv[1])/"src/Editing/NativeSearchField.h").read_text()
if "class NativeSearchField final" in s or "SearchFieldHostPolicy" not in s or "SearchFieldBinding" not in s:
 raise SystemExit("LVUI search field still owns visual behavior")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s:
 raise SystemExit("LVUI search field directly imports another SDK")
