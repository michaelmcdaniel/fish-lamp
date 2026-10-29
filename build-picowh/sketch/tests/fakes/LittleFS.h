#line 1 "C:\\Users\\mmcda\\OneDrive\\Code\\pico\\fish-lamp\\tests\\fakes\\LittleFS.h"
#pragma once
#include <string>
#include <map>
#include <memory>
#include <cstring>
#include <algorithm>
#include <stdint.h>

struct File {
  std::shared_ptr<std::string> contents;
  size_t position=0;
  bool shortWrite=false;
  explicit operator bool() const { return bool(contents); }
  size_t size() const { return contents ? contents->size() : 0; }
  size_t write(const uint8_t* data,size_t length) {
    if (!contents) return 0;
    size_t n=shortWrite ? length/2 : length;
    contents->assign(reinterpret_cast<const char*>(data),n); return n;
  }
  size_t readBytes(char* out,size_t length) {
    if (!contents) return 0;
    size_t n=std::min(length,contents->size()-position);
    memcpy(out,contents->data()+position,n); position+=n; return n;
  }
  int read() { return contents && position<contents->size() ? static_cast<unsigned char>((*contents)[position++]) : -1; }
  void flush() {}
  void close() { contents.reset(); }
};
struct LittleFSConfig { bool autoFormat=true; void setAutoFormat(bool value) { autoFormat=value; } };
struct FakeFS {
  std::map<std::string,std::shared_ptr<std::string>> files;
  bool mountOK=true, shortWrite=false, formatOK=true, autoFormat=true;
  unsigned formats=0, renames=0, failRenameAt=0;
  void setConfig(LittleFSConfig config) { autoFormat=config.autoFormat; }
  bool begin() { return mountOK; }
  bool format() { ++formats; if(!formatOK)return false; files.clear(); mountOK=true; return true; }
  File open(const char* path,const char* mode) {
    if (!mountOK) return {};
    if (*mode=='w') files[path]=std::make_shared<std::string>();
    auto it=files.find(path);
    return it==files.end() ? File{} : File{it->second,0,shortWrite};
  }
  bool rename(const char* from,const char* to) {
    if (++renames==failRenameAt) return false;
    auto it=files.find(from); if(it==files.end())return false;
    files[to]=it->second; files.erase(it); return true;
  }
};
inline FakeFS LittleFS;
