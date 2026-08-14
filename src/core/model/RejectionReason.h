#pragma once
#include <string>

/*
A single failed check or blocking condition attached to a candidate that
did not pass (spec sections 8/10: "candidate rejection must include
every failed check, not only the first failure").
*/
struct RejectionReason {
    std::string checkName;
    std::string explanation;
};
