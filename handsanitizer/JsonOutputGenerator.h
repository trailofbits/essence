#pragma once
#include <string>
#include <memory>
#include "DeclarationManager.h"
namespace handsanitizer{


class JsonOutputGenerator {

public:
    JsonOutputGenerator(std::shared_ptr<DeclarationManager> dm): declarationManager(dm){};
    std::string getJsonOutputText(const std::string& output_var_name, handsanitizer::Type* retType);
    std::string getJsonOutputForType(const std::string& json_name, const std::vector<std::string>& prefixes, handsanitizer::Type* type, bool skipRoot = false, bool isArrayValue = false);



private:
    std::shared_ptr<DeclarationManager> declarationManager;
};

}



