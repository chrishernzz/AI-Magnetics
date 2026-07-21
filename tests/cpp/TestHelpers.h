#pragma once
#include<cmath>

//precondition: a and b are finite doubles; tolerance is a non-negative finite double >= 0
/*postcondition: returns true if a and b are within tolerance of each other, false otherwise. abs converts to a non-negative difference, so the order of a and b doesn't matter
does not throw or modify inputs. Used as the comparison predicate for every assert() below, since exact floating point equality is unreliable*/
inline bool approxEqual(double a, double b, double tolerance) {
    return std::abs(a - b) <= tolerance;
}