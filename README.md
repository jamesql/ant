# Ant Picnic

A small first-person game in C++: you are an ant on a picnic table. Find the
food, carry it back to the anthill one piece at a time, and deliver all of it
before the clock runs out.

The game is also the training ground for a neural network we will write from
scratch, which will learn to play it.

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

## Controls

| Key | Action |
| --- | --- |
| `W` / `Up` | Walk forward |
| `S` / `Down` | Walk backward |
| `A` / `Left` | Turn left |
| `D` / `Right` | Turn right |
| `R` | Restart with a new food layout |
| `Esc` | Quit |

Food is picked up by walking into it and delivered by walking onto the dirt
patch around the anthill. You can carry one piece at a time, and carrying
slows you down.

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - how the game is put together.
- [docs/NEURAL_NETWORK.md](docs/NEURAL_NETWORK.md) - what a neural network is,
  all the math behind it, and the plan for building one that plays this game.
