/// @file test_pheromones.cpp
/// @brief Tests for the scent grid.

#include "core/pheromones.h"
#include "test_framework.h"

using namespace ant;

namespace {

/// A small field with the default scent behaviour.
PheromoneField smallField() { return PheromoneField({5.0f, 5.0f}); }

}  // namespace

TEST(pheromones_start_empty) {
    const PheromoneField field = smallField();
    CHECK(field.sample(Scent::Home, {0.0f, 0.0f}) == 0.0f);
    CHECK(field.columns() == 20 && field.rows() == 20);
}

TEST(pheromones_deposit_is_read_back_at_the_same_spot) {
    PheromoneField field = smallField();
    field.deposit(Scent::Food, {1.0f, 2.0f}, 0.4f);
    CHECK_NEAR(field.sample(Scent::Food, {1.0f, 2.0f}), 0.4f, 1e-6f);
    CHECK(field.sample(Scent::Food, {-3.0f, -3.0f}) == 0.0f);
}

TEST(pheromones_scents_are_independent) {
    PheromoneField field = smallField();
    field.deposit(Scent::Food, {1.0f, 2.0f}, 0.4f);
    CHECK(field.sample(Scent::Home, {1.0f, 2.0f}) == 0.0f);
}

TEST(pheromones_cell_never_exceeds_one) {
    PheromoneField field = smallField();
    field.deposit(Scent::Home, {0.1f, 0.1f}, 0.8f);
    field.deposit(Scent::Home, {0.1f, 0.1f}, 0.8f);
    CHECK(field.sample(Scent::Home, {0.1f, 0.1f}) == 1.0f);
}

TEST(pheromones_outside_the_area_are_ignored) {
    PheromoneField field = smallField();
    field.deposit(Scent::Home, {99.0f, 0.0f}, 1.0f);
    CHECK(field.sample(Scent::Home, {99.0f, 0.0f}) == 0.0f);
}

TEST(pheromones_evaporate_over_time) {
    // No diffusion, so only evaporation acts: after one lifetime about 37% (1/e) is left.
    PheromoneField field({5.0f, 5.0f}, {.cellSize = 0.5f, .lifetime = 10.0f, .diffusion = 0.0f});
    field.deposit(Scent::Home, {0.1f, 0.1f}, 1.0f);
    for (int i = 0; i < 100; ++i) {
        field.update(0.1f);
    }
    CHECK_NEAR(field.sample(Scent::Home, {0.1f, 0.1f}), 0.3679f, 1e-3f);
}

TEST(pheromones_diffuse_into_neighbouring_cells) {
    PheromoneField field = smallField();
    field.deposit(Scent::Home, {0.1f, 0.1f}, 1.0f);
    field.update(0.1f);
    CHECK(field.sample(Scent::Home, {0.6f, 0.1f}) > 0.0f);   // the next cell over caught some
    CHECK(field.sample(Scent::Home, {0.1f, 0.1f}) < 1.0f);   // and the source lost some
}

TEST(pheromones_do_not_enter_or_cross_solid_cells) {
    // A solid column of cells at x = 0 splits the field in two.
    PheromoneField field = smallField();
    for (int row = 0; row < field.rows(); ++row) {
        field.setSolid(10, row);
    }
    field.deposit(Scent::Food, {0.25f, 0.0f}, 1.0f);  // straight into the barrier: ignored
    field.deposit(Scent::Food, {-0.25f, 0.0f}, 1.0f);  // just left of it
    for (int i = 0; i < 600; ++i) {
        field.update(0.1f);
    }
    CHECK(field.sample(Scent::Food, {0.25f, 0.0f}) == 0.0f);
    CHECK(field.sample(Scent::Food, {0.75f, 0.0f}) == 0.0f);   // nothing leaked to the right side
    CHECK(field.sample(Scent::Food, {-0.75f, 0.0f}) > 0.0f);   // but it did spread on the left
}
