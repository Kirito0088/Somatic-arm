const { chromium } = require('playwright');
(async () => {
  const [,, html, pdf, pngPrefix] = process.argv;
  const b = await chromium.launch();
  const p = await b.newPage();
  await p.goto('file://' + html, { waitUntil: 'networkidle' });
  await p.pdf({ path: pdf, format: 'A4', printBackground: true, preferCSSPageSize: true });
  if (pngPrefix) {
    await p.setViewportSize({ width: 794, height: 1123 });
    await p.emulateMedia({ media: 'print' });
    const n = await p.$$eval('section.page', e => e.length);
    for (let i = 0; i < n; i++) {
      const el = (await p.$$('section.page'))[i];
      await el.screenshot({ path: `${pngPrefix}-${i+1}.png` });
    }
  }
  await b.close();
})();
