// Preview player - uses mpv via IPC for high-performance video playback
import { invoke } from '@tauri-apps/api/core';
import { open } from '@tauri-apps/plugin-dialog';

let lastBrowsePath = null;

export function initPreview() {
  const selectBtn = document.getElementById('prev-select');
  const selectDirBtn = document.getElementById('prev-select-dir');
  selectBtn?.addEventListener('click', async () => {
    const path = await open({
      directory: false,
      multiple: false,
      defaultPath: lastBrowsePath || undefined,
      filters: [
        { name: 'Video', extensions: ['mp4', 'mkv', 'mov', 'avi', 'mxf', 'webm', 'ogg'] },
        { name: 'All Files', extensions: ['*'] }
      ]
    });
    if (path) {
      lastBrowsePath = path.replace(/[/\\][^/\\]*$/, '');
      document.getElementById('prev-path').textContent = path;
      invoke('preview_load', { filePath: path }).catch((e) => {
        console.error('[preview] Failed to load:', e);
        document.getElementById('prev-time').textContent = 'Error: ' + e;
      });
    }
  });

  selectDirBtn?.addEventListener('click', async () => {
    const path = await open({ directory: true, multiple: false, defaultPath: lastBrowsePath || undefined });
    if (path) {
      lastBrowsePath = path;
      document.getElementById('prev-path').textContent = path;
      invoke('preview_load', { filePath: path }).catch((e) => {
        console.error('[preview] Failed to load:', e);
        document.getElementById('prev-time').textContent = 'Error: ' + e;
      });
    }
  });

  // DCP/IMP replay
  const replayDcpBtn = document.getElementById('prev-replay-dcp');
  replayDcpBtn?.addEventListener('click', async () => {
    const path = await open({ directory: true, multiple: false, defaultPath: lastBrowsePath || undefined });
    if (path) {
      lastBrowsePath = path;
      document.getElementById('prev-dcp-path').textContent = path;
      invoke('preview_load_dcp', { dirPath: path }).catch((e) => {
        console.error('[preview] Failed to load DCP/IMP:', e);
        document.getElementById('prev-time').textContent = 'Error: ' + e;
      });
    }
  });

  // Keyboard shortcuts
  document.addEventListener('keydown', (e) => {
    if (document.getElementById('create-page')?.classList.contains('active')) {
      if (e.key === ' ' && e.target.tagName !== 'INPUT' && e.target.tagName !== 'SELECT') {
        e.preventDefault(); invoke('preview_play_pause');
      }
      if (e.key === 'ArrowLeft') invoke('preview_seek', { seconds: -5.0 });
      if (e.key === 'ArrowRight') invoke('preview_seek', { seconds: 5.0 });
    }
  });
}
