#!/usr/bin/env python3
"""gen_sfx.py — the sampled sounds

  gen_sfx.py pack <sound packs dir> <dir>                (make sounds)
      Reads each sound in SOUNDS straight out of the packs' zip files,
      trims it, resamples it to RATE Hz, normalises it and codes it in
      blocks of 16 samples, 4.5 bits a sample (blocks() below), into the
      files DGSFX1, 2, ... (below), and wav/: each decoded as the program
      will, to listen to. make sounds puts the files on
      assets/baseimage.d81; assets/README.md says where each comes from.

  gen_sfx.py tables <dir> <build dir>                     (every build)
      The DGSFX files in dir (extracted from assets/baseimage.d81) to
      sfxdata.h/.c (SND_* numbers, each sound's place in attic RAM, length,
      slot) and files.txt (the files for the game's disk). Without them
      the game plays its SID effects only.

The program decodes them all into attic RAM at the start (src/audio.c)
and copies one into a play slot in bank 1 when it is wanted.
"""
import io
import os
import sys
import zipfile
import numpy as np

RATE = 12000
SLOT_LEN = {"L": 0x4000, "S": 0x2000}       # the play slots (src/audio.c)
FILE_MAX = 0x6000                            # a file's data: it loads into the play slots ($19800-$1F7FF)
PEAK = 0.92

NIMBE = ("NimbeKit_Fantasy_RPG_Collection.zip", "NimbeKit_Fantasy_RPG_SFX_160.zip")
FANTASY = ("free-fantasy-sounds-audio.zip",)
ROGUE = ("free-roguelike-sounds-audio.zip",)
TEMPLE = ("free-lost-temple-sounds-audio.zip",)

# name, pack (a zip, and a zip in it), file, start (s), length at most (s), gain, slot
SOUNDS = [
    ("STEP", NIMBE, "fr_movement_bare_stone_step.wav", 0.0, 0.30, 0.3, "S"),
    ("DOOR", TEMPLE, "hidden-latch-click.wav", 0.0, 0.60, 0.9, "S"),
    ("COIN", NIMBE, "fr_loot_single_coin.wav", 0.0, 0.40, 0.8, "S"),
    ("SWING", NIMBE, "fr_combat_sword_swing.wav", 0.02, 0.50, 0.9, "S"),
    ("HIT", FANTASY, "axe-hit-flesh.wav", 0.0, 0.36, 1.0, "S"),
    ("HURT", NIMBE, "fr_combat_cloth_armor_hit.wav", 0.0, 0.36, 1.0, "S"),
    ("CLANG", NIMBE, "fr_combat_parry_spark.wav", 0.0, 0.46, 1.0, "S"),
    ("HEAVY", NIMBE, "fr_combat_heavy_attack.wav", 0.0, 0.75, 1.0, "L"),
    ("FIRE", NIMBE, "fr_magic_ember_cast.wav", 0.0, 1.00, 1.0, "L"),
    ("FROST", NIMBE, "fr_magic_frost_shards.wav", 0.0, 1.00, 1.0, "L"),
    ("HEAL", NIMBE, "fr_magic_restoration.wav", 0.0, 1.20, 0.9, "L"),
    ("WARD", NIMBE, "fr_magic_ward_place.wav", 0.0, 0.70, 0.9, "L"),
    ("POISON", ROGUE, "poison-tick.wav", 0.0, 0.48, 0.9, "S"),
    ("CURSE", ROGUE, "curse-apply.wav", 0.0, 1.30, 0.9, "L"),
    ("DRINK", NIMBE, "fr_loot_potion_drink.wav", 0.0, 0.75, 0.9, "L"),
    ("VICTORY", NIMBE, "fr_feedback_combat_victory.wav", 0.0, 0.95, 0.85, "L"),
    ("LEVEL", NIMBE, "fr_feedback_level_up.wav", 0.0, 1.30, 0.85, "L"),
    ("DEFEAT", NIMBE, "fr_feedback_combat_defeat.wav", 0.0, 0.90, 1.0, "L"),
    ("GROWL", FANTASY, "dragon-idle.wav", 0.0, 1.20, 1.0, "L"),
    ("CHEST", FANTASY, "chest-open.wav", 0.0, 1.30, 1.0, "L"),
    ("BELL", NIMBE, "fr_ambience_ancient_bell.wav", 0.0, 1.00, 0.8, "L"),
    ("TRAP", TEMPLE, "dart-hits-wood.wav", 0.0, 0.60, 1.0, "S"),
    ("STIR", TEMPLE, "bat-screech.wav", 0.0, 0.80, 0.7, "L"),
    # the map's ambience, now and then
    ("DRIP1", NIMBE, "fr_ambience_water_drop_small.wav", 0.0, 0.62, 0.7, "S"),
    ("DRIP2", NIMBE, "fr_ambience_water_drop_deep.wav", 0.0, 0.66, 0.7, "S"),
    ("CHAIN", NIMBE, "fr_ambience_distant_chain.wav", 0.0, 1.30, 0.6, "L"),
    ("FOOT", NIMBE, "fr_ambience_far_footfall.wav", 0.0, 0.55, 0.6, "S"),
    ("EMBER", NIMBE, "fr_ambience_ember_pop.wav", 0.0, 0.40, 0.6, "S"),
]
AMBIENT = ["DRIP1", "DRIP2", "CHAIN", "FOOT", "EMBER"]



# ---------------------------------------------------------------- reading

def member(packs_dir, pack, name):
    """a file's bytes out of a zip (or a zip inside it)"""
    z = zipfile.ZipFile(os.path.join(packs_dir, pack[0]))
    for inner in pack[1:]:
        m = [n for n in z.namelist() if os.path.basename(n) == inner]
        z = zipfile.ZipFile(io.BytesIO(z.read(m[0])))
    m = [n for n in z.namelist() if os.path.basename(n) == name and "__MACOSX" not in n]
    wav = [n for n in m if "/WAV/" in n or n.lower().endswith(".wav")]
    return z.read((wav or m)[0])


def wav_mono(data):
    """a WAV's samples (mono, -1..1) and rate: 8, 16, 24 or 32-bit PCM"""
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE"
    p = 12
    fmt = None
    while p < len(data):
        cid, size = data[p:p + 4], int.from_bytes(data[p + 4:p + 8], "little")
        body = data[p + 8:p + 8 + size]
        if cid == b"fmt ":
            tag, ch, rate = int.from_bytes(body[0:2], "little"), int.from_bytes(body[2:4], "little"), int.from_bytes(body[4:8], "little")
            bits = int.from_bytes(body[14:16], "little")
            fmt = (tag, ch, rate, bits)
        elif cid == b"data":
            tag, ch, rate, bits = fmt
            if bits == 8:
                a = (np.frombuffer(body, np.uint8).astype(np.float32) - 128) / 128
            elif bits == 16:
                a = np.frombuffer(body[:len(body) // 2 * 2], "<i2").astype(np.float32) / 32768
            elif bits == 24:
                u = np.frombuffer(body[:len(body) // 3 * 3], np.uint8).reshape(-1, 3).astype(np.int32)
                a = ((u[:, 0] | u[:, 1] << 8 | u[:, 2] << 16) << 8 >> 8).astype(np.float32) / 8388608
            elif tag == 3:
                a = np.frombuffer(body[:len(body) // 4 * 4], "<f4").astype(np.float32)
            else:
                a = np.frombuffer(body[:len(body) // 4 * 4], "<i4").astype(np.float32) / 2147483648
            a = a[:len(a) // ch * ch].reshape(-1, ch).mean(1)
            return a, rate
        p += 8 + size + (size & 1)
    raise ValueError("no data")


def resample(a, rate):
    """to RATE: a windowed-sinc low-pass below the new Nyquist, then the new
    samples interpolated"""
    if rate == RATE:
        return a
    fc = 0.45 * RATE / rate                    # the cut-off, in cycles a sample
    taps = 255
    n = np.arange(taps) - taps // 2
    h = 2 * fc * np.sinc(2 * fc * n) * np.blackman(taps)
    h /= h.sum()
    f = np.convolve(a, h, mode="same")
    t = np.arange(0, len(a) - 1, rate / RATE)
    return np.interp(t, np.arange(len(a)), f).astype(np.float32)


def prepare(a, rate, start, length, gain):
    a = resample(a, rate)
    a = a[int(start * RATE):]
    a = a[:int(length * RATE)]
    # a short fade in (no click) and out (the end it was cut at)
    fi, fo = min(len(a), int(0.003 * RATE)), min(len(a), int(0.04 * RATE))
    a[:fi] *= np.linspace(0, 1, fi)
    a[len(a) - fo:] *= np.linspace(1, 0, fo)
    a = a - a.mean()
    pk = np.abs(a).max()
    if pk > 0:
        a = a / pk * PEAK * gain
    return a


# ---------------------------------------------------------------- the codec

# Blocks of 16 samples (14-bit, -8192..8191), each a byte (predictor << 4 |
# shift) and 8 bytes of 4-bit residues (-8..7, the first in the low
# nibble): sample = prediction + (residue << shift), the prediction from the
# two before (shifts and adds only, clamped so the sum fits 16 bits):
#   0: none   1: 15/16 of the last   2: 15/16 of 2 last - the one before
#   3: the last + half the step from the one before
# The encoder tries each predictor with the shifts near the block's need.

LIM = 8191
BLOCK = 16
MAX_SHIFT = 11


def predict(f, s1, s2):
    if f == 0:
        return 0
    if f == 1:
        return s1 - (s1 >> 4)
    if f == 2:
        x = 2 * s1 - s2
        x = x - (x >> 4)
    else:
        x = s1 + ((s1 - s2) >> 1)
    return max(-16384, min(16383, x))


def blocks(a):
    """the samples to blocks: (the bytes, the samples as the decoder will
    make them, 8-bit unsigned)"""
    s = np.clip(np.round(a * LIM), -LIM - 1, LIM).astype(np.int64)
    pad = (-len(s)) % BLOCK
    s = np.concatenate([s, np.zeros(pad, np.int64)])
    s1 = s2 = 0
    data = bytearray()
    out = bytearray()
    for b in range(0, len(s), BLOCK):
        blk = [int(v) for v in s[b:b + BLOCK]]
        best = None
        for f in range(4):
            # the shift the residues need without the feedback, and its neighbours
            a1, a2, need = s1, s2, 0
            for v in blk:
                need = max(need, abs(v - predict(f, a1, a2)))
                a2, a1 = a1, v
            sh0 = 0
            while sh0 < MAX_SHIFT and (7 << sh0) < need:
                sh0 += 1
            for sh in range(max(0, sh0 - 1), min(MAX_SHIFT, sh0 + 1) + 1):
                a1, a2, err, dec, res = s1, s2, 0, [], []
                for v in blk:
                    p = predict(f, a1, a2)
                    r = max(-8, min(7, int(round((v - p) / (1 << sh)))))
                    y = max(-LIM - 1, min(LIM, p + (r << sh)))
                    err += (v - y) ** 2
                    dec.append(y)
                    res.append(r & 15)
                    a2, a1 = a1, y
                if best is None or err < best[0]:
                    best = (err, f, sh, dec, res, a1, a2)
        err, f, sh, dec, res, s1, s2 = best
        data.append(f << 4 | sh)
        data.extend(res[k] | res[k + 1] << 4 for k in range(0, BLOCK, 2))
        out.extend((y + LIM + 1) >> 6 for y in dec)
    return bytes(data), bytes(out)


def unblock(data):
    """the coded blocks back to 8-bit unsigned samples, as src/audio.c does"""
    s1 = s2 = 0
    out = bytearray()
    for b in range(0, len(data), 9):
        f, sh = data[b] >> 4, data[b] & 15
        for k in range(BLOCK):
            r = data[b + 1 + (k >> 1)]
            r = r >> 4 if k & 1 else r & 15
            if r >= 8:
                r -= 16
            y = max(-LIM - 1, min(LIM, predict(f, s1, s2) + (r << sh)))
            s2, s1 = s1, y
            out.append((y + LIM + 1) >> 6)
    return bytes(out)


# ---------------------------------------------------------------- the files
#
# DGSFX1, 2, ...: each loads at $19800 (bank 1: the play slots, not yet
# in use; bank 5 holds the title's picture): a count, then for each
# sound in it its number, its coded length (2 bytes), its length decoded
# (2 bytes) and its slot (0 long, 1 short); then the coded sounds one
# after another. Whole sounds to a file, as many as fit.

HDR = 6


def pack(packs, out):
    """the sounds from the packs to the files (make sounds)"""
    os.makedirs(out, exist_ok=True)
    sounds = []
    for k, (name, pk, fn, start, length, gain, slot) in enumerate(SOUNDS):
        a, rate = wav_mono(member(packs, pk, fn))
        data, pcm = blocks(prepare(a, rate, start, length, gain))
        assert len(pcm) <= SLOT_LEN[slot], name
        assert unblock(data) == pcm, name
        sounds.append((k, data, len(pcm), 0 if slot == "L" else 1))
    files, cur, size = [], [], 0
    for snd in sounds:
        if cur and size + HDR + len(snd[1]) > FILE_MAX - 1:
            files.append(cur)
            cur, size = [], 0
        cur.append(snd)
        size += HDR + len(snd[1])
    if cur:
        files.append(cur)
    for f, snds in enumerate(files):
        hdr = bytearray([len(snds)])
        for k, data, plen, slot in snds:
            hdr += bytes([k]) + len(data).to_bytes(2, "little") + plen.to_bytes(2, "little") + bytes([slot])
        with open(os.path.join(out, "dgsfx%d.bin" % (f + 1)), "wb") as fo:
            fo.write(b"\x00\x98" + bytes(hdr) + b"".join(s[1] for s in snds))     # loads at $19800
    # previews to listen to (only here, from the packs: a build does not
    # write the sounds out as WAV files)
    import wave
    pv = os.path.join(out, "wav")
    os.makedirs(pv, exist_ok=True)
    for k, data, plen, slot in sounds:
        w = wave.open(os.path.join(pv, SOUNDS[k][0].lower() + ".wav"), "wb")
        w.setnchannels(1)
        w.setsampwidth(1)
        w.setframerate(RATE)
        w.writeframes(unblock(data)[:plen])
        w.close()
    total = sum(s[2] for s in sounds)
    print("gen_sfx: %d sounds, %.1f s, %d bytes coded in %d files"
          % (len(sounds), total / RATE, sum(len(s[1]) for s in sounds), len(files)))


def read_files(d):
    """the DGSFX files in a directory (as packed, or extracted from a disk
    image: dgsfx1.bin or DGSFX1.prg): [(host name, [(number, coded,
    decoded length, slot)])] in order"""
    found = {}
    if os.path.isdir(d):
        for fn in os.listdir(d):
            base = fn.split(".")[0].upper()
            if base.startswith("DGSFX") and base[5:].isdigit():
                found[int(base[5:])] = fn
    out = []
    for n in sorted(found):
        b = open(os.path.join(d, found[n]), "rb").read()[2:]
        cnt, p, snds = b[0], 1 + b[0] * HDR, []
        for k in range(cnt):
            e = b[1 + k * HDR:1 + (k + 1) * HDR]
            clen, plen = int.from_bytes(e[1:3], "little"), int.from_bytes(e[3:5], "little")
            snds.append((e[0], b[p:p + clen], plen, e[5]))
            p += clen
        out.append((found[n], snds))
    return out


def tables(d, out):
    """the files in d to the C tables, the disk's list, previews"""
    os.makedirs(out, exist_ok=True)
    files = read_files(d)
    sounds = sorted((s for f, snds in files for s in snds), key=lambda s: s[0])
    names = [s[0] for s in SOUNDS]
    if not sounds:
        print("gen_sfx: no sound files in %s: the SID effects only" % d)
    assert not sounds or [s[0] for s in sounds] == list(range(len(names))), \
        "the sound files do not match SOUNDS: make sounds"
    with open(os.path.join(out, "files.txt"), "w") as fl:
        for k, (fn, snds) in enumerate(files):
            fl.write("%s DGSFX%d\n" % (os.path.abspath(os.path.join(d, fn)), k + 1))
    at, ats = 0, []
    for s in sounds:
        ats.append(at)
        at += s[2]
    with open(os.path.join(out, "sfxdata.h"), "w") as h:
        h.write("/* sfxdata.h — made by tools/gen_sfx.py: the sampled sounds */\n#pragma once\n\n")
        h.write("#define SND_RATE %d\n#define SND_COUNT %d\n#define SND_FILES %d\n" % (RATE, len(sounds), len(files)))
        h.write("#define SND_RATE_REG 0x%06XUL       /* the audio DMA's rate: Hz * 2^24 / 40.5 MHz */\n"
                % round(RATE * (1 << 24) / 40500000))
        h.write("#define SND_NONE 0xFF\n")
        for k, name in enumerate(names):
            h.write("#define SND_%s %s\n" % (name, k if sounds else "SND_NONE"))
        h.write("#define SND_AMBIENT %d\n" % (len(AMBIENT) if sounds else 0))
        h.write("extern const unsigned long snd_at[];       /* from SND_ATTIC */\n")
        h.write("extern const unsigned int snd_len[];\n")
        h.write("extern const unsigned char snd_slot[];     /* 0 the long slot, 1 the short */\n")
        h.write("extern const unsigned char snd_ambient[];\n")
    with open(os.path.join(out, "sfxdata.c"), "w") as c:
        c.write("/* sfxdata.c — made by tools/gen_sfx.py */\n#include \"sfxdata.h\"\n\n")
        c.write("const unsigned long snd_at[] = {%s};\n" % ", ".join(str(a) for a in ats or [0]))
        c.write("const unsigned int snd_len[] = {%s};\n" % ", ".join(str(s[2]) for s in sounds or [(0, 0, 0)]))
        c.write("const unsigned char snd_slot[] = {%s};\n" % ", ".join(str(s[3]) for s in sounds or [(0, 0, 0, 0)]))
        c.write("const unsigned char snd_ambient[] = {%s};\n"
                % (", ".join("SND_%s" % n for n in AMBIENT) if sounds else "0"))
    if sounds:
        print("gen_sfx: %d sounds in %d files, %d bytes decoded" % (len(sounds), len(files), at))


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "pack":
        pack(sys.argv[2], sys.argv[3])
    elif len(sys.argv) == 4 and sys.argv[1] == "tables":
        tables(sys.argv[2], sys.argv[3])
    else:
        sys.exit(__doc__)
