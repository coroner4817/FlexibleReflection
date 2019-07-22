#include "Reflect.h"

using namespace std;

namespace reflect {


#define METAPROGRAMMING(type)\
struct TypeDescriptor_##type : TypeDescriptor {\
  TypeDescriptor_##type() : TypeDescriptor{#type, sizeof(type)} {\
  }\
  void dump(const void* obj, std::stringstream& ss, int /* unused */) const override {\
    /* Convert to byte array, not human readable */\
    auto p = reinterpret_cast<const char*>(obj);\
    ss << #type << "{" << string(p, sizeof(type)) << "}";\
  }\
  void fulfill(void* obj, const std::string& data, int /* unused */) const override{\
    *(type*)obj = ParseAs<type>(data);\
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
