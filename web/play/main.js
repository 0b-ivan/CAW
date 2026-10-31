// The game page at /s/{code}: one connection, the newest state, and the actions behind every button.
// The server decides everything; this page only shows the state and sends intentions.
import { Connection, lookupSession, secrets } from './connection.js';
import { applyStaticTexts, errorText, setTextLanguage, t } from './text.js';
import { isRunning, isSeated, render, sessionUrl } from './views.js';
import { renderMenu } from './menu.js';

const $ = (id) => document.getElementById(id);
const CODE = /^[A-HJKMNP-Z2-9]{6}$/;
const NAME_BYTES = 23;
const NOTICE_MS = 4000;
const CONFIRM_MS = 3000;

const code = (location.pathname.match(/^\/s\/([^/]+)\/?$/)?.[1] ?? '').toUpperCase();

const app = {
  code,
  frame: null, // the newest state frame
  isHost: false,
  connected: false,
  everConnected: false,
  pending: false, // a resume or host claim is on its way
  missing: false,
  removed: false,
  replaced: false,
  joinError: '',
  selection: [], // hand slots in play order
  selectionKey: '',
  judgePick: null,
  judgeKey: '',
  menu: { open: false, page: 'main' },
  armed: null, // the row waiting for its second tap
  catalog: null, // every pack the server offers, loaded when the host opens PACKS
  flashedRound: 0,
  clock: { left: 0, at: 0, frozen: true },
};

let connection = null;
let retryDelay = 1000;
let noticeTimer = null;
let armTimer = null;

// --- Connection ---

function openConnection() {
  connection?.close();
  connection = new Connection(code, { onOpen, onFrame, onClose });
  connection.open();
}

function onOpen() {
  app.connected = true;
  app.everConnected = true;
  retryDelay = 1000;
  const hostToken = secrets.hostToken(code);
  const ticket = secrets.ticket(code);
  app.pending = !!(hostToken || ticket);
  if (hostToken) connection.send({ type: 'host', token: hostToken });
  if (ticket) connection.send({ type: 'resume', token: ticket });
  update();
}

async function onClose() {
  app.connected = false;
  update();
  if (app.missing || app.removed || app.replaced) return;
  // A restart brings the room back from Redis; a 404 means it is really gone.
  const found = await lookupSession(code);
  if (found.status === 'missing') {
    app.missing = true;
    update();
    return;
  }
  setTimeout(openConnection, retryDelay);
  retryDelay = Math.min(retryDelay * 2, 8000);
}

function onFrame(frame) {
  switch (frame.type) {
    case 'state':
      acceptState(frame);
      break;
    case 'welcome':
      secrets.setTicket(code, frame.token);
      app.joinError = '';
      break;
    case 'host':
      app.isHost = true;
      break;
    case 'expired':
      secrets.setTicket(code, null);
      app.pending = false;
      app.joinError = t('seatExpired');
      if (!app.isHost) app.frame = null;
      break;
    case 'replaced':
      app.replaced = true;
      break;
    case 'kicked':
      secrets.setTicket(code, null);
      app.removed = true;
      app.frame = null;
      app.menu.open = false;
      break;
    case 'error':
      app.pending = false;
      if (app.frame && (isSeated(app) || app.isHost)) showNotice(errorText(frame.code));
      else app.joinError = errorText(frame.code);
      break;
    default:
      return;
  }
  update();
}

function acceptState(frame) {
  app.frame = frame;
  app.pending = false;
  // A selection belongs to one hand in one round; a swap or a new round clears it.
  const handKey = `${frame.round}:${(frame.me.hand ?? []).join('\n')}`;
  if (frame.phase !== 'playing' || handKey !== app.selectionKey) app.selection = [];
  app.selectionKey = handKey;
  const judgeKey = `${frame.round}:${frame.phase}`;
  if (judgeKey !== app.judgeKey) app.judgePick = null;
  app.judgeKey = judgeKey;
  app.clock = { left: frame.left, at: performance.now(), frozen: !!(frame.pausedBy || frame.blockedOn) };
}

// --- Sending ---

function sendPlayer(type, extra = {}) {
  const ticket = secrets.ticket(code);
  if (!ticket || !connection?.send({ type, token: ticket, ...extra })) showNotice(t('offline'));
}

function sendHost(message) {
  if (!connection?.send(message)) showNotice(t('offline'));
}

function sendJoin(input) {
  const name = input.value.trim();
  if (!name) return;
  if (new TextEncoder().encode(name).length > NAME_BYTES) {
    app.joinError = t('nameTooLong');
  } else if (!connection?.send({ type: 'join', name })) {
    app.joinError = t('offline');
  } else {
    app.joinError = '';
    app.pending = true;
  }
  update();
}

const actions = {
  toggleCard(slot) {
    const position = app.selection.indexOf(slot);
    if (position >= 0) app.selection.splice(position, 1);
    else if (app.selection.length < app.frame.pick) app.selection.push(slot);
    update();
  },
  pickPlay(index) {
    app.judgePick = index;
    update();
  },
  player(type) {
    closeMenu();
    sendPlayer(type);
  },
  host(type) {
    closeMenu();
    sendHost({ type });
  },
  choosePacks(ids) {
    sendHost({ type: 'packs', packs: ids });
  },
  loadCatalog() {
    if (app.catalogLoading) return;
    app.catalogLoading = true;
    fetch('/api/packs', { cache: 'no-store' })
      .then((response) => response.json())
      .then((body) => {
        app.catalog = body.packs;
        update();
      })
      .catch(() => showNotice(t('offline')))
      .finally(() => (app.catalogLoading = false));
  },
  kick(seat) {
    sendHost({ type: 'kick', index: seat });
  },
  changeSettings(changes) {
    sendHost({ type: 'settings', settings: { ...app.frame.settings, ...changes } });
  },
  confirmTwice(key, onConfirm) {
    clearTimeout(armTimer);
    if (app.armed === key) {
      app.armed = null;
      onConfirm();
    } else {
      app.armed = key;
      armTimer = setTimeout(() => {
        app.armed = null;
        update();
      }, CONFIRM_MS);
    }
    update();
  },
  openMenuPage(page) {
    app.menu.page = page;
    app.armed = null;
    update();
  },
  setLanguage(language) {
    secrets.setLanguage(language);
    applyLanguage(language);
  },
  copyLink() {
    actions.copyText(sessionUrl(code));
  },
  copyText(text) {
    const done = (key) => showNotice(t(key));
    if (!navigator.clipboard) return done('copyFailed');
    navigator.clipboard.writeText(text).then(() => done('copied'), () => done('copyFailed'));
  },
  share() {
    navigator.share?.({ title: 'C/A/W', url: sessionUrl(code) }).catch(() => {});
  },
};

// --- Page ---

function update() {
  render(app, actions);
  renderMenu(app, actions);
  tickClock();
}

function showNotice(text) {
  clearTimeout(noticeTimer);
  $('notice').textContent = text;
  noticeTimer = setTimeout(() => ($('notice').textContent = ''), NOTICE_MS);
}

function closeMenu() {
  app.menu = { open: false, page: 'main' };
  app.armed = null;
}

function applyLanguage(language) {
  setTextLanguage(language);
  applyStaticTexts();
  document.title = t('title', code);
  update();
}

function mmss(ms) {
  const seconds = Math.ceil(ms / 1000);
  return `${Math.floor(seconds / 60)}:${String(seconds % 60).padStart(2, '0')}`;
}

// The server sends the time left with each state; the page counts down on its own.
function tickClock() {
  const element = $('clock');
  const frame = app.frame;
  if (!frame || !frame.clock || !isRunning(frame)) {
    element.textContent = '';
    element.className = 'clock';
    return;
  }
  const clock = app.clock;
  const left = clock.frozen ? clock.left : Math.max(0, clock.left - (performance.now() - clock.at));
  element.textContent = mmss(left);
  element.classList.toggle('urgent', left <= 10000);
  element.classList.toggle('blink', left > 0 && left <= 10000 && !clock.frozen);
  if (frame.phase === 'reveal' && frame.clock === 'next') $('reveal-status').textContent = t('nextIn', mmss(left));
}

function bindButtons() {
  const on = (id, handler) => $(id).addEventListener('click', handler);
  $('join-form').addEventListener('submit', (event) => {
    event.preventDefault();
    sendJoin($('join-name'));
  });
  $('host-join').addEventListener('submit', (event) => {
    event.preventDefault();
    sendJoin($('host-name'));
  });
  on('start-button', () => actions.host('start'));
  on('submit-button', () => sendPlayer('submit', { revision: app.frame.me.revision, slots: app.selection }));
  on('swap-button', () => sendPlayer('swap', { revision: app.frame.me.revision, index: app.selection[0] }));
  on('judge-button', () => sendPlayer('judge', { revision: app.frame.me.revision, index: app.judgePick }));
  on('close-button', () => actions.host('close'));
  on('next-button', () => actions.host('next'));
  on('again-button', () => actions.player('again'));
  on('rematch-button', () => actions.host('rematch'));
  on('hold-resume', () => (app.frame.me.pausedByMe ? actions.player('unpause') : actions.host('hostUnpause')));
  on('hold-without', () => actions.host('continueWithout'));
  on('pause-button', () => actions.player('pause'));
  on('copy-link', actions.copyLink);
  on('share-link', actions.share);
  on('screen-link', () => {
    app.menu = { open: true, page: 'screen' };
    update();
  });
  on('rejoin-button', () => {
    app.removed = false;
    openConnection();
  });
  on('play-here-button', () => {
    app.replaced = false;
    openConnection();
  });
  on('menu-button', () => {
    app.menu = { open: !app.menu.open, page: 'main' };
    app.armed = null;
    update();
    if (app.menu.open) $('menu-close').focus();
  });
  on('menu-close', () => {
    closeMenu();
    update();
    $('menu-button').focus();
  });
  document.addEventListener('keydown', (event) => {
    if (event.key !== 'Escape' || !app.menu.open) return;
    closeMenu();
    update();
    $('menu-button').focus();
  });
}

async function start() {
  bindButtons();
  applyLanguage(secrets.language() ?? (navigator.language.startsWith('de') ? 'de' : 'en'));
  if (!CODE.test(code)) {
    app.missing = true;
    update();
    return;
  }
  const found = await lookupSession(code);
  if (found.status === 'missing') {
    app.missing = true;
    update();
    return;
  }
  // Without a choice on this device the page speaks the session's language.
  if (!secrets.language() && found.language) applyLanguage(found.language);
  openConnection();
  setInterval(tickClock, 250);
}

start();
