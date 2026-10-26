#pragma once
#include <stddef.h>
namespace mockdebug {
void log(const char* format, ...);
size_t take(char* destination, size_t capacity);
unsigned dropped();
}
