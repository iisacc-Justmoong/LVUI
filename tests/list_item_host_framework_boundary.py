from pathlib import Path
import re,sys
host=(Path(sys.argv[1])/'src/Collections/NativeListItem.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',host,re.M) or 'ii::gui::' in host:
    raise SystemExit('List item binding directly depends on the graphics SDK')
if 'ListItemBinding=' not in host:raise SystemExit('List item binding is not application-composable')
