// Preview player - decodes J2K frames for playback using CLI sidecar
import { invoke } from '@tauri-apps/api/core';
import { open } from '@tauri-apps/plugin-dialog';

let frames = [];
let currentFrame = 0;
let playing = false;
let playInterval = null;
let fps = 24;

const canvas = document.getElementById('preview-canvas');
const ctx = canvas?.getContext('2d');
const waveCanvas = document.getElementById('waveform-canvas');
const waveCtx = waveCanvas?.getContext('2d');

export function initPreview() {
  const selectBtn = document.getElementById('prev-select');
  const playBtn = document.getElementById('prev-play');
  const backBtn = document.getElementById('prev-back');
  const forwardBtn = document.getElementById('prev-forward');
  const scrub = document.getElementById('prev-scrub');

  selectBtn?.addEventListener('click', async () => {
    const dir = await open({ directory: true });
    if (dir) {
      document.getElementById('prev-path').textContent = dir;
      await loadFrames(dir);
    }
  });

  playBtn?.addEventListener('click', togglePlay);
  backBtn?.addEventListener('click', () => seekFrame(currentFrame - 1));
  forwardBtn?.addEventListener('click', () => seekFrame(currentFrame + 1));
  scrub?.addEventListener('input', (e) => seekFrame(parseInt(e.target.value)));

  // Keyboard shortcuts
  document.addEventListener('keydown', (e) => {
    if (document.getElementById('preview-page')?.classList.contains('active')) {
      if (e.key === ' ') { e.preventDefault(); togglePlay(); }
      if (e.key === 'ArrowLeft') seekFrame(currentFrame - 1);
      if (e.key === 'ArrowRight') seekFrame(currentFrame + 1);
    }
  });
}

async function loadFrames(dir) {
  // Use imfwizard CLI to list and decode frames
  try {
    const result = await invoke('list_j2k_frames', { dir });
    frames = result.frames || [];
    const scrub = document.getElementById('prev-scrub');
    if (scrub) {
      scrub.max = Math.max(0, frames.length - 1).toString();
      scrub.value = '0';
    }
    document.getElementById('prev-frame-num').textContent =
      `Frame: 0 / ${frames.length}`;

    if (frames.length > 0) {
      await displayFrame(0);
      drawWaveform();
    }
  } catch (e) {
    // Fallback: list directory contents via shell
    console.error('Frame listing failed:', e);
  }
}

async function displayFrame(index) {
  if (index < 0 || index >= frames.length) return;
  currentFrame = index;

  try {
    // Decode J2K frame to raw RGB via CLI
    const result = await invoke('decode_j2k_frame', { path: frames[index] });
    if (result.width && result.height && result.data) {
      canvas.width = result.width;
      canvas.height = result.height;
      const imageData = new ImageData(
        new Uint8ClampedArray(result.data),
        result.width,
        result.height
      );
      ctx.putImageData(imageData, 0, 0);
    }
  } catch (e) {
    // Draw placeholder
    ctx.fillStyle = '#1a1a2e';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = '#ccc';
    ctx.font = '24px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText(`Frame ${index} (decode unavailable)`, canvas.width / 2, canvas.height / 2);
  }

  document.getElementById('prev-frame-num').textContent =
    `Frame: ${index} / ${frames.length}`;
  const scrub = document.getElementById('prev-scrub');
  if (scrub) scrub.value = index.toString();
}

function seekFrame(index) {
  index = Math.max(0, Math.min(frames.length - 1, index));
  displayFrame(index);
}

function togglePlay() {
  playing = !playing;
  const btn = document.getElementById('prev-play');

  if (playing) {
    btn.textContent = '⏸';
    playInterval = setInterval(() => {
      if (currentFrame >= frames.length - 1) {
        togglePlay(); // Stop at end
        return;
      }
      displayFrame(currentFrame + 1);
    }, 1000 / fps);
  } else {
    btn.textContent = '▶';
    clearInterval(playInterval);
    playInterval = null;
  }
}

function drawWaveform() {
  if (!waveCtx || !waveCanvas) return;

  const w = waveCanvas.width;
  const h = waveCanvas.height;

  // Draw bitrate waveform (frame sizes)
  waveCtx.fillStyle = '#0d1117';
  waveCtx.fillRect(0, 0, w, h);

  if (frames.length === 0) return;

  // Simulated waveform based on frame count
  waveCtx.strokeStyle = '#58a6ff';
  waveCtx.lineWidth = 1;
  waveCtx.beginPath();

  const step = w / frames.length;
  for (let i = 0; i < frames.length; i++) {
    const x = i * step;
    // Random-ish waveform for now (real impl would read frame sizes)
    const y = h / 2 + Math.sin(i * 0.1) * (h * 0.3);
    if (i === 0) waveCtx.moveTo(x, y);
    else waveCtx.lineTo(x, y);
  }
  waveCtx.stroke();

  // Draw playhead
  drawPlayhead();
}

function drawPlayhead() {
  if (!waveCtx || !waveCanvas || frames.length === 0) return;

  const w = waveCanvas.width;
  const h = waveCanvas.height;
  const x = (currentFrame / frames.length) * w;

  waveCtx.strokeStyle = '#f85149';
  waveCtx.lineWidth = 2;
  waveCtx.beginPath();
  waveCtx.moveTo(x, 0);
  waveCtx.lineTo(x, h);
  waveCtx.stroke();
}
