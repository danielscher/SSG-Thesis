//
// Created by Daniel Sherbakov in 2025.
//

#ifndef BOX_APPROXIMATION_H
#define BOX_APPROXIMATION_H
#include "../../fd_adaptions/state.h"

/**
 * Computes an overapproximation of a set of states by finding the minimal box containing all state in a given state set.
 */
class BoundingBox {
public:
    static std::pair<double,std::unique_ptr<Expression>> compute_bounding_box(
        const std::unordered_set<std::unique_ptr<StateBase>>& state_set,
        const Model& model);
};

#endif //BOX_APPROXIMATION_H
