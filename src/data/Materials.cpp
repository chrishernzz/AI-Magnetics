#include "Materials.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

//precondition: call MaterialData that shows what the file should have
//postcondition: returns a vector of the data with the right data information
std::vector<MaterialData>Materials::load() {
    //emtpy array of the data that we are going to return to service
    std::vector<MaterialData> materials;

    //this will take you to the .csv file that shows the data
    std::filesystem::path path = std::filesystem::current_path() / "data" / "materials.csv";

    //DEBUG cout
    std::cout << "Loading materials from: " << path.string() << std::endl;

    //read from the file path
    std::ifstream file(path);
    //if no file then return 
    if (!file) {
        std::cout << "FAILED TO OPEN MATERIALS CSV" << std::endl;
        return materials;
    }
    //this read one line at time
    std::string line;
    //skips the header file
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);

        MaterialData material;
        std::string muOpt;
        std::string minFreq;
        std::string maxFreq;
        //throw away strings for new columns we don't store yet and do not need yet
        std::string bmaxT;
        std::string cuLossFactor;

        std::getline(ss, material.name, ',');
        std::getline(ss, muOpt, ',');
        std::getline(ss, minFreq, ',');
        std::getline(ss, maxFreq, ',');
        std::getline(ss, material.reason, ',');
        std::getline(ss, material.alternatives, ',');

        //read extra columns but they are discarded (not needed)
        std::getline(ss, bmaxT, ',');
        std::getline(ss, cuLossFactor);

        material.muOpt = std::stod(muOpt);
        material.minFrequencyHz = std::stod(minFreq);
        material.maxFrequencyHz = std::stod(maxFreq);
        //add to the vector
        materials.push_back(material);
    }
    //DEBUG to check the size of the vector
    //std::cout << "Loaded materials: " << materials.size() << std::endl;

    return materials;
}