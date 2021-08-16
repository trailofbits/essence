#include "handsan.hpp"

namespace handsanitizer {
    std::string getUnrolledTypeAsJson(Type &type) {
        std::stringstream output;
        output << "{" << std::endl;
        output << "\"type\": \"" << type.getTypeName() << "\"";
        if (type.isVoidTy()) {
            // do nothing
        }

        if (type.isPointerTy())
            output << ", \"pointerElementType\": " << getUnrolledTypeAsJson(*type.getPointerElementType());

        if (type.isIntegerTy())
            output << ", \"bitWidth\": " << type.getBitWidth();

        if (type.isStructTy()) {
            output << ", \"structName\": \"" << type.getCTypeName() << "\", \"structMembers\": [";
            for (auto &member : type.getNamedMembers()) {
                output << "{";
                output << "\"" << member.getName() << "\"" << ": " << getUnrolledTypeAsJson(*member.getType());
                output << "}";
                if (member.getName() != type.getNamedMembers().back().name)
                    output << ",";
            }
            output << "]";
        }
        if (type.isArrayTy()) {
            output << ", \"getArrayNumElements\": " << type.getArrayNumElements() << ", \"arrayElementType\":"
                   << getUnrolledTypeAsJson(*type.getArrayElementType());
        }

        output << "}" << std::endl;

        return output.str();
    }
};