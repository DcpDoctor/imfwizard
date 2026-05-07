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

document.getElementById("tc-queue")?.addEventListener("click", async () => {
  if (!tcInput || !tcOutput) { alert("Select input and output."); return; }
  const format = document.getElementById("tc-format").value;
  const bitdepth = document.getElementById("tc-bitdepth").value;
  const desc = `Transcode ${tcInput.split("/").pop()} → ${format}`;
  const args = ["batch", "add", "-T", "transcode", "-d", desc, "--",
    "transcode", "-i", tcInput, "-o", tcOutput, "-f", format, "--bit-depth", bitdepth];
  const result = await Command.sidecar("imfwizard", args).execute();
  if (result.code === 0) {
    alert("Job submitted to queue!");
  } else {
    alert("Failed: " + (result.stderr || "Daemon not running"));
  }
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

// === Jobs page ===
let jobsPollInterval = null;

async function refreshJobs() {
  const statusBadge = document.getElementById("jobs-daemon-status");
  try {
    const ping = Command.sidecar("imfwizard", ["batch", "list"]);
    const result = await ping.execute();
    if (result.code !== 0) {
      statusBadge.textContent = "Daemon offline";
      statusBadge.className = "status-badge offline";
      document.getElementById("jobs-tbody").innerHTML =
        '<tr><td colspan="6" style="text-align:center">Daemon not running</td></tr>';
      return;
    }
    statusBadge.textContent = "Daemon online";
    statusBadge.className = "status-badge online";

    // Parse the table output
    const lines = result.stdout.trim().split("\n");
    const tbody = document.getElementById("jobs-tbody");

    if (lines.length <= 1 || lines[0].startsWith("No jobs")) {
      tbody.innerHTML = '<tr><td colspan="6" style="text-align:center">No jobs in queue</td></tr>';
      return;
    }

    // Skip header lines (first 2)
    const jobLines = lines.slice(2).filter(l => l.trim());
    tbody.innerHTML = jobLines.map(line => {
      const parts = line.trim().split(/\s+/);
      const id = parts[0];
      const state = parts[1];
      const progress = parts[2];
      const type = parts[3];
      const desc = parts.slice(4).join(" ");
      const cancelBtn = (state === "queued" || state === "running")
        ? `<button class="btn-cancel" data-job-id="${id}">Cancel</button>`
        : "";
      const stateClass = `state-${state}`;
      return `<tr>
        <td>${id}</td>
        <td>${type}</td>
        <td>${desc}</td>
        <td><span class="${stateClass}">${state}</span></td>
        <td>${progress}</td>
        <td>${cancelBtn}</td>
      </tr>`;
    }).join("");

    // Attach cancel handlers
    tbody.querySelectorAll(".btn-cancel").forEach(btn => {
      btn.addEventListener("click", async () => {
        const jobId = btn.dataset.jobId;
        await Command.sidecar("imfwizard", ["batch", "cancel", jobId]).execute();
        refreshJobs();
      });
    });
  } catch (err) {
    statusBadge.textContent = "Error";
    statusBadge.className = "status-badge offline";
    document.getElementById("jobs-tbody").innerHTML =
      `<tr><td colspan="6" style="text-align:center;color:var(--accent)">${err}</td></tr>`;
  }
}

document.getElementById("jobs-refresh")?.addEventListener("click", refreshJobs);

document.getElementById("jobs-start-daemon")?.addEventListener("click", async () => {
  Command.sidecar("imfwizard", ["daemon"]).spawn();
  await new Promise(r => setTimeout(r, 1500));
  refreshJobs();
});

document.getElementById("job-submit")?.addEventListener("click", async () => {
  const type = document.getElementById("job-type").value;
  const desc = document.getElementById("job-desc").value;
  const argsStr = document.getElementById("job-args").value;
  if (!desc) { alert("Enter a description"); return; }
  if (!argsStr) { alert("Enter arguments"); return; }

  const args = ["batch", "add", "-T", type, "-d", desc, "--"].concat(argsStr.split(/\s+/));
  const result = await Command.sidecar("imfwizard", args).execute();
  if (result.code === 0) {
    document.getElementById("job-desc").value = "";
    document.getElementById("job-args").value = "";
    refreshJobs();
  } else {
    alert("Failed to submit: " + result.stderr);
  }
});

// Auto-refresh jobs when tab is active
document.querySelectorAll(".nav-tabs button[data-page]").forEach(btn => {
  btn.addEventListener("click", () => {
    if (btn.dataset.page === "jobs-page") {
      refreshJobs();
      if (!jobsPollInterval) {
        jobsPollInterval = setInterval(refreshJobs, 3000);
      }
    } else {
      if (jobsPollInterval) { clearInterval(jobsPollInterval); jobsPollInterval = null; }
    }
  });
});
