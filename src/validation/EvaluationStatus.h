#pragma once

//precondition: shared status vocabulary for any result that may be blocked on missing data 
//postcondition: (spec section 10: "If required data is missing, return not_evaluated ... never assume missing data equals a pass").
enum class EvaluationStatus {
    Evaluated,
    NotEvaluated,
    Rejected
};
