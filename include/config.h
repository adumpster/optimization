#pragma once
#include <map>
#include <string>
#include "types.h"

extern double ALPHA1;
extern double ALPHA2;
extern double LAMBDA;
extern double MU;

extern const double INF;
extern const double PI;

// Common corporate office drop location (set from dataset)
extern Location OFFICE;

// Filled during solve to explain WHY someone could not be routed.
extern std::map<std::string, std::string> g_unrouted_reason;
