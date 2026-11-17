/// @file instinct.h
/// @brief The built-in behaviour of a worker ant: forage by following scent.
///
/// This is a hand-written "brain": a handful of simple rules that turn what
/// an ant can sense into an `AntAction`. No single ant is clever, yet
/// together the colony finds food and carries it home, because each ant
/// reads and writes the shared scent trails. That is how real ants work.
///
/// A neural network will later do this same job (senses in, action out);
/// the instinct is both the colony's default behaviour and a useful teacher.

#pragma once

#include <cstddef>

#include "core/action.h"

namespace ant {

class World;

/// Decides what ant `antIndex` should do right now.
///
/// An ant is either **searching** (jaws empty) or **homing** (carrying food,
/// or giving up after a long fruitless search). The rules, in priority order:
///
///   1. **Goal in sight**: walk straight to food (if the jaws are empty) or
///      to the nest (if carrying) when it is close and nothing is in the way.
///   2. **Avoid trouble**: a searching ant steers away from obstacles, walls
///      and poison ahead. A homing ant only avoids poison; it pushes on past
///      solid things, which slides it around them.
///   3. **Use scent**: a searching ant turns toward the strongest food scent
///      ahead. A homing ant heads for the nest by its sense of direction
///      (real ants keep track of where home is) and lets home scent bend
///      its course.
///   4. **No lead**: a searching ant with nothing to follow wanders.
AntAction instinctAction(const World& world, std::size_t antIndex);

}  // namespace ant
