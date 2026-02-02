//
// Created by Daniel Sherbakov in 2025.
//

#ifndef VERIFICATION_TYPES_H
#define VERIFICATION_TYPES_H

#include <string>


namespace VerificationMethods {
    enum class Type {
        INVARIANT_STRENGTHENING,
        START_CONDITION_STRENGTHENING
    };
    std::string type_to_string(Type type);
    Type string_to_type(const std::string& type);
}

#endif //VERIFICATION_TYPES_H
