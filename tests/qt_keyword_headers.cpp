// Verify the supported umbrella include after Qt keyword macros are defined.
#define slots
#define signals public
#define emit
#include <LVUI>
#if !defined(slots) || !defined(signals) || !defined(emit)
#error "LVUI must restore the consuming application's Qt keyword macros"
#endif
int main() { return 0; }
