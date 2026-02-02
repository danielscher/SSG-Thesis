//
// Created by Daniel Sherbakov in 2025.
//

#include "unsafe_path_identifier.h"

#include "../../../parser/ast/expression/expression.h"
#include "../../factories/configuration.h"
#include "../../factories/safe_start_generator/safe_start_generator_options.h"
#include "../../fd_adaptions/timer.h"
#include "../../information/property_information.h"
#include "../../non_prob_search/policy/policy.h"
#include "../../stats/stats_base.h"
#include "../../successor_generation/simulation_environment.h"
#include "../start_generation_statistics.h"

UnsafePathIdentifier::UnsafePathIdentifier(
    const PLAJA::Configuration& config,
    const int time_limit,
    SimulationEnvironment& simulation_environment,
    const Policy& policy,
    const Expression& start_condition,
    const Expression& unsafety_condition,
    InitialStatesEnumerator* enumerator,
    PLAJA::StatsBase& search_statistics,
    StartGenerationStatistics* perIterStats,
    const bool terminateCyclesFlag,
    const bool usePolicyRunSampling):
    start_condition(start_condition),
    unsafety_condition(unsafety_condition),
    start_sampler(enumerator),
    sim_env(simulation_environment),
    policy(policy),
    timer(std::make_unique<Timer>(time_limit)),
    policy_run_sampler(nullptr),
    terminate_on_cycles(terminateCyclesFlag),
    search_stats(search_statistics),
    per_iteration_stats(perIterStats),
    log_path(config.is_flag_set(PLAJA_OPTION::log_path)) {
    if (usePolicyRunSampling) {
        // std::cout << "Using policy run sampling ..." << std::endl;
        sampling_probability = config.get_double_option(PLAJA_OPTION::sampling_probability);
        policy_run_sampler = std::make_unique<PolicyRunSampler>(
            *timer,
            sim_env,
            policy,
            start_condition,
            unsafety_condition,
            *config.get_sharable_as_const<ModelZ3>(PLAJA::SharableKey::MODEL_Z3),
            search_statistics,
            per_iteration_stats,
            config.is_flag_set(PLAJA_OPTION::use_probabilistic_sampling),
            config.get_int_option(PLAJA_OPTION::max_run_length));
    }
}

/**
 * Search policy envelope for unsafe paths.
 *
 * @return State IDs of states along unsafe paths identified excluding the unsafe states.
 */
std::unordered_set<StateID_type> UnsafePathIdentifier::identify_unsafe_paths() {
    transition_cache.clear(); // clear cache for new execution.
    while (!timer->is_expired()) {
        auto start_state_vals = start_sampler->sample_state();
        PLAJA_ASSERT(start_state_vals)
        if (!start_state_vals) {
            PLAJA_LOG("... Stopping: No start state found.")
            break;
        }
        auto start_state = sim_env.get_state(*start_state_vals).to_ptr();
        search_stats.inc_attr_unsigned(PLAJA::StatsUnsigned::START_STATES);

        path_cache.insert(start_state->get_id());
        set_current_state(*start_state);

        if (execute_policy(*start_state)) {
            unsafe_path_found = true;
            search_stats.inc_attr_unsigned(PLAJA::StatsUnsigned::UNSAFE_PATHS);
            unsafe_state_ids.insert(path_cache.begin(), path_cache.end());
            path_cache.clear();
        } else {
            path_cache.clear();
        }
    }
    return unsafe_state_ids;
}

/**
 * Simulates policy execution from a given state until a terminal state is reached.
 *
 * @param start_state state to begin policy execution from.
 *
 * @return true if unsafe state is reached, false otherwise.
 */
bool UnsafePathIdentifier::execute_policy(State& start_state) {
    std::unique_ptr<State> current_state = start_state.to_ptr(); // copy

    PLAJA_FLOG_IF(log_path, start_log)
    PLAJA_FLOG_IF(log_path, start_state.to_str())

    while (not is_terminal(*current_state)) {
        current_state = simulate_until_choice(*current_state);

        if (not current_state) {
            PLAJA_FLOG_IF(log_path, deadend_log)
            return false;
        } // dead-end reached -> safe.
        set_current_state(*current_state); // for cycle detection
        if (is_unsafe(*current_state)) {
            PLAJA_FLOG_IF(log_path, unsafety_log)
            return true;
        }

        const auto action_label = policy.evaluate(*current_state);
        // std::cout << "action: " << action_label << std::endl;
        // auto action_id = ModelInformation::action_label_to_id(action_label);
        // auto action_str = get_model().get_action_name(action_id);
        // std::cout << action_str << std::endl;

        current_state = sample_successor(*current_state, action_label);

        PLAJA_FLOG_IF(log_path, action_log(action_label))
        PLAJA_FLOG_IF(log_path, current_state->to_str())

        if (not current_state) { return false; } // dead-end reached.

        set_next_state(*current_state);
        if (not cache_and_check_cycle(action_label) and terminate_on_cycles) {
            PLAJA_FLOG_IF(log_path, cycle_log)
            return false;
        } // cycle detected.

        path_cache.insert(current_state->get_id());
        set_next_to_current_state();
        if (path_cache.size() >= path_length_limit) {
            PLAJA_FLOG_IF(log_path, cycle_log)
            return false;
        } // limit path length
    }
    return false;
}

/**
 * @brief Expands states until a state with multiple actions is found, or a state cannot be expanded (dead-end/unsafe).
 *
 * @param state to begin simulation from.
 *
 * @return state with multiple actions, an unsafe state, or nullptr for terminal states.
 */
std::unique_ptr<State> UnsafePathIdentifier::simulate_until_choice(const State& state) {
    std::unique_ptr<State> current_state = state.to_ptr(); // creates duplicate

    // simulate until next choice point
    auto cachedApplicableActions = sim_env.extract_applicable_actions(*current_state, true);
    while (cachedApplicableActions.size() <= 1) {

        if (cachedApplicableActions.empty()) { return nullptr; }

        // step:
        const ActionLabel_type next_action = cachedApplicableActions[0];
        current_state = sample_successor(*current_state, next_action);

        PLAJA_FLOG_IF(log_path, action_log(next_action))
        PLAJA_FLOG_IF(log_path, current_state->to_str())

        set_next_state(*current_state);
        if (not cache_and_check_cycle(next_action) and terminate_on_cycles) {
            PLAJA_FLOG_IF(log_path, cycle_log)
            return nullptr; // cycle detected.
        }

        if (is_unsafe(*current_state)) {
            PLAJA_FLOG_IF(log_path, unsafety_log)
            return current_state;
        }

        path_cache.insert(current_state->get_id());

        // applicable actions for next iteration
        cachedApplicableActions = sim_env.extract_applicable_actions(*current_state, true);
        set_next_to_current_state();
        if (path_cache.size() >= path_length_limit) {
            PLAJA_FLOG_IF(log_path, cycle_log)
            return nullptr;
        }
    }
    return current_state;
}

/**
 * @brief samples a successor based on transition probabilities or based on distance function.
 *
 * In case of multiple successors, and policy_run_sampling is enabled will perform policy run sampling with some probability.
 *
 * @return successor state in s[a].
 */
std::unique_ptr<State> UnsafePathIdentifier::sample_successor(const State& state, ActionLabel_type action_label) {
    const auto p = PLAJA_GLOBAL::rng->prob();
    auto successor_ids = sim_env.compute_successors(state, action_label);
    if (successor_ids.size() > 1 and p < sampling_probability and !timer->is_almost_expired(1)) {
        auto [successor,path] = policy_run_sampler->sample_run(successor_ids);
        for (auto s :path ) {
            path_cache.insert(s);
        }
        return std::move(successor);
    }
    return sim_env.compute_successor_if_applicable(state, action_label);
}

/// @return true if state has no successor states.
bool UnsafePathIdentifier::is_terminal(const State& state) {
    bool dead_end = sim_env.extract_applicable_actions(state, true).empty();
    if (dead_end) {
        search_stats.inc_attr_unsigned(PLAJA::StatsUnsigned::DEAD_ENDS);
        PLAJA_FLOG_IF(log_path, deadend_log)
    }
    return dead_end;
}

bool UnsafePathIdentifier::is_unsafe(const State& state) const {
    PUSH_LAP_IF(per_iteration_stats, PLAJA::StatsDouble::UNSAFETY_EVAL);
    bool result = unsafety_condition.evaluate_integer(state);
    POP_LAP_IF(per_iteration_stats, PLAJA::StatsDouble::UNSAFETY_EVAL);
    return result;
}

void UnsafePathIdentifier::set_current_state(const State& state) {
    source = state.get_id();
    target = -1;
}

void UnsafePathIdentifier::set_next_state(const State& state) { target = state.get_id(); }

void UnsafePathIdentifier::set_next_to_current_state() {
    assert(target != -1);
    source = target;
    target = -1;
}

/**
 * Caches transition and checks if cycle is detected.
 *
 * @return false if cycle detected.
 */
bool UnsafePathIdentifier::cache_and_check_cycle(const ActionLabel_type& action_label) {
    assert(target != -1 && source != -1);
    const bool inserted = transition_cache.insert({source, action_label, target}).second;
    if (not inserted) { search_stats.inc_attr_unsigned(PLAJA::StatsUnsigned::CYCLES); }
    return inserted;
}
