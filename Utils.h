#pragma once
#include <string>
#include <cstdio>

namespace reflect {

// format string
template <typename... Ts>
inline std::string FormatStr(const char* fmt, Ts... vs)
{
    size_t required = std::snprintf(nullptr, 0, fmt, std::forward<Ts>(vs)...) + 1;

    char bytes[required];
    std::snprintf(bytes, required, fmt, std::forward<Ts>(vs)...);

    return std::string(bytes);
}

// helper function to parse byte array to the primitive type
template <typename T>
inline T ParseAs(const std::string& str)
{
  if(str.empty()) return T();
  T val = *(reinterpret_cast<T*>(const_cast<char*>(str.c_str())));
  return val;
}

// have to use inline here because pragma once not guard this
inline std::string GetRootContent(const std::string& data)
{
  size_t st = data.find("{");
  size_t ed = data.rfind("}");
  return data.substr(st+1, ed-st-1);
}

}