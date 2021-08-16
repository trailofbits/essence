#pragma once
#include <string>
#include <memory>
#include "DeclarationManager.h"
namespace handsanitizer{


class JsonOutputGenerator {

public:
    JsonOutputGenerator(std::shared_ptr<DeclarationManager> dm): declarationManager(dm){};
    std::string getJsonOutputText(std::string output_var_name, handsanitizer::Type* retType);
    std::string getJsonOutputForType(std::string json_name, std::vector<std::string> prefixes, handsanitizer::Type* type, bool skipRoot = false);



private:
    std::shared_ptr<DeclarationManager> declarationManager;
};

}



