const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const root = path.join(__dirname, '..');
const html = fs.readFileSync(path.join(root, 'web/index.html'), 'utf8');
const app = fs.readFileSync(path.join(root, 'web/app.js'), 'utf8');
const css = fs.readFileSync(path.join(root, 'web/styles.css'), 'utf8');
const requiredIds = [
  'conceptsView','conceptMetrics','traversalNodes','traversalPrev','traversalNext',
  'traversalReset','traversalCaption','quizContent','quizProgress','quizScore',
  'resetQuiz','learningProgressSummary'
];
for (const id of requiredIds) {
  assert.match(html, new RegExp(`id=["']${id}["']`), `HTML should include ${id}`);
  assert.match(app, new RegExp(`\\$\\('${id}'\\)`), `app should wire ${id}`);
}
for (const phrase of ['Depth-first, pre-order walk','Complexity guide','Knowledge check','Progress on this device']) {
  assert.ok(html.includes(phrase), `Phase 2 UI should include ${phrase}`);
}
for (const phrase of ['function flattenTraversal','function renderConceptLab','function renderQuiz','function renderLearningProgress','function resetQuiz']) {
  assert.ok(app.includes(phrase), `Phase 2 app should implement ${phrase}`);
}
assert.ok(css.includes('.concept-grid') && css.includes('.quiz-question') && css.includes('@media(max-width:900px)'), 'Phase 2 layout and responsive rules should exist');
console.log(`PASS: ${requiredIds.length} Phase 2 UI bindings plus learning, traversal, quiz, progress, and responsive-layout smoke checks.`);
