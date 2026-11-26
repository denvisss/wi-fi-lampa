/* Копирует веб-интерфейс в firmware/data и готовит gzip-версии. */
import fs from "node:fs";
import path from "node:path";
import zlib from "node:zlib";
import { fileURLToPath } from "node:url";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const src = path.join(root, "web");
const dst = path.join(root, "firmware", "data");
const FILES = ["index.html", "setup.html", "styles.css", "app.js"];

fs.mkdirSync(dst, { recursive: true });
for (const f of fs.readdirSync(dst)) {
  if (f.endsWith(".gz") || FILES.includes(f)) fs.rmSync(path.join(dst, f));
}

let total = 0;
for (const name of FILES) {
  const data = fs.readFileSync(path.join(src, name));
  fs.writeFileSync(path.join(dst, name), data);
  const gz = zlib.gzipSync(data, { level: 9 });
  fs.writeFileSync(path.join(dst, name + ".gz"), gz);
  total += gz.length;
  console.log(`${name.padEnd(12)} ${String(data.length).padStart(7)} B  →  gzip ${String(gz.length).padStart(6)} B`);
}
console.log(`\nEmbedded ${FILES.length} files into firmware/data (${(total / 1024).toFixed(1)} KB gzip).`);
