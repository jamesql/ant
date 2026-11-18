/// @file dataset.h
/// @brief Recorded examples of a teacher driving an ant, for imitation learning.
///
/// An **example** is one moment: what the ant sensed, and what the teacher
/// did about it. A **dataset** is a pile of examples. Imitation learning
/// trains a network to give the teacher's answer for each one.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ant {

/// One recorded moment.
struct Example {
    std::vector<float> observation;  ///< What the ant sensed (see agent/senses.h).
    std::vector<float> action;       ///< What the teacher did: walking effort, turning effort.
};

using Dataset = std::vector<Example>;

/// How to record the built-in instinct as a teacher.
struct RecordSettings {
    float seconds = 240.0f;  ///< Game time recorded per seed.
    /// Only every Nth step is looked at. Neighbouring steps are almost
    /// identical, so skipping some loses little and saves a lot.
    int stepInterval = 3;
    /// Most moments are "walk straight ahead". Kept in full they would
    /// swamp the rare, important moments (turning at a wall, turning onto a
    /// trail), and the network would learn to just walk straight. So only
    /// this share of the straight-ahead moments is kept; all others are.
    float keepStraightShare = 0.15f;
};

/// True if an action is plain "walk straight ahead at full speed".
bool isStraightAhead(const std::vector<float>& action);

/// Plays the picnic level once per seed from `firstSeed` to `lastSeed`, with
/// every ant following its instinct, and records what each ant sensed and did.
Dataset recordInstinct(std::uint32_t firstSeed, std::uint32_t lastSeed, const RecordSettings& settings = {});

/// Writes a dataset to a plain-text file: a header line, then one example
/// per line (the observation's numbers followed by the action's).
/// Returns false if the file could not be written.
bool saveDataset(const Dataset& dataset, const std::string& path);

/// Reads a dataset written by `saveDataset`. Returns nothing if the file is
/// missing or is not a valid dataset.
std::optional<Dataset> loadDataset(const std::string& path);

}  // namespace ant
