"""pack.py — the art files' packing: cc45's pack45 format 1, which the
library's unpack() (<unpack.h>) unpacks with DMA, in place. (pack45 parses
greedily; this parse is optimal, a few per cent smaller, same format.)

A packed stream:
  1 byte    the format (1)
  4 bytes   where it unpacks to (a 28-bit address, low byte first)
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
FORMAT = 1          # pack45's
CHAIN = 96          # candidates looked at for each position (more: little gained)


def _run(data, i, n):
    """a run of one byte from i: its length (0 when under MIN_RUN)"""
    r = 1
    while i + r < n and r < MAX_RUN and data[i + r] == data[i]:
        r += 1
    return r if r >= MIN_RUN else 0


def pack(data, dest):
    """data unpacking to dest: (the stream, skip: where to load it, as an
    offset from dest). The tokens are chosen for the shortest stream: a
    parse from the end, each place's cheapest way on (a token, or a literal
    run); the copies looked for among the CHAIN last places that began
    with the same 3 bytes"""
    data = bytes(data)
    n = len(data)
    INF = 1 << 30
    # the longest copy from a near (<= 256 back) and from a far offset
    # at each place, and the run of one byte there
    near = [0] * n
    far = [0] * n
    near_off = [0] * n
    far_off = [0] * n
    run = [0] * n
    chains = {}
    for i in range(n):
        if i + MIN_RUN <= n:
            key = data[i:i + MIN_RUN]
            cands = chains.get(key)
            if cands:
                bn = bf = 0
                for j in reversed(cands[-CHAIN:]):
                    off = i - j
                    if off > WINDOW:
                        break
                    lim = min(MAX_RUN, off, n - i)
                    if lim < MIN_RUN or (off <= 256 and lim <= bn) or (off > 256 and lim <= bf):
                        continue
                    L = MIN_RUN
                    while L < lim and data[j + L] == data[i + L]:
                        L += 1
                    if off <= 256:
                        if L > bn:
                            bn, near_off[i] = L, off
                    elif L > bf:
                        bf, far_off[i] = L, off
                near[i], far[i] = bn, bf
            chains.setdefault(key, []).append(i)
        r = _run(data, i, n)
        run[i] = r
    # A[i]: the cheapest from i at a token's start; B[i]: from i inside a
    # literal run (data[i] one of its bytes)
    A = [INF] * (n + 1)
    B = [INF] * (n + 1)
    choice = [None] * (n + 1)
    A[n] = 0
    for i in range(n - 1, -1, -1):
        B[i] = 1 + min(B[i + 1], A[i + 1])
        best, how = 1 + B[i], None
        for L in range(MIN_RUN, run[i] + 1):
            c = 2 + A[i + L]
            if c < best:
                best, how = c, (L, 0)
        for L in range(MIN_RUN, near[i] + 1):
            c = 2 + A[i + L]
            if c < best:
                best, how = c, (L, near_off[i])
        for L in range(max(MIN_RUN, near[i] + 1), far[i] + 1):
            c = 3 + A[i + L]
            if c < best:
                best, how = c, (L, far_off[i])
        A[i] = best
        choice[i] = how
    out = bytearray([FORMAT, dest & 0xFF, (dest >> 8) & 0xFF, (dest >> 16) & 0xFF, (dest >> 24) & 0xFF])
    marks = []
    i = 0
    while i < n:
        how = choice[i]
        if how is None:
            # a literal run: on while staying in it is the cheaper way
            j = i + 1
            while j < n and B[j] <= A[j]:
                j += 1
            while i < j:
                k = min(j - i, MAX_LIT)
                out.append(k - 1)
                out.extend(data[i:i + k])
                i += k
                marks.append((i, len(out)))
            continue
        L, off = how
        t = L - MIN_RUN
        if off == 0:
            out += bytes([0x40 | t, data[i]])
        elif off <= 256:
            out += bytes([0x80 | t, off - 1])
        else:
            out += bytes([0xC0 | t, off & 0xFF, off >> 8])
        i += L
        marks.append((i, len(out)))
    out += b"\xC0\x00\x00"
    marks.append((n, len(out)))
    skip = max(0, max(o - r for o, r in marks))
    return bytes(out), skip


def unpack_in_place(stream, skip, size):
    """unpack as the program will, in one memory with the stream at skip:
    the result (an assertion fails if the writing overtakes the reading)"""
    mem = bytearray(max(size, skip + len(stream)) + 256)
    mem[skip:skip + len(stream)] = stream
    assert mem[skip] == FORMAT
    r = skip + 5
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
