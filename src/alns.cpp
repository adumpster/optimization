#include "alns.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_set>
#include <unordered_map>
#include <limits>
#include <iostream>
#include "geo.h"
#include "config.h"

// ------------------------------------------------------------
// REQUIRED HOOKS: wire these to your existing code
// ------------------------------------------------------------
//
// 1) Evaluate & recompute one route after modifying stops.
//    Must update: route feasibility (return bool), route.total_cost, route.total_distance,
//    and stop arrival/departure times.
//    You likely already have this logic in heuristic.cpp.
//
static bool HOOK_simulate_route(Route& route, const Vehicle& vehicle,
                                const std::vector<Employee>& employees) {
    if (route.stops.size() < 2) return false;
    if (route.stops.back().emp_id != "END") return false;

    std::unordered_map<std::string, const Employee*> emp_by_id;
    emp_by_id.reserve(employees.size() * 2);
    for (const auto& e : employees) emp_by_id.emplace(e.id, &e);

    const int SERVICE_PICKUP_MIN = 2;

    if (route.stops.front().emp_id != "START") {
        route.stops.front().emp_id = "START";
        route.stops.front().is_pickup = false;
    }
    if (route.stops.back().emp_id == "END" &&
        route.stops.back().loc.lat == 0.0 && route.stops.back().loc.lng == 0.0) {
        route.stops.back().loc = OFFICE;
    }

    for (size_t i = 1; i < route.stops.size(); i++) {
        Stop& s = route.stops[i];
        if (s.emp_id == "END") {
            s.is_pickup = false;
        } else {
            auto it = emp_by_id.find(s.emp_id);
            if (it == emp_by_id.end()) return false;
            const Employee& e = *it->second;
            s.loc = e.pickup;
            s.is_pickup = true;
        }
    }

    for (size_t i = 1; i < route.stops.size(); i++) {
        Stop& prev = route.stops[i - 1];
        Stop& cur = route.stops[i];
        double dist_km = get_dist(prev.loc, cur.loc);
        int tmin = travel_minutes(dist_km, vehicle.speed_kmh);
        int arrival = prev.departure_time + tmin;
        cur.arrival_time = arrival;

        if (cur.emp_id == "END") {
            cur.begin_service = arrival;
            cur.departure_time = arrival;
        } else {
            const Employee& e = *emp_by_id[cur.emp_id];
            int begin_service = arrival < e.ready_time ? e.ready_time : arrival;
            cur.begin_service = begin_service;
            cur.departure_time = begin_service + SERVICE_PICKUP_MIN;
        }
    }

    const int office_arrival = route.stops.back().arrival_time;
    for (size_t i = 1; i + 1 < route.stops.size(); i++) {
        const Stop& s = route.stops[i];
        if (s.emp_id == "START" || s.emp_id == "END") continue;
        const Employee& e = *emp_by_id[s.emp_id];
        if (office_arrival > e.due_time) return false;
    }

    route.current_capacity = (int)route.stops.size() - 2;
    if (route.current_capacity < 0) route.current_capacity = 0;

    double total_dist = 0.0;
    for (size_t i = 1; i < route.stops.size(); i++) {
        total_dist += get_dist(route.stops[i - 1].loc, route.stops[i].loc);
    }
    route.total_distance = total_dist;
    route.total_cost = total_dist * vehicle.cost_per_km;
    return true;
}

//
// 2) Remove employee from route (by id). Must erase exactly the pickup stop.
//    Should NOT break START/END.
//    After removal you should call HOOK_simulate_route(route,...).
//
static bool HOOK_remove_employee_from_route(Route& route,
                                           const std::string& emp_id) {
    // Default implementation based on Stop fields used in your output_json.cpp:
    // Stop has: emp_id and is_pickup
    auto before = route.stops.size();
    route.stops.erase(
        std::remove_if(route.stops.begin(), route.stops.end(),
                       [&](const Stop& s){ return s.is_pickup && s.emp_id == emp_id; }),
        route.stops.end()
    );
    return route.stops.size() != before;
}

//
// 3) Try to insert employee into a route at best position.
//    Returns true if inserted (feasible) and updates route.
//    You can reuse your Solomon "best insertion position" logic.
//    If you already have a function like best_insertion(route, emp, vehicle) use it.
//
static bool HOOK_best_insert(Route& route,
                             const Vehicle& vehicle,
                             const Employee& emp,
                             const std::vector<Employee>& employees) {
    // Generic fallback (slow but simple): try all insertion positions
    // between index 1..(n-1) so START/END fixed.
    // NOTE: this assumes route.stops includes START and END.
    int n = (int)route.stops.size();
    if (n < 2) return false;

    double best_cost = std::numeric_limits<double>::infinity();
    std::vector<Stop> best_stops;

    for (int pos = 1; pos <= n-1; pos++) {
        Route cand = route;

        Stop pickup;
        pickup.emp_id = emp.id;
        pickup.is_pickup = true;
        // If your Stop has location, set it here (if not, your simulator
        // probably looks up emp pickup from employees vector by id)
        // pickup.loc = emp.pickup;

        cand.stops.insert(cand.stops.begin() + pos, pickup);

        if (!HOOK_simulate_route(cand, vehicle, employees)) continue;

        if (cand.total_cost < best_cost) {
            best_cost = cand.total_cost;
            best_stops = cand.stops;
        }
    }

    if (best_stops.empty()) return false;

    route.stops = std::move(best_stops);
    // recompute final numbers
    return HOOK_simulate_route(route, vehicle, employees);
}

// Optional: hook into your 2-opt if you have it (keep false by default)
static void HOOK_two_opt_vehicle(std::vector<Employee>& employees,
                                 Vehicle& vehicle,
                                 bool debug) {
    (void)employees; (void)vehicle; (void)debug;
    // TODO: call your two-opt-all-routes on this vehicle if you implemented it.
}


// ------------------------------------------------------------
// Internal helpers
// ------------------------------------------------------------
static double total_solution_cost(const std::vector<Vehicle>& vehicles) {
    double c = 0.0;
    for (const auto& v : vehicles) c += v.total_cost;
    return c;
}

static int routed_count(const std::vector<Employee>& employees) {
    int c = 0;
    for (const auto& e : employees) if (e.is_routed) c++;
    return c;
}

// Lexicographic score: prioritize routing all employees, then cost.
// Convert to single scalar using big penalty M.
static double score_solution(const std::vector<Employee>& employees,
                             const std::vector<Vehicle>& vehicles) {
    int n = (int)employees.size();
    int r = routed_count(employees);
    double cost = total_solution_cost(vehicles);
    const double M = 1e9; // huge penalty per unrouted
    return (double)(n - r) * M + cost;
}

struct LocatedEmp {
    std::string emp_id;
    int veh_idx;
    int route_idx;
};

static std::vector<LocatedEmp> collect_routed_emps(const std::vector<Vehicle>& vehicles) {
    std::vector<LocatedEmp> out;
    for (int vi = 0; vi < (int)vehicles.size(); vi++) {
        const auto& v = vehicles[vi];
        for (int ri = 0; ri < (int)v.routes.size(); ri++) {
            const auto& r = v.routes[ri];
            for (const auto& s : r.stops) {
                if (s.is_pickup) out.push_back({s.emp_id, vi, ri});
            }
        }
    }
    return out;
}

static Employee* find_employee(std::vector<Employee>& employees, const std::string& id) {
    for (auto& e : employees) if (e.id == id) return &e;
    return nullptr;
}

static const Employee* find_employee_const(const std::vector<Employee>& employees, const std::string& id) {
    for (const auto& e : employees) if (e.id == id) return &e;
    return nullptr;
}

// Similarity metric for Shaw removal (distance+time window proximity)
static double similarity(const Employee& a, const Employee& b) {
    // You can improve with geo distance if you have it accessible.
    // For now: time proximity only (safe fallback).
    double dt = std::abs(a.ready_time - b.ready_time) + std::abs(a.due_time - b.due_time);
    return dt;
}

// ------------------------------------------------------------
// Destroy operators
// ------------------------------------------------------------
enum DestroyOp { RANDOM_REMOVE = 0, SHAW_REMOVE = 1, WORST_REMOVE = 2 };

static std::vector<std::string> destroy_random(std::vector<Employee>& employees,
                                               const std::vector<Vehicle>& vehicles,
                                               int q,
                                               std::mt19937& rng) {
    auto routed = collect_routed_emps(vehicles);
    std::shuffle(routed.begin(), routed.end(), rng);

    std::vector<std::string> removed;
    for (int i = 0; i < (int)routed.size() && (int)removed.size() < q; i++) {
        removed.push_back(routed[i].emp_id);
    }
    return removed;
}

static std::vector<std::string> destroy_shaw(std::vector<Employee>& employees,
                                             const std::vector<Vehicle>& vehicles,
                                             int q,
                                             std::mt19937& rng) {
    auto routed = collect_routed_emps(vehicles);
    if (routed.empty()) return {};

    std::uniform_int_distribution<int> pick(0, (int)routed.size()-1);
    std::string seed_id = routed[pick(rng)].emp_id;

    const Employee* seed = find_employee_const(employees, seed_id);
    if (!seed) return {};

    // rank by similarity (low = more similar)
    struct Cand { std::string id; double sim; };
    std::vector<Cand> cands;
    cands.reserve(routed.size());
    for (const auto& le : routed) {
        const Employee* e = find_employee_const(employees, le.emp_id);
        if (!e) continue;
        cands.push_back({le.emp_id, similarity(*seed, *e)});
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& x, const Cand& y){ return x.sim < y.sim; });

    std::vector<std::string> removed;
    for (int i = 0; i < (int)cands.size() && (int)removed.size() < q; i++) {
        removed.push_back(cands[i].id);
    }
    return removed;
}

// Worst removal: approximate “contribution” by removing and seeing cost delta (slow but effective)
static std::vector<std::string> destroy_worst(std::vector<Employee>& employees,
                                             std::vector<Vehicle> vehicles_copy,
                                             int q) {
    // This is expensive; keep q small.
    struct Cand { std::string id; double gain; };
    std::vector<Cand> scored;

    double base_cost = total_solution_cost(vehicles_copy);

    for (int vi = 0; vi < (int)vehicles_copy.size(); vi++) {
        auto& v = vehicles_copy[vi];
        for (int ri = 0; ri < (int)v.routes.size(); ri++) {
            auto& r = v.routes[ri];

            // collect ids in this route
            std::vector<std::string> ids;
            for (const auto& s : r.stops) if (s.is_pickup) ids.push_back(s.emp_id);

            for (const auto& id : ids) {
                Vehicle vbak = v;
                Route rbak = r;

                if (!HOOK_remove_employee_from_route(r, id)) continue;
                if (!HOOK_simulate_route(r, v, employees)) { r = rbak; continue; }

                // update vehicle cost locally
                double vc = 0.0;
                for (const auto& rr : v.routes) vc += rr.total_cost;
                v.total_cost = vc;

                double new_cost = total_solution_cost(vehicles_copy);
                double gain = base_cost - new_cost;

                scored.push_back({id, gain});

                // restore
                v = vbak;
                r = rbak;
            }
        }
    }

    std::sort(scored.begin(), scored.end(), [](const Cand& a, const Cand& b){ return a.gain > b.gain; });

    std::vector<std::string> removed;
    for (int i = 0; i < (int)scored.size() && (int)removed.size() < q; i++) {
        removed.push_back(scored[i].id);
    }
    return removed;
}


// ------------------------------------------------------------
// Apply removal to the real solution
// ------------------------------------------------------------
static void apply_removals(std::vector<Employee>& employees,
                           std::vector<Vehicle>& vehicles,
                           const std::vector<std::string>& removed_ids) {
    std::unordered_set<std::string> rem(removed_ids.begin(), removed_ids.end());

    // mark unrouted
    for (auto& e : employees) {
        if (rem.count(e.id)) e.is_routed = false;
    }

    // remove from routes
    for (auto& v : vehicles) {
        for (auto& r : v.routes) {
            bool changed = false;
            for (const auto& id : removed_ids) {
                if (HOOK_remove_employee_from_route(r, id)) changed = true;
            }
            if (changed) {
                // recompute route
                HOOK_simulate_route(r, v, employees);
            }
        }

        // recompute vehicle total
        v.total_cost = 0.0;
        for (const auto& r : v.routes) v.total_cost += r.total_cost;
    }
}


// ------------------------------------------------------------
// Repair operators
// ------------------------------------------------------------
static bool try_insert_anywhere(std::vector<Employee>& employees,
                                std::vector<Vehicle>& vehicles,
                                const Employee& emp) {
    // Try best insertion across all routes.
    double best_cost = std::numeric_limits<double>::infinity();
    int best_vi = -1, best_ri = -1;
    Route best_route;

    for (int vi = 0; vi < (int)vehicles.size(); vi++) {
        auto& v = vehicles[vi];
        for (int ri = 0; ri < (int)v.routes.size(); ri++) {
            Route cand = v.routes[ri];
            if (!HOOK_best_insert(cand, v, emp, employees)) continue;
            if (cand.total_cost < best_cost) {
                best_cost = cand.total_cost;
                best_vi = vi;
                best_ri = ri;
                best_route = std::move(cand);
            }
        }
    }

    if (best_vi == -1) return false;

    vehicles[best_vi].routes[best_ri] = std::move(best_route);
    // update vehicle total
    vehicles[best_vi].total_cost = 0.0;
    for (const auto& r : vehicles[best_vi].routes) vehicles[best_vi].total_cost += r.total_cost;

    // mark routed
    for (auto& e : employees) if (e.id == emp.id) { e.is_routed = true; break; }
    return true;
}

static void repair_greedy(std::vector<Employee>& employees,
                          std::vector<Vehicle>& vehicles,
                          std::vector<std::string>& removed_ids) {
    // Insert in given order
    for (const auto& id : removed_ids) {
        const Employee* ep = find_employee_const(employees, id);
        if (!ep) continue;
        (void)try_insert_anywhere(employees, vehicles, *ep);
    }
}

// Regret-2: insert hardest first
static void repair_regret2(std::vector<Employee>& employees,
                           std::vector<Vehicle>& vehicles,
                           std::vector<std::string> removed_ids) {
    std::unordered_set<std::string> remaining(removed_ids.begin(), removed_ids.end());

    while (!remaining.empty()) {
        std::string best_id;
        double best_regret = -1.0;

        // For each remaining employee, compute best and 2nd best insertion costs
        for (const auto& id : remaining) {
            const Employee* emp = find_employee_const(employees, id);
            if (!emp) continue;

            double best = std::numeric_limits<double>::infinity();
            double second = std::numeric_limits<double>::infinity();

            for (int vi = 0; vi < (int)vehicles.size(); vi++) {
                const auto& v = vehicles[vi];
                for (int ri = 0; ri < (int)v.routes.size(); ri++) {
                    Route cand = v.routes[ri];
                    if (!HOOK_best_insert(cand, v, *emp, employees)) continue;
                    double c = cand.total_cost;
                    if (c < best) { second = best; best = c; }
                    else if (c < second) { second = c; }
                }
            }

            if (!std::isfinite(best)) continue; // cannot insert anywhere

            double regret = std::isfinite(second) ? (second - best) : 1e6; // if only one option, huge regret
            if (regret > best_regret) {
                best_regret = regret;
                best_id = id;
            }
        }

        if (best_id.empty()) {
            // cannot insert any remaining -> stop
            break;
        }

        const Employee* chosen = find_employee_const(employees, best_id);
        if (chosen) {
            (void)try_insert_anywhere(employees, vehicles, *chosen);
        }
        remaining.erase(best_id);
    }
}


// ------------------------------------------------------------
// Operator selection with adaptive weights
// ------------------------------------------------------------
static int pick_weighted(const std::vector<double>& w, std::mt19937& rng) {
    double sum = 0.0;
    for (double x : w) sum += x;
    std::uniform_real_distribution<double> U(0.0, sum);
    double r = U(rng);
    for (int i = 0; i < (int)w.size(); i++) {
        r -= w[i];
        if (r <= 0.0) return i;
    }
    return (int)w.size() - 1;
}


// ------------------------------------------------------------
// ALNS main
// ------------------------------------------------------------
void run_alns(std::vector<Employee>& employees,
              std::vector<Vehicle>& vehicles,
              const ALNSConfig& cfg,
              bool debug) {

    std::mt19937 rng((unsigned)std::random_device{}());

    // weights for destroy ops
    std::vector<double> w_destroy = {1.0, 1.0, 1.0}; // random, shaw, worst

    // current solution is given
    double best_score = score_solution(employees, vehicles);
    double curr_score = best_score;

    auto best_emps = employees;
    auto best_vehs = vehicles;

    double T = cfg.T0;
    int no_improve = 0;

    std::uniform_int_distribution<int> remove_dist(cfg.min_remove, cfg.max_remove);
    std::uniform_real_distribution<double> U01(0.0, 1.0);

    for (int it = 1; it <= cfg.iterations; it++) {
        int q = remove_dist(rng);

        // pick destroy operator
        int d = pick_weighted(w_destroy, rng);

        // copy current solution
        auto trial_emps = employees;
        auto trial_vehs = vehicles;

        // choose removed set
        std::vector<std::string> removed;
        if (d == RANDOM_REMOVE) removed = destroy_random(trial_emps, trial_vehs, q, rng);
        else if (d == SHAW_REMOVE) removed = destroy_shaw(trial_emps, trial_vehs, q, rng);
        else removed = destroy_worst(trial_emps, trial_vehs, q);

        if (removed.empty()) continue;

        // apply removals
        apply_removals(trial_emps, trial_vehs, removed);

        // repair
        if (cfg.use_regret2) repair_regret2(trial_emps, trial_vehs, removed);
        else repair_greedy(trial_emps, trial_vehs, removed);

        // optional route-level polish
        if (cfg.apply_two_opt_after_repair) {
            for (auto& v : trial_vehs) HOOK_two_opt_vehicle(trial_emps, v, debug);
        }

        double trial_score = score_solution(trial_emps, trial_vehs);
        double delta = trial_score - curr_score;

        bool accept = false;
        if (delta <= 0.0) accept = true;
        else {
            double prob = std::exp(-delta / std::max(1e-9, T));
            if (U01(rng) < prob) accept = true;
        }

        // update weights (simple reward scheme)
        double reward = 0.0;

        if (accept) {
            employees = std::move(trial_emps);
            vehicles = std::move(trial_vehs);
            curr_score = trial_score;

            // accepted
            reward = 0.2;

            if (trial_score < best_score) {
                best_score = trial_score;
                best_emps = employees;
                best_vehs = vehicles;
                reward = 2.0; // big reward for new best
                no_improve = 0;
            } else {
                no_improve++;
            }
        } else {
            no_improve++;
        }

        // weight update (exponential smoothing)
        const double rho = 0.15;
        w_destroy[d] = (1.0 - rho) * w_destroy[d] + rho * (1.0 + reward);

        // cool down
        T *= cfg.cooling;

        if (debug && it % 100 == 0) {
            std::cout << "[ALNS] it=" << it
                      << " curr=" << curr_score
                      << " best=" << best_score
                      << " T=" << T
                      << " q=" << q
                      << " w=(" << w_destroy[0] << "," << w_destroy[1] << "," << w_destroy[2] << ")\n";
        }

        if (no_improve >= cfg.no_improve_stop) break;
    }

    // restore best
    employees = std::move(best_emps);
    vehicles = std::move(best_vehs);
}
