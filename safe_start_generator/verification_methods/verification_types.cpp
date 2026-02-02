//
// Created by Daniel Sherbakov in 2025.
//

#include "verification_types.h"
#include <stdexcept>
#include <string>

namespace VerificationMethods {

    std::string type_to_string(Type type) {
        switch (type) {
            case Type::INVARIANT_STRENGTHENING: return "inv_str";
            case Type::START_CONDITION_STRENGTHENING: return "scs";
            default: throw std::invalid_argument("Unknown verification method");
        }
    }

    Type string_to_type(const std::string& type_str) {
        if (type_str == "inv_str") return Type::INVARIANT_STRENGTHENING;
        if (type_str == "scs") return Type::START_CONDITION_STRENGTHENING;
        throw std::invalid_argument("Invalid verification method string: " + type_str);
    }
}