document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById('tab-' + btn.dataset.tab).classList.add('active');
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

const ws = new WebSocket(`ws://${location.host}/ws`);
ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  updateGauge(data.index_fast, data.index_slow_avg);
  document.getElementById('switch-freq').textContent = data.switches_per_min;
  document.getElementById('status-text').textContent =
    data.warmup_state === 0 ? 'Sensor warming up…' : 'Sensor ready';
  document.getElementById('wizard-live-mv').textContent = data.index_fast;
  lastLiveMv = data.index_fast;
};

async function refreshStats() {
  const res = await fetch('/api/stats');
  const s = await res.json();
  renderStackedBar('session-bar', s.t_warmup_s, s.t_lean_s, s.t_lambda1_s, s.t_rich_s);
  renderStackedBar('longterm-bar', s.t_warmup_s, s.t_lean_s, s.t_lambda1_s, s.t_rich_s);
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
  await fetch('/api/config', { method: 'POST', body: JSON.stringify(body) });
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

let chartWindowS = 60;
let frozen = false;

const chartData = [[], []];
const uplotInstance = new uPlot({
  width: 600,
  height: 300,
  series: [
    {},
    { label: 'Mixture Index', stroke: 'green', width: 2 },
  ],
  scales: { y: { range: [-100, 100] } },
}, chartData, document.getElementById('chart'));

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
