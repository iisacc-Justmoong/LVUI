#include "LVRSActivationWork.generated.h"
// Compile the native work factory against real LVRS public headers. Execution
// remains in the repository's final full-goal validation batch.
namespace lvui_compile_evidence {
auto activation_work=&lvui_generated::LVRSActivationWork::build;
}
