#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static bool hit(Rect2D a, Rect2D b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}
static void banner(Stage *s, const char *msg) {
    snprintf(s->banner, sizeof(s->banner), "%s", msg);
    s->banner_timer = 2.4f;
}

/* ---------- Persistence & GWA Engine ---------- */

bool loadProfile(GameState *g, const char *path) {
    g->best_score = 0; g->best_gwa = 5.00f; g->unlocked_stage = 1;
    g->master_volume = 100; g->sfx_volume = 100;
    FILE *fp = fopen(path, "r");
    if (!fp) return false;
    int sc = 0, st = 1, mv = 100, sv = 100;
    float gw = 5.00f;
    int ok = fscanf(fp, "best_score=%d\nbest_gwa=%f\nunlocked_stage=%d\nmaster_volume=%d\nsfx_volume=%d",
                    &sc, &gw, &st, &mv, &sv);
    fclose(fp);
    if (ok != 5) return false;
    g->best_score = sc < 0 ? 0 : sc;
    g->best_gwa = clampf(gw, 1.00f, 5.00f);
    g->unlocked_stage = clampi(st, 1, TOTAL_STAGES);
    g->master_volume = clampi(mv, 0, 100);
    g->sfx_volume = clampi(sv, 0, 100);
    return true;
}

bool saveProfile(const GameState *g, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return false;
    fprintf(fp, "best_score=%d\nbest_gwa=%.2f\nunlocked_stage=%d\nmaster_volume=%d\nsfx_volume=%d\n",
            g->best_score < 0 ? 0 : g->best_score, (double)clampf(g->best_gwa, 1.00f, 5.00f),
            clampi(g->unlocked_stage, 1, TOTAL_STAGES), clampi(g->master_volume, 0, 100),
            clampi(g->sfx_volume, 0, 100));
    return fclose(fp) == 0;
}

void startNewRun(GameState *g, int start_stage) {
    memset(&g->isko, 0, sizeof(g->isko));
    g->isko.w = 24; g->isko.h = 30; g->isko.gwa = 1.00f; g->isko.facing_right = true;
    g->total_score = 0;
    memset(g->paper_flags, 0, sizeof(g->paper_flags));
    loadStageRoom(g, clampi(start_stage, 1, TOTAL_STAGES));
}

void retakeCurrentStage(GameState *g) {
    g->isko.gwa = g->entry_gwa; g->isko.slips = 0; g->isko.stomp_streak = 0;
    g->isko.papers = g->entry_papers; g->total_score = g->entry_score;
    memcpy(g->paper_flags, g->entry_paper_flags, sizeof(g->paper_flags));
    loadStageRoom(g, g->stage.course_id);
}

void applyPenalty(GameState *g, float dgwa, int dslips) {
    if (g->isko.invuln_timer > 0.0f) return;
    g->isko.slips += dslips;
    g->isko.gwa = clampf(g->isko.gwa + dgwa, 1.00f, 5.00f);
    g->isko.stomp_streak = 0;
    g->isko.invuln_timer = 1.25f;
    if (g->isko.slips >= 3) g->isko.gwa = 5.00f;
    g->sfx |= SFX_HURT;
}

void applyRecovery(GameState *g, float dgwa, int pts) {
    g->isko.gwa = clampf(g->isko.gwa - dgwa, 1.00f, 5.00f);
    g->total_score += pts;
}

const char *laudeTag(float gwa, bool short_tag) {
    if (short_tag && gwa >= 4.999f) return "(NONE)";
    if (gwa <= 1.205f) return short_tag ? "(SUMMA)"     : "[SUMMA CUM LAUDE]";
    if (gwa <= 1.455f) return short_tag ? "(MAGNA)"     : "[MAGNA CUM LAUDE]";
    if (gwa <= 1.755f) return short_tag ? "(CUM LAUDE)" : "[CUM LAUDE]";
    return short_tag ? "(PASSING)" : "[PASSING]";
}

const char *laudeQuote(float gwa) {
    if (gwa <= 1.205f) return "Sablay secured with highest honors! Padayon, Iskolar!";
    if (gwa <= 1.455f) return "Magna Cum Laude standing! Sleep is temporary, honor is forever.";
    if (gwa <= 1.755f) return "Cum Laude! Survived every LE and lab exercise.";
    return "A pass is a pass.";
}

/* ---------- Week 3 Stages (NSTP 1 & Science 10) ---------- */

static void putItem(GameState *g, int kind, float x, float y, int pid) {
    if (kind == 0 && g->paper_flags[pid]) return;
    Stage *s = &g->stage;
    s->items[s->item_count++] = (Collectible){{x, y, 22, kind == 0 ? 26.0f : 22.0f}, kind, false, 0, pid};
}

void loadStageRoom(GameState *g, int id) {
    id = clampi(id, 1, TOTAL_STAGES);
    Stage *s = &g->stage;
    memset(s, 0, sizeof(*s));
    s->course_id = id;
    s->gravity_scale = (id == 1) ? 1.0f : 0.55f;

    g->entry_gwa = g->isko.gwa >= 4.999f ? 1.00f : g->isko.gwa;
    g->entry_score = g->total_score;
    g->entry_papers = g->isko.papers;
    memcpy(g->entry_paper_flags, g->paper_flags, sizeof(g->paper_flags));

    g->isko.vx = g->isko.vy = 0; g->isko.w = 24; g->isko.h = 30;
    g->isko.spawn_x = 48; g->isko.spawn_y = 480;
    g->isko.slips = 0; g->isko.invuln_timer = 0;
    g->isko.is_grounded = g->isko.on_ladder = false;

    if (id == 1) {
        snprintf(s->code, sizeof(s->code), "NSTP 1"); snprintf(s->subtitle, sizeof(s->subtitle), "CWTS");
        snprintf(s->title, sizeof(s->title), "NSTP 1: CWTS - Campus Orientation");
        snprintf(s->hint, sizeof(s->hint), "Push the wooden crate onto the plate, then climb to Exit!");
        s->time_limit = s->time_remaining = 90.0f;

        Rect2D p[3] = {{0, 512, 960, 64}, {140, 412, 220, 20}, {520, 352, 380, 20}};
        memcpy(s->platforms, p, sizeof(p)); s->platform_count = 3;
        s->ladders[0] = (Rect2D){160, 412, 28, 100}; s->ladders[1] = (Rect2D){550, 352, 28, 160}; s->ladder_count = 2;
        s->has_crate = true; s->crate = (Rect2D){290, 480, 30, 30};
        s->plates[0] = (Plate){{420, 502, 42, 10}, 0, false, 0, "CRATE"}; s->plate_count = 1;
        s->exit_terminal = (Rect2D){836, 304, 36, 48};
        s->hazards[0] = (Hazard){{640, 486, 26, 26}, 0, 85, 590, 860, 486, 0, true}; s->hazard_count = 1;
        putItem(g, 0, 240, 374, 0); putItem(g, 1, 96, 478, -1);
    } else {
        snprintf(s->code, sizeof(s->code), "SCI 10"); snprintf(s->subtitle, sizeof(s->subtitle), "LOW-GRAV");
        snprintf(s->title, sizeof(s->title), "Science 10: Low-Gravity Physics Lab");
        snprintf(s->hint, sizeof(s->hint), "0.55x Gravity! Trigger Plate [1] then Plate [2] for Exit!");
        s->time_limit = s->time_remaining = 95.0f;

        Rect2D p[8] = {
            {0, 512, 300, 64}, {420, 512, 220, 64}, {740, 512, 220, 64},
            {200, 404, 160, 20}, {440, 308, 180, 20}, {170, 212, 180, 20},
            {470, 412, 120, 20}, {690, 240, 230, 20}
        };
        memcpy(s->platforms, p, sizeof(p)); s->platform_count = 8;
        s->plates[0] = (Plate){{510, 298, 42, 10}, 1, false, 0, "STEP 1"};
        s->plates[1] = (Plate){{720, 230, 42, 10}, 2, false, 0, "STEP 2"};
        s->plate_count = 2;
        s->exit_terminal = (Rect2D){860, 192, 36, 48};
        s->hazards[0] = (Hazard){{370, 336, 26, 26}, 1, 65, 310, 430, 336, 0, true};
        s->hazards[1] = (Hazard){{635, 250, 26, 26}, 1, 70, 595, 680, 250, 0, true};
        s->hazard_count = 2;
        putItem(g, 0, 245, 174, 1); putItem(g, 0, 520, 374, 2); putItem(g, 1, 515, 478, -1);
    }

    g->isko.x = g->isko.spawn_x; g->isko.y = g->isko.spawn_y;
}

/* ---------- Gameplay Loop ---------- */

void updateStageGameplay(GameState *g, const InputState *in, float dt) {
    Stage *s = &g->stage;
    Player *p = &g->isko;
    if (dt > 0.05f) dt = 0.05f;
    if (s->banner_timer > 0) s->banner_timer -= dt;
    if (p->invuln_timer > 0) p->invuln_timer -= dt;

    s->time_remaining -= dt;
    if (s->time_remaining <= 0.0f) {
        s->time_remaining = 0; p->gwa = 5.00f; p->slips = 3;
        g->sfx |= SFX_GAMEOVER; g->over_cur = 0; g->scene = SCENE_GAMEOVER;
        return;
    }

    Rect2D pr = {p->x, p->y, p->w, p->h};
    bool on_lad = false;
    for (int i = 0; i < s->ladder_count; i++) if (hit(pr, s->ladders[i])) on_lad = true;
    if (on_lad && (in->up || in->down)) { p->on_ladder = true; p->vy = 0; }
    else if (!on_lad) p->on_ladder = false;

    p->vx = in->left ? -215.0f : (in->right ? 215.0f : 0.0f);
    if (in->left) p->facing_right = false;
    if (in->right) p->facing_right = true;

    float grav = 980.0f * s->gravity_scale;
    float jspd = s->gravity_scale < 0.8f ? -375.0f : -435.0f;
    if (p->on_ladder) {
        p->vy = in->up ? -165.0f : (in->down ? 165.0f : 0.0f);
        if (in->jump && !in->up) { p->on_ladder = false; p->vy = jspd; g->sfx |= SFX_JUMP; }
    } else {
        if (in->jump && p->is_grounded) { p->vy = jspd; p->is_grounded = false; g->sfx |= SFX_JUMP; }
        p->vy = clampf(p->vy + grav * dt, -800.0f, 680.0f);
    }

    /* Horizontal movement + Crate pushing */
    float px = p->x;
    p->x = clampf(p->x + p->vx * dt, 4.0f, (float)SCREEN_W - 4.0f - p->w);
    pr.x = p->x;
    for (int i = 0; i < s->platform_count; i++) {
        Rect2D br = s->platforms[i];
        if (hit(pr, br) && br.h >= 24.0f) {
            p->x = (px + p->w <= br.x + 2) ? br.x - p->w : br.x + br.w;
            pr.x = p->x;
        }
    }
    if (s->has_crate) {
        if (hit(pr, s->crate)) {
            if (px + p->w <= s->crate.x + 6) s->crate.x += (p->x + p->w) - s->crate.x;
            else if (px >= s->crate.x + s->crate.w - 6) s->crate.x -= (s->crate.x + s->crate.w) - p->x;
            s->crate.x = clampf(s->crate.x, 8.0f, (float)SCREEN_W - 8.0f - s->crate.w);
        }
        s->crate_vy += grav * dt; s->crate.y += s->crate_vy * dt;
        for (int j = 0; j < s->platform_count; j++)
            if (hit(s->crate, s->platforms[j]) && s->crate_vy >= 0) {
                s->crate.y = s->platforms[j].y - s->crate.h; s->crate_vy = 0;
            }
    }

    /* Vertical movement (one-way thin platforms) */
    float py = p->y;
    p->y = fmaxf(p->y + p->vy * dt, (float)HUD_H);
    p->is_grounded = false;
    pr = (Rect2D){p->x, p->y, p->w, p->h};
    if (!p->on_ladder || p->vy >= 0) {
        for (int i = 0; i < s->platform_count; i++) {
            Rect2D br = s->platforms[i];
            if (!hit(pr, br)) continue;
            if (p->vy >= 0 && py + p->h <= br.y + 8) { p->y = br.y - p->h; p->vy = 0; p->is_grounded = true; pr.y = p->y; }
            else if (br.h >= 24 && p->vy < 0 && py >= br.y + br.h - 6) { p->y = br.y + br.h; p->vy = 0; pr.y = p->y; }
        }
    }
    if (p->y > (float)SCREEN_H + 12.0f) {
        applyPenalty(g, 0.75f, 1); p->x = p->spawn_x; p->y = p->spawn_y; p->vx = p->vy = 0;
        banner(s, "FELL INTO PIT! +0.75 GWA & 1 Slip!");
    }
    if ((p->anim_timer += dt) >= 0.12f) { p->anim_frame = (p->anim_frame + 1) % 3; p->anim_timer = 0; }

    /* Collectibles & Hazards */
    pr = (Rect2D){p->x, p->y, p->w, p->h};
    for (int i = 0; i < s->item_count; i++) {
        Collectible *c = &s->items[i];
        c->phase += dt * 4.0f;
        if (c->collected || !hit(pr, c->rect)) continue;
        c->collected = true;
        if (c->kind == 0) {
            if (!g->paper_flags[c->paper_id]) { g->paper_flags[c->paper_id] = true; p->papers++; }
            applyRecovery(g, 0.05f, 500); g->sfx |= SFX_PAPER;
            banner(s, "PAPER COLLECTED! +500 PTS & -0.05 GWA!");
        } else {
            applyRecovery(g, 0.25f, 150); g->sfx |= SFX_COFFEE;
            banner(s, "UP COFFEE! -0.25 GWA Recovery!");
        }
    }
    for (int i = 0; i < s->hazard_count; i++) {
        Hazard *h = &s->hazards[i];
        if (!h->alive) continue;
        h->phase += dt * 3.0f; h->rect.x += h->vx * dt;
        if (h->rect.x <= h->min_x) { h->rect.x = h->min_x; h->vx = fabsf(h->vx); }
        if (h->rect.x + h->rect.w >= h->max_x) { h->rect.x = h->max_x - h->rect.w; h->vx = -fabsf(h->vx); }
        if (h->kind == 1) h->rect.y = h->base_y + sinf(h->phase) * 28.0f;
        if (hit(pr, h->rect)) {
            if (h->kind == 0 && p->vy > 0 && py + p->h <= h->rect.y + 14) {
                h->alive = false; p->vy = -310; p->stomp_streak++;
                applyRecovery(g, p->stomp_streak >= 2 ? 0.25f : 0.10f, 100 * p->stomp_streak);
                g->sfx |= SFX_STOMP; banner(s, "STRESS CLEARED! GWA Improved!");
            } else if (p->invuln_timer <= 0) { applyPenalty(g, 0.50f, 1); p->vy = -220; }
        }
    }

    /* Stage Puzzles */
    for (int i = 0; i < s->plate_count; i++) {
        Plate *pl = &s->plates[i];
        if (pl->cooldown > 0) pl->cooldown -= dt;
        Rect2D z = {pl->rect.x, pl->rect.y - 6, pl->rect.w, pl->rect.h + 8};
        if (pl->type == 0 && !pl->active && s->has_crate && hit(s->crate, z)) {
            pl->active = s->terminal_online = true; g->sfx |= SFX_SWITCH;
            banner(s, "TERMINAL ONLINE! Head to the Exit!");
        } else if (pl->type == 1 && !pl->active && hit(pr, z)) {
            pl->active = true; s->seq_step = 1; g->sfx |= SFX_SWITCH;
            banner(s, "Plate [1] Online! Activate Plate [2]!");
        } else if (pl->type == 2 && !pl->active && hit(pr, z)) {
            if (s->seq_step >= 1) {
                pl->active = s->terminal_online = true; s->seq_step = 2; g->sfx |= SFX_SWITCH;
                banner(s, "SEQUENCE COMPLETE! Lab Exit ONLINE!");
            } else if (pl->cooldown <= 0) { pl->cooldown = 1.5f; banner(s, "Activate Plate [1] first!"); }
        }
    }

    if (p->gwa >= 4.999f || p->slips >= 3) {
        p->gwa = 5.00f; g->sfx |= SFX_GAMEOVER; g->over_cur = 0; g->scene = SCENE_GAMEOVER;
    } else if (s->terminal_online && hit(pr, s->exit_terminal)) {
        g->total_score += (int)floorf(s->time_remaining) * 10 + (int)roundf((5.00f - p->gwa) * 150.0f);
        g->sfx |= SFX_CLEAR;
        if (s->course_id >= TOTAL_STAGES) {
            if (g->total_score > g->best_score) g->best_score = g->total_score;
            if (p->gwa < g->best_gwa) g->best_gwa = p->gwa;
            g->unlocked_stage = TOTAL_STAGES; saveProfile(g, SAVE_PATH); g->scene = SCENE_VICTORY;
        } else {
            if (s->course_id + 1 > g->unlocked_stage) { g->unlocked_stage = s->course_id + 1; saveProfile(g, SAVE_PATH); }
            loadStageRoom(g, s->course_id + 1); g->scene = SCENE_BRIEFING;
        }
    }
}
