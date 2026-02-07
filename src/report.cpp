#include "report.h"
#include "time_utils.h"
#include "config.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

using namespace std;

void display_report(const vector<Vehicle>& vehs, const vector<Employee>& emps) {
    double grand_total = 0;
    int total_routed = 0;
    int total_trips = 0;

    // Calculate Summary Stats
    for (const auto& v : vehs) {
        grand_total += v.total_cost;
        for (const auto& route : v.routes) {
            if (route.stops.size() > 1) {
                total_trips++;
                for (const auto& stop : route.stops) {
                    if (stop.emp_id != "START" && stop.is_pickup) total_routed++;
                }
            }
        }
    }

    double baseline = 0;
    for (const auto& e : emps) if (e.is_routed) baseline += e.baseline_cost;

    // Start JSON Output
    cout << "{" << endl;
    cout << "  \"summary\": {" << endl;
    cout << "    \"employees_routed\": " << total_routed << "," << endl;
    cout << "    \"total_employees\": " << emps.size() << "," << endl;
    cout << "    \"total_trips\": " << total_trips << "," << endl;
    cout << "    \"optimized_cost\": " << fixed << setprecision(2) << grand_total << "," << endl;
    cout << "    \"baseline_cost\": " << baseline << "," << endl;
    cout << "    \"savings\": " << (baseline - grand_total) << endl;
    cout << "  }," << endl;

    // Vehicles and Routes
    cout << "  \"vehicles\": [" << endl;
    bool first_veh = true;
    for (size_t i = 0; i < vehs.size(); ++i) {
        const auto& v = vehs[i];
        if (v.routes.empty() || (v.routes.size() == 1 && v.routes[0].stops.size() <= 1)) continue;

        if (!first_veh) cout << "," << endl;
        first_veh = false;

        cout << "    {" << endl;
        cout << "      \"vehicle_id\": \"" << v.id << "\"," << endl;
        cout << "      \"category\": \"" << (v.category == PREMIUM ? "Premium" : "Normal") << "\"," << endl;
        cout << "      \"total_cost\": " << v.total_cost << "," << endl;
        cout << "      \"trips\": [" << endl;
        
        bool first_trip = true;
        for (size_t r = 0; r < v.routes.size(); ++r) {
            const Route& route = v.routes[r];
            if (route.stops.size() <= 1) continue;
            
            if (!first_trip) cout << "," << endl;
            first_trip = false;

            cout << "        {" << endl;
            cout << "          \"trip_id\": " << (r + 1) << "," << endl;
            cout << "          \"cost\": " << route.total_cost << "," << endl;
            cout << "          \"stops\": [" << endl;
            
            for (size_t s = 0; s < route.stops.size(); ++s) {
                const auto& stop = route.stops[s];
                cout << "            {\"emp_id\": \"" << stop.emp_id << "\", \"arrival\": \"" << format_time(stop.arrival_time) << "\", \"type\": \"" << (stop.is_pickup ? "pickup" : "dropoff") << "\"}";
                if (s < route.stops.size() - 1) cout << ",";
                cout << endl;
            }
            cout << "          ]" << endl;
            cout << "        }";
        }
        cout << endl << "      ]" << endl;
        cout << "    }";
    }
    cout << endl << "  ]," << endl;

    // Unrouted Employees
    cout << "  \"unrouted\": [" << endl;
    bool first_unrouted = true;
    for (const auto& e : emps) {
        if (!e.is_routed) {
            if (!first_unrouted) cout << "," << endl;
            first_unrouted = false;

            string reason = "(no reason captured)";
            auto it = g_unrouted_reason.find(e.id);
            if (it != g_unrouted_reason.end()) reason = it->second;

            cout << "    {\"emp_id\": \"" << e.id << "\", \"reason\": \"" << reason << "\"}";
        }
    }
    cout << endl << "  ]" << endl;
    cout << "}" << endl;
}