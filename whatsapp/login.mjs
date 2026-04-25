#!/usr/bin/env node
// WhatsApp login — scan QR code, save session
// Usage: node login.mjs [--force]

import makeWASocket, {
  useMultiFileAuthState,
  makeCacheableSignalKeyStore,
  fetchLatestBaileysVersion,
} from "@whiskeysockets/baileys";
import qrcode from "qrcode-terminal";
import pino from "pino";
import { resolve, dirname } from "path";
import { fileURLToPath } from "url";
import { rmSync, existsSync, readdirSync, statSync } from "fs";

const __dirname = dirname(fileURLToPath(import.meta.url));
const AUTH_DIR = resolve(__dirname, "auth");
const logger = pino({ level: "silent" });

const force = process.argv.includes("--force");

if (force && existsSync(AUTH_DIR)) {
  console.log("Lösche alte Session (--force)...");
  rmSync(AUTH_DIR, { recursive: true, force: true });
}

async function startSocket() {
  const { state, saveCreds } = await useMultiFileAuthState(AUTH_DIR);
  const { version } = await fetchLatestBaileysVersion();

  console.log(`WhatsApp version: ${version.join(".")}`);

  const sock = makeWASocket({
    auth: {
      creds: state.creds,
      keys: makeCacheableSignalKeyStore(state.keys, logger),
    },
    version,
    logger,
    browser: ["FP-SNS-STBOX1", "CLI", "1.0"],
    syncFullHistory: false,
    markOnlineOnConnect: false,
  });

  sock.ev.on("creds.update", saveCreds);
  return sock;
}

async function loginOnce() {
  const sock = await startSocket();

  return new Promise((resolve) => {
    let done = false;
    const finish = (result) => {
      if (done) return;
      done = true;
      clearTimeout(timeout);
      resolve(result);
    };
    const timeout = setTimeout(() => {
      sock.end();
      finish({ ok: false, msg: "Timeout (120s) — kein QR-Code gescannt" });
    }, 120000);

    sock.ev.on("connection.update", (update) => {
      const { connection, qr, lastDisconnect } = update;

      if (qr) {
        console.log("\n  Scan this QR code with WhatsApp:");
        console.log("  (Settings > Linked Devices > Link a Device)\n");
        qrcode.generate(qr, { small: true });
      }

      if (connection === "open") {
        console.log("\n  Connected. Warte auf creds-flush...");
        // saveCreds writes creds.json asynchronously on `creds.update`.
        // If we sock.end() too early, the writeFile promise gets cancelled
        // and we end up with a 0-byte creds.json even though the pair
        // succeeded. Poll the file size, accept "successful" only once
        // creds.json is populated (typical < 2 s after open).
        const credsPath = resolve(AUTH_DIR, "creds.json");
        const tStart = Date.now();
        const poll = setInterval(() => {
          let size = 0;
          try { size = statSync(credsPath).size; } catch {}
          if (size > 100) {
            clearInterval(poll);
            console.log(`  creds.json = ${size} bytes — Login erfolgreich!`);
            console.log(`  Auth: ${AUTH_DIR}\n`);
            finish({ ok: true });
            setTimeout(() => sock.end(), 300);
          } else if (Date.now() - tStart > 15000) {
            clearInterval(poll);
            console.log(`  creds.json noch leer nach 15 s — Abbruch`);
            finish({ ok: false, msg: "creds flush timeout" });
            sock.end();
          }
        }, 250);
      }

      if (connection === "close") {
        const err = lastDisconnect?.error;
        const code = err?.output?.statusCode;
        const msg = err?.message || "unbekannt";
        finish({ ok: false, code, msg });
      }
    });
  });
}

async function main() {
  // Baileys' post-QR handshake always closes once with status 515
  // ("restart required") and sometimes comes back with no status code at
  // all. Retry on *any* close until we see a real "open" or we've looped
  // too many times — the only unrecoverable case is a deliberate logout
  // (401/403), where we wipe and start over with a fresh QR.
  const MAX_ATTEMPTS = 6;
  let result = { ok: false };
  for (let attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    result = await loginOnce();

    if (result.ok) break;

    console.log(`Versuch ${attempt}: geschlossen (Code: ${result.code ?? "?"}, ${result.msg})`);

    if (result.code === 401 || result.code === 403) {
      console.log("Session auf Telefon ungültig — lösche und starte Neu-Login...\n");
      if (existsSync(AUTH_DIR)) {
        rmSync(AUTH_DIR, { recursive: true, force: true });
      }
      continue;
    }

    // Any other close (515 restart required, undefined code after QR scan,
    // transient network) — just reconnect with the same auth dir.
    console.log("Reconnecting...\n");
  }

  if (!result.ok) throw new Error(`Login fehlgeschlagen nach ${MAX_ATTEMPTS} Versuchen`);

  const files = existsSync(AUTH_DIR) ? readdirSync(AUTH_DIR) : [];
  console.log(`Session-Dateien: ${files.length}`);
}

main()
  .then(() => process.exit(0))
  .catch((err) => {
    console.error("Error:", err.message);
    process.exit(1);
  });
