/// @file action.h
/// @brief The control signal that drives the ant.
///
/// This is the single doorway between "whoever is deciding" and the simulation.
/// Today the keyboard fills it in; later a neural network will produce the
/// exact same two numbers. The simulation does not know or care which.

#pragma once

namespace ant {

/// What the ant should do during one simulation step.
/// Both values are clamped to [-1, 1] by the simulation.
struct AntAction {
    /// Walking effort: +1 full speed ahead, -1 full speed backwards, 0 stand still.
    float forward = 0.0f;
    /// Turning effort: +1 turn right at full rate, -1 turn left, 0 go straight.
    float turn = 0.0f;
};

}  // namespace ant
