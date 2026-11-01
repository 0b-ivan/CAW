// Renders game sentences and embossed, per-line answer labels from trusted text.
// Visual treatment never changes card wording or the X4-compatible sentence rules.

const BLANK = /(_{3,})/;
const rendered = new Map();
let resizeScheduled = false;

// [{ text, answer }] in reading order. Answers without a blank go to the end.
export function sentenceParts(prompt, answers) {
  const pieces = String(prompt ?? '').split(BLANK);
  const parts = [];
  let used = 0;
  pieces.forEach((piece, i) => {
    const isBlank = i % 2 === 1;
    if (!isBlank) parts.push({ text: piece, answer: false });
    else if (used < answers.length) parts.push({ text: stripEndDot(answers[used++], pieces[i + 1]), answer: true });
    else parts.push({ text: piece, answer: false });
  });
  while (used < answers.length) {
    parts.push({ text: ' ', answer: false });
    parts.push({ text: String(answers[used++]), answer: true });
  }
  return parts;
}

// "Testantwort 3." before "." would read "Testantwort 3..": drop the card's own full stop.
function stripEndDot(answer, following) {
  const text = String(answer ?? '');
  return following && /^[.!?]/.test(following) ? text.replace(/[.!?]$/, '') : text;
}

export function sentenceText(prompt, answers) {
  return sentenceParts(prompt, answers).map((part) => part.text).join('');
}

// Keep the original inputs so labels can be reflowed after screen resizing.
export function fillSentence(element, prompt, answers) {
  rendered.set(element, { prompt, answers: [...answers] });
  drawSentence(element, prompt, answers);
}

function drawSentence(element, prompt, answers) {
  element.replaceChildren();
  let answerIndex = 0;
  for (const part of sentenceParts(prompt, answers)) {
    if (!part.answer) {
      element.append(document.createTextNode(part.text));
      continue;
    }
    const tape = createTape(part.text, answerIndex++);
    tape.classList.add('answer-tape--measuring');
    element.append(tape);
    splitTape(tape);
  }
}

// Different deterministic details suggest manually applied, slightly crooked labels.
function createTape(text, index) {
  const tape = document.createElement('mark');
  tape.className = 'answer-tape answer-tape--' + (index % 4);
  const letters = document.createElement('span');
  letters.className = 'answer-tape__letters';
  letters.textContent = text;
  tape.append(letters);
  return tape;
}

// Measure actual browser text rows, not guessed character counts. Split at word
// boundaries; a single unbreakable word keeps the browser's safe CSS wrapping.
function splitTape(tape) {
  const textNode = tape.firstElementChild.firstChild;
  if (!textNode || !tape.getClientRects().length) return;

  const text = textNode.textContent;
  const words = [...text.matchAll(/\S+/gu)];
  if (words.length < 2) {
    tape.classList.remove('answer-tape--measuring');
    return;
  }

  const range = document.createRange();
  const starts = [0];
  // Rotated physical cards shift a few pixels across a *single* text row.
  // Use a fraction of line height rather than comparing raw top coordinates.
  const lineHeight = parseFloat(getComputedStyle(tape).lineHeight);
  const rowThreshold = Math.max(9, lineHeight * .55);
  let previousTop = null;
  for (const word of words) {
    range.setStart(textNode, word.index);
    range.setEnd(textNode, word.index + word[0].length);
    const rects = range.getClientRects();
    if (rects.length !== 1) return; // Mid-word wraps use CSS cloning instead.
    const top = rects[0].top;
    if (previousTop !== null && top > previousTop + rowThreshold) starts.push(word.index);
    previousTop = top;
  }

  if (starts.length === 1) {
    tape.classList.remove('answer-tape--measuring');
    return;
  }

  const pieces = document.createDocumentFragment();
  const variant = Number(tape.className.match(/answer-tape--(\d)/)?.[1] ?? 0);
  starts.forEach((start, i) => {
    const end = starts[i + 1] ?? text.length;
    const line = text.slice(start, end).trim();
    if (!line) return;
    if (i > 0) {
      pieces.append(document.createTextNode(' '), document.createElement('br'));
    }
    const strip = createTape(line, variant + i);
    strip.classList.add('answer-tape--strip');
    pieces.append(strip);
  });
  tape.replaceWith(pieces);
}

// Screen cards are assembled off-DOM; finalize strips after they are attached.
export function layoutTapeLines() {
  for (const [element, state] of rendered) {
    if (!element.isConnected) {
      rendered.delete(element);
      continue;
    }
    if (element.getClientRects().length) drawSentence(element, state.prompt, state.answers);
  }
}

// Rebuild strips only when viewport dimensions change. This avoids frozen line
// splits on mobile rotation without running observers during every game tick.
window.addEventListener('resize', () => {
  if (resizeScheduled) return;
  resizeScheduled = true;
  requestAnimationFrame(() => {
    resizeScheduled = false;
    layoutTapeLines();
  });
});
