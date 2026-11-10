from pathlib import Path
import re,sys
r=Path(sys.argv[1]);cell=(r/"src/Collections/TableCellItem.h").read_text();row=(r/"src/Collections/TableRowView.h").read_text()
for text,name,policy in ((cell,"class TableCellItem final","TableCellItemModelPolicy"),(row,"class TableRowView final","TableRowViewModelPolicy")):
 if name in text or policy not in text:raise SystemExit("Table cell or row model remains in LVUI")
 if re.search(r"^\s*#\s*include\s*[<\"]ii(?!SDKCore)",text,re.M) or "ii::gui::" in text:raise SystemExit("Table model binding depends on another SDK")
