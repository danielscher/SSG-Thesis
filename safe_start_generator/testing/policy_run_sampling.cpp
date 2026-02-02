//
// Created by Daniel Sherbakov in 2025.
//

#include "policy_run_sampling.h"

#include "../../fd_adaptions/state.h"
#include "../../fd_adaptions/timer.h"
#include "../../non_prob_search/policy/policy.h"
#include "../../parser/ast/expression/expression.h"
#include "../../smt/bias_functions/distance_function.h"
#include "../../successor_generation/simulation_environment.h"
#include "../safe_start_generator.h"

#include <cmath> // Make sure this is included
#include <numeric>
#include <utility>
PolicyRunSampler::PolicyRunSampler(
    Timer& timer,
    const SimulationEnvironment& simulationEnv,
    const Policy& policy,
    const Expression& start_condition,
    const Expression& unsafety_condition,
    const ModelZ3& model_z3,
    PLAJA::StatsBase& search_statistics,
    StartGenerationStatistics* per_iter_stats,
    const bool probabilistic_sampling,
    const int max_run_length):
    start_condition(start_condition),
    unsafety_condition(unsafety_condition),
    simEnv(simulationEnv),
    policy(policy),
    timer(timer),
    use_probabilistic_sampling(probabilistic_sampling),
    max_policy_run_length(max_run_length),
    search_stats(search_statistics),
    per_iter_stats(per_iter_stats) {
    distance_to_avoid = std::make_unique<Bias::DistanceFunction>(
        unsafety_condition,
        model_z3,
        Bias::DistanceFunctionType::DistanceToTarget);
}

PolicyRunSampler::~PolicyRunSampler() = default;

/**
* @brief Samples a leaf state of all policy induced paths based on a distance of leaf to avoid condition.
*
* Explores policy induced paths from the given successor states. The exploration is done one step at a time until a leaf
* state that obtains a minimum distance to avoid condition is found, all leaf states are terminal, or a time-limit is
* reached. The leaf state is sampled from the probability distribution based on the distance of the leaves, or greedily
* depending on the flag `use_probabilistic_sampling` .
*
* @return leaf state sampled from probability distribution based on the distance of the leaves.
*/
std::pair<std::unique_ptr<State>, std::vector<StateID_type>> PolicyRunSampler::sample_run(
    const std::vector<StateID_type>& successor_ids) {
    std::unordered_map<StateID_type, int> successors_to_distance;
    std::unordered_set<StateID_type> current_successors; // frontier
    std::unordered_map<StateID_type, std::unique_ptr<SearchNode>> search_tree;

    for (StateID_type s: successor_ids) {
        current_successors.insert(s);
        search_tree[s] = std::make_unique<SearchNode>(s, nullptr);
    }
    int num_steps = 0;
    // run policy one step at a time until unique min distance is found, no progress can be made, or time-limit reached.
    while (check_timer()) {
        std::unordered_map<StateID_type, int> current_successors_distance;
        // evaluate distance of current states
        int min_distance = INT_MAX;
        for (auto const id: current_successors) {
            auto successor = simEnv.get_state(id);
            const auto d = distance_to_avoid->evaluate(successor);
            if (d < min_distance) { min_distance = d; }
            current_successors_distance[id] = d;
        }

        // prune states with higher than minimal distance.
        std::unordered_map<StateID_type, int> min_states_to_distance;
        for (const auto [s, d]: current_successors_distance) {
            if (d > min_distance) { continue; }
            min_states_to_distance[s] = d;
        }
        successors_to_distance = min_states_to_distance;

        // check if discrimination achieved
        if (unique_min_exists(successors_to_distance)) { break; }

        // check if all states are terminal
        bool all_terminal =
            std::all_of(current_successors.begin(), current_successors.end(), [this](const auto& state) {
                return is_terminal(simEnv.get_state(state));
            });
        if (all_terminal) { break; }

        // expand all states
        std::unordered_set<StateID_type> new_successors;
        for (auto s: current_successors) {
            if (is_unsafe(s)) {
                auto path = reconstruct_path(*search_tree[s], true);
                return std::make_pair(simEnv.get_state(s).to_ptr(), path);
            }
            auto successors = get_policy_successors(s); // single step
            // add all child states
            for (const auto& state: successors) {
                auto child_id = state->get_id();
                if (search_tree.count(child_id) || current_successors.count(child_id)) { continue; }
                new_successors.insert(child_id);
                search_tree[child_id] = std::make_unique<SearchNode>(child_id, search_tree[s].get());
            }
        }
        if (new_successors.empty()) { break; } // do not update frontier
        current_successors = new_successors;
        if (max_policy_run_length == ++num_steps) { break; }
    }

    // if (successors_to_distance.empty() && timer.is_expired()) { // policy run sampling didn't run.
    //     return std::make_pair<std::unique_ptr<State>, std::vector<StateID_type>>(nullptr, {});
    // }

    // collect leaf states and distances.
    // all leaves of a certain state will have the same distance.
    std::vector<StateID_type> state_ids;
    std::vector<int> distances;
    for (const auto& [s, d]: successors_to_distance) {
        state_ids.push_back(s);
        distances.push_back(successors_to_distance[s]);
    }

    // return leaf state greedily or sampled based on distance.
    auto selected_state =
        use_probabilistic_sampling ? sample_successor(state_ids, distances) : greedy_selection(state_ids, distances);
    auto path = reconstruct_path(*search_tree[selected_state->get_id()], false);
    return std::make_pair(std::move(selected_state), path);
}

/// @return all policy induced successor states of a state.
std::vector<std::unique_ptr<State>> PolicyRunSampler::get_policy_successors(StateID_type state_id) const {
    std::vector<std::unique_ptr<State>> successors;
    auto state = simEnv.get_state(state_id);
    const ActionLabel_type action_label = policy.evaluate(state);
    const auto successor_ids = simEnv.compute_successors(state, action_label);
    for (const auto& id: successor_ids) {
        auto successor = simEnv.get_state(id).to_ptr();
        successors.push_back(std::move(successor));
    }
    return successors;
}

/**
    * Selects the states with minimum distance.
    * If multiple states have min distance than return uniformly random one.
*/
std::unique_ptr<State> PolicyRunSampler::greedy_selection(
    const std::vector<StateID_type>& successors,
    const std::vector<int>& distances) {
    // get successors with min distance and return uniformly random state.
    const int min_distance = *std::min_element(distances.begin(), distances.end());
    std::vector<StateID_type> minimal_states;
    for (size_t i = 0; i < distances.size(); ++i) {
        if (distances[i] == min_distance) { minimal_states.push_back(successors[i]); }
    }
    if (minimal_states.size() > 1) {
        return simEnv.get_state(minimal_states[PLAJA_GLOBAL::rng->index(minimal_states.size())]).to_ptr();
    }
    return simEnv.get_state(minimal_states[0]).to_ptr();
}

/**
    * Samples a successor state based on the distances of the states.
    * The lower the distance the higher the sampling probability.
    * Uses softmax to get probabilities of the states and samples a state from the distribution.
    *
    * @param successors sample set of successor states.
    * @param distances values on which the softmax is applied.
    *
    * @return state from the distribution.
 */
std::unique_ptr<State> PolicyRunSampler::sample_successor(
    const std::vector<StateID_type>& successors,
    const std::vector<int>& distances) {
    // if distances are the same return uniformly random state.
    if (std::adjacent_find(distances.begin(), distances.end(), std::not_equal_to<>()) == distances.end()) {
        return sample_from_distribution(successors, std::vector<double>(successors.size(), 1.0 / successors.size()));
    }

    // compute softmax values
    std::vector<double> exp_values;
    double alpha = 1.0;
    exp_values.reserve(distances.size());
    for (double d: distances) { exp_values.push_back(std::exp(-alpha * d)); }

    // compute softmax probabilities
    std::vector<double> softmax_probabilities;
    double sum_exp = std::accumulate(exp_values.begin(), exp_values.end(), 0.0);
    softmax_probabilities.reserve(exp_values.size());
    for (auto d: exp_values) { softmax_probabilities.push_back(d / sum_exp); }

    return sample_from_distribution(successors, softmax_probabilities);
}

/**
 * Performs inverse transform sampling to sample a state from a given distribution.
 *
 * @param states list of states to sample from.
 * @param probabilities list of probabilities corresponding to a discrete probability distribution of the states.
 *
 * @return State from the distribution.
 */
std::unique_ptr<State> PolicyRunSampler::sample_from_distribution(
    const std::vector<StateID_type>& states,
    const std::vector<double>& probabilities) {
    const auto p = PLAJA_GLOBAL::rng->prob();
    double cumulative_probability = 0.0;
    for (size_t i = 0; i < probabilities.size(); ++i) {
        cumulative_probability += probabilities[i];
        if (p <= cumulative_probability) { return simEnv.get_state(states[i]).to_ptr(); }
    }
    return simEnv.get_state(states.back()).to_ptr();
}

/**
 * @brief Checks if a unique state with minimum distance is found.
 *
 * @param successors_to_distance map from successor states to their distance.
 *
 * @return true if a unique state with minimum distance is found.
 */
bool PolicyRunSampler::unique_min_exists(std::unordered_map<StateID_type, int>& successors_to_distance) {
    std::set<StateID_type> min_states;
    int min_distance = INT_MAX;
    for (const auto& [s, d]: successors_to_distance) {
        if (d < min_distance) {
            min_distance = d;
            min_states.clear();
            min_states.insert(s);
        } else if (d == min_distance) {
            min_states.insert(s);
        }
    }
    return min_states.size() == 1;
}

/// @return true if state has no successor states.
bool PolicyRunSampler::is_terminal(const State& state) {
    const bool dead_end = simEnv.extract_applicable_actions(state, true).empty();
    if (dead_end) { search_stats.inc_attr_unsigned(PLAJA::StatsUnsigned::DEAD_ENDS); }
    return dead_end;
}

bool PolicyRunSampler::is_unsafe(const StateID_type& id) const {
    const auto state = simEnv.get_state(id);
    const bool rlt = unsafety_condition.evaluate_integer(state);
    return rlt;
}

std::vector<StateID_type> PolicyRunSampler::reconstruct_path(const SearchNode& node, const bool unsafe_node) {
    std::vector<StateID_type> path;

    if (!unsafe_node) { path.push_back(node.id); }
    auto current_node = node.parent;
    while (current_node) {
        // std::cout << current_node->id << ",";
        path.push_back(current_node->id);
        current_node = current_node->parent;
    }
    // std::cout << "path size: " << path.size() << std::endl;
    return path;
}

/// checks if timer expired and handles statistics
bool PolicyRunSampler::check_timer() const {
    if (!timer.is_expired()) { return true; }
    if (per_iter_stats) { per_iter_stats->inc_unsigned(PLAJA::StatsUnsigned::TIME_LIMIT_REACHED, 1); }
    return false;
}
