/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 project: one flash object (OBJ_PROJ). A project of another format, or none, gives the
 * defaults. Part of the unity build, after app.h.
 *
 * The pieces (their recipes are the engine's, en_piece): each keeps its own settings as you leave it;
 * Tune and MIDI out are global. */
project_t proj;

const char *piece_name(int i)
{
    if ((proj.random_slots >> (unsigned)(i < NPIECES ? i : 0)) & 1u)
        return "Random";
    if (i == (int)proj.piece && proj.is_random)
        return "Random";
    return en_piece_name(i);
}

static int global_par(int p) { return p == P_TUNE || p == P_MIDI; }

static void recipe(int i, slot_t *s) { en_piece(i, s->par, &s->pool); }

static void take(const slot_t *s)                /* a piece's settings into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
    proj.pool = s->pool;
}

static void keep(void)                           /* the piece playing, into its slot */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = proj.par[p];
    proj.slot[proj.piece].pool = proj.pool;
}

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info(i)->def;
    take(&proj.slot[0]);
    proj.leds = 1;
    solo_get_user_preset(&proj.solo_user_preset);
    proj.solo_preset = 0;
}

static int pool_ok(uint32_t p)                   /* the root and at least two more notes */
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return (p & 1u) && !(p >> 12) && n >= 3;
}

static int pars_ok(const int16_t *par)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo || par[i] > p->hi)
            return 0;
    }
    return 1;
}

static int project_valid(void)                  /* an older format: the recipes again */
{
    int i;
    if (proj.magic != PROJ_MAGIC || !pool_ok(proj.pool) || !pars_ok(proj.par))
        return 0;
    if (proj.format != PROJ_FORMAT || proj.piece >= NPIECES)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pool_ok(proj.slot[i].pool) || !pars_ok(proj.slot[i].par))
            return 0;
    if (proj.solo_preset >= NSOLO_PRESETS)
        return 0;
    return 1;
}

/* format 3 (0.9 before the layer pages): 23 settings a piece. Carried over: each piece's own, the
 * new ones from its recipe */
#define V3_NPARAMS (P_PADTONE + 1)
typedef struct {
    int16_t par[V3_NPARAMS];
    uint16_t pool;
} slot_v3_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V3_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v3_t slot[8];
    uint8_t rsv[16];
} project_v3_t;

static void from_v3(const project_v3_t *o)
{
    slot_t r;
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < 8; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < V3_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (i = 8; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < V3_NPARAMS ? o->par[p] : r.par[p];
    solo_get_user_preset(&proj.solo_user_preset);
    proj.solo_preset = 0;
}

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_t slot[8];
    uint8_t rsv[16];
} project_v4_t;

static void from_v4(const project_v4_t *o)
{
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < 8; i++) {
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = o->slot[i].par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (i = 8; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = o->par[p];
    solo_get_user_preset(&proj.solo_user_preset);
    proj.solo_preset = 0;
}

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, solo_preset, rsv0;
    slot_t slot[8];
    solo_preset_t solo_user_preset;
    uint8_t rsv[16];
} project_v5_t;

static void from_v5(const project_v5_t *o)
{
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < 8; i++) {
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = o->slot[i].par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (i = 8; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = o->par[p];
    proj.solo_user_preset = o->solo_user_preset;
    proj.solo_preset = o->solo_preset < NSOLO_PRESETS ? o->solo_preset : 0;
    proj.is_random = 0;
    proj.random_slots = 0;
}

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, solo_preset, rsv0;
    slot_t slot[9];
    solo_preset_t solo_user_preset;
    uint8_t rsv[16];
} project_v6_t;

static void from_v6(const project_v6_t *o)
{
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < 9; i++) {
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = o->slot[i].par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (i = 9; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = o->par[p];
    proj.solo_user_preset = o->solo_user_preset;
    proj.solo_preset = o->solo_preset < NSOLO_PRESETS ? o->solo_preset : 0;
    proj.is_random = 0;
    proj.random_slots = 0;
}

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, solo_preset, rsv0;
    slot_t slot[10];
    solo_preset_t solo_user_preset;
    uint8_t rsv[16];
} project_v7_t;

static void from_v7(const project_v7_t *o)
{
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < 10; i++) {
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = o->slot[i].par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (i = 10; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = o->par[p];
    proj.solo_user_preset = o->solo_user_preset;
    proj.solo_preset = o->solo_preset < NSOLO_PRESETS ? o->solo_preset : 0;
    proj.is_random = 0;
    proj.random_slots = 0;
}

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, solo_preset, rsv0;
    slot_t slot[11];
    solo_preset_t solo_user_preset;
    uint8_t rsv[16];
} project_v8_t;

static void from_v8(const project_v8_t *o)
{
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < 11; i++) {
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = o->slot[i].par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (i = 11; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = o->par[p];
    proj.solo_user_preset = o->solo_user_preset;
    proj.solo_preset = o->solo_preset < NSOLO_PRESETS ? o->solo_preset : 0;
    proj.is_random = 0;
    proj.random_slots = 0;
}

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, solo_preset, rsv0;
    slot_t slot[12];
    solo_preset_t solo_user_preset;
    uint8_t rsv[16];
} project_v9_t;

static void from_v9(const project_v9_t *o)
{
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < 12; i++) {
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = o->slot[i].par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (i = 12; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = o->par[p];
    proj.solo_user_preset = o->solo_user_preset;
    proj.solo_preset = o->solo_preset < NSOLO_PRESETS ? o->solo_preset : 0;
    proj.is_random = 0;
    proj.random_slots = 0;
}

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, solo_preset, rsv0;
    slot_t slot[13];
    solo_preset_t solo_user_preset;
    uint8_t rsv[16];
} project_v10_t;

static void from_v10(const project_v10_t *o)
{
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < 13; i++) {
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = o->slot[i].par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (i = 13; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = o->par[p];
    proj.solo_user_preset = o->solo_user_preset;
    proj.solo_preset = o->solo_preset < NSOLO_PRESETS ? o->solo_preset : 0;
    proj.is_random = 0;
    proj.random_slots = 0;
}

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, solo_preset, rsv0;
    slot_t slot[14];
    solo_preset_t solo_user_preset;
    uint8_t rsv[16];
} project_v11_t;

static void from_v11(const project_v11_t *o)
{
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < 14; i++) {
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = o->slot[i].par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (i = 14; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = o->par[p];
    proj.solo_user_preset = o->solo_user_preset;
    proj.solo_preset = o->solo_preset < NSOLO_PRESETS ? o->solo_preset : 0;
    proj.is_random = 0;
    proj.random_slots = 0;
}

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, solo_preset, rsv0;
    slot_t slot[15];
    solo_preset_t solo_user_preset;
    uint8_t rsv[16];
} project_v12_t;

static void from_v12(const project_v12_t *o)
{
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < 15; i++) {
        for (p = 0; p < 28; p++)
            proj.slot[i].par[p] = o->slot[i].par[p];
        for (p = 28; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = en_param_info(p)->def;
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (i = 15; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (p = 0; p < 28; p++)
        proj.par[p] = o->par[p];
    for (p = 28; p < P_NPARAMS; p++)
        proj.par[p] = en_param_info(p)->def;
    proj.solo_user_preset = o->solo_user_preset;
    proj.solo_preset = o->solo_preset < NSOLO_PRESETS ? o->solo_preset : 0;
    proj.is_random = 0;
    proj.random_slots = 0;
}

/* format 13: 28 parameters, before atmospheric textures */
#define V13_NPARAMS 28
typedef struct {
    int16_t par[V13_NPARAMS];
    uint16_t pool;
} slot_v13_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V13_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, solo_preset, rsv0;
    slot_v13_t slot[NPIECES];
    solo_preset_t solo_user_preset;
    uint8_t rsv[16];
} project_v13_t;

static void from_v13(const project_v13_t *o)
{
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        for (p = 0; p < V13_NPARAMS; p++)
            proj.slot[i].par[p] = o->slot[i].par[p];
        for (p = V13_NPARAMS; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = en_param_info(p)->def;
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (p = 0; p < V13_NPARAMS; p++)
        proj.par[p] = o->par[p];
    for (p = V13_NPARAMS; p < P_NPARAMS; p++)
        proj.par[p] = en_param_info(p)->def;
    proj.solo_user_preset = o->solo_user_preset;
    proj.solo_preset = o->solo_preset < NSOLO_PRESETS ? o->solo_preset : 0;
    proj.is_random = 0;
    proj.random_slots = 0;
}

/* format 14: 32 parameters, before bowls */
#define V14_NPARAMS 32
typedef struct {
    int16_t par[V14_NPARAMS];
    uint16_t pool;
} slot_v14_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V14_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, solo_preset, rsv0;
    slot_v14_t slot[NPIECES];
    solo_preset_t solo_user_preset;
    uint8_t rsv[16];
} project_v14_t;

static void from_v14(const project_v14_t *o)
{
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        for (p = 0; p < V14_NPARAMS; p++)
            proj.slot[i].par[p] = o->slot[i].par[p];
        for (p = V14_NPARAMS; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = en_param_info(p)->def;
        proj.slot[i].pool = o->slot[i].pool;
    }
    for (p = 0; p < V14_NPARAMS; p++)
        proj.par[p] = o->par[p];
    for (p = V14_NPARAMS; p < P_NPARAMS; p++)
        proj.par[p] = en_param_info(p)->def;
    proj.solo_user_preset = o->solo_user_preset;
    proj.solo_preset = o->solo_preset < NSOLO_PRESETS ? o->solo_preset : 0;
    proj.is_random = 0;
    proj.random_slots = 0;
}

int project_load(void)
{
    static union {
        project_t now;
        project_v14_t v14;
        project_v13_t v13;
        project_v12_t v12;
        project_v11_t v11;
        project_v10_t v10;
        project_v9_t v9;
        project_v8_t v8;
        project_v7_t v7;
        project_v6_t v6;
        project_v5_t v5;
        project_v4_t v4;
        project_v3_t v3;
    } buf;
    int n;
    memset(&buf, 0, sizeof buf);
    n = plat_store_load(OBJ_PROJ, &buf, sizeof buf);
    if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 3u)
        from_v3(&buf.v3);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 4u)
        from_v4(&buf.v4);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 5u)
        from_v5(&buf.v5);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 6u)
        from_v6(&buf.v6);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 7u)
        from_v7(&buf.v7);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 8u)
        from_v8(&buf.v8);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 9u)
        from_v9(&buf.v9);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 10u)
        from_v10(&buf.v10);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 11u)
        from_v11(&buf.v11);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 12u)
        from_v12(&buf.v12);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 13u)
        from_v13(&buf.v13);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 14u)
        from_v14(&buf.v14);
    else
        proj = buf.now;
    int ok = (n >= 8 && project_valid());
    if (!ok)
        project_defaults();
    project_solo_apply();
    return ok ? 0 : -1;
}

int project_save(void)
{
    keep();
    solo_get_user_preset(&proj.solo_user_preset);
    proj.solo_preset = (uint8_t)solo_get(SP_PRESET);
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece]);
    proj.is_random = (uint8_t)((proj.random_slots >> proj.piece) & 1u);
}

void project_piece_reset(void)
{
    slot_t s;
    recipe(proj.piece, &s);
    take(&s);
    proj.random_slots &= (uint16_t)~(1u << proj.piece);
    proj.is_random = 0;
}

void project_solo_apply(void)
{
    solo_set_user_preset(&proj.solo_user_preset);
    solo_load_preset(proj.solo_preset);
}

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    en_pool(proj.pool);
}
