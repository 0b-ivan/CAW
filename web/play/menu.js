// The ≡ menu: plain rows grouped under GAME, HOST and SESSION, plus the rules
// and players pages for the host. Dangerous rows need a second tap.
import { t, currentLanguage } from './text.js';
import { isRunning, isSeated } from './views.js';

const $ = (id) => document.getElementById(id);

// The values the X4 offers on its rules screen; the server accepts exactly these.
const RULES = [
  { key: 'rounds', label: 'rounds', options: [3, 5, 8, 10, 12, 15, 20, 30] },
  { key: 'answerSecs', label: 'answerClock', options: [0, 30, 45, 60, 90, 120] },
  { key: 'extensionSecs', label: 'extension', options: [0, 10, 15, 20, 30] },
  { key: 'jurorSecs', label: 'jurorClock', options: [0, 30, 60, 90] },
  { key: 'autoSecs', label: 'autoNext', options: [0, 5, 8, 12] },
];

export function renderMenu(app, actions) {
  const menu = $('menu');
  menu.hidden = !app.menu.open;
  $('menu-button').setAttribute('aria-expanded', String(app.menu.open));
  if (!app.menu.open) return;
  const page = app.isHost && app.frame ? app.menu.page : 'main';
  const pages = { main: mainPage, rules: rulesPage, players: playersPage };
  $('menu-title').textContent = page === 'main' ? t('sessionCode', app.code) : t(page === 'rules' ? 'rules' : 'playersMenu');
  $('menu-body').replaceChildren(...pages[page](app, actions));
}

function mainPage(app, actions) {
  return [...gameGroup(app, actions), ...hostGroup(app, actions), ...sessionGroup(app, actions)];
}

function gameGroup(app, actions) {
  const frame = app.frame;
  const rows = [heading('groupGame')];
  if (isSeated(app) && isRunning(frame) && !app.isHost) {
    if (frame.me.pausedByMe) rows.push(row(t('hostResume'), '', () => actions.player('unpause')));
    else if (!frame.pausedBy) rows.push(row(t('hostPause'), '', () => actions.player('pause')));
  }
  const next = currentLanguage() === 'de' ? 'en' : 'de';
  rows.push(row(t('language'), currentLanguage().toUpperCase(), () => actions.setLanguage(next)));
  return rows;
}

function hostGroup(app, actions) {
  const frame = app.frame;
  if (!app.isHost || !frame) return [];
  const rows = [heading('groupHost')];
  const phase = frame.phase;
  if (phase === 'lobby' && frame.players.length >= 3) rows.push(row(t('startRow'), '', () => actions.host('start')));
  if (isRunning(frame)) {
    if (frame.pausedBy) rows.push(row(t('hostResume'), '', () => actions.host('hostUnpause')));
    else rows.push(row(t('hostPause'), '', () => actions.host('hostPause')));
  }
  if (phase === 'playing' && frame.submitted > 0) rows.push(row(t('revealNow'), '', () => actions.host('close')));
  if (frame.clock === 'answers' || frame.clock === 'juror') rows.push(row(t('addTime'), '', () => actions.host('addTime')));
  if (phase === 'playing' || phase === 'judging') rows.push(confirmRow(app, actions, 'abort', t('abortRound'), () => actions.host('abort')));
  rows.push(row(t('rules'), '›', () => actions.openMenuPage('rules')));
  rows.push(row(t('playersMenu'), String(frame.players.length), () => actions.openMenuPage('players')));
  if (isRunning(frame)) rows.push(confirmRow(app, actions, 'end', t('endGame'), () => actions.host('end')));
  if (phase === 'finished') rows.push(row(t('rematchRow'), '', () => actions.host('rematch')));
  return rows;
}

function sessionGroup(app, actions) {
  const rows = [heading('groupSession'), row(t('copyLinkRow'), '', () => actions.copyLink())];
  if (typeof navigator.share === 'function') rows.push(row(t('shareRow'), '', () => actions.share()));
  const home = document.createElement('a');
  home.className = 'menu-row';
  home.href = '/';
  home.append(cell(t('homeRow')), cell(''));
  rows.push(home);
  return rows;
}

// --- Rules ---

function rulesPage(app, actions) {
  const settings = app.frame.settings;
  const rows = [backRow(actions)];
  for (const rule of RULES) {
    const locked = rule.key === 'rounds' && app.frame.phase !== 'lobby';
    rows.push(stepperRow(rule, settings[rule.key], locked, (value) => actions.changeSettings({ [rule.key]: value })));
  }
  rows.push(row(t('randoRule'), t(settings.rando ? 'on' : 'off'), () => actions.changeSettings({ rando: !settings.rando })));
  if (app.frame.phase !== 'lobby') rows.push(note(t('roundsLocked')));
  return rows;
}

function optionText(rule, value) {
  if (rule.key === 'rounds') return String(value);
  return value === 0 ? t('off') : t('seconds', value);
}

function stepperRow(rule, value, locked, onChange) {
  const index = rule.options.indexOf(value);
  const line = document.createElement('div');
  line.className = 'menu-row stepper';
  const label = cell(t(rule.label));
  const fewer = stepButton('‹', 'fewer', locked || index <= 0, () => onChange(rule.options[index - 1]));
  const shown = cell(optionText(rule, value));
  shown.className = 'stepper-value';
  const more = stepButton('›', 'more', locked || index >= rule.options.length - 1, () => onChange(rule.options[index + 1]));
  line.append(label, fewer, shown, more);
  return line;
}

function stepButton(symbol, labelKey, disabled, onClick) {
  const button = document.createElement('button');
  button.type = 'button';
  button.className = 'mini step';
  button.textContent = `[${symbol}]`;
  button.setAttribute('aria-label', t(labelKey));
  button.disabled = disabled;
  button.addEventListener('click', onClick);
  return button;
}

// --- Players ---

function playersPage(app, actions) {
  const rows = [backRow(actions)];
  if (!app.frame.players.length) rows.push(note(t('noPlayers')));
  for (const player of app.frame.players) {
    const label = `${player.name}${player.connected ? '' : t('offlineMark')} · ${player.points}`;
    rows.push(confirmRow(app, actions, `kick-${player.seat}`, label, () => actions.kick(player.seat), t('remove')));
  }
  return rows;
}

// --- Building blocks ---

function heading(key) {
  const element = document.createElement('p');
  element.className = 'eyebrow menu-heading';
  element.textContent = t(key);
  return element;
}

function note(text) {
  const element = document.createElement('p');
  element.className = 'menu-note';
  element.textContent = text;
  return element;
}

function cell(text) {
  const span = document.createElement('span');
  span.textContent = text;
  return span;
}

function row(label, value, onClick) {
  const button = document.createElement('button');
  button.type = 'button';
  button.className = 'menu-row';
  button.append(cell(label), cell(value));
  button.addEventListener('click', onClick);
  return button;
}

// First tap arms the row and asks for a second one within three seconds.
function confirmRow(app, actions, key, label, onConfirm, value = '') {
  const armed = app.armed === key;
  const button = row(label, armed ? t('tapAgain') : value, () => actions.confirmTwice(key, onConfirm));
  button.setAttribute('aria-pressed', String(armed));
  return button;
}

function backRow(actions) {
  return row(t('backRow'), '‹', () => actions.openMenuPage('main'));
}
