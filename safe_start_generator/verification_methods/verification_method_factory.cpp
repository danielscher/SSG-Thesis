//
// Created by Daniel Sherbakov in 2025.
//

#include "verification_method_factory.h"
#include "../../factories/configuration.h"
#include "../../smt/solver/smt_solver_z3.h"
#include "../../smt_nn/solver/smt_solver_marabou.h"
#include "../../stats/stats_base.h"
#include "invariant_strengthening.h"
#include "start_condition_strengthening.h"

#include <functional>
#include <stdexcept>
#include <utility>

namespace VerificationMethods {
    // creation methods per concrete verification methods:
    std::unique_ptr<VerificationMethod> createInvariantStrengthening(
        const PLAJA::Configuration& config,
        PLAJA::StatsBase& search_statistics,
        StartGenerationStatistics* per_iteration_stats) {
        return std::make_unique<InvariantStrengthening>(config, search_statistics, per_iteration_stats);
    }

    std::unique_ptr<VerificationMethod> createStartConditionStrengthening(
        const PLAJA::Configuration& config,
        PLAJA::StatsBase& search_statistics,
        StartGenerationStatistics* per_iteration_stats) {
        return std::make_unique<StartConditionStrengthening>(config, search_statistics, per_iteration_stats);
    }
    // ============================================================================
    // Factory method:
    std::unique_ptr<VerificationMethod> VerificationMethodFactory::create(
        const Type method,
        const PLAJA::Configuration& config,
        PLAJA::StatsBase& search_statistics,
        StartGenerationStatistics* per_iter_stats) {
        // type aliasing for readability:
        using CreatorFn = std::function<std::unique_ptr<VerificationMethod>(
            const PLAJA::Configuration&,
            PLAJA::StatsBase&,
            StartGenerationStatistics*)>;

        // supported methods
        static const std::unordered_map<Type, CreatorFn> factoryMap = {
            {Type::INVARIANT_STRENGTHENING, createInvariantStrengthening},
            {Type::START_CONDITION_STRENGTHENING, createStartConditionStrengthening}
        };

        auto it = factoryMap.find(method);
        if (it != factoryMap.end()) {
            return it->second(
                config,
                search_statistics,
                per_iter_stats);
        }

        throw std::runtime_error("Unknown VerificationMethod type");
    }
} // namespace VerificationMethods