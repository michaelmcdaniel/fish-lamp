#include "ZipCodes.h"
#include <cstdint>
#include <cstring>
namespace {
struct Entry { uint32_t zip; int32_t latitude,longitude; };
const Entry entries[]={
#include "ZipCodeData.h"
};
}
bool zipcodes::valid(const char* zip) {
  if(!zip || strlen(zip)!=5) return false;
  for(unsigned i=0;i<5;++i) if(zip[i]<'0' || zip[i]>'9') return false;
  return true;
}
bool zipcodes::lookup(const char* zip,double& latitude,double& longitude) {
  if(!valid(zip)) return false;
  uint32_t code=0;for(unsigned i=0;i<5;++i)code=code*10+unsigned(zip[i]-'0');
  unsigned lo=0,hi=sizeof(entries)/sizeof(entries[0]);
  while(lo<hi) {unsigned mid=lo+(hi-lo)/2;if(entries[mid].zip<code)lo=mid+1;else hi=mid;}
  if(lo==sizeof(entries)/sizeof(entries[0]) || entries[lo].zip!=code) return false;
  latitude=entries[lo].latitude/100000.0;longitude=entries[lo].longitude/100000.0;
  return true;
}
