//
// Created by Daniel Sherbakov in 2025.
//

#ifndef TRANSITION_SET_H
#define TRANSITION_SET_H

namespace StartGenerator {

    struct Transition {

        StateID_type src;
        ActionLabel_type label;
        StateID_type successor;

        Transition(StateID_type src, ActionLabel_type label, StateID_type successor):
            src(src)
            , label(label)
            , successor(successor) {
        }

    };

    struct TransitionHash {
        std::size_t operator()(const Transition& transition) const {
            constexpr unsigned prime = 31;
            std::size_t result = 1;
            result = prime * result + transition.src;
            result = prime * result + transition.label;
            result = prime * result + transition.successor;
            return result;
        }
    };

    struct TransitionEqual {
        bool operator()(const Transition& transition1, const Transition& transition2) const {
            return transition1.src == transition2.src and transition1.label == transition2.label and transition1.successor == transition2.successor;
        }
    };

    typedef std::unordered_set<Transition, TransitionHash, TransitionEqual> TransitionSet;

}

#endif //TRANSITION_SET_H
