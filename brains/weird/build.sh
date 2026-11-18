#!/bin/bash
# Builds every brain in this folder from brains/forager.brain, then measures them.
#
# Each brain is the forager with a few of its numbers changed by hand. Nothing
# here is trained. Run from the project folder:
#
#     bash brains/weird/build.sh [path to ant_train]
#
# Senses are numbered from 0, in the order of docs/NEURAL_NETWORK.md 1.3:
#   0-4 feelers (far left, left, ahead, right, far right)
#   5-7 poison (left, ahead, right)       8-10 food scent (left, ahead, right)
#   11-13 home scent (left, ahead, right) 14-16 food seen (side, ahead, near)
#   17-19 nest (side, ahead, near)        20 carrying        21 health
# Hidden neurons are numbered from 1, as `ant_train inspect` prints them.
# The turn output is positive for "turn right", and "side" senses are positive
# when the thing is to the ant's right.

TRAIN=${1:-$HOME/ant-build/ant_train}
FORAGER=brains/forager.brain
OUT=brains/weird
TMP=$(mktemp -d)

# Shorthand: change a brain without stopping to score it.
graft()   { "$TRAIN" graft   --score 0 "$@" > /dev/null; }
silence() { "$TRAIN" silence --score 0 "$@" > /dev/null; }

# --- amnesiac: forgets the way home -----------------------------------------
# Hidden neuron h10 is the forager's homing neuron (see docs/TRAINING.md 5.5).
# Fix it at its average value, so it no longer reacts to anything.
silence --brain $FORAGER --neuron 10 --out $OUT/amnesiac.brain

# --- spinner: turns in circles ------------------------------------------------
# One new neuron that is always on (bias +3, so tanh gives +1) pushing "turn" hard.
graft --brain $FORAGER --bias 3 --turn 8 --out $OUT/spinner.brain

# --- moonwalker: walks backwards ----------------------------------------------
# One new neuron that is always on, pushing "walk" hard into reverse.
graft --brain $FORAGER --bias 3 --walk -12 --out $OUT/moonwalker.brain

# --- homebody: never leaves home ------------------------------------------------
# One new neuron that reads "nest: side" and turns the ant toward the nest,
# strongly enough to beat everything else.
graft --brain $FORAGER --inputs 17=3 --turn 8 --out $OUT/homebody.brain

# --- escapist: tries to leave the world -----------------------------------------
# 1. Silence h10, so it never turns for home.
# 2. Cut all five feelers (fix them at 0 = "nothing there"), so it does not
#    turn away from the edge of the map.
# 3. Add a neuron that reads "nest: side" and turns the ant AWAY from the nest.
silence --brain $FORAGER --neuron 10 --out $TMP/e0.brain
cp $TMP/e0.brain $TMP/e.brain
for feeler in 0 1 2 3 4; do
    silence --brain $TMP/e.brain --sense $feeler --value 0 --out $TMP/e.brain
done
graft --brain $TMP/e.brain --inputs 17=3 --turn -8 --out $OUT/escapist.brain

# --- poison-lover: walks into poison ----------------------------------------------
# 1. Cut the three poison senses out of the trained network (fix them at 0),
#    which removes everything it learned about avoiding poison.
# 2. Add a neuron that reads the same senses the other way round: poison on
#    the right turns it right, poison on the left turns it left.
cp $FORAGER $TMP/p.brain
for sense in 5 6 7; do
    silence --brain $TMP/p.brain --sense $sense --value 0 --out $TMP/p.brain
done
cp $TMP/p.brain $TMP/numb.brain
graft --brain $TMP/numb.brain --inputs 7=4,5=-4 --turn 8 --out $OUT/poison-lover.brain

# --- poison-courier: soaks in poison, then carries it home ---------------------------
# Starts from the same "numb" brain as the poison-lover, then adds two pairs
# of neurons. Each pair is built so the two cancel out exactly when switched
# off, and push the same way when switched on.
#
# Pair A (on while healthy): steer toward poison.
#   The "8 * health - 8" part is 0 at full health and very negative when hurt,
#   which holds both neurons at -1 so they cancel.
graft --brain $TMP/numb.brain --inputs 7=4,5=-4,21=8 --bias -8 --turn 4  --out $TMP/c.brain
graft --brain $TMP/c.brain    --inputs 5=4,7=-4,21=8 --bias -8 --turn -4 --out $TMP/c.brain
# Pair B (on while hurt): steer toward the nest.
#   "-60 * health + 51" is -9 at full health, which holds both neurons at -1
#   so they cancel (off), and turns positive once health drops below 0.85 (on).
graft --brain $TMP/c.brain --inputs 17=3,21=-60  --bias 51 --turn 4  --out $TMP/c.brain
graft --brain $TMP/c.brain --inputs 17=-3,21=-60 --bias 51 --turn -4 --out $OUT/poison-courier.brain

rm -rf "$TMP"

# --- measure everything -----------------------------------------------------------
"$TRAIN" behaviour --brain $FORAGER
for name in amnesiac spinner moonwalker homebody escapist poison-lover poison-courier; do
    "$TRAIN" behaviour --brain $OUT/$name.brain --header 0
done
