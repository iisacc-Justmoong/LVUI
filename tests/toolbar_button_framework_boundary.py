"""Toolbar visual behavior belongs to iiGUI; LVUI supplies independent values."""
from pathlib import Path
import re,sys
root=Path(sys.argv[1]);host=(root/'src/Controls/NativeToolbarButton.h').read_text()
if re.search(r'^\s*#\s*include\s*[<"]ii(?!SDKCore)',host,re.M) or 'ii::gui::' in host:
    raise SystemExit('Toolbar button binding directly depends on another SDK')
for required in ('ToolbarButtonModelPolicy','ToolbarButtonBinding','ToolbarButtonInputValue','NativeIconCache<Decoder>'):
    if required not in host:raise SystemExit('Toolbar button binding missing: '+required)
for forbidden in ('class NativeToolbarButton final','builder.retain(','builder.on_publish(','builder.on_retire(','WidgetDrawing','measure_text_layout('):
    if forbidden in host:raise SystemExit('LVUI still owns toolbar widget behavior: '+forbidden)
if 'class ToolbarButton final' in (root/'src/Controls/ToolbarButton.h').read_text():raise SystemExit('LVUI still owns toolbar button model')
layout=(root/'src/Collections/HierarchyToolbarLayout.h').read_text()
if any(name in layout for name in ('hierarchy_toolbar_geometry(','valid_hierarchy_toolbar_appearance(','ii::gui::','#include <iiGUI/')):
    raise SystemExit('LVUI hierarchy layout values retained graphics behavior')
source=(root/'src/Collections/NativeHierarchyPanel.h').read_text()
if 'HierarchyPanelBinding' not in source or 'ToolbarLayout' not in source or 'hierarchy_toolbar_geometry(' in source:
    raise SystemExit('Hierarchy panel does not inject the iiGUI toolbar layout')

if "HierarchyToolbarLayoutBinding" not in (root/"src/Collections/NativeHierarchyToolbar.h").read_text():raise SystemExit("Hierarchy toolbar does not compose the iiGUI layout")
