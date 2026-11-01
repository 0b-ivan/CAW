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
    assert.equal(paper.radius, '5px', 'answers have physical card corners');
    await cards.first().click();
    await page.locator('#hand .card .slot-mark', { hasText: '<1/' }).waitFor();
    const selected = await cards.first().evaluate((card) => ({
      background: getComputedStyle(card).backgroundColor,
      pressed: card.getAttribute('aria-pressed'),
    }));
    assert.equal(selected.background, 'rgb(8, 8, 8)', 'selected answer is dark');
    assert.equal(selected.pressed, 'true', 'selection is accessible as text/ARIA');
    assert.equal(await page.locator('#answer-prompt mark').count(), 1, 'the chosen card fills the blank');
    await page.locator('#hand .card').first().click();
    assert.equal(await page.locator('#hand .slot-mark').count(), 0);

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
    const screenPaper = await screen.locator('#screen-plays li').first()
      .evaluate((card) => getComputedStyle(card).backgroundColor);
    assert.equal(screenPaper, 'rgb(244, 244, 240)', 'TV shows legible paper answer cards');
  }
  const judgePaper = await juror.locator('#judge-list .choice').first()
    .evaluate((card) => getComputedStyle(card).backgroundColor);
  assert.equal(judgePaper, 'rgb(244, 244, 240)', 'juror options are paper strips');
  await juror.locator('#judge-list .choice').first().click();
  assert.equal(await juror.locator('#judge-list .choice').first().getAttribute('aria-pressed'),
    'true', 'the chosen answer is also announced to assistive technology');
  // The pick fills the blanks of the prompt shown once on top.
  assert.ok(await juror.locator('#judge-prompt mark').count() >= 1, 'the pick appears in the prompt');
  await juror.getByRole('button', { name: '[ BESTÄTIGEN ]' }).click();

  for (const page of [host, ...players]) await waitView(page, 'reveal');
  assert.match(await host.locator('#reveal-winner').textContent(), /^GEWINNER: /);
  const revealPaper = await host.locator('#reveal-sentence')
    .evaluate((card) => getComputedStyle(card).backgroundColor);
  assert.equal(revealPaper, 'rgb(244, 244, 240)', 'the reveal is a single large paper card');
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
  await host.locator('#lobby-qr svg').waitFor();
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
  for (const name of ['Ada', 'Bert', 'Cyra']) players.push(await joinAs(browser, code, name));
  await host.locator('#lobby-players li', { hasText: 'Cyra' }).waitFor();
  await checkFit(players[0], 'lobby', '#lobby-status');
  await screen.locator('#screen-players li', { hasText: 'Cyra·' }).waitFor();
  // Every name carries its word, and the three words differ.
  const words = await host.locator('#lobby-players li').allTextContents();
  assert.equal(new Set(words.map((label) => label.split('·')[1])).size, 3, 'three different words');
  await checkScreen(screen, 'lobby', 'lobby');

  // The host finds the big screen link as a QR in the menu.
  await host.locator('#screen-link').click();
  await host.locator('.menu-qr svg').waitFor();
  await checkFit(host, 'menu-screen', '#menu-close');
  await host.locator('#menu-close').click();

  // Three rounds instead of ten: ‹ steps 10 → 8 → 5 → 3.
  await openMenuPage(host, 'SPIELREGELN');
  const fewer = host.locator('.stepper').first().getByRole('button', { name: 'Weniger' });
  for (let i = 0; i < 3; i++) await fewer.click();
  await host.locator('.stepper .stepper-value').first().filter({ hasText: /^3$/ }).waitFor();
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

  await host.locator('#start-button').click();
  for (let round = 1; round <= 3; round++) await playRound(host, players, round, screen);

  for (const page of [host, ...players]) await waitView(page, 'finished');
  await checkScreen(screen, 'finished', 'finished');
  assert.equal(await players[0].locator('#finished-lines li').count(), 3, 'the three winning sentences');
  await screen.locator('#screen-line-text', { hasText: ':' }).waitFor();
  for (const payload of screenFrames) {
    const frame = JSON.parse(payload);
    assert.equal(frame.type, 'screen', 'the big screen only gets screen frames');
    assert.ok(!('me' in frame) && !payload.includes('"hand"'), 'the big screen never sees a hand');
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

  // The Wall of Fame: with Postgres (CI) the match is in it; without, it says so.
  let fameRows = 0;
  for (let attempt = 0; attempt < 20 && fameRows === 0; attempt++) {
    await players[0].locator('#menu-button').click();
    await players[0].locator('.menu-row', { hasText: 'RUHMESHALLE' }).click();
    // Rows, or the explicit "no entries" note; "LADE …" is neither and must not count.
    await players[0].locator('#menu-body .fame-row, #menu-body .menu-note:has-text("NOCH KEINE")')
      .first().waitFor();
    fameRows = await players[0].locator('#menu-body .fame-row').count();
    if (fameRows === 0 && process.env.CAW_EXPECT_FAME) {
      await players[0].locator('#menu-close').click();
      await players[0].waitForTimeout(500);
    } else break;
  }
  if (process.env.CAW_EXPECT_FAME) {
    assert.ok(fameRows > 0, 'the finished match is in the Wall of Fame');
    await players[0].locator('#menu-body .fame-row', { hasText: 'Ada·' }).first().waitFor();
    await checkFit(players[0], 'menu-fame', '#menu-close');
  }
  await players[0].locator('#menu-close').click();

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
    document.querySelector('#answer-prompt').textContent =
      'Testfrage mit einem langen Satz, der so lang ist wie die längsten Fragen der echten Packs: ___ und ___.';
    const hand = document.querySelector('#hand');
    for (let i = 0; i < 12; i++) {
      const card = document.createElement('button');
      card.className = 'card';
      card.textContent = i % 2 ? long : `Kurz ${i}`;
      hand.append(card);
    }
  });
  for (const [width, height] of SIZES) {
    await page.setViewportSize({ width, height });
    const clipped = await page.locator('#hand .card').evaluateAll((cards) =>
      cards.filter((card) => card.scrollHeight > card.clientHeight + 1).length);
    assert.equal(clipped, 0, `${width}x${height}: ${clipped} cards cut their text`);
    const pageHeight = await page.evaluate(() => document.documentElement.scrollHeight);
    assert.ok(pageHeight <= height, `${width}x${height}: long cards make the page scroll`);
    const submit = await page.locator('#submit-button').boundingBox();
    assert.ok(submit.y + submit.height <= height + 0.5, `${width}x${height}: submit pushed off screen`);
  }
  console.log('PASS long cards: no text cut, no page scroll, submit on screen at all sizes');
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
