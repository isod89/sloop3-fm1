#!/bin/sh
# The PC twin's scenarios, one after the other (each a start of the FM-1; some on the flash the one before left).
#   host/twin.sh [MONKEY_SECONDS] [SEEDS]
set -u
T=./build/twin; O=build/tw
export ASAN_OPTIONS=detect_leaks=0:handle_segv=0:allow_user_segv_handler=1:handle_sigfpe=0
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
MS=${1:-120}; SEEDS=${2:-"1 2 3"}; CUTS=${CUTS:-"11 12 13 14 15 16 17 18"}
fail=0
same() {  # the state a scenario saved, found again by the next start
  if diff -q $O/$1/end-state.txt $O/$2/boot-state.txt > /dev/null 2>&1; then printf '%-22s ok  the state saved came back\n' "$1 = $2"
  else printf '%-22s FAIL\n' "$1 = $2"; diff $O/$1/end-state.txt $O/$2/boot-state.txt | head -6; fail=1; fi
}
run() {   # name scenario flash_in seed
  mkdir -p $O/$1
  if timeout 900 $T $2 $O/$1 $3 ${4:-1} > $O/$1/log.txt 2>&1; then r=0; else r=$?; fi
  printf '%-22s %s  %s\n' "$1" "$([ $r = 0 ] && echo ok || echo "FAIL ($r)")" "$(grep '^TWIN:' $O/$1/log.txt | tail -1 | cut -c7-)"
  [ $r = 0 ] || { fail=1; grep -E "FAIL|ERROR|runtime error|TWIN: the|!! TWIN" $O/$1/log.txt | head -8; }
}
run boot boot -
TWIN_DRAW_US=1500 run boot-slow boot -     # each tile drawn costing the CPU 1.5 ms, as on the FM-1 (3.1.6: the start hung)
TWIN_DRAW_US=1500 run vis-slow vis -
run reboot reboot $O/boot/flash.bin
same boot reboot
run pages pages -
run record record -
run drums285 drums285 -                  # 3.3.8: SLOOP 2.8.5's drum gestures, the hits lit, HOME held: the menu
run savestutter savestutter -            # 3.3.8: a save while it plays: no half with the flash off
run punchsnd punchsnd -                  # 4.0.9, issue #6: FX + 1/4 on the sound browsers, at 40 BPM too
run midiimp midiimp -                    # 3.4.0, Sam: notes from the editor (a MIDI file) heard, each track alone
run arpall arpall -                      # 3.4.2, issue #22: ARP on every synth track, stopped and playing
run chordrec chordrec -                  # 3.4.2, issues #28, #30: a SEL chord recorded whole, STRUM both ways
run songeng songeng -                    # 3.4.2, issue #24: a song changing a track's engine: step 1 heard each bar
run midi2 midi2 -                        # 3.4.2, Sam: every track's channel (CCs, bend, pedal, programs), MIDI LEARN of CC 1 / 11, the pressure, notes out, REMOTE, FX channel
TWIN_BYTE_NS=500 TWIN_HALF_US=3000 TWIN_DRAW_US=1500 run savecrackle savecrackle -   # 3.4.3, Sam: no half late (a crackle) saving, opening, NEW, the pages, playing; a save that must erase waits for the stop
TWIN_EV_NS=80 TWIN_BYTE_NS=500 TWIN_HALF_US=4300 TWIN_DRAW_US=1000 run pagecost pagecost -   # 3.4.4, Sam: each page of a full project, playing a heavy sound: no half late (3.4.3: 37, the drums' GRID 10)
TWIN_SECS=30 TWIN_HALF_US=4300 run punchbig punchbig -   # 3.4.4, Sam: "a restart with PUNCH on a big track": the punch-ins as fast as hands go on the fullest project, a heavy sound
run savepat savepat -                    # 3.4.0, Sam: patterns saved while it plays, the autosave with PLAY: no erase, no cut
TWIN_ERASE_MS=300 run oversave oversave -   # 3.4.1: CPU OVERLOAD during a slow erase: said once the window is back (3.4.0 froze)
run recsave recsave -                    # 3.4.1 (the monkey, seed 6): REC armed, SAVE > PROJECT still saves
run selwalk selwalk -                    # 4.0.8: every button's pages, SELECT right and left
run saveproj saveproj -                  # 4.0.4: SAVE > PROJECT (SAVE, NEW)
run sections sections -                  # 4.0: SLOOP 2.8's sections: a pattern changes only when saved
run freetake freetake -                  # 3.3.8: the free take: REC pressed late on the "1", every hit on its step
run record-reboot reboot $O/record/flash.bin
same record record-reboot
run settings settings $O/boot/flash.bin
run settings-reboot reboot $O/settings/flash.bin
same settings settings-reboot
run sounds sounds -                        # 3.2: a DX7 bank's voices by name, SOUNDS, the favourites, the lights
run sounds-reboot reboot $O/sounds/flash.bin
same sounds sounds-reboot
run ukit ukit -                            # 3.3.6: YOUR KITS: a slot as the drums' kit, by its name, found again, kept
run ukit-reboot reboot $O/ukit/flash.bin
same ukit ukit-reboot
run calib calib -
run calib-reboot reboot $O/calib/flash.bin
same calib calib-reboot
run rescue rescue $O/boot/flash.bin
run play play -
run play-reboot reboot $O/play/flash.bin
same play play-reboot
run sample sample -
run sample-reboot reboot $O/sample/flash.bin
run editor editor -
run editor-reboot reboot $O/editor/flash.bin
same editor editor-reboot
run midi midi -
run editor2 editor2 -
run editor2-reboot reboot $O/editor2/flash.bin
same editor2 editor2-reboot
run fx fx -
run banksave banksave -                    # 3.1.8: a save under every factory sound (nothing reads the window)
run vis vis -
TWIN_SECS=60 run punch punch -             # 3.3: the punch-ins chained, FX held or locked (Sam: GATE, ECHO, 1/16 froze it)
TWIN_SECS=60 TWIN_LOCK=1 TWIN_DRAW_US=1500 run punch-lock punch -
# a SLOOP 2.8 FM-1 updated: a backup of its flash (SLOOP28_DUMP), SLOOP 3's area written over it as the installer does
if [ -n "${SLOOP28_DUMP:-}" ] && [ -f "$SLOOP28_DUMP" ]; then
  python3 -c "import sys; d=bytearray(open(sys.argv[1],'rb').read()); a=open('build/fw/fw.area','rb').read(); d[0x4000:0x4000+len(a)]=a; open(sys.argv[2],'wb').write(d)" "$SLOOP28_DUMP" $O/upgrade.bin
  run import import $O/upgrade.bin
  run import-reboot reboot $O/import/flash.bin
fi
run bigproj bigproj -                      # 3.3.5: the fullest project the sequencer holds: saved, opened, the same
run bigproj-reboot reboot $O/bigproj/flash.bin
same bigproj bigproj-reboot
for s in $SEEDS 21 22 23 24 25 26; do       # (its save cut by a power cut anywhere: the copy before stays whole)
  run bigcut-$s bigcut $O/bigproj/flash.bin $s
  run bigaftercut-$s aftercut $O/bigcut-$s/flash.bin $s
done
for s in $SEEDS $CUTS; do
  run cut-$s powercut $O/record/flash.bin $s
  run aftercut-$s aftercut $O/cut-$s/flash.bin $s
done
for s in $SEEDS; do TWIN_DENORM=1 TWIN_SECS=$MS run monkey-$s monkey - $s; run monkey-$s-reboot reboot $O/monkey-$s/flash.bin; same monkey-$s monkey-$s-reboot; done
exit $fail
