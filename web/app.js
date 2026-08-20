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
  const body = Object.fromEntries(form.entries());
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
