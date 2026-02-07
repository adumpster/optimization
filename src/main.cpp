#include <iostream>
#include <vector>
#include <string>

#include "io.h"
#include "heuristic.h"
#include "report.h"

using namespace std;

int main(int argc, char* argv[]) {
    vector<Employee> employees;
    vector<Vehicle> vehicles;

    cout << "\n=======================================================\n";
    cout << "    VELORA - SOLOMON I1 INSERTION HEURISTIC            \n";
    cout << "=======================================================\n";

    string input_file = "TC02.json";
    bool debug = false;

    if (argc > 1) input_file = argv[1];
    if (argc > 2 && string(argv[2]) == "--debug") debug = true;

    if (!load_from_json(input_file, employees, vehicles)) {
        cerr << "Failed to load. Exiting." << endl;
        return 1;
    }

    solve_solomon_insertion(employees, vehicles, debug);
    display_report(vehicles, employees);

    return 0;
}
