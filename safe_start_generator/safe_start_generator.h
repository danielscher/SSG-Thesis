//
// Created by Daniel Sherbakov in 2025.
//

#ifndef SAFE_START_GENERATOR_H
#define SAFE_START_GENERATOR_H
#include "../../parser/ast/expression/expression.h"
#include "../fd_adaptions/search_engine.h"
#include "start_generation_statistics.h"
#include "strengthening_strategy/strengthening_strategy.h"
#include "testing/unsafe_path_identifier.h"
#include "verification_methods/verification_method.h"
#include "verification_methods/verification_types.h"

class InitialStatesEnumerator;
/**
 * @brief A search engine that generates a safe start condition.
 *
 * This search engine generates a safe start condition by iterative refinement process dictated by the `condition_refiner`.
 * With each iteration, the `condition_refiner` refines the start condition by adding new constraints to it.
 */
class SafeStartGenerator final: public SearchEngine {
public:
    explicit SafeStartGenerator(const PLAJA::Configuration& config);
    ~SafeStartGenerator() override;

    // Engine Lifecycle/Interface
    SearchStatus initialize() override;
    SearchStatus finalize() override;
    SearchStatus step() override;

    // Statistics handling
    void dump_iteration_stats() const;
    void print_statistics() const override;
    void stats_to_csv(std::ofstream& file) const override;
    void stat_names_to_csv(std::ofstream& file) const override;

private:
    // Engine Configuration:
    const PLAJA::Configuration& config;
    VerificationMethods::Type verification_type;
    std::unique_ptr<StrengtheningStrategy> strengthening_strategy;
    const bool alternating_mode;
    bool approximate_testing;
    bool approximate_verification;

    enum class Mode {
        Testing,
        Verification,
        CheckStart,
    };

    Mode iteration_mode;

    // Engine Data
    std::unique_ptr<Expression> start_condition;
    std::unique_ptr<Expression> unsafety_condition;

    // Components
    std::unique_ptr<InitialStatesEnumerator> enumerator;
    std::unique_ptr<SimulationEnvironment> sim_env;

    // Statistics
    std::unique_ptr<StartGenerationStatistics> per_iteration_stats;

    // testing options
    bool use_testing = false;
    int testing_time_limit = 0;
    bool use_policy_run_sampling = false;
    bool terminate_cycles = false;

    void increase_testing_time_limit() {
        testing_time_limit *= 2;
        if (testing_time_limit > 1800) { testing_time_limit = 1800; }
    }

    // Helpers
    Mode run_testing();
    Mode run_verification();
    SearchStatus check_start_condition();
    std::unique_ptr<UnsafePathIdentifier> get_unsafe_path_identifier();
    [[nodiscard]] std::unique_ptr<VerificationMethod> get_verification_method() const;
    [[nodiscard]] std::unordered_set<std::unique_ptr<StateBase>> get_unsafe_states(
        const std::unordered_set<StateID_type>& ids) const;
};

#endif //SAFE_START_GENERATOR_H
