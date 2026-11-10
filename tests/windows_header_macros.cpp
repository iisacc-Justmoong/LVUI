#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <windows.h>

// Exercise the difficult include order even if the build defines NOMINMAX.
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#include <LVUI/NativeValues.h>
#include <LVUI/ListItemPreviewRow.h>

#if !defined(min) || !defined(max)
#error "LVUI public headers must preserve the consuming application's macros"
#endif
