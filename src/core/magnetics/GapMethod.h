#pragma once

//Only MachinedCenterLeg has a validated formula in Phase 1 (the McLyman
//CGS AL model in GapDesign.h). Spacer/Distributed/ManufacturerGapped are
//real gapping techniques used in the field, but no formula for any of
//them exists here - they are schema placeholders so the ruleset can
//express them later, not a claim that Phase 1 supports them.
//designTurnsAndGap() explicitly rejects any request whose DesignRules
//specify a method other than MachinedCenterLeg, rather than silently
//applying the one validated formula to a method it was never checked
//against.
enum class GapMethod { MachinedCenterLeg, Spacer, Distributed, ManufacturerGapped };
