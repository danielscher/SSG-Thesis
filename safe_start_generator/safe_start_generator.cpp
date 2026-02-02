//
// Created by Daniel Sherbakov in 2025.
//

#include "safe_start_generator.h"

#include "../../parser/visitor/to_normalform.h"
#include "../../stats/stats_base.h"
#include "../../stats/stats_unsigned.h"
#include "../factories/configuration.h"
#include "../factories/safe_start_generator/safe_start_generator_options.h"
#include "../fd_adaptions/timer.h"
#include "../information/property_information.h"
#include "../predicate_abstraction/smt/model_z3_pa.h"
#include "approximation_methods/bounding_box.h"
#include "start_generation_statistics.h"
#include "verification_methods/invariant_strengthening.h"
#include "verification_methods/verification_method_factory.h"

SafeStartGenerator::SafeStartGenerator(const PLAJA::Configuration& config):
    SearchEngine(config),
    config(config),
    verification_type(
        VerificationMethods::string_to_type(config.get_value_option_string(PLAJA_OPTION::verification_method))),
    alternating_mode(config.is_flag_set(PLAJA_OPTION::alternate)) {
    // init statistics.
    StartGenerationStatistics::add_basic_stats(*searchStatistics);
    if (config.has_value_option(PLAJA_OPTION::iteration_stats)) {
        per_iteration_stats =
            std::make_unique<StartGenerationStatistics>(config.get_value_option_string(PLAJA_OPTION::iteration_stats));
    }

    // init general safety property.
    unsafety_condition = propertyInfo->get_reach()->deepCopy_Exp();
    if (verification_type == VerificationMethods::Type::INVARIANT_STRENGTHENING) {
        PLAJA_LOG("Start is set to negation of unsafety.")
        start_condition = unsafety_condition->deepCopy_Exp();
        TO_NORMALFORM::negate(start_condition);
        TO_NORMALFORM::normalize(start_condition);
        TO_NORMALFORM::specialize(start_condition);
    } else {
        start_condition = propertyInfo->get_start()->deepCopy_Exp();
    }

    sim_env = std::make_unique<SimulationEnvironment>(config, *model);
    // create once, update later.
    enumerator = std::make_unique<InitialStatesEnumerator>(config, *start_condition);
    const Approximation::Type approximation_type =
        config.has_value_option(PLAJA_OPTION::approximation_type)
            ? Approximation::string_to_type(config.get_value_option_string(PLAJA_OPTION::approximation_type))
            : Approximation::Type::None;
    if (approximation_type != Approximation::Type::None) {
        const auto approximate = config.get_value_option_string(PLAJA_OPTION::approximate);
        approximate_testing = true;
        approximate_verification = approximate == "both";
    }
    strengthening_strategy =
        StrengtheningStrategy::create(verification_type, *model, approximation_type, per_iteration_stats.get());

    iteration_mode = Mode::Verification;

    // init testing.
    if ((use_testing = config.is_flag_set(PLAJA_OPTION::use_testing))) {
        iteration_mode = Mode::Testing;
        terminate_cycles = config.is_flag_set(PLAJA_OPTION::terminate_on_cycles);
        use_policy_run_sampling = config.is_flag_set(PLAJA_OPTION::policy_run_sampling);
        testing_time_limit = config.get_int_option(PLAJA_OPTION::testing_time);
    }
}

SafeStartGenerator::~SafeStartGenerator() = default;

SearchEngine::SearchStatus SafeStartGenerator::initialize() { return SearchStatus::IN_PROGRESS; }

SearchEngine::SearchStatus SafeStartGenerator::finalize() { return SearchStatus::IN_PROGRESS; }

SearchEngine::SearchStatus SafeStartGenerator::step() {

    switch (iteration_mode) {
        case Mode::Testing: iteration_mode = run_testing(); break;
        case Mode::Verification: iteration_mode = run_verification(); break;
        case Mode::CheckStart: return check_start_condition();
    }

    searchStatistics->inc_attr_unsigned(PLAJA::StatsUnsigned::ITERATIONS);
    dump_iteration_stats();

    // update strengthening method.
    enumerator->update_start_condition(*start_condition);

    return SearchStatus::IN_PROGRESS;
}

SafeStartGenerator::Mode SafeStartGenerator::run_testing() {
    std::cout << "Identifying unsafe paths ..." << '\n';
    per_iteration_stats->testing_iteration();
    PUSH_LAP(searchStatistics, PLAJA::StatsDouble::TOTAL_TESTING_TIME);
    PUSH_LAP_IF(per_iteration_stats.get(), PLAJA::StatsDouble::SEARCHING_TIME);
    const auto unsafe_states_ids = get_unsafe_path_identifier()->identify_unsafe_paths();
    POP_LAP(searchStatistics, PLAJA::StatsDouble::TOTAL_TESTING_TIME);
    POP_LAP_IF(per_iteration_stats.get(), PLAJA::StatsDouble::SEARCHING_TIME);
    std::cout << unsafe_states_ids.size() << " unsafe states found" << '\n';
    Mode next_mode;
    if (!unsafe_states_ids.empty()) {
        searchStatistics->inc_attr_unsigned(PLAJA::StatsUnsigned::UNSAFE_STATES, unsafe_states_ids.size());
        if (per_iteration_stats) {
            per_iteration_stats->inc_unsigned(PLAJA::StatsUnsigned::UNSAFE_STATES, unsafe_states_ids.size());
        }
        auto unsafe_states = get_unsafe_states(unsafe_states_ids);
        PUSH_LAP_IF(per_iteration_stats.get(),PLAJA::StatsDouble::REFINING_TIME)
        PUSH_LAP(searchStatistics, PLAJA::StatsDouble::TOTAL_REFINING_TIME);
        auto [new_start, new_unsafety] = strengthening_strategy->update_conditions(
            *start_condition,
            *unsafety_condition,
            approximate_testing,
            unsafe_states);
        POP_LAP_IF(per_iteration_stats.get(),PLAJA::StatsDouble::REFINING_TIME)
        POP_LAP(searchStatistics, PLAJA::StatsDouble::TOTAL_REFINING_TIME);
        start_condition = std::move(new_start);
        unsafety_condition = std::move(new_unsafety);
        next_mode = alternating_mode ? Mode::Verification : Mode::Testing;
    } else {
        // increase_testing_time_limit();
        next_mode = Mode::Verification;
    }
    return next_mode;
}

SafeStartGenerator::Mode SafeStartGenerator::run_verification() {
    PLAJA_LOG("Running verification... ")
    if (per_iteration_stats) { per_iteration_stats->verification_iteration(); }
    const auto verification_method = get_verification_method();
    auto unsafe_states = verification_method->run(*start_condition, *unsafety_condition);
    if (unsafe_states.empty()) { return Mode::CheckStart; }
    PUSH_LAP_IF(per_iteration_stats.get(),PLAJA::StatsDouble::REFINING_TIME)
    PUSH_LAP(searchStatistics, PLAJA::StatsDouble::TOTAL_REFINING_TIME);
    auto [new_start, new_unsafety] = strengthening_strategy->update_conditions(
        *start_condition,
        *unsafety_condition,
        approximate_verification,
        unsafe_states);
    POP_LAP_IF(per_iteration_stats.get(),PLAJA::StatsDouble::REFINING_TIME)
    POP_LAP(searchStatistics, PLAJA::StatsDouble::TOTAL_REFINING_TIME);
    start_condition = std::move(new_start);
    unsafety_condition = std::move(new_unsafety);
    const auto next_mode = (use_testing || alternating_mode) ? Mode::Testing : Mode::Verification;
    return next_mode;
}

/// @returns SOLVED if start condition is not empty, and FINISHED otherwise
SearchEngine::SearchStatus SafeStartGenerator::check_start_condition() {
    PLAJA_LOG("Checking start condition...")
    auto start_state = enumerator->sample_state();
    auto found = start_state != nullptr;
    if (per_iteration_stats) {per_iteration_stats->set_start_condition_status(found);}
    dump_iteration_stats();
    if (found) {
        PLAJA_LOG("Start condition is safe.")
        return SOLVED;
    }
    PLAJA_LOG("Start condition is empty.")
    return FINISHED;
}


std::unique_ptr<UnsafePathIdentifier> SafeStartGenerator::get_unsafe_path_identifier() {
    return std::make_unique<UnsafePathIdentifier>(
        config,
        testing_time_limit,
        *sim_env,
        propertyInfo->get_nn_interface()->load_policy(config),
        *start_condition,
        *unsafety_condition,
        enumerator.get(),
        *searchStatistics,
        per_iteration_stats.get(),
        terminate_cycles,
        use_policy_run_sampling);
}

std::unique_ptr<VerificationMethod> SafeStartGenerator::get_verification_method() const {
    return VerificationMethods::VerificationMethodFactory::create(
        verification_type,
        config,
        *searchStatistics,
        per_iteration_stats.get());
}

std::unordered_set<std::unique_ptr<StateBase>> SafeStartGenerator::get_unsafe_states(
    const std::unordered_set<StateID_type>& ids) const {
    std::unordered_set<std::unique_ptr<StateBase>> states;
    for (auto id: ids) { states.emplace(sim_env->get_state(id).to_ptr()); }
    return states;
}

void SafeStartGenerator::dump_iteration_stats() const {
    if (per_iteration_stats) { per_iteration_stats->dump_to_csv(); }
}

void SafeStartGenerator::print_statistics() const { searchStatistics->print_statistics(); }

void SafeStartGenerator::stats_to_csv(std::ofstream& file) const { searchStatistics->stats_to_csv(file); }

void SafeStartGenerator::stat_names_to_csv(std::ofstream& file) const { searchStatistics->stat_names_to_csv(file); }