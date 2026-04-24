#!/usr/bin/env node
// Send a file to a WhatsApp contact or group via Baileys
// Usage: node send.mjs <jid-or-phone> <file-path> [caption]
//
// JID forms accepted:
//   120363401234567890@g.us        group
//   41791234567@s.whatsapp.net     individual
//   41791234567                    individual (shorthand, auto-appended @s.whatsapp.net)
//
// File handling:
//   .png/.jpg/.jpeg → sent as image (with caption)
//   anything else    → sent as document (firmware .bin, .csv, .log, ...)
//
// First run: scan QR code with WhatsApp → session saved in auth/
// (or run `node login.mjs` first)

import makeWASocket, {
  useMultiFileAuthState,
  makeCacheableSignalKeyStore,
  fetchLatestBaileysVersion,
  DisconnectReason,
} from "@whiskeysockets/baileys";
import qrcode from "qrcode-terminal";
import pino from "pino";
import { readFileSync, existsSync, rmSync, statSync } from "fs";
import { resolve, dirname, basename, extname } from "path";
import { fileURLToPath } from "url";

const __dirname = dirname(fileURLToPath(import.meta.url));
const AUTH_DIR = resolve(__dirname, "auth");
const logger = pino({ level: "silent" });

const [,, jidArg, filePath, ...captionParts] = process.argv;
const caption = captionParts.join(" ") || "";

if (!jidArg || !filePath) {
  console.error("Usage: node send.mjs <jid-or-phone> <file-path> [caption]");
  console.error("Examples:");
  console.error("  node send.mjs 41791234567 ./firmware.bin 'New build'");
  console.error("  node send.mjs 120363401234567890@g.us ./png/plot.png 'Session 1'");
  process.exit(1);
}

const absPath = resolve(filePath);
if (!existsSync(absPath)) {
  console.error(`File not found: ${absPath}`);
  process.exit(1);
}

const fileName = basename(absPath);
const ext = extname(absPath).toLowerCase();
const isImage = ext === ".png" || ext === ".jpg" || ext === ".jpeg";

const mimeByExt = {
  ".png":  "image/png",
  ".jpg":  "image/jpeg",
  ".jpeg": "image/jpeg",
  ".bin":  "application/octet-stream",
  ".csv":  "text/csv",
  ".log":  "text/plain",
  ".txt":  "text/plain",
  ".pdf":  "application/pdf",
  ".zip":  "application/zip",
  ".gif":  "image/gif",
  ".mov":  "video/quicktime",
  ".mp4":  "video/mp4",
};
const mime = mimeByExt[ext] || "application/octet-stream";

// JID normalisation: add default domain when user passed a bare phone number.
let jid;
if (jidArg.includes("@")) {
  jid = jidArg;
} else if (/^\d+$/.test(jidArg)) {
  jid = `${jidArg}@s.whatsapp.net`;
} else {
  console.error(`Cannot parse JID: ${jidArg}`);
  console.error("Provide either <phone-digits> or <jid>@<domain>");
  process.exit(1);
}

const fileSize = statSync(absPath).size;
console.log(`Target : ${jid}`);
console.log(`File   : ${fileName} (${fileSize} bytes, ${mime})`);
console.log(`Mode   : ${isImage ? "image" : "document"}${caption ? ` + caption` : ""}`);

let retries = 0;
const MAX_RETRIES = 3;
let done = false;

async function connect() {
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

  return new Promise((resolvePromise, reject) => {
    const timeout = setTimeout(() => {
      sock.end();
      reject(new Error("Connection timeout (90s)"));
    }, 90000);

    sock.ev.on("connection.update", async (update) => {
      const { connection, lastDisconnect, qr } = update;

      if (qr) {
        console.log("\n  Scan this QR code with WhatsApp:");
        console.log("  (Settings > Linked Devices > Link a Device)\n");
        qrcode.generate(qr, { small: true });
        console.log("  Waiting for scan...\n");
      }

      if (connection === "open") {
        clearTimeout(timeout);
        try {
          console.log(`Connected. Sending to ${jid}...`);

          const buffer = readFileSync(absPath);
          const message = isImage
            ? { image: buffer, caption, mimetype: mime }
            : { document: buffer, fileName, mimetype: mime, caption };

          const sendResult = await Promise.race([
            sock.sendMessage(jid, message),
            new Promise((_, rej) =>
              setTimeout(() => rej(new Error("sendMessage timeout (60s)")), 60000)
            ),
          ]);

          console.log("Sent!", sendResult?.key?.id ? `(id: ${sendResult.key.id})` : "");
          done = true;
          // Force exit — sock.end() can hang via close handler chain.
          setTimeout(() => process.exit(0), 1000);
        } catch (err) {
          console.error("Send error:", err.message);
          sock.end();
          reject(err);
        }
      }

      if (connection === "close") {
        clearTimeout(timeout);
        if (done) {
          resolvePromise();
          return;
        }
        const statusCode = lastDisconnect?.error?.output?.statusCode;

        if (statusCode === DisconnectReason.loggedOut) {
          console.log("Session expired. Clearing auth, please scan again...");
          rmSync(AUTH_DIR, { recursive: true, force: true });
          retries = 0;
          connect().then(resolvePromise).catch(reject);
        } else if (retries < MAX_RETRIES) {
          retries++;
          connect().then(resolvePromise).catch(reject);
        } else {
          reject(new Error(`Connection failed after ${MAX_RETRIES} retries (status: ${statusCode})`));
        }
      }
    });
  });
}

connect()
  .then(() => process.exit(0))
  .catch((err) => {
    console.error("Error:", err.message);
    process.exit(1);
  });
