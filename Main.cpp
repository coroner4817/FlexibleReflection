#include <vector>
#include "Reflect.h"

struct Node {
    std::string key;
    int value;
    std::shared_ptr<Node> subnode;
    std::vector<Node> children;

    REFLECT()       // Enable reflection for this type
};

int main() {
    Node subnode1 = {"foo", 33, nullptr, {}};
    Node subnode2 = {"bar", 47, nullptr, {}};
    // Create an object of type Node
    Node node = {"apple", 3, nullptr, {
                    {"banana", 7, std::make_shared<Node>(subnode1), {
                        {"Hello", 15, std::make_shared<Node>(subnode2), {}}
                      }
                    }, 
                    {"cherry", 11, nullptr, {}}
                  }
                };

    // Find Node's type descriptor
    reflect::TypeDescriptor* typeDesc = reflect::TypeResolver<Node>::get();

    // Dump a description of the Node object to the console
    std::stringstream ss;
    typeDesc->dump(&node, ss);

    std::cout << ss.str() << std::endl;

    return 0;
}

// Define Node's type descriptor
REFLECT_STRUCT_BEGIN(Node)
REFLECT_STRUCT_MEMBER(key)
REFLECT_STRUCT_MEMBER(value)
REFLECT_STRUCT_MEMBER(subnode)
REFLECT_STRUCT_MEMBER(children)
REFLECT_STRUCT_END()
