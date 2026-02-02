//
// Created by Daniel Sherbakov in 2025.
//

#ifndef UNSAFE_PATH_IDENTIFIER_H
#define UNSAFE_PATH_IDENTIFIER_H
#include "../../search/non_prob_search/initial_states_enumerator.h"
#include "../../successor_generation/simulation_environment.h"
#include "policy_run_sampling.h"
#include "transition_set.h"

class StartGenerationStatistics;
/**
 * Explores the policy envelop to find unsafe paths.
 */
class UnsafePathIdentifier {

public:
    UnsafePathIdentifier(
        const PLAJA::Configuration& config,
        int time_limit,
        SimulationEnvironment& simulation_environment,
        const Policy& policy,
        const Expression& start_condition,
        const Expression& unsafety_condition,
        InitialStatesEnumerator* enumerator,
        PLAJA::StatsBase& search_statistics,
        StartGenerationStatistics* perIterStats,
        bool terminateCyclesFlag,
        bool usePolicyRunSampling);

    ~UnsafePathIdentifier() = default;

    std::unordered_set<StateID_type> identify_unsafe_paths();

private:
    const Expression& start_condition;
    const Expression& unsafety_condition;
    InitialStatesEnumerator* start_sampler;
    SimulationEnvironment& sim_env;
    const Policy& policy;
    const int path_length_limit = 1000;
    std::unique_ptr<Timer> timer;

    bool unsafe_path_found = false;
    std::unordered_set<StateID_type> unsafe_state_ids;
    std::unordered_set<StateID_type> path_cache; // excluding unsafe states.

    /* Policy-run sampling */
    double sampling_probability = 0;
    std::unique_ptr<PolicyRunSampler> policy_run_sampler;

    /* Cycle detection */
    bool terminate_on_cycles;
    StateID_type source = -1;
    StateID_type target = -1;
    StartGenerator::TransitionSet transition_cache;

    PLAJA::StatsBase& search_stats;
    StartGenerationStatistics* per_iteration_stats = nullptr;

    bool log_path = false; // flag to enable dumping of execution path.
    std::string start_log = "====== EXECUTION START ======";
    std::string deadend_log = "====== DEAD END ======\n";
    std::string cycle_log = "====== CYCLE ======\n";
    std::string unsafety_log = "====== UNSAFE ======\n";
    std::string action_log(ActionLabel_type action) { return "action: " + std::to_string(action); }


    /* Policy Execution*/
    std::unique_ptr<State> sample_successor(const State& state, ActionLabel_type action_label);
    std::unique_ptr<State> simulate_until_choice(const State& state);
    bool execute_policy(State& start_state);

    bool is_terminal(const State& state);
    bool is_unsafe(const State& state) const;

    /* Cycle detection */
    void set_current_state(const State& state);
    void set_next_state(const State& state);
    void set_next_to_current_state();
    bool cache_and_check_cycle(const ActionLabel_type& action_label);
};

#endif //UNSAFE_PATH_IDENTIFIER_H
