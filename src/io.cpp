#include "io.h"
#include "mini_json.h"
#include "time_utils.h"
#include "config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>

using namespace std;
using Json = mini_json::Value;

VehicleCat parse_vehicle_category(string cat) {
    if (cat == "premium") return PREMIUM;
    if (cat == "normal") return NORMAL;
    return ANY_CAT;
}

SharingPref parse_sharing_pref(string pref) {
    if (pref == "single") return SINGLE;
    if (pref == "double") return DOUBLE;
    if (pref == "triple") return TRIPLE;
    return ANY_SHARE;
}

bool load_from_json(const string& filename, vector<Employee>& emps, vector<Vehicle>& vehs) {
    try {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "ERROR: Cannot open file: " << filename << endl;
            return false;
        }

        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        Json data = mini_json::parse(text);

        // Redirected to cerr to keep stdout clean for JSON
        cerr << "Loading data from: " << filename << endl;

        map<string, double> baseline_map;
        if (data.is_object() && data.obj.find("baseline") != data.obj.end()) {
            const Json& base = data["baseline"];
            if (base.is_array()) {
                for (const auto& b : base.arr) {
                    if (!b.is_object()) continue;
                    std::string eid = b["employee_id"].as_string();
                    double bc = b["baseline_cost"].as_number(0.0);
                    if (!eid.empty()) baseline_map[eid] = bc;
                }
            }
        }

        if (data.is_object() && data.obj.find("employees") != data.obj.end()) {
            const Json& em = data["employees"];
            for (const auto& kv : em.obj) {
                string emp_id = kv.first;
                const Json& emp_data = kv.second;
                
                Employee e;
                e.id = emp_id;
                e.priority = emp_data["priority"].as_int(999);
                e.pickup = {emp_data["pickup"]["lat"].as_number(), emp_data["pickup"]["lng"].as_number()};
                e.drop   = {emp_data["drop"]["lat"].as_number(),  emp_data["drop"]["lng"].as_number()};
                
                if (OFFICE.lat == 0.0 && OFFICE.lng == 0.0) OFFICE = e.drop;
                
                if (emp_data["earliest_pickup"].is_number()) {
                    e.ready_time = (int)llround(emp_data["earliest_pickup"].as_number() * 1440.0);
                } else {
                    e.ready_time = parse_time(emp_data["earliest_pickup"].as_string("08:00"));
                }
                
                if (emp_data["latest_drop"].is_number()) {
                    e.due_time = (int)llround(emp_data["latest_drop"].as_number() * 1440.0);
                } else {
                    e.due_time = parse_time(emp_data["latest_drop"].as_string("23:59"));
                }
                
                e.veh_pref   = parse_vehicle_category(emp_data["vehicle_preference"].as_string("any"));
                e.share_pref = parse_sharing_pref(emp_data["sharing_preference"].as_string("any"));
                e.is_routed = false;
                e.baseline_cost = baseline_map.count(emp_id) ? baseline_map[emp_id] : 0;
                
                emps.push_back(e);
            }
        }

        cerr << "  Loaded " << emps.size() << " employees" << endl;

        if (data.is_object() && data.obj.find("vehicles") != data.obj.end()) {
            const Json& vv = data["vehicles"];
            for (const auto& veh_data : vv.arr) {
                Vehicle v;
                v.id = veh_data["vehicle_id"].as_string();
                v.capacity = veh_data["capacity"].as_number(0.0);
                v.cost_per_km = veh_data["cost_per_km"].as_number(0.0);
                v.speed_kmh = veh_data["avg_speed_kmph"].as_number(30.0);
                v.depot_loc = {veh_data["current_lat"].as_number(), veh_data["current_lng"].as_number()};
                v.current_loc = v.depot_loc;
                
                if (veh_data["available_from"].is_number()) {
                    v.available_time = (int)llround(veh_data["available_from"].as_number() * 1440.0);
                } else {
                    v.available_time = parse_time(veh_data["available_from"].as_string("08:00"));
                }

                v.category = parse_vehicle_category(veh_data["category"].as_string("any"));
                v.total_cost = 0;
                vehs.push_back(v);
            }
        }

        cerr << "  Loaded " << vehs.size() << " vehicles" << endl;
        return true;

    } catch (const exception& e) {
        cerr << "ERROR parsing JSON: " << e.what() << endl;
        return false;
    }
}