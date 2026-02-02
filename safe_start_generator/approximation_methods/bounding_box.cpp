//
// Created by Daniel Sherbakov in 2025.
//

#include "bounding_box.h"

#include "../../../parser/ast/expression/integer_value_expression.h"
#include "../../../parser/ast/model.h"
#include "../../../parser/ast/variable_declaration.h"
#include "../../information/model_information.h"
#include "../../parser/ast/expression/binary_op_expression.h"
#include "../../parser/ast/expression/special_cases/nary_expression.h"

#include <climits>
#include <iomanip>
#include <map>

std::pair<double, std::unique_ptr<Expression>> BoundingBox::compute_bounding_box(
    const std::unordered_set<std::unique_ptr<StateBase>>& state_set,
    const Model& model) {
    std::cout << "Computing bounding box ..." << '\n';
    std::unordered_map<VariableIndex_type, std::pair<int, int>> bounds;
    //init map
    // auto var_num = (*state_set.begin())->get_int_state_size(); // ignore location var
    auto var_num = model.get_number_variables();
    for (int state_index = 1; state_index <= var_num; ++state_index) { //
        bounds[state_index] = {INT_MAX, INT_MIN};
      }

    // std::cout << "dom box size: " << domain_box_size << std::endl;

    // find bounds.
    for (const auto& state: state_set) {
        // state->dump(&model);
        for (int state_index = 1; state_index <= var_num; ++state_index) { // start with 1 to skip location variable.
            auto value = state->get_int(state_index);
            //  std::cout << "index = " << state_index << " value: "  << value << '\n';
            // // sanity check for out of domain bounds values
            //  auto lb = model.get_model_information().get_lower_bound_int(state_index);
            //  auto ub = model.get_model_information().get_upper_bound_int(state_index);
            //  if (value < lb || value > ub) {
            //      PLAJA_LOG(PLAJA_UTILS::to_red_string("Bounds violated"))
            //      std::cout << value << " not in [" << lb << "," << ub << "]" << std::endl;
            //      state->dump();
            //      PLAJA_ABORT;
            //  }
            auto& bound = bounds[state_index];

            // update bounds.
            if (value < bound.first) { bound.first = value; }
            if (value > bound.second) { bound.second = value; }
        }
    }

    // compute box size relative to domain size.
    double box_size_rel = 1;
    for (const auto& [var_index, bound]: bounds) { // Bound: [lower,upper]
        auto lb = model.get_model_information().get_lower_bound_int(var_index);
        auto ub = model.get_model_information().get_upper_bound_int(var_index);
        double var_dom = (ub - lb) + 1;
        double var_box = (bound.second - bound.first) + 1;
        box_size_rel *= var_box / var_dom;
    }
    std::cout << "Box is  ~" << std::fixed << std::setprecision(2) << box_size_rel*100 << "%" << " of state space"
              << std::endl;

    // compute box expression
    auto box = std::make_unique<NaryExpression>(BinaryOpExpression::AND);
    for (int var_index = 0; var_index < var_num; ++var_index) {
        auto lower_bound = std::make_unique<BinaryOpExpression>(BinaryOpExpression::GE);
        auto upper_bound = std::make_unique<BinaryOpExpression>(BinaryOpExpression::LE);
        auto var_dec = model.get_variable(var_index);
        std::unique_ptr<Expression> var_expr = model.gen_var_expr(var_index, var_dec);
        // std::cout << "var: " << var_expr->to_string() << std::endl;
        lower_bound->set_left(std::move(var_expr->deepCopy_Exp()));
        lower_bound->set_right(std::make_unique<IntegerValueExpression>(bounds[var_index + 1].first));
        // lower_bound->dump();

        upper_bound->set_left(std::move(var_expr));
        upper_bound->set_right(std::make_unique<IntegerValueExpression>(bounds[var_index + 1].second));
        // upper_bound->dump();
        box->add_sub(std::move(lower_bound));
        box->add_sub(std::move(upper_bound));
    }
    box->dump(true);
    return std::make_pair(box_size_rel, std::move(box));
}