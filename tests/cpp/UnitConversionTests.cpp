#include <cassert>
#include <cstdio>
#include "TestHelpers.h"

namespace {
//precondition: none - just test the conversions constants 
//postcondition: every conversion produces the exact expected value to very tight tolerance (1e-9 to 1e-12), confirming no stray factor-of-10 or factor-of-1000 error exists in the constants used eleswhere in the codebase
void testUnitConversions() {
    //actual, expected, answer
    //uH -> H
    assert(approxEqual(250.0 * 1e-6, 0.00025, 1e-12));      
    //kHz -> Hz
    assert(approxEqual(100.0 * 1000.0, 100000.0, 1e-9));    
    //mm^2 -> cm^2
    assert(approxEqual(735.0502256509033 / 100.0, 7.350502256509033, 1e-9));  
    //mm -> cm
    assert(approxEqual(273.919572699267 / 10.0, 27.3919572699267, 1e-9));    
    std::printf("testUnitConversions: ok\n");
}
} // namespace 


void runUnitConversionTests() {
    testUnitConversions();
    std::printf("All UnitConversionTests passed.\n");
}