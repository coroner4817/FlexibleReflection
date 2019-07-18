#include "Reflect.h"

using namespace std;


namespace reflect {


#define METAPROGRAMMING(type)\
struct TypeDescriptor_##type : TypeDescriptor {\
  TypeDescriptor_##type() : TypeDescriptor{#type, sizeof(type)} {\
  }\
  void dump(const void* obj, std::stringstream& ss, int /* unused */) const override {\
    ss << #type << "{" << *(const type*)obj << "}";\
  }\
};\
template<>\
TypeDescriptor* getPrimitiveDescriptor<type>(){\
  static TypeDescriptor_##type typeDesc;\
  return &typeDesc;\
}

METAPROGRAMMING(int)
METAPROGRAMMING(string)

} // namespace reflect
