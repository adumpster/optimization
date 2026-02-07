#include <iostream>
#include <vector>
#include <string>

#include "io.h"
#include "heuristic.h"
#include "report.h"
#include "output_json.h"
#include "file_utils.h"


using namespace std;

int main(int argc, char** argv) {
    vector<Employee> employees;
    vector<Vehicle> vehicles;

    cout << "\n=======================================================\n";
    cout << "    VELORA - SOLOMON I1 INSERTION HEURISTIC            \n";
    cout << "=======================================================\n";

    string input_file = "TC02.json";
    string out="output.json";
    bool debug = false;

    if (argc > 1) input_file = argv[1];
    if (argc > 2) out = argv[2];
    if (argc > 3 && string(argv[3]) == "--debug") debug = true;

    if (!load_from_json(input_file, employees, vehicles)) {
        cerr << "Failed to load. Exiting." << endl;
        return 1;
    }

    solve_solomon_insertion(employees, vehicles, debug);
    display_report(vehicles, employees);

    std::string input_raw = read_file_to_string(input_file);


    if (!write_output_json(out,input_raw, vehicles, employees)) {
        std::cout << "ERROR: Failed to write output file: " << out << "\n";
        return 1;
    }
   std::cout << "Wrote output to: " << out << "\n";
    
    return 0;
}
