/* Генерирует docs/wiring.svg (RU), docs/wiring-en.svg (EN) и docs/components.svg. */
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const F = "'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif";

const C = {
  bg: "#FFFBF4", border: "#EFE3D2", ink: "#221D16", muted: "#6F655A",
  card: "#FFFFFF", cardStroke: "#EADFCB",
  red: "#E4574C", gnd: "#2E2A26", data: "#3E9E6E", v33: "#4B9BD8", out: "#8A63D2",
  board: "#24413A", boardStroke: "#172E28", gold: "#D9B44A", goldStroke: "#B5932F",
  mat: "#1F1D1A", matStroke: "#3A362F", matText: "#F1E9DC", matMuted: "#8F887C",
  psu: "#FDF3E7", psuStroke: "#E8D8C3", accent: "#4FAF7E",
};

const PALETTE = ["#FF2C2C", "#FF9218", "#FFD640", "#34CD54", "#3080FF", "#B05CFF", "#48E0FF", "#FFF0D6"];

const esc = (s) => String(s).replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;");
const rect = (x, y, w, h, r, fill, stroke, sw = 2) =>
  `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${r}" fill="${fill}" stroke="${stroke}" stroke-width="${sw}"/>`;
const text = (x, y, s, o = {}) => {
  const { size = 13, weight = 400, anchor = "start", fill = C.ink, family = F } = o;
  return `<text x="${x}" y="${y}" font-family="${family}" font-size="${size}" font-weight="${weight}" fill="${fill}" text-anchor="${anchor}">${esc(s)}</text>`;
};
const wire = (pts, color, sw = 4.5) =>
  `<polyline points="${pts.map((p) => p.join(",")).join(" ")}" fill="none" stroke="${color}" stroke-width="${sw}" stroke-linejoin="round" stroke-linecap="round"/>`;
const jdot = (x, y, color) => `<circle cx="${x}" cy="${y}" r="5.5" fill="${color}"/>`;
const padOn = (x, y, color, ring) =>
  `<circle cx="${x}" cy="${y}" r="10" fill="${ring}" stroke="${color}" stroke-width="3"/><circle cx="${x}" cy="${y}" r="4" fill="${color}"/>`;

function matrixDots() {
  let out = "";
  const x0 = 996, y0 = 128, cell = 12.5;
  for (let r = 0; r < 16; r++) {
    for (let c = 0; c < 16; c++) {
      let h = (c * 197 + r * 91) & 0xff;
      h ^= h >> 4;
      h = (h * 11) & 0xff;
      const col = PALETTE[(h >> 1) & 7];
      const cx = (x0 + c * cell + cell / 2).toFixed(1);
      const cy = (y0 + r * cell + cell / 2).toFixed(1);
      out += `<circle cx="${cx}" cy="${cy}" r="4.2" fill="${col}" opacity="0.95"/>`;
    }
  }
  return out;
}

function legend(items) {
  let x = 1192;
  let out = "";
  for (let i = items.length - 1; i >= 0; i--) {
    const it = items[i];
    const w = 20 + 6 + it.l.length * 6.4;
    x -= w;
    out += `<line x1="${x}" y1="47" x2="${x + 20}" y2="47" stroke="${it.c}" stroke-width="4" stroke-linecap="round"/>`;
    out += text(x + 26, 51, it.l, { size: 12.5, fill: C.muted });
    x -= 18;
  }
  return out;
}

function wiringDiagram(t) {
  const parts = [];
  parts.push(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1240 800" width="1240" height="800" role="img" aria-label="${esc(t.title)}">`);
  parts.push(`<defs>
  <filter id="soft" x="-20%" y="-20%" width="140%" height="140%">
    <feDropShadow dx="0" dy="3" stdDeviation="6" flood-color="#3A2E1F" flood-opacity="0.08"/>
  </filter>
  <filter id="glowDot" x="-120%" y="-120%" width="340%" height="340%">
    <feDropShadow dx="0" dy="0" stdDeviation="2.2" flood-color="#000" flood-opacity="0.35"/>
  </filter>
</defs>`);
  parts.push(rect(0.5, 0.5, 1239, 799, 0, C.bg, "none", 0));
  parts.push(`<rect x="10" y="10" width="1220" height="780" rx="26" fill="none" stroke="${C.border}" stroke-width="2"/>`);

  // Title
  parts.push(text(48, 50, t.title, { size: 30, weight: 700 }));
  parts.push(text(48, 74, t.subtitle, { size: 15, fill: C.muted }));
  parts.push(legend(t.legend));

  // Wires
  parts.push(wire([[288, 260], [360, 260], [360, 90], [940, 90], [940, 170], [950, 170]], C.red));
  parts.push(wire([[400, 90], [400, 180], [440, 180]], C.red));
  parts.push(wire([[288, 360], [390, 360], [390, 720], [940, 720], [940, 370], [950, 370]], C.gnd));
  parts.push(wire([[440, 280], [390, 280], [390, 360]], C.gnd));
  parts.push(wire([[560, 660], [560, 720]], C.gnd));
  parts.push(wire([[700, 260], [770, 260]], C.data));
  parts.push(wire([[840, 260], [950, 260]], C.data));
  parts.push(wire([[440, 380], [415, 380], [415, 700], [490, 700], [490, 660]], C.v33));
  parts.push(wire([[630, 660], [630, 700], [650, 700], [650, 360], [700, 360]], C.out));

  // PSU
  parts.push(`<g filter="url(#soft)">${rect(48, 200, 240, 220, 16, C.psu, C.psuStroke)}</g>`);
  parts.push(`<path d="M 78 246 L 68 266 h 8 l -3 14 L 86 260 h -8 l 3 -14 Z" fill="#E8A13C"/>`);
  parts.push(text(120, 266, t.psu.name, { size: 19, weight: 650 }));
  parts.push(text(120, 292, t.psu.spec, { size: 14, fill: C.muted }));
  parts.push(text(120, 316, t.psu.watt, { size: 12, fill: C.muted }));
  parts.push(padOn(288, 260, C.red, C.psu));
  parts.push(padOn(288, 360, C.gnd, C.psu));
  parts.push(text(288, 240, t.psu.plus, { size: 12.5, weight: 600, anchor: "middle", fill: C.red }));
  parts.push(text(288, 390, "GND", { size: 12.5, weight: 600, anchor: "middle", fill: C.gnd }));

  // NodeMCU board
  parts.push(`<g filter="url(#soft)">${rect(440, 120, 260, 360, 20, C.board, C.boardStroke)}</g>`);
  parts.push(text(570, 170, "ESP8266", { size: 21, weight: 700, anchor: "middle", fill: "#EAF6EE" }));
  parts.push(text(570, 194, t.board.sub, { size: 13, anchor: "middle", fill: "#9CC7B2" }));
  parts.push(text(570, 320, "Wi-Fi", { size: 34, weight: 700, anchor: "middle", fill: "#2F5248" }));
  parts.push(text(570, 342, "32-bit · 160 MHz", { size: 11.5, anchor: "middle", fill: "#3F6457" }));
  for (const [px, py, label, anchor] of [
    [440, 180, "VIN", "start"], [440, 280, "GND", "start"], [440, 380, "3V3", "start"],
    [700, 260, "D4 · GPIO2", "end"], [700, 360, "D2 · GPIO4", "end"],
  ]) {
    const rx = anchor === "start" ? px - 13 : px - 13;
    parts.push(rect(rx, py - 10, 26, 20, 5, C.gold, C.goldStroke, 1.5));
    const tx = anchor === "start" ? px + 22 : px - 22;
    parts.push(text(tx, py + 4, label, { size: 13, weight: 600, anchor, fill: "#EAF6EE" }));
  }

  // Matrix
  parts.push(`<g filter="url(#soft)">${rect(950, 110, 270, 350, 18, C.mat, C.matStroke)}</g>`);
  parts.push(`<g filter="url(#glowDot)">${matrixDots()}</g>`);
  parts.push(text(1085, 398, "WS2812B 16×16", { size: 16, weight: 650, anchor: "middle", fill: C.matText }));
  parts.push(text(1085, 421, t.matrix.sub, { size: 12, anchor: "middle", fill: C.matMuted }));
  parts.push(padOn(950, 170, C.red, C.mat));
  parts.push(padOn(950, 260, C.data, C.mat));
  parts.push(padOn(950, 370, C.gnd, C.mat));
  parts.push(text(980, 174, "5V", { size: 12.5, weight: 600, fill: C.matText }));
  parts.push(text(980, 264, "DIN", { size: 12.5, weight: 600, fill: C.matText }));
  parts.push(text(980, 374, "GND", { size: 12.5, weight: 600, fill: C.matText }));

  // Touch sensor
  parts.push(`<g filter="url(#soft)">${rect(450, 550, 220, 110, 14, C.psu, C.psuStroke)}</g>`);
  parts.push(padOn(490, 660, C.v33, C.psu));
  parts.push(padOn(560, 660, C.gnd, C.psu));
  parts.push(padOn(630, 660, C.out, C.psu));
  parts.push(text(490, 638, "VCC", { size: 11, weight: 600, anchor: "middle", fill: C.muted }));
  parts.push(text(560, 638, "GND", { size: 11, weight: 600, anchor: "middle", fill: C.muted }));
  parts.push(text(630, 638, "OUT", { size: 11, weight: 600, anchor: "middle", fill: C.muted }));
  parts.push(text(560, 588, t.sensor.name, { size: 16, weight: 650, anchor: "middle" }));
  parts.push(text(560, 610, t.sensor.sub, { size: 11.5, anchor: "middle", fill: C.muted }));

  // Resistor
  parts.push(rect(770, 248, 70, 24, 7, "#FFF6E8", "#C9A15E", 2));
  parts.push(text(805, 240, t.resistor, { size: 13, weight: 600, anchor: "middle", fill: C.muted }));
  parts.push(text(862, 292, t.optional, { size: 11, anchor: "middle", fill: C.muted }));

  // Capacitor (across the supply)
  parts.push(`<line x1="318" y1="296" x2="352" y2="296" stroke="${C.gnd}" stroke-width="4" stroke-linecap="round"/>`);
  parts.push(`<line x1="318" y1="320" x2="352" y2="320" stroke="${C.gnd}" stroke-width="4" stroke-linecap="round"/>`);
  parts.push(wire([[335, 260], [335, 296]], C.red));
  parts.push(wire([[335, 320], [335, 360]], C.gnd));
  parts.push(text(366, 318, t.cap, { size: 13, weight: 600, fill: C.muted }));

  // Junction dots
  parts.push(jdot(400, 90, C.red));
  parts.push(jdot(335, 260, C.red));
  parts.push(jdot(335, 360, C.gnd));
  parts.push(jdot(390, 360, C.gnd));
  parts.push(jdot(560, 720, C.gnd));

  // Tips card
  parts.push(`<g filter="url(#soft)">${rect(48, 470, 330, 180, 14, C.card, C.cardStroke)}</g>`);
  parts.push(text(68, 503, t.tipsTitle, { size: 16, weight: 650 }));
  t.tips.forEach((s, i) => {
    const y = 532 + i * 28;
    parts.push(`<circle cx="72" cy="${y - 4}" r="3.5" fill="${C.accent}"/>`);
    parts.push(text(84, y, s, { size: 13, fill: "#4A423A" }));
  });
  parts.push(text(1192, 772, t.footer, { size: 12, anchor: "end", fill: C.muted }));

  parts.push("</svg>");
  return parts.join("\n") + "\n";
}

const ru = {
  title: "Схема подключения",
  subtitle: "Wi-Fi Лампа · ESP8266 + WS2812B 16×16 + TTP223",
  legend: [
    { c: C.red, l: "+5 В" }, { c: C.gnd, l: "земля" }, { c: C.data, l: "данные" },
    { c: C.v33, l: "3,3 В" }, { c: C.out, l: "OUT сенсора" },
  ],
  psu: { name: "Блок питания", spec: "5 В · 10–15 А", watt: "≈ 50–75 Вт, DC-разъём", plus: "+5 В" },
  board: { sub: "NodeMCU · 4 МБ flash" },
  matrix: { sub: "256 светодиодов · гибкая" },
  sensor: { name: "Сенсор TTP223", sub: "тап · двойной тап · удержание" },
  resistor: "330–470 Ом",
  optional: "опционально: 74AHCT125",
  cap: "1000 мкФ",
  tipsTitle: "Важно",
  tips: [
    "Общая земля обязательна",
    "Не питать матрицу от платы",
    "Лента: 5 В, 10–15 А",
    "Конденсатор — у входа матрицы",
  ],
  footer: "Подробнее — docs/wiring.md",
};

const en = {
  title: "Wiring diagram",
  subtitle: "Wi-Fi Lampa · ESP8266 + WS2812B 16×16 + TTP223",
  legend: [
    { c: C.red, l: "+5 V" }, { c: C.gnd, l: "ground" }, { c: C.data, l: "data" },
    { c: C.v33, l: "3.3 V" }, { c: C.out, l: "sensor OUT" },
  ],
  psu: { name: "Power supply", spec: "5 V · 10–15 A", watt: "≈ 50–75 W, DC jack", plus: "+5 V" },
  board: { sub: "NodeMCU · 4 MB flash" },
  matrix: { sub: "256 LEDs · flexible" },
  sensor: { name: "TTP223 touch sensor", sub: "tap · double tap · hold" },
  resistor: "330–470 Ω",
  optional: "optional: 74AHCT125",
  cap: "1000 µF",
  tipsTitle: "Important",
  tips: [
    "Common ground is required",
    "Don't power the matrix from the board",
    "LED power: 5 V, 10–15 A",
    "Capacitor goes at the matrix input",
  ],
  footer: "Details — docs/wiring.md",
};

// --- Components strip -------------------------------------------------------

function componentIcons() {
  const icons = [];
  // NodeMCU
  icons.push(`<g transform="translate(130,112)">
    <rect x="-45" y="-28" width="90" height="56" rx="8" fill="${C.board}" stroke="${C.boardStroke}" stroke-width="2"/>
    ${[-30, -18, -6, 6, 18, 30].map((x) => `<rect x="${x - 3}" y="-36" width="6" height="10" rx="2" fill="${C.gold}" stroke="${C.goldStroke}" stroke-width="1"/>`).join("")}
    ${[-30, -18, -6, 6, 18, 30].map((x) => `<rect x="${x - 3}" y="26" width="6" height="10" rx="2" fill="${C.gold}" stroke="${C.goldStroke}" stroke-width="1"/>`).join("")}
    <rect x="-14" y="-12" width="28" height="24" rx="4" fill="#35594E"/>
    <text x="0" y="4" font-family="${F}" font-size="10" font-weight="700" fill="#EAF6EE" text-anchor="middle">ESP</text>
  </g>`);
  // Matrix
  let dots = "";
  for (let r = 0; r < 6; r++) {
    for (let c = 0; c < 6; c++) {
      const ci = (r * 6 + c);
      dots += `<circle cx="${-25 + c * 10}" cy="${-25 + r * 10}" r="3" fill="${PALETTE[(ci * 3 + r) & 7]}"/>`;
    }
  }
  icons.push(`<g transform="translate(130,112)">
    <rect x="-38" y="-38" width="76" height="76" rx="10" fill="${C.mat}" stroke="${C.matStroke}" stroke-width="2"/>${dots}
  </g>`);
  // TTP223
  icons.push(`<g transform="translate(130,112)">
    <rect x="-34" y="-24" width="68" height="48" rx="7" fill="#2E5F8F" stroke="#1F4569" stroke-width="2"/>
    <circle cx="-10" cy="0" r="12" fill="${C.gold}" stroke="${C.goldStroke}" stroke-width="2"/>
    <rect x="10" y="-16" width="16" height="12" rx="2" fill="#1B3550"/>
    ${[-22, -14, 22, 14].map((y, i) => `<circle cx="${i < 2 ? -26 : 26}" cy="${y}" r="2.4" fill="#D9B44A"/>`).join("")}
  </g>`);
  // Resistor
  icons.push(`<g transform="translate(130,112)">
    <line x1="-52" y1="0" x2="-38" y2="0" stroke="#8D8272" stroke-width="3"/>
    <line x1="38" y1="0" x2="52" y2="0" stroke="#8D8272" stroke-width="3"/>
    <rect x="-38" y="-11" width="76" height="22" rx="10" fill="#E9D9BE" stroke="#C9A15E" stroke-width="2"/>
    <rect x="-20" y="-11" width="7" height="22" fill="#7A4A2B"/>
    <rect x="-6" y="-11" width="7" height="22" fill="#2E2A26"/>
    <rect x="8" y="-11" width="7" height="22" fill="#D64541"/>
    <rect x="22" y="-11" width="4" height="22" fill="#C9A15E"/>
  </g>`);
  // Capacitor
  icons.push(`<g transform="translate(130,112)">
    <line x1="0" y1="-48" x2="0" y2="-34" stroke="#8D8272" stroke-width="3"/>
    <line x1="0" y1="34" x2="0" y2="48" stroke="#8D8272" stroke-width="3"/>
    <rect x="-20" y="-34" width="40" height="68" rx="12" fill="#3A66A8" stroke="#2B4C7E" stroke-width="2"/>
    <rect x="-20" y="-34" width="12" height="68" rx="6" fill="#A9C3E6"/>
    <path d="M 6 -20 h 14 M 13 -27 v 14" stroke="#F2F6FC" stroke-width="2.6" stroke-linecap="round"/>
  </g>`);
  // PSU
  icons.push(`<g transform="translate(130,112)">
    <rect x="-44" y="-28" width="80" height="56" rx="10" fill="#33302B" stroke="#211F1B" stroke-width="2"/>
    <path d="M -14 -16 L -24 4 h 10 l -4 16 L -2 0 h -10 l 4 -16 Z" fill="#FFC44D"/>
    <path d="M 36 0 q 10 0 10 10 t 10 10" fill="none" stroke="#8D8272" stroke-width="3" stroke-linecap="round"/>
    <rect x="34" y="-4" width="14" height="8" rx="3" fill="#2E2A26" stroke="#8D8272" stroke-width="1.5"/>
    ${[-18, -10, -2].map((y) => `<line x1="18" y1="${y}" x2="30" y2="${y}" stroke="#57544E" stroke-width="2"/>`).join("")}
  </g>`);
  return icons;
}

function componentsStrip() {
  const items = [
    ["ESP8266 NodeMCU", "4 MB · Wi-Fi"],
    ["WS2812B 16×16", "256 RGB"],
    ["TTP223", "touch"],
    ["330–470 Ω", "1/4 W"],
    ["1000 µF", "≥ 6.3 V"],
    ["5 V · 10 A", "50 W"],
  ];
  const icons = componentIcons();
  const parts = [];
  parts.push(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1240 290" width="1240" height="290" role="img" aria-label="Components">`);
  parts.push(`<defs><filter id="soft" x="-20%" y="-20%" width="140%" height="140%">
    <feDropShadow dx="0" dy="3" stdDeviation="6" flood-color="#3A2E1F" flood-opacity="0.08"/>
  </filter></defs>`);
  parts.push(`<rect x="0.5" y="0.5" width="1239" height="289" rx="0" fill="${C.bg}"/>`);
  parts.push(`<rect x="10" y="10" width="1220" height="270" rx="26" fill="none" stroke="${C.border}" stroke-width="2"/>`);
  items.forEach(([name, spec], i) => {
    const x = 42 + i * 196;
    parts.push(`<g filter="url(#soft)">${rect(x, 60, 176, 170, 16, C.card, C.cardStroke)}</g>`);
    parts.push(icons[i].replace('translate(130,112)', `translate(${x + 88},112)`));
    parts.push(text(x + 88, 184, name, { size: 13.5, weight: 650, anchor: "middle" }));
    parts.push(text(x + 88, 205, spec, { size: 12, anchor: "middle", fill: C.muted }));
  });
  parts.push("</svg>");
  return parts.join("\n") + "\n";
}

fs.writeFileSync(path.join(root, "docs", "wiring.svg"), wiringDiagram(ru));
fs.writeFileSync(path.join(root, "docs", "wiring-en.svg"), wiringDiagram(en));
fs.writeFileSync(path.join(root, "docs", "components.svg"), componentsStrip());
console.log("docs/wiring.svg, docs/wiring-en.svg, docs/components.svg written");
