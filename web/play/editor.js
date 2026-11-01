// Room-local card editor: keeps unsaved inputs on the host until a whole pack is saved.
import { t } from './text.js';

function element(tag, className, content) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (content !== undefined) node.textContent = content;
  return node;
}

function button(label, callback) {
  const node = element('button', '', label);
  node.type = 'button';
  node.addEventListener('click', callback);
  return node;
}

function cardRow(app, actions, type, index) {
  const entry = element('div', 'editor-card');
  const label = element('label', 'eyebrow', t(type === 'prompts' ? 'customPrompt' : 'customAnswer', index + 1));
  const id = 'card-' + type + '-' + index;
  label.htmlFor = id;
  const remove = button(t('customRemove'), () => {
    app.editor[type].splice(index, 1);
    app.editor.dirty = true;
    actions.refresh();
  });
  const header = element('div', 'editor-card-head');
  header.append(label, remove);
  const input = element('textarea', 'editor-input');
  input.id = id;
  input.rows = 2;
  input.maxLength = 300;
  input.value = app.editor[type][index];
  input.disabled = app.frame.phase !== 'lobby';
  input.addEventListener('input', () => {
    app.editor[type][index] = input.value;
    app.editor.dirty = true;
  });
  remove.disabled = input.disabled;
  entry.append(header, input);
  return entry;
}

function listSection(app, actions, type) {
  const container = element('section', 'editor-list');
  container.append(element('p', 'eyebrow menu-heading', t(type === 'prompts' ? 'customPrompts' : 'customAnswers')));
  if (type === 'prompts') container.append(element('p', 'menu-note', t('customBlanks')));
  app.editor[type].forEach((value, index) => {
    container.append(cardRow(app, actions, type, index));
  });
  const add = button(t(type === 'prompts' ? 'customAddPrompt' : 'customAddAnswer'), () => {
    if (app.editor[type].length >= 100) return;
    app.editor[type].push('');
    app.editor.dirty = true;
    actions.refresh();
    document.getElementById('card-' + type + '-' + (app.editor[type].length - 1))?.focus();
  });
  add.disabled = app.editor[type].length >= 100 || app.frame.phase !== 'lobby';
  container.append(add);
  return container;
}

export function editorPage(app, actions, back) {
  const rows = [back()];
  if (!app.editor) {
    if (!app.editorLoading) actions.loadCards();
    rows.push(element('p', 'menu-note', t('customLoading')));
    return rows;
  }
  rows.push(element('p', 'menu-note', t(app.frame.phase === 'lobby' ? 'customHint' : 'customLobbyOnly')));
  const status = element('p', 'menu-note', app.editor.status || '');
  status.setAttribute('role', 'status');
  rows.push(status);
  const save = button(t('customSave'), () => actions.saveCards());
  save.className = 'editor-save';
  save.disabled = app.editorSaving || app.frame.phase !== 'lobby';
  rows.push(save, listSection(app, actions, 'prompts'), listSection(app, actions, 'answers'));

  const tools = element('div', 'editor-tools');
  const download = button(t('customExport'), () => actions.exportCards());
  const upload = element('label', 'editor-upload', t('customImport'));
  const file = document.createElement('input');
  file.type = 'file';
  file.accept = '.json,application/json';
  file.disabled = app.frame.phase !== 'lobby';
  file.setAttribute('aria-label', t('customImport'));
  file.addEventListener('change', () => {
    if (file.files?.[0]) actions.importCards(file.files[0]);
  });
  upload.append(file);
  tools.append(download, upload);
  rows.push(tools);
  return rows;
}
