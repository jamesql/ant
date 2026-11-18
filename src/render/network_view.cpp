/// @file network_view.cpp
/// @brief Drawing of the live network diagram.

#include "render/network_view.h"

#include <algorithm>
#include <cmath>

#include "raylib.h"

namespace ant {
namespace {

constexpr float kPanelWidth = 500.0f;
constexpr float kPanelTop = 200.0f;      ///< Leaves room for the mini-map above.
constexpr float kMargin = 16.0f;         ///< Gap between the panel and the screen edge.
constexpr float kHeaderHeight = 62.0f;   ///< Space for the title and status line.
constexpr float kFooterHeight = 26.0f;   ///< Space for the colour key.
constexpr float kLabelWidth = 150.0f;    ///< Space for the input names on the left.
constexpr float kOutputWidth = 120.0f;   ///< Space for the output names and values on the right.
constexpr float kMaxNodeSpacing = 56.0f; ///< Neurons in a short column are not spread further apart than this.

constexpr Color kPanel{18, 24, 34, 215};
constexpr Color kPanelEdge{255, 255, 255, 46};
constexpr Color kText{244, 244, 240, 255};
constexpr Color kTextDim{170, 180, 196, 255};
constexpr Color kPositive{255, 170, 50, 255};  ///< Orange: values and signals above zero.
constexpr Color kNegative{80, 165, 255, 255};  ///< Blue: values and signals below zero.
constexpr Color kNeutral{40, 46, 58, 255};     ///< A neuron at zero.

/// Blends from `kNeutral` toward orange or blue as `value` moves from 0 toward +1 or -1.
Color valueColor(float value) {
    const float strength = std::clamp(std::fabs(value), 0.0f, 1.0f);
    const Color target = value >= 0.0f ? kPositive : kNegative;
    const auto channel = [strength](unsigned char from, unsigned char to) {
        return static_cast<unsigned char>(static_cast<float>(from) + (static_cast<float>(to) - from) * strength);
    };
    return {channel(kNeutral.r, target.r), channel(kNeutral.g, target.g), channel(kNeutral.b, target.b), 255};
}

/// Where the neurons of one column sit on screen.
struct Column {
    float x;
    float firstY;
    float spacing;
    float radius;

    Vector2 node(std::size_t index) const { return {x, firstY + spacing * static_cast<float>(index)}; }
};

/// Spreads `count` neurons evenly down a column centred in the given vertical span.
Column layoutColumn(float x, float top, float bottom, std::size_t count) {
    const float spacing = std::min((bottom - top) / static_cast<float>(count), kMaxNodeSpacing);
    const float used = spacing * static_cast<float>(count - 1);
    return {x, (top + bottom) / 2.0f - used / 2.0f, spacing, std::clamp(spacing * 0.38f, 4.0f, 10.0f)};
}

}  // namespace

void drawNetworkView(const Network& network, const Activations& activations,
                     const std::vector<const char*>& inputLabels, const std::vector<const char*>& outputLabels,
                     const char* status) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    const Rectangle panel{screenWidth - kMargin - kPanelWidth, kPanelTop, kPanelWidth,
                          screenHeight - kPanelTop - kMargin};

    DrawRectangleRounded(panel, 0.04f, 8, kPanel);
    DrawRectangleRoundedLinesEx(panel, 0.04f, 8, 2.0f, kPanelEdge);
    DrawText(TextFormat("NEURAL NETWORK  \"%s\"", network.name.c_str()), static_cast<int>(panel.x) + 16,
             static_cast<int>(panel.y) + 12, 20, kText);
    DrawText(status, static_cast<int>(panel.x) + 16, static_cast<int>(panel.y) + 38, 10, kTextDim);

    // One column of neurons per layer, spread from the input side to the output side.
    const float top = panel.y + kHeaderHeight;
    const float bottom = panel.y + panel.height - kFooterHeight;
    const float firstX = panel.x + kLabelWidth;
    const float lastX = panel.x + panel.width - kOutputWidth;
    std::vector<Column> columns;
    for (std::size_t layer = 0; layer < activations.size(); ++layer) {
        const float along = static_cast<float>(layer) / static_cast<float>(activations.size() - 1);
        columns.push_back(layoutColumn(firstX + (lastX - firstX) * along, top, bottom, activations[layer].size()));
    }

    // Connections first, so the neurons are drawn on top of them.
    for (std::size_t layer = 0; layer < network.layers().size(); ++layer) {
        const Matrix& weights = network.layers()[layer].weights;
        for (int to = 0; to < weights.rows(); ++to) {
            for (int from = 0; from < weights.columns(); ++from) {
                // The signal on this connection: its weight times the value entering it.
                const float signal = weights.at(to, from) * activations[layer][static_cast<std::size_t>(from)];
                const float strength = std::min(1.0f, std::fabs(signal) * 1.6f);
                if (strength < 0.04f) {
                    continue;  // carrying next to nothing: leave it out to keep the picture readable
                }
                DrawLineEx(columns[layer].node(static_cast<std::size_t>(from)),
                           columns[layer + 1].node(static_cast<std::size_t>(to)), 1.0f + 1.5f * strength,
                           Fade(signal >= 0.0f ? kPositive : kNegative, 0.85f * strength));
            }
        }
    }

    for (std::size_t layer = 0; layer < activations.size(); ++layer) {
        for (std::size_t i = 0; i < activations[layer].size(); ++i) {
            const Vector2 center = columns[layer].node(i);
            DrawCircleV(center, columns[layer].radius + 1.5f, Fade(WHITE, 0.55f));
            DrawCircleV(center, columns[layer].radius, valueColor(activations[layer][i]));
        }
    }

    // Input names, right-aligned against the first column.
    const Column& inputs = columns.front();
    for (std::size_t i = 0; i < activations.front().size() && i < inputLabels.size(); ++i) {
        const Vector2 center = inputs.node(i);
        const int width = MeasureText(inputLabels[i], 10);
        DrawText(inputLabels[i], static_cast<int>(center.x - inputs.radius - 8.0f) - width,
                 static_cast<int>(center.y) - 5, 10, kTextDim);
    }

    // Output names with their current values, to the right of the last column.
    const Column& outputs = columns.back();
    for (std::size_t i = 0; i < activations.back().size() && i < outputLabels.size(); ++i) {
        const Vector2 center = outputs.node(i);
        const int x = static_cast<int>(center.x + outputs.radius + 10.0f);
        const float value = activations.back()[i];
        const Color valueTint = std::fabs(value) < 0.05f ? kText : (value > 0.0f ? kPositive : kNegative);
        DrawText(outputLabels[i], x, static_cast<int>(center.y) - 18, 10, kTextDim);
        DrawText(TextFormat("%+.2f", value), x, static_cast<int>(center.y) - 5, 20, valueTint);
    }

    // Colour key.
    const int keyY = static_cast<int>(panel.y + panel.height - 20.0f);
    int keyX = static_cast<int>(panel.x) + 16;
    DrawCircle(keyX + 4, keyY + 5, 5.0f, kPositive);
    DrawText("positive", keyX + 14, keyY, 10, kTextDim);
    keyX += 74;
    DrawCircle(keyX + 4, keyY + 5, 5.0f, kNegative);
    DrawText("negative", keyX + 14, keyY, 10, kTextDim);
    DrawText(TextFormat("%d adjustable numbers", network.parameterCount()), keyX + 90, keyY, 10, kTextDim);
}

}  // namespace ant
