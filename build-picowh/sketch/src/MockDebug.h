#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\src\\MockDebug.h"
#pragma once
#include <stddef.h>
namespace mockdebug {
void log(const char* format, ...);
size_t take(char* destination, size_t capacity);
unsigned dropped();
}
