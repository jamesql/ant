/// @file senses.h
/// @brief What an ant perceives, written as the list of numbers a network reads.
///
/// A neural network only understands numbers, so this turns one ant's
/// situation into 22 of them (its **observation**). They are the same senses
/// a real ant has, and the same ones the hand-written instinct uses: touch,
/// smell, a little sight, and a sense of where home is. Nothing here tells
/// the ant where the food is; it has to work that out.
///
/// Every value is scaled to roughly -1..1 or 0..1, because networks learn
/// far more easily when all inputs are of similar size.

#pragma once

#include <cstddef>
#include <vector>

#include "core/world.h"

namespace ant {

/// How many numbers an observation contains.
inline constexpr int kSenseCount = 22;

/// The name written in brain files for this list of inputs.
inline constexpr const char* kSensesName = "standard-22";

/// The observation of ant `antIndex`, in this order:
///
/// | #     | Sense                  | Value                                              |
/// |-------|------------------------|----------------------------------------------------|
/// | 0-4   | feelers (5 rays)       | how close something solid is: 1 touching, 0 clear  |
/// | 5-7   | poison left/ahead/right| 1 if poison is just there, else 0                  |
/// | 8-10  | food scent L/A/R       | pheromone strength, 0..1                           |
/// | 11-13 | home scent L/A/R       | pheromone strength, 0..1                           |
/// | 14-15 | food in sight          | sin and cos of the angle to the nearest visible food |
/// | 16    | food in sight          | closeness: 1 on top of it, 0 at the edge of sight  |
/// | 17-18 | nest                   | sin and cos of the angle to the nest               |
/// | 19    | nest                   | closeness: 1 at the nest, 0 across the map         |
/// | 20    | carrying               | 1 if holding food, else 0                          |
/// | 21    | health                 | 1 healthy, 0 about to die                          |
///
/// Angles are measured from the way the ant is facing, so "sin > 0" always
/// means "to my right". Sin and cos are used instead of the angle itself
/// because they change smoothly all the way round, with no sudden jump when
/// the target passes behind the ant.
std::vector<float> observe(const World& world, std::size_t antIndex);

/// A short label for sense number `index` (for drawing the network).
const char* senseName(int index);

}  // namespace ant
