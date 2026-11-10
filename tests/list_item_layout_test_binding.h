#pragma once
#include <LVUI/ListItemDetailActions.h>
#include <iiGUI/ListItemDetailActionsLayout.h>
#include <LVUI/ListItemDetailControls.h>
#include <iiGUI/ListItemDetailControlsLayout.h>
#include <LVUI/ListItemTask.h>
#include <iiGUI/ListItemTaskLayout.h>
#include <LVUI/ListItemPreviewRow.h>
#include <iiGUI/ListItemPreviewRowLayout.h>
#include <LVUI/ListItemForm.h>
#include <iiGUI/ListItemFormLayout.h>
#include <LVUI/ListItemQuantity.h>
#include <iiGUI/ListItemQuantityLayout.h>
namespace lvui {
using ListItemDetailActions=ListItemDetailActionsBinding<ii::gui::BasicListItemDetailActions>;
using ListItemDetailControls=ListItemDetailControlsBinding<ii::gui::BasicListItemDetailControls>;
using ListItemTask=ListItemTaskBinding<ii::gui::BasicListItemTask>;
using ListItemPreviewRow=ListItemPreviewRowBinding<ii::gui::BasicListItemPreviewRow>;
using ListItemForm=ListItemFormBinding<ii::gui::BasicListItemForm>;
using ListItemQuantity=ListItemQuantityBinding<ii::gui::BasicListItemQuantity>;
}
