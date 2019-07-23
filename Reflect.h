#pragma once

#include <vector>
#include <iostream>
#include <string>
#include <cstddef>
#include <sstream>
#include <memory>
#include <regex>
#include <functional>
#include "Utils.h"

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
    virtual void dump(const void* obj, std::stringstream& ss, bool readable = false, int indentLevel = 0) const = 0;
    virtual void fulfill(void* obj, const std::string& data, int indentLevel = 0) const = 0;
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
    virtual void dump(const void* obj, std::stringstream& ss, bool readable, int indentLevel) const override {
        ss << name << " {" << std::endl;
        for (const Member& member : members) {
            ss << std::string(4 * (indentLevel + 1), ' ') << member.name << " = ";
            // YW - find the member base on offset. So this reflection design only work for struct  
            member.type->dump((char*) obj + member.offset, ss, readable, indentLevel + 1);
            ss << std::endl;
        }
        ss << std::string(4 * indentLevel, ' ') << "}";
    }
    virtual void fulfill(void* obj, const std::string& data, int indentLevel) const override {
      // obj here is already allocated
      std::string indent = "\n" + std::string(4 * (indentLevel + 1), ' ');
      std::string data_ = GetRootContent(data);
      size_t curNeedle = data_.find(FormatStr("%s%s = %s", indent.c_str(), members.front().name, members.front().type->getFullName().c_str())) + 1;
      for(size_t i = 0; i < members.size(); ++i){
        size_t nextNeedle;
        if(i == members.size() - 1){
          nextNeedle = data_.size();
        }else{
          nextNeedle = data_.find(FormatStr("%s%s = %s", indent.c_str(), members[i+1].name, members[i+1].type->getFullName().c_str())) + 1;
        }
        // find between cur - next        
        std::string content = GetRootContent(data_.substr(curNeedle, nextNeedle-curNeedle));
        members[i].type->fulfill((char*) obj + members[i].offset, content, indentLevel + 1);

        curNeedle = nextNeedle;
      }

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
    std::function<void(void*&, size_t)> instantiate;
    std::function<void*(void*, size_t)> getRawItem;

    // YW - this ItemType will be Node
    template <typename ItemType>
    TypeDescriptor_StdVector(ItemType*)
        : TypeDescriptor{"std::vector<>", sizeof(std::vector<ItemType>)},
                         itemType{TypeResolver<ItemType>::get()} {
        getSize = [](const void* vecPtr) -> size_t {
            const auto& vec = *(const std::vector<ItemType>*) vecPtr;
            return vec.size();
        };
        getItem = [](const void* vecPtr, size_t index) -> const void* {
            const auto& vec = *(const std::vector<ItemType>*) vecPtr;
            return &vec[index];
        };

        instantiate = [](void*& obj, size_t sz) -> void{
          auto& vec = *(std::vector<ItemType>*)obj;
          vec.resize(sz);
        };
        // todo: use the template lambda with c++20
        getRawItem = [](void* vecPtr, size_t index) -> void* {
            auto& vec = *(std::vector<ItemType>*) vecPtr;
            return &vec[index];
        };

    }
    virtual std::string getFullName() const override {
        return std::string("std::vector<") + itemType->getFullName() + ">";
    }
    virtual void dump(const void* obj, std::stringstream& ss, bool readable, int indentLevel) const override {
        size_t numItems = getSize(obj);
        ss << getFullName();
        if (numItems == 0) {
            ss << "{}";
        } else {
            ss << "{" << std::endl;
            for (size_t index = 0; index < numItems; index++) {
                ss << std::string(4 * (indentLevel + 1), ' ') << "[" << index << "] ";
                itemType->dump(getItem(obj, index), ss, readable, indentLevel + 1);
                ss << std::endl;
            }
            ss << std::string(4 * indentLevel, ' ') << "}";
        }
    }
    virtual void fulfill(void* obj, const std::string& data, int indentLevel) const override {
      // std::cout << data << std::endl;
      if(data.empty()){
        instantiate(obj, 0);
      }else{
        // WTF with c++ regex?
        // std::string regstr = R"(^\s{)" + std::to_string((indentLevel + 1) * 4) + R"(}\[\d\]\s)" + itemType->getFullName();
        // std::regex reg(regstr);
        // std::smatch matches;
        // std::regex_match(data, matches, reg);

        std::string indent = "\n" + std::string(4 * (indentLevel + 1), ' ');
        std::vector<std::string> items;
        size_t lastPos = 0;
        size_t count = 0;
        size_t pos;
        while((pos = data.find(FormatStr("%s[%d] %s", indent.c_str(), count, itemType->getFullName().c_str()))) 
                  != std::string::npos){
          if(count) items.push_back(data.substr(lastPos, pos-lastPos));
          lastPos = pos;
          count++;
        }
        items.push_back(data.substr(lastPos));        

        instantiate(obj, items.size());
        for(size_t i = 0; i < items.size(); ++i){
          itemType->fulfill(getRawItem(obj, i), items[i], indentLevel+1);
        }
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
  std::function<void*(void*&)> instantiate;

  template <typename ItemType>
  TypeDescriptor_StdSharedPtr(ItemType*) 
    : TypeDescriptor{"std::shared_ptr<>", sizeof(std::shared_ptr<ItemType>)},
                      itemType{TypeResolver<ItemType>::get()} {
    getRawPtr = [](const void* obj) -> const void* {
      const auto& ptr = *(const std::shared_ptr<ItemType>*)obj;
      return ptr.get();
    };      
    instantiate = [](void*& obj) -> void* {
      auto& ptr = *(std::shared_ptr<ItemType>*)obj;
      ptr = std::make_shared<ItemType>();
      return ptr.get();
    };
  }
  virtual std::string getFullName() const override {
    return std::string("std::shared_ptr<") + itemType->getFullName() + ">";
  }

  virtual void dump(const void* obj, std::stringstream& ss, bool readable, int indentLevel) const override {
    ss << getFullName();
    if(!getRawPtr(obj)){
      ss << "{}";
    }else{
      ss << "{" << std::endl;
      ss << std::string(4 * (indentLevel + 1), ' ');
      itemType->dump(getRawPtr(obj), ss, readable, indentLevel + 1);
      ss << std::endl;
      ss << std::string(4 * indentLevel, ' ') << "}";
    }
  }
  virtual void fulfill(void* obj, const std::string& data, int indentLevel) const override {
    // obj here the contaioner is already allocated but the data is not
    if(data.empty()){
      void* rawObj = instantiate(obj);
      rawObj = nullptr;
    }else{
      void* rawObj = instantiate(obj);
      itemType->fulfill(rawObj, data, indentLevel + 1);
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
