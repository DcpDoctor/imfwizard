import { Command } from "@tauri-apps/plugin-shell";
import { open } from "@tauri-apps/plugin-dialog";

// Timeline state
let segments = [];
let selectedSegment = -1;
let timelineZoom = 1.0;

export function initTimeline() {
  const container = document.getElementById("timeline-page");
  if (!container) return;

  container.innerHTML = `
    <div class="timeline-toolbar">
      <button id="tl-add-video" class="btn-sm">+ Video</button>
      <button id="tl-add-audio" class="btn-sm">+ Audio</button>
      <button id="tl-add-subtitle" class="btn-sm">+ Subtitle</button>
      <button id="tl-remove" class="btn-sm btn-danger" disabled>Remove</button>
      <div class="tl-spacer"></div>
      <button id="tl-zoom-in" class="btn-sm">+</button>
      <button id="tl-zoom-out" class="btn-sm">−</button>
      <span id="tl-duration" class="tl-info">Duration: 0 frames</span>
    </div>
    <div class="timeline-tracks">
      <div class="track-lane" data-type="video">
        <div class="track-label">Video</div>
        <div class="track-content" id="tl-video-lane"></div>
      </div>
      <div class="track-lane" data-type="audio">
        <div class="track-label">Audio</div>
        <div class="track-content" id="tl-audio-lane"></div>
      </div>
      <div class="track-lane" data-type="subtitle">
        <div class="track-label">Subtitle</div>
        <div class="track-content" id="tl-subtitle-lane"></div>
      </div>
    </div>
    <div class="timeline-properties" id="tl-properties">
      <h3>Segment Properties</h3>
      <div id="tl-props-content">Select a segment to edit properties</div>
    </div>
  `;

  // Event listeners
  document.getElementById("tl-add-video").addEventListener("click", () => addSegment("video"));
  document.getElementById("tl-add-audio").addEventListener("click", () => addSegment("audio"));
  document.getElementById("tl-add-subtitle").addEventListener("click", () => addSegment("subtitle"));
  document.getElementById("tl-remove").addEventListener("click", removeSelectedSegment);
  document.getElementById("tl-zoom-in").addEventListener("click", () => { timelineZoom *= 1.5; render(); });
  document.getElementById("tl-zoom-out").addEventListener("click", () => { timelineZoom /= 1.5; render(); });
}

async function addSegment(type) {
  let path;
  if (type === "video") {
    path = await open({ directory: true, title: "Select Video Directory" });
  } else if (type === "audio") {
    path = await open({ filters: [{ name: "Audio", extensions: ["wav"] }] });
  } else {
    path = await open({ filters: [{ name: "TTML", extensions: ["xml", "ttml"] }] });
  }

  if (!path) return;

  const segment = {
    id: Date.now(),
    type,
    path,
    name: path.split(/[/\\]/).pop(),
    startFrame: getTotalDuration(type),
    duration: 240, // Default 10 seconds at 24fps
    color: type === "video" ? "#7c3aed" : type === "audio" ? "#3b82f6" : "#f59e0b",
  };

  segments.push(segment);
  render();
}

function removeSelectedSegment() {
  if (selectedSegment >= 0) {
    segments = segments.filter((_, i) => i !== selectedSegment);
    selectedSegment = -1;
    render();
  }
}

function getTotalDuration(type) {
  return segments
    .filter(s => s.type === type)
    .reduce((max, s) => Math.max(max, s.startFrame + s.duration), 0);
}

function selectSegment(index) {
  selectedSegment = index;
  document.getElementById("tl-remove").disabled = false;
  render();
  showProperties(segments[index]);
}

function showProperties(segment) {
  const el = document.getElementById("tl-props-content");
  el.innerHTML = `
    <div class="prop-row">
      <label>Name:</label><span>${segment.name}</span>
    </div>
    <div class="prop-row">
      <label>Type:</label><span>${segment.type}</span>
    </div>
    <div class="prop-row">
      <label>Path:</label><span class="prop-path">${segment.path}</span>
    </div>
    <div class="prop-row">
      <label>Start Frame:</label>
      <input type="number" value="${segment.startFrame}" id="prop-start" min="0" />
    </div>
    <div class="prop-row">
      <label>Duration:</label>
      <input type="number" value="${segment.duration}" id="prop-duration" min="1" />
    </div>
  `;

  document.getElementById("prop-start").addEventListener("change", (e) => {
    segment.startFrame = parseInt(e.target.value) || 0;
    render();
  });
  document.getElementById("prop-duration").addEventListener("change", (e) => {
    segment.duration = parseInt(e.target.value) || 1;
    render();
  });
}

function render() {
  const maxDuration = Math.max(240, ...segments.map(s => s.startFrame + s.duration));
  const pxPerFrame = timelineZoom * 2;

  for (const type of ["video", "audio", "subtitle"]) {
    const lane = document.getElementById(`tl-${type}-lane`);
    lane.innerHTML = "";
    lane.style.width = (maxDuration * pxPerFrame) + "px";

    segments.filter(s => s.type === type).forEach((seg, i) => {
      const globalIdx = segments.indexOf(seg);
      const el = document.createElement("div");
      el.className = "segment" + (globalIdx === selectedSegment ? " selected" : "");
      el.style.left = (seg.startFrame * pxPerFrame) + "px";
      el.style.width = (seg.duration * pxPerFrame) + "px";
      el.style.backgroundColor = seg.color;
      el.textContent = seg.name;
      el.addEventListener("click", () => selectSegment(globalIdx));

      // Drag to reposition
      el.draggable = true;
      el.addEventListener("dragstart", (e) => {
        e.dataTransfer.setData("text/plain", String(globalIdx));
      });

      lane.appendChild(el);
    });
  }

  document.getElementById("tl-duration").textContent =
    `Duration: ${maxDuration} frames`;
}

export function getTimelineSegments() {
  return segments;
}

export function getTimelineAsImpArgs() {
  // Convert timeline state to CLI arguments for IMP creation
  const videoSegs = segments.filter(s => s.type === "video");
  const audioSegs = segments.filter(s => s.type === "audio");

  const args = [];
  if (videoSegs.length > 0) args.push("--video", videoSegs[0].path);
  if (audioSegs.length > 0) args.push("--audio", audioSegs[0].path);

  return args;
}
