#include "NativeOwnedProgress.generated.h"
#include "NativeOwnedProgressAdvanced.generated.h"
#include "NativeOwnedProgressSource.generated.h"
#include "NativeOwnedProgressScoped.generated.h"
#include "NativeOwnedProgressMotion.generated.h"

// Compiling the generated definitions exercises both constant and reactive
// construction without linking or loading a windowing implementation.
auto owned_progress_factory = &lvui_generated::NativeOwnedProgress::build;
auto owned_advanced_progress_factory = &lvui_generated::NativeOwnedProgressAdvanced::build;
auto owned_sourced_progress_factory = &lvui_generated::NativeOwnedProgressSource::build;
auto owned_scoped_progress_factory = &lvui_generated::NativeOwnedProgressScoped::build;
auto owned_motion_progress_factory = &lvui_generated::NativeOwnedProgressMotion::build;
