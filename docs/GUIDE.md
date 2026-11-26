# SLOOP 3: the guide

> **SLOOP 3 is a public beta.** It was rewritten from scratch: a new sound engine, a new sequencer and a new
> interface. It is being tested on real FM-1s now. Expect bugs, keep backups, and please
> [report what you find](https://github.com/isod89/sloop3-fm1/issues). Prefer the tried and tested version?
> SLOOP 2.8 stays at [github.com/isod89/sloop-fm1](https://github.com/isod89/sloop-fm1).

SLOOP 3 plays like SLOOP 2.8.5: the same buttons, the same layers, the same sounds. This guide says how to install
it, what it takes over from SLOOP 2.8, and what is new or has moved. The screen is in English.

## Installing

Open **[isod89.github.io/sloop3-fm1](https://isod89.github.io/sloop3-fm1/)** in Chrome or Edge (or the
`installer-sloop3-*.html` of a [release](https://github.com/isod89/sloop3-fm1/releases), opened from your disk). It
is a guided page, in English or French, that takes you through five steps, one at a time:

1. **Before you start.** What a beta means, and what you need: the FM-1, a USB data cable, Chrome or Edge.
2. **The safety net: m-bootloader, once.** SLOOP 3 is installed only through
   [m-bootloader](https://github.com/jvitkauskas/m-bootloader), the open-source bootloader for the FM-1 by Julius
   Vitkauskas, for your safety: it checks the firmware before starting it, and if an install is interrupted or a
   firmware is damaged, the FM-1 opens a recovery menu instead of hanging. To see whether it is there: switch the
   FM-1 on holding **PLAY**; a menu with BOOT APP, DOWNLOAD MODE and MIDI MODE means it is. If not, the recommended
   way is [M-Flasher](https://github.com/jvitkauskas/m-flasher) (Windows, macOS, Linux):
   - connect the FM-1 and put it in USB download mode (M-Flasher's DOWNLOAD MODE button; with SLOOP 2.8 running,
     OCT- and OCT+ held 5 seconds; with a SLOOP 3 beta, OCT- held at power-on, then REC);
   - **back up** the whole flash;
   - install `fm1-uboot.boot` from the
     [latest m-bootloader release](https://github.com/jvitkauskas/m-bootloader/releases/latest).

   Only the boot area is written. An FM-1 still on M-VAVE's firmware can also take `fm1-uboot.fwsc` in M-VAVE's
   M-UPGRADE app.
3. **Connect.** Switch the FM-1 off, then on holding **PLAY**: m-bootloader's menu. Choose **MIDI MODE**, connect
   USB and click **Connect**. The page finds the FM-1 by itself as soon as it appears, and shows its m-bootloader
   version and chip key.
4. **Back up.** The whole flash, 1 MB, as a file in your downloads (`fm1-backup-….bin`). Nothing is written. Already
   have a full backup from M-Flasher? Tick the box and go on.
5. **Install SLOOP 3.** About a minute: the FM-1 checks the whole package, then writes it. It restarts on SLOOP 3.
   Turn MASTER up gently.

Nothing is erased until the whole package has been read and checked. If an install stops while writing, the FM-1
opens m-bootloader's menu by itself at the next start: install again.

## The first start: what comes over from SLOOP 2.8

SLOOP 3 never writes SLOOP 2.8's area: it reads it. At the first start:

- your **sections A–D** become **your project**, patterns 1–4: notes, ties, nudges, conditions, ratchets, **locks**
  (of the sound and of the mix), each section's sound (the pattern keeps its own), the arpeggiators, the kit and the
  TUNE / DECAY of each drum sound, the mix, the delay, the reverb, the chorus, DUST / DUCK / FILT, TEXTURE, and
  **your song** (SAVE on the main screen, then SELECT: SONG 2/3);
- your **32 user presets** become your sounds (SAVE > USER, and SOUNDS > YOURS), your **favourites** the FAVS category
  of SOUNDS;
- your **samples USR1–4**, your **SYN1–4 kits**, your **FM6 bank**: read where they are;
- your settings: menu, lights, calibration, MIDI LEARN, MIDI settings.

**Updating from SLOOP 3.0–3.3.4: one project now.** From 3.3.5 the FM-1 keeps one project, guaranteed to fit at
its fullest (5 tracks × 8 patterns of 128 steps, each with its own sound, its notes and locks, the song). At the first
start of 3.3.5, the newest of your 8 places becomes that project (ONE PROJECT NOW: YOUR PROJECT n); the others are
not read again. **Before installing, back up everything with the editor (Projects, BACK UP EVERYTHING)**: your other
projects stay in that file, and the editor puts any of them back on the FM-1 (RESTORE, or ⇧ file).

**Going back to SLOOP 2.8.5:** restore the backup you made before installing SLOOP 3, with M-Flasher. Everything of
SLOOP 2.8 is still there.

## What is new or has moved

**3.4.0: SLOOP 2.8.5's pages and gestures.** From 3.4.0, SLOOP 3 has SLOOP 2.8.5's pages, layers and workflow, with
SLOOP 3's lettering, its SOUNDS and FAVS, the NOTES and DRUMS screens, the MIX page (GLO held) and its 5 tracks,
8 patterns a track and 128 steps. The table at the end of this section lists where everything is. **A project saved by
3.4.0 can't be read by 3.3.x**: back up everything with the editor before installing (Projects, BACK UP EVERYTHING).

**Saving as in SLOOP 2.8.** What plays can be changed freely: the autosave keeps it, and it comes back at power-on.
The 8 patterns only change when you save them: a pattern you leave for another, or play again (SAVE held + its key),
comes back as it was saved, with its sound. To save the project: SAVE on the main screen, SAVE again (PROJECT 3/3),
KNOB 1; or SAVE held + a pattern's key held 1.5 s; or HOME held + white key 5.

**Recording (3.3.7).** A new project records **on the steps** (QUANT ON), as SLOOP 2.8 did. To keep your notes exactly
where you played them: REC armed while stopped (REC READY), KNOB 4 **QUANT OFF**. Recording over a loop adds the notes
you play; **a note played again where it already is takes its place**, so a chord played on every loop is written
once. Changing every track's pattern (or a chain, or the song) starts **all the tracks together**, so tracks of the
same length never drift apart.

**The free take (3.3.8).** On an empty project, REC then just play (REC READY · PLAY FREELY): the take starts on your
first note. Press REC on the "1" after your last bar: SLOOP makes a loop of 1, 2 or 4 bars at the tempo you played (the one set, if within 3 %).
The loop's length comes from your notes, so pressing REC a little late (or early) no longer moves the last hits a
step early or leaves a hole at the end; a "1" played again just before REC is kept once.

**Messages come in two kinds:**

- **What just changed** (a mode, a value, a sound's name, a hint: KEY E MIN, PATTERN 3 ON THE BAR, TAKE 3.4s…) is
  written in the page's title line, underlined in yellow, for 1.5 s. Nothing is hidden: on TRACKS the tempo stays; on
  a layer, the title stays.
- **What matters** (a save you asked for, a confirmation such as AGAIN TO ERASE, a project or a bank received from
  the editor) shows in the window at the top, green frame; **an error**, red frame (SAVE FAILED,
  CPU OVERLOAD: VOICES CUT, CORE 2: FAILED…).

The autosave opens no window: PROJECT SAVED (AUTO) is written in the title line.
While it plays, a save never stops the sound. In the rare case where the flash would first have to be erased (a dozen
saves in a row without stopping), it says **SAVED WHEN YOU STOP** and is written as soon as you press STOP.

### SOUNDS: choosing a sound

Every sound sorted by category, 16 a page, **each sound on a white key** (the 16 tiles are the 16 white keys, in the
same order). The track's sound is in white; a yellow dot marks a favourite.

The categories: **FAVS** (your 16 favourites), BASS, KEYS, ORGAN, PAD, LEAD, PLUCK, STAB, FX, **YOURS** (your 32
places), **DX7** (the voices of your FM6 bank, by name; only shown when you have a bank). On the drum track: KIT.

Two ways there:

- **Turn PRESETS**: the sound changes as before, and SOUNDS shows where you are. The keys play as usual. The page
  closes by itself 4 seconds after the last turn. **EDIT** tapped while it shows: it stays, and the keys choose.
- **Tap EDIT up to SOUNDS (its last page)**: there, **a white key takes its tile's sound and plays it**. The white keys that
  hold a sound glow faintly, the one with the track's sound is lit. The black keys play the track's sound without
  changing anything.

| On SOUNDS | |
|---|---|
| KNOB 1 (CAT) | the category |
| KNOB 2 (PAGE) or SELECT | the page; SELECT goes on into the next category |
| PRESETS | the next or previous sound |
| KNOB 4 (FAV) | right: the track's sound becomes a favourite (the first free place); left: it no longer is |
| KNOB 3 (MOVE) | on FAVS: moves the favourite one place |

**A favourite at once:** EDIT up to SOUNDS, KNOB 1 on FAVS (the first category), then its white key. Favourites are
written to the flash a second after the last change (FAVOURITES KEPT).

Elsewhere (the steps page, NOTES, a layer held, while recording), PRESETS changes the sound without opening SOUNDS
and says its name in a message, as before.

### DX7 voices (the FM6 bank)

In the web editor, **FM6 bank** tab: **Load DX7 voices (.syx)**. If the bank is empty, the file's voices go into
B1–B27 by themselves (a DX7 file holds 32, the bank 27). Then **Save the bank to the FM-1**. On the FM-1 they are in
SOUNDS, category **DX7**, by name, and become favourites like the other sounds.

**On the FM-1's speaker: HOME held, SELECT to AUDIO, SPEAKER LOWCUT ON.** The small speaker can't play the lows (the body
of an 808 kick is around 50 Hz): without this setting the drums sound very weak on it. SPEAKER cuts what it can't play
and brings the drums' lows back as harmonics it can play, about 9 dB more drums on an 808 kit. On headphones or
speakers, leave it OFF: it cuts the lows on the jack too (USB audio always keeps them). It is kept with the
settings; it is SLOOP 2.8's SPEAKER LOWCUT, taken over at the first start, and when it is on the FM-1 says so at
power-on (SPEAKER LOWCUT ON: HOME > AUDIO).

| Where | What |
|---|---|
| ALGORITHM / PRESETS / SELECT | the track (it stops at track 1 and track 5) / its sound (**PRESETS opens SOUNDS**, above) / the next or previous page of the button you tapped (it stops at the ends) |
| The main screen (TRACKS) | KNOB 1 **SWING**, KNOB 2 the track's **LEVEL**, KNOB 3 its **STEPS**, KNOB 4 its **PAN**. Top right: the playing pattern's number in a square, in the track's colour (yellow while it waits for the bar), the battery under it |
| 5 tracks | the synths on 1–4 (the 4th, pink, is one more synth), the drums on 5. In MIDI: the synths on channels 1–4, the drums on 5 or 10 (out: 10) |
| EDIT tapped | EDIT 1, EDIT 2 (the engine's values), FILTER (ANALOG), VOICE (VCE GLD GLMOD PRIO), VOICE 2 (TRIM DTUNE PAN MUTE), SOUNDS. The drums: the DRUMS grid, KIT, SOUND |
| FX tapped | FX (DST CHO DLY REV), FILTER, SLICER (its 16 steps drawn, the one playing marked), INSERT, INSERT 2, DLY, REV · CHO |
| LFO, ENV tapped | LFO, LFO 2, LFO DEST · ENV, ENV DEST |
| SEQ tapped | NOTES (the full-screen piano roll), PATTERN (LEN DIV SWG GATE), SONG. The drums: the DRUMS grid, KIT, SOUND (each sound's TUNE, DEC and LEVEL, from -12 to +6 dB; the kit's level: MIX), PATTERN, SONG |
| SEL tapped / held | SEL (ROOT SCL QNT CHORD), SEL 2 (TRN STRUM VLEAD; STRUM in ms, below 0 from the highest note down) / CHORD, SCALE, KEYS, TRANSPOSE; a key sets the key |
| ARP tapped / held | ARP, ARP 2 / the roll: RATE |
| GLO tapped | GLOBAL (BPM SWING CLICK), MASTER, SYSTEM, DRUMS, TEXTURE, TEXTURE 2, MIDI, MIDI 2, REC (TRACK: STOP or KEEP when you change track while recording; START: NOTE or CLICK); GLO + F#3: MIDI LEARN (CC 1, the mod wheel, and CC 11 can be learnt too; the keys' pressure acts as CC 1; CC 11 alone is the track's level) |
| GLO held: MIX | one column a track: keys 1–5 mute, 6–10 solo, then FILL, BAR, TAP 16 and the tempo; ALGORITHM the track, KNOB 1–4 its LVL, PAN, DLY, REV |
| SAVE tapped, on the main screen | **SONG 1/3** (the patterns on the keys), **SONG 2/3** (the song line by line), **PROJECT 3/3**: KNOB 1 **SAVE**, KNOB 2 **NEW**, KNOB 3 **LOAD** (the saved project); NEW and LOAD ask for one more turn when something would be lost. SELECT goes from one page to the next, SAVE from SONG to PROJECT and back, HOME leaves |
| SAVE tapped, on a sound's page | PRESETS, USER (SLOT LOAD ERASE SAVE: your 32 sounds), PATTERNS (PAT ERASE LOAD SAVE), TOOLS (CLRSQ INIT NEW) |
| SAVE held | 1–8 **tapped**: that pattern, as it was saved, on the bar (stopped: at once); the one playing comes back as saved; several tapped without letting go of SAVE: a **chain**. **Held 1.5 s** (its tile fills): what plays is saved into that pattern, and the project with it; a pattern already used asks once more (AGAIN: SAVE INTO n). A big green **✓ SAVED** says it is done. 13 LOOP / SONG (on the SONG pages: from the LINE shown, then round to line 1), 14 SONG REC, 15 this track / all, 16 the SONG screen |
| HOME | **tapped**: the main screen (there: the visualiser). **Held 0.7 s: the menu** (SCREEN, LIGHTS, AUDIO, SEQ, SYSTEM), which stays open when you let go; SELECT the sections, KNOB 1–4 their settings, HOME closes it. SYSTEM, KNOB 4: the **VERSION**. While HOME is held, white key 1 **OPEN**, key 3 **NEW**, key 5 **SAVE** |
| FX held + a key | a punch-in effect while the key is held (27, white and black keys); KNOB FILTER, DUST, DUCK, TRK FILT; FADE shown top left; **FX held + HOME** locks the layer |
| EDIT held | a key erases its notes (on the drums: its sound); KNOB SHIFT, LENGTH, TRANSPOSE; EDIT held + a black key (drums): **MUTE** mode, the white keys mute their sound (not saved) |
| DRUMS grid | the **black keys** pick the sound, the **white keys** are the 16 steps of the page for that sound (silently). KNOB 1 SOUND, KNOB 2 STEP, KNOB 3 HIT, KNOB 4 LEVEL; OCT- / OCT+ the page; **OCT+ held** the hit's sheet: RATCHET, NUDGE, FILL, MOVE, COPY, DELETE |
| NOTES | the keys write the step; KNOB STEP NOTE LEN FORCE; PRESETS the zoom (8 16 32 64 steps); **OCT+ held** the note's sheet: SLIDE NUDGE FILL MOVE COPY DELETE |
| SEQ held | the track's SOUND / NOTE, DIV, SWING, LENGTH; a step held: that step (on the drums, KNOB 1 picks which sound of the step) |
| REC | **tapped**: REC on or off; **held 2 s**: the track erased (EDIT + OCT- brings it back) |
| Undo | EDIT held + OCT- (3 levels), OCT+ redoes |

## The start and the visualiser

At power-on the **sloop logo** draws itself in (the dial, the hull, the mast, the two sails: the mainsail in five
bands in the tracks' colours, then the word sloop), about 2.5 s. A button, a key or a knob stops it at once (and does
what it usually does). No window with the name and the version follows: the version is in the menu (HOME held, SYSTEM, KNOB 4). A window shows at the
start only when something asks for it (CORE 2: SILENT, SAFE START, SLOOP 2.8 IMPORTED).

**On the TRACKS screen, HOME tapped: the visualiser**, full screen. **SELECT** changes the style (its name shows for a
second; the last one is kept with the settings). **HOME** again, or any page button, closes it. The keys, PLAY, REC
and the layers work as usual (a layer held shows its page, then the visualiser comes back); the tempo is set with
GLO, KNOB 1 (GLOBAL). It sees the mix as if MASTER were at full. It costs the sound nothing: it is drawn in the screen's loop,
in place of the page.

| # | Style | What it shows |
|---|---|---|
| 1 | OSCILLOSCOPE | the mix's wave, standing still (caught on a rising edge), with a glow |
| 2 | SLOOP | the living logo: the mainsail's 5 bands are the tracks (they grow with their level, light up on a note), the dial the mix's level, the boat rolls on the wave |
| 3 | WIRES | a string per track, set ringing by its notes |
| 4 | VU METERS | the 5 tracks in their colours and the mix (L, R), peaks held, a dB scale |
| 5 | SPECTRUM | 40 bands from 40 Hz to 10 kHz, in the tracks' colours, with falling caps |
| 6 | LCD | the tempo on a 7-segment display, bar and beat, the 5 tracks as bars |

During a save (or a transfer from the editor), the visualiser freezes for a fraction of a second: its working memory
is the saves', lent when it is free.

## The battery

An icon in the **top right corner**, on every page: three bars for the charge left. Under 15 % the last bar blinks red:
time to plug the FM-1 in. It is worked out from the battery's voltage, so it reads a little lower while the music
plays, and it moves slowly (it is smoothed, once a second).

Plugged into a **computer**, the FM-1 is powered and charged by the USB: the bars fill in turn. Pull the cable and it
shows the level again within a second.

## SLICE: a sample in slices

Choose BREAK 16, CHOP 8, REVERSE or USR SLICE in PRESETS (category FX). Each key plays a slice, **the lowest
first** (in MIDI, note 60 is slice 1); the sequencer plays the slices in the order you write.

| Value | What it does |
|---|---|
| SRC | **BREAK** (a bar of acoustic drums at 120 BPM, built in) or **USR1–4** (the slot's first sample, or a break cut in the editor: its slices; empty: BREAK) |
| DIV | **4, 8, 16, 32** equal slices, or **AUTO**: one per hit (found for you); **EDIT**: a break cut in the editor, its own slices |
| STRT | shifts the slices along the keys |
| PTCH | faster = higher (the key doesn't change the pitch) |
| MODE | **ONE**: the slice to its end; **GATE**: while the key is held; **LOOP**: looped while it is held |
| REV | backwards |
| DCAY | a fade over the slice (127: none) |
| TONE | a gentle filter |

At 120 BPM and DIV 16, the BREAK's slices fall on the steps: playing the 16 in order gives the bar back.

**Your own breaks (3.3.6), with the web editor:** Samples, **as a BREAK (slices)**, choose the file (or ● RECORD).
Put START and END around the bars, then cut it: **AUTO** (a slice per hit, up to 8, 12 or 16), **GRID** (4, 8 or 16
equal slices) or by hand (double-click adds a cut or takes one out; drag a cut; click a slice or its number to hear it;
**SNAP 1/16** puts the cuts on the 16ths). Choose how many bars it is: the tempo is shown. **Send to the FM-1**: the
break goes into the slot, a slice per zone, and, if you leave them ticked, **on the track you choose**: a synth track
plays it with **SLICE** (its SRC that slot), or **the drum track** plays it as one of **YOUR KITS**, named by the sample (slice 1 on the
first pad, up to 16: your break is your drums, and the four synth tracks stay free). Choose another kit and come back
to it any time: on the drum track, PRESETS (or EDIT > SOUNDS) has a category **YOUR KITS** after the factory kits, your
four sample slots by their sample's name and your SYN1–4 kits; it is kept with the project; **the break as the pattern playing** (each
slice where it was: PLAY gives the break back) and **its tempo**. Then play the slices on the keys, rewrite the pattern, add ratchets, reverse a slice (REV)… On the FM-1, DIV
shows **EDIT**: the slices are yours. On the drum track (a kit of your samples) and in SLOOP 2.8, the same slot plays
its slices on the 16 pads.

## The factory samples

The SAMPLE and GRAIN engines and the sampled drum kits play SLOOP 2.8.5's CC0 sets (piano, upright bass, vibes,
horns, strings, flute, scratch and an acoustic GM kit, see `assets/CREDITS-SAMPLES.txt`). To fit the FM-1's flash
they are stored at 11 kHz (the bass), 16 kHz (the tonal sets and the drums), 24 kHz (the cymbals and hats) and
22 kHz (the small bright percussion). Before 3.3.1, a mistake in the tool that builds the bank made many keys play
another set's sound: fixed in 3.3.1.

## Rescue

- **OCT- held at power-on: RESCUE.** PLAY starts with an empty project; your saved project stays, never overwritten
  by it (HOME held, OPEN brings it back; SAVE writes over it only when asked twice: AGAIN TO SAVE OVER IT);
  REC puts the chip in USB download mode (M-Flasher).
- **PLAY held at power-on: m-bootloader's menu** (BOOT APP, DOWNLOAD MODE, MIDI MODE to install again).
- **OCT- and OCT+ held: the calibration** of the buttons and knobs.

## The web editor

Open **[isod89.github.io/sloop3-fm1/editor.html](https://isod89.github.io/sloop3-fm1/editor.html)** (or
`editor-sloop3.html` from a release) in Chrome or Edge, connect the FM-1 running SLOOP 3 by USB, **Connect**. The
tabs on the left, FR / EN, **view: essentials / all**, **full screen** at the top; a **?** on each card explains it.

- **LIVE**: the FM-1 played from here, like SLOOP 2.8.5's editor LIVE.
  - **Transport**: PLAY / STOP, REC, FILL (held) and a one-bar FILL, BPM and SWING, TAP, SONG and SONG REC,
    OCT- / OCT+, and the FM-1's buttons held (FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ: the layers from the
    page).
  - **Patterns**: the 8 of each track (lit: it has notes; full: it plays; blinking: it comes next); ALL n: every
    track. A **chain** of patterns that follow each other.
  - **Scenes**: 8, each keeps the mix and the master; Shift-click to keep one; **MORPH** from A to B on the crossfader.
  - **Mix**: each track's level, meter, pan, filter, chorus / delay / reverb sends, mute, solo.
  - **Punch-in effects**: the 27 (STUT, REV, GATE, ECHO…) held, or **LATCH**.
  - **Keyboard**: the FM-1's 27 keys (and the computer's keyboard: Q 2 W 3 E… Z S X D C V G B); **drum pads**: play
    (loud at the top of the pad), **ROLL** (repeated at 1/8, 1/16, 1/32…), **ERASE** (the pad held erases its hits).
  - **Master**: volume (KNOB gives it back to the MASTER knob), DJ filter, DUST, DUCK, delay and reverb THROW.
  - **Set list**: your .s3p files; CUE puts them in a place and opens them at their tempo; GO plays them.
  - **Record the set**: the FM-1's USB audio recorded as a WAV.
  - **MIDI LEARN** (touch a control on the page, then move a knob of your controller), **LOCK** (hold to unlock),
    **PANIC**. Shortcuts: Space PLAY/STOP, Shift+R REC, Shift+1–8 a pattern everywhere, ← → the track, ↑ ↓ the
    octave, F fill, Esc PANIC.
- **Sound**: the 48 values of each track's sound as knobs, heard at once; the FM-1's sounds to choose from (by
  category, search); **A / B**, **COPY / PASTE**, **RANDOM** (by group, with an amount); **keep among your sounds**
  (one of your 32 places, with a name and a category).
- **Sequencer**: the piano roll (drums: the grid of the 16 sounds). Add, move, lengthen, select (Shift, box); for the
  chosen notes: velocity, length, offset, **condition** (FILL, %, 1:2…), **ratchet**, **ACCENT**, **SLIDE**; the
  velocity lane; the **lock** lane of a value; COPY / PASTE, ◀ ▶ a step, ±1 / ±12, DOUBLE, QUANTIZE, HUMANIZE, ↶ ↷;
  the track's DIV, GATE, SWING, QUANT; **MIDI files** in and out.
- **Mixer**: the whole tracks (DRIVE too), the shared delay, reverb and chorus, the master.
- **Song**: the song line by line (bar, each track's pattern, mutes), its length; play, send.
- **Library**: your 32 sounds (rename, category, move, erase, put on a track, .s3u file), your 16 **favourites**, the
  FM-1's sounds and kits.
- **Samples**: audio files into USR1–4, as an instrument, a 16-pad kit or **a BREAK cut into slices for SLICE** (see
  SLICE), **drawn, trimmed (START / END), looped (LOOP), heard, reversed, recorded** from the computer's input, saved as
  WAV; the **quality** fits long sounds (about 7 s a slot at 22 kHz, 10 s at 16 kHz, 14 s at 11 kHz); where SLOOP 2.8
  keeps them: it reads them too (keep 22 kHz for it).
- **Drum kit**: the kits; each pad's TUNE and DECAY. **Drum synth**: your SYN1–4 kits, every value of the 16 sounds.
  **FM6 bank**: B1–B27, DX7 voices (.syx) placed at once in an empty bank, every value, saved as .syx.
- **Projects**: the project saved on the FM-1 (file out, file in, open), the project as it is, **BACK UP EVERYTHING**
  (the project and your sounds in a .s3b file) and **RESTORE** (a backup from before 3.3.5, with several projects:
  you choose the one that goes on the FM-1). Keep your other projects as files; the **set list** (LIVE) plays them
  one after the other.
- **Settings**: everything in HOME + SELECT and the MIDI pages (COLOR, ACCEL, LIGHTS, KEYS, NOTES, SPEAKER, USB, RATE,
  RESO, PREVIEW, CORE 2; MIDI IN, SYNC, OUT, CLOCK OUT, DRUM MAP, REMOTE, FX, BEND; the REC mode, bars and count-in).
- **Theme**: your colours, with a preview, then chosen in HOME > SCREEN > COLOR (YOURS).

SLOOP 2.8's FM6 bank and SYN kits are never written: at the first save, SLOOP 3 keeps its own (after the application
area) and plays them instead. Until you save anything, they are SLOOP 2.8's.

While the editor sends something (a project, a sample, your FM6 bank or your SYN kits), the FM-1 neither saves nor
loads: it shows **EDITOR BUSY** and the autosave waits for the end. A transfer stopped halfway (page closed, cable
pulled) is dropped after 10 s (**SAMPLE: STOPPED** for a sample: the slot stays empty, send it again).
