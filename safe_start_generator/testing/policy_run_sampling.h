//
// Created by Daniel Sherbakov in 2025.
//

#ifndef POLICY_RUN_SAMPLING_H
#define POLICY_RUN_SAMPLING_H

#include "../../../utils/rng.h"
#include "../../smt/bias_functions/distance_function.h"
#include "../start_generation_statistics.h"
#include "../../fd_adaptions/timer.h"
#include <memory>

/**
 * @brief This class implements the policy run sampling algorithm.
 *
 * Performs bias sampling of all possible policy runs up to some length n in order to identify unsafe states in
 * non-deterministic environments.
 *
 * For a given distance function that approximates the distance to the unsafe region, the algorithm samples a policy run
 * by evaluating the distance function over the leaves of the policy runs, Then selects the runs prioritizing minimizing
 * the distance function.
 */
class PolicyRunSampler {
private:
    const Expression& start_condition;
    const Expression& unsafety_condition;

    // simulation:
    const SimulationEnvironment& simEnv;
    const Policy& policy;

    //Policy run sampling:
    Timer& timer;
    std::unique_ptr<Bias::DistanceFunction> distance_to_avoid;
    bool use_probabilistic_sampling;
    int max_policy_run_length;

    // path caching
    struct SearchNode {
        StateID_type id;
        SearchNode* parent;

        SearchNode(const StateID_type id,SearchNode* parent_ptr) : id(id), parent(parent_ptr) {}
    };

    PLAJA::StatsBase& search_stats;
    StartGenerationStatistics* per_iter_stats;

    std::unique_ptr<State> sample_successor(
        const std::vector<StateID_type>& successors,
        const std::vector<int>& distances);

    std::unique_ptr<State> greedy_selection(
        const std::vector<StateID_type>& successors,
        const std::vector<int>& distances);

    std::unique_ptr<State> sample_from_distribution(
        const std::vector<StateID_type>& states,
        const std::vector<double>& probabilities);

    [[nodiscard]] std::vector<std::unique_ptr<State>> get_policy_successors(StateID_type state_id) const;

    bool unique_min_exists(std::unordered_map<StateID_type, int>& successors_to_distance);
    [[nodiscard]] bool is_terminal(const State& state);
    [[nodiscard]] bool is_unsafe(const StateID_type& id) const;
    std::vector<StateID_type> reconstruct_path(const SearchNode& node,bool unsafe_node);
    bool check_timer() const;


public:
    PolicyRunSampler(
        Timer& timer,
        const SimulationEnvironment& simulationEnv,
        const Policy& policy,
        const Expression& start_condition,
        const Expression& unsafety_condition,
        const ModelZ3& model_z3,
        PLAJA::StatsBase& search_statistics,
        StartGenerationStatistics* per_iter_stats,
        bool probabilistic_sampling,
        int max_run_length);

    ~PolicyRunSampler();
    DELETE_CONSTRUCTOR(PolicyRunSampler)

    std::pair<std::unique_ptr<State>,std::vector<StateID_type>> sample_run(const std::vector<StateID_type>& successor_ids);

};

#endif //POLICY_RUN_SAMPLING_H
