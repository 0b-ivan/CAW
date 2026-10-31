// Landing-page language switching and the one-shot Cut Deck introduction.
import { translations } from './translations.js';

const landing = document.querySelector('#landing');
const boot = document.querySelector('#boot-screen');
const replayButton = document.querySelector('#replay-intro');
const skipButton = document.querySelector('#skip-intro');
const introKey = 'caw.intro.seen';
const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');

let pendingTimers = [];
let introRun = 0;

function setLanguage(language) {
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
  boot.querySelectorAll('.zoom-card').forEach((card) => card.remove());
  landing.inert = false;
  markIntroSeen();
  if (focusReplay) replayButton.focus();
}

function zoomMiddleCard() {
  const middleCard = boot.querySelector('.cut-card--a');
  const from = middleCard.getBoundingClientRect();
  const bounds = boot.getBoundingClientRect();
  const zoomCard = document.createElement('span');
  zoomCard.className = 'zoom-card';
  zoomCard.textContent = 'A';
  zoomCard.setAttribute('aria-hidden', 'true');
  zoomCard.style.left = `${from.left - bounds.left}px`;
  zoomCard.style.top = `${from.top - bounds.top}px`;
  zoomCard.style.width = `${from.width}px`;
  zoomCard.style.height = `${from.height}px`;
  zoomCard.style.fontSize = `${from.width * 0.67}px`;
  boot.appendChild(zoomCard);
  void zoomCard.offsetWidth;

  // A separate flat rectangle preserves the exact A-card silhouette during zoom.
  requestAnimationFrame(() => {
    zoomCard.style.left = '0px';
    zoomCard.style.top = '0px';
    zoomCard.style.width = `${boot.clientWidth}px`;
    zoomCard.style.height = `${boot.clientHeight}px`;
    zoomCard.style.fontSize = `${Math.min(boot.clientWidth, boot.clientHeight) * 0.75}px`;
  });
  later(820, () => boot.classList.add('is-completing'));
  later(1120, () => finishIntro());
}

function startIntro() {
  stopIntroTimers();
  if (reducedMotion.matches) {
    finishIntro();
    return;
  }
  boot.querySelectorAll('.zoom-card').forEach((card) => card.remove());
  boot.className = 'boot-screen';
  boot.hidden = false;
  landing.inert = true;
  skipButton.focus();

  // Each step is independent of the server: no game state or timers are blocked.
  later(80, () => boot.classList.add('is-shuffling'));
  later(1030, () => boot.classList.add('is-shifted'));
  later(1640, () => boot.classList.add('has-slash'));
  later(2080, () => boot.classList.add('has-wordmark'));
  later(3130, zoomMiddleCard);
}

for (const button of document.querySelectorAll('[data-lang]')) {
  button.addEventListener('click', () => setLanguage(button.dataset.lang));
}
document.querySelector('#code').addEventListener('input', (event) => {
  event.target.value = event.target.value.toUpperCase();
});
skipButton.addEventListener('click', () => finishIntro(true));
replayButton.addEventListener('click', startIntro);
document.addEventListener('keydown', (event) => {
  if (event.key === 'Escape' && !boot.hidden) finishIntro(true);
});

setLanguage(navigator.language.startsWith('de') ? 'de' : 'en');
if (!hasSeenIntro() && !reducedMotion.matches) startIntro();
