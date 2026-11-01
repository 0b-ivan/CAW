// Load test: many rooms play real rounds at once, measured against the budgets of
// docs/KONZEPT.md section 5a. Needs Node 22+ (built-in WebSocket and fetch).
//
//   node tests/load/load.mjs http://localhost:8080 1000 3
//
// Arguments: base URL, rooms (default 100), rounds per room (default 3).
// Every room has a host, a big screen and eight players. Exit code 1 if a budget fails.
// The rooms are split over several worker processes: one Node process cannot parse the
// frames of 10 000 sockets fast enough, and would measure itself instead of the server.
import { fork } from 'node:child_process';
import { availableParallelism } from 'node:os';

const base = process.argv[2] ?? 'http://localhost:8080';
const rooms = Number(process.argv[3] ?? 100);
const rounds = Number(process.argv[4] ?? 3);
const wsBase = base.replace(/^http/, 'ws');
const PLAYERS = 8;
const OPEN_BATCH = 25; // rooms opened at a time, so connecting is not one burst
// People need a moment to read and choose. Real rounds take about a minute; 1–8 s per move
// is still several times faster than people, so the server sees more load than in a party.
const THINK_MS = [1000, 8000];

const BUDGET = {
  clientP99Ms: 50, // submit or judge sent → the sender sees the result
  serverShareWithin10Ms: 0.99, // caw_action_seconds: 99 % of actions within 10 ms
  frameBytes: 16 * 1024,
  typicalFrameBytes: 2 * 1024,
  bytesPerRoom: 64 * 1024,
};

const latencies = [];
let errors = 0;

const think = () =>
  new Promise((resolve) => setTimeout(resolve, THINK_MS[0] + Math.random() * (THINK_MS[1] - THINK_MS[0])));

function percentile(values, share) {
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[Math.min(sorted.length - 1, Math.floor(sorted.length * share))] ?? 0;
}

// One socket that remembers the newest state and wakes whoever waits for a condition.
function connect(url) {
  const tab = { state: null, token: null, waiters: [] };
  tab.ws = new WebSocket(url);
  tab.ws.onmessage = (event) => {
    const frame = JSON.parse(event.data);
    if (frame.type === 'error') errors += 1;
    if (frame.type === 'welcome') tab.token = frame.token;
    if (frame.type === 'state' || frame.type === 'screen') tab.state = frame;
    tab.waiters = tab.waiters.filter((waiter) => !waiter());
  };
  tab.ws.onerror = () => (errors += 1);
  tab.send = (message) => tab.ws.send(JSON.stringify(message));
  tab.until = (check, timeoutMs = 20000) =>
    new Promise((resolve, reject) => {
      if (check(tab)) return resolve();
      const timer = setTimeout(() => reject(new Error('timeout')), timeoutMs);
      tab.waiters.push(() => {
        if (!check(tab)) return false;
        clearTimeout(timer);
        resolve();
        return true;
      });
    });
  tab.opened = new Promise((resolve, reject) => {
    tab.ws.onopen = resolve;
    tab.ws.onclose = () => reject(new Error('closed before open'));
  });
  return tab;
}

async function openRoom() {
  const room = await (await fetch(`${base}/api/sessions`, { method: 'POST', body: '{}' })).json();
  const host = connect(`${wsBase}/ws/${room.code}`);
  const screen = connect(`${wsBase}/ws/${room.code}/screen`);
  const players = Array.from({ length: PLAYERS }, () => connect(`${wsBase}/ws/${room.code}`));
  await Promise.all([host, screen, ...players].map((tab) => tab.opened));
  host.send({ type: 'host', token: room.hostToken });
  players.forEach((tab, i) => tab.send({ type: 'join', name: `Bot ${i + 1}` }));
  await Promise.all(players.map((tab) => tab.until((t) => t.state?.players.length === PLAYERS)));
  return { host, screen, players };
}

// Thinks, sends, and measures until the sender's own state shows the result.
async function timed(tab, message, done) {
  await think();
  if (typeof message === 'function') message = message();
  const started = performance.now();
  tab.send(message);
  await tab.until(done);
  latencies.push(performance.now() - started);
}

async function playRound(room, round) {
  const { host, players } = room;
  await Promise.all(players.map((tab) => tab.until((t) => t.state.phase === 'playing' && t.state.round === round)));
  const juror = players.find((tab) => tab.state.me.isJuror);
  await Promise.all(players.filter((tab) => tab !== juror).map((tab) =>
    timed(tab, () => ({ type: 'submit', token: tab.token, revision: tab.state.me.revision,
      slots: [...Array(tab.state.pick).keys()] }), (t) => t.state.me.submitted || t.state.phase !== 'playing')));
  await juror.until((t) => t.state.phase === 'judging');
  await timed(juror, () => ({ type: 'judge', token: juror.token, revision: juror.state.me.revision, index: 0 }),
    (t) => t.state.phase === 'reveal');
  host.send({ type: 'next' });
}

async function playMatch(room) {
  room.host.send({ type: 'settings', settings: { hand: 7, rounds, answerSecs: 0, extensionSecs: 0,
    jurorSecs: 0, autoSecs: 0, rando: false } });
  room.host.send({ type: 'start' });
  for (let round = 1; round <= rounds; ++round) await playRound(room, round);
}

async function metrics() {
  const text = await (await fetch(`${base}/metrics`)).text();
  const value = (pattern) => Number(text.match(pattern)?.[1] ?? NaN);
  return {
    within10: value(/caw_action_seconds_bucket\{le="0\.01"\} (\d+)/),
    actions: value(/caw_action_seconds_count (\d+)/),
    frameMax: value(/caw_frame_bytes_max (\d+)/),
    frames: value(/caw_frames_total (\d+)/),
    frameBytes: value(/caw_frame_bytes_total (\d+)/),
    resident: value(/caw_resident_bytes (\d+)/),
    tickMax: value(/caw_tick_seconds_max ([\d.]+)/),
  };
}

// --- Worker: opens its share of rooms, waits for "play", plays, reports ---

async function work(share) {
  const opened = [];
  for (let i = 0; i < share; i += OPEN_BATCH) {
    const batch = Math.min(OPEN_BATCH, share - i);
    opened.push(...(await Promise.all(Array.from({ length: batch }, openRoom))));
  }
  process.send({ connected: true });
  await new Promise((resolve) => process.once('message', resolve));
  await Promise.all(opened.map(playMatch));
  for (const room of opened) for (const tab of [room.host, room.screen, ...room.players]) tab.ws.close();
  // Exit only once the report has really left; process.exit() would drop it.
  process.send({ latencies, errors }, () => process.exit(0));
  await new Promise(() => {});
}

if (process.env.CAW_LOAD_SHARE) await work(Number(process.env.CAW_LOAD_SHARE));

// --- Parent: starts the workers, measures the server before, between and after ---

const workers = Math.max(1, Math.min(availableParallelism() - 1, Math.ceil(rooms / 100)));
const before = await metrics();
const started = performance.now();
const children = Array.from({ length: workers }, (_, i) => {
  const share = Math.floor(rooms / workers) + (i < rooms % workers ? 1 : 0);
  return fork(new URL(import.meta.url), process.argv.slice(2), { env: { ...process.env, CAW_LOAD_SHARE: share } });
});
const nextMessage = (child) => new Promise((resolve, reject) => {
  child.once('message', resolve);
  child.once('exit', (code) => code && reject(new Error(`worker failed with ${code}`)));
});
await Promise.all(children.map(nextMessage));
const connected = performance.now();
const middle = await metrics();
const reports = children.map(nextMessage);
for (const child of children) child.send('play');
for (const report of await Promise.all(reports)) {
  latencies.push(...report.latencies);
  errors += report.errors;
}
const after = await metrics();

const actions = after.actions - before.actions;
const serverShare = (after.within10 - before.within10) / actions;
const bytesPerRoom = (middle.resident - before.resident) / rooms;
const typicalFrame = (after.frameBytes - middle.frameBytes) / (after.frames - middle.frames);
const results = [
  ['client p99 (submit/judge → own state)', `${percentile(latencies, 0.99).toFixed(1)} ms`,
    percentile(latencies, 0.99) < BUDGET.clientP99Ms],
  ['client p50', `${percentile(latencies, 0.5).toFixed(1)} ms`, true],
  ['server actions within 10 ms', `${(serverShare * 100).toFixed(2)} % of ${actions}`,
    serverShare >= BUDGET.serverShareWithin10Ms],
  ['average frame while playing', `${Math.round(typicalFrame)} bytes`, typicalFrame < BUDGET.typicalFrameBytes],
  ['largest frame', `${after.frameMax} bytes`, after.frameMax <= BUDGET.frameBytes],
  ['memory per room (10 sockets each)', `${Math.round(bytesPerRoom / 1024)} KB`,
    !Number.isFinite(bytesPerRoom) || bytesPerRoom < BUDGET.bytesPerRoom],
  ['longest clock tick (blocks the loop)', `${(after.tickMax * 1000).toFixed(1)} ms`, true],
  ['errors', String(errors), errors === 0],
];
console.log(`${rooms} rooms × (${PLAYERS} players + host + big screen), ${rounds} rounds each, ` +
  `${workers} load processes`);
console.log(`connected in ${((connected - started) / 1000).toFixed(1)} s, played in ` +
  `${((performance.now() - connected) / 1000).toFixed(1)} s`);
for (const [name, value, ok] of results) console.log(`${ok ? 'PASS' : 'FAIL'} ${name}: ${value}`);
process.exit(results.every(([, , ok]) => ok) ? 0 : 1);
