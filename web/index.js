// Landing page: new session, join by code, language and the one-shot Cut Deck introduction.
import { translations } from './translations.js';

const landing = document.querySelector('#landing');
const boot = document.querySelector('#boot-screen');
const replayButton = document.querySelector('#replay-intro');
const skipButton = document.querySelector('#skip-intro');
const guide = document.querySelector('#guide-dialog');
const openGuideButton = document.querySelector('#open-guide');
const closeGuideButton = document.querySelector('#close-guide');
const introKey = 'caw.intro.seen';
const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');

let pendingTimers = [];
let introRun = 0;

function storedLanguage() {
  try {
    return localStorage.getItem('caw.lang');
  } catch {
    return null;
  }
}

function setLanguage(language, remember = false) {
  if (remember) {
    try {
      localStorage.setItem('caw.lang', language);
    } catch {
      // The choice then lasts for this page only.
    }
  }
  document.documentElement.lang = language;
  const copy = translations[language];
  for (const element of document.querySelectorAll('[data-text]')) {
    element.textContent = copy[element.dataset.text];
  }
  document.querySelector('#code').setAttribute('aria-label', copy.codeLabel);
  document.querySelector('nav').setAttribute('aria-label', copy.languageLabel);
  replayButton.setAttribute('aria-label', copy.replayIntro);
  boot.setAttribute('aria-label', copy.bootIntro);
  for (const button of document.querySelectorAll('[data-lang]')) {
    button.setAttribute('aria-pressed', String(button.dataset.lang === language));
  }
}

function stopIntroTimers() {
  introRun += 1;
  for (const timer of pendingTimers) {
    clearTimeout(timer);
  }
  pendingTimers = [];
}

function later(delay, action) {
  const run = introRun;
  const timer = setTimeout(() => {
    if (run === introRun) action();
  }, delay);
  pendingTimers.push(timer);
}

function markIntroSeen() {
  try {
    sessionStorage.setItem(introKey, '1');
  } catch {
    // Private-browsing restrictions must not prevent the landing from working.
  }
}

function hasSeenIntro() {
  try {
    return sessionStorage.getItem(introKey) === '1';
  } catch {
    return true;
  }
}

function finishIntro(focusReplay = false) {
  stopIntroTimers();
  boot.hidden = true;
  boot.className = 'boot-screen';
  landing.inert = false;
  markIntroSeen();
  if (focusReplay) replayButton.focus();
}

function startIntro() {
  stopIntroTimers();
  if (reducedMotion.matches) {
    finishIntro();
    return;
  }
  boot.className = 'boot-screen';
  boot.hidden = false;
  landing.inert = true;
  skipButton.focus();

  // Keep the original, calm transition speeds. We shortened the intro by
  // removing the shuffle, full-screen card zoom and idle time, not by rushing.
  later(60, () => boot.classList.add('is-shifted'));
  later(670, () => boot.classList.add('has-slash'));
  later(930, () => boot.classList.add('has-wordmark'));
  later(1400, () => boot.classList.add('is-completing'));
  later(1700, () => finishIntro());
}

function showStatus(key) {
  document.querySelector('#status').textContent = translations[document.documentElement.lang][key];
}

// The host token is the only proof of being host; it stays in this browser.
async function createSession(button) {
  button.disabled = true;
  showStatus('creating');
  try {
    const response = await fetch('/api/sessions', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ language: document.documentElement.lang }),
    });
    if (!response.ok) throw new Error(`status ${response.status}`);
    const session = await response.json();
    localStorage.setItem(`caw.host.${session.code}`, session.hostToken);
    location.assign(`/s/${session.code}`);
  } catch {
    showStatus('createFailed');
    button.disabled = false;
  }
}

async function joinSession(input) {
  if (!input.checkValidity()) return showStatus('badCode');
  try {
    const response = await fetch(`/api/sessions/${input.value}`, { cache: 'no-store' });
    if (response.ok) return location.assign(`/s/${input.value}`);
    showStatus(response.status === 404 ? 'notFound' : 'serverDown');
  } catch {
    showStatus('serverDown');
  }
}

for (const button of document.querySelectorAll('[data-lang]')) {
  button.addEventListener('click', () => setLanguage(button.dataset.lang, true));
}
document.querySelector('#new-session').addEventListener('click', (event) => createSession(event.currentTarget));
document.querySelector('#join-form').addEventListener('submit', (event) => {
  event.preventDefault();
  joinSession(document.querySelector('#code'));
});
document.querySelector('#code').addEventListener('input', (event) => {
  event.target.value = event.target.value.toUpperCase();
});
skipButton.addEventListener('click', () => finishIntro(true));
openGuideButton.addEventListener('click', () => guide.showModal());
closeGuideButton.addEventListener('click', () => guide.close());
replayButton.addEventListener('click', startIntro);
document.addEventListener('keydown', (event) => {
  if (event.key === 'Escape' && !boot.hidden) finishIntro(true);
});

setLanguage(storedLanguage() ?? (navigator.language.startsWith('de') ? 'de' : 'en'));
if (!hasSeenIntro() && !reducedMotion.matches) startIntro();
