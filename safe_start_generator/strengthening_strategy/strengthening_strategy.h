//
// Created by Daniel Sherbakov in 2025.
//

#ifndef STRENGTHENING_STRATEGY_H
#define STRENGTHENING_STRATEGY_H

#include "../../states/forward_states.h"
#include "../../using_search.h"
#include "../approximation_methods/approximation_type.h"

#include <memory>
#include <unordered_set>

class StartGenerationStatistics;
namespace VerificationMethods {
    enum class Type;
}
class Model;
class Expression;

class StrengtheningStrategy {
public:
    virtual ~StrengtheningStrategy() = default;

    virtual std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>> update_conditions(
        Expression& start_condition,
        Expression& unsafety_condition,
        bool approximate,
        std::unordered_set<std::unique_ptr<StateBase>>& unsafe_states) = 0;

    // Factory method to create appropriate strategy
    static std::unique_ptr<StrengtheningStrategy> create(
        VerificationMethods::Type verification_type,
        const Model& model,
        Approximation::Type approximation_type,
        StartGenerationStatistics* per_iter_stats);

protected:
    explicit StrengtheningStrategy(
        const Model& model,
        Approximation::Type approximation_type,
        StartGenerationStatistics* per_iter_stats);
    const Model& model;
    const Approximation::Type approx;
    StartGenerationStatistics* per_iter_stats;

    std::unique_ptr<Expression> get_box_approximation(const std::unordered_set<std::unique_ptr<StateBase>>& set);
};

class InvariantStrengtheningStrategy: public StrengtheningStrategy {
public:
    explicit InvariantStrengtheningStrategy(
        const Model& model,
        Approximation::Type approximation_type,
        StartGenerationStatistics* per_iter_stats);

    std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>> update_conditions(
        Expression& start_condition,
        Expression& unsafety_condition,
        bool approximate,
        std::unordered_set<std::unique_ptr<StateBase>>& unsafe_states) override;
};

class StartConditionStrengtheningStrategy: public StrengtheningStrategy {
public:
    explicit StartConditionStrengtheningStrategy(
        const Model& model,
        Approximation::Type approximation_type,
        StartGenerationStatistics* per_iter_stats);

    std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>> update_conditions(
        Expression& start_condition,
        Expression& unsafety_condition,
        bool approximate,
        std::unordered_set<std::unique_ptr<StateBase>>& unsafe_states) override;
};

#endif //STRENGTHENING_STRATEGY_H
