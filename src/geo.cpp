#include "geo.h"
#include "config.h"
#include <cmath>

double get_dist(Location a, Location b) {
    double R = 6371.0;
    double dLat = (b.lat - a.lat) * PI / 180.0;
    double dLon = (b.lng - a.lng) * PI / 180.0;
    double lat1 = a.lat * PI / 180.0;
    double lat2 = b.lat * PI / 180.0;
    double a_val = pow(sin(dLat / 2), 2) + pow(sin(dLon / 2), 2) * cos(lat1) * cos(lat2);
    double c = 2 * asin(sqrt(a_val));
    return R * c;
}

int travel_minutes(double dist_km, double speed_kmh) {
    if (speed_kmh <= 0.0) return (int)1e9;
    return (int)llround((dist_km / speed_kmh) * 60.0);
}

double recompute_distance_km(const std::vector<Stop>& stops) {
    double dist = 0.0;
    for (size_t i = 1; i < stops.size(); i++) {
        dist += get_dist(stops[i-1].loc, stops[i].loc);
    }
    return dist;
}
