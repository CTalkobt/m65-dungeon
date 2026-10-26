"""pack.py — the art files' packing: a byte-oriented LZ the program unpacks
in place with DMA (src/scene.c unpack)

A packed stream:
  3 bytes   where it unpacks to (a chip RAM address, low byte first)
  then tokens, n = (t & $3F):
  $00-$3F   n + 1 bytes follow as they are
  $40-$7F   n + 3 bytes of the one byte that follows (a DMA fill)
  $80-$BF   n + 3 bytes repeat from o + 1 back (1 byte o follows)
  $C0-$FF   n + 3 bytes repeat from o back (2 bytes o follow, low first)
  $C0 $00 $00  the end
A repeat never overlaps what it writes (its offset is at least its length),
so one forward DMA copy does it.

In place: the stream is loaded inside (and a few bytes past) the area it
unpacks to, at an offset from its start (pack's skip) chosen so that the
writing never overtakes the reading; unpack_in_place checks it.
"""

MIN_RUN = 3
MAX_RUN = 0x3F + MIN_RUN
MAX_LIT = 0x40
WINDOW = 0xFFFF
CHAIN = 96          # candidates looked at for each position


def _match(data, i, cands, n):
    """the best earlier copy of data[i:] (offset >= length): (len, off, cost)
    by the bytes it saves"""
    best = (0, 0, 0)
    for j in reversed(cands[-CHAIN:]):
        off = i - j
        if off > WINDOW:
            break
        lim = min(MAX_RUN, off, n - i)
        if lim < MIN_RUN:
            continue
        L = MIN_RUN
        while L < lim and data[j + L] == data[i + L]:
            L += 1
        cost = 2 if off <= 256 else 3
        if L - cost > best[0] - best[2]:
            best = (L, off, cost)
            if L == MAX_RUN and cost == 2:
                break
    return best


def _run(data, i, n):
    """a run of one byte from i: its length (0 when under MIN_RUN)"""
    r = 1
    while i + r < n and r < MAX_RUN and data[i + r] == data[i]:
        r += 1
    return r if r >= MIN_RUN else 0


def pack(data, dest):
    """data unpacking to dest: (the stream, skip: where to load it, as an
    offset from dest)"""
    data = bytes(data)
    n = len(data)
    out = bytearray([dest & 0xFF, (dest >> 8) & 0xFF, (dest >> 16) & 0xFF])
    lits = bytearray()
    chains = {}
    marks = []                     # (bytes unpacked, bytes of stream read) after each token
    nxt = [0]                      # the next position to go into the chains

    def add_upto(p):
        while nxt[0] < p:
            q = nxt[0]
            if q + MIN_RUN <= n:
                chains.setdefault(data[q:q + MIN_RUN], []).append(q)
            nxt[0] += 1

    def best(p):
        """the best token at p: (len, off (0: a fill), cost)"""
        add_upto(p)
        m = (0, 0, 0)
        if p + MIN_RUN <= n:
            m = _match(data, p, chains.get(data[p:p + MIN_RUN], []), n)
        r = _run(data, p, n)
        if r and r - 2 >= m[0] - m[2]:
            m = (r, 0, 2)
        return m

    def flush(upto):
        """the literals waiting (they end at upto) as literal tokens"""
        while lits:
            k = min(len(lits), MAX_LIT)
            out.append(k - 1)
            out.extend(lits[:k])
            del lits[:k]
            marks.append((upto - len(lits), len(out)))

    def literal(p):
        lits.append(data[p])
        if len(lits) == MAX_LIT:
            flush(p + 1)

    i = 0
    while i < n:
        L, off, cost = best(i)
        if L > cost and i + 1 < n:
            # lazy: if what starts one byte on saves more, that byte is a literal
            L2, off2, cost2 = best(i + 1)
            if L2 - cost2 > L - cost + 1:
                literal(i)
                i += 1
                L, off, cost = L2, off2, cost2
        if L > cost:
            flush(i)
            t = L - MIN_RUN
            if off == 0:
                out += bytes([0x40 | t, data[i]])
            elif off <= 256:
                out += bytes([0x80 | t, off - 1])
            else:
                out += bytes([0xC0 | t, off & 0xFF, off >> 8])
            i += L
            marks.append((i, len(out)))
        else:
            literal(i)
            i += 1
    flush(n)
    out += b"\xC0\x00\x00"
    marks.append((n, len(out)))
    skip = max(0, max(o - r for o, r in marks))
    return bytes(out), skip


def unpack_in_place(stream, skip, size):
    """unpack as the program will, in one memory with the stream at skip:
    the result (an assertion fails if the writing overtakes the reading)"""
    mem = bytearray(max(size, skip + len(stream)) + 256)
    mem[skip:skip + len(stream)] = stream
    r = skip + 3
    w = 0
    while True:
        t = mem[r]
        k = (t & 0x3F)
        if t < 0x40:
            k += 1
            mem[w:w + k] = mem[r + 1:r + 1 + k]
            r += 1 + k
        elif t < 0x80:
            k += MIN_RUN
            mem[w:w + k] = bytes([mem[r + 1]]) * k
            r += 2
        else:
            if t < 0xC0:
                off = mem[r + 1] + 1
                r += 2
            else:
                off = mem[r + 1] | mem[r + 2] << 8
                r += 3
                if not off:
                    break
            k += MIN_RUN
            assert k <= off <= w
            mem[w:w + k] = mem[w - off:w - off + k]
        w += k
        assert w <= r, "the writing overtook the reading"
    return bytes(mem[:w])
