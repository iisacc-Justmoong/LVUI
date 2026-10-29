#include "LVRSRepetition.generated.h"
#include "LVRSSharedRepetition.generated.h"
#include "LVRSVirtualRepetition.generated.h"
#include "LVRSMeasuredRepetition.generated.h"
#include "LVRSNavigatedRepetition.generated.h"
#include "LVRSReturnedCallbacks.generated.h"
#include "LVRSBackendConstruction.generated.h"
// Force compilation of the actual LVRS factories and their row specializations.
// This object target has no entrypoint and is never executed in intermediate work.
namespace lvui_compile_evidence {
auto value_rows=&lvui_generated::LVRSRepetition::build;
auto shared_rows=&lvui_generated::LVRSSharedRepetition::build;
auto virtual_rows=&lvui_generated::LVRSVirtualRepetition::build;
auto measured_rows=&lvui_generated::LVRSMeasuredRepetition::build;
auto navigated_rows=&lvui_generated::LVRSNavigatedRepetition::build;
auto returned_callbacks=&lvui_generated::LVRSReturnedCallbacks::build;
auto backend_construction=&lvui_generated::LVRSBackendConstruction::build;
}
