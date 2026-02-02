//
// Created by Daniel Sherbakov in 2025.
//

#ifndef START_GENERATION_STATISTICS_H
#define START_GENERATION_STATISTICS_H

#include "../../assertions.h"
#include "../../stats/stats_base.h"
#include "../../stats/stats_unsigned.h"
#include "../../utils/default_constructors.h"

#include <fstream>
#include <string>
#include <vector>

namespace PLAJA {
    enum class StatsDouble;
    class StatsBase;
} // namespace PLAJA

class StartGenerationStatistics final: public PLAJA::StatsBase {

private:
    std::ofstream file;
    bool header_written = false;
    //
    size_t iteration = 0;
    std::string iteration_mode;
    size_t unsafe_states = 0;
    double search_time = 0; // time of testing or verification.
    double refining_time = 0;
    double unsafety_eval = 0;
    size_t sampling_timelimit_reached = 0;
    double box_size = 0;
    std::string start_condition_safe = "UNKNOWN";

    void dump_names_to_csv();

public:
    explicit StartGenerationStatistics(const std::string& file);
    ~StartGenerationStatistics() override;
    DELETE_CONSTRUCTOR(StartGenerationStatistics)

    static void add_basic_stats(PLAJA::StatsBase& stats);
    /// used by Timer.
    void inc_attr_double(PLAJA::StatsDouble attr, double inc) override;
    void inc_unsigned(PLAJA::StatsUnsigned attr, size_t inc);

    void reset() override;

    void testing_iteration();
    void verification_iteration();

    void set_start_condition_status(bool safe);

    // output
    // void print_statistics() const;
    void dump_to_csv();
};

#endif //START_GENERATION_STATISTICS_H
