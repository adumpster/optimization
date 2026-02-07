#include "report.h"
#include "time_utils.h"
#include "config.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

using namespace std;

// src/report.cpp
void display_report(const vector<Vehicle>& vehs, const vector<Employee>& emps) {
    cout << "{" << endl;
    cout << "  \"vehicles\": [" << endl;
    
    for (size_t i = 0; i < vehs.size(); ++i) {
        const auto& v = vehs[i];
        cout << "    {" << endl;
        cout << "      \"vehicle_id\": \"" << v.id << "\"," << endl;
        cout << "      \"total_cost\": " << v.total_cost << "," << endl;
        cout << "      \"trips\": [" << endl;
        
        for (size_t r = 0; r < v.routes.size(); ++r) {
            const Route& route = v.routes[r];
            if (route.stops.size() <= 1) continue;
            
            cout << "        {" << endl;
            cout << "          \"trip_id\": " << (r + 1) << "," << endl;
            cout << "          \"cost\": " << route.total_cost << "," << endl;
            cout << "          \"stops\": [" << endl;
            
            for (size_t s = 0; s < route.stops.size(); ++s) {
                const auto& stop = route.stops[s];
                cout << "            {\"emp_id\": \"" << stop.emp_id << "\", \"arrival\": " << stop.arrival_time << "}";
                if (s < route.stops.size() - 1) cout << ",";
                cout << endl;
            }
            cout << "          ]" << endl;
            cout << "        }";
            if (r < v.routes.size() - 1) cout << ",";
            cout << endl;
        }
        cout << "      ]" << endl;
        cout << "    }";
        if (i < vehs.size() - 1) cout << ",";
        cout << endl;
    }
    cout << "  ]" << endl;
    cout << "}" << endl;
}