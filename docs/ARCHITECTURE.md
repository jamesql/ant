# Game Architecture

This document explains how Ant Picnic is put together: what each part does,
how the parts talk to each other, and why it was built this way.

## 1. The big idea

The game is split into two halves that never mix:

- **The simulation** ("core") knows the rules of the game. It has no idea a
  screen or a keyboard exists.
- **The presentation** ("render" and "app") shows the simulation on screen and
  feeds it keyboard input. It contains no game rules.

```
   keyboard                                           screen
      |                                                  ^
      v                                                  |
+-----------+   AntAction   +-----------+   reads   +----------+
|    app    | ------------> |   core    | <-------- |  render  |
| main.cpp  |  (2 numbers)  |  World    |           | Renderer |
+-----------+               +-----------+           +----------+
                                  ^
                                  |  AntAction (the same 2 numbers)
                         +------------------+
                         |  neural network  |   <- comes later
                         +------------------+
```

The only thing that ever goes *into* the simulation is an `AntAction`: two
numbers saying "how hard to walk" and "how hard to turn". Right now the
keyboard produces those numbers. Later a neural network will produce them
instead, and nothing in the simulation has to change.

Because the simulation needs no window, it can also run far faster than real
time, which is what lets a network practise thousands of games in seconds.

## 2. Folder layout

```
CMakeLists.txt        build script (3 targets, see section 8)
src/
  core/               the simulation - pure C++, no graphics
    vec2.h              2D vector math
    action.h            AntAction: the control signal
    level.h / .cpp      Level: the map description + the picnic level builder
    world.h / .cpp      World: game state + rules
  render/             drawing - uses raylib
    sprites.h / .cpp    paints the sprite textures in code
    renderer.h / .cpp   first-person 3D view + HUD
  app/
    main.cpp            window, keyboard input, the game loop
tests/                unit tests for core/
docs/                 this document and the neural network guide
```

Dependency rule: `core` depends on nothing. `render` depends on `core` and
raylib. `app` depends on both. Nothing ever points back toward `app`.

## 3. The simulation (`src/core`)

### 3.1 Coordinates

The ant walks on a flat table, so the simulation is 2D.

- A position is a `Vec2 {x, y}`. The table's centre is `(0, 0)`.
- The table spans `-20..+20` in `x` and `-12..+12` in `y` (`tableHalfSize`).
- A **heading** is an angle in radians. Heading `0` faces `+x`. A positive
  turn increases the heading, which is a turn to the ant's right.
- `fromAngle(heading)` converts a heading into a direction:
  `(cos(heading), sin(heading))`.

### 3.2 `Level` - the map

A `Level` is plain data with no behaviour:

| Field | Meaning |
| --- | --- |
| `tableHalfSize` | size of the walkable table |
| `nest` | circle where food is delivered |
| `antStart`, `antStartHeading` | where the ant begins |
| `obstacles` | solid circles (plates, cups, bottles) |
| `foods` | the food pieces, each with a position and a state |
| `timeLimit` | seconds allowed |

`makePicnicLevel(seed)` builds the level. Obstacles are always in the same
place; food is scattered randomly. The same seed always gives the same
scatter, so a game can be replayed exactly.

### 3.3 `World` - the rules

A `World` holds a `Level`, the `Ant`, the clock and the score. It changes only
when `step(action, dt)` is called, which advances the game by `dt` seconds:

1. **Turn** - `heading += action.turn * turnSpeed * dt`
2. **Walk** - `position += direction * action.forward * speed * dt`
   (`speed` is 25% lower while carrying food)
3. **Collide** - if the ant overlaps an obstacle, push it straight out; then
   clamp it inside the table edges
4. **Pick up** - if the jaws are empty and the ant touches food, carry it
5. **Deliver** - if carrying food inside the nest circle, score it; if that
   was the last piece, the game is **won**
6. **Clock** - subtract `dt`; if time hits zero, the game is **lost**

After a win or a loss `step` does nothing.

Each piece of food moves through three states:

```
OnTable --(ant touches it)--> Carried --(ant enters nest)--> Delivered
```

### 3.4 Collision, in one picture

Everything solid is a circle, which keeps the math tiny. Two circles overlap
when the distance between their centres is less than the sum of their radii.
To fix an overlap, move the ant along the line between the centres until the
distance equals that sum:

```
 away        = (antPos - obstaclePos) / distance      (unit direction)
 new antPos  = obstaclePos + away * (obstacleRadius + antRadius)
```

## 4. The presentation (`src/render`, `src/app`)

### 4.1 The game loop (`main.cpp`)

Every frame does three things, always in this order:

```
1. Input   keyboard  ->  AntAction
2. Update  world.step(action, 1/60)   repeated as needed
3. Draw    renderer.draw(world)
```

**Fixed time step.** Frames do not take a constant amount of time, but the
simulation always advances in steps of exactly 1/60 of a second. The loop
keeps a running total of real time that has not been simulated yet, and runs
as many 1/60 steps as fit. The result: the game plays the same on a fast and
a slow computer, and the same sequence of actions always gives the same game.

### 4.2 The renderer

The renderer turns the 2D simulation into a 3D picture:

- A table point `(x, y)` is drawn at 3D position `(x, height, y)`. The table
  top is `height = 0`; raylib's Y axis points up.
- The **camera** sits at the ant's position, 0.3 units above the table, and
  looks along the ant's heading. That is all "first person" means.
- The **tablecloth** is one flat rectangle with a checked texture.
- **Obstacles** are plain cylinders.
- **Food and the anthill are sprites**: flat pictures that always turn to face
  the camera (called *billboards*). The pictures themselves are painted in
  code at start-up (`sprites.cpp`), so the game ships with no image files.
- Sprites are drawn farthest-first, so near ones correctly cover far ones.
- The **HUD** (score, clock, carried food, mini-map) is drawn last, in 2D, on
  top of everything.

The renderer takes a `const World&`: it can look but cannot change anything.

## 5. Why it is built this way

| Decision | Reason |
| --- | --- |
| Simulation separate from graphics | It can be unit-tested, and run headless at high speed for training. |
| One input type (`AntAction`) | A human and a neural network plug into the same socket. |
| Fixed time step | Deterministic, repeatable games; fair comparisons between networks. |
| Seeded level generation | The same seed replays the same game; different seeds give variety so a network cannot just memorise one layout. |
| Circles for all collision | Simplest possible collision math, and cheap to compute. |
| Sprites painted in code | No asset pipeline, nothing to load from disk. |

## 6. One frame, end to end

```
main.cpp        readPlayerAction()         W held -> AntAction{forward 1, turn 0}
main.cpp        world.step(action, 1/60)
  world.cpp       turn, walk               ant moves 5 * 1/60 units forward
  world.cpp       resolveCollisions()      pushed out of a plate if overlapping
  world.cpp       pickUpFood()             touching a crumb -> state = Carried
  world.cpp       deliverFood()            not in the nest -> nothing happens
  world.cpp       clock                    timeRemaining -= 1/60
main.cpp        renderer.draw(world)
  renderer.cpp    drawScene()              camera at ant, table, cylinders, sprites
  renderer.cpp    drawHud()                score, clock, crumb icon, mini-map
```

## 7. Tests (`tests/`)

Only `core` is tested, because that is where the rules are. The tests use a
tiny home-made framework (`tests/test_framework.h`, three macros: `TEST`,
`CHECK`, `CHECK_NEAR`), so there is no test library to install.

| File | What it proves |
| --- | --- |
| `test_vec2.cpp` | the vector math is right |
| `test_level.cpp` | levels are repeatable per seed, and food is always reachable |
| `test_world.cpp` | each game rule, one at a time, on tiny hand-made levels |

Run them with `ctest --test-dir build -C Release --output-on-failure`.

## 8. Build targets

| Target | Contents | Depends on |
| --- | --- | --- |
| `ant_core` | static library: `src/core` | nothing |
| `ant_tests` | the unit tests | `ant_core` |
| `ant_game` | the playable game | `ant_core`, raylib |

The neural network will become a fourth library, `ant_nn`, that also depends
on nothing. See [NEURAL_NETWORK.md](NEURAL_NETWORK.md).
