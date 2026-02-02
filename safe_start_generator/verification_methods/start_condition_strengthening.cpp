//
// Created by Daniel Sherbakov in 2025.
//

#include "start_condition_strengthening.h"

#include "../../../globals.h"
#include "../../../option_parser/plaja_options.h"
#include "../../factories/predicate_abstraction/pa_factory.h"
#include "../../fd_adaptions/timer.h"
#include "../../search/factories/predicate_abstraction/search_engine_config_pa.h"

namespace VerificationMethods{
StartConditionStrengthening::StartConditionStrengthening(
    const PLAJA::Configuration& config,
    PLAJA::StatsBase& search_statistics,
    StartGenerationStatistics* per_iteration_statistics):
    search_stats(search_statistics),
    per_iteration_stats(per_iteration_statistics),
    config(config){}

std::unordered_set<std::unique_ptr<StateBase>> StartConditionStrengthening::run(const Expression& start, const Expression& unsafety) {
    init_pa_cegar(start);
    PUSH_LAP(&search_stats, PLAJA::StatsDouble::TOTAL_VERIFICATION_TIME);
    PUSH_LAP_IF(per_iteration_stats, PLAJA::StatsDouble::SEARCHING_TIME);
    pa_cegar->search();
    POP_LAP(&search_stats, PLAJA::StatsDouble::TOTAL_VERIFICATION_TIME);
    POP_LAP_IF(per_iteration_stats, PLAJA::StatsDouble::SEARCHING_TIME);
    if (pa_cegar->is_safe) {
        return {};
    }
    if (pa_cegar->get_status() == SearchEngine::FINISHED) {
        PLAJA_LOG("Extracting unsafe path ... ")
        return pa_cegar->extract_concrete_unsafe_path();
    }
    PLAJA_LOG(PLAJA_UTILS::to_red_string("PA CEGAR TERMINATED WITHOUT SOLVING"))
    PLAJA_ABORT
}

/// Initializes structures with new start condition.
void StartConditionStrengthening::init_pa_cegar(const Expression& start) {
    // MODEL_Z3 initialized in InitialStateEnumerator and shared however pa_cegar requires a MODELZ3PA model
    // therefore the config is copied and we delete the model z3 from the shared objects.
    auto subconfig(config);
    const auto model = PLAJA_GLOBAL::currentModel;
    sub_prop_info = PropertyInformation::analyse_property(*model->get_property(1), *model);
    sub_prop_info->set_start(&start);
    subconfig.delete_sharable(PLAJA::SharableKey::MODEL_Z3);
    subconfig.delete_sharable(PLAJA::SharableKey::PROP_INFO);
    subconfig.set_sharable_const(PLAJA::SharableKey::PROP_INFO,sub_prop_info.get());
    pa_cegar = std::make_unique<PACegar>(PLAJA_UTILS::cast_ref<SearchEngineConfigPA>(subconfig));
}

}
