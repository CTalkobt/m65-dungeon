"""items.py — every thing one carries, one line each, and the C made of it.

    python3 tools/items.py build/items     writes items.h and items.c there

The list's order is the items' numbers (I_*): pl.items[] in a save is
kept by them, so a new item goes where its kind is (the flasks and the
charms stay together, at the end) and the save's VERSION (src/keep.c) goes
up. What an item does is its use (U_*): pack.c use() does it, the same for
all the items of a use. The rest says where it is found:

  sub     its place among those of its use (fuel's words, a flask's colour)
  val     the fuel's turns of burning
  key     thrown (X): the key that chooses it, and what it is called then
  shop    (so many, for so much gold): the merchant has it (shop.c)
  floor   (k, n): lying about on a level, k times in n (dungeon.c)
  chest   (group, weight, most): in a chest's bundle (1) or scroll case
          (2), weight among the group's, 1 .. most of it (detail.c)
"""
import os
import sys

USES = ["POTION", "DRAUGHT", "FIGHT", "RECALL", "SIGHT", "KEY", "FUEL", "LIGHT", "BOW", "GUN",
        "THROW", "ANTIDOTE", "BISCUIT", "STICK", "FLASK", "CHARM"]


def it(ident, name, plural, col, tile, use, val=0, key=None, shop=None, floor=None, chest=None):
    return dict(id=ident, name=name, plural=plural, col=col, tile=tile, use=use, val=val, key=key,
                shop=shop, floor=floor, chest=chest)


ITEMS = [
    it("POTION", "healing potion", "healing potions", "RED", "POTION", "POTION", shop=(1, 25)),
    it("DRAUGHT", "mana draught", "mana draughts", "BLUE", "DRAUGHT", "DRAUGHT", shop=(1, 30)),
    it("FIRE", "scroll of fire", "scrolls of fire", "BONE", "SCROLL", "FIGHT", chest=(2, 4, 1)),
    it("RECALL", "scroll of recall", "scrolls of recall", "BONE", "SCROLL", "RECALL", chest=(2, 3, 1)),
    it("SIGHT", "scroll of far sight", "scrolls of far sight", "BONE", "SCROLL", "SIGHT",
       shop=(1, 45), chest=(2, 3, 1)),
    it("KEY", "iron key", "iron keys", "GOLD", "KEY", "KEY"),
    it("RAGS", "twist of rags", "twists of rags", "ORANGE", "RAGS", "FUEL", val=80,
       shop=(1, 6), chest=(1, 3, 2)),
    it("STICKS", "bundle of sticks", "bundles of sticks", "ORANGE", "STICKS", "FUEL", val=120,
       shop=(1, 10), chest=(1, 2, 2)),
    it("FUR", "scrap of fur", "scraps of fur", "ORANGE", "FUR", "FUEL", val=160, chest=(1, 2, 2)),
    it("CANDLE", "tallow candle", "tallow candles", "ORANGE", "CANDLE", "FUEL", val=220,
       shop=(1, 18), chest=(1, 1, 2)),
    it("FLINT", "flint", "flints", "GREY", "FLINT", "LIGHT", shop=(1, 40), chest=(1, 1, 1)),
    it("FIRESTICK", "firestick", "firesticks", "ORANGE", "FIRESTICK", "LIGHT", shop=(1, 12), chest=(1, 1, 2)),
    it("ARROWS", "arrow", "arrows", "TEXT", "ARROWS", "BOW", shop=(5, 15)),
    it("SHOT", "cartridge", "cartridges", "GREY", "SHOT", "GUN", shop=(3, 24)),
    it("OIL", "flask of oil", "flasks of oil", "ORANGE", "OIL", "THROW", key=("o", "oil"),
       shop=(1, 20), floor=(1, 1), chest=(1, 2, 2)),
    it("FOOD", "strip of dried meat", "strips of dried meat", "BLOOD", "FOOD", "THROW", key=("m", "meat"),
       shop=(2, 8), floor=(1, 2), chest=(1, 1, 2)),
    it("ANTIDOTE", "vial of antidote", "vials of antidote", "CYAN", "ANTIDOTE", "ANTIDOTE",
       shop=(1, 20), floor=(2, 3)),
    it("BISCUIT", "dog biscuit", "dog biscuits", "GOLD", "BISCUIT", "BISCUIT",
       shop=(2, 8), floor=(1, 3), chest=(1, 2, 2)),
    it("STICK", "chewed stick", "chewed sticks", "BONE", "STICK", "STICK", key=("s", "the stick"),
       floor=(1, 5)),
    # the strange flasks, by their colour (what each does: pl.flask, game.c flask_fx)
    it("FLASK", "cloudy green flask", "cloudy green flasks", "GREEN", "FLASK1", "FLASK"),
    it("FLASK2", "smoky grey flask", "smoky grey flasks", "GREY", "FLASK2", "FLASK"),
    it("FLASK3", "violet flask", "violet flasks", "PURPLE", "FLASK3", "FLASK"),
    it("FLASK4", "amber flask", "amber flasks", "GOLD", "FLASK4", "FLASK"),
    # the rings and amulets, by their look (what each does: pl.charm, game.c charm_fx)
    it("CHARM", "iron ring", "iron rings", "GREY", "RING", "CHARM"),
    it("CHARM2", "jade ring", "jade rings", "GREEN", "RING", "CHARM"),
    it("CHARM3", "silver ring", "silver rings", "TEXT", "RING", "CHARM"),
    it("CHARM4", "bone amulet", "bone amulets", "BONE", "AMULET", "CHARM"),
    it("CHARM5", "amber amulet", "amber amulets", "GOLD", "AMULET", "CHARM"),
]

FIELDS = ["use", "sub", "val", "key", "shop_n", "shop_price", "floor_k", "floor_n", "chest", "chest_w", "chest_most"]


def check():
    for u in ("FLASK", "CHARM"):                # (a flask's or a charm's number: from the first of them)
        ks = [k for k, i in enumerate(ITEMS) if i["use"] == u]
        assert ks == list(range(ks[0], ks[0] + len(ks))), "the %s items are not together" % u
    keys = [i["key"][0] for i in ITEMS if i["key"]]
    assert len(set(keys)) == len(keys), "two things thrown by one key"
    for i in ITEMS:
        assert i["use"] in USES, i["id"]
        assert i["key"] is None or i["use"] in ("THROW", "STICK"), i["id"]
        assert len(i["name"]) < 32 and len(i["plural"]) < 32, i["id"]   # (ui.c far_buf)
    assert len(ITEMS) < 32, "O_PILE + I_COUNT: the things' kinds"


def row(i, sub):
    shop = i["shop"] or (0, 0)
    floor = i["floor"] or (0, 0)
    chest = i["chest"] or (0, 0, 0)
    return [USES.index(i["use"]), sub, i["val"], ord(i["key"][0]) if i["key"] else 0,
            shop[0], shop[1], floor[0], floor[1], chest[0], chest[1], chest[2]]


def main():
    check()
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    first = {}
    rows = []
    for k, i in enumerate(ITEMS):
        first.setdefault(i["use"], k)
        rows.append(row(i, k - first[i["use"]]))
    wares = [k for k, i in enumerate(ITEMS) if i["shop"]]
    thrown = [i for i in ITEMS if i["key"]]
    prompt = "Throw what? " + " ".join("%s: %s." % (i["key"][0].upper(), i["key"][1]) for i in thrown)
    with open(os.path.join(out, "items.h"), "w") as f:
        f.write("/* items.h — made by tools/items.py: the things one carries */\n#pragma once\n\n")
        for k, i in enumerate(ITEMS):
            f.write("#define I_%-10s %d\n" % (i["id"], k))
        f.write("#define I_COUNT    %d\n" % len(ITEMS))
        f.write("#define FLASKS     %d\n" % sum(i["use"] == "FLASK" for i in ITEMS))
        f.write("#define CHARMS     %d\n\n" % sum(i["use"] == "CHARM" for i in ITEMS))
        f.write("/* what each does (item_use; pack.c use) */\n")
        for k, u in enumerate(USES):
            f.write("#define U_%-9s %d\n" % (u, k))
        f.write("\n/* the merchant's kinds of wares (ware_item: their items) */\n")
        f.write("#define WARE_KINDS %d\n\n" % len(wares))
        f.write("/* the rest of what is known of an item, kept in attic RAM (item_info) */\n")
        f.write("struct item_info {\n    unsigned char %s;\n};\n" % ", ".join(FIELDS))
        f.write("#define ITEM_FIELDS %d\n\n" % len(FIELDS))
        f.write("extern const unsigned char item_col[I_COUNT];    /* its colour in text */\n")
        f.write("extern const unsigned char item_tile[I_COUNT];   /* its tile, put down (T_*) */\n")
        f.write("extern const unsigned char item_use[I_COUNT];    /* U_* */\n")
        f.write("extern const unsigned char ware_item[WARE_KINDS];\n")
        f.write("extern const char item_throw_what[];             /* X, more than one thing to throw */\n")
        f.write("/* (item_names, item_far: in attic RAM, read by game.c alone) */\n")
    with open(os.path.join(out, "items.c"), "w") as f:
        f.write("/* items.c — made by tools/items.py */\n#pragma encoding ascii\n")
        f.write("#include \"hw.h\"\n#include \"assets.h\"\n#include \"items.h\"\n\n")
        f.write("const unsigned char item_col[I_COUNT] = {%s};\n" % ", ".join("C_" + i["col"] for i in ITEMS))
        f.write("const unsigned char item_tile[I_COUNT] = {%s};\n" % ", ".join("T_" + i["tile"] for i in ITEMS))
        f.write("const unsigned char item_use[I_COUNT] = {%s};\n" % ", ".join("U_" + i["use"] for i in ITEMS))
        f.write("const unsigned char ware_item[WARE_KINDS] = {%s};\n" % ", ".join("I_" + ITEMS[k]["id"] for k in wares))
        f.write("const char item_throw_what[] = \"%s\";\n" % prompt)
        f.write("__bank(NAMES_BANK) const char item_names[] = \"%s\";\n"
                % "\\0".join(s for i in ITEMS for s in (i["name"], i["plural"])))
        f.write("__bank(NAMES_BANK) const unsigned char item_far[] = {\n")
        for i, r in zip(ITEMS, rows):
            f.write("    %s,   /* %s */\n" % (", ".join(str(v) for v in r), i["id"]))
        f.write("};\n")
    print("items: %d, %d wares, %d thrown by a key" % (len(ITEMS), len(wares), len(thrown)))


if __name__ == "__main__":
    main()
