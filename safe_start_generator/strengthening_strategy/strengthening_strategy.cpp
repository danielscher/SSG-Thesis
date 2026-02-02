//
// Created by Daniel Sherbakov in 2025.
//

#include "strengthening_strategy.h"

#include "../../fd_adaptions/timer.h"
#include "../../parser/ast/expression/expression.h"
#include "../../parser/visitor/to_normalform.h"
#include "../../states/state_values.h"
#include "../approximation_methods/bounded_box.h"
#include "../approximation_methods/bounding_box.h"
#include "../start_generation_statistics.h"
#include "../verification_methods/verification_types.h"

#include <list>

using namespace VerificationMethods;

StrengtheningStrategy::StrengtheningStrategy(
    const Model& model,
    Approximation::Type approximation_type,
    StartGenerationStatistics* per_iter_stats):
    model(model),
    approx(approximation_type),
    per_iter_stats(per_iter_stats) {}

std::unique_ptr<StrengtheningStrategy> StrengtheningStrategy::create(
    Type verification_type,
    const Model& model,
    Approximation::Type approximation_type,
    StartGenerationStatistics* per_iter_stats) {
    switch (verification_type) {
        case Type::INVARIANT_STRENGTHENING:
            return std::make_unique<InvariantStrengtheningStrategy>(model, approximation_type, per_iter_stats);
        case Type::START_CONDITION_STRENGTHENING:
            return std::make_unique<StartConditionStrengtheningStrategy>(model, approximation_type, per_iter_stats);
        default: throw std::runtime_error("Unknown strengthening strategy");
    }
}

std::unique_ptr<Expression> StrengtheningStrategy::get_box_approximation(
    const std::unordered_set<std::unique_ptr<StateBase>>& set) {
    switch (approx) {
        case Approximation::Type::Overapproximation: {
            PLAJA_LOG("Over approximating ...")
            auto rlt = BoundingBox::compute_bounding_box(set, model);
            per_iter_stats->inc_attr_double(PLAJA::StatsDouble::BOX_SIZE, rlt.first);
            return std::move(rlt.second);
        }
        case Approximation::Type::Underapproximation: {
            PLAJA_LOG("Under approximating ...")
            auto rlt = BoundedBox::compute_bounded_box(set, model);
            per_iter_stats->inc_attr_double(PLAJA::StatsDouble::BOX_SIZE, rlt.first);
            return std::move(rlt.second);
        }
        default:;
    }
    return nullptr;
}

InvariantStrengtheningStrategy::InvariantStrengtheningStrategy(
    const Model& model,
    Approximation::Type approximation_type,
    StartGenerationStatistics* per_iter_stats):
    StrengtheningStrategy(model, approximation_type, per_iter_stats) {}

StartConditionStrengtheningStrategy::StartConditionStrengtheningStrategy(
    const Model& model,
    Approximation::Type approximation_type,
    StartGenerationStatistics* per_iter_stats):
    StrengtheningStrategy(model, approximation_type, per_iter_stats) {}

/**
 * @brief Removes unsafe states from the start condition and adds them to the unsafety condition.
 *
 * The start condition is refined by negating each state and conjuncting it to the start condition.
 * Unsafety condition coarsened by disjuncting each unsafe state with it.
 *
 * @param unsafe_states set of states from which the policy reached the unsafety condition.
 *
 * @return Refined start_condition and coarsend unsafety condition.
 */
std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>> InvariantStrengtheningStrategy::update_conditions(
    Expression& start_condition,
    Expression& unsafety_condition,
    const bool approximate,
    std::unordered_set<std::unique_ptr<StateBase>>& unsafe_states) {

    PLAJA_LOG("Updating Conditions ...")

    auto start_condition_copy = start_condition.deepCopy_Exp();
    auto unsafety_condition_copy = unsafety_condition.deepCopy_Exp();

    // split start and unsafety conditions
    std::list<std::unique_ptr<Expression>> conjuncts =
        TO_NORMALFORM::split_conjunction(std::move(start_condition_copy), false);
    std::list<std::unique_ptr<Expression>> disjunctions =
        TO_NORMALFORM::split_disjunction(std::move(unsafety_condition_copy), false);

    if (approximate and approx != Approximation::Type::None) {
        auto box = get_box_approximation(unsafe_states);
        auto negated_box = box->deepCopy_Exp();
        TO_NORMALFORM::negate(negated_box);
        conjuncts.push_back(std::move(negated_box));
        disjunctions.push_back(std::move(box));
    } else {
        for (const auto& state: unsafe_states) {
            auto state_condition = state->to_condition(false, model);

            // exclude condition
            auto negated = state_condition->deepCopy_Exp();
            TO_NORMALFORM::negate(negated);
            conjuncts.push_back(std::move(negated));

            // include condition
            disjunctions.push_back(std::move(state_condition));
        }
    }

    auto new_start = TO_NORMALFORM::construct_conjunction(std::move(conjuncts));
    TO_NORMALFORM::normalize(new_start);
    TO_NORMALFORM::specialize(new_start);
    auto new_unsafety = TO_NORMALFORM::construct_disjunction(std::move(disjunctions));
    TO_NORMALFORM::normalize(new_unsafety);
    TO_NORMALFORM::specialize(new_unsafety);

    return std::make_pair(std::move(new_start), std::move(new_unsafety));
}

/**
 * @brief refines start condition only.
 *
 * Removes unsafe *start* states from start condition by conjunction of negated unsafe states.
 *
 * @param start_condition updated according to unsafe states.
 * @param unsafety_condition remains unchanged.
 * @param unsafe_states set of states from which the policy reached the unsafety condition.
 *
 * @return new start condition along with the unchanged unsafety condition.
 */
std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>> StartConditionStrengtheningStrategy::
    update_conditions(
        Expression& start_condition,
        Expression& unsafety_condition,
        const bool approximate,
        std::unordered_set<std::unique_ptr<StateBase>>& unsafe_states) {

    PLAJA_LOG("Updating Conditions ...")

    auto start_condition_copy = start_condition.deepCopy_Exp();
    auto unsafety_condition_copy = unsafety_condition.deepCopy_Exp();

    // split start and unsafety conditions
    std::list<std::unique_ptr<Expression>> conjuncts =
        TO_NORMALFORM::split_conjunction(std::move(start_condition_copy), false);
    std::list<std::unique_ptr<Expression>> disjunctions =
        TO_NORMALFORM::split_disjunction(std::move(unsafety_condition_copy), false);

    // filter start states.
    std::unordered_set<std::unique_ptr<StateBase>> unsafe_start_states;
    for (auto it = unsafe_states.begin(); it != unsafe_states.end();) {
        if (start_condition.evaluate_integer(**it)) {
            auto node = unsafe_states.extract(it++);
            unsafe_start_states.insert(std::move(node.value()));
        } else {
            ++it;
        }
    }

    if (per_iter_stats) {
        per_iter_stats->inc_unsigned(PLAJA::StatsUnsigned::UNSAFE_STATES, unsafe_start_states.size());
    }

    if (approximate and approx != Approximation::Type::None) {
        auto box = get_box_approximation(unsafe_start_states);
        auto negated_box = box->deepCopy_Exp();
        TO_NORMALFORM::negate(negated_box);
        conjuncts.push_back(std::move(negated_box));
        disjunctions.push_back(std::move(box));
    } else {
        for (const auto& state: unsafe_start_states) {
            auto state_condition = state->to_condition(false, model);
            auto negated_state = state_condition->deepCopy_Exp();
            // exclude condition
            TO_NORMALFORM::negate(negated_state);
            conjuncts.push_back(std::move(negated_state));
            disjunctions.push_back(std::move(state_condition));
        }
    }

    // construct new start condition from conjuncts
    auto new_start = TO_NORMALFORM::construct_conjunction(std::move(conjuncts));
    TO_NORMALFORM::normalize(new_start);
    TO_NORMALFORM::specialize(new_start);
    auto new_unsafety = TO_NORMALFORM::construct_disjunction(std::move(disjunctions));
    TO_NORMALFORM::normalize(new_unsafety);
    TO_NORMALFORM::specialize(new_unsafety);
    return std::make_pair(std::move(new_start), std::move(new_unsafety));
}