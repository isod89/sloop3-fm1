/* main: the tabs, the connection, the state polled, the top bar, the language, the views, the lock. */
const TABS = [['live', '◉', drawLive], ['sound', '♫', drawSound], ['seq', '▦', drawSeq], ['mixer', '☰', drawMixer], ['song', '≡', drawSong], ['library', '★', drawLibrary],
  ['samples', '∿', drawSamples], ['kits', '◎', drawKits], ['drumsynth', '◍', drawSyn], ['fm6', '⌁', drawFm6], ['projects', '▤', drawProjects], ['settings', '⚙', drawSettings], ['theme', '◐', drawTheme]];
const NEEDS2 = ['live', 'seq', 'mixer', 'song', 'library', 'kits', 'settings'];
let tab = 'live';
try { tab = localStorage.getItem('sloop3.tab') || 'live'; } catch (e) {}
function showTab(t) {
  tab = t; try { localStorage.setItem('sloop3.tab', t); } catch (e) {}
  for (const [k] of TABS) { $('tab_' + k).classList.toggle('on', k === t); }
  for (const b of $('tabs').children) b.classList.toggle('on', b.dataset.t === t);
  draw(t);
}
function draw(t) {
  const T2 = TABS.find(x => x[0] === t); if (!T2) return;
  if (info && !P2() && NEEDS2.includes(t)) { const P = $('tab_' + t); P.innerHTML = ''; card(P, L('tab_' + t), L('oldfw')); return; }
  T2[2]();
  if (!info) return;
  if (t === 'sound') soundLoad(); else if (t === 'seq') rollLoad(); else if (t === 'song') songRead(); else if (t === 'library' && !LIB.f) libRead();
}
function drawTabs() {
  const n = $('tabs'); n.innerHTML = '';
  for (const [k, ic] of TABS) { const b = el('button'); b.dataset.t = k; b.append(el('i', '', ic), el('span', '', L('tab_' + k))); b.onclick = () => showTab(k); if (k === tab) b.classList.add('on'); n.appendChild(b); }
}
function words2() {
  document.documentElement.lang = lang;
  for (const e of document.querySelectorAll('[data-t]')) e.textContent = L(e.dataset.t);
  $('b_lang').textContent = lang === 'fr' ? 'EN' : 'FR';
  $('b_view').textContent = document.body.classList.contains('simple') ? L('viewAll') : L('viewSimple');
  $('b_full').textContent = document.body.classList.contains('full') ? L('fullOff') : L('full');
  $('b_unlock').textContent = L('unlock');
  if (out) $('con').textContent = L('connected');
  drawTabs(); draw(tab);
}
/* the top bar from the state */
let lastMsg = -1, msgT = 0;
liveHooks.push(st => {
  $('s_play').textContent = st.rec ? '●' : st.playing ? '▶' : '■'; $('s_play').style.color = st.rec ? 'var(--bad)' : st.playing ? 'var(--ok)' : '';
  $('s_pos').textContent = (st.bar + 1) + '.' + (st.beat + 1); $('s_bpm').textContent = st.bpm.toFixed(1); $('s_cpu').textContent = st.cpu + '% · ' + st.voices + 'v';
  $('s_cpu').style.color = st.peak > 95 ? 'var(--bad)' : st.cpu > 80 ? 'var(--warn)' : '';
  if (info && st.proj !== info.proj) { info.proj = st.proj; $('s_proj').textContent = st.proj + 1; }
  if (st.msgGen !== lastMsg) { const first = lastMsg < 0; lastMsg = st.msgGen; if (!first && st.msgKind) { const m = $('devmsg'); m.textContent = st.msg; m.className = st.msgKind === 2 ? 'badm' : ''; clearTimeout(msgT); msgT = setTimeout(() => m.textContent = '', 2500); } }
});
/* polled: ~6 times a second while the page is seen and nothing is being sent */
let polling = 0;
async function poll() {
  if (out && P2() && !liveBusy && !(typeof document !== 'undefined' && document.hidden)) { try { await live(0); } catch (e) { /* again next time */ } }
  polling = setTimeout(poll, 160);
}
/* the FM-1's port: the one named "SLOOP 3", or else one whose name only looks like an FM-1 and answers INFO. macOS
 * keeps the name CoreMIDI first gave a USB device (VID:PID), and Chrome keeps it until it quits: on a Mac that has
 * seen SLOOP 2.8, SLOOP 3 (1209:0001, "FM-1 SLOOP 3" on USB) shows as "Felucca" (issue #2) */
const LOOKS = /SLOOP|Felucca|FM-?1/i, NOT_FW = /Update|ota-FM-1/i;
function answersInfo(o, i, ms = 800) {
  return new Promise(res => {
    let t = 0;
    const h = e => { const d = e.data; if (d.length > 5 && d[0] === 0xF0 && d[1] === 0x7D && d[2] === 0x53 && d[3] === 0x33 && d[4] === ED.INFO) done(true); };
    const done = ok => { clearTimeout(t); i.removeEventListener('midimessage', h); res(ok); };
    t = setTimeout(() => done(false), ms);
    Promise.resolve(i.open && i.open()).then(() => { i.addEventListener('midimessage', h); o.send(frame(ED.INFO, [])); }, () => done(false));
  });
}
async function findPort(midi) {
  if ([...midi.outputs.values()].some(p => /SLOOP 3/i.test(p.name))) return p => /SLOOP 3/i.test(p.name);
  const ins = [...midi.inputs.values()];
  for (const o of midi.outputs.values()) {
    if (!LOOKS.test(o.name) || NOT_FW.test(o.name) || o.state === 'disconnected') continue;
    const i = ins.find(p => p.name === o.name);
    if (i && await answersInfo(o, i)) return p => p.name === o.name;
  }
  return () => false;
}
async function connect() {
  try {
    if (!navigator.requestMIDIAccess) { log(L('nomidi'), 'bad'); return; }
    const midi = await navigator.requestMIDIAccess({ sysex: true });
    const is = await findPort(midi);
    const find = () => { out = [...midi.outputs.values()].find(is) || null; inp = [...midi.inputs.values()].find(is) || null; };
    find();
    if (!out || !inp) { log(L('noport') + [...midi.outputs.values()].map(p => p.name).join(', '), 'bad'); return; }
    inp.onmidimessage = onMsg;
    for (const p of midi.inputs.values()) if (p !== inp) p.onmidimessage = ctlIn;          /* the other controllers: MIDI learn */
    midi.onstatechange = () => { const was = out; find(); if (!out && was) { log(L('timeout'), 'bad'); $('con').textContent = L('connect'); $('con').classList.add('pri'); }
      for (const p of midi.inputs.values()) if (p !== inp && !p.onmidimessage) p.onmidimessage = ctlIn; if (inp) inp.onmidimessage = onMsg; };
    await refreshInfo(); log(L('found') + info.ver + L('proto') + info.proto, 'ok');
    if (!P2()) log(L('oldfw'), 'bad');
    $('con').textContent = L('connected'); $('con').classList.remove('pri');
    if (P2()) { await readNames(); await live(0); }
    clearTimeout(polling); poll();
    draw(tab);
  } catch (err) { fail(err); }
}
if (typeof window !== 'undefined' && typeof document !== 'undefined' && document.body && document.body.classList) {
  try { if (localStorage.getItem('sloop3.view') === 'simple') document.body.classList.add('simple'); const l = localStorage.getItem('sloop3.lang'); if (l) lang = l; } catch (e) {}
  $('con').onclick = connect;
  $('b_lang').onclick = () => { lang = lang === 'fr' ? 'en' : 'fr'; try { localStorage.setItem('sloop3.lang', lang); } catch (e) {} words2(); };
  $('b_view').onclick = () => { document.body.classList.toggle('simple'); try { localStorage.setItem('sloop3.view', document.body.classList.contains('simple') ? 'simple' : 'all'); } catch (e) {} words2(); };
  $('b_full').onclick = () => { const on = !document.body.classList.contains('full'); document.body.classList.toggle('full', on);
    try { if (on && document.documentElement.requestFullscreen) document.documentElement.requestFullscreen(); else if (!on && document.fullscreenElement) document.exitFullscreen(); } catch (e) {} words2(); };
  document.addEventListener('fullscreenchange', () => { if (!document.fullscreenElement && document.body.classList.contains('full')) { document.body.classList.remove('full'); words2(); } });
  let ut = 0; const ub = $('b_unlock');
  ub.addEventListener('pointerdown', () => { ub.classList.add('on'); ut = setTimeout(() => { $('locked').classList.remove('on'); ub.classList.remove('on'); }, 900); });
  for (const e of ['pointerup', 'pointerleave', 'pointercancel']) ub.addEventListener(e, () => { clearTimeout(ut); ub.classList.remove('on'); });
  window.addEventListener('resize', () => soon('rs', () => { if (tab === 'seq') drawRoll(); if (tab === 'samples') drawWave(); }, 150));
  words2();
  showTab(TABS.some(x => x[0] === tab) ? tab : 'live');
}
