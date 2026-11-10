"""Alert visuals belong to iiGUI; LVUI supplies independent values and bindings."""
from pathlib import Path
import re,sys
root=Path(sys.argv[1]);values=(root/'src/Overlays/Alert.h').read_text();host=(root/'src/Overlays/NativeAlert.h').read_text()
if 'class Alert final' in values or any(v in host for v in ('class NativeAlert final','builder.popover(','builder.on_retire(')):
    raise SystemExit('LVUI still owns alert visuals or lifecycle')
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',host,re.M) or 'ii::gui::' in host:
    raise SystemExit('Alert binding directly depends on another SDK')
for token in ('AlertBinding','AlertModelPolicy','AlertInputValue','NativeAlertCallbacksValue'):
    if token not in host:raise SystemExit('Alert independent binding missing: '+token)
