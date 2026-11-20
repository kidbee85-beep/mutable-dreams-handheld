/* SPDX-License-Identifier: GPL-3.0-only
 * Mutable Dreams for Trimui Brick: the firmware's host layer, as base/web/emu/emu_web.c uses it, but with the
 * Brick's own audio hook. plat_host.c (unchanged) calls web_audio after every 256-frame block it renders; at that
 * moment its USB channel buffer still holds the same block's six track stems (md_render_stems, FM1B_UAC_CH 8), so
 * the Sound Lab gets the mix and every track, sample-aligned. */
#include <stdint.h>

static void web_audio(const int32_t *blk, uint32_t n);
static void web_said(const uint8_t *p, uint32_t n) { (void)p; (void)n; }
#include "plat_host.c"
#undef __attribute__

/* the output ring, stereo float, filled by run_ms (web_audio), drained by bk_render */
#define BK_RING 8192u
static float bk_ring[BK_RING * 2];
static uint32_t bk_ring_w, bk_ring_r;
static void lab_process(float *lr, const int16_t (*stems)[FM1B_UAC_CH], uint32_t n);   /* brick_lab.inc */
static void bk_collect_out(void);

static void web_audio(const int32_t *blk, uint32_t n)
{
    static float lr[2 * APP_BLOCK];
    uint32_t k;
    for (k = 0; k < 2u * n; k++)
        lr[k] = (float)blk[k] / 8388608.0f;
    lab_process(lr, (const int16_t (*)[FM1B_UAC_CH])usb_ch, n);
    bk_collect_out();
    for (k = 0; k < n; k++) {
        uint32_t i = (bk_ring_w++ % BK_RING) * 2u;
        bk_ring[i] = lr[2 * k];
        bk_ring[i + 1] = lr[2 * k + 1];
    }
}

/* what emu_web.c offers the page, as far as the Brick needs it */
static void web_boot(void) { boot(); }
static void web_buttons(uint32_t m) { held_btn = m; }
static void web_enc(int role, int32_t d)
{
    if (role >= 0 && role < NE)
        enc_acc[role] += d;
}
static void web_master(uint32_t v) { master = v > 4096u ? 4096u : v; }
static uint16_t *web_fb(void) { return fb; }
static uint32_t web_cpu(void) { return cpu_pct; }
static uint32_t web_nobj(void) { return OBJ_NOBJ; }
static uint32_t web_store_max(void) { return PLAT_STORE_MAX; }
static uint8_t *web_store(uint32_t obj) { store_ready(); return obj < OBJ_NOBJ ? store[obj] : 0; }
static int web_store_len(uint32_t obj) { store_ready(); return obj < OBJ_NOBJ ? store_len[obj] : -1; }
static void web_store_set_len(uint32_t obj, int n)
{
    store_ready();
    if (obj < OBJ_NOBJ)
        store_len[obj] = n < 0 ? -1 : n > (int)PLAT_STORE_MAX ? (int)PLAT_STORE_MAX : n;
}
static uint32_t web_store_writes(void) { return store_writes; }

/* MIDI for the engine, as from USB (a packet: CIN | status << 8 | d1 << 16 | d2 << 24) */
static void web_midi_in(uint32_t pkt)
{
    if (mi_w - mi_r < MQ) {
        min_q[mi_w % MQ] = pkt;
        min_port[mi_w++ % MQ] = PORT_USB;
    }
}
/* what the engine sent to USB since the last call, as bytes (clock too): into out, at most max; the count */
static uint32_t bk_tx[1024], bk_tx_n;
static void bk_collect_out(void)
{
    for (uint32_t i = 0; i < n_out; i++)
        if ((out_to[i] & TO_USB) && bk_tx_n < 1024u)
            bk_tx[bk_tx_n++] = out_pkt[i];
    n_out = 0;
}

/* n frames (any number) into lr, the device running until they exist */
static void web_render_lr(float *lr, uint32_t n)
{
    uint32_t k;
    while (bk_ring_w - bk_ring_r < n)
        run_ms(1);
    for (k = 0; k < n; k++) {
        uint32_t i = (bk_ring_r++ % BK_RING) * 2u;
        lr[2 * k] = bk_ring[i];
        lr[2 * k + 1] = bk_ring[i + 1];
    }
}
