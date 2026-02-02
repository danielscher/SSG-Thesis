//
// Created by Daniel Sherbakov in 2025.
//

#ifndef APPROXIMATION_TYPE_H
#define APPROXIMATION_TYPE_H

#include <stdexcept>
#include <string>

namespace Approximation {
    enum class Type {
        Overapproximation,
        Underapproximation,
        None
    };

    inline std::string type_to_string(const Type type) {
        switch (type) {
            case Type::Overapproximation: return "over";
            case Type::Underapproximation: return "under";
            case Type::None: return "none";
            default: throw std::invalid_argument("Unknown approximation type");
        }
    }

    inline Type string_to_type(const std::string& type_str) {
        if (type_str == "over") return Type::Overapproximation;
        if (type_str == "under") return Type::Underapproximation;
        if (type_str == "none") return Type::None;
        throw std::invalid_argument("Invalid approximation type string: " + type_str);
    }
}

#endif //APPROXIMATION_TYPE_H
