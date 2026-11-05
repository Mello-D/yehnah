/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of MENU > USB TABLET's link (the big FM-1, docs/plans/BIG-FM1-PLAN.md step 1; src/usb.c tab_*):
 *   the descriptors: TABLET's are STEREO's (the audio function byte for byte) plus a vendor interface, IF 3, with
 *     EP 03 / 83 bulk 64; bcdDevice STEREO's + 0x08; STEREO's own unchanged with the TABLET mode built
 *     (tests/usb_desc_test.c checks those against the released bytes)
 *   the ring: messages put at random sizes while packets are taken at random times come out as one byte stream
 *     that splits back into the same messages in order; a message that does not fit is refused whole (counted),
 *     nothing is put before HELLO (live); packets <= 64 bytes; a ZLP only right after a full packet that emptied
 *     the ring, never two in a row */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define HALF_FRAMES 128
#define YEHNAH_OTA 1
#define YEHNAH_CDC 0
#define YEHNAH_UAC 1
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/src/usb.c"
static uint32_t ota_now_ms(void) { return 0; }
static void ota_idle(void) {}

static int fails;
static void check(const char *what, int ok)
{
    printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok)
        fails++;
}

static uint32_t rnd(void)
{
    static uint32_t s = 12345u;
    s = s * 1664525u + 1013904223u;
    return s >> 8;
}

/* the tablet's side: the stream back into messages */
static uint8_t st[1 << 20];
static uint32_t st_n;

int main(void)
{
    const uint8_t *dev, *c, *pdev, *pc;
    uint16_t dl, cl, pdl, pcl;
    uint32_t off, nif = 0, i, k;
    int ep03 = 0, ep83 = 0, vendor_if = -1, lens_ok = 1;

    printf("-- MENU > USB TABLET: descriptors, the ring, the packets\n");
    usb_tablet = 0;
    get_desc(0x0100, &pdev, &pdl);
    get_desc(0x0200, &pc, &pcl);
    check("STEREO with TABLET built: the plain device", pdev == DEV_DESC_PLAIN && pc == CFG_DESC_PLAIN);
    usb_tablet = 1;
    check("GET_DESCRIPTOR device / configuration", get_desc(0x0100, &dev, &dl) && dl == 18 && get_desc(0x0200, &c, &cl));
    check("device: STEREO's but bcdDevice + 0x08", memcmp(dev, pdev, 12) == 0 && dev[12] == pdev[12] + 8u &&
                                                     memcmp(dev + 13, pdev + 13, 5) == 0);
    check("configuration: wTotalLength, 4 interfaces", c[2] + (c[3] << 8) == cl && c[4] == 4 && cl == pcl + 23u);
    check("the audio function byte for byte STEREO's", memcmp(c + 5, pc + 5, 4) == 0 && memcmp(c + 9, pc + 9, pcl - 9u) == 0);
    for (off = 0; off < cl;) {
        const uint8_t *d = c + off;
        if (d[0] < 2 || off + d[0] > cl) {
            lens_ok = 0;
            break;
        }
        if (d[1] == 4 && d[3] == 0)
            nif++;
        if (d[1] == 4 && d[5] == 0xFF)
            vendor_if = d[2] == 3 && d[3] == 0 && d[4] == 2 ? 3 : -2;
        if (d[1] == 5 && d[2] == 0x03)
            ep03 = vendor_if == 3 && d[3] == 2 && d[4] == 64 && d[5] == 0;
        if (d[1] == 5 && d[2] == 0x83)
            ep83 = vendor_if == 3 && d[3] == 2 && d[4] == 64 && d[5] == 0;
        off += d[0];
    }
    check("lengths add up", lens_ok && off == cl && nif == 4);
    check("IF 3 vendor (FF), EP 03 OUT and 83 IN bulk 64 on it", vendor_if == 3 && ep03 && ep83);

    /* ---- the ring: random messages, random takes ---- */
    {
        static uint8_t sent[1 << 20], pk[64];
        uint32_t sn = 0, nmsg = 0, refused = 0, pkts = 0, zlps = 0, bad_zlp = 0, maxp = 0, last_full = 0, prev_zlp = 0;
        int ok = 1;
        check("nothing put before HELLO", !tab_put(5, (const uint8_t *)"abc", 3) && tq_w == tq_r && tab.full == 1);
        tab.live = 1;
        tab.full = 0;
        for (i = 0; i < 200000u; i++) {
            if ((i / 3000u) % 2u || rnd() % 3u == 0u) {  /* the main loop puts one (every pass, every other stretch) */
                uint8_t m[400];
                uint32_t n = rnd() % 8u == 0u ? rnd() % 400u : rnd() % 70u, fill = tq_w - tq_r;
                for (k = 0; k < n; k++)
                    m[k] = (uint8_t)rnd();
                if (tab_put(1u + rnd() % 100u, m, n)) {
                    uint32_t w = tq_w - n - 3u;
                    sent[sn++] = tab_q[w % TQ_N];
                    sent[sn++] = (uint8_t)n;
                    sent[sn++] = (uint8_t)(n >> 8);
                    memcpy(sent + sn, m, n);
                    sn += n;
                    nmsg++;
                    if (sn > sizeof sent - 512u)
                        break;
                } else {
                    refused++;
                    if (TQ_N - fill >= n + 3u)
                        ok = 0;                        /* refused though it fitted */
                }
            }
            if (rnd() % 2u == 0u && (tq_w != tq_r || tab.zlp)) {   /* a poll with the FIFO free: tab_ep_tx's path */
                uint32_t was_empty_after_full = tab.zlp && tq_w == tq_r, n = tab_pack(pk);
                pkts++;
                if (n > maxp)
                    maxp = n;
                if (!n) {
                    zlps++;
                    if (!was_empty_after_full || prev_zlp || !last_full)
                        bad_zlp++;
                }
                last_full = n == 64u;
                prev_zlp = !n;
                memcpy(st + st_n, pk, n);
                st_n += n;
            }
        }
        while (tq_w != tq_r || tab.zlp) {              /* drain */
            uint32_t n = tab_pack(pk);
            if (!n) {
                zlps++;
                if (!last_full || prev_zlp)
                    bad_zlp++;
            }
            last_full = n == 64u;
            prev_zlp = !n;
            memcpy(st + st_n, pk, n);
            st_n += n;
        }
        printf("     %u messages (%u B), %u refused, %u packets, %u ZLPs, highest fill %u\n", (unsigned)nmsg, (unsigned)sn,
               (unsigned)refused, (unsigned)pkts, (unsigned)zlps, (unsigned)tab.max_fill);
        check("a refusal only when it did not fit, each counted", ok && refused == tab.full && refused > 0);
        check("the stream = the messages, in order", st_n == sn && memcmp(st, sent, sn) == 0);
        {
            uint32_t p = 0, m = 0;
            while (p + 3u <= st_n) {
                p += 3u + (st[p + 1] | (uint32_t)st[p + 2] << 8);
                m++;
            }
            check("the stream splits back into as many messages", p == st_n && m == nmsg);
        }
        check("packets of 64 bytes at most", maxp == 64u);
        check("ZLPs only after a full packet that emptied the ring, never two in a row", zlps > 0 && bad_zlp == 0);
        check("the ring's highest fill within its size", tab.max_fill <= TQ_N);
    }

    printf(fails ? "FAILED: %d\n" : "all passed\n", fails);
    return fails != 0;
}
