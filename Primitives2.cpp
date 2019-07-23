#include "Reflect.h"
#include <type_traits>

using namespace std;

namespace reflect {

#define METAPROGRAMMING(type)\
struct TypeDescriptor_##type : TypeDescriptor {\
  TypeDescriptor_##type() : TypeDescriptor{#type, sizeof(type)} {\
  }\
  void dump(const void* obj, std::stringstream& ss, bool readable, int /* unused */) const override {\
    /* Convert to byte array, not human readable */\
    if(readable){\
      ss << #type << "{" << *(const type*)obj << "}";\
    }else{\
      if(std::is_same<type, string>::value){\
        ss << #type << "{" << *(const string*)obj << "}";\
      }else{\
        auto p = reinterpret_cast<const char*>(obj);\
        ss << #type << "{" << string(p, sizeof(type)) << "}";\
      }\
    }\
  }\
  void fulfill(void* obj, const std::string& data, int /* unused */) const override{\
    if(std::is_same<type, string>::value){\
      *(string*)obj = data;\
    }else{\
      *(type*)obj = ParseAs<type>(data);\
    }\
  }\
};\
template<>\
TypeDescriptor* getPrimitiveDescriptor<type>(){\
  static TypeDescriptor_##type typeDesc;\
  return &typeDesc;\
}

METAPROGRAMMING(int)
METAPROGRAMMING(bool)
METAPROGRAMMING(float)
METAPROGRAMMING(double)
METAPROGRAMMING(char)
METAPROGRAMMING(string)

} // namespace reflect
