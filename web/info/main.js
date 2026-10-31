// The info pages: licenses (with the packs the server has loaded), privacy and imprint.
// Which one is shown comes from <body data-page>.
import { texts } from './text.js';

const page = document.body.dataset.page;
const content = document.getElementById('content');

function storedLanguage() {
  try {
    return localStorage.getItem('caw.lang');
  } catch {
    return null;
  }
}

const language = storedLanguage() ?? (navigator.language.startsWith('de') ? 'de' : 'en');
const copy = texts[language];
const fill = (template, ...args) => template.replace(/\{(\d+)\}/g, (_, i) => String(args[Number(i)] ?? ''));

function element(tag, text, className) {
  const node = document.createElement(tag);
  node.textContent = text;
  if (className) node.className = className;
  return node;
}

async function licenses() {
  const parts = [element('p', copy.packsHeading, 'eyebrow')];
  try {
    const { packs } = await (await fetch('/api/packs', { cache: 'no-store' })).json();
    const list = document.createElement('ul');
    list.className = 'info-list';
    for (const pack of packs) {
      const item = element('li', fill(copy.packLine, pack.title, pack.license, pack.rightsHolder) + ' ');
      const source = element('a', pack.sourceUrl.replace(/^https?:\/\//, '').split('/')[0]);
      source.href = pack.sourceUrl;
      source.rel = 'noopener';
      item.append(source);
      list.append(item);
    }
    parts.push(list, element('p', copy.packsNote));
  } catch {
    parts.push(element('p', copy.packsMissing));
  }
  parts.push(element('p', copy.codeHeading, 'eyebrow'), ...copy.codeLines.map((line) => element('p', line)));
  return parts;
}

function privacy() {
  return copy.privacyParagraphs.map((text) => element('p', text));
}

// The operator's details come from web/operator.json, which every installation fills in.
async function imprint() {
  try {
    const operator = await (await fetch('/operator.json', { cache: 'no-store' })).json();
    if (!operator.name || !operator.address || !operator.email) throw new Error('incomplete');
    const values = [operator.name, operator.address, operator.email];
    return copy.imprintLines.map((line, i) => element('p', fill(line, values[i])));
  } catch {
    return [element('p', copy.imprintMissing)];
  }
}

async function start() {
  document.documentElement.lang = language;
  const title = { licenses: 'licensesTitle', privacy: 'privacyTitle', imprint: 'imprintTitle' }[page];
  document.getElementById('title').textContent = copy[title];
  document.title = `C/A/W · ${copy[title]}`;
  document.getElementById('back').textContent = copy.back;
  const build = { licenses, privacy, imprint }[page];
  content.replaceChildren(...(await build()));
}

start();
