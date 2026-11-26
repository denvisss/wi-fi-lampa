/* Генератор firmware/src/tzdata.h из tools/tz-zones.csv.
   Запуск: node tools/gen-tz.mjs */
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const SRC = path.join(ROOT, "tools", "tz-zones.csv");
const OUT = path.join(ROOT, "firmware", "src", "tzdata.h");

const extra = [
  ["UTC", "UTC0"],
  ["America/Ciudad_Juarez", "MST7MDT,M3.2.0,M11.1.0"],
  ["America/Coyhaique", "<-03>3"],
  ["Asia/Qostanay", "<+05>-5"],
];

const rows = fs
  .readFileSync(SRC, "utf8")
  .trim()
  .split(/\r?\n/)
  .map((line) => {
    const m = line.match(/^"([^"]+)","([^"]*)"$/);
    return m ? [m[1], m[2]] : null;
  })
  .filter(Boolean);

const seen = new Set(rows.map(([z]) => z));
for (const [z, p] of extra) if (!seen.has(z)) rows.push([z, p]);
rows.sort((a, b) => (a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0));

const esc = (s) => s.replace(/\\/g, "\\\\").replace(/"/g, '\\"');
const lines = rows.map(([z, p]) => `    "${esc(z)}\\t${esc(p)}\\n"`);

const header = `#pragma once
// IANA → POSIX TZ. Сгенерировано tools/gen-tz.mjs, не править вручную.
// ${rows.length} зон; таблица лежит в PROGMEM (flash) и читается через pgm_read_byte.
#include <pgmspace.h>

static const char TZ_TABLE[] PROGMEM =
${lines.join("\n")};
`;

fs.writeFileSync(OUT, header);
const bytes = fs.statSync(OUT).size;
console.log(`tzdata.h: ${rows.length} zones, ${bytes} bytes`);
