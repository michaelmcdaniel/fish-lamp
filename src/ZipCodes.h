#pragma once
namespace zipcodes {
constexpr double FallbackLatitude=35.84562, FallbackLongitude=-86.39027;
bool valid(const char* zip);
bool lookup(const char* zip,double& latitude,double& longitude);
}
