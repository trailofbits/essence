#include "handsan.hpp"

namespace handsanitizer {
    std::string getUnrolledTypeAsJson(Type &type, std::vector<std::pair<Type *, std::string>> typePath) {
        std::stringstream output;
        output << "{" << std::endl;
        output << "\"type\": \"" << type.getTypeName() << "\"";
        if (type.isVoidTy()) {
            // do nothing
        }

        if (type.isPointerTy())
            output << ", \"pointerElementType\": " << getUnrolledTypeAsJson(*type.getPointerElementType(), typePath);

        if (type.isIntegerTy())
            output << ", \"bitWidth\": " << type.getBitWidth();

        if (type.isStructTy()) {
            output << ", \"structName\": \"" << type.getCTypeName() << "\", \"structMembers\": [";
            for (auto &member : type.getNamedMembers()) {
                output << "{";

                std::vector<std::pair<Type *, std::string>>::iterator pathToPreviousUnrollingOfType;
                pathToPreviousUnrollingOfType = std::find_if(typePath.begin(), typePath.end(),
                                                             [member](std::pair<Type *, std::string> pair) { return member.type == pair.first; });
                if(pathToPreviousUnrollingOfType != typePath.end()){
                    // member type is already found
                    std::stringstream pathToFirstOccurrence;
                    for(auto& path : typePath)
                        pathToFirstOccurrence << "[\"" << path.second << "\"]";

                    output << "\"" << member.getName() << "\": \'cycles_with_" << pathToFirstOccurrence.str() << "\'";
                }
                else{
                    auto memberTypePath(typePath);
                    memberTypePath.push_back(std::pair<Type*, std::string>(member.getType(), member.getName()));
                    output << "\"" << member.getName() << "\"" << ": " << getUnrolledTypeAsJson(*member.getType(), memberTypePath);
                }


                output << "}";
                if (member.getName() != type.getNamedMembers().back().name)
                    output << ",";
            }
            output << "]";
        }
        if (type.isArrayTy()) {
            output << ", \"getArrayNumElements\": " << type.getArrayNumElements() << ", \"arrayElementType\":"
            << getUnrolledTypeAsJson(*type.getArrayElementType(), typePath);
        }

        output << "}" << std::endl;

        return output.str();
    }
};