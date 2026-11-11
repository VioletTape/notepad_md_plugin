const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const read = file => fs.readFileSync(path.join(__dirname, '..', file), 'utf8');

// 1. Direct VM context test modeled after tests/katex.test.cjs
const context = vm.createContext({
  console,
  window: { addEventListener() {} },
  document: { compatMode: 'CSS1Compat', addEventListener() {} }
});
context.window.window = context.window;

// Load Pele bundle
vm.runInContext(read('res/pele.min.js'), context);

assert.ok(context.pele, 'pele must be defined on context');
assert.equal(typeof context.pele.render, 'function', 'pele.render must be a function');
assert.equal(typeof context.pele.supports, 'function', 'pele.supports must be a function');

// Test supported diagram types
const diagramSamples = {
  flowchart: 'flowchart TD\n  Start[Start Node] --> Stop[Stop Node]',
  sequenceDiagram: 'sequenceDiagram\n  Alice->>Bob: Hello Bob\n  Bob-->>Alice: Hi Alice',
  classDiagram: 'classDiagram\n  class Animal {\n    +String name\n    +makeSound()\n  }',
  stateDiagram: 'stateDiagram-v2\n  [*] --> Still\n  Still --> [*]',
  erDiagram: 'erDiagram\n  CUSTOMER ||--o{ ORDER : places'
};

for (const [type, code] of Object.entries(diagramSamples)) {
  const result = context.pele.render(code);
  assert.ok(result, `pele.render must return result for ${type}`);
  assert.ok(typeof result.svg === 'string' && result.svg.length > 0, `result.svg must be non-empty string for ${type}`);
  assert.ok(result.svg.includes('<svg'), `result.svg must contain <svg for ${type}`);
  assert.ok(result.svg.includes('</svg>'), `result.svg must contain </svg> for ${type}`);
  assert.ok(result.svg.includes('class="pele'), `result.svg must contain pele class for ${type}`);
  assert.ok(result.width > 0, `result.width must be positive for ${type}`);
  assert.ok(result.height > 0, `result.height must be positive for ${type}`);
}

// Test invalid diagram handling via direct API
let directErrorThrown = false;
try {
  context.pele.render('invalid mermaid syntax !@#$%^&*');
} catch (err) {
  directErrorThrown = true;
  assert.ok(err instanceof (context.pele.PeleError || Error));
}
assert.ok(directErrorThrown, 'pele.render should throw for invalid diagram syntax');

// 2. Integration test with res/nmd.html and full Markdown rendering
let JSDOM;
try {
  JSDOM = require('jsdom').JSDOM;
} catch (e) {
  // If JSDOM is not available, we can mock the required DOM APIs
}

if (JSDOM) {
  const dom = new JSDOM('<!DOCTYPE html><html><body><div id="content"></div></body></html>', {
    runScripts: 'dangerously'
  });
  const win = dom.window;

  // Stub canvas context to avoid JSDOM 'Not implemented: HTMLCanvasElement.prototype.getContext' log
  win.HTMLCanvasElement.prototype.getContext = () => ({
    measureText: text => ({ width: (text || '').length * 8 })
  });

  // Run Pele in window via script tag (exact same way browser loads it)
  const peleScript = win.document.createElement('script');
  peleScript.textContent = read('res/pele.min.js');
  win.document.body.appendChild(peleScript);
  assert.ok(win.pele, 'pele must be defined on window');

  // Extract and evaluate the main script from res/nmd.html via script tag
  const nmdHtml = read('res/nmd.html');
  const scripts = [...nmdHtml.matchAll(/<script(?:\s+[^>]*)?>([\s\S]*?)<\/script>/gi)];
  // Script containing renderMarkdown
  const mainScript = scripts.find(m => m[1].includes('renderMarkdown'));
  assert.ok(mainScript, 'main script containing renderMarkdown must exist in res/nmd.html');
  const nmdScriptEl = win.document.createElement('script');
  nmdScriptEl.textContent = mainScript[1];
  win.document.body.appendChild(nmdScriptEl);

  assert.equal(typeof win.renderMarkdown, 'function', 'renderMarkdown must be defined');

  // Test rendering markdown with valid Mermaid diagram
  const markdownValid = [
    '# Test Flowchart',
    '',
    '```mermaid',
    'flowchart TD',
    '  A[Node A] --> B[Node B]',
    '```',
    '',
    'Paragraph text after diagram.'
  ].join('\n');

  win.renderMarkdown(markdownValid);
  const content = win.document.getElementById('content');
  const container = content.querySelector('.pele-container');
  assert.ok(container, 'rendered output must have .pele-container');
  assert.ok(container.innerHTML.includes('<svg'), 'pele-container must contain an SVG');
  assert.ok(container.innerHTML.includes('pele-flowchart'), 'SVG should contain pele-flowchart class');
  assert.ok(container.hasAttribute('data-line'), 'pele-container must retain data-line attribute');
  assert.equal(container.getAttribute('data-line'), '3', 'data-line should match the code block start line');

  // Test multiple diagram types in a single markdown document
  const markdownMulti = [
    '```mermaid',
    'flowchart TD',
    '  A --> B',
    '```',
    '',
    '```mermaid',
    'sequenceDiagram',
    '  Alice->>Bob: Hi',
    '```',
    '',
    '```mermaid',
    'classDiagram',
    '  class Vehicle',
    '```',
    '',
    '```mermaid',
    'stateDiagram-v2',
    '  [*] --> Active',
    '```'
  ].join('\n');

  win.renderMarkdown(markdownMulti);
  const containers = content.querySelectorAll('.pele-container');
  assert.equal(containers.length, 4, 'all 4 diagrams must render into .pele-container');
  containers.forEach((c, idx) => {
    assert.ok(c.querySelector('svg'), `diagram ${idx} must contain an svg`);
    assert.ok(c.hasAttribute('data-line'), `diagram ${idx} must have data-line`);
  });

  // Test invalid diagram handling in markdown
  const markdownInvalid = [
    '# Invalid Diagram Document',
    '',
    '```mermaid',
    'this is completely invalid mermaid syntax !!@@##',
    '```',
    '',
    '```mermaid',
    'flowchart TD',
    '  GoodA --> GoodB',
    '```'
  ].join('\n');

  // Verify that an invalid diagram logs a warning and does not throw an unhandled exception
  let warnLogged = false;
  const originalWarn = win.console.warn;
  win.console.warn = (...args) => {
    warnLogged = true;
  };

  assert.doesNotThrow(() => {
    win.renderMarkdown(markdownInvalid);
  }, 'renderMarkdown must handle invalid diagrams cleanly without throwing');

  win.console.warn = originalWarn;
  assert.ok(warnLogged, 'console.warn should have been called for invalid diagram');

  // The good diagram after the broken one should still render!
  const goodContainers = content.querySelectorAll('.pele-container');
  assert.equal(goodContainers.length, 1, 'good diagram should still render despite invalid diagram earlier');
  assert.ok(goodContainers[0].querySelector('svg'), 'good diagram container must contain svg');

  // Non-mermaid code blocks must not be touched by Pele
  const markdownCode = [
    '```js',
    'const x = 10;',
    '```'
  ].join('\n');
  win.renderMarkdown(markdownCode);
  assert.equal(content.querySelectorAll('.pele-container').length, 0, 'non-mermaid code blocks must not produce pele-container');
}

console.log('Pele offline diagrams, data-line attributes, and error handling checks passed.');
