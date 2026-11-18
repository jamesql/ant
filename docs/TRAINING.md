# Training Guide

[NEURAL_NETWORK.md](NEURAL_NETWORK.md) explains what a network is and the
math inside it. This document covers everything around it:

1. how training works, step by step
2. what you have at the end of training
3. how to use it
4. how to shape it, for example "an ant that only goes after berries"

Imitation learning and neuroevolution are built and working; Part 0 shows how
to run them and what they achieved. A few ideas later in this document are
still plans, and each is marked "not built yet". Terms are defined where they
first appear and collected in a glossary at the end.

---

## Part 0 - Quick start: train a brain and watch it

### 0.1 The commands

The training tool is `ant_train`. Run it with no arguments for its options.

```
ant_train xor                                             prove the learning math works
ant_train imitate --seeds 1-20 --out brains/copycat.brain copy the instinct
ant_train evolve  --start brains/copycat.brain --first-seed 100 --out brains/forager.brain
ant_train score   --brain brains/forager.brain --seeds 1000-1039
```

To train an existing brain further, start from it and give a `--first-seed`
it has not used yet, so it meets new maps:

```
ant_train evolve --start brains/forager.brain --first-seed 5000 --sigma 0.05 --out brains/forager2.brain
```

Then watch the result in the game, with the same brain driving your ant and
the other nine:

```
ant_game --brain brains/forager.brain --colony-brain brains/forager.brain
```

Both brains produced by these commands are included in the `brains/` folder,
so the last command works without training anything.

**If Windows blocks `ant_train.exe`** ("An Application Control policy has
blocked this file"), that is Smart App Control refusing a freshly built,
unsigned program. Build and run the tool inside WSL instead; brain files are
plain text, so the Windows game loads them unchanged. From the project folder
inside WSL:

```
cmake -S . -B ~/ant-build -DANT_BUILD_GAME=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build ~/ant-build -j
~/ant-build/ant_train xor
```

On Windows, run long training in a normal foreground terminal. Runs started
in the background were throttled to a fraction of their speed.

### 0.2 What happened when we ran it

Every map is random: the nest, the basket, the obstacles, the poison and the
food are all placed differently for each seed. All scores below are measured
on 40 maps that no brain was trained on (seeds 1000-1039), full 4-minute
games, every ant in the colony driven by the same brain:

| Brain | How it was made | Food delivered per game (of 14) | Games won (of 40) |
| --- | --- | --- | --- |
| random | untrained | 0.0 | 0 |
| `copycat` | imitation of the instinct | 5.9 | 3 |
| (not kept) | `copycat` + 32 generations of evolution **on one fixed map layout** | 9.4 | 10 |
| `forager` | the above + 36 generations of evolution **on random maps** (about 16 minutes) | 11.7 | 24 |
| the instinct | hand-written rules | 11.8 | 26 |

What this shows:

- **Imitation alone gets part-way.** The loss falls and the colony goes from
  delivering nothing to delivering a good part of the food, but it does not
  match its teacher, for the reasons given in 2.1 (drift, and a teacher whose
  wandering depends on things the network cannot sense).
- **Imitation is also unreliable.** Retraining it on 40 random maps reached a
  *lower* loss (0.073) yet a *worse* colony (2.0 pieces per game). A low loss
  means "usually presses what the teacher would", which is not the same as
  "forages well". The better copycat was kept.
- **Training on one map teaches the map.** The middle row is the history of
  this project. Before maps were randomised, obstacles and the basket were
  always in the same place, and a brain evolved there scored 13.1 on "unseen"
  games that only moved the food. On genuinely different maps it drops to
  9.4. It had partly memorised the layout. This is overfitting (see 1.3), and
  it stayed hidden for as long as the test maps resembled the training maps.
- **Training on random maps fixes it.** Continuing evolution with a different
  map every game brought the same brain up to the level of the hand-written
  rules on maps it has never seen.

The exact numbers will differ slightly from one compiler to another, because
shuffling the examples is not done identically everywhere.

### 0.3 Teaching it your own way of playing

```
ant_game --record my-play.examples          play normally; your play is saved when you quit
ant_train imitate --data my-play.examples --out brains/me.brain
ant_game --brain brains/me.brain
```

Recording stores what your ant sensed and which keys you held, every step.
A few minutes of play gives several thousand examples. Starting from an
existing brain (`--start brains/forager.brain`) and then imitating you nudges
that brain toward your habits instead of starting from nothing.

---

## Part 1 - The picture to keep in mind

A **brain** is a network's list of numbers (its weights and biases; 402 of
them for the network in NEURAL_NETWORK.md). With random numbers the ant
staggers about. **Training** is a loop that keeps nudging those numbers until
the ant behaves the way we want:

```
        +--------------------------------------------------+
        |                                                  |
        v                                                  |
   let the brain drive an ant  -->  measure how well  -->  adjust the
   in the simulation                it did                 numbers a little
```

Three things in that loop are choices we make, and they are the three levers
we have over what the ant ends up doing:

| Lever | Question it answers |
| --- | --- |
| **Senses** | What can the ant perceive? (the network's inputs) |
| **Goal** | What counts as "doing well"? (targets, fitness or reward) |
| **Starting point** | Does it start from random numbers or from an existing brain? |

Everything in Part 5 ("shaping behaviour") is one of these three levers.

### 1.1 Where training happens

Training never opens the game window. It uses the simulation library
(`ant_core`) directly, which is why the simulation was built with no graphics
in it:

```cpp
World world(makePicnicLevel(seed));
while (world.status() == GameStatus::Playing) {
    for (each ant i)
        actions[i] = brainOrInstinct(world, i);   // senses in, action out
    world.step(actions, 1.0f / 60.0f);
}
// now read the result: world.deliveredCount(), world.ants()[0].deliveries, ...
```

One complete game like this is called an **episode**.

### 1.2 Who the brain controls

`World::step` takes one action per ant, so we can choose:

- **A whole colony sharing one brain.** Every ant runs the same network. This
  is what `ant_train` does. It is the real test (can the brain replace the
  instinct entirely?), and ten ants per game give ten times the experience.
- **One learner among instinct ants.** The brain drives ant 0; the other nine
  use the built-in instinct. The game can do this (`ant_game --brain FILE`),
  but training in this mode is not built yet.

### 1.3 Seeds, and why we never train on just one

`makePicnicLevel(seed)` builds a different map for every seed: the nest, the
basket, the obstacles, the poison and the food all move. If we trained on one
map only, the network could simply memorise "turn left after three seconds"
for that one map. That is called **overfitting**: doing well on what you
practised and badly on anything new.

So we always:

- train on **many maps**, and
- judge the result on **maps it has never seen** (seeds 1000 and up).

The score on unseen maps is the honest one. For it to be honest, the unseen
maps must differ from the training maps in every way that matters. Part 0.2
tells how this project learned that: while only the food moved between maps,
a brain could memorise where the basket was and still pass the test.

---

## Part 2 - The three ways to train

### 2.1 Imitation learning: copy a teacher

**Idea.** Watch someone who already plays well, and train the network to
press the same "buttons" in the same situations. We have two teachers
available: **you**, and the **instinct**.

**Step 1 - Record.** Run episodes with the teacher in control. At every step
save one **example**: what the ant sensed, and what the teacher did.

```
example = ( observation: 22 numbers ,  action: 2 numbers )
```

At 60 steps per second, one 4-minute game gives 14,400 examples. The pile of
all recorded examples is the **dataset**.

**Step 2 - Split.** Set aside about a fifth of the dataset as the
**validation set**. The network never trains on it; we only use it to check
whether the network is learning the *behaviour* or just memorising the
examples it trained on.

**Step 3 - Train.** This is exactly the math of NEURAL_NETWORK.md Part 3. The
teacher's action is the target `y`.

```
repeat for many epochs:
    shuffle the training examples
    for each mini-batch of 32 examples:
        forward pass on each example            -> network's action a
        loss for each example                   -> 1/2 * |a - y|^2
        backward pass on each example           -> gradients
        average the gradients over the batch
        every weight:  w <- w - learningRate * averageGradient
    measure the average loss on the validation set
```

New terms:

- An **epoch** is one pass through the whole training set.
- A **mini-batch** is a small group of examples whose gradients are averaged
  before each update. Averaging smooths out the noise of single examples, and
  shuffling stops the network from learning the order of the recording.
- The **batch loss** is the average of the example losses:

  ```
  L = (1/N) * sum over the N examples of  1/2 * |a - y|^2
  ```

**Step 4 - Know when to stop.** Watch two numbers, the loss on the training
set and the loss on the validation set:

```
loss
 |\
 | \ .
 |  \  ' .                              validation
 |   \     ' - . _ _ _ _ . . - - ' '    <- starts rising: stop here
 |    \_
 |      '--.___________                 training
 +------------------------------------> epochs
```

While both fall, the network is learning. When validation loss stops falling
or starts to rise while training loss keeps falling, the network has begun to
memorise. Keep the brain from the epoch with the lowest validation loss.

**Strengths and limits.**

- Fast, and uses the math directly, so it is the best first exercise.
- The ant can only become about as good as its teacher.
- **Unbalanced data.** Most recorded moments are "walk straight ahead". A
  network can score a low loss by always walking straight and never learning
  the rare, important moments (turning at a wall, turning onto a trail). The
  fix is to keep every example where the teacher turned and only a fraction
  of the ones where it did not.
- **Drift.** A small mistake puts the ant somewhere the teacher never went,
  where it has no examples to rely on, so mistakes can snowball. This is the
  main reason to follow imitation with one of the next two methods.

### 2.2 Neuroevolution: survival of the fittest

**Idea.** No teacher and no derivatives. Make many brains, let each one play,
keep the best, and make slightly changed copies of them. Repeat.

```
create a population of 100 brains (random, or copies of an imitation brain)

repeat for many generations:
    for each brain:
        play one episode on each of a few maps (new maps every generation)
        score = average fitness over those episodes
    sort brains by their average score over every generation they have played
    keep the top 20 unchanged                      (the "elite")
    refill to 100 with mutated copies of the elite

at the end: the elite all play a larger "final exam" of fresh maps;
            the one that does best there is the result
```

New terms:

- A **population** is the set of brains being compared.
- A **generation** is one round of play, score, select, mutate.
- **Fitness** is one number saying how well a brain did. Higher is better.
  (It plays the role the loss plays in backpropagation, upside down.)
- The **elite** are the best brains, carried into the next generation
  unchanged so that progress is never lost.
- **Mutation** makes a slightly different copy of a brain by adding a small
  random amount to every number in it:

  ```
  w_new = w_old + sigma * n        n = a random number from a bell curve
                                       centred on 0 (mostly small, rarely big)
  ```

  `sigma` is the **mutation strength**. A typical value is 0.05 to 0.1. Too
  large and children are nothing like their parents (random search); too
  small and nothing ever changes.
- **Crossover** (optional, not built yet) builds a child from two parents by taking each
  number from one or the other at random.

**The fitness function is where you say what you want.** A starting point for
"be a good forager":

```
fitness =  100 * pieces of food the colony delivered
        +   25 * times an ant picked food up
        +    2 * world units by which carried food got closer to the nest
        -   40 * times an ant died
        +    1 * seconds left on the clock, if the colony won
```

These five numbers are the `FitnessWeights` struct in `src/train/episode.h`.

Each line exists for a reason:

- **Delivered** is the actual goal.
- **Picked up** and **carried closer** are stepping stones. A random brain almost
  never delivers anything, so if delivery were the only thing rewarded, every
  brain in the first generation would score zero and evolution would have
  nothing to select. Smaller rewards for getting part-way give it a slope to
  climb. This is called **reward shaping**.
- **Died** teaches it to avoid poison.

**Strengths and limits.**

- Simple to write, needs no backpropagation, and is fun to watch.
- It finds whatever maximises the number you wrote, which is not always what
  you meant (see 6.1).
- It needs a great many episodes. This is what the headless simulation is
  for, and why episodes during training will be short.

### 2.3 Reinforcement learning: learn from reward

**Idea.** One ant plays. Good events give it a **reward** (a number), bad
events a penalty. After the episode, each action it took is made a little
more likely if what followed was good and a little less likely if it was bad.
The adjusting is done with backpropagation.

It combines the strengths of the other two (it uses gradients like imitation,
and needs no teacher like evolution), and it is also the most intricate to
get right. It is **not built yet**.

### 2.4 The order we will do them in

```
1. Imitate the instinct     quick; proves the whole pipeline works
2. Imitate you              the ant picks up your habits
3. Evolve from there        start the population from the imitation brain,
                            so generation 1 already forages a bit
4. Reinforcement learning   later (not built yet)
```

Starting evolution from an imitation brain instead of random numbers skips
the long, boring phase where every brain walks in circles.

---

## Part 3 - What training produces

### 3.1 The brain file

The final output of training is one small file containing everything needed
to rebuild the network: its shape and its numbers. The format is
plain text, so you can open it and look:

```
ant-brain 1
name      forager
senses    standard-22
layer     22 16 tanh
  0.4127 -0.0931  0.2210 ...        16 rows of 22 weights
  ...
  0.0114 -0.2001 ...                16 biases
layer     16 2 tanh
  0.3310  0.0872 ...                 2 rows of 16 weights
  ...
 -0.0450  0.1203                     2 biases
```

| Line | Meaning |
| --- | --- |
| `ant-brain 1` | file type and format version |
| `name` | a label for you |
| `senses` | which list of inputs this brain expects (see 5.1) |
| `layer 22 16 tanh` | a layer with 22 inputs, 16 neurons, `tanh` activation |
| the numbers | that layer's weight matrix, then its biases |

For our network that is 402 numbers: a few kilobytes. There is no code in the
file. The "program" is always the same forward pass; only the numbers differ
from one ant to another.

### 3.2 The training log

Alongside the brain, evolution writes a log (`forager.brain.log`), one line per
generation: the leader's score (averaged over every generation it has survived)
and the average of the whole population. These lines are from the real run on
random maps in Part 0:

```
generation best_fitness average_fitness
1 2138.23 891.774
2 2173.56 1037.92
...
18 2509.32 1321.49
...
36 2387.22 1545.33
```

The leader's score jumps about, because each generation plays different maps
and some are harder than others. The population average is the steadier
sign: it climbed from about 900 to about 1,550.

Plotted, this is the **learning curve**. It tells you whether training is
still improving, has levelled off, or has gone wrong.

### 3.3 Checkpoints

Every 10 generations evolution saves the current best brain as a **checkpoint**
(`forager-gen010.brain`, `forager-gen020.brain`, ...). They let you:

- watch how behaviour developed over time,
- go back if a later change made things worse,
- use any of them as the starting point for something new (see 5.3).

---

## Part 4 - Using a brain

```
ant_train score --brain brains/forager.brain --seeds 1000-1019

ant_game --brain brains/forager.brain            your ant is driven by the brain
ant_game --colony-brain brains/forager.brain     every other ant is driven by it
```

- **`score`** plays unseen seeds with no window and prints the numbers that
  matter: food delivered, deaths, time taken. This is how two brains are
  compared fairly.
- **`--brain`** lets you sit behind the brain's eyes and watch it decide, with
  the live network view open. It
  uses the same slot `--autopilot` uses today for the instinct.
- **`--colony-brain`** replaces the instinct in the other nine ants while you
  play normally.

In code, a brain is a drop-in replacement for `instinctAction`:

```cpp
AntAction action = brainAction(brain, world, antIndex);   // instead of instinctAction(world, antIndex)
```

Because every ant takes its own action, different ants can use different
brains in the same game. A colony could have six foragers and three berry
specialists; real colonies divide work between castes in just this way.

---

## Part 5 - Shaping behaviour

### 5.0 Why you cannot just edit the numbers

It is natural to open the brain file hoping to find the "likes berries"
number and turn it up. There isn't one. Each behaviour is spread across many
weights, and each weight takes part in many behaviours. Changing one number
by hand changes a little of everything and usually makes the ant worse.

So we do not edit the brain directly. We change what it is trained *on* and
let training find the numbers. The three levers from Part 1 follow.

What you can do today: change the five fitness numbers in
`src/train/episode.h` and rebuild (lever 2), train on your own recorded play
(lever 2), and start any run from an existing brain with `--start` (lever 3).
The rest of this part describes extensions that are **not built yet**: senses
that tell food kinds apart, fitness rules that depend on the kind of food,
and command inputs. Looking inside a brain and adding a neuron by hand (5.5) are
built.

### 5.1 Lever 1 - Senses: an ant cannot act on what it cannot perceive

This is the lever people forget. With the standard 22 inputs, an ant senses
"there is food over there" but not *what kind*. No amount of training can
make that ant prefer berries: to it a berry and a crumb are identical.

To make food preferences possible, the food-in-sight inputs (three numbers:
direction `sin`, direction `cos`, closeness) are given once **per kind**:

```
standard-22      food in sight:   3 numbers
by-kind-28       crumb in sight:  3 numbers
                 sugar in sight:  3 numbers
                 berry in sight:  3 numbers          (6 more inputs)
```

The network grows a little: `16 * 28 + 16 + 2 * 16 + 2 = 498` numbers. This
is what the `senses` line in the brain file records, so a brain is never fed
inputs it was not trained on.

The same reasoning applies to anything else. Want ants that avoid crowds?
They need an input for "other ants nearby". Want ants that react to an alarm
scent? The simulation needs that scent and the network needs to smell it.

### 5.2 Lever 2 - The goal: say what you want in numbers

**With evolution**, edit the fitness function. Some recipes:

| Behaviour wanted | Change to the fitness |
| --- | --- |
| **Berry specialist** | `+100` per berry delivered, `0` for other kinds, `-20` for picking up a non-berry. |
| **Cautious ant** | `-200` per death and `-5` per second spent in poison. |
| **Speed runner** | Bigger bonus for time left; small penalty per second. |
| **Basket raider** | Only food taken from inside the basket counts. |
| **Trail builder** | Reward food delivered by *other* ants that followed this ant's trail (measured as: the colony's total, not this ant's own). |
| **Scout** | Reward ground covered that no ant has visited yet. |

**With imitation**, change the demonstrations instead. Play twenty games
where you only ever fetch berries, record them, and train on that. The
network copies the habit along with everything else you do.

Everything the fitness function needs is already available from the
simulation: `ant.deliveries`, `ant.alive`, `ant.health`, each food's `kind`
and `state`, `world.deliveredCount()`, `world.timeRemaining()`.

### 5.3 Lever 3 - The starting point: fine-tuning

You rarely need to start from random numbers. To make a berry specialist:

```
ant_train evolve --start brains/forager.brain --senses by-kind-28 \
                 --fitness berry --generations 60 --out brains/berry-hunter.brain
```

The population starts as mutated copies of a brain that already knows how to
walk, avoid poison, follow trails and find the nest. It only has to learn
what is new. This is called **fine-tuning**. It is much faster than training
from scratch, and the specialist keeps the general skills.

(When the senses change, as here, the new inputs' weights start at zero so
the brain behaves exactly as before until training gives them meaning.)

### 5.4 One steerable brain instead of many specialists

Specialists need one brain per behaviour. The alternative is a single brain
with extra **command inputs** that you set, for example three numbers saying
how much it should want each kind of food:

```
want crumb, want sugar, want berry        e.g.  0, 0, 1  = "fetch berries"
```

During training each episode picks a random command, and fitness rewards
delivering the kinds that command asked for. The brain learns to read the
command, and afterwards you can change what an ant goes after while the game
is running, simply by changing those three inputs. It is harder to train than
a specialist but far more flexible.

### 5.5 Looking inside a brain: `inspect`, `silence` and `graft`

A brain's numbers do not explain themselves, but three experiments do.

```
ant_train inspect --brain brains/forager.brain
ant_train silence --brain brains/forager.brain --neuron 8
ant_train graft   --brain brains/forager.brain --inputs 7=4,5=-4 --turn 4
```

**Ablation: "what breaks if I remove this?"** `inspect` silences each hidden
neuron and each sense in turn, lets the colony play 40 maps, and prints what
it lost. A silenced neuron is replaced by its usual (average) value, so only
the *information* it carried is removed. This is the strongest evidence of
what a part is for, because it tests cause.

What it found in `forager` (food per game lost when silenced; the whole
brain delivers about 12):

| Part | Loss | What it appears to do |
| --- | --- | --- |
| neuron h10 | -11.3 | **Homing.** Listens to "carrying" and "nest: side" and pushes the turn output. Without it ants still pick food up but stop bringing it home. |
| neuron h1 | -10.4 | General steering from the nest direction and the left feeler. |
| neurons h2, h11, h15 | -8.2, -8.3, -2.5 | **Trail following.** All listen to food scent left and ahead. |
| neuron h5 | -1.6 | **Obstacle avoidance on the right**: listens to "feel right" (+6.0), turns left. |
| neurons h4, h8 | about 0 | Nothing measurable. The brain works as well with 14 neurons. |
| sense "feel far left" | -10.1 | The single most relied-on sense. |
| sense "carrying" | -9.2 | Without it the ant cannot switch between searching and homing. |
| senses "health", "nest: ahead", "food seen: near" | about 0 | Ignored. |

Two things stand out. No single neuron handles poison: with any one poison
sense or any one neuron removed, deaths stay at zero, so that skill is spread
across several parts. And the brain is lopsided: it leans on its left-hand
senses far more than its right-hand ones ("feel far left" -10.1 against
"feel far right" -2.1), a quirk evolution happened to settle on.

**The Jacobian: "what pushes what?"** For each sense and each action, how
much a small increase in the sense changes the action, averaged over the
moments the ants actually lived through. For our network it is
`J = diag(1 - o^2) * W2 * diag(1 - h^2) * W1`, which is the chain rule of
NEURAL_NETWORK.md applied from an output back to the inputs. The largest
entries of `forager`'s Jacobian for the turn output (positive = turn right):

| Sense | While searching | While carrying |
| --- | --- | --- |
| food scent left / right | -6.85 / +6.78 | -1.62 / +0.70 |
| poison left / right | +2.86 / -2.09 | +3.52 / -2.27 |
| food seen: side | +2.77 | +0.70 |
| feel far left / feel right | +1.84 / -1.12 | +0.92 / -1.06 |
| nest: side | +0.38 | +1.31 |

Read as rules: turn toward the stronger food scent; turn away from poison;
turn toward food in sight; turn away from whatever a feeler touches; and,
only when carrying, turn toward the nest and mostly ignore food scent. Nobody
wrote these rules. They are the ones the hand-written instinct uses, found
again by training. The walk output's Jacobian is almost zero everywhere: the
brain simply walks at full speed all the time and does all its work by turning.

**Grafting: "can I add a behaviour?"** `graft` attaches one new hidden neuron
with weights you choose (senses are numbered from 0 in the order of
NEURAL_NETWORK.md section 1.3) and measures the result on 200 maps.

| Hand-made neuron | Food per game | Wins | Deaths per game |
| --- | --- | --- | --- |
| none (the `forager` as trained) | 12.07 | 63% | 0.00 |
| "veer right": always on, pushes turn | 10.45 | 40% | 0.00 |
| "poison lover": turns toward poison | 11.01 | 44% | 0.15 |
| "eager": turns harder toward food in sight | 11.60 | 56% | 0.00 |
| "follow home scent when carrying" (two neurons) | 12.2 | 62% | 0.00 |
| "steer harder for the nest when carrying" (two neurons) | 7.4 | 13% | 0.00 |

So yes, a property can be wired in by hand, and it shows up in behaviour: the
poison lover is the only brain here whose ants die. But none of the grafts
made the colony better, and one that looked sensible ("steer harder for
home") nearly halved its food. The trained weights are balanced against each
other, and a strong new push upsets that balance. A graft is best used as a
starting point: add the neuron, then let evolution (`--start`) adjust
everything around it.

### 5.6 Other things you can do with finished brains

| Operation | What it does |
| --- | --- |
| **Compare** | `ant_train score` two brains on the same unseen seeds. |
| **Blend** | Crossover of two brains as the seed of a new population (for example a cautious one and a fast one), then fine-tune. |
| **Mix in a colony** | Give different ants different brains and see how the colony does. |
| **Roll back** | Return to an earlier checkpoint. |

---

## Part 6 - Settings and pitfalls

### 6.1 You get what you measure, not what you meant

A network finds the easiest way to raise the number it is judged by. Typical
surprises:

- Reward "being close to food" and the ant may stand next to food forever
  without picking it up, because leaving would lower its score.
- Reward "picking up" too generously and it may learn to pick up food and
  wander, never delivering.
- Punish death harshly and it may learn to stay in the nest.

When behaviour looks strange, the first question is always: "is this
actually the best way to score well under the rule I wrote?" It usually is.
Fix the rule, not the ant.

### 6.2 The main settings (hyperparameters)

Settings that we choose, as opposed to numbers the training finds, are called
**hyperparameters**.

| Setting | Used in | Typical | If too small | If too large |
| --- | --- | --- | --- | --- |
| Learning rate | imitation | 0.05 | learns very slowly | loss jumps around or explodes |
| Mini-batch size | imitation | 32 | noisy, slow | fewer updates per epoch |
| Epochs | imitation | 20-100 | under-trained | memorises (overfits) |
| Hidden neurons | all | 16 | cannot represent the behaviour | slow; memorises more easily |
| Population | evolution | 64 | gets stuck early | each generation is slow |
| Elite kept | evolution | 20% | loses good brains | too little variety |
| Mutation strength | evolution | 0.05-0.1 | nothing changes | children are random |
| Maps per generation | evolution | 4 | lucky brains win | each generation is slow |
| Episode length | evolution | 240 s | cannot reach far food | each generation is slow |

Change one at a time, and keep the training log of each run so runs can be
compared.

### 6.3 Checklist when training is not working

1. **Does the pipeline work at all?** Train on a tiny dataset and check the
   loss goes to almost zero. If it cannot memorise ten examples, there is a
   bug, not a tuning problem.
2. **Are the inputs sensibly scaled?** Print a few observations. Everything
   should be roughly between -1 and 1.
3. **Is the data balanced?** Count how many examples have a turn in them.
4. **Is fitness all zeros in generation 1?** Then add stepping-stone rewards
   or start from an imitation brain.
5. **Good on training seeds, bad on unseen ones?** Overfitting: use more
   seeds, fewer epochs, or a smaller network.
6. **Watch it play.** `ant_game --brain ...` usually shows the problem within
   seconds.

---

## Glossary

| Term | Meaning |
| --- | --- |
| **Brain** | A trained network's shape and numbers; the contents of a brain file. |
| **Checkpoint** | A brain saved part-way through training. |
| **Command input** | An extra input we set by hand to tell a brain what we want from it. |
| **Crossover** | Building a child brain by mixing the numbers of two parents. |
| **Dataset** | All the recorded examples used for imitation learning. |
| **Elite** | The best brains of a generation, kept unchanged. |
| **Episode** | One complete game played from start to finish. |
| **Epoch** | One pass through the whole training set. |
| **Example** | One recorded pair: what the ant sensed, and what the teacher did. |
| **Fine-tuning** | Training that starts from an existing brain rather than random numbers. |
| **Fitness** | A score for how well a brain did; higher is better. |
| **Generation** | One round of play, score, select and mutate. |
| **Hyperparameter** | A setting we choose (learning rate, population size...), not one training finds. |
| **Imitation learning** | Training a network to copy a teacher's actions. |
| **Learning curve** | A plot of loss or fitness against training time. |
| **Mini-batch** | A small group of examples whose gradients are averaged for one update. |
| **Mutation** | A small random change to every number in a brain. |
| **Neuroevolution** | Improving brains by selection and mutation instead of gradients. |
| **Overfitting** | Doing well on what was practised and badly on anything new. |
| **Population** | The set of brains being compared in one generation. |
| **Reinforcement learning** | Learning from rewards earned by the ant's own actions. |
| **Reward shaping** | Adding smaller rewards for partial progress so learning has a slope to climb. |
| **Seed** | The number that selects a map; same seed, same map. |
| **Validation set** | Examples held back from training, used only to check for overfitting. |
