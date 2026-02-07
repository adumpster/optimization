#pragma once
#include <string>
#include <vector>
#include "types.h"

VehicleCat parse_vehicle_category(std::string cat);
SharingPref parse_sharing_pref(std::string pref);

bool load_from_json(const std::string& filename,
                    std::vector<Employee>& emps,
                    std::vector<Vehicle>& vehs);
