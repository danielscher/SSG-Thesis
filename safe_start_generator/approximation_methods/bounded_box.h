//
// Created by Daniel Sherbakov in 2025.
//

#ifndef BOUNED_BOX_H
#define BOUNED_BOX_H
#include "../../fd_adaptions/state.h"
#include <boost/functional/hash.hpp>


/**
* Hash function using boost.
*/
struct VectorHash {

    std::size_t operator()(const std::vector<int>& vec) const {
        std::size_t seed = vec.size();
        for (int x : vec) {
            boost::hash_combine(seed,x);
        }
        return seed;
    }
};

using ValuationSet = std::unordered_set<std::vector<int>,VectorHash>;
/**
 * Computes an underapproximation for a set of states by finding a maximal box contained within the set of states.
 */
class BoundedBox {
public:
    static std::pair<size_t,std::unique_ptr<Expression>> compute_bounded_box(
        const std::unordered_set<std::unique_ptr<StateBase>>& state_set,
        const Model& model);

private:
    /// transforms state into a simple integer vector excluding loc variable.
    static std::vector<int> get_state_vec(const StateBase& state);

    /**
     * @brief checks if box represented by corners is bounded by set.
     *
     * A box is bounded by the set of points if all points in the box are contained in the set of points, and it doesn't
     * exceed any domain bounds.
     *
     * The algorithm iterates over all points in box and checks membership in set, starting from the lowest corner.
     *
     * @param min_corner the furthest along the negative direction of the axes corner coordinate.
     * @param max_corner the furthest along the positive direction of the axes corner coordinate.
     * @param point_set set of n-dimensional points that should bound the box.
     *
     * @return true if box is bounded, otherwise false.
     */
    static bool is_bounded(
        const std::vector<int>& min_corner,
        const std::vector<int>& max_corner, ValuationSet& point_set,const Model& model);

};

#endif //BOUNED_BOX_H
