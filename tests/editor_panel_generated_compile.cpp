#include "EditorPanelFixture.generated.h"

// The generated view must type-check with the real iiGUI controls and the
// caller's native viewmodel; no window or platform application is launched.
using EditorPanelCompiledView = lvui_generated::EditorPanelFixture;
static_assert(sizeof(EditorPanelCompiledView) > 0);
int main() { return 0; }
