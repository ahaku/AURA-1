/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* Browser-only controls. Included by fm1-base's emu_web.c after the app. */

__attribute__((used, visibility("default"))) void web_piece(uint32_t i)
{
    if (i < NPIECES) {
        if ((proj.random_slots >> i) & 1u) {
            proj.random_slots &= (uint16_t)~(1u << i);
            en_piece((int)i, proj.slot[i].par, &proj.slot[i].pool);
            if (proj.piece == (uint8_t)i) {
                proj.is_random = 0;
                take(&proj.slot[i]);
            }
        }
        set_piece((int)i);
    }
}

__attribute__((used, visibility("default"))) void web_random(void)
{
    randomize_world();
}

__attribute__((used, visibility("default"))) uint32_t web_piece_index(void)
{
    return proj.piece;
}

/* Browser storage is instantaneous, so keep its shared project current even
 * while the garden plays (the hardware deliberately postpones flash writes). */
__attribute__((used, visibility("default"))) void web_persist(void)
{
    static project_t saved;
    static int saved_valid;

    keep();
    solo_get_user_preset(&proj.solo_user_preset);
    proj.solo_preset = (uint8_t)solo_get(SP_PRESET);
    if (!saved_valid || memcmp(&saved, &proj, sizeof proj)) {
        if (plat_store_save(OBJ_PROJ, &proj, sizeof proj) == 0) {
            saved = proj;
            saved_valid = 1;
        }
    }
}
