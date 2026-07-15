#include "MaterialSelection.h"
#include "../data/Materials.h"
#include<iostream>

//precondition: call MaterialSelectionInput but only 'switchingFreqHz' to compare
//postcondition: returns the recommened material selection 'name', 'muOpt', 'reason', and 'alternatives' (other material)
MaterialSelectionResult selectMaterial(const MaterialSelectionInput& in) {
    /*go to the database
    const auto& instead of auto — Materials::load() now returns a
    reference (see Materials.h). Plain `auto` would deduce a value type
    and copy the whole vector right back out, undoing that. `const auto&`
    binds to the reference instead — no copy.*/
    const auto& materials = Materials::load();

    //range based loops through the database results
    for (const auto& material : materials) {
        //see what is between the min and max 
        if(in.switchingFreqHz >= material.minFrequencyHz && in.switchingFreqHz < material.maxFrequencyHz) {
            return {material.name, material.muOpt, material.reason, material.alternatives};
        }
    }
    
    //if none find then error
    return {"Unknown", 0.0, "No matching material found.", "None"};
}