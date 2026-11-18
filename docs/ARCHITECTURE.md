# Game Architecture

This document explains how Ant Picnic is put together: what each part does,
how the parts talk to each other, and why it was built this way.

## 1. The big idea

The program is split into two halves that never mix:

- **The simulation** ("core") knows the rules of the world: how ants move,
  what scent does, what poison does, when the game is won. It has no idea a
  screen or a keyboard exists.
- **The presentation** ("render" and "app") shows the simulation on screen and
  feeds it keyboard input. It contains no rules.

```
   keyboard                                             screen
      |                                                    ^
      v                                                    |
+-----------+  AntAction    +-------------+    reads   +----------+
|    app    | ------------> |    core     | <--------- |  render  |
| main.cpp  | (player ant)  |   World     |            | Renderer |
+-----------+               |             |            +----------+
                            |   ^     |   |
              AntAction     |   |     v   |
            (every other    | instinct    |   <- today: hand-written rules
                 ant)       +-------------+   <- later: a neural network
```

The only thing that ever goes *into* the simulation is an `AntAction` per
ant: two numbers saying "how hard to walk" and "how hard to turn". The
keyboard produces them for your ant. A set of hand-written rules called the
**instinct** produces them for every other ant. Later a neural network will
produce them instead, and nothing in the simulation has to change.

Because the simulation needs no window, it can also run far faster than real
time, which is what lets a network practise thousands of games in seconds.

## 2. Folder layout

```
CMakeLists.txt        build script (7 targets, see section 9)
src/
  core/               the simulation - pure C++, no graphics
    vec2.h              2D vector math
    action.h            AntAction: the control signal for one ant
    level.h / .cpp      Level: the map description + the picnic level builder
    pheromones.h / .cpp PheromoneField: the grid of scent trails
    world.h / .cpp      World: all game state + the rules
    instinct.h / .cpp   the built-in foraging behaviour of a worker ant
  render/             drawing - uses raylib
    palette.h           colours shared by the 3D scene and the HUD
    sprites.h / .cpp    paints every texture in code
    lighting.h / .cpp   the GPU shaders (sunlight + haze, sprite cut-outs)
    renderer.h / .cpp   the first-person 3D scene
    hud.h / .cpp        the 2D overlay: panels, mini-map, messages
    network_view.h/.cpp the live picture of a neural network
  app/
    main.cpp            window, keyboard input, the game loop
  nn/                 the neural network - pure math, knows nothing about ants
    matrix.h / .cpp     Matrix: a grid of numbers, and matrix-times-vector
    network.h / .cpp    Network: layers, the forward pass, brain files
  agent/              connects a network to an ant
    senses.h / .cpp     what an ant perceives, as 22 numbers
    brain.h / .cpp      senses -> network -> AntAction
  train/              teaches a network: episodes, datasets, imitation, evolution
tests/                unit tests for core/, nn/, agent/ and train/
docs/                 this document and the neural network guides
```

Dependency rule: `core` and `nn` depend on nothing. `agent` depends on both,
and `train` on `agent`.
`render` depends on `core`, `nn` and raylib. `app` depends on all of them.
Nothing ever points back toward `app`.

## 3. The simulation (`src/core`)

### 3.1 Coordinates

The ants walk on flat ground, so the simulation is 2D.

- A position is a `Vec2 {x, y}`. The centre of the blanket is `(0, 0)`.
- The walkable area spans `-26..+26` in `x` and `-17..+17` in `y`
  (`areaHalfSize`). The blanket covers the middle `-20..+20` by `-12..+12`.
- A **heading** is an angle in radians. Heading `0` faces `+x`. A positive
  turn increases the heading, which is a turn to the ant's right.
- `fromAngle(heading)` converts a heading into a direction:
  `(cos(heading), sin(heading))`.

### 3.2 `Level` - the map

A `Level` is plain data with no behaviour:

| Field | Meaning |
| --- | --- |
| `areaHalfSize`, `blanketHalfSize` | size of the walkable ground and of the blanket |
| `nest` | circle where food is delivered |
| `antStart`, `antStartHeading` | where the player's ant begins |
| `antCount` | size of the colony, including the player's ant |
| `obstacles` | solid circles: plates, cups, glasses, bottles |
| `walls` | solid straight barriers: the sides of the basket |
| `baskets` | where each basket is (for drawing; its solid parts are in `walls`) |
| `poisons` | circular poison puddles |
| `foods` | the food pieces, each with a position, a kind and a state |
| `timeLimit` | seconds allowed |

`makePicnicLevel(seed)` builds the level. The layout is always the same; only
the food is scattered randomly (some in the open, most inside the basket). The
same seed always gives the same scatter, so a game can be replayed exactly.

### 3.3 `World` - the rules

A `World` holds the `Level`, every `Ant`, the scent grid, the clock and the
score. It changes only when `step(actions, dt)` is called, which advances the
game by `dt` seconds. For each living ant, in order:

1. **Turn** - `heading += action.turn * turnSpeed * dt`
2. **Walk** - `position += direction * action.forward * speed * dt`
   (`speed` is 25% lower while carrying food)
3. **Collide** - push the ant out of any obstacle or wall it overlaps, then
   clamp it inside the map
4. **Poison** - standing in a puddle drains health; away from poison it slowly
   recovers. At zero health the ant dies and drops what it carried
5. **Pick up** - if the jaws are empty and the ant touches food, carry it
6. **Deliver** - if carrying food inside the nest circle, score it; if that
   was the last piece, the game is **won**
7. **Lay scent** - mark the ground under the ant (see 3.5)

Then the scent grid spreads and fades, and the clock runs down; at zero the
game is **lost**. After a win or a loss `step` does nothing.

A dead ant is replaced by a fresh one at the nest after a few seconds.

Each piece of food moves through three states:

```
OnGround --(an ant touches it)--> Carried --(ant enters nest)--> Delivered
    ^                                |
    +-------(carrier dies)-----------+
```

There are two ways to call `step`:

- `step(actions, dt)` - you supply one action per ant. This is the form a
  trainer will use.
- `step(playerAction, dt)` - you supply the player's action and every other
  ant uses its instinct. This is the form the game uses.

### 3.4 Collision, in one picture

Everything solid is either a circle or a thick line, which keeps the math
tiny. Both reduce to the same question: "how far is the ant from the nearest
solid point?"

- For a circle (plate, cup...), the nearest solid point is its centre and the
  minimum allowed distance is `obstacleRadius + antRadius`.
- For a wall, the nearest solid point is the closest point on the wall's line
  segment and the minimum allowed distance is `wallThickness + antRadius`.

If the ant is too close, move it straight away from that point:

```
 away        = (antPos - solidPoint) / distance        (unit direction)
 new antPos  = solidPoint + away * minimumDistance
```

An ant that keeps walking into something solid therefore slides along it.

### 3.5 Pheromones - scent trails

Real ants are nearly blind and navigate by smell. The ground is divided into
a grid of half-unit cells, and each cell stores the strength (0 to 1) of two
scents:

| Scent | Laid by | Strongest near | Following it uphill leads to |
| --- | --- | --- | --- |
| **Home** (drawn blue) | ants that are searching | the nest | the nest |
| **Food** (drawn orange) | ants carrying food | where the food was | food |

Three things make this work:

- **Fading reserve.** Each ant has a "scent reserve" that refills to full when
  it visits the nest or picks up food, then fades. An ant lays scent in
  proportion to its reserve, so a trail is strongest at the end worth walking
  toward.
- **Evaporation.** Every cell loses a fixed fraction of its scent each moment
  (`strength *= exp(-dt / lifetime)`). Trails nobody renews disappear, so the
  colony forgets food sources that have run out.
- **Diffusion.** Every cell drifts toward the average of its four neighbours,
  so trails widen slightly and are easier to find.

Scent cannot enter or cross solid cells, so it does not leak through the
basket's walls. It reaches the outside only through the doorway.

### 3.6 The instinct - how a worker ant decides

`instinctAction(world, antIndex)` turns what one ant can sense into an
`AntAction`. An ant is either **searching** (jaws empty) or **homing**
(carrying food, or giving up after about 70 seconds of finding nothing).
The rules, highest priority first:

1. **Goal in sight** - walk straight to nearby food (if the jaws are empty)
   or to the nearby nest (if carrying), when nothing is in the way.
2. **Avoid trouble** - a searching ant steers away from anything solid or
   poisonous just ahead. A homing ant only avoids poison and otherwise pushes
   on, which slides it around obstacles.
3. **Use scent** - a searching ant turns toward the strongest food scent
   ahead. A homing ant heads for the nest by its sense of direction and lets
   home scent bend its course.
4. **No lead** - a searching ant with nothing to follow wanders.

No ant knows where the food is, and no ant is told what the others are doing.
The colony's behaviour (finding the basket, forming a trail through its
doorway, emptying it) comes entirely from these rules plus the shared scent.

The "senses" used here (three feelers ahead for obstacles and poison, three
scent samples ahead, food in sight, direction to the nest) are the same ones
a neural network will be given later.

## 4. The presentation (`src/render`, `src/app`)

### 4.1 The game loop (`main.cpp`)

Every frame does three things, always in this order:

```
1. Input   keyboard  ->  AntAction for the player's ant
2. Update  world.step(action, 1/60)   repeated as needed
3. Draw    renderer.draw(world)
```

**Fixed time step.** Frames do not take a constant amount of time, but the
simulation always advances in steps of exactly 1/60 of a second. The loop
keeps a running total of real time that has not been simulated yet, and runs
as many 1/60 steps as fit. The result: the game plays the same on a fast and
a slow computer, and the same sequence of actions always gives the same game.

### 4.2 The 3D scene (`renderer.cpp`)

The renderer turns the 2D simulation into a 3D picture:

- A ground point `(x, y)` is drawn at 3D position `(x, height, y)`. The ground
  is `height = 0`; raylib's Y axis points up.
- The **camera** sits at the player ant's position, 0.3 units up, looking
  along its heading. That is all "first person" means. It bobs slightly with
  each step.
- **Every solid shape is built from five simple meshes** - a square, a box, a
  cylinder, a cone and a ball - each stretched, turned and moved into place
  by a *transform matrix*. A bottle is three cylinders and a cone. An ant is
  three stretched balls (abdomen, thorax, head) plus thin rods for its six
  legs and two antennae; the legs swing in the alternating "tripod gait" real
  ants use.
- **Food, the anthill, trees and clouds are sprites**: flat pictures that
  always turn to face the camera (*billboards*).
- **Decals** are pictures painted flat on the ground: shadows, poison puddles
  and the scent trails. The scent decal is a texture with one pixel per
  pheromone cell, repainted every frame straight from the simulation.

One frame is drawn in this order, which matters because see-through things
must be drawn after whatever is behind them:

```
1. sky gradient                     (2D, behind everything)
2. grass, blanket                   (solid)
3. decals: poison, scent, shadows   (see-through, flat on the ground)
4. nest, obstacles, basket, ants    (solid)
5. sprites                          (cut-outs)
6. drinking-glass shells            (see-through)
7. HUD                              (2D, on top of everything)
```

### 4.3 Shaders (`lighting.cpp`)

A *shader* is a tiny program the graphics card runs for every pixel. There
are two:

- The **lighting shader** makes surfaces facing the sun brighter than those
  facing away (so a cylinder looks round), and fades distant surfaces toward
  the horizon colour (haze), which gives depth and hides the edge of the lawn.
- The **cut-out shader** discards the transparent pixels of sprites, so a
  sprite never hides what is behind its empty corners.

### 4.4 Textures (`sprites.cpp`)

Every picture is painted in code at start-up, so the game ships with no image
files: pixel-art food and anthill with dark outlines, the gingham blanket,
wicker for the basket, grass, trees, clouds, and the soft blobs used for
shadows and puddles.

### 4.5 The HUD (`hud.cpp`)

Drawn last, in 2D:

- **Status panel** - colony food delivered, how much you carried yourself,
  ants alive, your health.
- **Clock** - turns orange, then pulses red, as time runs short.
- **Mini-map** - the whole area from above, including the scent trails.
- **Your own body** - your antennae at the top of the view and your jaws at
  the bottom, holding whatever you carry.
- **Messages** and full-screen notices (poisoned, won, lost).

The renderer and HUD take a `const World&`: they can look but cannot change
anything.

## 5. Why it is built this way

| Decision | Reason |
| --- | --- |
| Simulation separate from graphics | It can be unit-tested, and run headless at high speed for training. |
| One input type (`AntAction`) per ant | A human, the instinct and a neural network all plug into the same socket. |
| Instinct lives in `core`, apart from `World` | `World` only enforces rules; *deciding* is a separate, replaceable job. |
| Fixed time step | Deterministic, repeatable games; fair comparisons between networks. |
| No random numbers during play | Wandering uses smooth sine-wave "noise", so a game replays identically. |
| Seeded level generation | Same seed, same game; different seeds give variety so a network cannot just memorise one layout. |
| Circles and thick lines for collision | Simplest possible collision math, and cheap to compute. |
| Everything painted in code | No asset pipeline, nothing to load from disk. |

## 6. One frame, end to end

```
main.cpp        readPlayerAction()         W held -> AntAction{forward 1, turn 0}
main.cpp        world.step(action, 1/60)
  instinct.cpp    instinctAction() x 9     one AntAction for each other ant
  world.cpp       for every living ant:
                    move                   turn, then walk along the heading
                    resolveCollisions      pushed out of plates and walls
                    applyPoison            health drains in a puddle
                    pickUpFood             touching a crumb -> state = Carried
                    deliverFood            inside the nest -> Delivered
                    layScent               mark the ground underfoot
  pheromones.cpp  update                   scent spreads and fades
  world.cpp       clock                    timeRemaining -= 1/60
main.cpp        renderer.draw(world)
  renderer.cpp    3D scene                 ground, decals, objects, ants, sprites
  hud.cpp         overlay                  panels, mini-map, antennae, messages
```

## 7. Emergent behaviour worth watching

Run `ant_game --autopilot` and watch the mini-map:

1. Ants fan out from the nest, leaving a blue haze of home scent.
2. The scattered food near the nest is found quickly by wandering.
3. Eventually one ant finds the basket's doorway and carries a piece home,
   drawing the first orange line from the basket to the nest.
4. Other ants crossing that line turn onto it. Each success reinforces it.
   Within a minute it is a busy highway and the basket empties fast.
5. When the food is gone nobody renews the trail, and it evaporates.

Nothing in the code describes steps 3 to 5. They emerge.

## 8. Tests (`tests/`)

Everything except drawing is tested: the rules, the network, the senses and training. The tests use a
tiny home-made framework (`tests/test_framework.h`, three macros: `TEST`,
`CHECK`, `CHECK_NEAR`), so there is no test library to install.

| File | What it proves |
| --- | --- |
| `test_vec2.cpp` | the vector math is right |
| `test_level.cpp` | levels are repeatable per seed; food is reachable and safe; the basket has a doorway |
| `test_pheromones.cpp` | scent deposits, evaporates, diffuses, and does not cross solid cells |
| `test_world.cpp` | each game rule, one at a time, on tiny hand-made levels |
| `test_instinct.cpp` | single rules of the instinct, and a whole colony foraging the real level with no player |
| `test_network.cpp` | matrix math, the forward pass (including the worked example from the guide), random starting weights, and saving and loading brain files |
| `test_senses.cpp` | each sense reacts to the one thing it should; a network can drive an ant |
| `test_training.cpp` | backpropagation (against the guide's worked example and against a gradient check), learning XOR, headless episodes and fitness, recording and loading examples, imitation, mutation, and a repeatable evolution run |

Run them with `ctest --test-dir build -C Release --output-on-failure`.

## 9. Build targets

| Target | Contents | Depends on |
| --- | --- | --- |
| `ant_core` | static library: `src/core` | nothing |
| `ant_nn` | static library: `src/nn` | nothing |
| `ant_agent` | static library: `src/agent` | `ant_core`, `ant_nn` |
| `ant_training` | static library: `src/train` | `ant_agent` |
| `ant_train` | the training tool | `ant_training` |
| `ant_tests` | the unit tests | `ant_training` |
| `ant_game` | the playable game | `ant_training`, raylib |

`ant_nn` deliberately knows nothing about ants: it is plain math that could
drive anything. `ant_agent` is the thin layer that says "these 22 numbers are
what an ant senses, and these 2 outputs are how it moves".

### 9.1 The neural network in the running game

```
World --observe()--> 22 numbers --Network::run()--> every neuron's value --> AntAction
        (agent/senses)            (nn/network)        |                      (agent/brain)
                                                      v
                                              drawNetworkView()
                                             (render/network_view)
```

- `main.cpp` holds one `Network`. Each simulation step, whoever is currently
  driving the player's ant (keyboard, instinct or network) supplies its action.
- The network view asks the network what it would do *right now*, whoever is
  driving, and draws every neuron and connection. So you can watch its
  reaction while playing by hand, then press `B` and let it take over.
- Training happens outside the game, in `ant_train`. See [NEURAL_NETWORK.md](NEURAL_NETWORK.md) for
  the math and [TRAINING.md](TRAINING.md) for how it will be trained.

## 10. What is not simulated (yet)

Ideas that would fit the same structure, roughly in order of effort:

- **Poisoned bait** - food that looks normal but harms the colony when brought
  home (how real ant bait works).
- **Food kinds that matter** - sugar gives energy, protein feeds larvae.
- **Hunger and energy** - ants tire and must eat; the colony starves without food.
- **Ants bumping into each other** and traffic jams on busy trails.
- **Alarm scent** - a third pheromone that warns others away from danger.
- **Rival colonies**, spiders, or a human hand that occasionally comes down.
