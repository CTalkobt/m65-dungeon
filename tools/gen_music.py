#!/usr/bin/env python3
"""gen_music.py <out.txt> — the game's music, written out as tune45's text

Two tunes on all four SIDs (tune voice v on SID voice v: 1-6 SIDs 1-2,
the left; 7-12 SIDs 3-4, the right):

  title    the title theme: D minor, slow, 16 bars round and round. The
           tune itself on the left (flute, bass, harp, two pad voices, a
           drone); on the right the same a few hundredths of a second
           later and an octave apart, which thickens it (an ensemble, not
           one player) and spreads it across the two sides.

  explore  the levels: a low drone with far-off bells and a few notes now
           and then; the bass begins to walk, a melody looks about; then
           once round, a march - drums, horns, the melody high: we will
           survive - before it sinks back into the drone. Voice 12 is left
           free for sound effects.

A voice is a list of (note, ticks) - 'c4', 'r' a rest, '~' a tie - and
('@name', 0) instrument changes; the helpers below build them from short
phrases and chords, delay them, move them by octaves, and cut or pad each
voice to the tune's length.
"""
import sys

# ---------------------------------------------------------------- notes

NAMES = ["c", "c#", "d", "eb", "e", "f", "f#", "g", "g#", "a", "bb", "b"]
ALIAS = {"db": "c#", "d#": "eb", "gb": "f#", "ab": "g#", "a#": "bb"}


def parse(n):
    """'c#4' -> MIDI number"""
    k = len(n) - 1
    while n[k - 1].isdigit() or n[k - 1] == "-":
        k -= 1
    name, octv = n[:k], int(n[k:])
    name = ALIAS.get(name, name)
    return NAMES.index(name) + (octv + 1) * 12


def name(m):
    return "%s%d" % (NAMES[m % 12], m // 12 - 1)


def shift(v, semis):
    """a voice moved by semitones"""
    out = []
    for n, t in v:
        if n[0] in "@r~":
            out.append((n, t))
        else:
            out.append((name(parse(n) + semis), t))
    return out


def length(v):
    return sum(t for n, t in v)


def fit(v, total):
    """cut (the last events shortened) or pad with a rest to total ticks"""
    out, at = [], 0
    for n, t in v:
        if at >= total:
            break
        if at + t > total:
            t = total - at
        if t or n[0] == "@":
            out.append((n, t))
        at += t
    if at < total:
        out.append(("r", total - at))
    return out


def delay(v, d, total):
    """the voice d ticks later (the first instrument kept first)"""
    head = [e for e in v[:1] if e[0][0] == "@"]
    rest = v[len(head):]
    return fit(head + [("r", d)] + rest, total)


def notes(spec, unit):
    """'d5:3 e5:1 f5:2' (lengths in units) -> events"""
    out = []
    for tok in spec.split():
        n, k = tok.split(":")
        out.append((n, int(float(k) * unit)))
    return out


def held(chords, unit_bar, pick, octv):
    """a pad: each chord's tone pick (0 root, 1 third, 2 fifth) held a bar"""
    out = []
    for c in chords:
        out.append((name(chord(c)[pick] + 12 * (octv - 4)), unit_bar))
    return out


def arpeggio(chords, unit, pattern, octv=4):
    """the chord's tones in a pattern (indices into root, third, fifth, root
    above), a note a unit, a bar each"""
    out = []
    for c in chords:
        t = chord(c)
        tones = [t[0], t[1], t[2], t[0] + 12]
        for p in pattern:
            out.append((name(tones[p] + 12 * (octv - 4)), unit))
    return out


def chord(c):
    """'Dm', 'Bb', 'A' ... -> the triad's MIDI notes about octave 4"""
    minor = c.endswith("m")
    root = parse(c[:-1].lower() + "4" if minor else c.lower() + "4")
    return [root, root + (3 if minor else 4), root + 7]


def render(v):
    out = []
    for n, t in v:
        if n[0] == "@":
            out.append(n)
        else:
            out.append("%s:%d" % (n, t))
    # wrap long lines (tune45: a line ending in \ goes on)
    lines, line = [], ""
    for tok in out:
        if len(line) + len(tok) > 100:
            lines.append(line)
            line = ""
        line += tok + " "
    lines.append(line)
    return " \\\n    ".join(l.rstrip() for l in lines)


# ---------------------------------------------------------------- instruments

INSTRUMENTS = """
;          name       wave  ad sr pulse vib gap
instrument flute      tri   48 a9 0     3   0 vdelay=18
instrument flute_soft tri   58 68 0     2   0 vdelay=24
instrument reed       pulse 48 87 0280  2   0 vdelay=18 pwm=4
instrument bass       pulse 2a 86 0380  0   1 pwm=6
instrument bass_soft  pulse 3a 65 0300  0   1 pwm=4
instrument harp       pulse 09 00 0700  0   0
instrument harp_soft  pulse 09 00 0500  0   0
instrument pad        saw   b9 a9 0     0   0
instrument pad_soft   saw   c9 69 0     0   0
instrument drone      tri   c0 f9 0     0   0
instrument bell       pulse 0b 00 0200  0   0
instrument brass      saw   38 b6 0     2   0 vdelay=10
instrument kick       pulse 08 00 0800  0   0 slide=-47
instrument snare      noise 0a 00 0     0   0
"""

# ---------------------------------------------------------------- the title

def title():
    unit = 5                                     # ticks an eighth (a tick 4 frames)
    bar = 8 * unit
    chords = ["Dm", "Dm", "Bb", "C", "Dm", "Gm", "A", "A",
              "Dm", "F", "C", "Dm", "Bb", "Gm", "A", "Dm"]
    total = bar * len(chords)
    lead = [("@flute", 0)] + notes(
        "d5:3 e5:1 f5:2 a5:2 g5:2 f5:2 e5:2 d5:2 f5:4 d5:2 bb4:2 c5:4 e5:2 g5:2 "
        "a5:3 g5:1 f5:2 e5:2 d5:2 bb4:2 d5:2 g5:2 e5:4 c#5:2 e5:2 a4:8 "
        "d5:3 e5:1 f5:2 a5:2 c6:4 a5:2 f5:2 g5:3 f5:1 e5:2 c5:2 d5:2 f5:2 a5:4 "
        "bb5:3 a5:1 g5:2 f5:2 g5:2 f5:2 e5:2 d5:2 c#5:4 e5:2 a4:2 d5:8", unit)
    bass = [("@bass", 0)] + [(name(chord(c)[k] - 24 + (0 if k == 0 else -12)), 4 * unit)
                             for c in chords for k in (0, 2)]
    harp = [("@harp", 0)] + arpeggio(chords, unit, [0, 1, 2, 1, 0, 1, 2, 3])
    pad3 = [("@pad", 0)] + held(chords, bar, 1, 4)
    pad5 = [("@pad", 0)] + held(chords, bar, 2, 4)
    drone = [("@drone", 0)] + [(name(chord(c)[0] - 24), bar) for c in chords]
    left = [lead, bass, harp, pad3, drone, shift([("@flute_soft", 0)] + lead[1:], -12)]
    right = [
        delay(shift([("@reed", 0)] + lead[1:], 0), 1, total),          # the lead again, a reed, just behind
        delay(shift([("@bass_soft", 0)] + bass[1:], 12), 1, total),    # the bass an octave up
        delay(shift([("@harp_soft", 0)] + harp[1:], 12), 1, total),    # the harp an octave up: a shimmer
        delay(pad5, 2, total),
        delay(shift([("@flute_soft", 0)] + lead[1:], 12), 3, total),   # an echo of the lead, high
        delay(shift(drone, 7), 2, total),                              # the drone's fifth
    ]
    voices = [fit(v, total) for v in left] + [fit(v, total) for v in right]
    return "tune title 4 loop", voices


# ---------------------------------------------------------------- the levels

def explore():
    unit = 4                                     # ticks an eighth (a tick 5 frames)
    bar = 8 * unit
    # A: the drone, far-off bells (16 bars)
    a_ch = ["Dm", "Dm", "C", "C", "Bb", "Bb", "A", "A", "Dm", "Dm", "Gm", "Gm", "Bb", "Bb", "A", "A"]
    # B: the bass walks, a melody looks about (16 bars)
    b_ch = ["Dm", "C", "Bb", "A", "Dm", "F", "Gm", "A", "Dm", "C", "Bb", "C", "Dm", "Bb", "Gm", "A"]
    # C: the march (8 bars)
    c_ch = ["Dm", "Bb", "F", "C", "Dm", "Bb", "C", "A"]
    # D: back into the dark (8 bars)
    d_ch = ["Dm", "Dm", "Bb", "Bb", "Gm", "Gm", "A", "A"]
    chords = a_ch + b_ch + c_ch + d_ch
    total = bar * len(chords)
    na, nb, nc, nd = (len(x) * bar for x in (a_ch, b_ch, c_ch, d_ch))

    drone = [("@drone", 0)] + [(name(chord(c)[0] - 24), bar) for c in chords]
    fifth = [("@drone", 0)] + [(name(chord(c)[2] - 24), bar) for c in chords]
    pad = [("@pad_soft", 0)] + held(chords, bar, 1, 4)
    pad2 = [("@pad_soft", 0)] + held(chords, bar, 2, 4)

    # the bass: silent, then walking quarters, driving eighths, long notes
    bass = [("@bass_soft", 0), ("r", na)]
    for c in b_ch:
        t = chord(c)
        bass += [(name(n - 24), 2 * unit) for n in (t[0], t[2] - 12, t[1], t[2] - 12)]
    bass.append(("@bass", 0))
    for c in c_ch:
        t = chord(c)
        bass += [(name(t[0] - 24), unit), (name(t[0] - 12), unit)] * 4
    bass.append(("@bass_soft", 0))
    bass += [(name(chord(c)[0] - 24), bar) for c in d_ch]

    # the melody: a few notes in A, a looking-about line in B (D dorian:
    # the B natural), the march's tune in C, a last sigh in D
    mel = [("@flute_soft", 0)]
    mel += notes("r:8 r:8 r:4 d4:2 f4:2 e4:8 r:8 r:8 r:8 r:8 r:4 a3:2 c4:2 d4:8 "
                 "r:8 r:8 r:4 f4:2 g4:2 a4:8 r:8 r:8 r:8 r:8 r:4 e4:2 c#4:2", unit)
    mel = fit(mel, na)
    mel += [("@flute", 0)] + notes(
        "a4:3 b4:1 c5:2 d5:2 e5:4 d5:4 c5:2 b4:2 a4:4 g4:4 e4:4 f4:4 g4:2 a4:6 "
        "r:4 f4:2 a4:2 c5:4 a4:4 bb4:3 a4:1 g4:4 e4:8 "
        "d5:3 c5:1 b4:2 a4:2 g4:4 e4:4 f4:2 g4:2 a4:4 c5:4 g4:4 "
        "f4:3 e4:1 d4:2 f4:2 a4:4 f4:4 g4:4 bb4:4 e4:8", unit)
    mel = fit(mel, na + nb)
    mel += [("@brass", 0)] + notes(
        "d5:2 d5:1 e5:1 f5:2 a5:2 bb5:4 a5:2 f5:2 a5:3 g5:1 f5:2 c5:2 e5:4 g5:4 "
        "f5:2 f5:1 g5:1 a5:2 d6:2 bb5:4 a5:2 g5:2 g5:3 f5:1 e5:2 c5:2 c#5:4 e5:2 a5:2", unit)
    mel = fit(mel, na + nb + nc)
    mel += [("@flute_soft", 0)] + notes("a4:4 f4:4 d4:8 r:8 r:8 bb3:4 d4:4 g4:8 r:8 e4:4 c#4:4 a3:8", unit)
    mel = fit(mel, total)

    # bells far off in A (and D), horns in the march
    bells = [("@bell", 0)]
    pings = ["a5", "d6", "f5", "e6", "c6", "a5", "d6", "g5"]
    for k in range(len(a_ch)):
        bells += [("r", 5 * unit), (pings[k % 8], 3 * unit)] if k % 2 == 0 else [("r", bar)]
    bells.append(("r", nb))
    bells.append(("@brass", 0))
    for c in c_ch:
        t = chord(c)
        bells += [(name(t[2]), 3 * unit), (name(t[2]), unit), (name(t[1] + 12), 4 * unit)]
    bells.append(("@bell", 0))
    for k in range(len(d_ch)):
        bells += [("r", 6 * unit), (pings[(k * 3) % 8], 2 * unit)] if k % 4 == 1 else [("r", bar)]

    # the harp: from B on, softly, a bar of picking every other bar
    harp = [("@harp_soft", 0), ("r", na)]
    for k, c in enumerate(b_ch):
        harp += arpeggio([c], unit, [0, 2, 3, 2, 1, 2, 3, 2], 4) if k % 2 == 0 else [("r", bar)]
    harp.append(("@harp", 0))
    harp += arpeggio(c_ch, unit, [0, 1, 2, 3, 2, 1, 2, 1], 4)
    harp.append(("@harp_soft", 0))
    for k, c in enumerate(d_ch):
        harp += arpeggio([c], unit, [0, 2, 3, 2, 1, 2, 3, 2], 4) if k < 2 else [("r", bar)]

    # drums: only in the march - kick on 1 and 3, snare on 2 and 4, a roll in
    drums = [("r", na + nb - 2 * unit), ("@snare", 0), ("c6", unit // 2), ("c6", unit // 2),
             ("c6", unit // 2), ("c6", unit // 2)]
    for k in range(len(c_ch)):
        for beat in range(4):
            if beat % 2 == 0:
                drums += [("@kick", 0), ("c3", unit), ("r", unit)]
            else:
                drums += [("@snare", 0), ("c6", unit), ("r", unit)]
    drums += [("r", nd)]

    left = [drone, bass, pad, mel, harp, drums]
    right = [
        fifth,
        pad2,
        delay(shift([("@flute_soft", 0)] + mel[1:], -12), 2, total),   # the melody's shadow, low
        bells,
        delay(shift([("@harp_soft", 0)] + harp[1:], 12), 1, total),    # the harp's shimmer
    ]
    voices = [fit(v, total) for v in left] + [fit(v, total) for v in right]
    return "tune explore 5 loop", voices


def main():
    out = ["; music.txt — made by tools/gen_music.py: the game's music for tune45",
           "; (do not edit: edit gen_music.py)", INSTRUMENTS]
    for head, voices in (title(), explore()):
        out.append(head)
        for k, v in enumerate(voices):
            out.append("v%d: %s" % (k + 1, render(v)))
        out.append("")
    with open(sys.argv[1], "w") as f:
        f.write("\n".join(out))


if __name__ == "__main__":
    main()
