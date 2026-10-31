// Browser regression tests for the M0 landing and the one-shot CUT DECK intro.
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

async function verifyCardsHaveSquareCorners(page) {
  const corners = await page.locator('.cut-deck--small .cut-card--w').evaluate((card) => {
    const style = getComputedStyle(card);
    const marker = getComputedStyle(card, '::after');
    return {
      radius: style.borderBottomRightRadius,
      markerRight: marker.right,
      markerBorderRight: marker.borderRightWidth,
      markerBorderBottom: marker.borderBottomWidth,
    };
  });
  assert.equal(corners.radius, '0px');
  assert.equal(corners.markerRight, '3px');
  assert.equal(corners.markerBorderRight, '1px');
  assert.equal(corners.markerBorderBottom, '1px');
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
    await verifyCardsHaveSquareCorners(page);

    await page.getByRole('button', { name: 'DE', exact: true }).click();
    await page.getByText('[ NEUE SESSION ]', { exact: true }).waitFor();
    assert.equal(await page.getByText('[ NEUE SESSION ]', { exact: true }).isDisabled(), true);
    await page.getByLabel('Session-Code (6 Zeichen)').fill('k7m2qx');
    assert.equal(await page.locator('#code').inputValue(), 'K7M2QX');
    await page.locator('#code').fill('LLLLLL');
    assert.equal(await page.locator('#code').evaluate(input => input.checkValidity()), false);
    await page.locator('#code').fill('K7M2QX');

    await page.getByRole('button', { name: 'EN', exact: true }).click();
    await page.getByText('[ NEW SESSION ]', { exact: true }).waitFor();
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
      await page.getByRole('button', { name: 'CAW-Intro erneut abspielen' }).click();
      await page.locator('#boot-screen').waitFor({ state: 'visible' });
      await page.locator('#boot-screen.has-wordmark').waitFor({ state: 'visible', timeout: 5000 });
      await page.locator('#boot-screen').waitFor({ state: 'hidden', timeout: 5000 });
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
    console.log(`PASS ${width}x${height}: intro, DE/EN, keyboard, square cards, 48px targets, zero scroll`);
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
  } finally {
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
