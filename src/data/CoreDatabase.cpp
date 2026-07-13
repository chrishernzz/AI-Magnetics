#include "CoreDatabase.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

//precondition: call CoreData that shows what the file should have
//postcondition: returns a vector of the data with the right data information
std::vector<CoreData>CoreDatabase::load() {
    std::vector<CoreData> cores;

    std::filesystem::path path =std::filesystem::current_path() / "data" / "cores.csv";

    //reading the file
    std::ifstream file(path);
    //if no file, then return 
    if (!file) {
        std::cout << "FAILED TO OPEN cores.csv" << std::endl;
        return cores;
    }

    std::string line;
    //skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);

        CoreData core;
        std::string mu;
        std::string al;
        std::string ae;
        std::string wa;
        std::string le;

        //throw away strings for new columns we don't store yet and do not need yet
        std::string partCost;
        std::string vendor;
        std::string maxCurrent;
        std::string maxFreq;

        std::getline(ss, core.partNumber, ',');
        std::getline(ss, core.material, ',');
        std::getline(ss, mu, ',');
        std::getline(ss, al, ',');
        std::getline(ss, ae, ',');
        std::getline(ss, wa, ',');
        std::getline(ss, le, ',');

        //read extra columns but they are discarded (not needed)
        std::getline(ss, partCost, ',');
        std::getline(ss, vendor, ',');
        std::getline(ss, maxCurrent, ',');
        std::getline(ss, maxFreq);

        core.mu = std::stod(mu);
        core.al = std::stod(al);
        core.ae = std::stod(ae);
        core.wa = std::stod(wa);
        core.le = std::stod(le);

        //add to the vector
        cores.push_back(core);
    }

    return cores;
}