//
// Created by Daniel Sherbakov in 2025.
//

#ifndef STRENGTHENING_METHOD_FACTORY_H
#define STRENGTHENING_METHOD_FACTORY_H

#include "start_condition_strengthening.h"
#include "verification_method.h"
#include "verification_types.h"

#include <memory>

class InitialStatesEnumerator;
namespace VerificationMethods {

    class VerificationMethodFactory {
    public:
        static std::unique_ptr<VerificationMethod> create(
            Type method,
            const PLAJA::Configuration& config,
            PLAJA::StatsBase& search_statistics,
            StartGenerationStatistics* per_iter_stats);
    };
} // namespace VerificationMethodFactory

#endif //STRENGTHENING_METHOD_FACTORY_H
