const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const read = file => fs.readFileSync(path.join(__dirname, '..', file), 'utf8');
const context = vm.createContext({
  console,
  window: { addEventListener() {} },
  document: { compatMode: 'CSS1Compat', addEventListener() {} }
});
vm.runInContext(read('res/katex.bundle.js'), context);
vm.runInContext(read('res/nmd.html').match(/<script>\s*([\s\S]*?)<\/script>/)[1], context);
const render = md => {
  const tokens = context.marked.lexer(md);
  return context.marked.parser(tokens);
};

for (const text of ['$50 bucks and $100', '$50 and$100', '$50', '$ x$', '$x $']) {
  const output = render(text);
  assert.ok(!output.includes('class="katex'), text);
  assert.ok(output.includes(text), text);
}
for (const text of ['$x$', '$50 bucks and $100', '<img src=x onerror="alert(1)">']) {
  const output = render('`' + text + '`');
  assert.ok(output.includes('<code>'), text);
  assert.ok(!output.includes('class="katex'), text);
  assert.ok(!output.includes('<img'), text);
}
assert.ok(render('`&lt;div&gt;`').includes('&amp;lt;div&amp;gt;'));
for (const text of ['$E = mc^2$', '$50$', '($x$)', '$$x^2$$', '$$\nx^2\n$$', '```math\nx^2\n```']) {
  assert.ok(render(text).includes('class="katex'), text);
}
const mixed = render('$50 bucks and $100; then $x^2$');
assert.ok(mixed.includes('$50 bucks and $100; then '));
assert.equal((mixed.match(/class="katex"/g) || []).length, 1);
console.log('KaTeX currency, literal code, and math checks passed.');
