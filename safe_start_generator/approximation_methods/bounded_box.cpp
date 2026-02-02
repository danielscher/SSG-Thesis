//
// Created by Daniel Sherbakov in 2025.
//

#include "bounded_box.h"

#include "../../../parser/ast/expression/integer_value_expression.h"
#include "../../../parser/ast/model.h"
#include "../../../parser/ast/variable_declaration.h"
#include "../../information/model_information.h"
#include "../../parser/ast/expression/binary_op_expression.h"
#include "../../parser/ast/expression/special_cases/nary_expression.h"

std::vector<int> BoundedBox::get_state_vec(const StateBase& state) {
    std::vector<int> state_vec;
    size_t num_variables = state.get_int_state_size();
    state_vec.reserve(num_variables);
    for (size_t i = 1; i < num_variables; i++) { // loc var at position 0.
        state_vec.push_back(state.get_int(i));
    }
    return state_vec;
}

std::pair<size_t,std::unique_ptr<Expression>> BoundedBox::compute_bounded_box(
    const std::unordered_set<std::unique_ptr<StateBase>>& state_set,
    const Model& model) {

    // construct state set for faster lookup.
    ValuationSet val_set;
    val_set.reserve(state_set.size());
    for (auto const& state: state_set) { val_set.insert(get_state_vec(*state)); }

    // best box.
    std::vector<int> best_min, best_max;
    size_t max_volume = 0;

    // find maximal bounded box.
    for (const auto& state: state_set) {

        auto center = get_state_vec(*state);
        std::vector<int> min_corner = center;
        std::vector<int> max_corner = center;

        // expand in all directions.
        bool fixed_point = false;
        while (!fixed_point) {
            fixed_point = true;
            for (size_t dim = 0; dim < center.size(); dim++) {
                min_corner[dim] -= 1;
                if (!is_bounded(min_corner, max_corner, val_set, model)) {
                    min_corner[dim] += 1; // revert
                } else {
                    fixed_point = false;
                }

                max_corner[dim] += 1;
                if (!is_bounded(max_corner, min_corner, val_set, model)) {
                    max_corner[dim] -= 1; // revert
                } else {
                    fixed_point = false;
                }
            }
        }

        // compute volume.
        size_t volume = 1;
        for (size_t dim = 0; dim < center.size(); dim++) { volume *= max_corner[dim] - min_corner[dim] + 1; }

        // update current max box.
        if (volume > max_volume) {
            max_volume = volume;
            best_min = min_corner;
            best_max = max_corner;
        }
    }

    std::cout << "box size: " << max_volume << std::endl;

    auto box = std::make_unique<NaryExpression>(BinaryOpExpression::AND);
    for (size_t var_index = 0; var_index < best_min.size(); ++var_index) {
        const auto var_dec = model.get_variable(var_index);
        auto var_expr = model.gen_var_expr(var_index, var_dec);

        auto lower = std::make_unique<BinaryOpExpression>(BinaryOpExpression::GE);
        lower->set_left(var_expr->deepCopy_Exp());
        lower->set_right(std::make_unique<IntegerValueExpression>(best_min[var_index]));
        box->add_sub(std::move(lower));

        auto upper = std::make_unique<BinaryOpExpression>(BinaryOpExpression::LE);
        upper->set_left(std::move(var_expr));
        upper->set_right(std::make_unique<IntegerValueExpression>(best_max[var_index]));
        box->add_sub(std::move(upper));
    }
    box->dump(true);
    return std::make_pair(max_volume,std::move(box));
}

bool BoundedBox::is_bounded(
    const std::vector<int>& min_corner,
    const std::vector<int>& max_corner,
    ValuationSet& point_set,
    const Model& model) {
    const auto& info = model.get_model_information();
    const size_t dims = min_corner.size();

    // Domain bound check
    for (size_t i = 0; i < dims; ++i) {
        const int model_lb = info.get_lower_bound_int(i+1); // skip loc variable at 0.
        const int model_ub = info.get_upper_bound_int(i+1);
        if (min_corner[i] < model_lb || max_corner[i] > model_ub) {
            return false; // domain bound violated.
        }
    }

    auto current = min_corner;

    // iterate over all points in box and check existence
    while (true) {

        if (point_set.find(current)==point_set.end()) {
            return false;
        }

        size_t dim = current.size();
        while (dim-- > 0) {
            current[dim]++; // next point
            if (current[dim] <= max_corner[dim]) {
                break; // check membership of new point
            }
            current[dim] = min_corner[dim]; // bound exceeded, revert, and proceed to next dim.
            if (dim == 0) { return true; }  // all points processed.
        }
    }
    return true;
}
