/* shop.c — the merchant: walk into him, and buy with gold (the overlay
 * "pack") */
#pragma cc45 overlay(pack)
#pragma cc45 noescape_strings
#include "hw.h"
#include "assets.h"
#include "video.h"
#include "game.h"

/* his kinds of wares: ware_item (items.h); how many to the sale, for how
 * much gold: each item's shop_n, shop_price (tools/items.py) */

#define SH_COL 14
#define SH_ROW 8
#define SH_W   52
#define SH_H   20

unsigned char shop(void) {
    unsigned char k, w, sel = 0, wn[WARES], wp[WARES];
    struct item_info in;
    char buf[64];
    for (k = 0; k < WARES; k++) {
        item_info(ware_item[ware[k]], &in);
        wn[k] = in.shop_n;
        wp[k] = in.shop_price;
    }
    msg("A merchant lifts his lantern. \"Coin for light?\"", C_GOLD);
    video_blank(SH_COL, SH_ROW, SH_W, SH_H);
    ui_frame(SH_COL, SH_ROW, SH_W, SH_H);
    video_text(SH_COL + 3, SH_ROW + 1, "The merchant's wares", C_GOLD);
    video_text(SH_COL + 3, SH_ROW + 18, "RETURN: buy.  RUN/STOP: leave.", C_DIM);
    for (;;) {
        buf[0] = 0;
        str_cat(buf, "You have ");
        str_num(buf, pl.gold);
        str_cat(buf, " gold.");
        video_blank(SH_COL + 3, SH_ROW + 3, SH_W - 6, 1);
        video_text(SH_COL + 3, SH_ROW + 3, buf, C_TEXT);
        for (k = 0; k < WARES; k++) {
            w = ware[k];
            buf[0] = k == sel ? '>' : ' ';
            buf[1] = ' ';
            buf[2] = 0;
            if (wn[k] > 1) {
                str_num(buf, wn[k]);
                str_cat(buf, " ");
            }
            str_cat(buf, item_name(ware_item[w], wn[k]));
            video_blank(SH_COL + 2, SH_ROW + 5 + k * 2, SH_W - 4, 1);
            video_text(SH_COL + 2, SH_ROW + 5 + k * 2, buf,
                       !ware_left[k] ? C_DARK : k == sel ? C_HILITE : item_col[ware_item[w]]);
            buf[0] = 0;
            if (ware_left[k]) {
                str_num(buf, wp[k]);
                str_cat(buf, " gold  (");
                str_num(buf, ware_left[k]);
                str_cat(buf, ")");
            } else str_cat(buf, "sold");
            video_text(SH_COL + 32, SH_ROW + 5 + k * 2, buf, ware_left[k] && pl.gold >= wp[k] ? C_TEXT : C_DARK);
        }
        k = key_wait();
        if (k == K_UP) sel = sel ? sel - 1 : WARES - 1;
        else if (k == K_DOWN) sel = sel + 1 < WARES ? sel + 1 : 0;
        else if (k == K_STOP) break;
        else if (k == K_FIRE) {
            w = ware[sel];
            if (!ware_left[sel]) msg("\"That is gone, friend.\"", C_DIM);
            else if (pl.gold < wp[sel]) msg("\"More coin than that, friend.\"", C_DIM);
            else {
                pl.gold -= wp[sel];
                ware_left[sel]--;
                pl.items[ware_item[w]] += wn[sel];
                buf[0] = 0;
                str_cat(buf, "You buy ");
                if (wn[sel] > 1) str_num(buf, wn[sel]);
                else str_cat(buf, "a");
                str_cat(buf, " ");
                str_cat(buf, item_name(ware_item[w], wn[sel]));
                str_cat(buf, ".");
                msg(buf, C_GOLD);
                sfx(SFX_COIN);
            }
        }
    }
    msg("\"Mind the dark.\"", C_DIM);
    view_draw();
    view_hud();
    return 0;
}
