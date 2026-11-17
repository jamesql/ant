# Ant Picnic

A small first-person ant-colony simulation in C++. You are one ant in a colony
of ten. A picnic has been left on the grass: food is scattered over the
blanket and packed inside the basket. Bring all of it home to the anthill
before the clock runs out.

The other ants are not scripted along fixed paths. Like real ants, each one
follows a few simple rules and communicates only by leaving scent trails
(pheromones) on the ground. You can see those trails, add to them, and lead
the colony to food.

The game is also the training ground for a neural network we will write from
scratch, which will learn to drive an ant.

## Build and run

Requirements: CMake 3.24+ and a C++20 compiler (Visual Studio Build Tools on
Windows). raylib 5.5 is downloaded automatically the first time you configure.

```bash
cmake -S . -B build
```

```bash
cmake --build build --config Release
```

```bash
ctest --test-dir build -C Release --output-on-failure
```

```bash
build/Release/ant_game.exe
```

To sit back and watch the colony work on its own (your ant follows its
instinct too):

```bash
build/Release/ant_game.exe --autopilot
```

## Controls

| Key | Action |
| --- | --- |
| `W` / `Up` | Walk forward |
| `S` / `Down` | Walk backward |
| `A` / `Left` | Turn left |
| `D` / `Right` | Turn right |
| `P` | Show or hide the scent trails |
| `R` | Restart with a new food layout |
| `Esc` | Quit |

## How to play

- Walk into food to pick it up; walk onto the anthill's dirt patch to deliver
  it. An ant carries one piece at a time, and carrying slows it down.
- **Scent trails**: blue marks lead home, orange marks lead to food. Every ant
  lays them automatically, including you. Carry food home from somewhere new
  and the colony will follow your orange trail back to it.
- **The basket** holds most of the food. Its only way in is a gap at one
  corner.
- **Poison puddles** (green) drain your health. Leave quickly and you recover;
  stay and your ant dies, and the colony hatches a replacement at the nest a
  few seconds later.
- Left alone, the colony finishes close to the time limit and sometimes falls
  short. Your help decides it.

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - how the game and simulation
  are put together.
- [docs/NEURAL_NETWORK.md](docs/NEURAL_NETWORK.md) - what a neural network is
  and all the math behind it.
- [docs/TRAINING.md](docs/TRAINING.md) - how we will train a network to drive
  an ant, what training produces, and how to shape an ant's behaviour.
