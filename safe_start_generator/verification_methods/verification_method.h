//
// Created by Daniel Sherbakov in 2025.
//

#ifndef STRENGTHENINGMETHOD_H
#define STRENGTHENINGMETHOD_H

#include "../../parser/ast/expression/expression.h"
#include "../testing/policy_run_sampling.h"
#include <memory>
#include <utility>

class VerificationMethod {
public:
    virtual ~VerificationMethod() = default;
    /// @return a set of unsafe states.
    virtual std::unordered_set<std::unique_ptr<StateBase>> run(const Expression& start, const Expression& unsafety) = 0;
};

#endif //STRENGTHENINGMETHOD_H
