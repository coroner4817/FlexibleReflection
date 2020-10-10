#include <vector>
#include "Reflect.h"

struct Node;
struct Subnode {
    bool flag;
    float value;
    std::vector<Node> siblings;
    std::vector<Subnode> subsubnode;

    REFLECT()       // Enable reflection for this type
};

struct Node {
    std::string key;
    int value;
    std::shared_ptr<Subnode> subnode;
    std::vector<Node> children;

    REFLECT()       // Enable reflection for this type
};

int main() {
    Subnode subnode1 = {true, 1.2345f, {{"orange", 25, nullptr, {}}}, {}};
    Subnode subnode2 = {false, 4.3219f, {}, {{true, 7.234f, {}, {}}}};
    // Create an object of type Node
    Node node = {"apple", 3, std::make_shared<Subnode>(subnode1), {
                    {"banana", 125, nullptr, {
                        {"Hello", 15, std::make_shared<Subnode>(subnode2), {}}
                      }
                    },
                    {"cherry", 11, nullptr, {{}}},
                    {"C++ is a general-purpose programming "
                    "language created by Bjarne Stroustrup as"
                    " an extension of the C programming language, "
                    "or C with Classes.", 131, nullptr, {{}}}
                  }
                };

    // Find Node's type descriptor
    reflect::TypeDescriptor* typeDesc = reflect::TypeResolver<Node>::get();

    // print the readable serialization
    std::stringstream readable;
    typeDesc->dump(&node, readable, true);
    std::cout << readable.str() << std::endl;

    std::cout << "----------------------------------" << std::endl;
    std::stringstream serialize;
    typeDesc->dump(&node, serialize);
    std::cout << serialize.str() << std::endl;

    std::cout << "----------------------------------" << std::endl;
    Node node_deserialized;
    typeDesc->fulfill(&node_deserialized, serialize.str());
    std::stringstream deserilze;
    typeDesc->dump(&node, deserilze);
    std::cout << deserilze.str() << std::endl;

    std::cout << "----------------------------------" << std::endl;
    if(serialize.str() == deserilze.str()){
      std::cout << "Pass!" << std::endl;
    }

    return 0;
}

// Define Node's type descriptor
REFLECT_STRUCT_BEGIN(Node)
REFLECT_STRUCT_MEMBER(key)
REFLECT_STRUCT_MEMBER(value)
REFLECT_STRUCT_MEMBER(subnode)
REFLECT_STRUCT_MEMBER(children)
REFLECT_STRUCT_END()

REFLECT_STRUCT_BEGIN(Subnode)
REFLECT_STRUCT_MEMBER(flag)
REFLECT_STRUCT_MEMBER(value)
REFLECT_STRUCT_MEMBER(siblings)
REFLECT_STRUCT_MEMBER(subsubnode)
REFLECT_STRUCT_END()