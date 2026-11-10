"""List-footer presentation belongs to iiGUI; LVUI supplies independent values."""
from pathlib import Path
import re,sys
root=Path(sys.argv[1]);host=(root/'src/Collections/NativeListFooter.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',host,re.M) or 'ii::gui::' in host:
    raise SystemExit('List footer binding directly depends on another SDK')
for required in ('ListFooterModelPolicy','ListFooterBinding','NativeIconCache<Decoder>'):
    if required not in host:raise SystemExit('List footer binding missing: '+required)
for forbidden in ('class NativeListFooter final','builder.retain(','builder.on_publish(','builder.on_retire(','measure_text_layout('):
    if forbidden in host:raise SystemExit('LVUI still owns list-footer behavior: '+forbidden)
if 'class ListFooter final' in (root/'src/Collections/ListFooter.h').read_text():raise SystemExit('LVUI still owns list-footer geometry')
source=(root/'src/Collections/NativeListPanel.h').read_text()
if 'ListPanelBinding' not in source or 'FooterModel' not in source:
    raise SystemExit('List panel does not inject the iiGUI footer')

panel=(root/"src/Collections/NativeHierarchyPanel.h").read_text()
if "HierarchyPanelBinding" not in panel or "FooterModel" not in panel:raise SystemExit("Hierarchy panel does not inject the footer")
