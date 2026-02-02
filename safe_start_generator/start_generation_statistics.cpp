//
// Created by Daniel Sherbakov in 2025.
//

#include "start_generation_statistics.h"

#include "../../exception/not_implemented_exception.h"
#include "../../search/fd_adaptions/search_statistics.h"
#include "../../stats/stats_base.h"
#include <fstream>
#include <numeric>
#include <sstream>

StartGenerationStatistics::StartGenerationStatistics(const std::string& file):
    file(file) {

    };

StartGenerationStatistics::~StartGenerationStatistics() = default;

void StartGenerationStatistics::add_basic_stats(PLAJA::StatsBase& stats) {

    SEARCH_STATISTICS::add_basic_stats(stats);

    stats.add_unsigned_stats(
        std::list<PLAJA::StatsUnsigned> {
            PLAJA::StatsUnsigned::ITERATIONS,
            PLAJA::StatsUnsigned::START_STATES,
            PLAJA::StatsUnsigned::UNSAFE_PATHS,
            PLAJA::StatsUnsigned::UNSAFE_STATES,
            PLAJA::StatsUnsigned::TESTING_FAILED,
            PLAJA::StatsUnsigned::DEAD_ENDS,
            PLAJA::StatsUnsigned::CYCLES,
            //
            PLAJA::StatsUnsigned::UNSAFE_STATES_VERIFIED,
        },
        std::list<std::string> {
            "Iterations",
            "StartStates",
            "UnsafePaths",
            "UnsafeStates",
            "TestingFailed",
            "DeadEnds",
            "Cycles",
            //
            "UnsafeStatesVerified",
        },
        0);

    stats.add_double_stats(
        std::list<PLAJA::StatsDouble> {
            PLAJA::StatsDouble::TOTAL_REFINING_TIME,
            PLAJA::StatsDouble::TOTAL_TESTING_TIME,
            PLAJA::StatsDouble::TOTAL_VERIFICATION_TIME,
        },
        std::list<std::string> {
            "TotalRefiningTime",
            "TotalTestingTime",
            "TotalVerificationTime",
        },
        0);
}

// ################################### specific for per iteration stats ###################################

void StartGenerationStatistics::inc_attr_double(const PLAJA::StatsDouble attr, const double inc) {
    switch (attr) {
        case PLAJA::StatsDouble::REFINING_TIME: refining_time = inc; break;
        case PLAJA::StatsDouble::SEARCHING_TIME: search_time = inc; break;
        case PLAJA::StatsDouble::UNSAFETY_EVAL: unsafety_eval = inc; break;
        case PLAJA::StatsDouble::BOX_SIZE: box_size = inc; break;
        default: throw NotImplementedException("StartGenerationStatistics::inc_attr_double");
    }
}
void StartGenerationStatistics::inc_unsigned(const PLAJA::StatsUnsigned attr, const size_t inc) {
    switch (attr) {
        case PLAJA::StatsUnsigned::UNSAFE_STATES: unsafe_states = inc; break;
        case PLAJA::StatsUnsigned::TIME_LIMIT_REACHED: sampling_timelimit_reached = inc; break;
        default: throw NotImplementedException("StartGenerationStatistics::inc_unsigned");
    }
}

void StartGenerationStatistics::reset() {
    unsafe_states = 0;
    iteration_mode = "";
    search_time = 0;
    refining_time = 0;
    unsafety_eval = 0;
    sampling_timelimit_reached = 0;
    box_size = 0;
}

void StartGenerationStatistics::testing_iteration() {
    iteration_mode = "Testing";
}

void StartGenerationStatistics::verification_iteration() {
    iteration_mode = "Verification";
}

void StartGenerationStatistics::set_start_condition_status(const bool safe) {
    iteration_mode = "Start_Checking";
    if (safe) {start_condition_safe = "SAFE";}
    else {start_condition_safe = "NOT_SAFE";}
}


void StartGenerationStatistics::dump_to_csv() {
    if (not header_written) {
        dump_names_to_csv();
        header_written = true;
    }
    file << iteration << PLAJA_UTILS::commaString;
    file << iteration_mode << PLAJA_UTILS::commaString;
    file << unsafe_states << PLAJA_UTILS::commaString;
    file << search_time << PLAJA_UTILS::commaString;
    file << refining_time << PLAJA_UTILS::commaString;
    file << unsafety_eval << PLAJA_UTILS::commaString;
    file << sampling_timelimit_reached << PLAJA_UTILS::commaString;
    file << box_size << PLAJA_UTILS::commaString;
    file << start_condition_safe;
    file << std::endl;
    iteration++;
    reset();
}

void StartGenerationStatistics::dump_names_to_csv() {
    const std::list<std::string> headers = {
        "Iteration",
        "IterationMode",
        "UnsafeStates",
        "SearchTime",
        "RefiningTime",
        "UnsafetyEval",
        "SamplingTimeLimitReached",
        "BoxSize",
        "StartConditionSafe",
    };

    auto it = headers.begin();
    while (it != headers.end()) {
        file << *it;
        ++it;                      // Move to the next element
        if (it != headers.end()) { // If it's not the last element, print the comma
            file << PLAJA_UTILS::commaString;
        }
    }
    file << std::endl;
}
