#pragma once
#include "types.h"
#include <vector>

double get_dist(Location a, Location b);
int travel_minutes(double dist_km, double speed_kmh);
double recompute_distance_km(const std::vector<Stop>& stops);
