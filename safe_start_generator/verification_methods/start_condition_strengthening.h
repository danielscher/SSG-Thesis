//
// Created by Daniel Sherbakov in 2025.
//

#ifndef START_CONDITION_STRENGTHENING_H
#define START_CONDITION_STRENGTHENING_H
#include "verification_method.h"
#include "../../predicate_abstraction/cegar/pa_cegar.h"
#include "../start_generation_statistics.h"
#include "../../search/information/property_information.h"



namespace VerificationMethods {

    class StartConditionStrengthening: public VerificationMethod {
    private:
        std::unique_ptr<PACegar> pa_cegar;

        PLAJA::StatsBase& search_stats;
        StartGenerationStatistics* per_iteration_stats;

        const PLAJA::Configuration& config;
        std::unique_ptr<PropertyInformation> sub_prop_info;

        void init_pa_cegar(const Expression& start);

    public:
        StartConditionStrengthening(
            const PLAJA::Configuration& config,
            PLAJA::StatsBase& search_statistics,
            StartGenerationStatistics* per_iteration_statistics);

        std::unordered_set<std::unique_ptr<StateBase>> run(const Expression& start, const Expression& unsafety) override;
    };
} // namespace VerificationMethods

#endif //START_CONDITION_STRENGTHENING_H
