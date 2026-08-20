'use strict';

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

function renderStackedBar(elId, tWarmup, tLean, tLambda1, tRich) {
  const total = tWarmup + tLean + tLambda1 + tRich;
  const el = document.getElementById(elId);
  el.innerHTML = '';
  if (total === 0) return;
  const segments = [
    ['bar-warmup', tWarmup], ['bar-lean', tLean],
    ['bar-lambda1', tLambda1], ['bar-rich', tRich],
  ];
  for (const [cls, seconds] of segments) {
    const pct = (seconds / total) * 100;
    if (pct <= 0) continue;
    const div = document.createElement('div');
    div.className = cls;
    div.style.width = pct + '%';
    el.appendChild(div);
  }
}

let lastLiveMv = 0;

async function updateLiveData() {
  try {
    const res = await fetch('/api/snapshot');
    const data = await res.json();
    updateGauge(data.index_fast, data.index_slow_avg);
    document.getElementById('switch-freq').textContent = data.switches_per_min;
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

async function refreshStats() {
  const res = await fetch('/api/stats');
  const data = await res.json();
  renderStackedBar('session-bar', data.session.t_warmup_s, data.session.t_lean_s, data.session.t_lambda1_s, data.session.t_rich_s);
  renderStackedBar('longterm-bar', data.t_warmup_s, data.t_lean_s, data.t_lambda1_s, data.t_rich_s);
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
  autocal_running = true;
  document.getElementById('wizard-autocal-status').textContent = 'Collecting samples...';
  await fetch('/api/calibrate/auto', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ action: 'start' })
  });
});

document.getElementById('wizard-autocal-derive').addEventListener('click', async () => {
  autocal_running = false;
  document.getElementById('wizard-autocal-status').textContent = 'Deriving calibration...';
  const res = await fetch('/api/calibrate/auto', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ action: 'derive' })
  });
  const cal = await res.json();
  document.querySelector('[name=u_min_mv]').value = cal.u_min_mv;
  document.querySelector('[name=u_max_mv]').value = cal.u_max_mv;
  document.querySelector('[name=u_lambda1_mv]').value = cal.u_lambda1_mv;
  document.querySelector('[name=deadband_mv]').value = cal.deadband_mv;
  document.getElementById('wizard-autocal-status').textContent = 'Done! Review and save.';
});

async function updateAutocalStatus() {
  if (!autocal_running) return;
  const res = await fetch('/api/calibrate/auto');
  const data = await res.json();
  document.getElementById('wizard-autocal-progress').textContent = `${data.count}/${data.max} samples`;
}
setInterval(updateAutocalStatus, 200);

let chartWindowS = 60;
let frozen = false;

const chartContainer = document.getElementById('chart');
const chartData = [[], []];
const uplotInstance = new uPlot({
  width: chartContainer.clientWidth,
  height: 300,
  series: [
    {},
    { label: 'Mixture Index', stroke: 'green', width: 2 },
  ],
  scales: { y: { range: [-100, 100] } },
}, chartData, chartContainer);

window.resizeChartToContainer = () => {
  uplotInstance.setSize({ width: chartContainer.clientWidth, height: 300 });
};
window.addEventListener('resize', window.resizeChartToContainer);

async function refreshChart() {
  if (frozen) return;
  const res = await fetch('/api/curve');
  const data = await res.json();
  const now = data.timestamps_s.length ? data.timestamps_s[data.timestamps_s.length - 1] : 0;
  const cutoff = now - chartWindowS;

  const xs = [];
  const ys = [];
  for (let i = 0; i < data.timestamps_s.length; i++) {
    if (data.timestamps_s[i] >= cutoff) {
      xs.push(data.timestamps_s[i]);
      ys.push(data.index_values[i]);
    }
  }
  uplotInstance.setData([xs, ys]);
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
