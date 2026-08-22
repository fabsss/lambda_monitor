'use strict';

/* Keep the screen on while this page is open (e.g. mounted in a car during
 * a drive) — the wake lock is released by the browser whenever the tab is
 * hidden, so re-acquire it on visibilitychange. Unsupported browsers just
 * silently keep their normal screen-timeout behavior. */
let wakeLock = null;

async function requestWakeLock() {
  if (!('wakeLock' in navigator)) return;
  try {
    wakeLock = await navigator.wakeLock.request('screen');
    wakeLock.addEventListener('release', () => { wakeLock = null; });
  } catch (e) {
    console.error('wake lock request failed:', e);
  }
}
requestWakeLock();

document.addEventListener('visibilitychange', () => {
  if (wakeLock === null && document.visibilityState === 'visible') {
    requestWakeLock();
  }
});

document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById('tab-' + btn.dataset.tab).classList.add('active');
    if (btn.dataset.tab === 'chart' && window.resizeChartToContainer) {
      window.resizeChartToContainer();
    }
  });
});

function indexToAngle(index) {
  return (index / 100) * 90;
}

function updateGauge(indexFast, indexSlow) {
  const needleFast = document.getElementById('needle-fast');
  const needleSlow = document.getElementById('needle-slow');
  needleFast.setAttribute('transform', `rotate(${indexToAngle(indexFast)} 100 110)`);
  needleSlow.setAttribute('transform', `rotate(${indexToAngle(indexSlow)} 100 110)`);
}

const CATEGORY_SWATCHES = {
  'bar-warmup': '#999',
  'bar-very-lean': '#c92a2a',
  'bar-lean': '#ff6b6b',
  'bar-lambda1': '#51cf66',
  'bar-rich': '#4dabf7',
  'bar-very-rich': '#1864ab',
};

function renderStackedBar(elId, tWarmup, tVeryLean, tLean, tLambda1, tRich, tVeryRich) {
  const total = tWarmup + tVeryLean + tLean + tLambda1 + tRich + tVeryRich;
  const el = document.getElementById(elId);
  el.innerHTML = '';
  if (total === 0) return;

  const segments = [
    ['bar-warmup', tWarmup, 'Warmup'],
    ['bar-very-lean', tVeryLean, 'Very Lean'],
    ['bar-lean', tLean, 'Lean'],
    ['bar-lambda1', tLambda1, 'λ=1'],
    ['bar-rich', tRich, 'Rich'],
    ['bar-very-rich', tVeryRich, 'Very Rich'],
  ];

  for (const [cls, seconds, label] of segments) {
    const pct = (seconds / total) * 100;
    if (pct <= 0) continue;
    const div = document.createElement('div');
    div.className = cls;
    div.style.width = pct + '%';
    div.title = `${label}: ${seconds}s (${pct.toFixed(1)}%)`;
    el.appendChild(div);
  }
}

function formatAvg2sAvg(avg) {
  const rounded = Math.round(avg * 10) / 10;
  if (rounded > 0) return `+${rounded} (rich bias)`;
  if (rounded < 0) return `${rounded} (lean bias)`;
  return '0 (centered)';
}

function renderStatsTable(tableId, tWarmup, tVeryLean, tLean, tLambda1, tRich, tVeryRich, avg2sAvg) {
  const total = tWarmup + tVeryLean + tLean + tLambda1 + tRich + tVeryRich;
  const rows = [
    ['bar-warmup', 'Warmup', tWarmup],
    ['bar-very-lean', 'Very Lean', tVeryLean],
    ['bar-lean', 'Lean', tLean],
    ['bar-lambda1', 'λ=1', tLambda1],
    ['bar-rich', 'Rich', tRich],
    ['bar-very-rich', 'Very Rich', tVeryRich],
  ];

  const table = document.getElementById(tableId);
  table.innerHTML = '';
  for (const [cls, label, seconds] of rows) {
    const pct = total ? (seconds / total * 100).toFixed(1) : '0.0';
    const tr = document.createElement('tr');
    const labelTd = document.createElement('td');
    const swatch = document.createElement('span');
    swatch.className = 'swatch';
    swatch.style.background = CATEGORY_SWATCHES[cls];
    labelTd.appendChild(swatch);
    labelTd.appendChild(document.createTextNode(label));
    const valueTd = document.createElement('td');
    valueTd.textContent = `${seconds}s (${pct}%)`;
    tr.appendChild(labelTd);
    tr.appendChild(valueTd);
    table.appendChild(tr);
  }

  const avgTr = document.createElement('tr');
  const avgLabelTd = document.createElement('td');
  avgLabelTd.textContent = 'Ø Control quality';
  const avgValueTd = document.createElement('td');
  avgValueTd.textContent = formatAvg2sAvg(avg2sAvg);
  avgTr.appendChild(avgLabelTd);
  avgTr.appendChild(avgValueTd);
  table.appendChild(avgTr);
}

let lastLiveMv = 0;

async function updateLiveData() {
  try {
    const res = await fetch('/api/snapshot');
    const data = await res.json();
    updateGauge(data.index_fast, data.index_slow_avg);
    document.getElementById('live-mv').textContent = data.raw_mv;
    document.getElementById('switch-freq').textContent = data.switches_per_min;
    document.getElementById('avg-2s').textContent = formatAvg2sAvg(data.index_avg_2s);
    document.getElementById('status-text').textContent =
      data.warmup_state === 0 ? 'Sensor warming up…' : 'Sensor ready';
    document.getElementById('wizard-live-mv').textContent = data.raw_mv + ' mV';
    lastLiveMv = data.raw_mv;
  } catch (e) {
    console.error('snapshot fetch failed:', e);
  }
}
setInterval(updateLiveData, 500);
updateLiveData();

async function refreshConfig() {
  try {
    const res = await fetch('/api/config');
    const data = await res.json();
    document.querySelector('[name=u_min_mv]').value = data.u_min_mv;
    document.querySelector('[name=u_max_mv]').value = data.u_max_mv;
    document.querySelector('[name=u_lambda1_mv]').value = data.u_lambda1_mv;
    document.querySelector('[name=deadband_mv]').value = data.deadband_mv;
  } catch (e) {
    console.error('config fetch failed:', e);
  }
}
refreshConfig();

async function refreshVersion() {
  try {
    const res = await fetch('/api/version');
    const data = await res.json();
    document.getElementById('fw-version').textContent = data.fw_version;
    document.getElementById('fw-build').textContent = `${data.build_date} ${data.build_time}`;
  } catch (e) {
    console.error('version fetch failed:', e);
  }
}
refreshVersion();

async function refreshWifi() {
  try {
    const res = await fetch('/api/wifi');
    const data = await res.json();
    document.getElementById('wifi-current-ssid').textContent = data.ssid;
    document.querySelector('#wifi-form [name=ssid]').value = data.ssid;
  } catch (e) {
    console.error('wifi fetch failed:', e);
  }
}
refreshWifi();

document.getElementById('wifi-form').addEventListener('submit', async (e) => {
  e.preventDefault();
  const form = new FormData(e.target);
  const ssid = form.get('ssid');
  const password = form.get('password');
  if (!confirm(`Save access point "${ssid}" and reboot? You will need to reconnect using the new name/password.`)) {
    return;
  }
  const res = await fetch('/api/wifi', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ ssid, password })
  });
  if (!res.ok) {
    alert('Failed to save access point settings: ' + (await res.text()));
  } else {
    alert('Saved. Device is rebooting - reconnect using the new name/password.');
  }
});

async function refreshStats() {
  const res = await fetch('/api/stats');
  const data = await res.json();
  renderStackedBar('session-bar', data.session.t_warmup_s, data.session.t_very_lean_s, data.session.t_lean_s, data.session.t_lambda1_s, data.session.t_rich_s, data.session.t_very_rich_s);
  renderStackedBar('longterm-bar', data.t_warmup_s, data.t_very_lean_s, data.t_lean_s, data.t_lambda1_s, data.t_rich_s, data.t_very_rich_s);
  renderStatsTable('session-table', data.session.t_warmup_s, data.session.t_very_lean_s, data.session.t_lean_s, data.session.t_lambda1_s, data.session.t_rich_s, data.session.t_very_rich_s, data.session.avg2s_avg);
  renderStatsTable('longterm-table', data.t_warmup_s, data.t_very_lean_s, data.t_lean_s, data.t_lambda1_s, data.t_rich_s, data.t_very_rich_s, data.avg2s_avg);
}
setInterval(refreshStats, 2000);
refreshStats();

document.getElementById('reset-btn').addEventListener('click', async () => {
  if (confirm('Reset long-term statistics? This cannot be undone.')) {
    await fetch('/api/reset', { method: 'POST' });
    refreshStats();
  }
});

document.getElementById('config-form').addEventListener('submit', async (e) => {
  e.preventDefault();
  const form = new FormData(e.target);
  const body = {};
  for (const [key, value] of form.entries()) {
    body[key] = parseInt(value, 10);
  }
  const res = await fetch('/api/config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  });
  if (!res.ok) {
    alert('Failed to save calibration: ' + (await res.text()));
  } else {
    alert('Calibration saved successfully');
    refreshConfig();
  }
});

document.getElementById('wizard-set-lambda1').addEventListener('click', () => {
  document.querySelector('[name=u_lambda1_mv]').value = lastLiveMv;
});
document.getElementById('wizard-set-min').addEventListener('click', () => {
  document.querySelector('[name=u_min_mv]').value = lastLiveMv;
});
document.getElementById('wizard-set-max').addEventListener('click', () => {
  document.querySelector('[name=u_max_mv]').value = lastLiveMv;
});

let autocal_running = false;

document.getElementById('wizard-autocal-start').addEventListener('click', async () => {
  try {
    autocal_running = true;
    document.getElementById('wizard-autocal-status').textContent = 'Collecting samples...';
    document.getElementById('wizard-autocal-progress').textContent = '0/100 samples';
    const res = await fetch('/api/calibrate/auto', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'start' })
    });
    if (!res.ok) {
      document.getElementById('wizard-autocal-status').textContent = 'Error starting calibration';
      autocal_running = false;
    }
  } catch (e) {
    console.error('autocal start failed:', e);
    autocal_running = false;
  }
});

document.getElementById('wizard-autocal-derive').addEventListener('click', async () => {
  try {
    autocal_running = false;
    document.getElementById('wizard-autocal-status').textContent = 'Deriving calibration...';
    const res = await fetch('/api/calibrate/auto', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'derive' })
    });
    if (!res.ok) {
      document.getElementById('wizard-autocal-status').textContent = 'Error deriving calibration';
      return;
    }
    const cal = await res.json();
    document.querySelector('[name=u_min_mv]').value = cal.u_min_mv;
    document.querySelector('[name=u_max_mv]').value = cal.u_max_mv;
    document.querySelector('[name=u_lambda1_mv]').value = cal.u_lambda1_mv;
    document.querySelector('[name=deadband_mv]').value = cal.deadband_mv;
    document.getElementById('wizard-autocal-status').textContent = 'Done! Review and save.';
    document.getElementById('wizard-autocal-progress').textContent = '0/100 samples';
  } catch (e) {
    console.error('autocal derive failed:', e);
    document.getElementById('wizard-autocal-status').textContent = 'Error deriving calibration';
  }
});

async function updateAutocalStatus() {
  if (!autocal_running) return;
  try {
    const res = await fetch('/api/calibrate/auto');
    const data = await res.json();
    document.getElementById('wizard-autocal-progress').textContent = `${data.count}/${data.max} samples`;
    if (data.count >= data.max) {
      document.getElementById('wizard-autocal-status').textContent = 'Collection complete (100 samples). Click "Derive & apply" to analyze.';
    }
  } catch (e) {
    console.error('autocal status fetch failed:', e);
  }
}
setInterval(updateAutocalStatus, 500);

let chartWindowS = 60;
let frozen = false;
let chartConfig = null;

const chartContainer = document.getElementById('chart');
const chartData = [[], []];

const uplotInstance = new uPlot({
  width: chartContainer.clientWidth,
  height: 350,
  series: [
    { label: 'Time (s)' },
    { label: 'Voltage (mV)', stroke: '#000', width: 2 },
  ],
  scales: {
    x: { time: false },
    y: { range: [0, 3300] }
  },
  axes: [
    { label: 'Time since start (s)' },
    { label: 'Voltage (mV)' },
  ],
  hooks: {
    draw: [
      (u) => {
        if (!chartConfig) return;
        const { u_lambda1_mv, deadband_mv } = chartConfig;
        const yScale = u.scales.y;

        const ctx = u.ctx;
        const plotTop = u.bbox.top;
        const plotBottom = u.bbox.top + u.bbox.height;
        const plotHeight = u.bbox.height;

        const yValToPixel = (yVal) => plotBottom - ((yVal - yScale.min) / (yScale.max - yScale.min)) * plotHeight;

        const leanLower = yScale.min;
        const leanUpper = u_lambda1_mv - deadband_mv;
        const lambda1Lower = u_lambda1_mv - deadband_mv;
        const lambda1Upper = u_lambda1_mv + deadband_mv;
        const richLower = u_lambda1_mv + deadband_mv;
        const richUpper = yScale.max;

        const leanY1 = yValToPixel(leanUpper);
        const leanY2 = yValToPixel(leanLower);
        const lambda1Y1 = yValToPixel(lambda1Upper);
        const lambda1Y2 = yValToPixel(lambda1Lower);
        const richY1 = yValToPixel(richUpper);
        const richY2 = yValToPixel(richLower);

        ctx.fillStyle = 'rgba(255, 107, 107, 0.2)';
        ctx.fillRect(u.bbox.left, leanY1, u.bbox.width, leanY2 - leanY1);

        ctx.fillStyle = 'rgba(81, 207, 102, 0.2)';
        ctx.fillRect(u.bbox.left, lambda1Y1, u.bbox.width, lambda1Y2 - lambda1Y1);

        ctx.fillStyle = 'rgba(77, 171, 247, 0.2)';
        ctx.fillRect(u.bbox.left, richY1, u.bbox.width, richY2 - richY1);
      }
    ]
  }
}, chartData, chartContainer);

/* Inverse of si_mv_to_index() (lib/signal_interpreter/signal_interpreter.c):
 * piecewise-linear around u_lambda1_mv, using the live calibration rather
 * than hardcoded defaults. The lean and rich sides can have different
 * spans (u_lambda1 - u_min vs u_max - u_lambda1), so a single symmetric
 * linear formula is wrong and clips lean-side extremes off the y-axis. */
function indexToMv(idx, cfg) {
  if (idx <= 0) {
    return cfg.u_lambda1_mv + (idx / 100) * (cfg.u_lambda1_mv - cfg.u_min_mv);
  }
  return cfg.u_lambda1_mv + (idx / 100) * (cfg.u_max_mv - cfg.u_lambda1_mv);
}

window.resizeChartToContainer = () => {
  uplotInstance.setSize({ width: chartContainer.clientWidth, height: 350 });
};
window.addEventListener('resize', window.resizeChartToContainer);

async function refreshChart() {
  if (frozen) return;
  try {
    const res = await fetch('/api/curve');
    const data = await res.json();
    const configRes = await fetch('/api/config');
    const config = await configRes.json();
    chartConfig = config;

    const now = data.timestamps_s.length ? data.timestamps_s[data.timestamps_s.length - 1] : 0;
    const cutoff = now - chartWindowS;

    const xs = [];
    const ys = [];

    for (let i = 0; i < data.timestamps_s.length; i++) {
      const ts = data.timestamps_s[i];
      if (ts >= cutoff) {
        const idx = data.index_values[i];
        const mv = indexToMv(idx, chartConfig);
        xs.push(ts);
        ys.push(mv);
      }
    }
    uplotInstance.setData([xs, ys]);
  } catch (e) {
    console.error('chart refresh failed:', e);
  }
}
setInterval(refreshChart, 1000);

document.querySelectorAll('[data-window]').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('[data-window]').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    chartWindowS = parseInt(btn.dataset.window, 10);
    refreshChart();
  });
});

document.getElementById('freeze-btn').addEventListener('click', () => {
  frozen = !frozen;
  document.getElementById('freeze-btn').textContent = frozen ? 'Resume' : 'Freeze';
});

document.getElementById('ota-form').addEventListener('submit', async (e) => {
  e.preventDefault();
  const file = document.getElementById('ota-file').files[0];
  if (!file) return;
  if (!confirm(`Flash firmware "${file.name}" (${file.size} bytes)? The device will reboot.`)) {
    return;
  }

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/ota');
  xhr.upload.addEventListener('progress', (evt) => {
    if (evt.lengthComputable) {
      document.getElementById('ota-progress').value = (evt.loaded / evt.total) * 100;
    }
  });
  xhr.onload = () => alert('Upload complete, device is rebooting.');
  xhr.send(file);
});
