// Browser regression tests for the landing page and the one-shot CUT DECK intro.
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');

const target = process.env.CAW_SMOKE_URL || 'http://localhost:8080';

async function assertFitsViewport(page) {
  const size = await page.evaluate(() => ({
    width: document.documentElement.scrollWidth,
    height: document.documentElement.scrollHeight,
    viewportWidth: innerWidth,
    viewportHeight: innerHeight,
  }));
  assert.ok(size.width <= size.viewportWidth, 'Landing must not scroll horizontally');
  assert.ok(size.height <= size.viewportHeight, 'Landing should fit without vertical scrolling');
}

async function verifyPhysicalDeck(page) {
  const cards = await page.locator('.cut-deck--small .cut-card').evaluateAll((nodes) =>
    nodes.map((card) => {
      const style = getComputedStyle(card);
      return {
        radius: style.borderRadius,
        background: style.backgroundColor,
        transform: style.transform,
      };
    }));
  assert.equal(cards.length, 3, 'the logo has two card backs and a front card');
  assert.ok(cards.every((card) => card.radius === '5px'), 'cards have subtle paper corners');
  assert.equal(cards[0].background, cards[1].background, 'two light paper backs');
  assert.notEqual(cards[1].background, cards[2].background, 'contrasting black card front');
  assert.notEqual(cards[0].transform, cards[2].transform, 'stacked cards have distinct angles');
  const front = await page.locator('.cut-deck--small .cut-card--w')
    .evaluate((card) => getComputedStyle(card, '::before').content);
  assert.ok(front.includes('C/A/W'), 'the front card carries the CAW mark');
}

async function checkLanding(browser, width, height) {
  const page = await browser.newPage({ viewport: { width, height }, locale: 'de-DE' });
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  try {
    await page.goto(target);
    await page.locator('#boot-screen').waitFor({ state: 'visible' });
    await page.getByRole('button', { name: /ÜBERSPRINGEN/ }).click();
    await page.locator('#boot-screen').waitFor({ state: 'hidden' });
    await verifyPhysicalDeck(page);

    await page.getByRole('button', { name: 'DE', exact: true }).click();
    await page.getByText('[ NEUE SESSION ]', { exact: true }).waitFor();
    // The four-step opening guide is visible on every screen size, but
    // its text scrolls only inside the native modal, never the page.
    await page.getByRole('button', { name: '[ ANLEITUNG ]' }).click();
    const guide = page.getByRole('dialog', { name: 'SO ÖFFNEST DU CAW' });
    await guide.waitFor({ state: 'visible' });
    assert.equal(await guide.locator('li').count(), 4);
    assert.match(await guide.textContent(), /QR-Code/);
    assert.match(await guide.textContent(), /localhost/);
    assert.match(await guide.textContent(), /BIG SCREEN/);
    const guideBounds = await guide.boundingBox();
    assert.ok(guideBounds.y >= 0 && guideBounds.y + guideBounds.height <= height + 1,
      'the guide dialog must fit inside the viewport');
    await page.keyboard.press('Escape');
    await guide.waitFor({ state: 'hidden' });
    assert.equal(await page.getByText('[ NEUE SESSION ]', { exact: true }).isDisabled(), false);
    await page.getByLabel('Session-Code (6 Zeichen)').fill('k7m2qx');
    assert.equal(await page.locator('#code').inputValue(), 'K7M2QX');
    await page.locator('#code').fill('LLLLLL');
    assert.equal(await page.locator('#code').evaluate(input => input.checkValidity()), false);
    await page.getByRole('button', { name: '[ BEITRETEN ]' }).click();
    await page.locator('#status', { hasText: '6 ZEICHEN' }).waitFor();
    // A well-formed code that does not exist is asked for and refused by the server.
    await page.locator('#code').fill('ZZZZZZ');
    await page.getByRole('button', { name: '[ BEITRETEN ]' }).click();
    await page.locator('#status', { hasText: 'SESSION NICHT GEFUNDEN' }).waitFor();
    await page.locator('#code').fill('K7M2QX');

    await page.getByRole('button', { name: 'EN', exact: true }).click();
    await page.getByText('[ NEW SESSION ]', { exact: true }).waitFor();
    await page.getByRole('button', { name: '[ HOW TO START ]' }).click();
    const englishGuide = page.getByRole('dialog', { name: 'HOW TO OPEN CAW' });
    await englishGuide.waitFor({ state: 'visible' });
    assert.match(await englishGuide.textContent(), /Cloudflare Tunnel/);
    await page.getByRole('button', { name: '[ CLOSE ]' }).click();
    await englishGuide.waitFor({ state: 'hidden' });
    assert.equal(await page.locator('html').getAttribute('lang'), 'en');
    await assertFitsViewport(page);

    for (const control of await page.locator('button, input').all()) {
      if (!await control.isVisible()) continue;
      const bounds = await control.boundingBox();
      assert.ok(bounds.height >= 48, 'Visible controls must be at least 48px tall');
    }

    await page.getByRole('button', { name: 'DE', exact: true }).focus();
    await page.keyboard.press('Enter');
    assert.equal(await page.locator('html').getAttribute('lang'), 'de');
    await page.reload();
    assert.equal(await page.locator('#boot-screen').isVisible(), false,
      'Intro must not replay on reload within the same browser session');

    if (width === 1280) {
      const beganAt = Date.now();
      await page.getByRole('button', { name: 'CAW-Intro erneut abspielen' }).click();
      await page.locator('#boot-screen').waitFor({ state: 'visible' });
      await page.locator('#boot-screen.has-wordmark').waitFor({ state: 'visible', timeout: 2000 });
      await page.locator('#boot-screen').waitFor({ state: 'hidden', timeout: 2600 });
      assert.ok(Date.now() - beganAt < 2400, 'the full intro should be under 2.4 seconds');
      await assertFitsViewport(page);
    }

    assert.deepEqual(errors, []);
    if (process.env.CAW_SCREENSHOT_DIR) {
      fs.mkdirSync(process.env.CAW_SCREENSHOT_DIR, { recursive: true });
      await page.screenshot({
        path: `${process.env.CAW_SCREENSHOT_DIR}/landing-${width}.png`,
        fullPage: true,
      });
    }
    console.log(`PASS ${width}x${height}: intro, DE/EN, keyboard, physical cards, 48px targets, zero scroll`);
  } finally {
    await page.close();
  }
}

// The footer's info pages render from their text tables and the server's pack list.
async function checkInfoPages(browser) {
  const page = await browser.newPage({ locale: 'de-DE' });
  try {
    await page.goto(`${target}/lizenzen.html`);
    await page.locator('.info-list li').first().waitFor();
    assert.match(await page.locator('.info-list').textContent(), /CC BY-NC-SA 2\.0/);
    await page.goto(`${target}/datenschutz.html`);
    await page.locator('#content p').nth(4).waitFor();
    assert.match(await page.locator('#content').textContent(), /keine Cookies/);
    await page.goto(`${target}/impressum.html`);
    await page.locator('#content p').first().waitFor();
    console.log('PASS info pages: licenses with packs, privacy, imprint');
  } finally {
    await page.close();
  }
}

async function checkReducedMotion(browser) {
  const page = await browser.newPage({ locale: 'en-US' });
  try {
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await page.goto(target);
    assert.equal(await page.locator('#boot-screen').isVisible(), false,
      'Reduced-motion visitors must skip the animation');
    await page.getByRole('button', { name: 'Replay CAW intro' }).click();
    assert.equal(await page.locator('#boot-screen').isVisible(), false);
    await assertFitsViewport(page);
    console.log('PASS reduced motion: intro stays skipped');
  } finally {
    await page.close();
  }
}

(async () => {
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.CAW_CHROME_PATH,
  });
  try {
    for (const [width, height] of [[320, 568], [390, 844], [1280, 800]]) {
      await checkLanding(browser, width, height);
    }
    await checkReducedMotion(browser);
    await checkInfoPages(browser);
  } finally {
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
