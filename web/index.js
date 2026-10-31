// M0 landing page: language switching only; session transport arrives in M2.
import { translations } from './translations.js';

function setLanguage(language) {
  document.documentElement.lang = language;
  const copy = translations[language];
  for (const element of document.querySelectorAll('[data-text]')) {
    element.textContent = copy[element.dataset.text];
  }
  document.querySelector('#code').setAttribute('aria-label', copy.codeLabel);
  document.querySelector('nav').setAttribute('aria-label', copy.languageLabel);
  for (const button of document.querySelectorAll('[data-lang]')) {
    button.setAttribute('aria-pressed', String(button.dataset.lang === language));
  }
}
for (const button of document.querySelectorAll('[data-lang]')) {
  button.addEventListener('click', () => setLanguage(button.dataset.lang));
}
document.querySelector('#code').addEventListener('input', (event) => {
  event.target.value = event.target.value.toUpperCase();
});
setLanguage(navigator.language.startsWith('de') ? 'de' : 'en');
