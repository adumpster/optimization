#pragma once
#include <vector>
#include "types.h"

// Config is intentionally simple (hackathon friendly).
struct ALNSConfig {
    int iterations = 2000;         // total iterations
    int min_remove = 3;            // min employees removed in ruin
    int max_remove = 12;           // max employees removed in ruin
    int no_improve_stop = 400;     // stop early if no improvement

    // Simulated annealing acceptance
    double T0 = 500.0;             // initial temperature (scaled later if you want)
    double cooling = 0.999;        // T *= cooling

    // Repair style
    bool use_regret2 = true;       // regret-2 insertion vs greedy
    bool apply_two_opt_after_repair = false;
};

// Runs ALNS starting from the current solution already stored in vehicles/routes.
// employees vector will be updated (is_routed etc.) as your solver already does.
void run_alns(std::vector<Employee>& employees,
              std::vector<Vehicle>& vehicles,
              const ALNSConfig& cfg,
              bool debug);
