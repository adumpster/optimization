#include "report.h"
#include "time_utils.h"
#include "config.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

using namespace std;

void display_report(const vector<Vehicle>& vehs, const vector<Employee>& emps) {
    cout << "\n=======================================================" << endl;
    cout << "          VELORA ROUTING SUMMARY                       " << endl;
    cout << "=======================================================" << endl;

    double grand_total = 0;
    int total_routed = 0;
    int total_trips = 0;

    for (const auto& v : vehs) {
        if (v.routes.empty() || v.routes[0].stops.size() <= 1) continue;

        cout << "\n" << v.id << " (" << (v.category == PREMIUM ? "Premium" : "Normal") << "):" << endl;

        for (size_t r = 0; r < v.routes.size(); r++) {
            const Route& route = v.routes[r];
            if (route.stops.size() <= 1) continue;
            
            total_trips++;
            
            // Extract unique employees
            vector<string> employees_in_route;
            for (const auto& stop : route.stops) {
                if (stop.emp_id != "START" && stop.is_pickup) {
                    employees_in_route.push_back(stop.emp_id);
                }
            }
            
            cout << "  Trip #" << (r + 1) << " (" << employees_in_route.size() << " passenger"
                 << (employees_in_route.size() > 1 ? "s" : "") << "):" << endl;
            
            for (const string& eid : employees_in_route) {
                // Find pickup + dropoff times
                string pu_time = "N/A";
                // All passengers share the same dropoff time at END (office).
                string dr_time = format_time(route.stops.back().arrival_time);
                for (const auto& stop : route.stops) {
                    if (stop.emp_id != eid) continue;
                    if (stop.is_pickup) pu_time = format_time(stop.begin_service); // respect earliest_pickup
                }
               
                cout << "    - " << eid << " (Pickup " << pu_time << ", Dropoff " << dr_time << ")" << endl;
                total_routed++;
            }
            
            cout << "    Cost: Rs." << fixed << setprecision(2) << route.total_cost << endl;
        }

        cout << "  Vehicle Total: Rs." << v.total_cost << endl;
        grand_total += v.total_cost;
    }

    cout << "\n-------------------------------------------------------" << endl;
    
    bool has_unrouted = false;
    for (const auto& e : emps) {
        if (!e.is_routed) {
            if (!has_unrouted) {
                cout << "UNROUTED EMPLOYEES:" << endl;
                has_unrouted = true;
            }
            auto it = g_unrouted_reason.find(e.id);
            if (it != g_unrouted_reason.end()) cout << "  - " << e.id << " : " << it->second << endl;
            else cout << "  - " << e.id << " : (no reason captured)" << endl;
        }
    }
    if (!has_unrouted) cout << "All employees routed!" << endl;
    
    double baseline = 0;
    for (const auto& e : emps) if (e.is_routed) baseline += e.baseline_cost;
    
    cout << "\n=======================================================" << endl;
    cout << "SUMMARY:" << endl;
    cout << "  Employees Routed: " << total_routed << "/" << emps.size() << endl;
    cout << "  Total Trips:      " << total_trips << endl;
    cout << "  Optimized Cost:   Rs." << grand_total << endl;
    cout << "  Baseline Cost:    Rs." << baseline << endl;
    double savings = (baseline - grand_total);
    cout << "  Savings:          Rs." << savings;
    if (baseline > 0.0) {
        cout << " (" << (savings / baseline * 100) << "%)";
    }
    cout << endl;
    cout << "=======================================================" << endl;
}

