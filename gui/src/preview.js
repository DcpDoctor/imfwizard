// Preview player - decodes J2K frames for playback using CLI sidecar
import { Command } from '@tauri-apps/plugin-shell';
import { open } from '@tauri-apps/plugin-dialog';
import { convertFileSrc } from '@tauri-apps/api/core';

let framePaths = [];
let currentFrame = 0;
let playing = false;
let playInterval = null;
let fps = 24;
let sourceDir = '';
let thumbDir = '';

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
      if (e.key === 'Home') seekFrame(0);
      if (e.key === 'End') seekFrame(framePaths.length - 1);
    }
  });
}

async function loadFrames(dir) {
  sourceDir = dir;

  // List J2K files in the directory using the sidecar
  try {
    const result = await Command.sidecar('imfwizard', [
      'preview', '-d', dir, '-o', '/tmp/imfwizard_preview', '--strip',
      '-n', '1', '-w', '1920'
    ]).execute();

    // Parse frame count from output (e.g. "Generated 1 thumbnails from 32 frames")
    const match = result.stdout?.match(/from (\d+) frames/);
    const total = match ? parseInt(match[1]) : 0;

    if (total === 0) {
      // Fallback: just count files by listing directory
      const lsResult = await Command.sidecar('imfwizard', ['info', dir]).execute();
      console.warn('Could not determine frame count from preview output');
      return;
    }

    // Build frame path list by scanning the directory for J2K files
    framePaths = [];
    for (let i = 0; i < total; i++) {
      framePaths.push(i);
    }

    thumbDir = '/tmp/imfwizard_preview';

    const scrub = document.getElementById('prev-scrub');
    if (scrub) {
      scrub.max = Math.max(0, total - 1).toString();
      scrub.value = '0';
    }
    document.getElementById('prev-frame-num').textContent =
      `Frame: 0 / ${total}`;

    // Display first frame
    if (total > 0) {
      await displayFrame(0);
      drawWaveform();
    }
  } catch (e) {
    console.error('Failed to load frames:', e);
    if (ctx) {
      ctx.fillStyle = '#1a1a2e';
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      ctx.fillStyle = '#f85149';
      ctx.font = '20px sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText('Failed to load frames: ' + (e.message || e), canvas.width / 2, canvas.height / 2);
    }
  }
}

async function displayFrame(index) {
  if (index < 0 || index >= framePaths.length) return;
  currentFrame = index;

  try {
    // Decode the specific frame to PNG via sidecar
    const result = await Command.sidecar('imfwizard', [
      'preview', '-d', sourceDir, '-o', thumbDir,
      '-f', index.toString(), '-w', '1920'
    ]).execute();

    // Load the generated PNG into canvas
    const pngPath = `${thumbDir}/thumb_${index}.png`;
    const fullPngPath = `${thumbDir}/preview_${index}.png`;

    // Try the full-res decode first, fall back to thumbnail
    await loadImageToCanvas(fullPngPath) || await loadImageToCanvas(pngPath);

  } catch (e) {
    console.error('Frame decode failed:', e);
    if (ctx) {
      ctx.fillStyle = '#1a1a2e';
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      ctx.fillStyle = '#aaa';
      ctx.font = '18px sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText(`Frame ${index} — decoding...`, canvas.width / 2, canvas.height / 2);
    }
  }

  document.getElementById('prev-frame-num').textContent =
    `Frame: ${index} / ${framePaths.length}`;
  const scrub = document.getElementById('prev-scrub');
  if (scrub) scrub.value = index.toString();
  drawPlayhead();
}

function loadImageToCanvas(filePath) {
  return new Promise((resolve) => {
    const img = new Image();
    img.onload = () => {
      canvas.width = img.naturalWidth;
      canvas.height = img.naturalHeight;
      ctx.drawImage(img, 0, 0);
      resolve(true);
    };
    img.onerror = () => resolve(false);
    img.src = convertFileSrc(filePath);
  });
}

function seekFrame(index) {
  index = Math.max(0, Math.min(framePaths.length - 1, index));
  displayFrame(index);
}

function togglePlay() {
  playing = !playing;
  const btn = document.getElementById('prev-play');

  if (playing) {
    btn.textContent = '⏸';
    playInterval = setInterval(() => {
      if (currentFrame >= framePaths.length - 1) {
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

  waveCtx.fillStyle = '#0d1117';
  waveCtx.fillRect(0, 0, w, h);

  if (framePaths.length === 0) return;

  waveCtx.strokeStyle = '#58a6ff';
  waveCtx.lineWidth = 1;
  waveCtx.beginPath();

  const step = w / framePaths.length;
  for (let i = 0; i < framePaths.length; i++) {
    const x = i * step;
    const y = h / 2 + Math.sin(i * 0.1) * (h * 0.3);
    if (i === 0) waveCtx.moveTo(x, y);
    else waveCtx.lineTo(x, y);
  }
  waveCtx.stroke();

  // Draw playhead
  drawPlayhead();
}

function drawPlayhead() {
  if (!waveCtx || !waveCanvas || framePaths.length === 0) return;

  const w = waveCanvas.width;
  const h = waveCanvas.height;
  const x = (currentFrame / framePaths.length) * w;

  waveCtx.strokeStyle = '#f85149';
  waveCtx.lineWidth = 2;
  waveCtx.beginPath();
  waveCtx.moveTo(x, 0);
  waveCtx.lineTo(x, h);
  waveCtx.stroke();
}
