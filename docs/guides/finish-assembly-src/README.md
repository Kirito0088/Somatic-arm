# Finish-assembly guide: source

`guide.html` + `img/` build `../Somatic-Arm-Finish-Assembly.pdf`. Part pictures in `img/r*.jpg` are cropped from the stringing guide's renders; `photo*.jpg` is the 7 Oct build photo.

Rebuild the PDF (needs Node and Playwright with Chromium):

```
npm install playwright
npx playwright install chromium
node render.js "<full path to>/guide.html" ../Somatic-Arm-Finish-Assembly.pdf
```

The HTML path must be absolute. Or open `guide.html` in Chrome and print to PDF (A4, margins none, background graphics on).
