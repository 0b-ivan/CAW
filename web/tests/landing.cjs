// Responsive, keyboard and language smoke for the M0 page only.
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
(async () => {
  const browser = await chromium.launch({ headless: true, executablePath: process.env.CAW_CHROME_PATH });
  try {
    for (const [width, height] of [[320, 568], [390, 844], [1280, 800]]) {
      const page = await browser.newPage({ viewport: { width, height }, locale: 'de-DE' });
      const errors = [];
      page.on('pageerror', error => errors.push(error.message));
      await page.goto(process.env.CAW_SMOKE_URL || 'http://localhost:8080');
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
      assert.equal(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), true);
      for (const control of await page.locator('button, input').all()) {
        assert.ok((await control.boundingBox()).height >= 48);
      }
      await page.getByRole('button', { name: 'DE', exact: true }).focus();
      await page.keyboard.press('Enter');
      assert.equal(await page.locator('html').getAttribute('lang'), 'de');
      assert.deepEqual(errors, []);
      if (process.env.CAW_SCREENSHOT_DIR) {
        fs.mkdirSync(process.env.CAW_SCREENSHOT_DIR, { recursive: true });
        await page.screenshot({ path: `${process.env.CAW_SCREENSHOT_DIR}/landing-${width}.png`, fullPage: true });
      }
      console.log(`PASS ${width}x${height}: DE/EN, keyboard, 48px targets, no horizontal overflow`);
      await page.close();
    }
  } finally {
    await browser.close();
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
