# Neural Network Guide

This document explains what a neural network is, every piece of math it needs,
and how the one in this project, which drives an ant in Ant Picnic, is built.

What exists today: the network itself (Parts 1 and 2) runs in the game and
can be watched live. What does not exist yet: learning (Part 3). Until then
the network's numbers are random, so it behaves randomly. Section 4.2 lists
exactly what is built.

Read it top to bottom. Every term is defined the first time it appears, and
there is a glossary at the end.

---

## Part 1 - What we are building

### 1.1 The goal

Every ant in the simulation is driven by an `AntAction`:

```
AntAction { forward, turn }      both between -1 and +1
```

Today the keyboard produces it for your ant, and a set of hand-written rules
called the **instinct** (`src/core/instinct.cpp`) produces it for the others.
We want a third source: a program that looks at an ant's situation and
chooses those two numbers by itself, having *learned* how, well enough to
find food and bring it home.

### 1.2 A neural network is just a function

A neural network is a function: numbers go in, numbers come out.

```
what the ant senses              the network              what the ant does
  (22 numbers)        ---->   f(inputs) = outputs  ---->    (2 numbers)
  "observation"                                             "action"
```

What makes it special is that the function contains a few hundred adjustable
numbers called **parameters**. With one set of parameters the ant spins in
circles; with another set it plays well. **Learning** means searching for a
good set of parameters. That is the whole idea; the rest is detail.

### 1.3 What the ant will sense (the inputs)

A network can only work with numbers, so we must describe the ant's situation
as a list of numbers. We give it the same senses a real ant has, which are
also the ones the instinct already uses. The proposed list, 22 values, each
scaled to roughly `-1..1` or `0..1`:

| # | Input | Meaning |
| --- | --- | --- |
| 1-5 | 5 feelers | Rays fanned across the ant's front. Each reports how close the nearest solid thing (obstacle, wall, edge of the map) is along that ray: `1` = touching, `0` = nothing within reach. |
| 6-8 | poison smell | Left, ahead, right: `1` if there is poison just there, else `0`. |
| 9-11 | food scent | Strength of the food pheromone to the left, ahead and right. |
| 12-14 | home scent | Strength of the home pheromone to the left, ahead and right. |
| 15-16 | food in sight: direction | `sin` and `cos` of the angle from the ant's heading to the nearest food it can see. Both `0` if none is in sight. |
| 17 | food in sight: closeness | `1` = on top of it, `0` = at the edge of sight (or none). |
| 18-19 | nest direction | `sin` and `cos` of the angle from the ant's heading to the nest (an ant's sense of where home is). |
| 20 | nest closeness | `1` = at the nest, `0` = far away. |
| 21 | carrying | `1` if holding food, otherwise `0`. |
| 22 | health | `1` = healthy, `0` = about to die. |

Notice what is *not* in the list: where the food is, where the basket's
doorway is, what the other ants are doing. Like a real ant, the network has
to work that out from scent.

Two notes on why the inputs look like this:

- **Why `sin` and `cos` instead of the angle itself?** An angle jumps from
  `+180` degrees to `-180` degrees when the target passes behind the ant. That
  sudden jump confuses a network. `sin` and `cos` change smoothly all the way
  round.
- **Why scale everything to a small range?** Networks learn far more easily
  when all inputs are of similar size.

### 1.4 The shape of our network

```
 22 inputs          16 hidden neurons         2 outputs
 (observation)                                (forward, turn)

   x1 ---\
   x2 ----\        [h1]
   x3 -----+-----> [h2] ----\
   ...    /        ...       +-----> [forward]
   x22 --/         [h16] ---/+-----> [turn]
```

Every input connects to every hidden neuron, and every hidden neuron connects
to every output. This is called a **fully connected** or **dense** network.
The middle layer is called **hidden** because we never see its values
directly.

---

## Part 2 - The math of running a network (the forward pass)

### 2.1 One neuron

A neuron takes several numbers in and produces one number out, in two steps.

**Step 1: weighted sum.** Multiply each input by its own **weight**, add them
up, and add one extra number called the **bias**:

```
z = w1*x1 + w2*x2 + ... + wn*xn + b
```

- `x1..xn` are the inputs.
- `w1..wn` are the weights: how much the neuron cares about each input. A
  large positive weight means "this input pushes me up", a negative weight
  means "this input pushes me down", near zero means "I ignore this".
- `b` is the bias: the neuron's starting level before any input arrives.

**Step 2: activation.** Pass `z` through a fixed curve called the
**activation function**, written `f`:

```
a = f(z)
```

`a` is the neuron's output, its **activation**.

### 2.2 Why the activation function matters

Without step 2, a neuron is only a weighted sum. Stacking weighted sums gives
just another weighted sum, so a network with a hundred layers could do nothing
more than a network with one. It could only ever draw straight lines.

The activation function bends the line. Many bent pieces combined can
approximate almost any shape, which is what gives networks their power.

We will use `tanh` (the hyperbolic tangent):

```
tanh(z) = (e^z - e^-z) / (e^z + e^-z)

  z:      -3      -1       0       1       3
  tanh:  -0.995  -0.762    0     0.762   0.995
```

It squashes any number into the range `-1..1`. That is perfect for us, because
`forward` and `turn` must be between `-1` and `1`.

Two other common activation functions, for reference:

| Name | Formula | Output range | Typical use |
| --- | --- | --- | --- |
| `tanh` | `(e^z - e^-z) / (e^z + e^-z)` | -1 to 1 | small networks, outputs that can be negative |
| sigmoid | `1 / (1 + e^-z)` | 0 to 1 | outputs that mean "probability" |
| ReLU | `max(0, z)` | 0 to infinity | large, deep networks |

### 2.3 A layer of neurons, written with matrices

A layer is a group of neurons that all read the same inputs. Writing each
neuron's sum separately gets long, so we stack them.

- Put the inputs in a column called the vector `x`.
- Put all the weights in a grid called the matrix `W`, one **row per neuron**.
  `W[i][j]` is the weight from input `j` to neuron `i`.
- Put the biases in a vector `b`, one per neuron.

Then the whole layer is:

```
z = W * x + b          (matrix times vector, plus vector)
a = f(z)               (apply f to each element)
```

A small example, 2 inputs feeding 3 neurons:

```
    | w11 w12 |             | b1 |     | w11*x1 + w12*x2 + b1 |
z = | w21 w22 | * | x1 |  + | b2 |  =  | w21*x1 + w22*x2 + b2 |
    | w31 w32 |   | x2 |    | b3 |     | w31*x1 + w32*x2 + b3 |
```

Each row of the result is exactly the single-neuron formula from 2.1.
"Matrix times vector" is nothing more than shorthand for "do the weighted sum
for every neuron".

### 2.4 The whole network

Feed the output of one layer into the next. For our network:

```
h      = tanh( W1 * x + b1 )       hidden layer:  W1 is 16 x 22, b1 has 16
output = tanh( W2 * h + b2 )       output layer:  W2 is  2 x 16, b2 has  2
```

Running this from input to output is called the **forward pass**.

**Counting the parameters:**

```
W1: 16 * 22 = 352      b1: 16
W2:  2 * 16 =  32      b2:  2
                             total = 402 numbers
```

Those 402 numbers *are* the ant's brain. Everything it knows is in them.

---

## Part 3 - The math of learning (training)

### 3.1 Measuring how wrong the network is: the loss

To improve, we need a single number that says how bad the current parameters
are. That number is the **loss**. Lower is better; zero is perfect.

Suppose that for some input we know the correct output, the **target** `y`,
and the network produced `a`. The usual measure is the **squared error**:

```
L = 1/2 * [ (a1 - y1)^2 + (a2 - y2)^2 + ... ]
```

- Squaring makes every error positive, and punishes big mistakes much more
  than small ones.
- The `1/2` is only there to make the derivative tidy later.

### 3.2 Derivatives in one paragraph

A **derivative** answers: "if I nudge this number up a tiny bit, how much does
the result change?" It is a slope.

- Written `dL/dw`: "how much `L` changes per tiny change in `w`".
- Positive: increasing `w` makes the loss go up, so we should decrease `w`.
- Negative: increasing `w` makes the loss go down, so we should increase `w`.

The list of derivatives of the loss with respect to *every* parameter is
called the **gradient**. It points in the direction that makes the loss worse
fastest, so we step the opposite way.

### 3.3 Gradient descent

The learning rule is one line, applied to every weight and bias:

```
w  <-  w - learningRate * dL/dw
```

- `learningRate` (often written `eta`) is a small number such as `0.01` or
  `0.1` that controls the step size.
- Too large and the steps overshoot and the loss bounces around or explodes.
  Too small and learning takes forever.

Picture standing on a foggy hillside wanting to reach the valley. You cannot
see the valley, but you can feel which way the ground slopes under your feet.
Take a small step downhill, feel again, repeat. That is gradient descent.

### 3.4 The chain rule

The loss depends on a weight only through a chain of steps:

```
w  ->  z  ->  a  ->  L
```

The **chain rule** says: to find how `L` reacts to `w`, multiply how each link
reacts to the one before it:

```
dL/dw = dL/da * da/dz * dz/dw
```

Each of the three pieces is simple:

```
dL/da = a - y           from L = 1/2 (a - y)^2
da/dz = f'(z)           the slope of the activation function
dz/dw = x               from z = w*x + b    (and dz/db = 1)
```

The slopes of the activation functions have neat formulas:

| Activation | Slope `f'(z)` |
| --- | --- |
| `tanh` | `1 - a^2` where `a = tanh(z)` |
| sigmoid | `a * (1 - a)` where `a = sigmoid(z)` |
| ReLU | `1` if `z > 0`, otherwise `0` |

For `tanh`, notice the slope is computed from the output `a` we already have,
so it costs almost nothing.

### 3.5 A full worked example: one neuron, one learning step

One `tanh` neuron with two inputs. Everything is small enough to check with a
calculator.

```
inputs:   x = (1.0, 0.5)
weights:  w = (0.4, -0.2)
bias:     b = 0.1
target:   y = 1.0
learning rate = 0.1
```

**Forward pass**

```
z = 0.4*1.0 + (-0.2)*0.5 + 0.1 = 0.4
a = tanh(0.4)                  = 0.3799
L = 1/2 * (0.3799 - 1.0)^2     = 0.1922
```

**Backward pass** (the chain rule)

```
dL/da = a - y        = 0.3799 - 1.0       = -0.6201
da/dz = 1 - a^2      = 1 - 0.1444         =  0.8556
delta = dL/da * da/dz = -0.6201 * 0.8556  = -0.5305     <- "the neuron's error"

dL/dw1 = delta * x1 = -0.5305 * 1.0 = -0.5305
dL/dw2 = delta * x2 = -0.5305 * 0.5 = -0.2653
dL/db  = delta * 1                  = -0.5305
```

**Update** (`new = old - 0.1 * gradient`)

```
w1 =  0.4 - 0.1 * (-0.5305) =  0.4531
w2 = -0.2 - 0.1 * (-0.2653) = -0.1735
b  =  0.1 - 0.1 * (-0.5305) =  0.1531
```

**Check: did it improve?** Run the forward pass again with the new values:

```
z = 0.4531*1.0 + (-0.1735)*0.5 + 0.1531 = 0.5194
a = tanh(0.5194)                        = 0.477     (was 0.380, target 1.0)
L = 1/2 * (0.477 - 1.0)^2               = 0.137     (was 0.192)
```

The output moved toward the target and the loss fell. Repeat this thousands of
times over many examples and the network learns.

### 3.6 Backpropagation: the same thing for a whole network

With several layers, the weights in early layers affect the loss through all
the layers after them. **Backpropagation** is the chain rule organised so that
nothing is computed twice: work out the error at the output, then pass it
*backwards* one layer at a time.

Define each neuron's **delta** as "how much the loss changes when this
neuron's `z` changes" (the same `delta` as in the worked example).

Numbering the layers `1, 2, ..., last`, and writing `a[0]` for the network's
input:

```
(1) Output layer error:
        delta[last] = (a[last] - y)  .*  f'(z[last])

(2) Pass the error back one layer:
        delta[l] = ( transpose(W[l+1]) * delta[l+1] )  .*  f'(z[l])

(3) Gradient for the weights of layer l:
        dL/dW[l] = delta[l] * transpose(a[l-1])

(4) Gradient for the biases of layer l:
        dL/db[l] = delta[l]
```

Reading the symbols:

- `.*` means multiply element by element (first with first, second with
  second, and so on).
- `transpose(W)` flips a matrix so rows become columns. In step (2) it sends
  each neuron's error back along the same connections the signal came forward
  on: a neuron that contributed a lot to a wrong answer receives a lot of the
  blame.
- In step (3), a column times a row gives a full grid: the entry for the
  weight from input `j` to neuron `i` is `delta_i * a_j`. That is exactly
  `delta * x` from the worked example, for every weight at once.

One round of training on one example is therefore:

```
1. forward pass     compute and remember z and a for every layer
2. backward pass    compute delta for every layer, last to first
3. update           W[l] -= learningRate * dL/dW[l]
                    b[l] -= learningRate * dL/db[l]
```

### 3.7 Starting values

Before training, the weights need starting values.

- All zeros does not work: every neuron in a layer would compute the same
  thing, receive the same update, and stay identical forever.
- So we start with small random numbers. A reliable recipe (**Xavier
  initialisation**) picks each weight uniformly from `-r..+r` with

  ```
  r = sqrt( 6 / (numberOfInputs + numberOfOutputs) )    for that layer
  ```

  which keeps signals from growing or shrinking as they pass through layers.
- Biases start at zero.

### 3.8 How we will know our math is right: gradient checking

Backpropagation is easy to get subtly wrong. Fortunately there is a slow but
foolproof way to compute a derivative straight from its definition: nudge one
weight up and down by a tiny amount `eps` and see how the loss changes.

```
dL/dw  ~=  ( L(w + eps) - L(w - eps) ) / (2 * eps)        eps ~ 0.0001
```

Our unit tests will compare this against backpropagation for every weight.
If they agree to several decimal places, the implementation is correct.

---

## Part 4 - From math to a working ant

### 4.1 Where the "right answers" come from

Backpropagation needs a **target**: the right answer for each input. In a
game nobody hands us the right answer for every moment. There are three
standard ways around this:

| Method | Where the targets come from | Uses backpropagation? |
| --- | --- | --- |
| **Imitation learning** | Copy a teacher: you at the keyboard, or the built-in instinct. | Yes |
| **Neuroevolution** | No targets at all. Try many random brains, keep and mutate the best. | No |
| **Reinforcement learning** | The ant's own experience, scored by rewards. | Yes |

How each one works, what comes out at the end, and how to steer an ant toward
a particular behaviour is the subject of [TRAINING.md](TRAINING.md).

### 4.2 The build plan

Each stage is small, tested, and ends with something you can see working.

| Stage | Status | What we build | How we know it works |
| --- | --- | --- | --- |
| 1 | **built** | `Matrix`: a grid of numbers, and matrix times vector | unit tests against hand-computed values |
| 2 | **built** | `Network`: layers, forward pass | reproduces the worked example in 3.5 |
| 3 | next | Backpropagation and gradient descent (adds transpose to `Matrix`) | gradient checking (3.8); learns XOR |
| 4 | **built** | Senses: build the 22-number observation from a `World` | unit tests on hand-made levels |
| 5 | **built** | Brain files: save a network to disk and load it back | a saved-then-loaded network gives identical outputs |
| 6 | planned | Imitation: record a teacher, train, watch the ant copy it | loss falls; the ant delivers food on its own |
| 7 | planned | Neuroevolution: population, fitness, mutation | best fitness rises generation after generation |
| 8 | later | Reinforcement learning | to be planned |

**XOR** (stage 3) is the classic first test for a network. It is the rule
"output 1 if exactly one of two inputs is 1":

```
inputs   target
 0  0      0
 0  1      1
 1  0      1
 1  1      0
```

No single neuron can learn it, because no single straight line separates the
1s from the 0s. A network with a hidden layer can. If ours learns XOR, the
forward pass, backpropagation and gradient descent all work.

### 4.3 Where the code lives

```
src/
  nn/                 library "ant_nn" - depends on nothing            (built)
    matrix.h / .cpp     Matrix: the grid-of-numbers type
    network.h / .cpp    Network: layers, run(), forward(), save/load
                        (backward() and update will be added here)
  agent/              library "ant_agent" - connects a network to an ant (built)
    senses.h / .cpp     observe(): World + ant index -> 22-number observation
    brain.h / .cpp      brainAction(): observation -> Network -> AntAction
  render/network_view   the live picture of the network                 (built)
  train/              the training programs                           (planned)
    imitation...        record a teacher and train by backpropagation
    evolution...        population, fitness, mutation
```

And how it plugs into the simulation:

```
            +--------+  observation   +---------+  AntAction   +-------+
World ----> | senses | -------------> | Network | -----------> | World |
            +--------+  (22 numbers)  +---------+  (2 numbers) | .step |
                                                               +-------+
```

`World::step` accepts one `AntAction` per ant, so the simulation
itself did not have to change at all. `brainAction` is a drop-in replacement for
`instinctAction`. See [ARCHITECTURE.md](ARCHITECTURE.md).


### 4.4 Watching the network

Run the game with a random network driving your ant:

```
ant_game --brain random
```

A panel shows the network live (key `N` shows or hides it):

```
 inputs (22)              hidden (16)            outputs (2)
 what the ant senses                             what it decides

 feel ahead     O ---\
 food scent     O ----\---> O ---\
 nest: ahead    O -----\--> O ----+--> O  walk  +0.27
 carrying       O ------/-> O ---/+--> O  turn  +0.01
 ...                        ...
```

- Each **circle** is a neuron. Its colour is its value right now: orange is
  positive, blue is negative, dark is near zero. This is the `a` of Part 2.
- Each **line** is a connection, drawn only when it is carrying something.
  Its colour and brightness show `weight * value of the neuron it leaves`,
  which is one term of the weighted sum in section 2.1. Orange pushes the
  next neuron up, blue pushes it down.
- The left column is labelled with the senses of section 1.3; the right
  column shows the two outputs as numbers.

Things to try:

- Walk toward a plate and watch the "feel" inputs light up one by one.
- Stand on a scent trail and turn on the spot; the three scent inputs take
  turns being brightest.
- Press `B` to hand control to the network, and again to take it back. While
  you drive, the panel shows what the network *would* do.
- Press `G` for a different set of random numbers. The wiring changes
  colour, and the ant behaves differently: the numbers are the behaviour.
- `ant_game --brain random --save-brain my.brain` writes the network to a
  text file you can open; `ant_game --brain my.brain` loads it back.

When training exists, this same panel will show a brain that has learned
something, and the patterns will mean something.

---

## Glossary

| Term | Meaning |
| --- | --- |
| **Activation** | A neuron's output, `a = f(z)`. |
| **Activation function** | The fixed curve `f` applied to a neuron's weighted sum (we use `tanh`). |
| **Backpropagation** | The method for computing the gradient of every parameter by passing errors backwards through the network. |
| **Bias** | A neuron's adjustable offset, added to its weighted sum. |
| **Chain rule** | To find how a result reacts to an early step, multiply the reactions of each step in between. |
| **Delta** | A neuron's error: how much the loss changes when that neuron's `z` changes. |
| **Derivative** | The slope: how much an output changes per tiny change in an input. |
| **Fitness** | A score for how well one brain played a whole game (used in neuroevolution). |
| **Forward pass** | Running the network from inputs to outputs. |
| **Generation** | One round of play-score-select-mutate in neuroevolution. |
| **Gradient** | The collection of derivatives of the loss with respect to every parameter. |
| **Gradient descent** | Repeatedly nudging every parameter a small step against its gradient. |
| **Hidden layer** | A layer between the inputs and the outputs. |
| **Layer** | A group of neurons that all read the same inputs. |
| **Learning rate** | The step size used in gradient descent. |
| **Loss** | One number measuring how wrong the network is. Lower is better. |
| **Matrix** | A rectangular grid of numbers. |
| **Mutation** | A small random change to a parameter (used in neuroevolution). |
| **Neuron** | A unit computing `a = f(w1*x1 + ... + wn*xn + b)`. |
| **Instinct** | The hand-written rules that drive the worker ants today. |
| **Observation** | The list of numbers describing what the ant currently senses. |
| **Parameters** | All the weights and biases: the numbers that learning adjusts. |
| **Pheromone** | A scent mark ants leave on the ground to guide each other. |
| **Target** | The correct output for a training example. |
| **Transpose** | A matrix flipped so its rows become its columns. |
| **Vector** | A list of numbers (a matrix with one column). |
| **Weight** | How strongly one input influences one neuron. |
