// Draws the current state into the static sections of play.html.
// Every function only reads `app` and calls `actions`; nothing here talks to the server.
import { t } from './text.js';
import { fillSentence } from './sentence.js';
import { qrSvg } from './qr.js';
import { hostCrown, markHost } from './crown.js';
import { configuredInviteOrigin, isLoopback, shareableSessionUrl } from './invite.js';

const NO_SEAT = 255;
const $ = (id) => document.getElementById(id);
const VIEWS = ['connecting', 'missing', 'join', 'lobby', 'answer', 'waiting', 'judge', 'reveal',
  'finished', 'hold', 'removed', 'replaced'];

export { sessionUrl } from './invite.js';

export function isSeated(app) {
  return !!app.frame && app.frame.me.seat !== NO_SEAT;
}

export function isRunning(frame) {
  return !!frame && ['playing', 'judging', 'reveal'].includes(frame.phase);
}

// "host" stands for the host connection, everything else is a player name.
export function displayName(name) {
  return name === 'host' ? t('theHost') : name;
}

// "KARTEN: Cards vs Humanity · CC BY-NC-SA 2.0": who owns the cards in play and under which license.
// "Ada·Toast": the word tells two Adas apart.
export function nameWithWord(player) {
  return player.word ? `${player.name}·${player.word}` : player.name;
}

export function creditsText(packs) {
  const credits = [...new Set(packs.map((pack) => `${pack.rightsHolder} · ${pack.license}`))];
  return credits.length ? t('credits', credits.join(' / ')) : '';
}

export function chooseView(app) {
  if (app.missing) return 'missing';
  if (app.replaced) return 'replaced';
  if (app.removed) return 'removed';
  const frame = app.frame;
  if (!frame) return app.connected && !app.pending ? 'join' : 'connecting';
  if (!isSeated(app) && !app.isHost) return 'join';
  if (isRunning(frame) && (frame.pausedBy || frame.blockedOn)) return 'hold';
  if (frame.phase === 'lobby') return 'lobby';
  if (frame.phase === 'playing') {
    const me = frame.me;
    return isSeated(app) && !me.isJuror && !me.submitted ? 'answer' : 'waiting';
  }
  if (frame.phase === 'judging') return 'judge';
  if (frame.phase === 'reveal') return 'reveal';
  return 'finished';
}

export function render(app, actions) {
  const view = chooseView(app);
  for (const name of VIEWS) $(`view-${name}`).hidden = name !== view;
  renderMast(app, view);
  const draw = {
    join: renderJoin, lobby: renderLobby, answer: renderAnswer, waiting: renderWaiting,
    judge: renderJudge, reveal: renderReveal, finished: renderFinished, hold: renderHold,
  }[view];
  if (draw) draw(app, actions);
}

function renderMast(app, view) {
  const status = app.connected ? 'connected' : app.everConnected ? 'offline' : 'connecting';
  $('connection').textContent = view === 'missing' ? '' : t(status);
  // A non-playing host has no player row to wear the crown.
  const badge = $('host-badge');
  badge.hidden = !app.isHost || isSeated(app);
  if (!badge.hidden) {
    if (!badge.firstChild) {
      const title = document.createElement('span');
      badge.append(hostCrown(), title);
    }
    badge.lastChild.textContent = t('hostRole');
  }
  const frame = app.frame;
  const seatedRunning = isSeated(app) && isRunning(frame) && !frame.pausedBy && !frame.blockedOn;
  $('pause-button').hidden = !seatedRunning || view === 'hold';
  if (!frame || frame.phase === 'lobby' || frame.phase === 'finished') {
    $('meta-left').textContent = t('sessionCode', app.code);
  } else {
    const juror = frame.juror ? ` · ${t('juror', frame.juror)}` : '';
    $('meta-left').textContent = t('round', frame.round, frame.rounds) + juror;
  }
}

function renderJoin(app) {
  $('join-code').textContent = app.code;
  $('join-error').textContent = app.joinError;
}

// --- Lobby ---

function renderLobby(app) {
  const frame = app.frame;
  const players = frame.players;
  $('lobby-share').hidden = !app.isHost;
  $('view-lobby').classList.toggle('has-host', app.isHost);
  if (app.isHost) renderShare(app);
  $('lobby-count').textContent = t('players', players.length);
  $('lobby-players').replaceChildren(...rosterItems(app));
  $('lobby-credits').textContent = creditsText(frame.packs);
  $('host-join').hidden = !app.isHost || isSeated(app);
  $('start-button').hidden = !app.isHost;
  $('start-button').disabled = players.length < 3;
  if (!app.isHost) $('lobby-status').textContent = t('waitForHost');
  else if (frame.locked) $('lobby-status').textContent = t('lobbyLocked');
  else $('lobby-status').textContent = players.length < 3 ? t('needPlayers', 3 - players.length) : t('ready');
}

function rosterItems(app) {
  if (!app.frame.players.length) return [listItem(t('noPlayers'))];
  return app.frame.players.map((player) => {
    let label = nameWithWord(player);
    if (player.seat === app.frame.me.seat) label += t('youMark');
    if (!player.connected) label += t('offlineMark');
    const item = listItem(label);
    return player.isHost ? markHost(item, label, t('hostRole')) : item;
  });
}

// The QR Code is drawn once per link; redrawing it on every frame would be wasted work.
function renderShare(app) {
  const url = shareableSessionUrl(app.code);
  $('lobby-share').classList.toggle('needs-address', !url);
  $('lobby-code').textContent = app.code;
  $('lobby-url').textContent = url ? url.replace(/^https?:\/\//, '') : t('inviteAddressNeeded');
  $('share-link').hidden = typeof navigator.share !== 'function';
  $('copy-link').disabled = !url;
  $('share-link').disabled = !url;
  const row = $('invite-address-row');
  row.hidden = !isLoopback(location.hostname) && !configuredInviteOrigin(app.code);
  const input = $('invite-address');
  if (document.activeElement !== input) {
    input.value = configuredInviteOrigin(app.code) ?? '';
    $('invite-hint').textContent = url ? '' : t('inviteAddressNeeded');
  }
  const box = $('lobby-qr');
  box.hidden = !url;
  if (box.dataset.url === (url ?? '')) return;
  box.dataset.url = url ?? '';
  box.replaceChildren(...(url ? [qrSvg(url, t('qrLabel'))] : []));
}

// --- Answers ---

function renderAnswer(app, actions) {
  const frame = app.frame;
  const hand = frame.me.hand;
  const chosen = app.selection.map((slot) => hand[slot]);
  $('answer-eyebrow').textContent = t('pick', frame.pick);
  // The prompt doubles as the preview: chosen cards appear in the blanks.
  fillSentence($('answer-prompt'), frame.prompt, chosen);

  // Render every server slot in one scrolling hand. Remember the scroll offset
  // across picks, but reset it when the round or hand changes.
  const list = $('hand');
  const sameHand = list.dataset.handKey === app.selectionKey;
  const scroll = sameHand ? list.scrollTop : 0;
  const focused = [...list.children].indexOf(document.activeElement);
  list.replaceChildren(...hand.map((card, slot) => cardButton(app, actions, card, slot)));
  list.dataset.handKey = app.selectionKey;
  list.scrollTop = scroll;
  if (sameHand && focused >= 0 && list.children[focused]) {
    list.children[focused].focus({ preventScroll: true });
  }

  $('hand-title').textContent = t('handTitle', hand.length);

  $('answer-count').textContent = t('selected', app.selection.length, frame.pick);
  $('submit-button').disabled = app.selection.length !== frame.pick;
  $('swap-button').textContent = t(frame.me.swapped ? 'swapped' : 'swap');
  $('swap-button').disabled = frame.me.swapped || app.selection.length !== 1;
}

function cardButton(app, actions, card, slot) {
  const button = document.createElement('button');
  button.type = 'button';
  button.className = 'card';
  const position = app.selection.indexOf(slot);
  button.setAttribute('aria-pressed', String(position >= 0));
  if (position >= 0) {
    const mark = document.createElement('span');
    mark.className = 'slot-mark';
    mark.textContent = t('slot', position + 1, app.frame.pick);
    button.append(mark, ' ');
    button.setAttribute('aria-label', t('slotLabel', position + 1, app.frame.pick, card));
  }
  button.append(card);
  button.addEventListener('click', () => actions.toggleCard(slot));
  return button;
}

// --- Waiting for answers (players who are done, the Juror, a host without a seat) ---

function renderWaiting(app) {
  const frame = app.frame;
  $('waiting-eyebrow').textContent = t('pick', frame.pick);
  fillSentence($('waiting-prompt'), frame.prompt, []);
  const title = !isSeated(app) ? 'answersTitle' : frame.me.isJuror ? 'answersTitle' : 'submittedTitle';
  $('waiting-title').textContent = t(title, frame.submitted, frame.expected);
  $('waiting-progress').textContent = progressBar(frame.submitted, frame.expected);
  $('waiting-hint').textContent = isSeated(app) && frame.me.isJuror ? t('youAreJuror') : t('waitAnswers');
  $('close-button').hidden = !app.isHost || frame.submitted === 0;
}

function progressBar(done, total) {
  return '█'.repeat(done) + '░'.repeat(Math.max(0, total - done));
}

// --- Juror ---

function renderJudge(app, actions) {
  const frame = app.frame;
  const canJudge = isSeated(app) && frame.me.isJuror;
  $('judge-eyebrow').textContent = canJudge ? t('chooseWinner') : t('jurorDecides', frame.juror);
  // The prompt stands once on top; the Juror's pick appears in its blanks, like a hand pick does.
  const picked = canJudge && app.judgePick !== null ? frame.plays[app.judgePick].cards : [];
  fillSentence($('judge-prompt'), frame.prompt, picked);
  const list = $('judge-list');
  const scroll = list.scrollTop;
  list.replaceChildren(...frame.plays.map((play, index) => playItem(app, actions, play, index, canJudge)));
  list.scrollTop = scroll;
  $('judge-button').hidden = !canJudge;
  $('judge-button').disabled = app.judgePick === null;
  $('judge-status').textContent = !canJudge ? '' :
    app.judgePick === null ? t('nothingChosen') : t('chosen', app.judgePick + 1);
}

function playItem(app, actions, play, index, canJudge) {
  const item = document.createElement('li');
  const body = document.createElement(canJudge ? 'button' : 'div');
  body.className = 'choice';
  const number = document.createElement('span');
  number.className = 'choice-number';
  number.textContent = `[${index + 1}]`;
  // Only the answers: repeating a long prompt in every play would double the list.
  const sentence = document.createElement('span');
  sentence.className = 'choice-content';
  sentence.append(number);
  play.cards.forEach((card, i) => {
    const mark = document.createElement('mark');
    mark.textContent = card;
    sentence.append(i === 0 ? ' ' : ' / ', mark);
  });
  body.append(sentence);
  if (canJudge) {
    const indicator = document.createElement('span');
    indicator.className = 'choice-indicator';
    indicator.setAttribute('aria-hidden', 'true');
    body.append(indicator);
    body.type = 'button';
    body.setAttribute('aria-pressed', String(app.judgePick === index));
    body.addEventListener('click', () => actions.pickPlay(index));
  }
  item.append(body);
  return item;
}

// --- Reveal ---

function renderReveal(app) {
  const frame = app.frame;
  $('reveal-eyebrow').textContent = t('revealTitle', frame.round, frame.rounds);
  const winning = frame.selected === null ? null : frame.plays[frame.selected];
  if (winning) fillSentence($('reveal-sentence'), frame.prompt, winning.cards);
  else $('reveal-sentence').textContent = t('roundAborted');
  // Rando has no seat and so no name in the state.
  const winner = winning ? frame.winner || t('rando') : '';
  $('reveal-winner').textContent = winner ? t('winner', winner) : '';
  const iWon = isSeated(app) && winner !== '' && winner === myName(app);
  $('reveal-flash').hidden = !iWon;
  if (iWon && app.flashedRound !== frame.round) {
    app.flashedRound = frame.round;
    restartAnimation($('reveal-flash'));
  }
  $('reveal-scores').replaceChildren(...rankedPlayers(frame).map((p) => scoreItem(p.name, p.points)));
  const last = frame.round >= frame.rounds;
  $('next-button').hidden = !app.isHost || frame.clock === 'next';
  $('next-button').textContent = t(last ? 'toResult' : 'nextRound');
  if (frame.clock !== 'next') $('reveal-status').textContent = app.isHost ? '' : t('hostContinues');
}

function myName(app) {
  return app.frame.players.find((player) => player.seat === app.frame.me.seat)?.name ?? '';
}

function restartAnimation(element) {
  element.classList.remove('flash-on');
  void element.offsetWidth;
  element.classList.add('flash-on');
}

function rankedPlayers(frame) {
  return [...frame.players].sort((a, b) => b.points - a.points);
}

function scoreItem(name, points) {
  const item = document.createElement('li');
  const label = document.createElement('span');
  label.textContent = name;
  const value = document.createElement('span');
  value.textContent = t('points', points);
  item.append(label, value);
  return item;
}

// --- End of the match ---

const BAR_WIDTH = 10;

function renderFinished(app) {
  const frame = app.frame;
  const ranked = rankedPlayers(frame);
  const best = ranked.length ? ranked[0].points : 0;
  const winners = ranked.filter((p) => p.points === best).map((p) => p.name);
  $('finished-eyebrow').textContent = t('gameOver');
  $('finished-winner').textContent = `${winners.join(' / ')} ${t(winners.length > 1 ? 'winTie' : 'wins')}`;
  $('finished-bars').replaceChildren(...ranked.map((p) => barItem(p, best)));
  $('finished-votes').textContent = t('votes', frame.votes, frame.players.length);
  $('finished-credits').textContent = creditsText(frame.packs);
  $('finished-lines-title').hidden = frame.lines.length === 0;
  $('finished-lines').replaceChildren(...frame.lines.map((line) => listItem(`${line.name}: ${line.sentence}`)));
  const voter = isSeated(app) && !app.isHost;
  $('again-button').hidden = !voter;
  $('again-button').disabled = voter && frame.me.voted;
  $('again-button').textContent = t(voter && frame.me.voted ? 'againDone' : 'again');
  $('rematch-button').hidden = !app.isHost;
}

function barItem(player, best) {
  const item = document.createElement('li');
  const name = document.createElement('span');
  name.textContent = player.name;
  const bar = document.createElement('span');
  bar.className = 'bar-chars';
  const length = best ? Math.round((player.points / best) * BAR_WIDTH) : 0;
  bar.textContent = `${'█'.repeat(length)} ${player.points}`;
  item.append(name, bar);
  return item;
}

// --- Pause and waiting for a player who dropped ---

function renderHold(app) {
  const frame = app.frame;
  const paused = !!frame.pausedBy;
  if (paused) {
    const who = displayName(frame.pausedBy);
    $('hold-eyebrow').textContent = t('paused');
    $('hold-title').textContent = t('pausedBy', who);
    $('hold-text').textContent = frame.me.pausedByMe ? t('pausedMine') : t('pausedOther', who);
  } else {
    $('hold-eyebrow').textContent = t('waiting');
    $('hold-title').textContent = t('waitingFor', frame.blockedOn);
    $('hold-text').textContent = t('blockedText', frame.blockedOn);
  }
  $('hold-resume').hidden = !paused || !(frame.me.pausedByMe || app.isHost);
  $('hold-without').hidden = paused || !app.isHost;
  $('hold-without').textContent = t('continueWithout', frame.blockedOn);
}

function listItem(text) {
  const item = document.createElement('li');
  item.textContent = text;
  return item;
}
