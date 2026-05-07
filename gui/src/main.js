import { invoke } from "@tauri-apps/api/core";
import { open } from "@tauri-apps/plugin-dialog";
import { Command } from "@tauri-apps/plugin-shell";
import { initTimeline } from "./timeline.js";
import { initPreview } from "./preview.js";

// Tab navigation
document.querySelectorAll(".nav-tabs button[data-page]").forEach((btn) => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".nav-tabs button[data-page]").forEach(b => b.classList.remove("active"));
    document.querySelectorAll(".page").forEach(p => p.classList.remove("active"));
    btn.classList.add("active");
    document.getElementById(btn.dataset.page).classList.add("active");
  });
});

// Theme toggle
const themeBtn = document.getElementById("theme-toggle");
themeBtn?.addEventListener("click", () => {
  document.body.classList.toggle("light-theme");
  themeBtn.textContent = document.body.classList.contains("light-theme") ? "☀️" : "🌙";
  localStorage.setItem("theme", document.body.classList.contains("light-theme") ? "light" : "dark");
});
// Restore saved theme
if (localStorage.getItem("theme") === "light") {
  document.body.classList.add("light-theme");
  if (themeBtn) themeBtn.textContent = "☀️";
}

// Initialize timeline & preview
initTimeline();
initPreview();

let videoDir = "";
let audioFile = "";
let outputDir = "";

document.getElementById("select-video").addEventListener("click", async () => {
  const selected = await open({ directory: true, title: "Select J2K Directory" });
  if (selected) {
    videoDir = selected;
    document.getElementById("video-path").textContent = selected;
  }
});

document.getElementById("select-audio").addEventListener("click", async () => {
  const selected = await open({
    filters: [{ name: "WAV Audio", extensions: ["wav"] }],
    title: "Select Audio File",
  });
  if (selected) {
    audioFile = selected;
    document.getElementById("audio-path").textContent = selected;
  }
});

document.getElementById("select-output").addEventListener("click", async () => {
  const selected = await open({ directory: true, title: "Select Output Directory" });
  if (selected) {
    outputDir = selected;
    document.getElementById("output-path").textContent = selected;
  }
});

document.getElementById("create-form").addEventListener("submit", async (e) => {
  e.preventDefault();

  const title = document.getElementById("title").value;
  const issuer = document.getElementById("issuer").value;
  const fps = document.getElementById("fps").value;
  const [fpsNum, fpsDen] = fps.split("/");

  if (!videoDir || !outputDir) {
    alert("Please select video directory and output directory.");
    return;
  }

  const btn = document.getElementById("create-btn");
  btn.disabled = true;
  btn.textContent = "Creating...";

  const outputPanel = document.getElementById("output-panel");
  const outputLog = document.getElementById("output-log");
  outputPanel.hidden = false;
  outputLog.textContent = "Starting IMP creation...\n";

  try {
    const args = [
      "create",
      "--title", title,
      "--issuer", issuer,
      "--video", videoDir,
      "--output", outputDir,
      "--fps-num", fpsNum,
      "--fps-den", fpsDen,
    ];

    if (audioFile) {
      args.push("--audio", audioFile);
    }

    const command = Command.sidecar("imfwizard", args);
    const output = await command.execute();

    if (output.code === 0) {
      outputLog.textContent += output.stdout + "\nDone!\n";
    } else {
      outputLog.textContent += "ERROR:\n" + output.stderr + "\n";
    }
  } catch (err) {
    outputLog.textContent += "Error: " + err + "\n";
  }

  btn.disabled = false;
  btn.textContent = "Create IMP";
});

// === Transcode page ===
let tcInput = "", tcOutput = "";

document.getElementById("tc-select-input").addEventListener("click", async () => {
  const selected = await open({ title: "Select Video File" });
  if (selected) { tcInput = selected; document.getElementById("tc-input-path").textContent = selected; }
});

document.getElementById("tc-select-output").addEventListener("click", async () => {
  const selected = await open({ directory: true, title: "Select Output Directory" });
  if (selected) { tcOutput = selected; document.getElementById("tc-output-path").textContent = selected; }
});

document.getElementById("tc-start").addEventListener("click", async () => {
  if (!tcInput || !tcOutput) { alert("Select input and output."); return; }
  const log = document.getElementById("tc-log");
  log.style.display = "block";
  log.textContent = "Transcoding...\n";

  const format = document.getElementById("tc-format").value;
  const bitdepth = document.getElementById("tc-bitdepth").value;

  const command = Command.sidecar("imfwizard", [
    "transcode", "-i", tcInput, "-o", tcOutput, "-f", format, "--bit-depth", bitdepth
  ]);
  const output = await command.execute();
  log.textContent += output.code === 0 ? output.stdout + "\nDone!" : "ERROR:\n" + output.stderr;
});

// === Validate page ===
let valDir = "";

document.getElementById("val-select").addEventListener("click", async () => {
  const selected = await open({ directory: true, title: "Select IMP Directory" });
  if (selected) { valDir = selected; document.getElementById("val-path").textContent = selected; }
});

document.getElementById("val-start").addEventListener("click", async () => {
  if (!valDir) { alert("Select an IMP directory."); return; }
  const log = document.getElementById("val-log");
  log.style.display = "block";
  log.textContent = "Validating...\n";

  const command = Command.sidecar("imfwizard", ["validate", valDir]);
  const output = await command.execute();
  log.textContent += output.stdout + (output.stderr || "");
});
