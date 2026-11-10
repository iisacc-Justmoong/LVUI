from pathlib import Path
import sys
b=Path(sys.argv[1])/"src/Collections"
for suffix in ("DetailActions","DetailControls","Task","PreviewRow","Form","Quantity"):
 s=(b/("ListItem"+suffix+".h")).read_text()
 if "class ListItem"+suffix+" final" in s or "ListItem"+suffix+"Binding" not in s:raise SystemExit("LVUI still owns list item "+suffix+" layout/state")
