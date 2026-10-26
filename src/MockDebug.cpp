#include "MockDebug.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <algorithm>
namespace {
char queue[4096]{};
size_t head=0, count=0;
unsigned lost=0;
}
void mockdebug::log(const char* format, ...) {
  char line[128];
  va_list args; va_start(args,format);
  int n=vsnprintf(line,sizeof(line),format,args); va_end(args);
  if(n<=0) return;
  size_t length=std::min(size_t(n),sizeof(line)-1);
  if(length>sizeof(queue)-count) {++lost; return;}
  for(size_t i=0;i<length;++i) queue[(head+count+i)%sizeof(queue)]=line[i];
  count+=length;
}
size_t mockdebug::take(char* destination,size_t capacity) {
  size_t n=std::min(count,capacity);
  for(size_t i=0;i<n;++i) destination[i]=queue[(head+i)%sizeof(queue)];
  head=(head+n)%sizeof(queue); count-=n; return n;
}
unsigned mockdebug::dropped() {return lost;}
