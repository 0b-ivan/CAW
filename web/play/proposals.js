// A seat can submit a card; only the host may see or review pending suggestions.
import { t } from './text.js';

function node(tag, className, text) {
  const item = document.createElement(tag);
  if (className) item.className = className;
  if (text !== undefined) item.textContent = text;
  return item;
}

function button(key, onClick) {
  const item = node('button', '', t(key));
  item.type = 'button';
  item.addEventListener('click', onClick);
  return item;
}

function note(value) {
  const item = node('p', 'menu-note', value);
  item.setAttribute('role', 'status');
  return item;
}

export function suggestPage(app, actions, back) {
  const rows = [back(), note(t('proposalHint'))];
  const label = node('label', 'eyebrow', t('proposalType'));
  label.htmlFor = 'proposal-type';
  const type = node('select', 'proposal-select');
  type.id = 'proposal-type';
  for (const choice of ['prompt', 'answer']) {
    const option = node('option', '', t(choice === 'prompt' ? 'proposalPrompt' : 'proposalAnswer'));
    option.value = choice;
    type.append(option);
  }
  type.value = app.proposalDraft.type;
  type.addEventListener('change', () => (app.proposalDraft.type = type.value));

  const textLabel = node('label', 'eyebrow', t('proposalText'));
  textLabel.htmlFor = 'proposal-text';
  const input = node('textarea', 'editor-input');
  input.id = 'proposal-text';
  input.rows = 3;
  input.maxLength = 300;
  input.value = app.proposalDraft.text;
  input.addEventListener('input', () => (app.proposalDraft.text = input.value));
  const send = button('proposalSend', actions.submitProposal);
  send.className = 'editor-save';
  send.disabled = app.proposalSending || app.frame.phase !== 'lobby';
  rows.push(label, type, textLabel, input, send, note(app.proposalDraft.status));
  return rows;
}

export function reviewPage(app, actions, back) {
  const rows = [back(), note(t('proposalReviewHint'))];
  const reload = button('proposalRefresh', actions.loadProposals);
  reload.className = 'editor-save';
  reload.disabled = app.proposalLoading || app.proposalReviewing;
  rows.push(reload, note(app.proposalStatus));
  if (!app.proposals) {
    rows.push(note(t('proposalLoading')));
    return rows;
  }
  if (!app.proposals?.length) {
    rows.push(note(t('proposalEmpty')));
    return rows;
  }
  for (const suggestion of app.proposals) {
    const item = node('section', 'editor-card');
    item.append(node('p', 'eyebrow', t('proposalBy', suggestion.author)),
      node('p', 'eyebrow', t(suggestion.type === 'prompt' ? 'proposalPrompt' : 'proposalAnswer')),
      node('p', 'proposal-content', suggestion.text));
    const controls = node('div', 'proposal-actions');
    const accept = button('proposalAccept', () => actions.reviewProposal(suggestion.id, true));
    const reject = button('proposalReject', () => actions.reviewProposal(suggestion.id, false));
    accept.disabled = app.proposalReviewing || app.frame.phase !== 'lobby';
    reject.disabled = accept.disabled;
    controls.append(accept, reject);
    item.append(controls);
    rows.push(item);
  }
  return rows;
}
