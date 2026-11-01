// The game page at /s/{code}: one connection, the newest state, and the actions behind every button.
// The server decides everything; this page only shows the state and sends intentions.
import { Connection, lookupSession, secrets } from './connection.js';
import { applyStaticTexts, errorText, setTextLanguage, t } from './text.js';
import { isRunning, isSeated, render } from './views.js';
import { saveInviteOrigin, shareableSessionUrl } from './invite.js';
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
  editInvite: false,
  selection: [], // hand slots in play order
  selectionKey: '',
  judgePick: null,
  judgeKey: '',
  menu: { open: false, page: 'main' },
  armed: null, // the row waiting for its second tap
  catalog: null, // every pack the server offers, loaded when the host opens PACKS
  editor: null, // room-only custom cards, editable only in the lobby
  editorLoading: false,
  editorSaving: false,
  fame: null, // the Wall of Fame, loaded when the menu page opens
  famePage: 0,
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
  if (frame.phase !== 'playing' || handKey !== app.selectionKey) {
    app.selection = [];
  }
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
  } else if (!connection?.send({ type: 'join', name, player: secrets.playerId() })) {
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
  refresh() {
    update();
  },
  loadCards() {
    const token = secrets.hostToken(code);
    if (!token || app.editorLoading) return;
    app.editorLoading = true;
    fetch('/api/sessions/' + code + '/cards', {
      headers: { Authorization: 'Bearer ' + token }, cache: 'no-store',
    }).then((response) => {
      if (!response.ok) throw new Error('Custom cards unavailable');
      return response.json();
    }).then((cards) => {
      app.editor = { ...cards, dirty: false, status: '' };
    }).catch(() => {
      app.editor = { prompts: [], answers: [], dirty: false, status: t('customLoadFailed') };
    }).finally(() => {
      app.editorLoading = false;
      update();
    });
  },
  async saveCards() {
    if (!app.editor || app.editorSaving || app.frame.phase !== 'lobby') return;
    const trim = (cards) => cards.map((text) => text.trim()).filter(Boolean);
    const cards = { prompts: trim(app.editor.prompts), answers: trim(app.editor.answers) };
    const valid = [...cards.prompts, ...cards.answers].every((text) =>
      new TextEncoder().encode(text).length <= 300);
    if (!valid || (cards.prompts.length + cards.answers.length === 0 && !app.editor.id)) {
      app.editor.status = t('customInvalid');
      update();
      return;
    }
    app.editorSaving = true;
    try {
      const response = await fetch('/api/sessions/' + code + '/cards', {
        method: 'PUT',
        headers: { Authorization: 'Bearer ' + secrets.hostToken(code), 'Content-Type': 'application/json' },
        body: JSON.stringify(cards),
      });
      if (!response.ok) throw new Error(String(response.status));
      const saved = await response.json();
      app.editor = { ...saved, dirty: false,
        status: saved.id ? t('customSaved') : t('customCleared') };
      app.catalog = null;
    } catch (error) {
      app.editor.status = error.message === '409' ? t('customUnavailable') : t('customSaveFailed');
    } finally {
      app.editorSaving = false;
      update();
    }
  },
  exportCards() {
    if (!app.editor) return;
    const content = JSON.stringify({
      prompts: app.editor.prompts,
      answers: app.editor.answers,
    }, null, 2);
    const url = URL.createObjectURL(new Blob([content], { type: 'application/json' }));
    const link = document.createElement('a');
    link.href = url;
    link.download = 'caw-eigene-karten.json';
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  },
  async importCards(file) {
    try {
      if (file.size > 128 * 1024) throw new Error('oversized');
      const parsed = JSON.parse(await file.text());
      if (!parsed || !Array.isArray(parsed.prompts) || !Array.isArray(parsed.answers) ||
          parsed.prompts.length > 100 || parsed.answers.length > 100 ||
          ![...parsed.prompts, ...parsed.answers].every((text) => typeof text === 'string')) {
        throw new Error('invalid');
      }
      app.editor = { prompts: parsed.prompts, answers: parsed.answers,
        dirty: true, status: t('customImported') };
    } catch {
      app.editor.status = t('customImportFailed');
    }
    update();
  },
  loadFame() {
    fetch('/api/fame', { cache: 'no-store' })
      .then((response) => response.json())
      .then((body) => {
        app.fame = body;
        update();
      })
      .catch(() => showNotice(t('offline')));
  },
  turnFamePage(page) {
    app.famePage = page;
    update();
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
    if (page === 'fame') {
      app.fame = null;
      app.famePage = 0;
    }
    app.menu.page = page;
    app.armed = null;
    update();
  },
  setLanguage(language) {
    secrets.setLanguage(language);
    applyLanguage(language);
  },
  copyLink() {
    const url = shareableSessionUrl(code);
    if (!url) return showNotice(t('inviteAddressNeeded'));
    actions.copyText(url);
  },
  setInviteOrigin(value) {
    if (!saveInviteOrigin(code, value)) {
      $('invite-hint').textContent = t('inviteAddressInvalid');
      return;
    }
    app.editInvite = false;
    update();
  },
  copyText(text) {
    const done = (key) => showNotice(t(key));
    if (!navigator.clipboard) return done('copyFailed');
    navigator.clipboard.writeText(text).then(() => done('copied'), () => done('copyFailed'));
  },
  share() {
    const url = shareableSessionUrl(code);
    if (!url) return showNotice(t('inviteAddressNeeded'));
    navigator.share?.({ title: 'C/A/W', url }).catch(() => {});
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
  on('lobby-url', () => {
    app.editInvite = !app.editInvite;
    update();
    if (app.editInvite) $('invite-address').focus();
  });
  $('invite-address').addEventListener('change', (event) => {
    actions.setInviteOrigin(event.target.value);
  });
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
