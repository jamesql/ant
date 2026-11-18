# Ant Picnic

A small first-person ant-colony simulation in C++. You are one ant in a colony
of ten. A picnic has been left on the grass: food is scattered over the
blanket and packed inside the basket. Bring all of it home to the anthill
before the clock runs out.

The other ants are not scripted along fixed paths. Like real ants, each one
follows a few simple rules and communicates only by leaving scent trails
(pheromones) on the ground. You can see those trails, add to them, and lead
the colony to food.

The project also contains a neural network written from scratch, with no
machine-learning libraries. It learns to drive the ants, and you can watch it
think. A network
trained here (included in `brains/`) carries home as much food as the
hand-written rules.

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

To watch a trained neural network drive the whole colony, with a live view of
it "thinking" (key `N` hides the view, `B` gives you back control of your ant):

```bash
build/Release/ant_game.exe --brain brains/forager.brain --colony-brain brains/forager.brain
```

| Option | Effect |
| --- | --- |
| `--autopilot` | Your ant follows the built-in instinct. |
| `--brain <file>` | A network loaded from a brain file drives your ant. |
| `--brain random` | A random, untrained network drives your ant. |
| `--colony-brain <file>` | A network drives the other nine ants in place of their instinct. |
| `--record <file>` | Record your own play, for a network to learn from. |
| `--save-brain <file>` | Write the current network to a brain file. |
| `--show-network` | Start with the network view open. |

## Training a brain

`ant_train` trains and scores brains with no window. Run it with no arguments
for the full list of options.

```bash
build/Release/ant_train.exe xor
```

```bash
build/Release/ant_train.exe imitate --seeds 1-20 --out brains/copycat.brain
```

```bash
build/Release/ant_train.exe evolve --start brains/copycat.brain --seconds 240 --seeds-per-generation 3 --first-seed 100 --out brains/forager.brain
```

```bash
build/Release/ant_train.exe score --brain brains/forager.brain --seeds 1000-1019
```

On 20 food layouts it never trained on, the included `forager` brain delivers
13.1 of 14 pieces per game; the hand-written instinct delivers 12.9. See
[docs/TRAINING.md](docs/TRAINING.md) for how it was made.

**If Windows refuses to run `ant_train.exe` or `ant_tests.exe`** ("An
Application Control policy has blocked this file"), Smart App Control is
blocking a freshly built, unsigned program. Build and run them in WSL
instead; from the project folder inside WSL:

```bash
cmake -S . -B ~/ant-build -DANT_BUILD_GAME=OFF -DCMAKE_BUILD_TYPE=Release && cmake --build ~/ant-build -j
```

```bash
~/ant-build/ant_tests && ~/ant-build/ant_train xor
```

Brain files are plain text, so the Windows game loads brains trained in WSL.

## Controls

| Key | Action |
| --- | --- |
| `W` / `Up` | Walk forward |
| `S` / `Down` | Walk backward |
| `A` / `Left` | Turn left |
| `D` / `Right` | Turn right |
| `P` | Show or hide the scent trails |
| `N` | Show or hide the live neural network view |
| `B` | Let the neural network drive your ant, or take control back |
| `G` | Swap in a new random network |
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
- [docs/TRAINING.md](docs/TRAINING.md) - how a network is trained to drive
  an ant, what training produces, and how to shape an ant's behaviour.
