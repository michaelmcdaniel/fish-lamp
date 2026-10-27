#pragma once
#include "Config.h"

// One bounded, non-pipelined HTTP request. Feed only bytes already available.
class SetupRequest {
 public:
  enum class Route { Page, Status, Session, Networks, Configure, Redirect, NotFound };
  void reset(bool station = false);
  void feed(char ch);
  bool complete = false;
  int error = 0;
  Route route = Route::NotFound;
  char token[33]{};
  char body[MaxConfigBytes + 1]{};
  size_t bodyLength = 0;
 private:
  void parseHeaders();
  char headers_[2049]{};
  size_t headerLength_ = 0, expected_ = 0;
  bool headersDone_ = false;
  bool station_ = false;
};
