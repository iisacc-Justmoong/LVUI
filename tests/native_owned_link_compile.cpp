#include <LVUI/NativeLink.h>
#include <iiGUI/NativeLinkView.h>
#include "NativeOwnedLink.generated.h"
#include "NativeOwnedLinkMethods.generated.h"
namespace lvui::native { using NativeLink=LinkBinding<ii::gui::BasicLink,ii::gui::BasicNativeLink>; }
static_assert(sizeof(lvui::native::NativeLink)>0);
static_assert(sizeof(lvui_generated::NativeOwnedLink::Scope)>0);
static_assert(sizeof(lvui_generated::NativeOwnedLinkMethods::Scope)>0);
