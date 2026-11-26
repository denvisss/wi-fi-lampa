/* Лампа · mock-сервер для разработки веб-интерфейса (без зависимостей)
   Запуск: node web/mock-server.mjs [порт]
   Отдаёт /web как статику, REST API и WebSocket на том же порту. */
import http from "node:http";
import fs from "node:fs";
import fsp from "node:fs/promises";
import path from "node:path";
import crypto from "node:crypto";
import { fileURLToPath } from "node:url";

const ROOT = path.dirname(fileURLToPath(import.meta.url));
const PORT = Number(process.argv[2] || process.env.PORT || 8080);
const START = Date.now();

const EFFECTS = [
  ["solid", "Сплошной цвет", "atmo", 1, 0, 0],
  ["breathing", "Дыхание", "atmo", 1, 1, 0],
  ["candle", "Свеча", "atmo", 0, 0, 1],
  ["aurora", "Северное сияние", "atmo", 0, 1, 1],
  ["ocean", "Океан", "atmo", 0, 1, 1],
  ["starfield", "Звёздное небо", "atmo", 0, 1, 1],
  ["rainbow", "Радуга", "atmo", 0, 1, 0],
  ["plasma", "Плазма", "atmo", 0, 1, 1],
  ["fire", "Камин", "ny", 0, 1, 1],
  ["snowfall", "Снегопад", "ny", 0, 1, 1],
  ["garland", "Гирлянда", "ny", 0, 1, 0],
  ["spiral", "Ёлочная спираль", "ny", 1, 1, 0],
  ["candy", "Карамель", "ny", 0, 1, 0],
  ["fireworks", "Салют", "ny", 0, 1, 1],
  ["clock", "Часы", "time", 1, 1, 0],
  ["nycountdown", "До Нового года", "time", 1, 1, 0],
  ["nyshow", "Новогоднее шоу", "ny", 0, 1, 1],
].map(([id, name, group, color, speed, intensity]) => ({ id, name, group, color: !!color, speed: !!speed, intensity: !!intensity }));

const state = {
  power: true,
  brightness: 160,
  effect: "aurora",
  color: "#FFB066",
  speed: 128,
  intensity: 160,
  currentLimit: 9000,
  tz: "Europe/Moscow",
  manualTime: false,
  clock: { colorMode: 0, speed: 80, showDate: true, hour12: false },
  mapping: { serpentine: true, reverse: false, flipY: false, offset: 0 },
  playlist: { enabled: false, intervalMin: 15 },
  alarms: [
    { enabled: true, days: 0x1f, hour: 7, minute: 0, durationMin: 15, maxBrightness: 200, snoozeMin: 9, afterOffMin: 0, lastFiredYday: 0 },
    { enabled: false, days: 0x60, hour: 9, minute: 30, durationMin: 20, maxBrightness: 160, snoozeMin: 9, afterOffMin: 30, lastFiredYday: 0 },
  ],
};

/* Ручное время: база + ход после установки */
let manualClock = null;
const nowMs = () => (manualClock ? manualClock.epoch * 1000 + (Date.now() - manualClock.at) : Date.now());
const lampIso = () => new Date(nowMs()).toISOString();

/* Демо-состояние GitHub-релиза */
const GH = {
  tag: "v1.1.0-pre1",
  notes: "• многоцветная гирлянда\n• ручное время и все часовые пояса\n• обновление прошивки с GitHub",
  assetSize: 660000,
};
let fwVersion = "1.0.2";
let otaSim = null;

const sunrise = { active: false, snoozing: false, progress: 0, alarmId: -1, t0: 0, frozen: 0, test: false };
const SUNRISE_MS = 45000;
const SNOOZE_MS = 9000;

/* ---------- WS ---------- */

const clients = new Set();

function frame(opcode, payload) {
  const len = payload.length;
  let head;
  if (len < 126) {
    head = Buffer.from([0x80 | opcode, len]);
  } else if (len < 65536) {
    head = Buffer.alloc(4);
    head[0] = 0x80 | opcode;
    head[1] = 126;
    head.writeUInt16BE(len, 2);
  } else {
    head = Buffer.alloc(10);
    head[0] = 0x80 | opcode;
    head[1] = 127;
    head.writeBigUInt64BE(BigInt(len), 2);
  }
  return Buffer.concat([head, payload]);
}

function sendText(client, text) {
  if (client.socket.destroyed) return;
  client.socket.write(frame(0x1, Buffer.from(text)));
}

function sendBinary(client, data) {
  if (client.socket.destroyed) return;
  client.socket.write(frame(0x2, data));
}

function broadcastText(text) {
  for (const c of clients) sendText(c, text);
}

function broadcastOta() {
  if (!otaSim) return;
  broadcastText(JSON.stringify({ type: "ota", phase: otaSim.phase, progress: otaSim.progress, error: "" }));
}

function stateMessage() {
  return JSON.stringify({ type: "state", data: statePayload() });
}

function broadcastState() {
  const msg = stateMessage();
  for (const c of clients) sendText(c, msg);
}

let broadcastTimer = null;
function broadcastStateSoon() {
  clearTimeout(broadcastTimer);
  broadcastTimer = setTimeout(broadcastState, 150);
}

function nyDays() {
  const now = new Date();
  let next = new Date(now.getFullYear() + 1, 0, 1);
  if (now.getMonth() === 0 && now.getDate() === 1) next = new Date(now.getFullYear(), 0, 1);
  return Math.max(0, Math.ceil((next - now) / 86400000));
}

function statePayload() {
  return {
    power: state.power,
    brightness: state.brightness,
    effect: state.effect,
    color: state.color,
    speed: state.speed,
    intensity: state.intensity,
    currentLimit: state.currentLimit,
    tz: state.tz,
    manualTime: state.manualTime,
    clock: state.clock,
    mapping: state.mapping,
    playlist: state.playlist,
    alarms: state.alarms,
    device: {
      fw: fwVersion,
      ip: "192.168.1.42",
      rssi: -54,
      uptime: Math.floor((Date.now() - START) / 1000),
      heap: 31240,
      mac: "A4:CF:12:AA:BB:CC",
      time: lampIso(),
      ap: false,
    },
    sunrise: {
      active: sunrise.active,
      snoozing: sunrise.snoozing,
      progress: sunrise.progress,
      alarmId: sunrise.alarmId,
    },
    ny: { days: nyDays() },
  };
}

function onWsData(client, chunk) {
  client.buf = Buffer.concat([client.buf, chunk]);
  while (client.buf.length >= 2) {
    const b0 = client.buf[0];
    const b1 = client.buf[1];
    const opcode = b0 & 0x0f;
    const masked = (b1 & 0x80) !== 0;
    let len = b1 & 0x7f;
    let off = 2;
    if (len === 126) {
      if (client.buf.length < 4) break;
      len = client.buf.readUInt16BE(2);
      off = 4;
    } else if (len === 127) {
      if (client.buf.length < 10) break;
      len = Number(client.buf.readBigUInt64BE(2));
      off = 10;
    }
    let mask = null;
    if (masked) {
      if (client.buf.length < off + 4) break;
      mask = client.buf.subarray(off, off + 4);
      off += 4;
    }
    if (client.buf.length < off + len) break;
    const payload = Buffer.from(client.buf.subarray(off, off + len));
    if (mask) for (let i = 0; i < len; i++) payload[i] ^= mask[i % 4];
    client.buf = client.buf.subarray(off + len);

    if (opcode === 0x8) {
      client.socket.end(frame(0x8, Buffer.alloc(0)));
      clients.delete(client);
      return;
    }
    if (opcode === 0x9) {
      client.socket.write(frame(0xa, payload));
      continue;
    }
    if (opcode !== 0x1) continue;
    try {
      const msg = JSON.parse(payload.toString());
      if (msg.type === "preview") client.preview = !!msg.on;
      else if (msg.type === "ping") sendText(client, '{"type":"pong"}');
    } catch {
      /* ignore */
    }
  }
}

/* ---------- fake matrix rendering ---------- */

const clamp01 = (v) => Math.max(0, Math.min(1, v));
const hsv = (h, s, v) => {
  h = ((h % 1) + 1) % 1;
  const i = Math.floor(h * 6);
  const f = h * 6 - i;
  const p = v * (1 - s);
  const q = v * (1 - f * s);
  const t = v * (1 - (1 - f) * s);
  return [[v, t, p], [q, v, p], [p, v, t], [p, q, v], [t, p, v], [v, p, q]][i % 6];
};
const hexRgb = (hex) => {
  const n = parseInt(hex.replace("#", ""), 16);
  return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
};

const GARLAND_PALETTE = [
  [255, 44, 44], [255, 146, 24], [255, 214, 64], [52, 205, 84],
  [48, 128, 255], [176, 92, 255], [72, 224, 255], [255, 240, 214],
];

function renderMock() {
  const buf = Buffer.alloc(768);
  const t = Date.now() / 1000;
  const [cr, cg, cb] = hexRgb(state.color);
  const k = (state.power ? state.brightness : state.brightness * 0.04) / 255;
  const hueBase = t * 0.05;

  for (let y = 0; y < 16; y++) {
    for (let x = 0; x < 16; x++) {
      let r = 0, g = 0, b = 0;
      switch (state.effect) {
        case "solid":
          [r, g, b] = [cr, cg, cb];
          break;
        case "garland": {
          const rings = [3, 8, 13];
          const ri = rings.indexOf(y);
          [r, g, b] = [2, 2, 0];
          if (ri >= 0 && ((x + ri * 2) & 3) === 0) {
            let h = (x * 197 + ri * 91) & 255;
            h ^= h >> 4;
            h = (h * 11) & 255;
            const [pr, pg, pb] = GARLAND_PALETTE[(h >> 1) & 7];
            const tw = 0.72 + 0.28 * Math.sin(t * 1.4 + x * 0.7 + ri * 1.3);
            const blink = Math.sin(t * 2.6 + x * 0.9 + ri * 1.7) > 0.93 ? 1 : 0;
            const kk = blink ? 1 : tw;
            [r, g, b] = [pr * kk, pg * kk, pb * kk];
          }
          break;
        }
        case "breathing": {
          const ph = (Math.sin(t * 0.6) + 1) / 2;
          const kk = (0.08 + 0.92 * ph * ph) * state.brightness / 255;
          [r, g, b] = [cr * kk, cg * kk, cb * kk];
          break;
        }
        case "candle": {
          const fl = 0.75 + 0.25 * Math.sin(t * 8 + x * 1.7 + y * 2.3) * Math.sin(t * 3.1 + y);
          [r, g, b] = [255 * fl, 120 * fl * 0.8, 30 * fl * 0.5];
          break;
        }
        case "aurora": {
          const w = Math.sin(x * 0.55 + t * 0.7) + Math.sin(y * 0.7 - t * 0.5 + x * 0.3) + Math.sin((x + y) * 0.35 + t * 0.3);
          const v = clamp01(0.35 + 0.35 * w);
          const [rr, gg, bb] = hsv(0.42 + 0.18 * w + hueBase, 0.8, v);
          [r, g, b] = [rr * 120, gg * 255, bb * 220];
          break;
        }
        case "ocean": {
          const v = clamp01(0.3 + 0.4 * Math.sin(x * 0.4 + t) + 0.25 * Math.sin(y * 0.8 - t * 1.2));
          [r, g, b] = [20 * v, 120 * v, 255 * v];
          break;
        }
        case "starfield": {
          const h = Math.sin(x * 127.1 + y * 311.7) * 43758.5453;
          const tw = Math.abs(h - Math.floor(h));
          const on = tw > 0.965 && Math.sin(t * (1 + tw * 4)) > 0.2;
          [r, g, b] = on ? [200, 210, 255] : [0, 0, 8];
          break;
        }
        case "rainbow": {
          const [rr, gg, bb] = hsv(x / 16 + y / 40 + hueBase * 2, 1, 1);
          [r, g, b] = [rr * 255, gg * 255, bb * 255];
          break;
        }
        case "plasma": {
          const v = (Math.sin(x / 2.5 + t) + Math.sin(y / 3 - t * 0.9) + Math.sin((x + y) / 4 + t * 0.6) + 3) / 6;
          const [rr, gg, bb] = hsv(v * 0.7 + hueBase, 0.85, 0.9);
          [r, g, b] = [rr * 255, gg * 255, bb * 255];
          break;
        }
        case "fire": {
          const heat = clamp01((15 - y) / 15 + 0.35 * Math.sin(x * 0.9 + t * 2.2) + (Math.random() - 0.5) * 0.25);
          [r, g, b] = [255 * heat, 150 * heat * heat, 30 * heat * heat * heat];
          break;
        }
        case "snowfall": {
          const sx = (x + Math.floor(t * 2 + y)) % 16;
          const flake = (y * 7 + sx * 13) % 37 < 2 || (y * 11 + x * 5) % 53 < 2;
          [r, g, b] = flake ? [220, 230, 255] : [4, 8, 20];
          break;
        }
        case "spiral": {
          const v = (Math.sin((y * 32 + x * 2 - t * 160) * 0.05) + 1) / 2;
          const [rr, gg, bb] = hsv(hueBase * 3 + y * 0.02, 0.9, v);
          [r, g, b] = [rr * 255, gg * 255, bb * 255];
          break;
        }
        case "candy": {
          const stripe = Math.sin((x + y * 2 - t * 8) * 0.9) > 0;
          [r, g, b] = stripe ? [255, 40, 70] : [255, 240, 225];
          break;
        }
        case "fireworks":
        case "nyshow": {
          const h = Math.sin(x * 91.7 + y * 47.3 + Math.floor(t * 3) * 13.37) * 43758.5;
          const on = Math.abs(h - Math.floor(h)) > 0.94;
          const [rr, gg, bb] = hsv((Math.floor(t * 3) * 0.13) % 1, 0.7, 1);
          [r, g, b] = on ? [rr * 255, gg * 255, bb * 255] : [0, 0, 0];
          break;
        }
        case "clock":
        case "nycountdown": {
          const dot = (x + Math.floor(t * 6)) % 16 < 2;
          [r, g, b] = dot ? [cr, cg, cb] : [cr * 0.06, cg * 0.06, cb * 0.06];
          break;
        }
        default:
          [r, g, b] = [cr, cg, cb];
      }
      if (state.effect !== "breathing") {
        r *= k; g *= k; b *= k;
      }
      const i = (y * 16 + x) * 3;
      buf[i] = Math.min(255, Math.max(0, r));
      buf[i + 1] = Math.min(255, Math.max(0, g));
      buf[i + 2] = Math.min(255, Math.max(0, b));
    }
  }

  if (sunrise.active && !sunrise.snoozing) {
    const front = sunrise.progress * 20;
    for (let y = 0; y < 16; y++) {
      if (y > front) continue;
      const edge = clamp01(front - y);
      for (let x = 0; x < 16; x++) {
        const i = (y * 16 + x) * 3;
        buf[i] = Math.min(255, buf[i] + 255 * edge * 0.6);
        buf[i + 1] = Math.min(255, buf[i + 1] + 130 * edge * 0.6);
        buf[i + 2] = Math.min(255, buf[i + 2] + 40 * edge * 0.6);
      }
    }
  }
  return buf;
}

setInterval(() => {
  let pv = false;
  for (const c of clients) if (c.preview) { pv = true; break; }
  if (!pv) return;
  const data = renderMock();
  for (const c of clients) if (c.preview) sendBinary(c, data);
}, 160);

/* ---------- sunrise engine (демо) ---------- */

function sunriseTick() {
  if (!sunrise.active) return;
  const now = Date.now();
  if (sunrise.snoozing) {
    if (now >= sunrise.frozen) {
      sunrise.snoozing = false;
      sunrise.t0 = now - sunrise.progress * SUNRISE_MS;
    }
  } else {
    sunrise.progress = Math.min(1, (now - sunrise.t0) / SUNRISE_MS);
    if (!sunrise.test && sunrise.progress >= 1) {
      sunrise.active = false;
      state.power = true;
      state.effect = "solid";
      state.color = "#FFD9B0";
      state.brightness = 220;
      broadcastState();
    }
  }
  broadcastText(
    JSON.stringify({
      type: "sunrise",
      active: sunrise.active,
      snoozing: sunrise.snoozing,
      progress: sunrise.progress,
    })
  );
}

setInterval(sunriseTick, 700);

function startSunrise(test = false) {
  sunrise.active = true;
  sunrise.snoozing = false;
  sunrise.progress = 0;
  sunrise.alarmId = test ? 0 : 0;
  sunrise.t0 = Date.now();
  sunrise.frozen = 0;
  sunrise.test = test;
  state.power = true;
  broadcastState();
  broadcastText(JSON.stringify({ type: "sunrise", active: true, snoozing: false, progress: 0 }));
}

setInterval(() => {
  const now = new Date();
  if (now.getSeconds() !== 0) return;
  const dayBit = 1 << ((now.getDay() + 6) % 7);
  const yday = now.getFullYear() * 400 + now.getMonth() * 31 + now.getDate();
  state.alarms.forEach((a, i) => {
    if (!a.enabled || !(a.days & dayBit) || a.hour !== now.getHours() || a.minute !== now.getMinutes()) return;
    if (a.lastFiredYday === yday) return;
    a.lastFiredYday = yday;
    sunrise.progress = 0;
    startSunrise(false);
    sunrise.alarmId = i;
  });
}, 1000);

setInterval(broadcastState, 15000);

/* ---------- HTTP ---------- */

const MIME = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".svg": "image/svg+xml",
  ".png": "image/png",
  ".ico": "image/x-icon",
};

function sendJson(res, code, obj) {
  const body = JSON.stringify(obj);
  res.writeHead(code, {
    "Content-Type": "application/json; charset=utf-8",
    "Access-Control-Allow-Origin": "*",
    "Access-Control-Allow-Headers": "Content-Type",
    "Access-Control-Allow-Methods": "GET, POST, PUT, OPTIONS",
    "Cache-Control": "no-store",
  });
  res.end(body);
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let size = 0;
    req.on("data", (c) => {
      size += c.length;
      if (size > 4 * 1024 * 1024) {
        reject(new Error("too large"));
        req.destroy();
        return;
      }
      chunks.push(c);
    });
    req.on("end", () => resolve(Buffer.concat(chunks)));
    req.on("error", reject);
  });
}

function clamp(v, lo, hi) {
  return Math.max(lo, Math.min(hi, Number(v)));
}

function patchState(p) {
  if (!p || typeof p !== "object") return;
  if (p.power !== undefined) state.power = !!p.power;
  if (p.brightness !== undefined) state.brightness = clamp(p.brightness, 1, 255);
  if (p.effect && EFFECTS.some((e) => e.id === p.effect)) state.effect = p.effect;
  if (typeof p.color === "string" && /^#[0-9a-fA-F]{6}$/.test(p.color)) state.color = p.color.toUpperCase();
  if (p.speed !== undefined) state.speed = clamp(p.speed, 0, 255);
  if (p.intensity !== undefined) state.intensity = clamp(p.intensity, 0, 255);
  if (p.currentLimit !== undefined) state.currentLimit = clamp(p.currentLimit, 500, 15000);
  if (typeof p.tz === "string" && p.tz.length > 0 && p.tz.length < 47) state.tz = p.tz;
  if (p.clock) {
    if (p.clock.colorMode !== undefined) state.clock.colorMode = clamp(p.clock.colorMode, 0, 1);
    if (p.clock.speed !== undefined) state.clock.speed = clamp(p.clock.speed, 10, 150);
    if (p.clock.showDate !== undefined) state.clock.showDate = !!p.clock.showDate;
    if (p.clock.hour12 !== undefined) state.clock.hour12 = !!p.clock.hour12;
  }
  if (p.mapping) {
    for (const key of ["serpentine", "reverse", "flipY"]) {
      if (p.mapping[key] !== undefined) state.mapping[key] = !!p.mapping[key];
    }
    if (p.mapping.offset !== undefined) state.mapping.offset = ((Number(p.mapping.offset) % 16) + 16) % 16;
  }
  if (p.playlist) {
    if (p.playlist.enabled !== undefined) state.playlist.enabled = !!p.playlist.enabled;
    if (p.playlist.intervalMin !== undefined) state.playlist.intervalMin = clamp(p.playlist.intervalMin, 1, 60);
  }
}

async function handleApi(req, res, url) {
  const p = url.pathname;
  if (req.method === "OPTIONS") {
    res.writeHead(204, {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Headers": "Content-Type",
      "Access-Control-Allow-Methods": "GET, POST, PUT, OPTIONS",
    });
    res.end();
    return;
  }

  if (p === "/api/state" && req.method === "GET") return sendJson(res, 200, statePayload());
  if (p === "/api/effects") return sendJson(res, 200, EFFECTS);
  if (p === "/api/timezones") {
    const zones = Array.from(new Set([...Intl.supportedValuesOf("timeZone"), "UTC"])).sort();
    return sendJson(res, 200, zones);
  }

  if (p === "/api/update/check") {
    return sendJson(res, 200, {
      current: fwVersion,
      latest: GH.tag,
      available: fwVersion !== GH.tag.replace(/^v/, ""),
      url: `https://github.com/denvisss/lampa/releases/download/${GH.tag}/firmware.bin`,
      size: GH.assetSize,
      notes: GH.notes,
    });
  }

  if (p === "/api/update/install" && req.method === "POST") {
    await readBody(req);
    if (otaSim) return sendJson(res, 409, { error: "busy" });
    if (fwVersion === GH.tag.replace(/^v/, "")) return sendJson(res, 409, { error: "already latest" });
    const started = Date.now();
    otaSim = { phase: "connecting", progress: 2 };
    broadcastOta();
    const timer = setInterval(() => {
      const el = Date.now() - started;
      if (el < 700) {
        otaSim = { phase: "connecting", progress: 2 };
      } else if (el < 4200) {
        otaSim = { phase: "downloading", progress: Math.min(99, Math.round(((el - 700) / 3500) * 100)) };
      } else if (el < 5000) {
        otaSim = { phase: "finalizing", progress: 99 };
      } else {
        clearInterval(timer);
        otaSim = null;
        fwVersion = GH.tag.replace(/^v/, "");
        broadcastState();
        broadcastText(JSON.stringify({ type: "ota", phase: "done", progress: 100, error: "" }));
        return;
      }
      broadcastOta();
    }, 200);
    return sendJson(res, 200, { ok: true });
  }

  if (p === "/api/state" && req.method === "POST") {
    const body = JSON.parse((await readBody(req)) || "{}");
    patchState(body);
    broadcastStateSoon();
    return sendJson(res, 200, statePayload());
  }

  if (p === "/api/alarms" && req.method === "PUT") {
    const body = JSON.parse((await readBody(req)) || "{}");
    if (Array.isArray(body.alarms)) {
      state.alarms = body.alarms.slice(0, 4).map((a) => ({
        enabled: !!a.enabled,
        days: clamp(a.days, 0, 127),
        hour: clamp(a.hour, 0, 23),
        minute: clamp(a.minute, 0, 59),
        durationMin: clamp(a.durationMin, 5, 30),
        maxBrightness: clamp(a.maxBrightness, 5, 255),
        snoozeMin: clamp(a.snoozeMin, 0, 15),
        afterOffMin: clamp(a.afterOffMin, 0, 180),
        lastFiredYday: 0,
      }));
    }
    broadcastState();
    return sendJson(res, 200, statePayload());
  }

  if (p === "/api/action" && req.method === "POST") {
    const body = JSON.parse((await readBody(req)) || "{}");
    switch (body.action) {
      case "sunrise_test":
        startSunrise(true);
        break;
      case "sunrise_stop":
        sunrise.active = false;
        sunrise.snoozing = false;
        break;
      case "sunrise_snooze":
        if (sunrise.active) {
          sunrise.snoozing = true;
          sunrise.frozen = Date.now() + SNOOZE_MS;
        }
        break;
      case "nyshow":
        state.effect = "nyshow";
        state.power = true;
        break;
      case "settime": {
        const epoch = Number(body.epoch) || 0;
        if (epoch < 1700000000) return sendJson(res, 400, { error: "bad epoch" });
        manualClock = { epoch, at: Date.now() };
        state.manualTime = true;
        break;
      }
      case "timens":
        manualClock = null;
        state.manualTime = false;
        break;
      case "factory":
      case "reboot":
        break;
      default:
        break;
    }
    broadcastStateSoon();
    return sendJson(res, 200, statePayload());
  }

  if (p === "/api/wifi" && req.method === "POST") {
    await readBody(req);
    return sendJson(res, 200, { ok: true });
  }

  if (p === "/api/scan") {
    return sendJson(res, 200, [
      { ssid: "Home_5G", rssi: -41, secure: true },
      { ssid: "Home_2.4G", rssi: -47, secure: true },
      { ssid: "TP-LINK_2G", rssi: -63, secure: true },
      { ssid: "Guest", rssi: -78, secure: false },
    ]);
  }

  if (p === "/api/ota" && req.method === "POST") {
    req.resume();
    req.on("end", () => sendJson(res, 200, { ok: true }));
    return;
  }

  sendJson(res, 404, { error: "not found" });
}

async function serveStatic(req, res, url) {
  let rel = decodeURIComponent(url.pathname);
  if (rel === "/") rel = "/index.html";
  const file = path.join(ROOT, path.normalize(rel).replace(/^([/\\])+/, ""));
  if (!file.startsWith(ROOT)) {
    res.writeHead(403);
    res.end();
    return;
  }
  try {
    const data = await fsp.readFile(file);
    const ext = path.extname(file).toLowerCase();
    res.writeHead(200, {
      "Content-Type": MIME[ext] || "application/octet-stream",
      "Cache-Control": "no-store",
    });
    res.end(data);
  } catch {
    if (!path.extname(rel)) return serveStatic(req, res, new URL("/index.html", "http://x"));
    res.writeHead(404, { "Content-Type": "text/plain" });
    res.end("not found");
  }
}

const server = http.createServer(async (req, res) => {
  const url = new URL(req.url, "http://localhost");
  try {
    if (url.pathname.startsWith("/api/")) return await handleApi(req, res, url);
    await serveStatic(req, res, url);
  } catch (e) {
    if (!res.headersSent) sendJson(res, 400, { error: String(e.message || e) });
  }
});

server.on("upgrade", (req, socket) => {
  const key = req.headers["sec-websocket-key"];
  if (!key) {
    socket.destroy();
    return;
  }
  const accept = crypto.createHash("sha1").update(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").digest("base64");
  socket.write(
    "HTTP/1.1 101 Switching Protocols\r\n" +
      "Upgrade: websocket\r\n" +
      "Connection: Upgrade\r\n" +
      `Sec-WebSocket-Accept: ${accept}\r\n\r\n`
  );
  const client = { socket, buf: Buffer.alloc(0), preview: false };
  clients.add(client);
  socket.on("data", (chunk) => onWsData(client, chunk));
  const drop = () => {
    clients.delete(client);
    socket.destroy();
  };
  socket.on("close", drop);
  socket.on("error", drop);
  sendText(client, stateMessage());
});

server.listen(PORT, () => {
  console.log(`Лампа mock: http://localhost:${PORT}  (WS на том же порту)`);
  console.log("API: /api/state, /api/effects, /api/alarms, /api/action, /api/scan");
});
