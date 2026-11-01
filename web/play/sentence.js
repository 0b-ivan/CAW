// Puts answer cards into the blanks of a prompt, like CardPartySentence.h on the X4.
// The answers are marked so the page can show them inverted.

const BLANK = /(_{3,})/;

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

// Replaces the element's content with the sentence; answers become <mark> elements.
export function fillSentence(element, prompt, answers) {
  element.replaceChildren();
  for (const part of sentenceParts(prompt, answers)) {
    if (!part.answer) {
      element.append(document.createTextNode(part.text));
      continue;
    }
    const mark = document.createElement('mark');
    mark.className = 'answer-tape';
    mark.textContent = part.text;
    element.append(mark);
  }
}
