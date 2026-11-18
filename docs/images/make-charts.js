// Draws the charts used in the README as SVG files.
//
//     node docs/images/make-charts.js
//
// Every number below was measured with ant_train; see docs/TRAINING.md for
// the commands. The charts are plain SVG so GitHub can show them directly.

const fs = require("fs");
const path = require("path");

// Colours: one validated set, used the same way in every chart.
const SURFACE = "#fcfcfb";
const BORDER = "#e1e0d9";
const GRID = "#e1e0d9";
const AXIS = "#c3c2b7";
const INK = "#0b0b0b";
const INK_2 = "#52514e";
const MUTED = "#898781";
const BLUE = "#2a78d6";   // a trained network / "while searching"
const ORANGE = "#eb6834"; // "while carrying"
const AQUA = "#1baf7a";   // the hand-written instinct
const FONT = "font-family=\"-apple-system, 'Segoe UI', Helvetica, Arial, sans-serif\"";

const text = (x, y, content, { size = 13, fill = INK, anchor = "start", weight = 400 } = {}) =>
    `<text x="${x}" y="${y}" font-size="${size}" fill="${fill}" text-anchor="${anchor}" font-weight="${weight}" ${FONT}>${content}</text>`;

// A bar whose data end is rounded and whose baseline end is square.
function bar(x, y, width, height, fill, towardRight = true) {
    const r = Math.min(4, Math.abs(width), height / 2);
    if (width <= 0) return "";
    const d = towardRight
        ? `M${x},${y} h${width - r} a${r},${r} 0 0 1 ${r},${r} v${height - 2 * r} a${r},${r} 0 0 1 ${-r},${r} h${-(width - r)} z`
        : `M${x + width},${y} h${-(width - r)} a${r},${r} 0 0 0 ${-r},${r} v${height - 2 * r} a${r},${r} 0 0 0 ${r},${r} h${width - r} z`;
    return `<path d="${d}" fill="${fill}"/>`;
}

// An upright bar with a rounded top.
function column(x, y, width, height, fill) {
    const r = Math.min(4, width / 2, height);
    if (height <= 0) return "";
    return `<path d="M${x},${y + height} v${-(height - r)} a${r},${r} 0 0 1 ${r},${-r} h${width - 2 * r} a${r},${r} 0 0 1 ${r},${r} v${height - r} z" fill="${fill}"/>`;
}

function frame(width, height, title, subtitle, body) {
    return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${width} ${height}" width="${width}" height="${height}" role="img" aria-label="${title}">
<rect x="0.5" y="0.5" width="${width - 1}" height="${height - 1}" rx="10" fill="${SURFACE}" stroke="${BORDER}"/>
${text(24, 34, title, { size: 17, weight: 600 })}
${text(24, 56, subtitle, { size: 13, fill: INK_2 })}
${body}
</svg>
`;
}

const legendItem = (x, y, color, label) =>
    `<rect x="${x}" y="${y - 10}" width="12" height="12" rx="3" fill="${color}"/>` + text(x + 18, y, label, { size: 12.5, fill: INK_2 });

// --- 1. How far training got ------------------------------------------------
function progressChart() {
    const rows = [
        ["Random weights", 0.0, BLUE, "untrained"],
        ["copycat", 5.79, BLUE, "imitating the instinct"],
        ["forager v1", 8.83, BLUE, "evolved on one fixed map"],
        ["forager", 12.04, BLUE, "evolved on random maps"],
        ["careful", 12.35, BLUE, "evolved with less luck in scoring"],
        ["The instinct", 12.28, AQUA, "hand-written rules"],
    ];
    const width = 820, left = 150, right = 250, top = 92, rowHeight = 38;
    const height = top + rows.length * rowHeight + 46;
    const x = v => left + (v / 14) * (width - left - right);
    let body = legendItem(24, 80, BLUE, "neural network") + legendItem(160, 80, AQUA, "hand-written rules");
    for (let tick = 0; tick <= 14; tick += 2) {
        body += `<line x1="${x(tick)}" x2="${x(tick)}" y1="${top}" y2="${height - 40}" stroke="${GRID}"/>`;
        body += text(x(tick), height - 22, tick, { size: 12, fill: MUTED, anchor: "middle" });
    }
    body += text(x(7), height - 6, "food delivered per game (of 14)", { size: 12, fill: MUTED, anchor: "middle" });
    rows.forEach(([name, value, color, how], i) => {
        const y = top + i * rowHeight + 8;
        body += text(left - 12, y + 15, name, { anchor: "end", weight: 500 });
        body += bar(x(0), y, Math.max(x(value) - x(0), 0), 20, color);
        body += text(x(value) + 8, y + 15, value.toFixed(2), { weight: 600 });
        body += text(x(value) + 52, y + 15, how, { size: 12, fill: INK_2 });
    });
    body += `<line x1="${x(0)}" x2="${x(0)}" y1="${top}" y2="${height - 40}" stroke="${AXIS}"/>`;
    return frame(width, height, "From nothing to as good as the hand-written rules",
        "Average over 400 random maps that no brain was trained on. Every ant in the colony uses the same brain.", body);
}

// --- 2. Where games are lost ------------------------------------------------
function lossesChart() {
    const counts = [0, 0, 0, 0, 1, 1, 50, 19, 6, 15, 11, 8, 10, 32, 247];
    const width = 820, height = 330, left = 60, right = 24, top = 84, bottom = 56;
    const band = (width - left - right) / counts.length;
    const y = v => top + (1 - v / 260) * (height - top - bottom);
    let body = "";
    for (const tick of [0, 50, 100, 150, 200, 250]) {
        body += `<line x1="${left}" x2="${width - right}" y1="${y(tick)}" y2="${y(tick)}" stroke="${GRID}"/>`;
        body += text(left - 8, y(tick) + 4, tick, { size: 12, fill: MUTED, anchor: "end" });
    }
    counts.forEach((count, pieces) => {
        const bx = left + pieces * band;
        body += column(bx + 5, y(count), band - 10, y(0) - y(count), BLUE);
        body += text(bx + band / 2, height - bottom + 18, pieces, { size: 12, fill: MUTED, anchor: "middle" });
    });
    body += `<line x1="${left}" x2="${width - right}" y1="${y(0)}" y2="${y(0)}" stroke="${AXIS}"/>`;
    body += text(left + 6.5 * band, y(50) - 26, "50 games stuck on 6:", { size: 12.5, weight: 600, anchor: "middle" });
    body += text(left + 6.5 * band, y(50) - 10, "nothing came out of the basket", { size: 12.5, fill: INK_2, anchor: "middle" });
    body += text(left + 14.5 * band, y(247) - 8, "247 wins", { size: 12.5, weight: 600, anchor: "middle" });
    body += text(left + (width - left - right) / 2, height - 12, "pieces of food delivered in a game", { size: 12, fill: MUTED, anchor: "middle" });
    return frame(width, height, "Where the games are lost",
        "The forager brain on 400 maps. 6 pieces lie in the open and 8 are inside the basket.", body);
}

// --- 3. What each neuron is for ---------------------------------------------
function neuronsChart() {
    // Food per game lost when the neuron is silenced (whole brain: about 12).
    const loss = [10.4, 8.2, 5.8, 0.5, 1.6, 2.3, 4.8, 0.0, 2.9, 11.3, 8.3, 4.0, 2.9, 5.2, 2.5, 3.7];
    const notes = { 1: "steering", 2: "trail", 5: "avoid", 10: "homing", 11: "trail", 15: "trail" };
    const width = 820, height = 340, left = 60, right = 24, top = 84, bottom = 62;
    const band = (width - left - right) / loss.length;
    const y = v => top + (1 - v / 12) * (height - top - bottom);
    let body = "";
    for (const tick of [0, 3, 6, 9, 12]) {
        body += `<line x1="${left}" x2="${width - right}" y1="${y(tick)}" y2="${y(tick)}" stroke="${GRID}"/>`;
        body += text(left - 8, y(tick) + 4, tick, { size: 12, fill: MUTED, anchor: "end" });
    }
    loss.forEach((value, i) => {
        const bx = left + i * band;
        body += column(bx + 6, y(value), band - 12, y(0) - y(value), BLUE);
        body += text(bx + band / 2, height - bottom + 18, `h${i + 1}`, { size: 12, fill: MUTED, anchor: "middle" });
        if (notes[i + 1]) {
            body += text(bx + band / 2, height - bottom + 34, notes[i + 1], { size: 11.5, fill: INK_2, anchor: "middle", weight: 600 });
            body += text(bx + band / 2, y(value) - 6, value.toFixed(1), { size: 12, weight: 600, anchor: "middle" });
        }
    });
    body += `<line x1="${left}" x2="${width - right}" y1="${y(0)}" y2="${y(0)}" stroke="${AXIS}"/>`;
    body += text(left + (width - left - right) / 2, height - 8, "hidden neuron (labels are our reading of what it does)", { size: 12, fill: MUTED, anchor: "middle" });
    return frame(width, height, "What the colony loses when one neuron is silenced",
        "Food per game lost, out of about 12. Two neurons (h4, h8) can be removed with no measurable effect.", body);
}

// --- 4. The Jacobian ----------------------------------------------------------
function jacobianChart() {
    // Effect of each sense on the turn output (+ = turn right), searching vs carrying.
    const rows = [
        ["food scent on the left", -6.85, -1.62],
        ["food scent on the right", 6.78, 0.70],
        ["poison on the left", 2.86, 3.52],
        ["poison on the right", -2.09, -2.27],
        ["food in sight, to the right", 2.77, 0.70],
        ["obstacle far left", 1.84, 0.92],
        ["obstacle on the right", -1.12, -1.06],
        ["nest to the right", 0.38, 1.31],
    ];
    const width = 820, left = 210, right = 40, top = 100, rowHeight = 40;
    const height = top + rows.length * rowHeight + 50;
    const x = v => left + ((v + 8) / 16) * (width - left - right);
    let body = legendItem(24, 82, BLUE, "while searching") + legendItem(170, 82, ORANGE, "while carrying food");
    for (let tick = -8; tick <= 8; tick += 2) {
        body += `<line x1="${x(tick)}" x2="${x(tick)}" y1="${top}" y2="${height - 44}" stroke="${GRID}"/>`;
        body += text(x(tick), height - 26, (tick > 0 ? "+" : "") + tick, { size: 12, fill: MUTED, anchor: "middle" });
    }
    body += text(x(-4), height - 8, "turns LEFT", { size: 12, fill: MUTED, anchor: "middle" });
    body += text(x(4), height - 8, "turns RIGHT", { size: 12, fill: MUTED, anchor: "middle" });
    rows.forEach(([name, searching, carrying], i) => {
        const y = top + i * rowHeight + 6;
        body += text(left - 12, y + 18, name, { anchor: "end", size: 12.5 });
        [[searching, BLUE, 0], [carrying, ORANGE, 15]].forEach(([value, color, dy]) => {
            const w = Math.abs(x(value) - x(0));
            body += value >= 0 ? bar(x(0), y + dy, w, 13, color, true) : bar(x(0) - w, y + dy, w, 13, color, false);
        });
    });
    body += `<line x1="${x(0)}" x2="${x(0)}" y1="${top}" y2="${height - 44}" stroke="${AXIS}" stroke-width="1.5"/>`;
    return frame(width, height, "The rules the network found on its own",
        "How strongly each sense pushes the ant to turn (the network's Jacobian), averaged over 32,000 real moments.", body);
}

const outputs = {
    "chart-progress.svg": progressChart(),
    "chart-losses.svg": lossesChart(),
    "chart-neurons.svg": neuronsChart(),
    "chart-jacobian.svg": jacobianChart(),
};
for (const [name, svg] of Object.entries(outputs)) {
    fs.writeFileSync(path.join(__dirname, name), svg);
    console.log("wrote", name);
}
