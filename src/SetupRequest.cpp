#include "SetupRequest.h"
#include <cstring>

void SetupRequest::reset() {
  memset(headers_,0,sizeof(headers_)); memset(body,0,sizeof(body)); memset(token,0,sizeof(token));
  headerLength_ = expected_ = bodyLength = 0;
  headersDone_ = complete = false; error = 0; route = Route::NotFound;
}
static bool same(const char* a, const char* b) {
  for (; *a && *b; ++a, ++b) {
    char x = (*a >= 'A' && *a <= 'Z') ? char(*a+32) : *a;
    if (x != *b) return false;
  }
  return *a == *b;
}
void SetupRequest::parseHeaders() {
  char* lineEnd = strstr(headers_, "\r\n");
  if (!lineEnd || lineEnd-headers_ > 127) { error=400; return; }
  *lineEnd = 0;
  if (!strcmp(headers_,"GET / HTTP/1.1")) route=Route::Page;
  else if (!strcmp(headers_,"GET /session HTTP/1.1")) route=Route::Session;
  else if (!strcmp(headers_,"POST /configure HTTP/1.1")) route=Route::Configure;
  bool lengthSeen=false, typeSeen=false, tokenSeen=false, hostSeen=false;
  for (char* line = lineEnd+2; *line; line = lineEnd+2) {
    lineEnd = strstr(line,"\r\n");
    if (!lineEnd) { error=400; return; }
    if (lineEnd==line) break;
    *lineEnd=0;
    char* colon = strchr(line,':');
    if (!colon || line[0]==' ' || line[0]=='\t') { error=400; return; }
    *colon=0; char* value=colon+1;
    while (*value==' ' || *value=='\t') ++value;
    char* end=value+strlen(value);
    while (end>value && (end[-1]==' ' || end[-1]=='\t')) *--end=0;
    if (same(line,"transfer-encoding")) { error=400; return; }
    if (same(line,"host")) {
      if (hostSeen || (strcmp(value,"192.168.4.1") && strcmp(value,"192.168.4.1:80"))) { error=400; return; }
      hostSeen=true;
    } else if (same(line,"origin")) {
      if (strcmp(value,"http://192.168.4.1") && strcmp(value,"http://192.168.4.1:80")) { error=403; return; }
    } else if (same(line,"content-length")) {
      if (lengthSeen || !*value) { error=400; return; }
      lengthSeen=true;
      for (char* p=value; *p; ++p) {
        if (*p<'0' || *p>'9') { error=400; return; }
        expected_ = expected_*10 + unsigned(*p-'0');
        if (expected_>MaxConfigBytes) { error=413; return; }
      }
    } else if (same(line,"content-type")) {
      if (typeSeen || !same(value,"application/json")) { error=415; return; }
      typeSeen=true;
    } else if (same(line,"x-setup-token")) {
      if (tokenSeen || strlen(value)!=32) { error=403; return; }
      tokenSeen=true; memcpy(token,value,33);
    }
  }
  if (!hostSeen) { error=400; return; }
  if (route==Route::Configure) {
    if (!lengthSeen || expected_==0 || !typeSeen || !tokenSeen) { error=400; return; }
  } else if (expected_) { error=400; return; }
  headersDone_=true;
  complete = expected_==0;
}
void SetupRequest::feed(char ch) {
  if (complete || error) return;
  if (!ch) { error=400; return; }
  if (!headersDone_) {
    if (headerLength_==sizeof(headers_)-1) { error=431; return; }
    if (static_cast<unsigned char>(ch)<32 && ch!='\r' && ch!='\n' && ch!='\t') { error=400; return; }
    headers_[headerLength_++]=ch;
    if (headerLength_>=4 && !memcmp(headers_+headerLength_-4,"\r\n\r\n",4)) parseHeaders();
  } else {
    body[bodyLength++]=ch;
    complete=bodyLength==expected_;
  }
}
