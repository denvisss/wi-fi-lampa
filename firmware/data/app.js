/* Лампа · SPA · Material-ish Astra UI */
"use strict";

const $ = (sel, el = document) => el.querySelector(sel);
const $$ = (sel, el = document) => Array.from(el.querySelectorAll(sel));

const state = {
  s: null,
  effects: [],
  route: "overview",
  ws: null,
  wsOpen: false,
  wsRetry: null,
  wsDelay: 800,
  previewOn: false,
  lastOk: 0,
  tzones: [],
  update: null,
};

const DAYS = ["Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"];
const DAY_FULL = ["Понедельник", "Вторник", "Среда", "Четверг", "Пятница", "Суббота", "Воскресенье"];

const ICON = {
  sun: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"><circle cx="12" cy="12" r="4.2"/><path d="M12 2.5v2.2M12 19.3v2.2M2.5 12h2.2M19.3 12h2.2M5 5l1.6 1.6M17.4 17.4L19 19M19 5l-1.6 1.6M6.6 17.4L5 19"/></svg>',
  moon: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M20.5 14.5A8.5 8.5 0 0 1 9.5 3.5a8.5 8.5 0 1 0 11 11Z"/></svg>',
  power: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M12 3v8"/><path d="M6.4 6.4a8 8 0 1 0 11.2 0"/></svg>',
  edit: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M4 20h4l10-10-4-4L4 16v4Z"/><path d="m13.5 6.5 4 4"/></svg>',
  plus: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M12 5v14M5 12h14"/></svg>',
};

/* ---------------- API ---------------- */

async function api(path, method = "GET", body = null) {
  const opts = { method, headers: {} };
  if (body !== null) {
    opts.headers["Content-Type"] = "application/json";
    opts.body = JSON.stringify(body);
  }
  const res = await fetch(path, opts);
  if (!res.ok) throw new Error((await res.text()) || res.statusText);
  const data = await res.json();
  state.lastOk = Date.now();
  updateConnChip();
  return data;
}

async function postPatch(patch) {
  try {
    applyState(await api("/api/state", "POST", patch));
  } catch (e) {
    toast("Не удалось применить настройку", true);
  }
}

async function doAction(action, extra = {}) {
  try {
    applyState(await api("/api/action", "POST", { action, ...extra }));
  } catch (e) {
    toast("Действие не выполнено", true);
  }
}

/* ---------------- WebSocket ---------------- */

function wsUrl() {
  const proto = location.protocol === "https:" ? "wss" : "ws";
  const isMock = location.port === "8080" || location.port === "5500";
  const port = isMock ? (location.port || "80") : "81";
  return `${proto}://${location.hostname}:${port}/`;
}

function connectWs() {
  if (state.ws) return;
  try {
    const ws = new WebSocket(wsUrl());
    state.ws = ws;
    ws.binaryType = "arraybuffer";
    ws.onopen = () => {
      state.wsOpen = true;
      state.wsDelay = 800;
      updateConnChip();
      if (state.previewOn) sendPreview(true);
    };
    ws.onclose = () => {
      state.ws = null;
      state.wsOpen = false;
      updateConnChip();
      clearTimeout(state.wsRetry);
      state.wsRetry = setTimeout(connectWs, state.wsDelay);
      state.wsDelay = Math.min(10000, state.wsDelay * 1.6);
    };
    ws.onerror = () => ws.close();
    ws.onmessage = (ev) => {
      if (typeof ev.data === "string") {
        let msg;
        try { msg = JSON.parse(ev.data); } catch { return; }
        if (msg.type === "state") applyState(msg.data);
        else if (msg.type === "sunrise") updateSunrise(msg);
        else if (msg.type === "ota") updateOtaUi(msg);
      } else if (ev.data instanceof ArrayBuffer && ev.data.byteLength === 768) {
        drawPreview(new Uint8Array(ev.data));
      }
    };
  } catch {
    state.wsRetry = setTimeout(connectWs, 2000);
  }
}

function sendPreview(on) {
  if (state.ws && state.ws.readyState === 1) {
    state.ws.send(JSON.stringify({ type: "preview", on, fps: 6 }));
  }
}

/* ---------------- state sync ---------------- */

function applyState(s) {
  if (!s) return;
  state.s = s;
  state.lastOk = Date.now();
  updateConnChip();
  updateSunrise(s.sunrise);
  if (!state.effects.length) {
    api("/api/effects")
      .then((list) => {
        state.effects = list;
        syncRouteUI();
      })
      .catch(() => {});
  }
  syncRouteUI();
}

function updateConnChip() {
  const online = state.wsOpen || Date.now() - state.lastOk < 15000;
  const chip = $("#conn-chip");
  const text = $("#conn-text");
  if (!chip || !text) return;
  chip.classList.toggle("offline", !online);
  const s = state.s;
  if (online && s && s.device) {
    text.textContent = s.device.ap ? "Точка доступа" : "Онлайн";
  } else {
    text.textContent = online ? "Онлайн" : "Нет связи";
  }
}

setInterval(updateConnChip, 4000);

/* ---------------- helpers ---------------- */

function toast(msg, err = false) {
  const stack = $("#toasts");
  if (!stack) return;
  const el = document.createElement("div");
  el.className = "toast" + (err ? " err" : "");
  el.textContent = msg;
  stack.appendChild(el);
  setTimeout(() => el.remove(), 3200);
}

const pct = (v) => Math.round((v / 255) * 100);
const effectName = (id) => (state.effects.find((e) => e.id === id) || {}).name || id;

function fmtUptime(sec) {
  if (sec < 60) return sec + " с";
  if (sec < 3600) return Math.floor(sec / 60) + " мин";
  if (sec < 86400) return Math.floor(sec / 3600) + " ч " + (Math.floor(sec / 60) % 60) + " мин";
  return Math.floor(sec / 86400) + " дн " + Math.floor((sec % 86400) / 3600) + " ч";
}

function plural(n, forms) {
  const a = Math.abs(n) % 100;
  const b = a % 10;
  if (a > 10 && a < 20) return forms[2];
  if (b > 1 && b < 5) return forms[1];
  if (b === 1) return forms[0];
  return forms[2];
}

function daysLabel(mask) {
  if (mask === 0) return "нет дней";
  if (mask === 0x7f) return "каждый день";
  if (mask === 0x1f) return "будни";
  if (mask === 0x60) return "выходные";
  return DAYS.filter((_, i) => mask & (1 << i)).join(", ");
}

function greeting() {
  const h = new Date().getHours();
  if (h < 5) return "Доброй ночи";
  if (h < 12) return "Доброе утро";
  if (h < 18) return "Добрый день";
  return "Добрый вечер";
}

/* ---------------- theme ---------------- */

function initTheme() {
  const saved = localStorage.getItem("lampa-theme");
  const prefersDark = window.matchMedia("(prefers-color-scheme: dark)").matches;
  document.documentElement.dataset.theme = saved || (prefersDark ? "dark" : "light");
  renderThemeIcon();
}

function renderThemeIcon() {
  const btn = $("#theme-btn");
  if (!btn) return;
  const dark = document.documentElement.dataset.theme === "dark";
  btn.innerHTML = dark ? ICON.sun : ICON.moon;
}

function toggleTheme() {
  const cur = document.documentElement.dataset.theme;
  document.documentElement.dataset.theme = cur === "dark" ? "light" : "dark";
  localStorage.setItem("lampa-theme", document.documentElement.dataset.theme);
  renderThemeIcon();
}

/* ---------------- router ---------------- */

const ROUTES = ["overview", "effects", "alarm", "clock", "settings"];

function currentRoute() {
  const h = location.hash.replace(/^#\/?/, "").split("?")[0];
  return ROUTES.includes(h) ? h : "overview";
}

function route() {
  const r = currentRoute();
  state.route = r;
  $$("#nav a").forEach((a) => a.classList.toggle("active", a.dataset.route === r));
  if (state.previewOn && r !== "overview") {
    state.previewOn = false;
    sendPreview(false);
  }
  const view = $("#view");
  const render = {
    overview: renderOverview,
    effects: renderEffects,
    alarm: renderAlarm,
    clock: renderClock,
    settings: renderSettings,
  }[r];
  view.innerHTML = render();
  bindOverview();
  bindEffects();
  bindAlarm();
  bindClock();
  bindSettings();
  syncRouteUI();
}

function syncRouteUI() {
  const s = state.s;
  if (!s) return;
  if (state.route === "overview") syncOverview(s);
  if (state.route === "alarm") syncAlarm(s);
  if (state.route === "clock") syncClock(s);
  if (state.route === "settings") syncSettings(s);
  if (state.route === "effects") syncEffects(s);
}

/* ---------------- overview ---------------- */

function effectCard(e, id) {
  return `<div class="effect-card ${id}" data-effect="${e.id}">
    <div class="effect-art art-${e.id}"></div>
    <div class="effect-meta"><div><div class="effect-name">${e.name}</div><div class="effect-kind">${groupName(e.group)}</div></div></div>
    <span class="effect-check">✓</span>
  </div>`;
}

function groupName(g) {
  return g === "ny" ? "Новый год" : g === "time" ? "Время" : "Атмосфера";
}

const COLOR_PRESETS = ["#FFB066", "#FF6B57", "#FF2E63", "#B16BFF", "#4FC3F7", "#59E3A7", "#FFE27A", "#FFFFFF"];

function renderOverview() {
  return `
  <div class="page-head">
    <div><h1>${greeting()}</h1><p>Управление лампой в вашей домашней сети</p></div>
  </div>
  <section class="hero">
    <div class="hero-left">
      <h2 class="hero-title" id="hero-effect">Лампа</h2>
      <p class="hero-sub">Яркость <span id="hero-bright">—</span> · матрица 16×16</p>
      <div class="power-row">
        <button class="power-orb" id="power-orb" aria-label="Питание">${ICON.power}</button>
        <div class="power-text"><strong id="power-title">—</strong><span id="power-sub"></span></div>
      </div>
      <div class="brightness">
        <div class="brightness-head"><label for="brightness">Яркость</label><output id="brightness-out">—</output></div>
        <input type="range" id="brightness" min="1" max="255" value="160">
      </div>
    </div>
    <div class="preview-card">
      <div class="preview-top">
        <span>Превью матрицы 16×16</span>
        <label class="preview-toggle"><input type="checkbox" id="preview-toggle"><span class="track"></span>LIVE</label>
      </div>
      <div class="preview-canvas-wrap"><canvas id="preview" width="16" height="16"></canvas></div>
    </div>
  </section>

  <section class="grid cols-2" style="margin-top:18px">
    <div class="card">
      <div class="row between" style="margin-bottom:10px">
        <div><h2>Эффекты</h2><p class="sub" style="margin:0">Быстрое переключение</p></div>
        <a class="btn small ghost" href="#/effects">Все</a>
      </div>
      <div class="effects-scroller" id="quick-effects"></div>
    </div>
    <div class="stack">
      <div class="card">
        <h2>Цвет</h2><p class="sub">Акцент для цветовых эффектов</p>
        <div class="chips" id="color-chips"></div>
        <div class="row" style="margin-top:14px">
          <input type="color" id="color-custom" class="color-input" style="width:38px;height:38px;border:none;background:none;padding:0;cursor:pointer">
          <span class="hint">Свой цвет</span>
        </div>
      </div>
      <div class="card">
        <div class="row between">
          <div><h2>Плейлист</h2><p class="sub" style="margin:0">Автосмена эффектов</p></div>
          <label class="switch"><input type="checkbox" id="playlist-toggle"><span class="track"></span></label>
        </div>
        <div class="row between" style="margin-top:14px">
          <span class="hint">Менять каждые</span>
          <select class="text-input" id="playlist-interval" style="width:auto">
            <option value="5">5 мин</option><option value="10">10 мин</option>
            <option value="15">15 мин</option><option value="30">30 мин</option><option value="60">60 мин</option>
          </select>
        </div>
      </div>
      <a class="card" href="#/alarm" style="display:flex;justify-content:space-between;align-items:center;gap:12px">
        <div><h2>Будильник-рассвет</h2><p class="sub" id="alarm-summary" style="margin:0">не настроен</p></div>
        <span class="badge">Рассвет</span>
      </a>
    </div>
  </section>`;
}

function syncOverview(s) {
  const orb = $("#power-orb");
  if (!orb) return;
  orb.classList.toggle("on", !!s.power);
  $("#power-title").textContent = s.power ? "Включено" : "Выключено";
  $("#power-sub").textContent = s.power ? "Нажмите, чтобы выключить" : "Нажмите, чтобы включить";
  $("#hero-effect").textContent = effectName(s.effect);
  $("#hero-bright").textContent = pct(s.brightness) + "%";
  const slider = $("#brightness");
  if (document.activeElement !== slider) slider.value = s.brightness;
  slider.style.setProperty("--fill", pct(s.brightness) + "%");
  $("#brightness-out").textContent = pct(s.brightness) + "%";

  const scroller = $("#quick-effects");
  if (scroller && !scroller.dataset.built) {
    scroller.dataset.built = "1";
    scroller.innerHTML = state.effects.map((e) => effectCard(e, "")).join("");
    scroller.addEventListener("click", (ev) => {
      const card = ev.target.closest("[data-effect]");
      if (card) postPatch({ effect: card.dataset.effect, power: true });
    });
  }
  $$("#quick-effects .effect-card").forEach((c) => c.classList.toggle("active", c.dataset.effect === s.effect));

  const chips = $("#color-chips");
  if (chips && !chips.dataset.built) {
    chips.dataset.built = "1";
    chips.innerHTML = COLOR_PRESETS.map(
      (c) => `<button class="chip color-chip" data-color="${c}" style="padding:0;width:36px;height:36px;border-radius:12px;background:${c};border:2px solid var(--line-strong)" title="${c}"></button>`
    ).join("");
    chips.addEventListener("click", (ev) => {
      const b = ev.target.closest("[data-color]");
      if (b) postPatch({ color: b.dataset.color });
    });
  }
  $$("#color-chips .color-chip").forEach((b) => {
    b.style.boxShadow = b.dataset.color.toLowerCase() === s.color.toLowerCase() ? "0 0 0 3px var(--accent-soft), 0 0 0 4px var(--accent)" : "none";
  });
  const custom = $("#color-custom");
  if (custom && document.activeElement !== custom) custom.value = s.color;

  $("#playlist-toggle").checked = !!s.playlist.enabled;
  $("#playlist-interval").value = String(s.playlist.intervalMin);

  const next = nextAlarm(s.alarms);
  $("#alarm-summary").textContent = next
    ? `${daysLabel(next.days)} · ${pad(next.hour)}:${pad(next.minute)} · ${next.durationMin} мин`
    : "не настроен";

  if (state.previewOn) drawIdlePreview();
}

function bindOverview() {
  const orb = $("#power-orb");
  if (orb) orb.addEventListener("click", () => state.s && postPatch({ power: !state.s.power }));

  const slider = $("#brightness");
  if (slider) {
    let t = null;
    slider.addEventListener("input", () => {
      const v = +slider.value;
      slider.style.setProperty("--fill", pct(v) + "%");
      $("#brightness-out").textContent = pct(v) + "%";
      $("#hero-bright").textContent = pct(v) + "%";
      clearTimeout(t);
      t = setTimeout(() => postPatch({ brightness: v }), 130);
    });
  }

  const pt = $("#preview-toggle");
  if (pt) {
    pt.checked = state.previewOn;
    pt.addEventListener("change", () => {
      state.previewOn = pt.checked;
      sendPreview(state.previewOn);
      if (state.previewOn) drawIdlePreview();
    });
  }

  const custom = $("#color-custom");
  if (custom)
    custom.addEventListener("change", () => postPatch({ color: custom.value.toUpperCase() }));

  const pl = $("#playlist-toggle");
  if (pl) pl.addEventListener("change", () => postPatch({ playlist: { enabled: pl.checked } }));

  const pi = $("#playlist-interval");
  if (pi) pi.addEventListener("change", () => postPatch({ playlist: { intervalMin: +pi.value } }));
}

function nextAlarm(alarms) {
  const enabled = (alarms || []).filter((a) => a.enabled && a.days);
  if (!enabled.length) return null;
  return enabled.sort((a, b) => a.hour * 60 + a.minute - (b.hour * 60 + b.minute))[0];
}

const pad = (n) => String(n).padStart(2, "0");

/* ---------------- preview ---------------- */

function drawPreview(bytes) {
  const canvas = $("#preview");
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  const img = ctx.createImageData(16, 16);
  for (let i = 0; i < 256; i++) {
    img.data[i * 4] = bytes[i * 3];
    img.data[i * 4 + 1] = bytes[i * 3 + 1];
    img.data[i * 4 + 2] = bytes[i * 3 + 2];
    img.data[i * 4 + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

function drawIdlePreview() {
  const canvas = $("#preview");
  if (!canvas) return;
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = "#0a0808";
  ctx.fillRect(0, 0, 16, 16);
  ctx.fillStyle = "#3a2a1c";
  ctx.font = "6px sans-serif";
  ctx.fillText("…", 6, 10);
}

/* ---------------- effects page ---------------- */

function renderEffects() {
  return `
  <div class="page-head">
    <div><h1>Эффекты</h1><p id="effects-count">${effectsCountLabel()}</p></div>
  </div>
  <div class="chips" id="effect-filter" style="margin-bottom:16px">
    <button class="chip active" data-filter="all">Все</button>
    <button class="chip" data-filter="atmo">Атмосфера</button>
    <button class="chip" data-filter="ny">Новый год</button>
    <button class="chip" data-filter="time">Время</button>
  </div>
  <div class="effects-grid" id="effects-grid">${state.effects.map((e) => effectCard(e, "")).join("")}</div>`;
}

let effectFilter = "all";

function effectsCountLabel() {
  const n = state.effects.length;
  return n ? `${n} ${plural(n, ["сцена", "сцены", "сцен"])} для лампы-цилиндра` : "Сцены для лампы-цилиндра";
}

function syncEffects(s) {
  const grid = $("#effects-grid");
  if (!grid) return;
  if (!grid.children.length && state.effects.length) {
    grid.innerHTML = state.effects.map((e) => effectCard(e, "")).join("");
  }
  const count = $("#effects-count");
  if (count && state.effects.length) count.textContent = effectsCountLabel();
  $$("#effects-grid .effect-card").forEach((c) => {
    const e = state.effects.find((x) => x.id === c.dataset.effect);
    const show = effectFilter === "all" || (e && e.group === effectFilter);
    c.style.display = show ? "" : "none";
    c.classList.toggle("active", c.dataset.effect === s.effect);
  });
}

function bindEffects() {
  const filter = $("#effect-filter");
  if (filter) {
    filter.addEventListener("click", (ev) => {
      const b = ev.target.closest("[data-filter]");
      if (!b) return;
      effectFilter = b.dataset.filter;
      $$("#effect-filter .chip").forEach((c) => c.classList.toggle("active", c === b));
      if (state.s) syncRouteUI();
    });
  }
  const grid = $("#effects-grid");
  if (grid) {
    grid.addEventListener("click", (ev) => {
      const card = ev.target.closest("[data-effect]");
      if (card) postPatch({ effect: card.dataset.effect, power: true });
    });
  }
}

/* ---------------- alarm page ---------------- */

function renderAlarm() {
  return `
  <div class="page-head">
    <div><h1>Будильник-рассвет</h1><p>Плавное пробуждение: от тёплого огонька до мягкого утреннего света</p></div>
    <button class="btn dark" id="add-alarm">${ICON.plus} Добавить</button>
  </div>
  <div class="stack" id="alarm-list"></div>`;
}

function alarmCardHTML(a, idx) {
  const days = DAYS.map(
    (d, i) => `<span class="day-dot ${a.days & (1 << i) ? "on" : ""}">${d}</span>`
  ).join("");
  return `<div class="card alarm-card">
    <div class="alarm-time-block">
      <div class="alarm-time">${pad(a.hour)}:${pad(a.minute)}</div>
      <div class="alarm-days">${days}</div>
    </div>
    <div class="alarm-meta">
      <div class="name">${a.enabled ? "Активен" : "Выключен"} · ${a.durationMin} мин</div>
      <div class="info">Яркость ${pct(a.maxBrightness)}% · отсрочка ${a.snoozeMin} мин${a.afterOffMin ? ` · авто-выкл ${a.afterOffMin} мин` : ""}</div>
    </div>
    <div class="alarm-side">
      <label class="switch"><input type="checkbox" data-alarm-toggle="${idx}" ${a.enabled ? "checked" : ""}><span class="track"></span></label>
      <button class="btn small ghost" data-alarm-edit="${idx}" aria-label="Редактировать">${ICON.edit}</button>
    </div>
  </div>`;
}

function syncAlarm(s) {
  const list = $("#alarm-list");
  if (!list) return;
  const alarms = s.alarms || [];
  list.innerHTML = alarms.length
    ? alarms.map((a, i) => alarmCardHTML(a, i)).join("")
    : `<div class="card empty">Пока нет будильников. Добавьте первый — и утро станет мягче.</div>`;
}

function bindAlarm() {
  const list = $("#alarm-list");
  const add = $("#add-alarm");
  if (add) add.addEventListener("click", () => openAlarmEditor(-1));
  if (list) {
    list.addEventListener("change", (ev) => {
      const t = ev.target.closest("[data-alarm-toggle]");
      if (!t || !state.s) return;
      const idx = +t.dataset.alarmToggle;
      const alarms = cloneAlarms();
      alarms[idx].enabled = t.checked;
      saveAlarms(alarms);
    });
    list.addEventListener("click", (ev) => {
      const b = ev.target.closest("[data-alarm-edit]");
      if (b) openAlarmEditor(+b.dataset.alarmEdit);
    });
  }
}

function cloneAlarms() {
  return JSON.parse(JSON.stringify(state.s.alarms));
}

async function saveAlarms(alarms) {
  try {
    applyState(await api("/api/alarms", "PUT", { alarms }));
    toast("Будильники сохранены");
  } catch {
    toast("Не удалось сохранить", true);
  }
}

function openAlarmEditor(idx) {
  const existing = idx >= 0 && state.s ? state.s.alarms[idx] : null;
  const a = existing
    ? JSON.parse(JSON.stringify(existing))
    : { enabled: true, days: 0x1f, hour: 7, minute: 0, durationMin: 15, maxBrightness: 200, snoozeMin: 9, afterOffMin: 0 };

  const dlg = document.createElement("dialog");
  dlg.className = "modal";
  dlg.innerHTML = `
    <div class="modal-body">
      <div class="modal-head"><h3>${existing ? "Будильник" : "Новый будильник"}</h3>
        <label class="switch"><input type="checkbox" id="al-enabled" ${a.enabled ? "checked" : ""}><span class="track"></span></label></div>
      <div class="field"><label for="al-time">Время</label><input class="text-input" type="time" id="al-time" step="60" value="${pad(a.hour)}:${pad(a.minute)}"></div>
      <div class="field"><label>Дни недели</label><div class="day-picker">${DAYS.map(
        (d, i) => `<button type="button" data-day="${i}" class="${a.days & (1 << i) ? "on" : ""}">${d}</button>`
      ).join("")}</div></div>
      <div class="field-row">
        <div class="field"><label for="al-dur">Длительность</label>
          <div class="range-label"><span>10 мин</span><span id="al-dur-out">${a.durationMin} мин</span><span>30 мин</span></div>
          <input type="range" id="al-dur" min="5" max="30" value="${a.durationMin}">
        </div>
        <div class="field"><label for="al-bright">Макс. яркость</label>
          <div class="range-label"><span>2%</span><span id="al-bright-out">${pct(a.maxBrightness)}%</span><span>100%</span></div>
          <input type="range" id="al-bright" min="5" max="255" value="${a.maxBrightness}">
        </div>
      </div>
      <div class="field-row">
        <div class="field"><label for="al-snooze">Отсрочка</label>
          <select class="text-input" id="al-snooze">${[0, 5, 9, 10, 15].map((m) => `<option value="${m}" ${a.snoozeMin === m ? "selected" : ""}>${m ? m + " мин" : "выкл"}</option>`).join("")}</select>
        </div>
        <div class="field"><label for="al-off">Авто-выключение</label>
          <select class="text-input" id="al-off">${[0, 15, 30, 45, 60, 90].map((m) => `<option value="${m}" ${a.afterOffMin === m ? "selected" : ""}>${m ? "через " + m + " мин" : "не выключать"}</option>`).join("")}</select>
        </div>
      </div>
      <div class="modal-foot">
        <div class="row">${existing ? '<button class="btn danger small" id="al-del">Удалить</button>' : '<button class="btn ghost small" id="al-test">Тест рассвета</button>'}</div>
        <div class="row"><button class="btn ghost" id="al-cancel">Отмена</button><button class="btn dark" id="al-save">Сохранить</button></div>
      </div>
    </div>`;
  document.body.appendChild(dlg);
  dlg.showModal();
  dlg.addEventListener("close", () => dlg.remove());
  dlg.addEventListener("click", (ev) => { if (ev.target === dlg) dlg.close(); });

  $$(".day-picker button", dlg).forEach((b) =>
    b.addEventListener("click", () => b.classList.toggle("on"))
  );
  $("#al-dur", dlg).addEventListener("input", (e) => { $("#al-dur-out", dlg).textContent = e.target.value + " мин"; });
  $("#al-bright", dlg).addEventListener("input", (e) => { $("#al-bright-out", dlg).textContent = pct(+e.target.value) + "%"; });

  const grab = () => {
    const [h, m] = ($("#al-time", dlg).value || "07:00").split(":").map(Number);
    let days = 0;
    $$(".day-picker button", dlg).forEach((b, i) => { if (b.classList.contains("on")) days |= 1 << i; });
    return {
      enabled: $("#al-enabled", dlg).checked,
      days,
      hour: h,
      minute: m,
      durationMin: +$("#al-dur", dlg).value,
      maxBrightness: +$("#al-bright", dlg).value,
      snoozeMin: +$("#al-snooze", dlg).value,
      afterOffMin: +$("#al-off", dlg).value,
    };
  };

  $("#al-cancel", dlg).addEventListener("click", () => dlg.close());
  const testBtn = $("#al-test", dlg);
  if (testBtn) testBtn.addEventListener("click", () => { doAction("sunrise_test"); dlg.close(); });
  $("#al-save", dlg).addEventListener("click", () => {
    const alarms = state.s ? cloneAlarms() : [];
    const val = grab();
    if (existing && idx >= 0) alarms[idx] = { ...alarms[idx], ...val };
    else alarms.push(val);
    if (alarms.length > 4) alarms.length = 4;
    saveAlarms(alarms);
    dlg.close();
  });
  const del = $("#al-del", dlg);
  if (del) del.addEventListener("click", () => {
    const alarms = cloneAlarms();
    alarms.splice(idx, 1);
    saveAlarms(alarms);
    dlg.close();
  });
}

/* ---------------- clock page ---------------- */

function renderClock() {
  return `
  <div class="page-head">
    <div><h1>Часы</h1><p>Время бегущей строкой вокруг цилиндра</p></div>
    <button class="btn dark" id="show-clock">Показать часы</button>
  </div>
  <div class="stack">
    <div class="card">
      <h2>Время <span class="badge" id="cl-src">—</span></h2>
      <p class="sub">Сейчас на лампе: <span class="mono" id="cl-now">—</span></p>
      <div class="row" style="flex-wrap:wrap;gap:10px;margin-top:10px">
        <input type="date" class="text-input" id="cl-date-in" style="max-width:170px">
        <input type="time" class="text-input" id="cl-time-in" style="max-width:130px">
        <button class="btn dark" id="cl-set">Установить</button>
      </div>
      <div class="row" style="flex-wrap:wrap;gap:10px;margin-top:10px">
        <button class="btn ghost small" id="cl-fill-now">Подставить текущее</button>
        <button class="btn ghost small" id="cl-ntp">Автосинхронизация NTP</button>
      </div>
      <p class="hint" id="cl-manual-hint" style="margin-top:10px"></p>
    </div>

    <div class="card">
      <h2>Часовой пояс</h2><p class="sub">Влияет на часы, будильник и новогоднее шоу</p>
      <p class="tz-current"><span class="mono" id="tz-current">—</span></p>
      <input class="text-input" id="tz-search" placeholder="Поиск: Москва, Екатеринбург, Berlin…" autocomplete="off" style="margin:10px 0">
      <div class="tz-list" id="tz-list"></div>
      <p class="hint" style="margin-top:10px">Зон в списке: <span id="tz-count">—</span>. Список часовых поясов приходит с лампы.</p>
    </div>

    <div class="card">
      <div class="setting-row"><div class="label"><strong>Показывать дату</strong><span>День недели и число после времени</span></div>
        <label class="switch"><input type="checkbox" id="cl-date"><span class="track"></span></label></div>
      <div class="setting-row"><div class="label"><strong>12-часовой формат</strong><span>Вместо 24-часового</span></div>
        <label class="switch"><input type="checkbox" id="cl-12"><span class="track"></span></label></div>
      <div class="setting-row" style="align-items:flex-start">
        <div class="label"><strong>Цвет строки</strong><span>Радуга переливается по буквам</span></div>
        <div class="segmented" id="cl-color"><button data-mode="0">Выбранный</button><button data-mode="1">Радуга</button></div>
      </div>
      <div class="setting-row" style="display:block">
        <div class="range-label" style="margin-bottom:6px"><span class="label"><strong>Скорость</strong></span><span id="cl-speed-out">—</span></div>
        <input type="range" id="cl-speed" min="10" max="150" value="90">
      </div>
    </div>
    <div class="card">
      <h2>Новогоднее шоу</h2><p class="sub">Салют и поздравление — в полночь 1 января включится само</p>
      <div class="row" style="flex-wrap:wrap">
        <button class="btn accent" id="ny-play">Запустить сейчас</button>
        <a class="btn ghost" href="#/effects">Эффект «До Нового года»</a>
      </div>
      <p class="hint" id="ny-days" style="margin-top:12px"></p>
    </div>
  </div>`;
}

function syncClock(s) {
  const d = $("#cl-date");
  if (!d) return;
  d.checked = !!s.clock.showDate;
  $("#cl-12").checked = !!s.clock.hour12;
  $("#cl-speed").value = s.clock.speed;
  $("#cl-speed").style.setProperty("--fill", Math.round((s.clock.speed - 10) / 140 * 100) + "%");
  $("#cl-speed-out").textContent = s.clock.speed;
  $$("#cl-color button").forEach((b) => b.classList.toggle("active", +b.dataset.mode === s.clock.colorMode));

  $("#cl-now").textContent = (s.device && s.device.time ? s.device.time.replace("T", " ") : "—");
  const badge = $("#cl-src");
  badge.textContent = s.manualTime ? "ручное время" : "NTP";
  badge.classList.toggle("manual", !!s.manualTime);
  $("#cl-manual-hint").textContent = s.manualTime
    ? "Время идёт вручную: после перезагрузки лампа восстановит последнее сохранённое значение. Для точного хода верните автосинхронизацию NTP."
    : "Время синхронизируется с интернетом (NTP) при каждом подключении к Wi-Fi.";

  $("#tz-current").textContent = s.tz;
  $("#tz-count").textContent = state.tzones.length || "—";
  if ($("#tz-list").children.length) {
    $$("#tz-list .tz-item").forEach((b) => b.classList.toggle("active", b.dataset.tz === s.tz));
  }

  const days = s.ny ? s.ny.days : -1;
  $("#ny-days").textContent = days > 0 ? `До Нового года ${days} дн.` : days === 0 ? "Новый год уже сегодня!" : "";
}

function clockZone() {
  return state.s && state.s.tz ? state.s.tz : "";
}

function tzOffsetAt(zone, utcMs) {
  const dtf = new Intl.DateTimeFormat("en-US", {
    timeZone: zone,
    hour12: false,
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
  const p = {};
  for (const part of dtf.formatToParts(new Date(utcMs))) p[part.type] = part.value;
  const asUTC = Date.UTC(+p.year, +p.month - 1, +p.day, +p.hour % 24, +p.minute, +p.second);
  return (asUTC - utcMs) / 1000;
}

function epochFromParts(dateStr, timeStr) {
  const [y, mo, d] = dateStr.split("-").map(Number);
  const [h, mi] = timeStr.split(":").map(Number);
  const zone = clockZone();
  if (zone.includes("/")) {
    try {
      const naive = Date.UTC(y, mo - 1, d, h, mi, 0);
      let ts = naive - tzOffsetAt(zone, naive) * 1000;
      ts = naive - tzOffsetAt(zone, ts) * 1000;
      return Math.floor(ts / 1000);
    } catch {
      /* неизвестная зона — падаем в локальное время браузера */
    }
  }
  return Math.floor(new Date(y, mo - 1, d, h, mi, 0).getTime() / 1000);
}

function fillClockInputsNow() {
  const zone = clockZone();
  const now = new Date();
  let parts;
  try {
    const dtf = new Intl.DateTimeFormat("en-CA", {
      timeZone: zone.includes("/") ? zone : undefined,
      hourCycle: "h23",
      year: "numeric",
      month: "2-digit",
      day: "2-digit",
      hour: "2-digit",
      minute: "2-digit",
    });
    parts = {};
    for (const p of dtf.formatToParts(now)) parts[p.type] = p.value;
  } catch {
    parts = null;
  }
  if (parts) {
    $("#cl-date-in").value = `${parts.year}-${parts.month}-${parts.day}`;
    $("#cl-time-in").value = `${parts.hour}:${parts.minute}`;
  } else {
    const p = (n) => String(n).padStart(2, "0");
    $("#cl-date-in").value = `${now.getFullYear()}-${p(now.getMonth() + 1)}-${p(now.getDate())}`;
    $("#cl-time-in").value = `${p(now.getHours())}:${p(now.getMinutes())}`;
  }
}

async function loadTzones() {
  if (state.tzones.length) return;
  try {
    const list = await api("/api/timezones");
    if (Array.isArray(list) && list.length) {
      state.tzones = list;
      renderTzList();
      $("#tz-count").textContent = list.length;
    }
  } catch {
    state.tzones = ["Europe/Moscow", "Europe/Berlin", "Asia/Yekaterinburg", "Asia/Novosibirsk", "Asia/Vladivostok", "UTC"];
    renderTzList();
    $("#tz-count").textContent = state.tzones.length + " (резерв)";
  }
}

function renderTzList() {
  const list = $("#tz-list");
  if (!list) return;
  const q = ($("#tz-search").value || "").trim().toLowerCase();
  const cur = clockZone();
  const items = state.tzones
    .map((id) => ({ id, label: id.replace(/_/g, " ") }))
    .filter((z) => !q || z.label.toLowerCase().includes(q) || z.id.toLowerCase().includes(q));
  list.innerHTML = items.length
    ? items.map((z) => `<button class="tz-item ${z.id === cur ? "active" : ""}" data-tz="${z.id}">${z.label}</button>`).join("")
    : `<p class="hint" style="padding:8px">Ничего не нашлось</p>`;
}

function bindClock() {
  const d = $("#cl-date");
  if (!d) return;
  d.addEventListener("change", () => postPatch({ clock: { showDate: d.checked } }));
  $("#cl-12").addEventListener("change", (e) => postPatch({ clock: { hour12: e.target.checked } }));
  const sp = $("#cl-speed");
  let t = null;
  sp.addEventListener("input", () => {
    $("#cl-speed").style.setProperty("--fill", Math.round((sp.value - 10) / 140 * 100) + "%");
    $("#cl-speed-out").textContent = sp.value;
    clearTimeout(t);
    t = setTimeout(() => postPatch({ clock: { speed: +sp.value } }), 140);
  });
  $("#cl-color").addEventListener("click", (ev) => {
    const b = ev.target.closest("[data-mode]");
    if (b) postPatch({ clock: { colorMode: +b.dataset.mode } });
  });
  $("#show-clock").addEventListener("click", () => postPatch({ effect: "clock", power: true }));
  $("#ny-play").addEventListener("click", () => doAction("nyshow"));

  $("#cl-fill-now").addEventListener("click", fillClockInputsNow);
  $("#cl-set").addEventListener("click", () => {
    const date = $("#cl-date-in").value;
    const time = $("#cl-time-in").value;
    if (!date || !time) return toast("Укажите дату и время", true);
    const epoch = epochFromParts(date, time);
    if (!epoch || epoch < 1700000000) return toast("Некорректная дата", true);
    doAction("settime", { epoch });
    toast("Время установлено вручную");
  });
  $("#cl-ntp").addEventListener("click", () => {
    doAction("timens");
    toast("Автосинхронизация времени включена");
  });

  $("#tz-search").addEventListener("input", renderTzList);
  $("#tz-list").addEventListener("click", (ev) => {
    const b = ev.target.closest("[data-tz]");
    if (!b) return;
    postPatch({ tz: b.dataset.tz });
    toast("Часовой пояс: " + b.dataset.tz.replace(/_/g, " "));
  });

  loadTzones();
}

/* ---------------- settings page ---------------- */

function renderSettings() {
  return `
  <div class="page-head"><div><h1>Настройки</h1><p>Устройство, калибровка и обновления</p></div></div>
  <div class="grid cols-2">
    <div class="stack">
      <div class="card">
        <h2>Устройство</h2><p class="sub">Карточка лампы</p>
        <dl class="kv">
          <dt>Имя</dt><dd>lampa.local</dd>
          <dt>IP-адрес</dt><dd id="d-ip" class="mono">—</dd>
          <dt>Wi-Fi</dt><dd id="d-rssi">—</dd>
          <dt>Прошивка</dt><dd id="d-fw">—</dd>
          <dt>Свободно памяти</dt><dd id="d-heap" class="mono">—</dd>
          <dt>Аптайм</dt><dd id="d-up" class="mono">—</dd>
          <dt>Время лампы</dt><dd id="d-time" class="mono">—</dd>
          <dt>MAC</dt><dd id="d-mac" class="mono">—</dd>
        </dl>
      </div>

      <div class="card">
        <h2>Калибровка матрицы</h2><p class="sub">Если картинка «не туда» — включите узор и подберите ориентацию</p>
        <div class="row" style="flex-wrap:wrap">
          <button class="btn small" data-calib="grid">Сетка</button>
          <button class="btn small" data-calib="chase">Бегущая точка</button>
          <button class="btn small ghost" data-calib="off">Стоп</button>
        </div>
        <div class="divider" style="margin:16px 0"></div>
        <div class="setting-row"><div class="label"><strong>Змейка</strong><span>Чётные ряды идут в обратную сторону</span></div>
          <label class="switch"><input type="checkbox" id="map-serp"><span class="track"></span></label></div>
        <div class="setting-row"><div class="label"><strong>Зеркало</strong><span>Обратное направление ленты</span></div>
          <label class="switch"><input type="checkbox" id="map-rev"><span class="track"></span></label></div>
        <div class="setting-row"><div class="label"><strong>Переворот по высоте</strong><span>Если огонь «стекает» вниз</span></div>
          <label class="switch"><input type="checkbox" id="map-flip"><span class="track"></span></label></div>
        <div class="setting-row"><div class="label"><strong>Сдвиг по кругу</strong><span>Поворот картинки вокруг трубки</span></div>
          <div class="row">
            <button class="btn small ghost" id="off-minus">−</button>
            <span class="mono" id="map-offset">0</span>
            <button class="btn small ghost" id="off-plus">+</button>
          </div>
        </div>
      </div>

      <div class="card">
        <h2>Wi-Fi</h2><p class="sub">Смена сети — лампа перезагрузится</p>
        <div class="stack">
          <input class="text-input" id="wifi-ssid" placeholder="Имя сети (SSID)" autocomplete="off">
          <input class="text-input" id="wifi-pass" placeholder="Пароль" type="password" autocomplete="off">
          <button class="btn dark" id="wifi-save">Подключить</button>
        </div>
      </div>
    </div>

    <div class="stack">
      <div class="card">
        <h2>Питание</h2><p class="sub">Лимит тока блока питания</p>
        <div class="brightness">
          <div class="brightness-head"><label for="pl-limit">Ограничение тока</label><output id="pl-out">—</output></div>
          <input type="range" id="pl-limit" min="500" max="15000" step="250" value="9000">
        </div>
        <p class="hint" style="margin-top:10px">Если расчётный ток превысит лимит, лампа сама приглушит матрицу. Все 256 светодиодов на полной яркости — примерно 15.4 А.</p>
      </div>

      <div class="card">
        <h2>Обновление прошивки</h2><p class="sub">Вручную из файла или онлайн с GitHub</p>
        <div class="update-head">
          <div class="label"><strong>Версия прошивки</strong><span class="mono" id="fw-current">—</span></div>
          <button class="btn small dark" id="gh-check">Проверить обновления</button>
        </div>
        <p class="hint" id="gh-state" style="margin-top:10px"></p>
        <button class="btn accent hidden" id="gh-install" style="margin-top:10px">Обновить с GitHub</button>
        <div class="ota-progress" style="margin-top:10px"><span id="gh-bar"></span></div>
        <div class="divider" style="margin:16px 0"></div>
        <div class="stack">
          <div class="file-field">
            <input type="file" id="ota-file" accept=".bin" class="file-input">
            <label class="btn ghost small" for="ota-file">Выбрать файл</label>
            <span class="file-name" id="ota-name">Прошивка не выбрана</span>
          </div>
          <button class="btn accent" id="ota-btn">Обновить из файла</button>
          <div class="ota-progress"><span id="ota-bar"></span></div>
          <p class="hint" id="ota-state"></p>
        </div>
      </div>

      <div class="card">
        <h2>Сброс</h2><p class="sub">Удалить настройки и вернуться к точке доступа</p>
        <button class="btn danger" id="factory-btn">Сбросить до заводских</button>
      </div>
    </div>
  </div>`;
}

function syncSettings(s) {
  const dev = s.device;
  if (!dev) return;
  $("#d-ip").textContent = dev.ip || "—";
  const r = dev.rssi;
  $("#d-rssi").textContent = s.device.ap ? "точка доступа" : r ? `${r} dBm` : "—";
  $("#d-fw").textContent = "v" + dev.fw;
  $("#d-heap").textContent = Math.round(dev.heap / 1024) + " КБ";
  $("#d-up").textContent = fmtUptime(dev.uptime);
  $("#d-time").textContent = (dev.time || "").replace("T", " ").replace(/\.\d+.*$/, "");
  $("#d-mac").textContent = dev.mac || "—";
  $("#map-serp").checked = !!s.mapping.serpentine;
  $("#map-rev").checked = !!s.mapping.reverse;
  $("#map-flip").checked = !!s.mapping.flipY;
  $("#map-offset").textContent = s.mapping.offset;
  $("#fw-current").textContent = "v" + dev.fw;
  const pl = $("#pl-limit");
  if (pl) {
    if (document.activeElement !== pl) pl.value = s.currentLimit;
    pl.style.setProperty("--fill", Math.round((s.currentLimit - 500) / 14500 * 100) + "%");
    $("#pl-out").textContent = fmtAmps(s.currentLimit);
  }
}

function fmtAmps(ma) {
  return (ma / 1000).toFixed(1) + " А";
}

function bindSettings() {
  const bindToggle = (sel, fn) => {
    const el = $(sel);
    if (el) el.addEventListener("change", () => fn(el.checked));
  };
  bindToggle("#map-serp", (v) => postPatch({ mapping: { serpentine: v } }));
  bindToggle("#map-rev", (v) => postPatch({ mapping: { reverse: v } }));
  bindToggle("#map-flip", (v) => postPatch({ mapping: { flipY: v } }));

  const minus = $("#off-minus");
  const plus = $("#off-plus");
  if (minus && plus) {
    minus.addEventListener("click", () => {
      const v = ((state.s?.mapping.offset ?? 0) + 15) % 16;
      postPatch({ mapping: { offset: v } });
    });
    plus.addEventListener("click", () => {
      const v = ((state.s?.mapping.offset ?? 0) + 1) % 16;
      postPatch({ mapping: { offset: v } });
    });
  }

  const ghCheck = $("#gh-check");
  if (ghCheck) {
    ghCheck.addEventListener("click", checkFirmwareUpdates);
    $("#gh-install").addEventListener("click", installFromGithub);
  }

  const pl = $("#pl-limit");
  if (pl) {
    let t = null;
    pl.addEventListener("input", () => {
      pl.style.setProperty("--fill", Math.round((pl.value - 500) / 14500 * 100) + "%");
      $("#pl-out").textContent = fmtAmps(+pl.value);
      clearTimeout(t);
      t = setTimeout(() => postPatch({ currentLimit: +pl.value }), 150);
    });
  }

  const wifiBtn = $("#wifi-save");
  if (wifiBtn) {
    wifiBtn.addEventListener("click", async () => {
      const ssid = $("#wifi-ssid").value.trim();
      const pass = $("#wifi-pass").value;
      if (!ssid) return toast("Укажите имя сети", true);
      try {
        await api("/api/wifi", "POST", { ssid, pass });
        toast("Лампа перезагружается и подключается…");
      } catch {
        toast("Не удалось сохранить сеть", true);
      }
    });
  }

  const otaBtn = $("#ota-btn");
  if (otaBtn) {
    const otaFile = $("#ota-file");
    otaFile.addEventListener("change", () => {
      const f = otaFile.files[0];
      const name = $("#ota-name");
      if (name) name.textContent = f ? f.name : "Прошивка не выбрана";
    });
    otaBtn.addEventListener("click", () => {
      const file = $("#ota-file").files[0];
      if (!file) return toast("Выберите .bin файл", true);
      const fd = new FormData();
      fd.append("firmware", file);
      const xhr = new XMLHttpRequest();
      xhr.open("POST", "/api/ota");
      xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
          const p = Math.round((e.loaded / e.total) * 100);
          $("#ota-bar").style.width = p + "%";
          $("#ota-state").textContent = `Загрузка… ${p}%`;
        }
      };
      xhr.onload = () => {
        if (xhr.status === 200) {
          $("#ota-state").textContent = "Обновлено, лампа перезагружается…";
          toast("Прошивка обновлена");
        } else {
          $("#ota-state").textContent = "Ошибка обновления";
          toast("Ошибка обновления", true);
        }
      };
      xhr.onerror = () => toast("Ошибка сети при обновлении", true);
      $("#ota-state").textContent = "Загрузка…";
      $("#ota-bar").style.width = "0%";
      $("#ota-bar").parentElement.classList.add("on");
      xhr.send(fd);
    });
  }

  const factory = $("#factory-btn");
  if (factory) {
    factory.addEventListener("click", async () => {
      if (!confirm("Сбросить лампу до заводских настроек? Wi-Fi придётся настроить заново.")) return;
      try {
        await api("/api/action", "POST", { action: "factory" });
        toast("Сброс выполнен, лампа перезагружается");
      } catch {
        toast("Не удалось сбросить", true);
      }
    });
  }
}

/* ---------------- firmware update ---------------- */

async function checkFirmwareUpdates() {
  const out = $("#gh-state");
  const btn = $("#gh-check");
  if (!out) return;
  out.textContent = "Проверяю GitHub…";
  $("#gh-install").classList.add("hidden");
  btn.disabled = true;
  try {
    const r = await api("/api/update/check");
    state.update = r;
    if (r.error) {
      out.textContent = "Не удалось проверить: " + r.error;
    } else if (!r.latest) {
      out.textContent = "На GitHub пока нет релизов.";
    } else if (!r.available) {
      out.textContent = `Установлена последняя версия (v${r.current}).`;
    } else {
      out.textContent = `Доступна ${r.latest} — у вас v${r.current}.`;
      const inst = $("#gh-install");
      inst.classList.remove("hidden");
      inst.textContent = `Обновить до ${r.latest}`;
    }
  } catch {
    out.textContent = "Лампа не ответила на запрос обновления.";
  } finally {
    btn.disabled = false;
  }
}

async function installFromGithub() {
  try {
    await api("/api/update/install", "POST", {});
    $("#gh-install").classList.add("hidden");
    $("#gh-state").textContent = "Соединяюсь с GitHub…";
    $("#gh-bar").style.width = "2%";
    $("#gh-bar").parentElement.classList.add("on");
    toast("Обновление запущено");
  } catch {
    toast("Не удалось запустить обновление", true);
  }
}

function updateOtaUi(m) {
  const bar = $("#gh-bar");
  const st = $("#gh-state");
  const pct = Math.max(m.progress || 0, m.phase === "connecting" ? 2 : 0);
  if (bar) bar.style.width = Math.min(100, pct) + "%";
  if (!st) return;
  const texts = {
    connecting: "Соединяюсь с GitHub…",
    downloading: "Загрузка прошивки…",
    finalizing: "Проверяю и записываю…",
    done: "Готово! Лампа перезагружается…",
    error: "Ошибка: " + (m.error || "неизвестная"),
  };
  const text = texts[m.phase];
  if (text) st.textContent = m.phase === "downloading" ? `${text} ${m.progress}%` : text;
  if (m.phase === "done") toast("Прошивка обновлена");
  if (m.phase === "error") toast("Ошибка обновления: " + (m.error || ""), true);
}

/* ---------------- sunrise banner ---------------- */

function updateSunrise(sr) {
  const banner = $("#sunrise-banner");
  if (!banner) return;
  if (!sr || !sr.active) {
    banner.classList.add("hidden");
    return;
  }
  banner.classList.remove("hidden");
  $("#sunrise-title").textContent = sr.snoozing ? "Отсрочка" : "Рассвет";
  $("#sunrise-sub").textContent = sr.snoozing ? "лампа снова разгорится через несколько минут" : "плавное пробуждение в процессе";
  $("#sunrise-bar").style.width = Math.round((sr.progress || 0) * 100) + "%";
}

/* ---------------- boot ---------------- */

async function boot() {
  initTheme();
  $("#theme-btn").addEventListener("click", toggleTheme);
  $("#sunrise-snooze").addEventListener("click", () => doAction("sunrise_snooze"));
  $("#sunrise-stop").addEventListener("click", () => doAction("sunrise_stop"));
  window.addEventListener("hashchange", route);
  $("#view").addEventListener("click", (ev) => {
    const calib = ev.target.closest("[data-calib]");
    if (calib) doAction("calibrate", { pattern: calib.dataset.calib, ms: 20000 });
  });
  route();
  try {
    const [effects, st] = await Promise.all([api("/api/effects"), api("/api/state")]);
    state.effects = effects;
    applyState(st);
  } catch {
    toast("Лампа недоступна. Проверьте Wi-Fi и адрес.", true);
    const view = $("#view");
    view.insertAdjacentHTML(
      "afterbegin",
      `<div class="offline-bar">Нет связи с лампой. Откройте страницу по адресу устройства (lampa.local) или обновите.</div>`
    );
  }
  connectWs();
}

document.addEventListener("DOMContentLoaded", boot);
