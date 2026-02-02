//
// Created by Daniel Sherbakov in 2025.
//

#ifndef INVARIANT_STRENGTHENING_H
#define INVARIANT_STRENGTHENING_H

// #include "../../fd_adaptions/search_engine.h"
// #include "../../non_prob_search/initial_states_enumerator.h"
#include "../../smt/bias_functions/distance_function.h"
#include "../../smt/forward_smt_z3.h"
#include "../../smt_nn/forward_smt_nn.h"
#include "../../smt_nn/model/model_marabou.h"
// #include "../../states/forward_states.h"
#include "../testing/unsafe_path_identifier.h"
#include "verification_method.h"

#include <memory>
#include <vector>

#include <unordered_set>

class StartGenerationStatistics;
class Expression;

namespace VerificationMethods {
    /**
     * This class represents an iterative refinement method for the start condition.
     *
     * The idea is to partition the state space into two regions: the invariant (representing all states that do not
     * satisfy the unsafety condition) and the non-invariant (unsafe state i.e. all states that lead the policy to unsafety).
     * Once such a partition is established it can be iteratively updated based on counter examples (unsafe states) found
     * during verification or testing while maintaining the partition.
     *
     * The exact update of the partition is done by moving states from the invariant to the non-invariant.
     */
    class InvariantStrengthening final: public VerificationMethod {
    public:
        InvariantStrengthening(
            const PLAJA::Configuration& config,
            PLAJA::StatsBase& searchStatistics,
            StartGenerationStatistics* perIterStats);

        std::unordered_set<std::unique_ptr<StateBase>> run(const Expression& start, const Expression& unsafety) override;

    private:
        std::shared_ptr<const ModelZ3> model_z3;
        std::unique_ptr<Z3_IN_PLAJA::SMTSolver> solver_z3;

        // NN
        std::unique_ptr<ModelMarabou> model_marabou;
        std::unique_ptr<MARABOU_IN_PLAJA::SMTSolver> solver_marabou;

        std::unordered_set<std::unique_ptr<StateBase>> unsafe_states;

        PLAJA::StatsBase& search_stats;
        StartGenerationStatistics* per_iteration_stats;

        void verify(const Expression& start, const Expression& unsafety);
        bool exists_non_policy_transitions(ActionOpID_type action_op_id, UpdateIndex_type update_index, bool do_locs);
        bool exists_policy_transitions(ActionOpID_type action_op_id, UpdateIndex_type update_index, bool do_locs);
        void extract_solver_solution(bool do_locs);
        std::shared_ptr<const ModelZ3> set_z3_model(const PLAJA::Configuration& config);
    };

} // namespace VerificationMethods

#endif //INVARIANT_STRENGTHENING_H
