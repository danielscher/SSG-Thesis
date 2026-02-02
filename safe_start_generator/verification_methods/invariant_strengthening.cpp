//
// Created by Daniel Sherbakov in 2025.
//

#include "invariant_strengthening.h"
#include "../../../stats/stats_base.h"
#include "../../factories/configuration.h"
#include "../../factories/safe_start_generator/safe_start_generator_options.h"
#include "../../fd_adaptions/state.h"
#include "../../information/jani_2_interface.h"
#include "../../information/property_information.h"
#include "../../smt/model/model_z3.h"
#include "../../smt/solver/smt_solver_z3.h"
#include "../../smt/solver/solution_z3.h" // removing this causes incomplete type error although not used.
#include "../../smt/vars_in_z3.h"
#include "../../smt_nn/model/model_marabou.h"
#include "../../smt_nn/solver/smt_solver_marabou.h"
#include "../../smt_nn/solver/solution_marabou.h" // removing this causes incomplete type error although not used.
#include "../../smt_nn/solver/statistics_smt_solver_marabou.h"
#include "../../smt_nn/vars_in_marabou.h"
#include "../../states/state_base.h"
#include "../../states/state_values.h"
#include "../../successor_generation/action_op.h"
#include "../../successor_generation/successor_generator_c.h"
#include "../start_generation_statistics.h"
#include <memory>

namespace VerificationMethods {

    InvariantStrengthening::InvariantStrengthening(
        const PLAJA::Configuration& config,
        PLAJA::StatsBase& searchStatistics,
        StartGenerationStatistics* perIterStats):
        model_z3(set_z3_model(config)),
        solver_z3(PLAJA_UTILS::cast_unique<Z3_IN_PLAJA::SMTSolver>(model_z3->init_solver(config, 1))),
        search_stats(searchStatistics),
        per_iteration_stats(perIterStats) {
        // init solvers.
        if (model_z3->has_nn()) {
            MARABOU_IN_PLAJA::add_solver_stats(search_stats);
            model_marabou = std::make_unique<ModelMarabou>(config);
            solver_marabou =
                PLAJA_UTILS::cast_unique<MARABOU_IN_PLAJA::SMTSolver>(model_marabou->init_solver(config, 1));
            model_marabou->add_nn_to_query(solver_marabou->_query(), 0); // Encode policy.
        }
    }

    std::unordered_set<std::unique_ptr<StateBase>> InvariantStrengthening::run(const Expression& start, const Expression& unsafety) {
        // run verification.
        PUSH_LAP(&search_stats, PLAJA::StatsDouble::TOTAL_VERIFICATION_TIME);
        PUSH_LAP_IF(per_iteration_stats, PLAJA::StatsDouble::SEARCHING_TIME);
        verify(start, unsafety);
        POP_LAP(&search_stats, PLAJA::StatsDouble::TOTAL_VERIFICATION_TIME);
        POP_LAP_IF(per_iteration_stats, PLAJA::StatsDouble::SEARCHING_TIME);
        search_stats.inc_attr_unsigned(PLAJA::StatsUnsigned::UNSAFE_STATES_VERIFIED, unsafe_states.size());
        if (per_iteration_stats) {
            per_iteration_stats->inc_unsigned(
                PLAJA::StatsUnsigned::UNSAFE_STATES,
                unsafe_states.size());
        }
        return std::move(unsafe_states);
    }

    /**
     * @brief performs verification of the start condition.
     *
     * Checks all update functions of all action labels in order to find a state in the invariant with a transition to
     * the non-invariant. If found it is added to `unsafe_states` set for refinement.
     *
     * @return true if the start condition is not safe, false otherwise.
     */
    void InvariantStrengthening::verify(
        const Expression& start,
        const Expression& unsafety) {
        std::cout << "Verifying ..." << '\n';
        const bool has_nn = model_z3->has_nn();
        PLAJA_ASSERT(not has_nn or (model_marabou and solver_marabou))

        const bool do_locs = not model_z3->ignore_locs();
        const auto& suc_gen = model_z3->get_successor_generator();

        bool violation_found = false;

        solver_z3->push();
        if (has_nn) { solver_marabou->push(); }

        /* Add invariant and non-invariant. */
        model_z3->add_to_solver(*solver_z3, start, 0);
        model_z3->add_to_solver(*solver_z3, unsafety, 1);
        if (model_marabou) {
            model_marabou->add_to_solver(*solver_marabou, start, 0);
            model_marabou->add_to_solver(*solver_marabou, unsafety, 1);
        }
        /* Iterate labels. */
        for (auto it_action = suc_gen.init_action_id_it(true); !it_action.end(); ++it_action) {
            const auto action_label = it_action.get_label();

            const bool is_learned = (has_nn and model_marabou->get_interface()->is_learned(action_label));
            if (is_learned) {
                if (has_nn) { model_marabou->add_output_interface(*solver_marabou, action_label, 0); }
            }

            /* Iterate ops. */
            for (auto it_op = suc_gen.init_action_it_static(action_label); !it_op.end(); ++it_op) {
                const auto& action_op = it_op.operator*();

                for (auto it_upd = action_op.updateIterator(); !it_upd.end(); ++it_upd) {

                    /* Check non-policy transition. */
                    if (!exists_non_policy_transitions(action_op._op_id(), it_upd.update_index(), do_locs)) {
                        continue; // no transition exists
                    }

                    if (has_nn) {
                        if (!exists_policy_transitions(action_op._op_id(), it_upd.update_index(), do_locs)) {
                            continue; // no policy transition exists
                        }
                    }

                    violation_found = true;
                    extract_solver_solution(do_locs);
                }
            }
        }
        if (has_nn) { solver_marabou->pop(); }
        solver_z3->pop();
        [[maybe_unused]] const bool agree = unsafe_states.empty() == violation_found;
        assert(agree);
        std::cout << "verified " << unsafe_states.size() << " states" << '\n';
    }

    /**
     * @brief Verifies the existence of a transition from the invariant to the non-invariant.
     *
     * Uses Z3 to check the existence of any transition between invariant and non-invariant.
     * Excludes the NN for the set of constraints checked.
     * In case a transition exits adds the invariant state with the transition to the set of unsafe states.
     *
     * @return true if a transition exists, false otherwise.
     */
    bool InvariantStrengthening::exists_non_policy_transitions(
        ActionOpID_type action_op_id,
        UpdateIndex_type update_index,
        bool do_locs) {

        solver_z3->push();
        model_z3->add_action_op(*solver_z3, action_op_id, update_index, do_locs, true, 0);
        const bool rlt = solver_z3->check_pop();
        if (not rlt) { return false; }
        return true;
    }

    /**
     * @brief Verifies an existence of policy induced transitions between invariant and non-invariant.
     *
     * Uses Marabou with the NN encoded to determine existence of policy induced transitions.
     * In case a transition exits adds the invariant state with the transition to the set of unsafe states.
     *
     * @return True if transition exists, false otherwise.
     */
    bool InvariantStrengthening::exists_policy_transitions(
        ActionOpID_type action_op_id,
        UpdateIndex_type update_index,
        bool do_locs) {
        solver_marabou->push();
        model_marabou->add_action_op(*solver_marabou, action_op_id, update_index, do_locs, true, 0);
        const bool rlt = solver_marabou->check();
        solver_marabou->pop();

        if (not rlt) {
            // solver_marabou->reset();
            return false;
        }
        return true;
    }

    /// retrieves solution state from marabou solver and adds it to unsafe_states.
    void InvariantStrengthening::extract_solver_solution(const bool do_locs) {
        auto solution_state = model_marabou->get_model_info().get_initial_values();
        model_marabou->get_state_indexes(0).extract_solution(
            solver_marabou->extract_solution(),
            solution_state,
            do_locs);
        unsafe_states.emplace(solution_state.to_ptr());
        // after check_pop BB marabou was calling reset but solution was lost therefore the reset now
        // happens after solution is extracted.
        solver_marabou->reset();
    }

    std::shared_ptr<const ModelZ3> InvariantStrengthening::set_z3_model(const PLAJA::Configuration& config) {
        if (!config.has_sharable(PLAJA::SharableKey::MODEL_Z3)) {
            config.set_sharable(PLAJA::SharableKey::MODEL_Z3, std::make_shared<ModelZ3>(config));
        }
        return config.get_sharable_as_const<ModelZ3>(PLAJA::SharableKey::MODEL_Z3);
    }

} // namespace VerificationMethods