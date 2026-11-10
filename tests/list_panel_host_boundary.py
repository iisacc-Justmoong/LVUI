from pathlib import Path
import re,sys
root=Path(sys.argv[1]);s=(root/"src/Collections/NativeListPanel.h").read_text()
if "class NativeListPanel final" in s or "ListPanelHostPolicy" not in s or "ListPanelBinding" not in s:raise SystemExit("LVUI still owns list panel host")
if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",s,re.M) or "ii::gui::" in s or "<LVUI/NativeListView.h>" in s:raise SystemExit("List panel binding directly depends on graphics SDK")
