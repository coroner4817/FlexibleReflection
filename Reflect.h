#pragma once

#include <vector>
#include <iostream>
#include <string>
#include <cstddef>
#include <sstream>
#include <memory>
#include <functional>

namespace reflect {

//--------------------------------------------------------
// Base class of all type descriptors
//--------------------------------------------------------

struct TypeDescriptor {
    const char* name;
    size_t size;

    TypeDescriptor(const char* name, size_t size) : name{name}, size{size} {}
    virtual ~TypeDescriptor() {}
    virtual std::string getFullName() const { return name; }
    virtual void dump(const void* obj, std::stringstream& ss, int indentLevel = 0) const = 0;
};

//--------------------------------------------------------
// Finding type descriptors
//--------------------------------------------------------

// Declare the function template that handles primitive types such as int, std::string, etc.:
template <typename T>
TypeDescriptor* getPrimitiveDescriptor();

// A helper class to find TypeDescriptors in different ways:
struct DefaultResolver {
    template <typename T> static char func(decltype(&T::Reflection));
    template <typename T> static int func(...);
    template <typename T>
    struct IsReflected {
        // YW - if T has a static member named "Reflection", value will be true
        //      because the func<T>(nullptr) can match both declaration above
        //      At runtime, only when T::Reflection exist, will use the first func
        //      And also func(decltype(&T::Reflection)) return char, so value is true
        enum { value = (sizeof(func<T>(nullptr)) == sizeof(char)) };
    };

    // This version is called if T has a static member named "Reflection":
    // YW - if IsReflected<T>::value if true then this will be compiled
    //      This is determine at the compiling time
    //      enable<bool B, class T>, if bool B is true then the following definition get class T typename
    //      type is the int class type, need to assign default value to 0 if the B is false, the default value can be any number
    //      typename std::enable_if<IsReflected<T>::value, int>::type = 0 is called SFINAE
    template <typename T, typename std::enable_if<IsReflected<T>::value, int>::type = 0>
    static TypeDescriptor* get() {
        // YW - in the vector case, here T = Node, so it will return a TypeDescriptor_Struct
        //      which is also a TypeDescriptor
        return &T::Reflection;
    }

    // This version is called otherwise:
    template <typename T, typename std::enable_if<!IsReflected<T>::value, int>::type = 0>
    static TypeDescriptor* get() {
        return getPrimitiveDescriptor<T>();
    }
};

// This is the primary class template for finding all TypeDescriptors:
template <typename T>
struct TypeResolver {
    static TypeDescriptor* get() {
        return DefaultResolver::get<T>();
    }
};

//--------------------------------------------------------
// Type descriptors for user-defined structs/classes
//--------------------------------------------------------
// YW - type descriptor for the Node struct
//      Also for use in the outside to serilize and deserilize the struct
struct TypeDescriptor_Struct : TypeDescriptor {
    struct Member {
        const char* name;
        size_t offset;
        TypeDescriptor* type;
    };

    std::vector<Member> members;

    // YW - following marco use this to construct
    //      when declare a static variable, the constructor is not get called
    //      constructor get called when use it for the first time
    TypeDescriptor_Struct(void (*init)(TypeDescriptor_Struct*)) : TypeDescriptor{nullptr, 0} {
        init(this);
    }
    TypeDescriptor_Struct(const char* name, size_t size, const std::initializer_list<Member>& init) : TypeDescriptor{nullptr, 0}, members{init} {
    }
    virtual void dump(const void* obj, std::stringstream& ss, int indentLevel) const override {
        ss << name << " {" << std::endl;
        for (const Member& member : members) {
            ss << std::string(4 * (indentLevel + 1), ' ') << member.name << " = ";
            // YW - find the member base on offset. So this reflection design only work for struct  
            member.type->dump((char*) obj + member.offset, ss, indentLevel + 1);
            ss << std::endl;
        }
        ss << std::string(4 * indentLevel, ' ') << "}";
    }
};

#define REFLECT() \
    friend struct reflect::DefaultResolver; \
    static reflect::TypeDescriptor_Struct Reflection; \
    static void initReflection(reflect::TypeDescriptor_Struct*);

#define REFLECT_STRUCT_BEGIN(type) \
    reflect::TypeDescriptor_Struct type::Reflection{type::initReflection}; \
    void type::initReflection(reflect::TypeDescriptor_Struct* typeDesc) { \
        using T = type; \
        typeDesc->name = #type; \
        typeDesc->size = sizeof(T); \
        typeDesc->members = {

#define REFLECT_STRUCT_MEMBER(name) \
            {#name, offsetof(T, name), reflect::TypeResolver<decltype(T::name)>::get()},

#define REFLECT_STRUCT_END() \
        }; \
    }

//--------------------------------------------------------
// Type descriptors for std::vector
//--------------------------------------------------------
// YW - type descriptor for the std::vector containor relationship
struct TypeDescriptor_StdVector : TypeDescriptor {
    TypeDescriptor* itemType;
    size_t (*getSize)(const void*);
    const void* (*getItem)(const void*, size_t);
    
    // YW - this ItemType will be Node
    template <typename ItemType>
    TypeDescriptor_StdVector(ItemType*)
        : TypeDescriptor{"std::vector<>", sizeof(std::vector<ItemType>)},
                         itemType{TypeResolver<ItemType>::get()} {
        getSize = [](const void* vecPtr) -> size_t {
            const auto& vec = *(const std::vector<ItemType>*) vecPtr;
            return vec.size();
        };
	      // YW - this require the data type in the vector to be struct
        getItem = [](const void* vecPtr, size_t index) -> const void* {
            const auto& vec = *(const std::vector<ItemType>*) vecPtr;
            return &vec[index];
        };
    }
    virtual std::string getFullName() const override {
        return std::string("std::vector<") + itemType->getFullName() + ">";
    }
    virtual void dump(const void* obj, std::stringstream& ss, int indentLevel) const override {
        size_t numItems = getSize(obj);
        ss << getFullName();
        if (numItems == 0) {
            ss << "{}";
        } else {
            ss << "{" << std::endl;
            for (size_t index = 0; index < numItems; index++) {
                ss << std::string(4 * (indentLevel + 1), ' ') << "[" << index << "] ";
                itemType->dump(getItem(obj, index), ss, indentLevel + 1);
                ss << std::endl;
            }
            ss << std::string(4 * indentLevel, ' ') << "}";
        }
    }
};

// Partially specialize TypeResolver<> for std::vectors:
// YW - match for the std::vector and also extrat the container data type 
//      So the T here is the Node not the std::vector<Node>
//      Any other container class including smart pointer should following this way
template <typename T>
class TypeResolver<std::vector<T>> {
public:
    static TypeDescriptor* get() {
        // YW - we can make the struct a template struct and use TypeDescriptor_StdVector<T> here instead
        //      In the current code it will auto match the ItemType to T
        static TypeDescriptor_StdVector typeDesc{(T*) nullptr};
        return &typeDesc;
    }
};


struct TypeDescriptor_StdSharedPtr : TypeDescriptor{
  TypeDescriptor* itemType;
  std::function<const void*(const void*)> getRawPtr;

  template <typename ItemType>
  TypeDescriptor_StdSharedPtr(ItemType*) 
    : TypeDescriptor{"std::shared_ptr<>", sizeof(std::shared_ptr<ItemType>)},
                      itemType{TypeResolver<ItemType>::get()} {
    getRawPtr = [](const void* obj) -> const void* {
      const auto& ptr = *(const std::shared_ptr<ItemType>*)obj;
      return ptr.get();
    };      
  }
  virtual std::string getFullName() const override {
    return std::string("std::shared_ptr<") + itemType->getFullName() + ">";
  }

  virtual void dump(const void* obj, std::stringstream& ss, int indentLevel) const override {
    ss << getFullName();
    if(!getRawPtr(obj)){
      ss << "{}";
    }else{
      ss << "{" << std::endl;
      ss << std::string(4 * (indentLevel + 1), ' ');
      itemType->dump(getRawPtr(obj), ss, indentLevel + 1);
      ss << std::endl;
      ss << std::string(4 * indentLevel, ' ') << "}";
    }
  }
};

template <typename T>
class TypeResolver<std::shared_ptr<T>> {
public:
  static TypeDescriptor* get() {
    static TypeDescriptor_StdSharedPtr typeDesc{(T*) nullptr};
    return &typeDesc; 
  }
};

} // namespace reflect
