#!/usr/bin/env node
/**
 * Guards the chooser/late-import bootstrap: main.js must init when
 * document.readyState is already past "loading".
 */
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const dir = path.dirname(fileURLToPath(import.meta.url));
const mainSrc = fs.readFileSync(path.join(dir, 'main.js'), 'utf8');
const htmlSrc = fs.readFileSync(path.join(dir, 'index.html'), 'utf8');

assert.match(
  mainSrc,
  /document\.readyState\s*===\s*['"]loading['"]/,
  'main.js must check document.readyState before waiting for DOMContentLoaded',
);
assert.match(
  mainSrc,
  /addEventListener\(\s*['"]DOMContentLoaded['"]\s*,\s*init/,
  'main.js must still listen for DOMContentLoaded when the document is loading',
);
assert.match(
  mainSrc,
  /else\s*\{\s*init\(\)\s*;?\s*\}/,
  'main.js must call init() immediately when the document is already ready',
);

assert.match(
  htmlSrc,
  /<a[^>]*id="pick-web"[^>]*href="\?play=1"/,
  'Play in browser must be a link to ?play=1 so it works if click JS does not bind',
);

function whenDocumentReady(document, addEventListener, init) {
  if (document.readyState === 'loading') {
    addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
}

let called = 0;
whenDocumentReady({ readyState: 'complete' }, () => {
  throw new Error('should not wait for DOMContentLoaded after load');
}, () => { called += 1; });
assert.equal(called, 1, 'init runs when readyState is complete');

called = 0;
whenDocumentReady({ readyState: 'interactive' }, () => {
  throw new Error('should not wait for DOMContentLoaded when interactive');
}, () => { called += 1; });
assert.equal(called, 1, 'init runs when readyState is interactive');

called = 0;
let listener = null;
whenDocumentReady({ readyState: 'loading' }, (type, fn) => {
  assert.equal(type, 'DOMContentLoaded');
  listener = fn;
}, () => { called += 1; });
assert.equal(called, 0, 'init waits while the document is still loading');
listener();
assert.equal(called, 1, 'init runs once DOMContentLoaded fires');

console.log('web/bootstrap.test.mjs: ok');
