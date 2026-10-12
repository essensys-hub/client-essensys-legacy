/* COPIE de essensys-gcc/bp/app/ba_resil.c -- ne pas modifier ici.
 * Source de reference et tests hote (NR-gcc-*) dans essensys-gcc ;
 * resynchroniser avec scripts/sync-from-essensys-gcc.sh.
 * Feature i2c-surge-protection-2026-10-007, Refs essensys-hub/essensys-board-SC944D#1 */
/*
 * ba_resil.c -- Resilience du dialogue BP <-> BA (voir ba_resil.h).
 *
 * Feature : i2c-surge-protection-2026-10-007
 * Refs    : essensys-hub/essensys-board-SC944D#1
 */
#include "ba_resil.h"

#include <string.h>

#define ELAPSED(now, then)  ((uint32_t)((uint32_t)(now) - (uint32_t)(then)))

static const uint8_t s_idx12_bits[BAR_NB_BA] = { 0x04u, 0x08u, 0x10u };

uint8_t bar_idx12_bit(unsigned ba)
{
    return (ba < BAR_NB_BA) ? s_idx12_bits[ba] : 0u;
}

void bar_init(bar_ctx_t *c)
{
    unsigned i;

    memset(c, 0, sizeof(*c));
    c->volet_expiry_ms = BAR_VOLET_EXPIRY_MS_DEFAULT;
    c->te2.j58_open = 1;    /* tant que non declare ferme : aucun cycle */
    for (i = 0; i < BAR_NB_BA; i++) {
        c->ba[i].health = BAR_SUSPECT;     /* inconnu jusqu'au premier echange */
        c->ba[i].resync_pending = 1;       /* resynchronisation au demarrage */
    }
}

/* ------------------------------------------------------------------------ */
/* Sante                                                                     */
/* ------------------------------------------------------------------------ */

static void update_all_down(bar_ctx_t *c)
{
    unsigned i;
    int all = 1;

    for (i = 0; i < BAR_NB_BA; i++) {
        if (c->ba[i].health != BAR_ABSENT) {
            all = 0;
        }
    }
    if (all && !c->all_down) {
        c->all_down_events++;
    }
    c->all_down = (uint8_t)all;
}

int bar_on_exchange(bar_ctx_t *c, unsigned ba, int ok, uint32_t now_ms)
{
    bar_ba_t *b;
    int back = 0;

    if (ba >= BAR_NB_BA) {
        return 0;
    }
    b = &c->ba[ba];
    b->last_attempt_ms = now_ms;
    b->have_attempt = 1;
    if (ok) {
        if (b->health != BAR_OK && b->ever_ok) {
            back = 1;
            b->resync_pending = 1;     /* retour a OK : resynchronisation */
        }
        b->health = BAR_OK;
        b->fails = 0;
        b->ever_ok = 1;
        b->last_ok_ms = now_ms;
    } else {
        if (b->fails < 0xFFu) {
            b->fails++;
        }
        b->health = (b->fails >= BAR_FAILS_TO_ABSENT) ? BAR_ABSENT : BAR_SUSPECT;
    }
    update_all_down(c);
    return back;
}

bar_health_t bar_health(const bar_ctx_t *c, unsigned ba)
{
    return (ba < BAR_NB_BA) ? c->ba[ba].health : BAR_ABSENT;
}

int bar_may_send(const bar_ctx_t *c, unsigned ba, uint32_t now_ms)
{
    const bar_ba_t *b;

    if (ba >= BAR_NB_BA) {
        return 0;
    }
    b = &c->ba[ba];
    if (b->health == BAR_OK || !b->have_attempt) {
        return 1;
    }
    return ELAPSED(now_ms, b->last_attempt_ms) >= BAR_RETRY_NOK_MS;
}

int bar_presence_due(const bar_ctx_t *c, unsigned ba, uint32_t now_ms)
{
    const bar_ba_t *b;

    if (ba >= BAR_NB_BA) {
        return 0;
    }
    b = &c->ba[ba];
    if (!b->have_attempt) {
        return 1;
    }
    if (b->health == BAR_OK) {
        return ELAPSED(now_ms, b->last_attempt_ms) >= BAR_PRESENCE_POLL_OK_MS;
    }
    return ELAPSED(now_ms, b->last_attempt_ms) >= BAR_RETRY_NOK_MS;
}

uint8_t bar_idx12_bits(const bar_ctx_t *c)
{
    uint8_t v = 0;
    unsigned i;

    for (i = 0; i < BAR_NB_BA; i++) {
        if (c->ba[i].health == BAR_ABSENT) {
            v |= s_idx12_bits[i];
        }
    }
    return v;
}

uint8_t bar_idx12_apply(const bar_ctx_t *c, uint8_t idx12)
{
    return (uint8_t)((idx12 & (uint8_t)~BAR_IDX12_BA_MASK) | bar_idx12_bits(c));
}

/* ------------------------------------------------------------------------ */
/* Resynchronisation                                                         */
/* ------------------------------------------------------------------------ */

void bar_request_resync(bar_ctx_t *c, unsigned ba)
{
    if (ba < BAR_NB_BA) {
        c->ba[ba].resync_pending = 1;
    }
}

void bar_request_resync_all(bar_ctx_t *c)
{
    unsigned i;
    for (i = 0; i < BAR_NB_BA; i++) {
        c->ba[i].resync_pending = 1;
    }
}

int bar_take_resync(bar_ctx_t *c, unsigned ba, uint32_t now_ms)
{
    bar_ba_t *b;

    if (ba >= BAR_NB_BA) {
        return 0;
    }
    b = &c->ba[ba];
    if (!b->resync_pending || !bar_may_send(c, ba, now_ms)) {
        return 0;
    }
    b->resync_pending = 0;
    b->resyncs++;
    return 1;
}

int bar_parse_comm_stat(uint8_t byte, uint8_t *flags)
{
    if ((byte & BAR_COMM_STAT_SIG_MASK) != BAR_COMM_STAT_SIG) {
        return 0;
    }
    if (flags != NULL) {
        *flags = (uint8_t)(byte & 0x0Fu);
    }
    return 1;
}

uint8_t bar_on_comm_stat(bar_ctx_t *c, unsigned ba, uint8_t byte)
{
    uint8_t flags = 0;

    if (ba >= BAR_NB_BA || !bar_parse_comm_stat(byte, &flags)) {
        return 0;
    }
    if (flags & (BAR_COMM_STAT_RESTART | BAR_COMM_STAT_WDT)) {
        c->ba[ba].resync_pending = 1;
        c->ba[ba].restarts_reported++;
    }
    return flags;
}

/* ------------------------------------------------------------------------ */
/* Commandes conservees jusqu'a acquittement                                 */
/* ------------------------------------------------------------------------ */

void bar_cmd_merge(bar_ctx_t *c, unsigned ba, const uint8_t data[8], uint32_t now_ms)
{
    bar_ba_t *b;
    uint16_t s_off, s_on;
    uint8_t v_off, v_on, up, down;
    unsigned i;

    if (ba >= BAR_NB_BA) {
        return;
    }
    b = &c->ba[ba];
    s_off = (uint16_t)(data[0] | ((uint16_t)data[2] << 8));
    s_on  = (uint16_t)(data[1] | ((uint16_t)data[3] << 8));
    v_off = data[4];
    v_on  = data[5];
    up    = data[6];
    down  = data[7];

    /* Sorties maintenues : seul le dernier etat demande compte. Dans une
     * meme trame, le BA applique l'allumage puis l'extinction (extinction
     * prioritaire) : meme ordre ici. */
    b->simple_on  = (uint16_t)((b->simple_on | s_on) & (uint16_t)~s_off);
    b->simple_off = (uint16_t)((b->simple_off & (uint16_t)~s_on) | s_off);
    b->var_on     = (uint8_t)((b->var_on | v_on) & (uint8_t)~v_off);
    b->var_off    = (uint8_t)((b->var_off & (uint8_t)~v_on) | v_off);

    /* Volets : ouverture prioritaire dans une meme trame (comme le BA). */
    down = (uint8_t)(down & (uint8_t)~up);
    b->volet_down = (uint8_t)((b->volet_down & (uint8_t)~up) | down);
    b->volet_up   = (uint8_t)((b->volet_up & (uint8_t)~down) | up);
    for (i = 0; i < BAR_NB_VOLETS; i++) {
        if ((up | down) & (1u << i)) {
            b->volet_ts[i] = now_ms;
        }
    }
}

uint8_t bar_cmd_expire(bar_ctx_t *c, unsigned ba, uint32_t now_ms)
{
    bar_ba_t *b;
    uint8_t expired = 0;
    unsigned i;

    if (ba >= BAR_NB_BA) {
        return 0;
    }
    b = &c->ba[ba];
    for (i = 0; i < BAR_NB_VOLETS; i++) {
        uint8_t m = (uint8_t)(1u << i);
        if (((b->volet_up | b->volet_down) & m) &&
            ELAPSED(now_ms, b->volet_ts[i]) >= c->volet_expiry_ms) {
            expired |= m;
        }
    }
    if (expired) {
        b->volet_up &= (uint8_t)~expired;
        b->volet_down &= (uint8_t)~expired;
        b->if_volet_up &= (uint8_t)~expired;
        b->if_volet_down &= (uint8_t)~expired;
        for (i = 0; i < BAR_NB_VOLETS; i++) {
            if (expired & (1u << i)) {
                b->expired++;
            }
        }
        b->last_expired_mask = expired;
    }
    return expired;
}

int bar_cmd_pending(const bar_ctx_t *c, unsigned ba)
{
    const bar_ba_t *b;

    if (ba >= BAR_NB_BA) {
        return 0;
    }
    b = &c->ba[ba];
    return (b->simple_on | b->simple_off | b->var_on | b->var_off |
            b->volet_up | b->volet_down) != 0;
}

int bar_cmd_build(bar_ctx_t *c, unsigned ba, uint8_t out[8])
{
    bar_ba_t *b;

    if (!bar_cmd_pending(c, ba)) {
        return 0;
    }
    b = &c->ba[ba];
    b->if_simple_on  = b->simple_on;
    b->if_simple_off = b->simple_off;
    b->if_var_on     = b->var_on;
    b->if_var_off    = b->var_off;
    b->if_volet_up   = b->volet_up;
    b->if_volet_down = b->volet_down;

    out[0] = (uint8_t)(b->simple_off & 0xFFu);
    out[1] = (uint8_t)(b->simple_on & 0xFFu);
    out[2] = (uint8_t)(b->simple_off >> 8);
    out[3] = (uint8_t)(b->simple_on >> 8);
    out[4] = b->var_off;
    out[5] = b->var_on;
    out[6] = b->volet_up;
    out[7] = b->volet_down;
    return 1;
}

void bar_cmd_acked(bar_ctx_t *c, unsigned ba)
{
    bar_ba_t *b;

    if (ba >= BAR_NB_BA) {
        return;
    }
    b = &c->ba[ba];
    /* Seuls les bits emis dans la trame acquittee sont retires ; un ordre
     * arrive entre-temps (sens oppose) reste en attente. */
    b->simple_on  &= (uint16_t)~b->if_simple_on;
    b->simple_off &= (uint16_t)~b->if_simple_off;
    b->var_on     &= (uint8_t)~b->if_var_on;
    b->var_off    &= (uint8_t)~b->if_var_off;
    b->volet_up   &= (uint8_t)~b->if_volet_up;
    b->volet_down &= (uint8_t)~b->if_volet_down;
    b->if_simple_on = b->if_simple_off = 0;
    b->if_var_on = b->if_var_off = 0;
    b->if_volet_up = b->if_volet_down = 0;
}

/* ------------------------------------------------------------------------ */
/* Cycle TE2 en dernier recours                                              */
/* ------------------------------------------------------------------------ */

unsigned bar_te2_cycles_in_window(const bar_ctx_t *c, uint32_t epoch_s)
{
    unsigned i, n = 0;

    for (i = 0; i < BAR_TE2_MAX_PER_WINDOW; i++) {
        uint32_t t = c->te2.cycles_epoch[i];
        if (t != 0u && t <= epoch_s && (epoch_s - t) < BAR_TE2_WINDOW_S) {
            n++;
        }
    }
    return n;
}

uint32_t bar_te2_last_cycle_epoch(const bar_ctx_t *c)
{
    uint32_t last = 0;
    unsigned i;

    for (i = 0; i < BAR_TE2_MAX_PER_WINDOW; i++) {
        if (c->te2.cycles_epoch[i] > last) {
            last = c->te2.cycles_epoch[i];
        }
    }
    return last;
}

static void te2_record_cycle(bar_te2_t *t, uint32_t epoch_s)
{
    unsigned i, oldest = 0;

    for (i = 1; i < BAR_TE2_MAX_PER_WINDOW; i++) {
        if (t->cycles_epoch[i] < t->cycles_epoch[oldest]) {
            oldest = i;
        }
    }
    t->cycles_epoch[oldest] = epoch_s;
    t->total_cycles++;
    t->nv_dirty = 1;
}

static bar_te2_action_t te2_abstain(bar_te2_t *t, bar_te2_why_t why)
{
    if (t->why != why) {
        t->blocked_reports++;
    }
    t->why = why;
    return BAR_TE2_NONE;
}

bar_te2_action_t bar_te2_step(bar_ctx_t *c, uint32_t now_ms, uint32_t epoch_s,
                              int bus_blocked, uint32_t blocked_since_ms,
                              int secouru)
{
    bar_te2_t *t = &c->te2;
    uint32_t last;

    if (t->cutting) {
        if (ELAPSED(now_ms, t->cut_start_ms) >= BAR_TE2_CUT_MS) {
            t->cutting = 0;
            t->awaiting_effect = 1;
            t->cycle_end_ms = now_ms;
            bar_request_resync_all(c);   /* resynchronisation complete */
            return BAR_TE2_CUT_END;
        }
        return BAR_TE2_NONE;
    }

    if (!bus_blocked) {
        /* Retour du bus : le cycle a eu de l'effet, la suspension tombe. */
        t->awaiting_effect = 0;
        if (t->suspended) {
            t->suspended = 0;
            t->nv_dirty = 1;
        }
        t->why = BAR_TE2_WHY_NONE;
        return BAR_TE2_NONE;
    }

    if (t->awaiting_effect) {
        if (ELAPSED(now_ms, t->cycle_end_ms) < BAR_TE2_EFFECT_MS) {
            return te2_abstain(t, BAR_TE2_WHY_WAIT_EFFECT);
        }
        /* Cycle sans effet (typiquement un BA mort qui tient le bus) :
         * plus aucun cycle jusqu'au retour du bus ou a une RAZ manuelle. */
        t->awaiting_effect = 0;
        t->suspended = 1;
        t->nv_dirty = 1;
        return te2_abstain(t, BAR_TE2_WHY_SUSPENDED);
    }

    if (ELAPSED(now_ms, blocked_since_ms) < BAR_TE2_BLOCK_MS) {
        return BAR_TE2_NONE;
    }

    /* Blocage persistant (>= 60 s malgre les recuperations). */
    if (t->j58_open) {
        return te2_abstain(t, BAR_TE2_WHY_J58_OPEN);  /* TE2 sans effet : ni cycle ni comptage */
    }
    if (secouru) {
        return te2_abstain(t, BAR_TE2_WHY_SECOURU);
    }
    if (t->suspended) {
        return te2_abstain(t, BAR_TE2_WHY_SUSPENDED);
    }
    if (epoch_s == 0u) {
        return te2_abstain(t, BAR_TE2_WHY_TIME_UNKNOWN);
    }
    last = bar_te2_last_cycle_epoch(c);
    if (last != 0u && (last > epoch_s || (epoch_s - last) < BAR_TE2_MIN_SPACING_S)) {
        return te2_abstain(t, BAR_TE2_WHY_SPACING);
    }
    if (bar_te2_cycles_in_window(c, epoch_s) >= BAR_TE2_MAX_PER_WINDOW) {
        return te2_abstain(t, BAR_TE2_WHY_LIMIT);
    }

    te2_record_cycle(t, epoch_s);
    t->cutting = 1;
    t->cut_start_ms = now_ms;
    t->why = BAR_TE2_WHY_NONE;
    return BAR_TE2_CUT_START;
}

void bar_te2_manual_reset(bar_ctx_t *c)
{
    c->te2.suspended = 0;
    c->te2.awaiting_effect = 0;
    c->te2.why = BAR_TE2_WHY_NONE;
    c->te2.nv_dirty = 1;
}

void bar_te2_set_j58_open(bar_ctx_t *c, int open)
{
    c->te2.j58_open = open ? 1u : 0u;
    c->te2.nv_dirty = 1;
}

/* ---- Persistance --------------------------------------------------------
 * [0..1]  signature 'T','2'
 * [2]     version (1)
 * [3]     drapeaux : bit0 suspendu, bit1 J58 ouvert
 * [4..15] dates des 3 derniers cycles (epoch s, LE)
 * [16..19] total des cycles (LE)
 * [20..21] reserve (0)
 * [22..23] somme de controle (complement a 2 de la somme des octets 0..21)
 */
static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t nv_sum(const uint8_t *p)
{
    uint16_t s = 0;
    unsigned i;
    for (i = 0; i < BAR_TE2_NV_SIZE - 2u; i++) {
        s = (uint16_t)(s + p[i]);
    }
    return (uint16_t)(0u - s);
}

void bar_te2_save(const bar_ctx_t *c, uint8_t out[BAR_TE2_NV_SIZE])
{
    unsigned i;
    uint16_t s;

    memset(out, 0, BAR_TE2_NV_SIZE);
    out[0] = 'T';
    out[1] = '2';
    out[2] = 1u;
    out[3] = (uint8_t)((c->te2.suspended ? 1u : 0u) | (c->te2.j58_open ? 2u : 0u));
    for (i = 0; i < BAR_TE2_MAX_PER_WINDOW; i++) {
        put32(&out[4u + 4u * i], c->te2.cycles_epoch[i]);
    }
    put32(&out[16], c->te2.total_cycles);
    s = nv_sum(out);
    out[22] = (uint8_t)s;
    out[23] = (uint8_t)(s >> 8);
}

int bar_te2_load(bar_ctx_t *c, const uint8_t in[BAR_TE2_NV_SIZE])
{
    unsigned i;
    uint16_t s;

    if (in[0] != 'T' || in[1] != '2' || in[2] != 1u) {
        return 0;
    }
    s = nv_sum(in);
    if (in[22] != (uint8_t)s || in[23] != (uint8_t)(s >> 8)) {
        return 0;
    }
    c->te2.suspended = (uint8_t)((in[3] & 1u) ? 1u : 0u);
    c->te2.j58_open  = (uint8_t)((in[3] & 2u) ? 1u : 0u);
    for (i = 0; i < BAR_TE2_MAX_PER_WINDOW; i++) {
        c->te2.cycles_epoch[i] = get32(&in[4u + 4u * i]);
    }
    c->te2.total_cycles = get32(&in[16]);
    c->te2.nv_dirty = 0;
    return 1;
}

/* ------------------------------------------------------------------------ */
/* Reinitialisation du canal I2C (5.3)                                       */
/* ------------------------------------------------------------------------ */

void *bar_i2c_reopen(const bar_i2c_ops_t *ops, void *current, int *ok)
{
    *ok = 0;
    if (current != NULL) {
        if (ops->close(ops->user, current) == 0) {
            current = NULL;
        }
        /* Fermeture refusee : on garde ce descripteur (pas de second
         * fopen, donc pas de fuite) et on tente de le reconfigurer. */
    }
    if (current == NULL) {
        current = ops->open(ops->user);
        if (current == NULL) {
            return NULL;
        }
    }
    if (ops->configure(ops->user, current) == 0) {
        *ok = 1;
    }
    return current;
}
