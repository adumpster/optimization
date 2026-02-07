#include "heuristic.h"
#include "geo.h"
#include "time_utils.h"
#include "config.h"
#include "stubs.h"
#include <iostream>
#include <iomanip>
#include <numeric>
#include <algorithm>

using namespace std;


vector<int> get_sorted_indices_by_tightness(const vector<Employee>& emps) {
    vector<int> indices(emps.size());
    iota(indices.begin(), indices.end(), 0); // Fill with 0, 1, 2, ...

    sort(indices.begin(), indices.end(), [&emps](int a, int b) {
        if (emps[a].due_time != emps[b].due_time) return emps[a].due_time < emps[b].due_time;
        return emps[a].ready_time < emps[b].ready_time;
    });

    return indices;
}


static bool simulate_insertion_and_check(
    const Route& route,
    const Employee& emp,
    int insert_before_idx,
    double speed_kmh,
    const map<string, Employee*>& emp_by_id,
    vector<Stop>& out_stops,
    string& fail_reason
) {
    if (route.stops.empty()) {
        fail_reason = "route has no start";
        return false;
    }

    // Valid insert positions are between START and END (i.e., [1, size-1])
    // where insert_before_idx refers to the index in the *existing* route before which we insert.
    // For example, inserting before END means insert_before_idx == route.stops.size() - 1.
    if (insert_before_idx < 1 || insert_before_idx > (int)route.stops.size() - 1) {
        fail_reason = "invalid insertion position";
        return false;
    }

    // Enforce structure: last stop must be END at office.
    if (route.stops.back().emp_id != "END") {
        fail_reason = "route missing END sentinel";
        return false;
    }

    // Build stop list with the new pickup inserted.
    out_stops.clear();
    out_stops.reserve(route.stops.size() + 1);
    for (int i = 0; i < insert_before_idx; i++) out_stops.push_back(route.stops[i]);

    Stop pu;
    pu.emp_id = emp.id;
    pu.loc = emp.pickup;
    pu.is_pickup = true;
    pu.arrival_time = pu.begin_service = pu.departure_time = 0;
    out_stops.push_back(pu);

    for (int i = insert_before_idx; i < (int)route.stops.size(); i++) out_stops.push_back(route.stops[i]);

    const int SERVICE_PICKUP_MIN = 2;

    // START times are fixed by the vehicle's availability.
    out_stops[0].arrival_time = route.stops[0].arrival_time;
    out_stops[0].begin_service = route.stops[0].begin_service;
    out_stops[0].departure_time = route.stops[0].departure_time;
     // After calculating times for the inserted employee:

    // Resimulate timeline.
    for (int i = 1; i < (int)out_stops.size(); i++) {
        // After calculating times for the inserted employee:

        double dist_km = get_dist(out_stops[i - 1].loc, out_stops[i].loc);
        int tmin = travel_minutes(dist_km, speed_kmh);
        int arrival = out_stops[i - 1].departure_time + tmin;
        out_stops[i].arrival_time = arrival;
      // After calculating times for the inserted employee:
        if (out_stops[i].emp_id == "END") {
            // Office end: no service time.
            out_stops[i].begin_service = arrival;
            out_stops[i].departure_time = arrival;
        } else {
            auto it = emp_by_id.find(out_stops[i].emp_id);
            if (it == emp_by_id.end() || it->second == nullptr) {
                fail_reason = "unknown employee in route";
                return false;
            }
            const Employee& ei = *it->second;
            // Respect earliest pickup (ready time): wait if early.
            out_stops[i].begin_service = max(arrival, ei.ready_time);
            out_stops[i].departure_time = out_stops[i].begin_service + SERVICE_PICKUP_MIN;
          
// cout << "[DEBUG] " << ei.id << ": arrival=" << arrival 
//      << ", ready=" << ei.ready_time 
//      << ", begin=" << max(arrival, ei.ready_time) << endl;
out_stops[i].begin_service = max(arrival, ei.ready_time);
        }
    }

    // Enforce latest drop for ALL picked employees: office arrival must be <= due_time.
    const int office_arrival = out_stops.back().arrival_time;
    for (int i = 1; i < (int)out_stops.size() - 1; i++) {
        const string& eid = out_stops[i].emp_id;
        auto it = emp_by_id.find(eid);
        if (it == emp_by_id.end() || it->second == nullptr) continue;
        const Employee& ei = *it->second;
        if (office_arrival > ei.due_time) {
            fail_reason = "latest_drop violated for " + eid;
            return false;
        }
    }

    return true;
}


// Solomon c1 criterion: insertion cost
double calc_c1(const Route& route, const Employee& emp, int pos, double speed_kmh) {
    if (route.stops.size() <= 1) {
        // Only depot exists
        double dist = get_dist(route.stops.front().loc, emp.pickup);
        return MU * dist;
    }
    
    Location prev_loc = (pos == 0) ? route.stops[0].loc : route.stops[pos - 1].loc;
    Location next_loc = (pos >= route.stops.size() - 1) ? route.stops.back().loc : route.stops[pos].loc;
    
    double d_iu = get_dist(prev_loc, emp.pickup);
    double d_uj = get_dist(emp.pickup, next_loc);
    double d_ij = get_dist(prev_loc, next_loc);
    
    double c11 = d_iu + d_uj - MU * d_ij;
    
    int prev_time = (pos == 0) ? route.stops[0].departure_time : route.stops[pos - 1].departure_time;
    double t_iu = (d_iu / speed_kmh) * 60.0;
    int b_u = max((int)(prev_time + t_iu), emp.ready_time);
    
    double c12 = b_u - prev_time;
    
    return ALPHA1 * c11 + ALPHA2 * c12;
}

// Solomon c2 criterion: customer selection
double calc_c2(const Route& route, const Employee& emp, double c1_val, double speed_kmh) {
    double d_0u = get_dist(route.stops[0].loc, emp.pickup);
    return LAMBDA * d_0u - c1_val;
}

bool check_compatibility(const Vehicle& v, const Employee& e, const Route& route) {
    if (e.veh_pref == PREMIUM && v.category != PREMIUM) return false;
    // if (e.veh_pref == NORMAL && v.category == PREMIUM) return false;
    
    int emp_limit = 100;
    // if (e.share_pref == SINGLE) emp_limit = 1;
    // else if (e.share_pref == DOUBLE) emp_limit = 2;
    // else if (e.share_pref == TRIPLE) emp_limit = 3;
    
    int effective_capacity = min(route.max_capacity, min((int)v.capacity, emp_limit));
    // route.current_capacity is number of passengers already in this trip.
    if (route.current_capacity + 1 > effective_capacity) return false;
    
    return true;
}

// Simple regret-2 implementation
double calculate_regret(const Route& route, const Employee& emp, 
                       double speed_kmh, const map<string, Employee*>& emp_by_id) {
    double best_c1 = INF, second_best_c1 = INF;
    int best_pos = -1;
    
    for (int pos = 1; pos <= (int)route.stops.size() - 1; pos++) {
        vector<Stop> cand;
        string why;
        if (!simulate_insertion_and_check(route, emp, pos, speed_kmh, emp_by_id, cand, why)) {
            continue;
        }
        
        double new_dist = recompute_distance_km(cand);
        double old_dist = recompute_distance_km(route.stops);
        double c1_val = MU * max(0.0, new_dist - old_dist);
        
        if (c1_val < best_c1) {
            second_best_c1 = best_c1;
            best_c1 = c1_val;
            best_pos = pos;
        } else if (c1_val < second_best_c1) {
            second_best_c1 = c1_val;
        }
    }
    
    if (best_pos == -1) return -INF; // No feasible insertion
    if (second_best_c1 == INF) return 0; // Only one feasible position
    
    return second_best_c1 - best_c1; // Regret value
}
// SOLOMON I1 HEURISTIC IMPLEMENTATION
void solve_solomon_insertion(vector<Employee>& emps, vector<Vehicle>& vehs, bool debug) {
    
    cout << "--- Solomon I1 Insertion Heuristic (No Priority) ---\n" << endl;
    // Build quick lookup for time windows.
    map<string, Employee*> emp_by_id;
    for (auto& e : emps) emp_by_id[e.id] = &e;

    // Step 1: Initialize 1 open trip per vehicle
    for (auto& v : vehs) {
        if (v.available_time == 0) v.available_time = parse_time("08:00");
        Route initial_route;
        
        // Trip #1 start: vehicle's dataset start location.
        Stop depot_start;
        depot_start.emp_id = "START";
        depot_start.loc = v.current_loc;
        depot_start.arrival_time = v.available_time;
        depot_start.begin_service = v.available_time;
        depot_start.departure_time = v.available_time;
        depot_start.is_pickup = false;
        
        initial_route.stops.push_back(depot_start);

        // Trip end sentinel: the common corporate office.
        Stop depot_end = depot_start;
        depot_end.emp_id = "END";
        depot_end.loc = OFFICE;
        initial_route.stops.push_back(depot_end);
        initial_route.current_capacity = 0;
        initial_route.max_capacity = (int)v.capacity;
        initial_route.total_distance = 0;
        initial_route.total_cost = 0;
        
        v.routes.push_back(initial_route);
    }
    
    if (debug) {
        cout << "[DEBUG] Initialized " << vehs.size() << " routes (one per vehicle)" << endl;
        cout << "[DEBUG] Processing employees in order: ";
        for (const auto& e : emps) cout << e.id << " ";
        cout << "\n" << endl;
    }
    vector<int> sorted_indices = get_sorted_indices_by_tightness(emps);
    // Step 2: Insert employees one by one using Solomon's criteria
    for (size_t order_idx = 0; order_idx < sorted_indices.size(); order_idx++) {
         int emp_idx = sorted_indices[order_idx];
         if (emps[emp_idx].is_routed) continue;
    
        
        if (debug) cout << "\n[" << emps[emp_idx].id << "] Finding best insertion..." << endl;
        
        double best_c2 = -INF;
        int best_vehicle_idx = -1;
        int best_route_idx = -1;
        int best_insert_pos = -1;
        double best_c1 = INF;
        
        // Try inserting in ALL routes of ALL vehicles
        for (size_t v_idx = 0; v_idx < vehs.size(); v_idx++) {
            Vehicle& veh = vehs[v_idx];
            
            for (size_t r_idx = 0; r_idx < veh.routes.size(); r_idx++) {
                // Only build on the vehicle's current (last) trip; earlier trips are already finished.
                if (r_idx + 1 != veh.routes.size()) continue;
                Route& route = veh.routes[r_idx];
                
                // Check compatibility
                if (!check_compatibility(veh, emps[emp_idx], route)) {
                    if (debug) cout << "  [" << veh.id << "-R" << r_idx << "] Incompatible" << endl;
                    continue;
                }
                
                // Try all insertion positions (insert pickup BEFORE index insert_before_idx)
                double best_c1_this_route = INF;
                int best_insert_before_this_route = -1;
                vector<Stop> best_stops_this_route;

                const double old_dist = recompute_distance_km(route.stops);
                // Valid positions are 1..size-1 (i.e., between START and END)
                for (int insert_before_idx = 1; insert_before_idx <= (int)route.stops.size() - 1; insert_before_idx++) {
                    vector<Stop> cand;
                    string why;
                    if (!simulate_insertion_and_check(route, emps[emp_idx], insert_before_idx, veh.speed_kmh, emp_by_id, cand, why)) {
                        continue;
                    }

                    const double new_dist = recompute_distance_km(cand);
                    const double delta = max(0.0, new_dist - old_dist);
                    // const double c1_val = MU * delta;
                       const double c1_val = calc_c1( route,emps[emp_idx],insert_before_idx,veh.speed_kmh);
                    if (c1_val < best_c1_this_route) {
                        best_c1_this_route = c1_val;
                        best_insert_before_this_route = insert_before_idx;
                        best_stops_this_route = std::move(cand);
                    }
                }

                // If we found a feasible insertion in this route
                if (best_insert_before_this_route != -1) {
                    const double d_0u = get_dist(route.stops.front().loc, emps[emp_idx].pickup);
                    const double regret = calculate_regret(route, emps[emp_idx], veh.speed_kmh, emp_by_id);
                    const double c2_val = LAMBDA * d_0u - best_c1_this_route+0.5*regret;

                    if (debug) {
                        cout << "  [" << veh.id << "-R" << r_idx << "] c1=" << best_c1_this_route
                             << ", c2=" << c2_val << ", insert_before=" << best_insert_before_this_route << endl;
                    }

                    if (c2_val > best_c2) {
                        best_c2 = c2_val;
                        best_vehicle_idx = (int)v_idx;
                        best_route_idx = (int)r_idx;
                        best_insert_pos = best_insert_before_this_route;
                        best_c1 = best_c1_this_route;
                        // Stash the actual schedule so we can apply it later.
                        // (We re-simulate once more below to keep this patch minimal.)
                    }
                }
            }
        }
        
        // Insert employee into best position
        if (best_vehicle_idx != -1) {
            Vehicle& veh = vehs[best_vehicle_idx];
            Route& route = veh.routes[best_route_idx];

            vector<Stop> new_stops;
            string why;
            if (!simulate_insertion_and_check(route, emps[emp_idx], best_insert_pos, veh.speed_kmh, emp_by_id, new_stops, why)) {
                // This should be rare; treat as unrouted with an explicit reason.
                g_unrouted_reason[emps[emp_idx].id] = "Insertion became infeasible at apply-time: " + why;
                continue;
            }

            // Update route capacity limit based on the strictest sharing pref in this trip.
            int emp_limit = 100;
            // if (emps[emp_idx].share_pref == SINGLE) emp_limit = 1;
            // else if (emps[emp_idx].share_pref == DOUBLE) emp_limit = 2;
            // else if (emps[emp_idx].share_pref == TRIPLE) emp_limit = 3;
            route.max_capacity = min(route.max_capacity, min((int)veh.capacity, emp_limit));

            route.stops = std::move(new_stops);
            route.current_capacity++;

            route.total_distance = recompute_distance_km(route.stops);
            route.total_cost = route.total_distance * veh.cost_per_km;
            // Vehicle becomes available again at office (END).
            veh.available_time = route.stops.back().departure_time;
            veh.current_loc = route.stops.back().loc;

            emps[emp_idx].is_routed = true;
            g_unrouted_reason.erase(emps[emp_idx].id);

            if (debug) {
                cout << "  >>> INSERTED into " << veh.id << "-R" << best_route_idx
                     << " after stop#" << best_insert_pos << " (c2=" << best_c2 << ")" << endl;
            }

        } else {
            // No feasible insertion found - start new route on least-cost vehicle
            if (debug) cout << "  >>> Starting NEW ROUTE" << endl;
            
            bool started = false;
            string fail_reason = "No feasible insertion and could not start a new trip (category/capacity/time window)";

            // Find any vehicle that can accommodate as a brand-new trip
            for (auto& v : vehs) {
                // Check if we can start a new route
                int emp_limit = 100;
                // if (emps[emp_idx].share_pref == SINGLE) emp_limit = 1;
                // else if (emps[emp_idx].share_pref == DOUBLE) emp_limit = 2;
                // else if (emps[emp_idx].share_pref == TRIPLE) emp_limit = 3;
                
                if (emp_limit == 0) continue;
                if (emps[emp_idx].veh_pref == PREMIUM && v.category != PREMIUM) continue;
                // if (emps[emp_idx].veh_pref == NORMAL && v.category == PREMIUM) continue;
                
                // Create new route (Trip #2+): must start from OFFICE.
                Route new_route;

                Stop depot;
                depot.emp_id = "START";
                depot.loc = OFFICE; // subsequent trips start from office hub
                depot.arrival_time = v.available_time;
                depot.begin_service = v.available_time;
                depot.departure_time = v.available_time;
                depot.is_pickup = false;
                new_route.stops.push_back(depot);

                Stop depot_end = depot;
                depot_end.emp_id = "END";
                depot_end.loc = OFFICE;
                new_route.stops.push_back(depot_end);

                vector<Stop> planned;
                string why;
                if (!simulate_insertion_and_check(new_route, emps[emp_idx], 1, v.speed_kmh, emp_by_id, planned, why)) {
                    fail_reason = "Could not start a new trip: " + why;
                    continue;
                }

                new_route.stops = std::move(planned);
                new_route.current_capacity = 1;
                new_route.max_capacity = min((int)v.capacity, emp_limit);
                new_route.total_distance = recompute_distance_km(new_route.stops);
                new_route.total_cost = new_route.total_distance * v.cost_per_km;

                v.available_time = new_route.stops.back().departure_time;
                v.current_loc = new_route.stops.back().loc;
                v.routes.push_back(std::move(new_route));
                emps[emp_idx].is_routed = true;
                g_unrouted_reason.erase(emps[emp_idx].id);

                if (debug) cout << "  >>> NEW ROUTE started on " << v.id << endl;
                started = true;
                break;
            }

            if (!started) {
                g_unrouted_reason[emps[emp_idx].id] = fail_reason;
                if (debug) cout << "  !!! DROPPED " << emps[emp_idx].id << " : " << fail_reason << endl;
            }
        }
    }

    // Recompute vehicle totals cleanly
    for (auto& v : vehs) {
        v.total_cost = 0.0;
        for (auto& r : v.routes) {
            // ensure costs are consistent
            r.total_distance = recompute_distance_km(r.stops);
            r.total_cost = r.total_distance * v.cost_per_km;
            if (r.stops.size() > 1) v.total_cost += r.total_cost;
        }
    }

    cout << "\n--- Optimization Complete ---\n" << endl;
}

