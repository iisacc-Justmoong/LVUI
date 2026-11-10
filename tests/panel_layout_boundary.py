from pathlib import Path
import sys
root=Path(sys.argv[1]);p=(root/"src/Layout/PanelRegions.h").read_text();h=(root/"src/Collections/HierarchyPanel.h").read_text();l=(root/"src/Collections/ListPanel.h").read_text()
if any(x in p for x in ("panel_regions(","ii::gui::","#include <iiGUI/")) or "PanelLayoutBinding" not in p:raise SystemExit("LVUI still owns panel region layout")
if any(x in h+l for x in ("hierarchy_panel_geometry(","list_panel_geometry(","valid_hierarchy_panel_configuration(","valid_list_panel_configuration(")):raise SystemExit("LVUI still owns panel geometry")
