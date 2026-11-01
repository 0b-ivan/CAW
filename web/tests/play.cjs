// A host and three phones play a whole match in real browsers against the running stack.
// Every view is checked at 320x568, 390x844 and 1280x800: no page scroll in either
// direction, the main action on screen, all controls at least 48 px tall.
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');

const target = process.env.CAW_SMOKE_URL || 'http://localhost:8080';
const SIZES = [[320, 568], [390, 844], [1280, 800]];
const SCREEN_SIZES = [[1920, 1080], [1024, 768]];
const pages = [];

async function openPhone(browser, locale = 'de-DE') {
  const context = await browser.newContext({ viewport: { width: 390, height: 844 }, locale });
  await context.addInitScript(() => sessionStorage.setItem('caw.intro.seen', '1'));
  const page = await context.newPage();
  page.errors = [];
  page.on('pageerror', (error) => page.errors.push(error.message));
  pages.push(page);
  return page;
}

async function checkFit(page, label, mainAction) {
  for (const [width, height] of SIZES) {
    await page.setViewportSize({ width, height });
    const size = await page.evaluate(() => ({
      width: document.documentElement.scrollWidth,
      height: document.documentElement.scrollHeight,
    }));
    assert.ok(size.width <= width, `${label} ${width}x${height}: horizontal scroll`);
    assert.ok(size.height <= height, `${label} ${width}x${height}: the page scrolls vertically`);
    if (mainAction) {
      const box = await page.locator(mainAction).boundingBox();
      assert.ok(box && box.y >= 0 && box.y + box.height <= height + 0.5,
        `${label} ${width}x${height}: ${mainAction} is off screen`);
    }
    for (const control of await page.locator('button:visible, input:visible, a.button:visible').all()) {
      const box = await control.boundingBox();
      if (box) assert.ok(box.height >= 47.5, `${label} ${width}x${height}: a control is ${box.height}px tall`);
    }
    // Desktop uses side-by-side panels instead of stretching the phone mockup.
    if (width === 1280 && label === 'answer') {
      const prompt = await page.locator('#view-answer .prompt-stack').boundingBox();
      const hand = await page.locator('#hand').boundingBox();
      assert.ok(prompt && hand && prompt.x + prompt.width < hand.x,
        'desktop question must be left of the answer cards');
      // Bounding boxes include the deliberate two-degree card rotation.
      assert.ok(prompt.width >= 310 && prompt.width <= 450,
        'desktop question retains compact physical-card proportions');
      const font = await page.locator('#hand .card').first()
        .evaluate((card) => parseFloat(getComputedStyle(card).fontSize));
      assert.ok(font >= 16, 'desktop answer cards have readable text');
    }
    if (width === 1280 && label === 'judge') {
      const prompt = await page.locator('#view-judge .prompt-stack').boundingBox();
      const choices = await page.locator('#judge-list').boundingBox();
      assert.ok(prompt && choices && prompt.x + prompt.width < choices.x,
        'desktop juror question must be left of the anonymous choices');
      const optionHeight = await page.locator('#judge-list .choice').first()
        .evaluate((card) => card.getBoundingClientRect().height);
      assert.ok(optionHeight >= 84, 'desktop juror choices have generous tap targets');
    }
    if (width === 1280 && label === 'lobby-host-full') {
      const invite = await page.locator('#lobby-share').boundingBox();
      const players = await page.locator('#lobby-players').boundingBox();
      assert.ok(invite && players && invite.x + invite.width < players.x,
        'desktop lobby must show QR invite beside the players, not above them');
    }
    if (process.env.CAW_SCREENSHOT_DIR) {
      fs.mkdirSync(process.env.CAW_SCREENSHOT_DIR, { recursive: true });
      await page.screenshot({ path: `${process.env.CAW_SCREENSHOT_DIR}/${label}-${width}.png` });
    }
  }
  await page.setViewportSize({ width: 390, height: 844 });
}

// The big screen at 16:9 and 4:3: nothing scrolls and no fitted text is cut.
async function checkScreen(page, view, label) {
  await page.locator(`#screen-${view}`).waitFor({ state: 'visible', timeout: 10000 });
  for (const [width, height] of SCREEN_SIZES) {
    await page.setViewportSize({ width, height });
    await page.waitForTimeout(50);
    const size = await page.evaluate(() => ({
      width: document.documentElement.scrollWidth,
      height: document.documentElement.scrollHeight,
      cut: [...document.querySelectorAll('.screen-view:not([hidden]) .fit')]
        .filter((box) => box.scrollHeight > box.clientHeight + 1).length,
    }));
    assert.ok(size.width <= width && size.height <= height, `screen ${label} ${width}x${height}: scrolls`);
    assert.equal(size.cut, 0, `screen ${label} ${width}x${height}: text does not fit`);
    if (process.env.CAW_SCREENSHOT_DIR) {
      await page.screenshot({ path: `${process.env.CAW_SCREENSHOT_DIR}/screen-${label}-${width}.png` });
    }
  }
}

// The raised tape is a real text element: readable, high-contrast and asset-free.
async function checkTape(page, selector) {
  const tape = page.locator(selector).first();
  await tape.waitFor({ state: 'visible' });
  const material = await tape.evaluate((element) => {
    const style = getComputedStyle(element);
    const letters = element.querySelector('.answer-tape__letters');
    const raised = letters && getComputedStyle(letters);
    return {
      background: style.backgroundColor,
      sheen: style.backgroundImage,
      font: style.fontFamily,
      edge: style.boxShadow,
      band: style.getPropertyValue('-webkit-box-decoration-break') ||
        style.getPropertyValue('box-decoration-break'),
      reflection: getComputedStyle(element, '::before').backgroundImage,
      crease: getComputedStyle(element, '::after').backgroundImage,
      letterColor: raised?.color,
      relief: raised?.textShadow,
      raisedEdge: raised?.getPropertyValue('-webkit-text-stroke-width'),
    };
  });
  assert.equal(material.background, 'rgb(8, 8, 8)', `${selector}: black plastic band`);
  assert.match(material.sheen, /linear-gradient/, `${selector}: molded plastic surface`);
  assert.match(material.reflection, /linear-gradient/, `${selector}: scuffed reflections`);
  assert.match(material.crease, /linear-gradient/, `${selector}: plastic crease`);
  assert.match(material.font, /Arial Rounded MT Bold/, `${selector}: embossing font, not UI monospace`);
  assert.equal(material.letterColor, 'rgb(185, 185, 180)', `${selector}: embossed silver lettering`);
  assert.ok(material.relief.split('rgb(').length >= 4, `${selector}: three-dimensional letters`);
  assert.equal(material.raisedEdge, '0.3px', `${selector}: pressed letter edges`);
  assert.notEqual(material.edge, 'none', `${selector}: cut tape edges cast shadows`);
  assert.equal(material.band, 'clone', `${selector}: clean fallback wrapping`);
}

const visible = (page, view) => page.locator(`#view-${view}`).isVisible();

// Waits for a view; on timeout says which view is shown instead and what the notice line says.
async function waitView(page, view) {
  try {
    await page.locator(`#view-${view}`).waitFor({ state: 'visible', timeout: 10000 });
  } catch {
    const shown = await page.locator('.view:visible').getAttribute('id');
    const notice = await page.locator('#notice').textContent();
    throw new Error(`expected #view-${view}, found #${shown}, notice "${notice}" on ${page.url()}`);
  }
}

async function joinAs(browser, code, name) {
  const page = await openPhone(browser);
  await page.goto(`${target}/s/${code}`);
  await page.getByLabel('DEIN NAME').fill(name);
  await page.getByRole('button', { name: '[ BEITRETEN ]' }).click();
  await waitView(page, 'lobby');
  return page;
}

async function openMenuPage(host, row) {
  await host.locator('#menu-button').click();
  await host.locator('.menu-row', { hasText: row }).first().click();
}

// The phones that still have to answer, the Juror, and everyone else.
async function roles(players) {
  const answering = [];
  let juror = null;
  for (const page of players) {
    if (await visible(page, 'answer')) answering.push(page);
    else if (await page.locator('#waiting-hint').textContent() === 'DU BIST JUROR') juror = page;
  }
  return { answering, juror };
}

async function submitFirstCards(page) {
  const pick = Number((await page.locator('#answer-count').textContent()).split('/')[1].split(' ')[0]);
  const cards = page.locator('#hand .card');
  for (let i = 0; i < pick; i++) await cards.nth(i).click();
  await page.getByRole('button', { name: '[ ABGEBEN ]' }).click();
  await page.locator('#view-waiting:visible, #view-judge:visible').first().waitFor();
}

async function playRound(host, players, round, screen) {
  for (const page of players) await page.locator('#view-answer:visible, #view-waiting:visible').first().waitFor();
  await host.locator('#meta-left', { hasText: `RUNDE ${round}/3` }).waitFor();
  const { answering, juror } = await roles(players);
  assert.equal(answering.length, 2, 'two of three players answer, one judges');
  assert.ok(juror, 'one phone is the Juror');

  if (round === 1) {
    await checkScreen(screen, 'answers', 'answers');
    const page = answering[0];
    await checkFit(page, 'answer', '#submit-button');
    const cards = page.locator('#hand .card');
    const paper = await cards.first().evaluate((card) => {
      const style = getComputedStyle(card);
      return { background: style.backgroundColor, radius: style.borderRadius };
    });
    assert.equal(paper.background, 'rgb(244, 244, 240)', 'answers look like paper cards');
    assert.equal(paper.radius, '6px', 'answers have physical card corners');
    assert.equal(await page.locator('#hand .card mark').count(), 0,
      'white hand cards must use plain text without embossed strips');
    const handText = await cards.first().evaluate((card) => getComputedStyle(card).color);
    assert.equal(handText, 'rgb(8, 8, 8)', 'unselected hand text stays dark on paper');
    await cards.first().click();
    await checkTape(page, '#answer-prompt mark.answer-tape');
    await page.locator('#hand .card .slot-mark', { hasText: '<1/' }).waitFor();
    const selected = await cards.first().evaluate((card) => ({
      background: getComputedStyle(card).backgroundColor,
      color: getComputedStyle(card).color,
      pressed: card.getAttribute('aria-pressed'),
    }));
    assert.equal(selected.background, 'rgb(8, 8, 8)', 'selected answer is dark');
    assert.equal(selected.color, 'rgb(244, 244, 240)', 'selected hand text is light and readable');
    assert.equal(selected.pressed, 'true', 'selection is accessible as text/ARIA');
    assert.equal(await page.locator('#answer-prompt mark').count(), 1, 'the chosen card fills the blank');
    await page.locator('#hand .card').first().click();
    assert.equal(await page.locator('#hand .slot-mark').count(), 0);

    // All seven cards stay in one scroll area. Use a short viewport to
    // exercise scrolling regardless of the synthetic card text.
    const promptWidth = await page.locator('#view-answer .prompt-stack').boundingBox();
    const shellWidth = await page.locator('#shell').boundingBox();
    assert.ok(promptWidth.width < shellWidth.width * .9, 'prompt is a compact card stack');
    const handSize = await cards.count();
    assert.ok(handSize >= 7 && handSize <= 9,
      'seven regular cards plus at most two optional Draw-2 cards');
    assert.equal(await page.locator('#hand-pager').count(), 0, 'no paging buttons');
    await page.setViewportSize({ width: 320, height: 568 });
    const hand = page.locator('#hand');
    const metrics = await hand.evaluate((el) => ({
      height: el.clientHeight, scrollHeight: el.scrollHeight,
    }));
    assert.ok(metrics.scrollHeight > metrics.height + 1, 'mobile hand is independently scrollable');
    await cards.last().click();
    const lowerScroll = await hand.evaluate((el) => el.scrollTop);
    assert.ok(lowerScroll > 0, 'the last card can be reached by scrolling');
    assert.equal(await cards.last().getAttribute('aria-pressed'), 'true',
      'the last original server slot can be selected');
    assert.match(await page.locator('#answer-count').textContent(), /^1\//);
    await hand.evaluate((el) => { el.scrollTop = 0; });
    assert.equal(await cards.last().getAttribute('aria-pressed'), 'true',
      'selected cards remain selected when the hand scrolls');
    const submitBox = await page.locator('#submit-button').boundingBox();
    assert.ok(submitBox.y + submitBox.height <= 568, 'submit is fixed below the hand');
    await cards.last().click();
    assert.equal(await cards.last().getAttribute('aria-pressed'), 'false');
    await page.setViewportSize({ width: 390, height: 844 });

    // A reload keeps the seat: the ticket is in localStorage.
    await answering[1].reload();
    await waitView(answering[1], 'answer');
  }

  if (round === 2) {
    // A player pauses; only that player or the host can resume.
    await answering[0].getByRole('button', { name: '[ PAUSE ]' }).click();
    for (const page of players) await waitView(page, 'hold');
    await checkFit(answering[1], 'hold', null);
    assert.equal(await answering[1].locator('#hold-resume').isVisible(), false);
    await answering[0].locator('#hold-resume').click();
    await waitView(answering[0], 'answer');
  }

  await submitFirstCards(answering[0]);
  if (round === 1) {
    await waitView(answering[0], 'waiting');
    await checkFit(answering[0], 'waiting', '#waiting-title');
  }
  await submitFirstCards(answering[1]);

  await waitView(juror, 'judge');
  await answering[0].locator('#judge-eyebrow', { hasText: 'ENTSCHEIDET' }).waitFor();
  assert.equal(await answering[0].locator('#judge-list button').count(), 0, 'only the Juror can choose');
  if (round === 1) {
    await checkFit(juror, 'judge', '#judge-button');
    await checkScreen(screen, 'judging', 'judging');
    assert.ok((await screen.locator('#screen-judge-prompt').textContent()).length > 0,
      'the TV shows the question above answer cards');
    const screenPaper = await screen.locator('#screen-plays li').first()
      .evaluate((card) => getComputedStyle(card).backgroundColor);
    assert.equal(screenPaper, 'rgb(244, 244, 240)', 'TV shows legible paper answer cards');
    await checkTape(screen, '#screen-plays li mark.answer-tape');
  }
  const judgePaper = await juror.locator('#judge-list .choice').first()
    .evaluate((card) => getComputedStyle(card).backgroundColor);
  assert.equal(judgePaper, 'rgb(244, 244, 240)', 'juror options are paper strips');
  assert.equal(await juror.locator('#judge-list mark').count(), 0,
    'juror answer options must stay plain without embossed tape');
  const choiceText = await juror.locator('#judge-list .choice').first()
    .evaluate((element) => getComputedStyle(element).color);
  assert.equal(choiceText, 'rgb(8, 8, 8)', 'unselected juror options use readable dark text');
  await juror.locator('#judge-list .choice').first().click();
  assert.equal(await juror.locator('#judge-list .choice').first().getAttribute('aria-pressed'),
    'true', 'the chosen answer is also announced to assistive technology');
  const selectedChoiceText = await juror.locator('#judge-list .choice').first()
    .evaluate((element) => getComputedStyle(element).color);
  assert.equal(selectedChoiceText, 'rgb(244, 244, 240)',
    'selected juror options use readable inverted plain text');
  assert.equal(await juror.locator('#judge-list mark').count(), 0,
    'selecting a juror option does not add an embossed strip below');
  // The pick fills the blanks of the prompt shown once on top.
  assert.ok(await juror.locator('#judge-prompt mark').count() >= 1, 'the pick appears in the prompt');
  await checkTape(juror, '#judge-prompt mark.answer-tape');
  await juror.getByRole('button', { name: '[ BESTÄTIGEN ]' }).click();

  for (const page of [host, ...players]) await waitView(page, 'reveal');
  assert.match(await host.locator('#reveal-winner').textContent(), /^GEWINNER: /);
  const revealPaper = await host.locator('#reveal-sentence')
    .evaluate((card) => getComputedStyle(card).backgroundColor);
  assert.equal(revealPaper, 'rgb(244, 244, 240)', 'the reveal is a single large paper card');
  await checkTape(host, '#reveal-sentence mark.answer-tape');
  await checkTape(screen, '#screen-sentence mark.answer-tape');
  if (round === 1) {
    await checkScreen(screen, 'reveal', 'reveal');
    await checkFit(host, 'reveal-host', '#next-button');
    await checkFit(juror, 'reveal', null);
  }
  await host.locator('#next-button').click();
}

async function run(browser) {
  // The host starts on the landing page.
  const host = await openPhone(browser);
  await host.goto(target);
  await host.getByRole('button', { name: '[ NEUE SESSION ]' }).click();
  await host.waitForURL(/\/s\/[A-HJKMNP-Z2-9]{6}$/);
  const code = host.url().split('/').pop();
  // A localhost URL is only reachable on the host device. Require an
  // explicitly entered LAN origin before exposing QR, Copy or Share.
  await host.locator('#invite-address').waitFor();
  assert.equal(await host.locator('#lobby-qr svg').count(), 0, 'no unusable localhost QR');
  assert.equal(await host.locator('#copy-link').isDisabled(), true, 'no unusable localhost copy link');
  const inviteOrigin = 'http://192.168.50.77:8080';
  await host.locator('#invite-address').fill('http://127.0.0.1:8080');
  await host.locator('#invite-address').press('Tab');
  await host.locator('#invite-hint', { hasText: 'BITTE VOLLSTÄNDIGE' }).waitFor();
  await host.locator('#invite-address').fill(inviteOrigin);
  await host.locator('#invite-address').press('Tab');
  await host.locator('#lobby-qr svg').waitFor();
  assert.equal(await host.locator('#lobby-qr').getAttribute('data-url'),
    `${inviteOrigin}/s/${code}`, 'QR uses the explicit LAN URL');
  assert.equal(await host.locator('#lobby-url').textContent(),
    `192.168.50.77:8080/s/${code}`);
  assert.equal(await host.locator('#copy-link').isDisabled(), false);
  assert.equal(await host.locator('#invite-address-row').isVisible(), false,
    'the configured address editor collapses to leave space for players');
  await host.locator('#lobby-url').click();
  assert.equal(await host.locator('#invite-address-row').isVisible(), true,
    'the URL is an accessible toggle for editing the address');
  await host.locator('#lobby-url').click();
  assert.equal(await host.locator('#invite-address-row').isVisible(), false);
  assert.equal(await host.evaluate(async (session) =>
    (await import('/play/invite.js')).shareableSessionUrl(session), code),
  `${inviteOrigin}/s/${code}`, 'Copy and Share use the same URL as the QR');
  await host.locator('#host-badge .host-crown').waitFor();
  assert.equal(await host.locator('#host-badge .host-crown')
    .evaluate((crown) => getComputedStyle(crown).color), 'rgb(215, 174, 74)',
  'a non-playing host has a gold crown in the header');
  assert.ok(await host.locator('#view-lobby').evaluate((view) => view.classList.contains('has-host')),
    'the host lobby has invite controls');
  assert.equal(await host.locator('#lobby-code').textContent(), code);
  assert.equal(await host.locator('#start-button').isDisabled(), true, 'START needs three players');
  await checkFit(host, 'lobby-host', '#start-button');

  // The big screen listens on its own socket; every frame it gets is recorded.
  const screen = await openPhone(browser);
  const screenFrames = [];
  screen.on('websocket', (socket) => socket.on('framereceived', (frame) => screenFrames.push(frame.payload)));
  await screen.goto(`${target}/s/${code}/screen`);
  await screen.locator('#screen-code', { hasText: code }).waitFor();
  await screen.locator('#screen-qr svg').waitFor();

  const players = [];
  for (const name of ['BrowserAda', 'Bert', 'Cyra']) players.push(await joinAs(browser, code, name));
  await host.locator('#lobby-players li', { hasText: 'Cyra' }).waitFor();
  await checkFit(host, 'lobby-host-full', '#start-button');
  await host.setViewportSize({ width: 320, height: 568 });
  assert.equal(await host.locator('#invite-address-row').isVisible(), false);
  const invitePanel = await host.locator('#lobby-share').evaluate((panel) => ({
    scrollHeight: panel.scrollHeight, height: panel.clientHeight,
  }));
  assert.ok(invitePanel.scrollHeight <= invitePanel.height + 1,
    'all invite actions are exposed without an internally clipped QR panel');
  await host.locator('#screen-link').scrollIntoViewIfNeeded();
  const bigScreenButton = await host.locator('#screen-link').boundingBox();
  const copyButton = await host.locator('#copy-link').boundingBox();
  assert.ok(bigScreenButton.y >= 0 && bigScreenButton.y + bigScreenButton.height <= 568,
    'Big Screen action is reachable at 320x568');
  assert.ok(Math.abs(bigScreenButton.y - copyButton.y) <= 2,
    'mobile invite actions use one row and leave space for the roster');
  await host.setViewportSize({ width: 390, height: 844 });
  assert.equal(await players[0].locator('#lobby-share').isVisible(), false,
    'the guest lobby does not show host-only share controls');
  await checkFit(players[0], 'lobby', '#lobby-status');
  await screen.locator('#screen-players li', { hasText: 'Cyra·' }).waitFor();
  assert.equal(await players[0].locator('#lobby-players .host-crown').count(), 0,
    'host-only organizers are never mistaken for players');
  assert.equal(await screen.locator('#screen-players .host-crown').count(), 0,
    'the TV does not invent a host seat');
  // Every name carries its word, and the three words differ.
  const words = await host.locator('#lobby-players li').allTextContents();
  assert.equal(new Set(words.map((label) => label.split('·')[1])).size, 3, 'three different words');
  await checkScreen(screen, 'lobby', 'lobby');

  // The host finds the big screen link as a QR in the menu.
  await host.locator('#screen-link').click();
  await host.locator('.menu-qr svg').waitFor();
  await checkFit(host, 'menu-screen', '#menu-close');
  await host.locator('#menu-close').click();

  // Hand is seven by default. Check the host's new lobby control, then
  // restore seven for the scrollable-hand match.
  await openMenuPage(host, 'SPIELREGELN');
  const handRule = host.locator('.stepper', { hasText: 'HANDKARTEN' });
  await handRule.locator('.stepper-value', { hasText: /^7$/ }).waitFor();
  await handRule.getByRole('button', { name: 'Mehr' }).click();
  await handRule.locator('.stepper-value', { hasText: /^8$/ }).waitFor();
  await handRule.getByRole('button', { name: 'Weniger' }).click();
  await handRule.locator('.stepper-value', { hasText: /^7$/ }).waitFor();

  // Three rounds instead of ten: ‹ steps 10 → 8 → 5 → 3.
  const roundsRule = host.locator('.stepper', { hasText: 'RUNDEN' });
  const fewer = roundsRule.getByRole('button', { name: 'Weniger' });
  for (let i = 0; i < 3; i++) await fewer.click();
  await roundsRule.locator('.stepper-value', { hasText: /^3$/ }).waitFor();
  await checkFit(host, 'menu-rules', '#menu-close');
  await host.locator('#menu-close').click();

  // Packs: switching one off and on again changes the deck; the credits name the owners.
  await host.locator('#lobby-credits', { hasText: /KARTEN: .+ · .+/ }).waitFor();
  await openMenuPage(host, 'KARTEN-PACKS');
  const firstPack = host.locator('#menu-body .menu-row[aria-pressed="true"]').first();
  await firstPack.waitFor();
  const chosen = await host.locator('#menu-body .menu-row[aria-pressed="true"]').count();
  if (chosen > 1) {
    const title = (await firstPack.locator('span').first().textContent()).trim();
    const pressed = (count) => host.waitForFunction((n) =>
      document.querySelectorAll('#menu-body .menu-row[aria-pressed="true"]').length === n, count);
    await firstPack.click();
    await pressed(chosen - 1);
    await host.locator('#menu-body .menu-row', { hasText: title }).click();
    await pressed(chosen);
  }
  await checkFit(host, 'menu-packs', '#menu-close');
  await host.locator('#menu-close').click();

  // Host-created cards are playable in this room but invisible to other visitors.
  await openMenuPage(host, 'EIGENE KARTEN');
  await host.locator('.editor-save').waitFor();
  await host.getByRole('button', { name: '[ FRAGE HINZUFÜGEN ]' }).click();
  await host.locator('#card-prompts-0').fill('Eigene Testfrage ___!');
  await host.getByRole('button', { name: '[ ANTWORT HINZUFÜGEN ]' }).click();
  await host.locator('#card-answers-0').fill('Eigene Testantwort');
  await host.locator('.editor-save').click();
  await host.locator('#menu-body', { hasText: 'KARTEN GESPEICHERT UND AKTIV.' }).waitFor();
  assert.equal(await players[0].evaluate(async (room) =>
    (await fetch('/api/sessions/' + room + '/cards')).status, code), 403,
  'guests cannot read private draft cards');
  await checkFit(host, 'menu-custom-cards', '#menu-close');
  await host.locator('#menu-close').click();
  await host.reload();
  await host.locator('#view-lobby:visible').waitFor();
  await openMenuPage(host, 'EIGENE KARTEN');
  await host.locator('#card-prompts-0').waitFor();
  assert.equal(await host.locator('#card-prompts-0').inputValue(), 'Eigene Testfrage ___!',
    'the custom card editor reloads its saved room cards');
  await host.locator('#menu-close').click();

  await host.locator('#start-button').click();
  for (let round = 1; round <= 3; round++) await playRound(host, players, round, screen);

  for (const page of [host, ...players]) await waitView(page, 'finished');
  await checkScreen(screen, 'finished', 'finished');
  assert.equal(await players[0].locator('#finished-lines li').count(), 3, 'the three winning sentences');
  await screen.locator('#screen-line-text', { hasText: ':' }).waitFor();
  for (const payload of screenFrames) {
    const frame = JSON.parse(payload);
    assert.equal(frame.type, 'screen', 'the big screen only gets screen frames');
    assert.ok(!('me' in frame), 'the big screen never sees a private player hand');
    assert.ok(frame.settings.hand >= 3 && frame.settings.hand <= 10,
      'the screen may see the public hand-size rule');
  }
  await checkFit(players[0], 'finished', '#again-button');
  await checkFit(host, 'finished-host', '#rematch-button');
  await players[0].getByRole('button', { name: '[ NOCHMAL ]' }).click();
  await players[1].locator('#finished-votes', { hasText: '1 VON 3' }).waitFor();
  await host.locator('#rematch-button').click();
  for (const page of players) await waitView(page, 'lobby');

  // A locked lobby turns a newcomer away with a reason; opened again, it lets them in.
  await host.locator('#menu-button').click();
  await host.locator('.menu-row', { hasText: 'LOBBY SPERREN' }).click();
  await host.locator('#lobby-status', { hasText: 'LOBBY GESPERRT' }).waitFor();
  const late = await openPhone(browser);
  await late.goto(`${target}/s/${code}`);
  await late.getByLabel('DEIN NAME').fill('Dora');
  await late.getByRole('button', { name: '[ BEITRETEN ]' }).click();
  await late.locator('#join-error', { hasText: 'LOBBY GESPERRT' }).waitFor();
  await host.locator('#menu-button').click();
  await host.locator('.menu-row', { hasText: 'LOBBY ÖFFNEN' }).click();
  await late.getByRole('button', { name: '[ BEITRETEN ]' }).click();
  await waitView(late, 'lobby');

  // The host removes Cyra with two taps.
  await openMenuPage(host, 'SPIELER');
  const cyra = host.locator('.menu-row', { hasText: 'Cyra' });
  await cyra.click();
  await cyra.filter({ hasText: 'NOCHMAL TIPPEN' }).waitFor();
  await cyra.click();
  await waitView(players[2], 'removed');
  await checkFit(players[2], 'removed', '#rejoin-button');
  await host.locator('#menu-close').click();

  // Language is per device.
  await players[1].locator('#menu-button').click();
  await players[1].locator('.menu-row', { hasText: 'SPRACHE' }).click();
  assert.equal(await players[1].locator('html').getAttribute('lang'), 'en');
  await players[1].locator('#menu-close').click();
  await players[1].locator('#lobby-count', { hasText: 'PLAYERS' }).waitFor();
  assert.equal(await host.locator('html').getAttribute('lang'), 'de');

  // The bot smoke runs first and fills the first Fame page. Wait for the actual
  // browser match to persist, then find its distinct player on any Fame page.
  let ownFame;
  if (process.env.CAW_EXPECT_FAME) {
    for (let attempt = 0; attempt < 30; attempt++) {
      const fame = await players[0].evaluate(async () =>
        (await (await fetch('/api/fame', { cache: 'no-store' })).json()).players);
      ownFame = fame.find((player) => player.name === 'BrowserAda' && player.games >= 1);
      if (ownFame) break;
      await players[0].waitForTimeout(500);
    }
    assert.ok(ownFame, 'the browser match was persisted to the Wall of Fame');
    assert.equal(ownFame.rounds, 0, 'private custom-card rounds are not public history');
  }

  await openMenuPage(players[0], 'RUHMESHALLE');
  // "LADE …" is not a loaded page, even when the database is empty.
  await players[0].locator('#menu-body .fame-row, #menu-body .menu-note:has-text("NOCH KEINE")')
    .first().waitFor();
  if (process.env.CAW_EXPECT_FAME) {
    let found = false;
    for (let page = 0; page < 20; page++) {
      if (await players[0].locator('#menu-body .fame-row',
        { hasText: `BrowserAda·${ownFame.word}` }).count()) {
        found = true;
        break;
      }
      const next = players[0].getByRole('button', { name: 'Nächste Seite' });
      if (!(await next.count()) || (await next.isDisabled())) break;
      await next.click();
    }
    assert.ok(found, 'the browser player is reachable through Fame pagination');
    await checkFit(players[0], 'menu-fame', '#menu-close');
  }
  await players[0].locator('#menu-close').click();

  // The organizer now joins as a player. Every device must mark only that
  // authenticated seat, while the non-playing header badge disappears.
  await host.locator('#host-name').fill('Ivan');
  await host.locator('#host-join button').click();
  await host.locator('#lobby-players li.host-player', { hasText: 'Ivan' }).waitFor();
  await players[0].locator('#lobby-players li.host-player', { hasText: 'Ivan' }).waitFor();
  await screen.locator('#screen-players li.host-player', { hasText: 'Ivan' }).waitFor();
  for (const page of [host, players[0], screen]) {
    const list = page === screen ? '#screen-players' : '#lobby-players';
    assert.equal(await page.locator(`${list} .host-crown`).count(), 1,
      'exactly one player wears the crown');
    assert.match(await page.locator(`${list} li.host-player`).textContent(), /HOST/);
    assert.equal(await page.locator(`${list} .host-crown`)
      .evaluate((crown) => getComputedStyle(crown).color), 'rgb(215, 174, 74)');
  }
  assert.equal(await host.locator('#host-badge').isVisible(), false,
    'a playing host gets the crown next to their name instead of twice');
  await checkFit(host, 'host-player-lobby', '#start-button');
  await checkScreen(screen, 'lobby', 'lobby-host-player');

  // An unknown code ends on a clear page instead of a spinner.
  const lost = await openPhone(browser);
  await lost.goto(`${target}/s/ZZZZZZ`);
  await waitView(lost, 'missing');
  await checkFit(lost, 'missing', 'a.button');

  for (const page of pages) assert.deepEqual(page.errors, [], 'no script errors');
  console.log('PASS host + 3 phones + big screen: 3 rounds, reload, pause, judging, reveal, finish, rematch,');
  console.log('     kick, DE/EN; big screen public only and fitted at 1920x1080 / 1024x768;');
  console.log('     no page scroll at 320x568 / 390x844 / 1280x800, main action always on screen');
}

// Real packs have answers up to ~90 and prompts up to ~135 characters. The match above
// deals short test cards, so this draws a hand of long synthetic cards into the answer
// view and checks that no card cuts its text and the page still does not scroll.
async function checkLongCards(browser) {
  const page = await openPhone(browser);
  await page.goto(`${target}/s/ZZZZZZ`);
  await waitView(page, 'missing');
  await page.evaluate(() => {
    for (const view of document.querySelectorAll('.view')) view.hidden = view.id !== 'view-answer';
    const long = 'Testantwort mit einem sehr langen Kartentext, der auf dem Handy über mehrere Zeilen läuft';
    import('/play/sentence.js').then(({ fillSentence }) => {
      fillSentence(document.querySelector('#answer-prompt'),
        'Testfrage mit einem langen Satz, der so lang ist wie die längsten Fragen der echten Packs: ___ und ___.',
        [long, long]);
    });
    const hand = document.querySelector('#hand');
    for (let i = 0; i < 12; i++) {
      const card = document.createElement('button');
      card.className = 'card';
      card.textContent = i % 2 ? long : `Kurz ${i}`;
      hand.append(card);
    }
  });
  await checkTape(page, '#answer-prompt mark.answer-tape');
  assert.equal(await page.locator('#hand .card mark').count(), 0,
    'long hand cards remain plain and readable');
  for (const [width, height] of SIZES) {
    await page.setViewportSize({ width, height });
    await page.waitForTimeout(50); // Let the resize reflow the physical tape rows.
    assert.ok(await page.locator('#answer-prompt mark.answer-tape--strip').count() >= 2,
      `${width}x${height}: long inserted answers need separate strips per line`);
    const renderedText = await page.locator('#answer-prompt').textContent();
    assert.match(renderedText, /Testantwort mit einem sehr langen Kartentext/,
      'splitting labels must preserve the original answer wording');
    const clipped = await page.locator('#hand .card').evaluateAll((cards) =>
      cards.filter((card) => card.scrollHeight > card.clientHeight + 1).length);
    assert.equal(clipped, 0, `${width}x${height}: ${clipped} cards cut their text`);
    const dimensionsOfPage = await page.evaluate(() => ({
      height: document.documentElement.scrollHeight,
      width: document.documentElement.scrollWidth,
    }));
    assert.ok(dimensionsOfPage.height <= height, `${width}x${height}: long cards make the page scroll`);
    assert.ok(dimensionsOfPage.width <= width, `${width}x${height}: long tape overflows horizontally`);
    const hand = page.locator('#hand');
    const dimensions = await hand.evaluate((el) => ({
      scrollHeight: el.scrollHeight, clientHeight: el.clientHeight,
    }));
    assert.ok(dimensions.scrollHeight > dimensions.clientHeight,
      `${width}x${height}: all long cards should scroll inside the hand`);
    await page.locator('#hand .card').last().scrollIntoViewIfNeeded();
    const scrollTop = await hand.evaluate((el) => el.scrollTop);
    assert.ok(scrollTop > 0, `${width}x${height}: last card is reachable by scrolling`);
    const submit = await page.locator('#submit-button').boundingBox();
    assert.ok(submit.y >= 0 && submit.y + submit.height <= height + 0.5,
      `${width}x${height}: submit pushed off screen`);
  }
  console.log('PASS long cards: all cards scroll internally, text intact, submit fixed at every size');
}

(async () => {
  const browser = await chromium.launch({ headless: true, executablePath: process.env.CAW_CHROME_PATH });
  try {
    await checkLongCards(browser);
    await run(browser);
  } finally {
    await browser.close();
  }
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
