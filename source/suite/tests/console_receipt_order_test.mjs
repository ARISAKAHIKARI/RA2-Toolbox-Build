import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const source = process.argv.includes('--app') ? process.argv[process.argv.indexOf('--app') + 1] : path.join(here, '../frontend/app.js');
const context = vm.createContext({ console, Date, Map, Set, Object, Number, String, Boolean, RangeError });
vm.runInContext(fs.readFileSync(source, 'utf8'), context);
const { ConsoleStore } = context.RA2ConsoleCore;
const tests = [];
const test = (name, run) => tests.push({ name, run });
const toggle = { id: 1, type: 'toggle', checked: false };
const text = { id: 1, type: 'text', text: 'original' };

function setup(control = toggle) {
  const messages = [];
  const store = new ConsoleStore(message => messages.push(message), () => {}, { initialSeq: 0 });
  const state = (revision, patch = {}, generation = 1) => ({
    kind: 'state', protocol: 1, module: 0, generation, revision, page: 'cursor', controls: [{ ...control, ...patch }]
  });
  const ack = (seq, okay, revision = 0, generation = 1) => ({
    kind: 'ack', protocol: 1, module: 0, generation, id: 1, seq, ok: okay,
    revision, error: okay ? '' : '此设置当前不可用'
  });
  store.receive(state(1));
  return { store, messages, state, ack };
}

test('a round-trip setting waits for its own receipt when the original value reappears', () => {
  const { store, state, ack } = setup();
  store.act(1, 'toggle', true); store.act(1, 'toggle', false);
  store.receive(state(2));
  assert.equal(store.isPending(1), true, 'a matching state is not evidence that the latest request ran');
  assert.equal(store.receive(ack(2, false)), true);
  assert.equal(store.isPending(1), false); assert.equal(store.error(1), '此设置当前不可用');
  assert.equal(store.value(store.getControl(1), 'toggle'), false);
});

test('a snapshot sent after a bounded ACK batch cannot swallow the later rejection', () => {
  const { store, state, ack } = setup();
  store.act(1, 'toggle', true); store.act(1, 'toggle', false);
  // The native publisher drains at most 32 ACKs before publishing a state.
  // Receipts for other controls may precede this control's own receipt.
  for (let id = 2; id <= 33; id++) store.receive({ ...ack(id, true, 2), id });
  store.receive(state(2));
  assert.equal(store.receive(ack(2, false)), true);
  assert.equal(store.error(1), '此设置当前不可用');
});

for (const [name, control, action, value, patch] of [
  ['toggle', toggle, 'toggle', true, { checked: true }],
  ['text', text, 'text', 'wanted', { text: 'wanted' }],
  ['choice', { id: 1, type: 'choice', options: ['A', 'B'], selected: 0, readonly: true }, 'select', 1, { selected: 1 }],
  ['range', { id: 1, type: 'range', min: 0, max: 100, text: '0' }, 'range', 40, { text: '40' }],
  ['hotkey', { id: 1, type: 'hotkey', selected: 0 }, 'hotkey', 0x241, { selected: 0x241 }]
]) {
  test(`${name} state before success ACK settles immediately once its revision is certified`, () => {
    const { store, state, ack } = setup(control);
    assert.equal(store.act(1, action, value), true);
    store.receive(state(2, patch));
    assert.equal(store.isPending(1), true);
    assert.equal(store.receive(ack(1, true, 2)), true);
    assert.equal(store.isPending(1), false);
    assert.equal(store.value(store.getControl(1), action), value);
  });
}

test('an early matching value cannot bypass a later normalized revision', () => {
  const { store, state, ack } = setup({ ...text, type: 'number', text: '240' });
  store.act(1, 'text', '999'); store.receive(state(2, { text: '999' }));
  assert.equal(store.receive(ack(1, true, 3)), true);
  assert.equal(store.isPending(1), true);
  assert.equal(store.value(store.getControl(1), 'text'), '999');
  store.receive(state(3, { text: '500' }));
  assert.equal(store.isPending(1), false);
  assert.equal(store.value(store.getControl(1), 'text'), '500');
});

test('ACK before state still waits for the required native revision', () => {
  const { store, state, ack } = setup();
  store.act(1, 'toggle', true); store.receive(ack(1, true, 3));
  store.receive(state(2, { checked: true })); assert.equal(store.isPending(1), true);
  store.receive(state(3, { checked: false })); assert.equal(store.isPending(1), false);
  assert.equal(store.value(store.getControl(1), 'toggle'), false, 'the certified normalized state wins');
});

test('older receipts cannot settle a later request after a matching intermediate state', () => {
  const { store, state, ack } = setup(text);
  store.act(1, 'text', 'first'); store.act(1, 'text', 'second');
  store.receive(state(2, { text: 'second' }));
  assert.equal(store.receive(ack(1, false)), false);
  assert.equal(store.receive(ack(1, true, 2)), false);
  assert.equal(store.isPending(1), true); assert.equal(store.error(1), '');
  assert.equal(store.receive(ack(2, true, 2)), true); assert.equal(store.isPending(1), false);
});

test('a late rejection preserves a newer local draft and reports the failed request', () => {
  const { store, state, ack } = setup(text);
  store.act(1, 'text', 'submitted'); store.beginEdit(1); store.draft(1, 'newer draft');
  store.receive(state(2, { text: 'submitted' }));
  assert.equal(store.receive(ack(1, false)), true);
  assert.equal(store.value(store.getControl(1), 'text'), 'newer draft');
  assert.equal(store.error(1), '此设置当前不可用');
});

test('a delayed success for the former edit cannot clear a new pending edit', () => {
  const { store, state, ack } = setup(text);
  store.act(1, 'text', 'first'); store.receive(state(2, { text: 'first' }));
  store.act(1, 'text', 'second'); assert.equal(store.receive(ack(1, true, 2)), false);
  assert.equal(store.isPending(1), true); assert.equal(store.value(store.getControl(1), 'text'), 'second');
  store.receive(state(3, { text: 'second' })); store.receive(ack(2, true, 3));
  assert.equal(store.isPending(1), false);
});

test('old-generation receipts still cannot affect a replacement page action', () => {
  const { store, state, ack } = setup();
  store.act(1, 'toggle', true); store.receive(state(1, {}, 2));
  store.act(1, 'toggle', true);
  assert.equal(store.receive(ack(1, false, 0, 1)), false);
  store.receive(state(2, { checked: true }, 2));
  assert.equal(store.isPending(1), true);
  assert.equal(store.receive(ack(2, true, 2, 2)), true); assert.equal(store.isPending(1), false);
});

test('button and list activation retain the receipt and revision requirements', () => {
  for (const [control, action, value] of [
    [{ id: 1, type: 'button' }, 'press', undefined],
    [{ id: 1, type: 'choice', presentation: 'list', selected: 0, options: ['unit'], readonly: true }, 'activate', 0]
  ]) {
    const { store, state, ack } = setup(control);
    assert.equal(store.act(1, action, value), true); store.receive(state(2));
    assert.equal(store.isPending(1), true); store.receive(ack(1, true, 2));
    assert.equal(store.isPending(1), false);
  }
});

test('legacy successful receipts without revision still converge on observed values', () => {
  const { store, state, ack } = setup(text);
  store.act(1, 'text', 'requested'); store.receive(state(2, { text: 'normalized' }));
  store.receive(ack(1, true)); assert.equal(store.isPending(1), false);
  assert.equal(store.value(store.getControl(1), 'text'), 'normalized');
});

for (const [name, control, value] of [
  ['text', text, 'original'],
  ['number', { ...text, type: 'number', text: '240' }, '240'],
  ['editable choice', { ...text, type: 'choice', options: ['original'], selected: 0, readonly: false }, 'original'],
  ['range', { id: 1, type: 'range', min: 0, max: 100, text: '0' }, 0]
]) {
  test(`focusing an unchanged ${name} editor does not report an unsaved change`, () => {
    const { store, messages } = setup(control);
    store.beginEdit(1); store.draft(1, value);
    const draft = store.drafts.get('0:1'), state = store.state;
    store.receive({ kind: 'close-check', protocol: 1, seq: 42 });
    assert.equal(messages.at(-1).unsubmitted, 0);
    assert.equal(store.drafts.get('0:1'), draft, 'the close inquiry is read-only');
    assert.equal(store.state, state); assert.equal(store.closed, false);
  });
}

test('a changed draft remains protected and undoing the edit removes the false close warning', () => {
  const { store, messages } = setup(text);
  store.beginEdit(1); store.draft(1, 'changed');
  store.receive({ kind: 'close-check', protocol: 1, seq: 42 });
  assert.equal(messages.at(-1).unsubmitted, 1);
  store.draft(1, 'original'); store.receive({ kind: 'close-check', protocol: 1, seq: 43 });
  assert.equal(messages.at(-1).unsubmitted, 0); assert.equal(store.drafts.size, 1);
});

test('a draft already equal to the current native value needs no separate write', () => {
  const { store, messages, state } = setup(text);
  store.beginEdit(1); store.draft(1, 'changed'); store.receive(state(2, { text: 'changed' }));
  store.receive({ kind: 'close-check', protocol: 1, seq: 42 });
  assert.equal(messages.at(-1).unsubmitted, 0);
  assert.equal(store.commit(1), false, 'the existing commit path also considers the value unchanged');
});

test('retired-page drafts remain protected even if their cached old value is identical', () => {
  const { store, messages } = setup();
  store.navigate(2, 1);
  store.receive({ kind: 'state', protocol: 1, module: 2, generation: 1, revision: 1, page: 'tactical/1', controls: [text] });
  store.beginEdit(1); store.draft(1, 'original'); store.navigate(2, 2);
  store.receive({ kind: 'state', protocol: 1, module: 2, generation: 2, revision: 1, page: 'tactical/2', controls: [{ ...text, id: 2 }] });
  store.receive({ kind: 'close-check', protocol: 1, seq: 42 });
  assert.equal(messages.at(-1).unsubmitted, 1);
  assert.equal(store.drafts.get('2:1').value, 'original');
});

test('an unacknowledged operation still prevents a clean close after its value appears', () => {
  const { store, messages, state, ack } = setup(text);
  store.act(1, 'text', 'wanted'); store.receive(state(2, { text: 'wanted' }));
  store.beginEdit(1); store.draft(1, 'wanted');
  store.receive({ kind: 'close-check', protocol: 1, seq: 42 });
  assert.equal(messages.at(-1).unsubmitted, 1, 'the unchanged focused draft must not hide the pending receipt');
  store.receive(ack(1, true, 2)); store.receive({ kind: 'close-check', protocol: 1, seq: 43 });
  assert.equal(messages.at(-1).unsubmitted, 0);
});

test('unknown draft ownership is retained and close counts stay bounded', () => {
  const { store, messages } = setup();
  for (let id = 1; id <= 10001; id++) store.drafts.set(`0:${id}`, { value: 'unknown', page: 'retired' });
  store.receive({ kind: 'close-check', protocol: 1, seq: 42 });
  assert.equal(messages.at(-1).unsubmitted, 10000); assert.equal(store.drafts.size, 10001);
});

let failures = 0;
for (const { name, run } of tests) {
  try { run(); console.log(`PASS ${name}`); }
  catch (error) { failures++; console.error(`FAIL ${name}\n${error.stack}`); }
}
console.log(JSON.stringify({ suite: 'console_receipt_order', checks: tests.length, failures, passed: failures === 0 }));
if (failures) process.exitCode = 1;
