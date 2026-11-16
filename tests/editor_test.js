/* SPDX-License-Identifier: GPL-3.0-only
 * The web editor's FM6 bank and SYN kits (web/editor.html) in node, with a fake DOM: DX7 voices packed and unpacked
 * (every value back; a single-voice dump the same bytes as the packed voice), a 32-voice .syx read, and an image
 * written then read back through a fake FM-1 answering command 14 as fw/main.c does. The images (with an OUT
 * directory) for tools that check them with app/xbank.c.   node tests/editor_test.js web/editor.html [OUT] */
const fs = require('fs');
const h = fs.readFileSync(process.argv[2], 'utf8');
const js = h.slice(h.indexOf('<script>') + 8, h.lastIndexOf('</script>'));
function el() {
  const e = { style: {}, dataset: {}, children: [], value: '', disabled: false, className: '', textContent: '', files: [],
    appendChild(c) { this.children.push(c); return c; }, addEventListener() {}, append(...c) { this.children.push(...c); }, remove() {}, setAttribute() {}, getContext() { return new Proxy({}, { get: () => () => ({}) , set: () => true }); }, getBoundingClientRect() { return { left: 0, top: 0, width: 860, height: 300 }; }, setPointerCapture() {}, querySelector() { return el(); }, click() {}, set innerHTML(v) { this.children = []; }, get innerHTML() { return ''; },
    get options() { return this.children; }, get selectedIndex() { return 0; }, set selectedIndex(v) {} };
  return e;
}
const els = {}; const logs = [];
const document = { documentElement: {}, getElementById: id => els[id] || (els[id] = el()), createElement: () => el(), body: el() };
els.log = el(); els.log.appendChild = d => logs.push(d.textContent);
const navigator = { language: 'en' };
let sent = [];
const out = { send: d => sent.push(d) };
const ctx = new Function('document', 'navigator', 'out_', 'logs', js + '\n;out = out_; return { vUnpack, vPack, vFrom155, syxVoices, bankRead, bankWrite, findPort, F, Y, XB, crc32, set ask(f) { ask = f; } };')(document, navigator, out, logs);
let fails = 0; const check = (c, w) => { if (!c) { console.log('FAIL ' + w); fails++; } };
// 1. pack <-> unpack, every value in range
for (let n = 0; n < 200; n++) {
  const r = m => Math.floor(Math.random() * (m + 1));
  const v = { op: [], g: {}, name: 'TEST ' + n };
  for (let o = 0; o < 6; o++) { const x = {}; for (const [k, m] of [['R1', 99], ['R2', 99], ['R3', 99], ['R4', 99], ['L1', 99], ['L2', 99], ['L3', 99], ['L4', 99], ['BP', 99], ['LD', 99], ['RD', 99], ['LC', 3], ['RC', 3], ['RS', 7], ['AMS', 3], ['KVS', 7], ['OUT', 99], ['MODE', 1], ['CRS', 31], ['FINE', 99], ['DET', 14]]) x[k] = r(m); v.op.push(x); }
  for (const [k, m] of [['ALG', 31], ['FB', 7], ['SYNC', 1], ['TRSP', 48], ['PR1', 99], ['PR2', 99], ['PR3', 99], ['PR4', 99], ['PL1', 99], ['PL2', 99], ['PL3', 99], ['PL4', 99], ['LSPD', 99], ['LDLY', 99], ['LPMD', 99], ['LAMD', 99], ['LSYNC', 1], ['LWAVE', 5], ['PMS', 7]]) v.g[k] = r(m);
  const b = ctx.vPack(v), u = ctx.vUnpack(b);
  const same = (a, b) => Object.keys(b).every(k => a[k] === b[k]);
  if (!v.op.every((x, o) => same(u.op[o], x)) || !same(u.g, v.g) || u.name !== v.name.toUpperCase()) { check(false, 'roundtrip ' + n); break; }
  // the same voice as DX7 format 0 (155 values) -> packed: the same bytes
  const u155 = []; for (let o = 5; o >= 0; o--) for (const k of ['R1', 'R2', 'R3', 'R4', 'L1', 'L2', 'L3', 'L4', 'BP', 'LD', 'RD', 'LC', 'RC', 'RS', 'AMS', 'KVS', 'OUT', 'MODE', 'CRS', 'FINE', 'DET']) u155.push(v.op[o][k]);
  for (const k of ['PR1', 'PR2', 'PR3', 'PR4', 'PL1', 'PL2', 'PL3', 'PL4', 'ALG', 'FB', 'SYNC', 'LSPD', 'LDLY', 'LPMD', 'LAMD', 'LSYNC', 'LWAVE', 'PMS', 'TRSP']) u155.push(v.g[k]);
  for (const c of v.name.toUpperCase().padEnd(10).slice(0, 10)) u155.push(c.charCodeAt(0));
  if (Buffer.compare(Buffer.from(ctx.vFrom155(u155)), Buffer.from(b))) { check(false, 'format 0 -> packed ' + n); break; }
}
// 2. .syx files: a bank and a single voice
const bank = new Uint8Array(4104); bank.set([0xF0, 0x43, 0, 9, 0x20, 0]); for (let i = 6; i < 4102; i++) bank[i] = (i * 13) & 127; bank[4103] = 0xF7;
check(ctx.syxVoices(bank).length === 32, 'a 32-voice bank read');
// 3. the bank written as the FM-1 expects, read back: a fake FM-1 doing what fw/main.c's ED_BANK does
const flash = {}; let wimg = null;
ctx.ask = async (cmd, raw) => {
  const sub = raw[0], which = raw[1], len = which ? ctx.XB.SYN : ctx.XB.FM6;
  if (sub === 2) { wimg = new Uint8Array(len); return [2, which]; }
  if (sub === 3) { const off = raw[2] | raw[3] << 8; wimg.set(raw.slice(4), off); return [3, which]; }
  if (sub === 4) { flash[which] = wimg; return [4, which]; }
  if (sub === 1) { const off = raw[2] | raw[3] << 8, m = Math.min(384, len - off), img = flash[which]; return [1, which, off & 255, off >> 8, len & 255, len >> 8, ...img.slice(off, off + m)]; }
  if (sub === 0) return [0, 2, 0, 0, 0, 0, 2];
};
(async () => {
  const img = new Uint8Array(ctx.XB.FM6); for (let i = 16; i < img.length; i++) img[i] = (i * 31) & 127; img.set([5, 0, 0, 4], 8);
  await ctx.bankWrite(0, img);
  const back = await ctx.bankRead(0);
  check(Buffer.compare(Buffer.from(back), Buffer.from(flash[0])) === 0, 'the bank read back as written');
  if (process.argv[3]) fs.writeFileSync(process.argv[3] + '/fm6.img', flash[0]);
  const s = new Uint8Array(ctx.XB.SYN); for (let i = 16; i < s.length; i++) s[i] = (i * 7) & 127;
  await ctx.bankWrite(1, s); if (process.argv[3]) fs.writeFileSync(process.argv[3] + '/syn.img', flash[1]);
  // 4. the FM-1's port (issue #2): named "SLOOP 3", or a look-alike that answers INFO (a Mac that has seen SLOOP 2.8
  // shows SLOOP 3 as "Felucca"); a silent look-alike, MIDI MODE and other gear are passed over
  const fake = (name, answers) => {
    const ls = new Set(), sent = [];
    const i = { name, state: 'connected', open: async () => i, addEventListener: (t, f) => ls.add(f), removeEventListener: (t, f) => ls.delete(f) };
    const o = { name, state: 'connected', sent, send: d => { sent.push(d); if (answers && d[4] === 1) setTimeout(() => { for (const f of ls) f({ data: [0xF0, 0x7D, 0x53, 0x33, 1, 3, 0xF7] }); }, 5); } };
    return [o, i];
  };
  const access = ports => ({ outputs: new Map(ports.map(([o]) => [o.name, o])), inputs: new Map(ports.map(([, i]) => [i.name, i])) });
  const synth = fake('Arturia MiniLab mkII', true), silentFw = fake('FM-1', false), mbMode = fake('Felucca Update', true), stale = fake('Felucca', true);
  let is = await ctx.findPort(access([synth, silentFw, mbMode, stale]));
  check(is(stale[0]) && !is(silentFw[0]) && !is(mbMode[0]) && !is(synth[0]), 'a look-alike that answers INFO is the FM-1');
  check(synth[0].sent.length === 0 && mbMode[0].sent.length === 0, 'other gear and MIDI MODE are not asked');
  const named = fake('FM-1 SLOOP 3', false);
  is = await ctx.findPort(access([stale, named]));
  check(is(named[0]) && !is(stale[0]), 'a port named SLOOP 3 is taken as it is');
  is = await ctx.findPort(access([fake('FM-1', false), fake('Felucca', false)]));
  check(!is(fake('FM-1')[0]) && !is(fake('Felucca')[0]), 'no FM-1 when no look-alike answers');
  console.log('editor bank test: ' + fails + ' failed');
  process.exit(fails ? 1 : 0);
})();
