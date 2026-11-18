# Weird brains

Seven ant brains with strange habits. None of them was trained. Each one is
the trained `forager` brain with a few of its numbers changed by hand, to
show that behaviour really does live in the weights: change the right
numbers and you get a different ant.

To watch one, give the same file to your ant and to the rest of the colony:

```bash
build/Release/ant_game.exe --brain brains/weird/escapist.brain --colony-brain brains/weird/escapist.brain
```

Press `B` at any time to take your own ant back, and `N` to hide the network view.

## The gallery

Measured over 50 random maps, 4-minute games, every ant driven by the same
brain. The first row is the unmodified `forager`, for comparison.

| Brain | What it does | Food per game | Deaths per game | Seconds in poison | Poisoned trips home | Distance from nest | Time at map edge | Turns per minute | Walking backwards |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `forager` | the normal one | 12.0 | 0.00 | 0.9 | 0.2 | 19.6 | 10% | +3.8 | 0% |
| `amnesiac` | finds food, forgets the way home | 0.9 | 0.00 | 1.1 | 0.0 | 21.8 | 7% | -9.5 | 1% |
| `spinner` | turns in circles on the spot | 0.0 | 0.00 | 0.0 | 0.0 | 2.2 | 4% | +28.6 | 0% |
| `moonwalker` | walks backwards until it hits the edge of the world | 0.0 | 0.00 | 0.2 | 0.0 | 20.5 | 97% | +0.5 | 100% |
| `homebody` | never leaves the nest | 0.0 | 0.00 | 0.0 | 0.0 | 1.9 | 11% | +12.0 | 0% |
| `escapist` | runs from home and pushes against the edge of the world | 0.0 | 0.00 | 1.9 | 0.0 | 33.8 | 94% | 0.0 | 1% |
| `poison-lover` | steers into poison | 11.8 | 0.98 | 30.1 | 5.2 | 19.2 | 9% | +4.3 | 0% |
| `poison-courier` | soaks in poison, then walks home poisoned | 11.4 | 0.08 | 25.3 | 20.2 | 18.2 | 10% | +4.4 | 0% |

How to read the columns:

- **Seconds in poison** is added up over all ten ants.
- **Poisoned trips home** counts each time an ant walks into the nest with less than full health.
- **Distance from nest** is in world units; the map is 52 by 34.
- **Time at map edge** is the share of the time an ant is within 0.75 units of the edge.
- **Turns per minute** is average turning; 28.6 is the fastest an ant can spin.

## What was changed, brain by brain

Some background for reading this section:

- The forager has 22 inputs (senses), 16 hidden neurons and 2 outputs (walk, turn).
- Senses are numbered from 0 and hidden neurons from 1 (as `ant_train inspect` prints them).
- **Turn** is positive for "turn right". A **"side"** sense is positive when the thing is on the ant's right.
- Three kinds of edit are used:
  - **Silence a neuron**: fix a hidden neuron at its average value, so it stops reacting. In the file this zeroes that neuron's two outgoing weights and adds a constant to the two output biases.
  - **Cut a sense**: fix an input at a value (here always 0). This zeroes that sense's 16 outgoing weights.
  - **Graft a neuron**: add a 17th (18th...) hidden neuron with weights chosen by hand. It computes `tanh(weights . senses + bias)` and adds its output weight times that to walk or turn.

The exact commands are in [build.sh](build.sh), which rebuilds every brain here from `brains/forager.brain`.

### amnesiac

| Edit | Detail |
| --- | --- |
| Silence neuron h10 | h10 listens to "carrying" and "nest: side" and pushes turn (+2.58). It is the forager's homing neuron. |

Nothing else changes. Ants still find and pick up food (7.6 pickups per game)
but wander with it instead of bringing it home (0.9 delivered).

### spinner

| Edit | Detail |
| --- | --- |
| Graft one neuron | no inputs, bias +3 (so it always outputs +1), turn weight +8 |

A constant push of +8 on the turn output swamps everything the trained
network says, so the turn output sits at +1: full right turn, forever.

### moonwalker

| Edit | Detail |
| --- | --- |
| Graft one neuron | no inputs, bias +3 (always +1), walk weight -12 |

The same trick on the walk output. The ant reverses at full speed while its
steering, which assumes it is going forwards, makes things worse. It ends up
backed against the edge of the map.

### homebody

| Edit | Detail |
| --- | --- |
| Graft one neuron | input "nest: side" (sense 17) weight +3, turn weight +8 |

Nest on the right gives a strong right turn, nest on the left a strong left
turn, whatever else is going on. The ants circle the nest at about 2 units.

### escapist

| Edit | Detail |
| --- | --- |
| Silence neuron h10 | removes homing, so nothing pulls it back |
| Cut senses 0 to 4 | all five feelers fixed at 0 ("nothing there"), so it does not turn away from the edge of the map |
| Graft one neuron | input "nest: side" (sense 17) weight +3, turn weight -8: the homebody neuron with the sign flipped |

It turns until the nest is directly behind it and walks until the world
stops, then keeps pushing: 94% of its time is spent against the edge. With
its feelers cut it also walks into plates and basket walls on the way.

### poison-lover

| Edit | Detail |
| --- | --- |
| Cut senses 5, 6, 7 | the three poison senses fixed at 0 for the trained network, which removes everything it learned about avoiding poison |
| Graft one neuron | "poison right" (sense 7) weight +4, "poison left" (sense 5) weight -4, turn weight +8 |

The grafted neuron reads the same poison senses the other way round: it
turns toward poison instead of away. About one ant per game dies, and the
colony spends 30 seconds standing in puddles. It still forages almost as well
as the forager (11.8), because the rest of the brain is untouched.

### poison-courier

In this simulation poison is a puddle; an ant cannot pick it up. So this is
the nearest thing weights alone can do: go and get poisoned, then carry the
poison home in its own body.

It starts from the same cut as the poison-lover (senses 5, 6, 7 fixed at 0),
then adds four neurons in two pairs.

| Edit | Detail |
| --- | --- |
| Graft pair A, "seek poison while healthy" | neuron 1: poison right +4, poison left -4, health +8, bias -8, turn +4. Neuron 2: the mirror image (poison left +4, poison right -4, health +8, bias -8, turn -4). |
| Graft pair B, "go home when hurt" | neuron 3: nest: side +3, health -60, bias +51, turn +4. Neuron 4: the mirror image (nest: side -3, health -60, bias +51, turn -4). |

Why pairs: one neuron cannot say "do this only when that is true". But two
mirror-image neurons can. When the pair is switched off, both are pinned at
-1 and their opposite turn weights cancel exactly. When it is switched on,
they pull the same way.

- **Pair A** uses `8 * health - 8`: zero at full health (on), strongly negative when hurt (off).
- **Pair B** uses `-60 * health + 51`: -9 at full health (off), positive once health falls below 0.85 (on).

The result walks into the nest while poisoned about 20 times per game (the
forager: 0.2). It rarely dies (0.08 per game against the poison-lover's 0.98),
because it leaves the puddle to go home, and health recovers on the way.

## Things worth knowing

- **Why two of them still forage well.** The poison brains keep all 16
  trained neurons. The grafts only act near poison or when hurt, so the rest
  of the time they are the forager.
- **Why big weights.** A graft has to out-shout the trained network. An
  earlier poison-lover with a turn weight of 4 and the poison senses left
  intact managed only 0.15 deaths per game: the trained avoidance fought back.
- **None of these is better at anything useful.** They are demonstrations.
  To get a brain that is strange *and* competent, graft the neuron and then
  let evolution adjust the rest: `ant_train evolve --start brains/weird/<name>.brain`.

## Make your own

```bash
ant_train graft   --brain brains/forager.brain --inputs 17=3 --turn 8 --out my.brain
ant_train silence --brain my.brain --neuron 10 --out my.brain
ant_train silence --brain my.brain --sense 20 --value 1 --out my.brain
ant_train behaviour --brain my.brain
```

The third line fixes the "carrying" sense at 1, giving an ant that always
believes it is holding food. `ant_train inspect` shows which neurons and
senses do what in the forager, which is where the ideas above came from.
