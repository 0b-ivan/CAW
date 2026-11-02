// The big screen at /s/{code}/screen: the public table, large enough to read from 4 m.
// It only listens; the server sends it "screen" frames without any hand.
import { Connection, lookupSession, secrets } from '../play/connection.js';
import { applyStaticTexts, setTextLanguage, t } from '../play/text.js';
import { fillSentence, layoutTapeLines } from '../play/sentence.js';
import { qrSvg } from '../play/qr.js';
import { markHost } from '../play/crown.js';
import { creditsText, nameWithWord } from '../play/views.js';

const $ = (id) => document.getElementById(id);
const CODE = /^[A-HJKMNP-Z2-9]{6}$/;
const VIEWS = ['connecting', 'missing', 'lobby', 'answers', 'judging', 'reveal', 'hold', 'finished'];
const BAR_WIDTH = 20;
const IDLE_MS = 3000;
const LINE_MS = 6000;

const code = (location.pathname.match(/^\/s\/([^/]+)\/screen\/?$/)?.[1] ?? '').toUpperCase();
const screen = { frame: null, missing: false, clock: { left: 0, at: 0, frozen: true } };
let connection = null;
let retryDelay = 1000;
let idleTimer = null;
let lineIndex = 0;

const joinUrl = () => `${location.origin}/s/${code}`;
const isRunning = (frame) => ['playing', 'judging', 'reveal'].includes(frame.phase);
const displayName = (name) => (name === 'host' ? t('theHost') : name);

// --- Connection ---

function openConnection() {
  connection?.close();
  connection = new Connection(code, { onOpen, onFrame, onClose }, '/screen');
  connection.open();
}

function onOpen() {
  retryDelay = 1000;
}

function onFrame(frame) {
  if (frame.type !== 'screen') return;
  screen.frame = frame;
  screen.clock = { left: frame.left, at: performance.now(), frozen: !!(frame.pausedBy || frame.blockedOn) };
  render();
}

async function onClose() {
  const found = await lookupSession(code);
  if (found.status === 'missing') {
    screen.missing = true;
    render();
    return;
  }
  setTimeout(openConnection, retryDelay);
  retryDelay = Math.min(retryDelay * 2, 8000);
}

// --- Views ---

function chooseView() {
  const frame = screen.frame;
  if (screen.missing) return 'missing';
  if (!frame) return 'connecting';
  if (isRunning(frame) && (frame.pausedBy || frame.blockedOn)) return 'hold';
  return { lobby: 'lobby', playing: 'answers', judging: 'judging', reveal: 'reveal' }[frame.phase] ?? 'finished';
}

function render() {
  const view = chooseView();
  for (const name of VIEWS) $(`screen-${name}`).hidden = name !== view;
  $('screen-status').textContent = '';
  renderMeta(view);
  // Lobby and end name the packs' rights holders and licenses (CC BY-NC-SA asks for it).
  const credited = view === 'lobby' || view === 'finished';
  $('screen-credits').textContent = credited ? creditsText(screen.frame.packs) : '';
  const draw = { lobby: renderLobby, answers: renderAnswers, judging: renderJudging, reveal: renderReveal,
    hold: renderHold, finished: renderFinished }[view];
  if (draw) draw(screen.frame);
  // Juror sentences were built detached, so measure their tape after insertion.
  layoutTapeLines();
  fitAll();
  tickClock();
}

function renderMeta(view) {
  const frame = screen.frame;
  if (!frame || view === 'lobby' || view === 'finished' || view === 'missing') {
    $('screen-meta').textContent = t('sessionCode', code);
  } else if (view === 'judging') {
    $('screen-meta').textContent = `${t('round', frame.round, frame.rounds)} · ${t('jurorDecides', frame.juror)}`;
  } else {
    $('screen-meta').textContent = `${t('round', frame.round, frame.rounds)} · ${t('juror', frame.juror)}`;
  }
}

function renderLobby(frame) {
  const box = $('screen-qr');
  if (box.dataset.url !== joinUrl()) {
    box.dataset.url = joinUrl();
    box.replaceChildren(qrSvg(joinUrl(), t('qrLabel')));
  }
  $('screen-code').textContent = code;
  $('screen-url').textContent = joinUrl().replace(/^https?:\/\//, '');
  $('screen-count').textContent = t('players', frame.players.length);
  $('screen-players').replaceChildren(...frame.players.map((player) => {
    const name = nameWithWord(player);
    const entry = item(name);
    return player.isHost ? markHost(entry, name, t('hostRole')) : entry;
  }));
  $('screen-status').textContent = `${t('screenJoin')} · ${t('waitForHost')}`;
}

// No names of who is missing: only the count, like on the phones.
function renderAnswers(frame) {
  fillSentence($('screen-prompt'), frame.prompt, []);
  const bar = '█'.repeat(frame.submitted) + '░'.repeat(Math.max(0, frame.expected - frame.submitted));
  $('screen-progress').textContent = `${t('submittedBar')} ${bar} ${frame.submitted}/${frame.expected}`;
}

function renderJudging(frame) {
  fillSentence($('screen-judge-prompt'), frame.prompt, []);
  const list = $('screen-plays');
  list.classList.toggle('two-columns', frame.plays.length > 4);
  list.replaceChildren(...frame.plays.map((play, index) => {
    const entry = document.createElement('li');
    const number = document.createElement('span');
    number.className = 'choice-number';
    number.textContent = `[${index + 1}] `;
    const sentence = document.createElement('span');
    fillSentence(sentence, frame.prompt, play.cards);
    entry.append(number, sentence);
    return entry;
  }));
}

function renderReveal(frame) {
  const winning = frame.selected === null ? null : frame.plays[frame.selected];
  if (winning) fillSentence($('screen-sentence'), frame.prompt, winning.cards);
  else $('screen-sentence').textContent = t('roundAborted');
  $('screen-winner').textContent = winning ? t('winner', frame.winner || t('rando')) : '';
  $('screen-scores').replaceChildren(...ranked(frame).map((p) => item(`${p.name} ${p.points}`)));
  if (frame.clock !== 'next') $('screen-status').textContent = t('hostContinues');
}

function renderHold(frame) {
  if (frame.pausedBy) {
    $('screen-hold-title').textContent = t('paused');
    $('screen-hold-text').textContent = t('pausedBy', displayName(frame.pausedBy));
  } else {
    $('screen-hold-title').textContent = t('waitingFor', frame.blockedOn);
    $('screen-hold-text').textContent = t('blockedText', frame.blockedOn);
  }
}

function renderFinished(frame) {
  const players = ranked(frame);
  const best = players.length ? players[0].points : 0;
  const winners = players.filter((p) => p.points === best).map((p) => p.name);
  $('screen-champion').textContent = `${winners.join(' / ')} ${t(winners.length > 1 ? 'winTie' : 'wins')}`;
  $('screen-bars').replaceChildren(...players.map((player) => {
    const entry = document.createElement('li');
    const name = document.createElement('span');
    name.textContent = player.name;
    const bar = document.createElement('span');
    const length = best ? Math.round((player.points / best) * BAR_WIDTH) : 0;
    bar.textContent = `${'█'.repeat(length)} ${player.points}`;
    entry.append(name, bar);
    return entry;
  }));
  $('screen-status').textContent = t('votes', frame.votes, frame.players.length);
  showLine();
}

// At the end the match's winning sentences take turns under the standings.
function showLine() {
  const lines = screen.frame?.lines ?? [];
  $('screen-line').hidden = lines.length === 0;
  if (!lines.length) return;
  const line = lines[lineIndex % lines.length];
  $('screen-line-text').textContent = `${line.name}: ${line.sentence}`;
  fitAll();
}

function nextLine() {
  if (chooseView() !== 'finished') return;
  lineIndex += 1;
  showLine();
}

function ranked(frame) {
  return [...frame.players].sort((a, b) => b.points - a.points);
}

function item(text) {
  const entry = document.createElement('li');
  entry.textContent = text;
  return entry;
}

// --- Fitting text to the screen ---

// Starts large and steps down until the text fits its box: no scrolling on a TV.
function fit(box) {
  let size = Math.max(40, Math.min(96, innerWidth * 0.05));
  box.style.fontSize = `${size}px`;
  while ((box.scrollHeight > box.clientHeight || box.scrollWidth > box.clientWidth) && size > 16) {
    size *= 0.92;
    box.style.fontSize = `${size}px`;
  }
}

function fitAll() {
  for (const box of document.querySelectorAll('.screen-view:not([hidden]) .fit')) fit(box);
}

// --- Clock, full screen, start ---

function mmss(ms) {
  const seconds = Math.ceil(ms / 1000);
  return `${Math.floor(seconds / 60)}:${String(seconds % 60).padStart(2, '0')}`;
}

function tickClock() {
  const element = $('screen-clock');
  const frame = screen.frame;
  if (!frame || !frame.clock || !isRunning(frame)) {
    element.textContent = '';
    element.className = 'clock screen-clock';
    return;
  }
  const clock = screen.clock;
  const left = clock.frozen ? clock.left : Math.max(0, clock.left - (performance.now() - clock.at));
  element.textContent = mmss(left);
  element.classList.toggle('urgent', left <= 10000);
  element.classList.toggle('blink', left > 0 && left <= 10000 && !clock.frozen);
  if (frame.phase === 'reveal' && frame.clock === 'next') $('screen-status').textContent = t('nextIn', mmss(left));
}

// The only control fades out after three seconds without the mouse.
function wake() {
  document.body.classList.remove('idle');
  clearTimeout(idleTimer);
  idleTimer = setTimeout(() => document.body.classList.add('idle'), IDLE_MS);
}

async function start() {
  setTextLanguage(secrets.language() ?? (navigator.language.startsWith('de') ? 'de' : 'en'));
  applyStaticTexts();
  $('fullscreen').addEventListener('click', () => document.documentElement.requestFullscreen?.());
  document.addEventListener('mousemove', wake);
  // Refit after sentence.js has rebuilt tape lines for the new viewport.
  addEventListener('resize', () => requestAnimationFrame(fitAll));
  wake();
  if (!CODE.test(code)) {
    screen.missing = true;
    render();
    return;
  }
  const found = await lookupSession(code);
  if (found.status === 'missing') {
    screen.missing = true;
    render();
    return;
  }
  if (!secrets.language() && found.language) {
    setTextLanguage(found.language);
    applyStaticTexts();
  }
  document.title = t('screenTitle', code);
  render();
  openConnection();
  setInterval(tickClock, 250);
  setInterval(nextLine, LINE_MS);
}

start();
