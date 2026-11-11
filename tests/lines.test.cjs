const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const read = file => fs.readFileSync(path.join(__dirname, '..', file), 'utf8');

// 1. Test __computeTargetLines logic directly
const context = vm.createContext({
  console,
  window: { addEventListener() {} },
  document: { compatMode: 'CSS1Compat', addEventListener() {} }
});
vm.runInContext(read('res/nmd.html').match(/<script>\s*([\s\S]*?)<\/script>/)[1], context);

assert.equal(typeof context.__computeTargetLines, 'function', '__computeTargetLines must be defined');

// Test Case 1: Dense document with 50 lines (all non-empty)
const denseMd = Array.from({ length: 50 }, (_, i) => `Line ${i + 1}`).join('\n');
const targetsDense = context.__computeTargetLines(denseMd);
assert.deepEqual([...targetsDense], [10, 20, 30, 40, 50], 'Dense markdown should have multiples of 10');

// Test Case 2: User-specified scenario: 10 non-empty, 15-33 empty, 34 non-empty, 40 non-empty
const sparseLines = Array.from({ length: 60 }, (_, i) => {
  const line = i + 1;
  if (line >= 15 && line <= 33) return '';
  return `Line ${line} content`;
});
const targetsSparse = context.__computeTargetLines(sparseLines.join('\n'));
assert.deepEqual([...targetsSparse], [10, 34, 40, 50, 60], 'Should skip empty lines to 34, then continue with 40');

// Test Case 3: Empty line right on line 10 (lines 10-11 empty, line 12 has text)
const empty10Lines = Array.from({ length: 30 }, (_, i) => {
  const line = i + 1;
  if (line === 10 || line === 11) return '';
  return `Line ${line} content`;
});
const targetsEmpty10 = context.__computeTargetLines(empty10Lines.join('\n'));
assert.deepEqual([...targetsEmpty10], [12, 20, 30], 'Should advance to line 12 when line 10 is empty');

// Test Case 4: Short document (<10 lines)
const shortMd = 'Line 1\nLine 2\nLine 3';
assert.deepEqual([...context.__computeTargetLines(shortMd)], [], 'Fewer than 10 lines should produce no numbers');

// 2. Integration test with full renderMarkdown and DOM anchors
const mockGutter = {
  id: 'nmd-line-gutter',
  innerHTML: '',
  children: [],
  appendChild(child) {
    if (child.nodeType === 11) { // DocumentFragment
      while (child.childNodes && child.childNodes.length) {
        this.children.push(child.childNodes.shift());
      }
    } else {
      this.children.push(child);
    }
  }
};

const mockElements = new Map();
const mockContent = {
  id: 'content',
  innerHTML: '',
  children: [],
  querySelectorAll(sel) {
    const results = [];
    if (sel.includes('img')) return results;
    if (sel.includes('blockquote')) return results;
    if (sel.includes('h1')) return results;
    return results;
  }
};

const domContext = vm.createContext({
  console,
  window: {
    addEventListener() {},
    scrollY: 0,
    pageYOffset: 0
  },
  document: {
    compatMode: 'CSS1Compat',
    addEventListener() {},
    getElementById(id) {
      if (id === 'nmd-line-gutter') return mockGutter;
      if (id === 'content') return mockContent;
      return null;
    },
    createElement(tag) {
      return {
        tagName: tag.toUpperCase(),
        className: '',
        style: {},
        textContent: '',
        setAttribute(k, v) { this[k] = v; },
        getAttribute(k) { return this[k]; }
      };
    },
    createDocumentFragment() {
      return {
        nodeType: 11,
        childNodes: [],
        appendChild(child) { this.childNodes.push(child); }
      };
    },
    querySelectorAll(sel) {
      return Array.from(mockElements.values());
    },
    querySelector(sel) {
      const match = sel.match(/\[data-line="(\d+)"\]/);
      if (match) {
        return mockElements.get(parseInt(match[1], 10)) || null;
      }
      return null;
    }
  }
});
domContext.window.window = domContext.window;

vm.runInContext(read('res/nmd.html').match(/<script>\s*([\s\S]*?)<\/script>/)[1], domContext);

// Create sample markdown with YAML front-matter (3 lines) and text
const testDoc = [
  '---',
  'title: Test',
  '---',
  '',
  '# Header at editor line 5',
  '',
  'Line 7 paragraph text.',
  'Line 8 paragraph text.',
  'Line 9 paragraph text.',
  'Line 10 paragraph text (editor line 10).',
  'Line 11 paragraph text.',
  '',
  '- Item 1 (line 13)',
  '- Item 2 (line 14)',
  '- Item 3 (line 15)'
].join('\n');

domContext.renderMarkdown(testDoc);

assert.equal(domContext.window._fmOffset, 3, 'Front-matter offset must be 3');
assert.ok(domContext.window.__targetLines.includes(10), 'Target lines must include editor line 10');

// Verify that parsed HTML in content has data-line anchors for multiline paragraph and list
const htmlOutput = mockContent.innerHTML;
assert.ok(htmlOutput.includes('data-line='), 'HTML output must have data-line attributes');
assert.ok(htmlOutput.includes('class="nmd-line-anchor"'), 'HTML output must include nmd-line-anchor spans');
assert.ok(htmlOutput.includes('<li data-line='), 'List items must have data-line attributes');

// Verify that bodyLine 7 (which corresponds to editor line 10 = 7 + fmOffset 3) exists in HTML
assert.ok(htmlOutput.includes('data-line="7"'), 'Body line 7 must be present in HTML for editor line 10');

console.log('Line numbers logic, gutter targets, and DOM anchor tests passed.');
