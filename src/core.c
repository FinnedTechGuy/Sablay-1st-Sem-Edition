#include "game.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int clampInt(int value, int min_val, int max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

static float clampFloat(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

static bool boxesOverlap(Rect2D a, Rect2D b) {
    bool overlap_x = (a.x < b.x + b.w) && (a.x + a.w > b.x);
    bool overlap_y = (a.y < b.y + b.h) && (a.y + a.h > b.y);
    return overlap_x && overlap_y;
}

static void showBanner(Stage *stage, const char *message) {
    snprintf(stage->banner, sizeof(stage->banner), "%s", message);
    stage->banner_timer = 2.4f;
}

/* ---------- Save / Load & GWA Rules ---------- */

bool loadProfile(GameState *game, const char *path) {
    game->best_score = 0;
    game->best_gwa = 5.00f;
    game->unlocked_stage = 1;
    game->master_volume = 100;
    game->sfx_volume = 100;

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return false;
    }

    int score = 0;
    int stage = 1;
    int master = 100;
    int sfx = 100;
    float gwa = 5.00f;

    int read_count = fscanf(
        file,
        "best_score=%d\nbest_gwa=%f\nunlocked_stage=%d\nmaster_volume=%d\nsfx_volume=%d",
        &score, &gwa, &stage, &master, &sfx
    );
    fclose(file);

    if (read_count != 5) {
        return false;
    }

    if (score < 0) score = 0;
    game->best_score = score;
    game->best_gwa = clampFloat(gwa, 1.00f, 5.00f);
    game->unlocked_stage = clampInt(stage, 1, TOTAL_STAGES);
    game->master_volume = clampInt(master, 0, 100);
    game->sfx_volume = clampInt(sfx, 0, 100);
    return true;
}

bool saveProfile(const GameState *game, const char *path) {
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        return false;
    }

    int score = (game->best_score < 0) ? 0 : game->best_score;
    fprintf(
        file,
        "best_score=%d\nbest_gwa=%.2f\nunlocked_stage=%d\nmaster_volume=%d\nsfx_volume=%d\n",
        score,
        (double)clampFloat(game->best_gwa, 1.00f, 5.00f),
        clampInt(game->unlocked_stage, 1, TOTAL_STAGES),
        clampInt(game->master_volume, 0, 100),
        clampInt(game->sfx_volume, 0, 100)
    );
    return fclose(file) == 0;
}

void startNewRun(GameState *game, int start_stage) {
    memset(&game->isko, 0, sizeof(game->isko));
    game->isko.w = 24.0f;
    game->isko.h = 30.0f;
    game->isko.gwa = 1.00f;
    game->isko.facing_right = true;
    game->total_score = 0;
    memset(game->paper_flags, 0, sizeof(game->paper_flags));

    loadStageRoom(game, clampInt(start_stage, 1, TOTAL_STAGES));
}

void retakeCurrentStage(GameState *game) {
    game->isko.gwa = game->entry_gwa;
    game->isko.slips = 0;
    game->isko.stomp_streak = 0;
    game->isko.papers = game->entry_papers;
    game->total_score = game->entry_score;
    memcpy(game->paper_flags, game->entry_paper_flags, sizeof(game->paper_flags));

    loadStageRoom(game, game->stage.course_id);
}

void applyPenalty(GameState *game, float gwa_delta, int slip_delta) {
    if (game->isko.invuln_timer > 0.0f) {
        return;
    }

    game->isko.slips += slip_delta;
    game->isko.gwa = clampFloat(game->isko.gwa + gwa_delta, 1.00f, 5.00f);
    game->isko.stomp_streak = 0;
    game->isko.invuln_timer = 1.25f;

    if (game->isko.slips >= 3) {
        game->isko.gwa = 5.00f;
    }
    game->play_sfx[SFX_HURT] = true;
}

void applyRecovery(GameState *game, float gwa_delta, int points) {
    game->isko.gwa = clampFloat(game->isko.gwa - gwa_delta, 1.00f, 5.00f);
    game->total_score += points;
}

const char *laudeTag(float gwa, bool short_tag) {
    if (short_tag && gwa >= 4.999f) return "(NONE)";
    if (gwa <= 1.205f) return short_tag ? "(SUMMA)" : "[SUMMA CUM LAUDE]";
    if (gwa <= 1.455f) return short_tag ? "(MAGNA)" : "[MAGNA CUM LAUDE]";
    if (gwa <= 1.755f) return short_tag ? "(CUM LAUDE)" : "[CUM LAUDE]";
    return short_tag ? "(PASSING)" : "[PASSING]";
}

const char *laudeQuote(float gwa) {
    if (gwa <= 1.205f) return "Sablay secured with highest honors! Padayon, Iskolar!";
    if (gwa <= 1.455f) return "Magna Cum Laude standing! Sleep is temporary, honor is forever.";
    if (gwa <= 1.755f) return "Cum Laude! Survived every LE and lab exercise.";
    return "A pass is a pass.";
}

/* ---------- Stages 1-5 Setup & Helpers ---------- */

bool evaluateLogicCircuit(const Stage *stage) {
    bool a = stage->logic_inputs[0];
    bool b = stage->logic_inputs[1];
    bool c = stage->logic_inputs[2];
    return (a && b) || (!c);
}

static void addPlatform(Stage *stage, float x, float y, float w, float h) {
    if (stage->platform_count >= 12) return;
    Platform *plat = &stage->platforms[stage->platform_count++];
    memset(plat, 0, sizeof(*plat));
    plat->rect = (Rect2D){x, y, w, h};
}

static void addMovingPlatform(Stage *stage, float x, float y, float w, float h,
                              float min_x, float max_x, float speed, float dir) {
    if (stage->platform_count >= 12) return;
    Platform *plat = &stage->platforms[stage->platform_count++];
    memset(plat, 0, sizeof(*plat));
    plat->rect = (Rect2D){x, y, w, h};
    plat->is_moving = true;
    plat->min_x = min_x;
    plat->max_x = max_x;
    plat->move_speed = speed;
    plat->move_dir = dir;
}

static void addHurdle(Stage *stage, float x, float y, float w, float h) {
    if (stage->platform_count >= 12) return;
    Platform *plat = &stage->platforms[stage->platform_count++];
    memset(plat, 0, sizeof(*plat));
    plat->rect = (Rect2D){x, y, w, h};
    plat->is_hurdle = true;
}

static void addCollectible(GameState *game, int kind, float x, float y, int paper_id) {
    if (kind == 0 && game->paper_flags[paper_id]) {
        return;
    }

    Stage *stage = &game->stage;
    float height = (kind == 0) ? 26.0f : 22.0f;
    stage->items[stage->item_count] = (Collectible){
        .rect = {x, y, 22.0f, height},
        .kind = kind,
        .collected = false,
        .phase = 0.0f,
        .paper_id = paper_id
    };
    stage->item_count++;
}

void loadStageRoom(GameState *game, int course_id) {
    course_id = clampInt(course_id, 1, TOTAL_STAGES);
    Stage *stage = &game->stage;
    memset(stage, 0, sizeof(*stage));

    stage->course_id = course_id;
    stage->gravity_scale = (course_id == 2) ? 0.55f : 1.0f;

    /* Snapshot player stats so RETAKE COURSE can rewind cleanly */
    game->entry_gwa = (game->isko.gwa >= 4.999f) ? 1.00f : game->isko.gwa;
    game->entry_score = game->total_score;
    game->entry_papers = game->isko.papers;
    memcpy(game->entry_paper_flags, game->paper_flags, sizeof(game->paper_flags));

    game->isko.vx = 0.0f;
    game->isko.vy = 0.0f;
    game->isko.w = 24.0f;
    game->isko.h = 30.0f;
    game->isko.spawn_x = 48.0f;
    game->isko.spawn_y = 480.0f;
    game->isko.slips = 0;
    game->isko.invuln_timer = 0.0f;
    game->isko.is_grounded = false;
    game->isko.on_ladder = false;

    if (course_id == 1) {
        snprintf(stage->code, sizeof(stage->code), "NSTP 1");
        snprintf(stage->subtitle, sizeof(stage->subtitle), "CWTS");
        snprintf(stage->title, sizeof(stage->title), "NSTP 1: CWTS - Campus Orientation");
        snprintf(stage->hint, sizeof(stage->hint), "Push the wooden crate onto the plate, then climb to Exit!");
        stage->time_limit = 90.0f;
        stage->time_remaining = 90.0f;

        addPlatform(stage, 0, 512, 960, 64);
        addPlatform(stage, 140, 412, 220, 20);
        addPlatform(stage, 520, 352, 380, 20);

        stage->ladders[0] = (Rect2D){160, 412, 28, 100};
        stage->ladders[1] = (Rect2D){550, 352, 28, 160};
        stage->ladder_count = 2;

        stage->has_crate = true;
        stage->crate = (Rect2D){290, 480, 30, 30};

        stage->plates[0] = (Plate){.rect = {420, 502, 42, 10}, .type = 0, .label = "CRATE"};
        stage->plate_count = 1;

        stage->exit_terminal = (Rect2D){836, 304, 36, 48};
        stage->hazards[0] = (Hazard){{640, 486, 26, 26}, 0, 85.0f, 590.0f, 860.0f, 486.0f, 0.0f, true};
        stage->hazard_count = 1;

        addCollectible(game, 0, 240, 374, 0);
        addCollectible(game, 1, 96, 478, -1);
    } else if (course_id == 2) {
        snprintf(stage->code, sizeof(stage->code), "SCI 10");
        snprintf(stage->subtitle, sizeof(stage->subtitle), "LOW-GRAV");
        snprintf(stage->title, sizeof(stage->title), "Science 10: Low-Gravity Physics Lab");
        snprintf(stage->hint, sizeof(stage->hint), "0.55x Gravity! Trigger Plate [1] then Plate [2] for Exit!");
        stage->time_limit = 95.0f;
        stage->time_remaining = 95.0f;

        addPlatform(stage, 0, 512, 300, 64);
        addPlatform(stage, 420, 512, 220, 64);
        addPlatform(stage, 740, 512, 220, 64);
        addPlatform(stage, 200, 404, 160, 20);
        addPlatform(stage, 440, 308, 180, 20);
        addPlatform(stage, 170, 212, 180, 20);
        addPlatform(stage, 470, 412, 120, 20);
        addPlatform(stage, 690, 240, 230, 20);

        stage->plates[0] = (Plate){.rect = {510, 298, 42, 10}, .type = 1, .label = "STEP 1"};
        stage->plates[1] = (Plate){.rect = {720, 230, 42, 10}, .type = 2, .label = "STEP 2"};
        stage->plate_count = 2;

        stage->exit_terminal = (Rect2D){860, 192, 36, 48};
        stage->hazards[0] = (Hazard){{370, 336, 26, 26}, 1, 65.0f, 310.0f, 430.0f, 336.0f, 0.0f, true};
        stage->hazards[1] = (Hazard){{635, 250, 26, 26}, 1, 70.0f, 595.0f, 680.0f, 250.0f, 0.0f, true};
        stage->hazard_count = 2;

        addCollectible(game, 0, 245, 174, 1);
        addCollectible(game, 0, 520, 374, 2);
        addCollectible(game, 1, 515, 478, -1);
    } else if (course_id == 3) {
        snprintf(stage->code, sizeof(stage->code), "MATH 18");
        snprintf(stage->subtitle, sizeof(stage->subtitle), "CALCULUS");
        snprintf(stage->title, sizeof(stage->title), "Math 18: Solve x^2 - 5x + 6 = 0");
        snprintf(stage->hint, sizeof(stage->hint), "Jump over false root x=1! Step on the two true roots.");
        stage->time_limit = 100.0f;
        stage->time_remaining = 100.0f;

        addPlatform(stage, 0, 512, 960, 64);
        addPlatform(stage, 100, 396, 150, 20);
        addPlatform(stage, 350, 356, 190, 20);

        stage->ladders[0] = (Rect2D){130, 396, 28, 116};
        stage->ladders[1] = (Rect2D){390, 356, 28, 156};
        stage->ladder_count = 2;

        stage->plates[0] = (Plate){.rect = {230, 502, 42, 10}, .type = 3, .is_correct = false, .value = 1, .label = "x = 1"};
        stage->plates[1] = (Plate){.rect = {460, 502, 42, 10}, .type = 3, .is_correct = true,  .value = 2, .label = "x = 2"};
        stage->plates[2] = (Plate){.rect = {690, 502, 42, 10}, .type = 3, .is_correct = true,  .value = 3, .label = "x = 3"};
        stage->plate_count = 3;

        stage->exit_terminal = (Rect2D){870, 464, 36, 48};
        stage->hazards[0] = (Hazard){{290, 486, 26, 26}, 0, 75.0f, 290.0f, 440.0f, 486.0f, 0.0f, true};
        stage->hazards[1] = (Hazard){{520, 486, 26, 26}, 0, -75.0f, 520.0f, 670.0f, 486.0f, 0.0f, true};
        stage->hazard_count = 2;

        addCollectible(game, 1, 180, 364, -1);
        addCollectible(game, 0, 470, 318, 3);
    } else if (course_id == 4) {
        snprintf(stage->code, sizeof(stage->code), "CMSC 56");
        snprintf(stage->subtitle, sizeof(stage->subtitle), "DISCRETE");
        snprintf(stage->title, sizeof(stage->title), "CMSC 56: Discrete Logic Circuits");
        snprintf(stage->hint, sizeof(stage->hint), "Make OUT = (A AND B) OR (NOT C) = 1! Step or press E.");
        stage->time_limit = 110.0f;
        stage->time_remaining = 110.0f;

        stage->logic_inputs[0] = false;
        stage->logic_inputs[1] = false;
        stage->logic_inputs[2] = true;

        addPlatform(stage, 0, 512, 960, 64);
        addPlatform(stage, 80, 392, 220, 20);
        addPlatform(stage, 320, 332, 270, 20);
        addPlatform(stage, 620, 392, 240, 20);

        stage->ladders[0] = (Rect2D){100, 392, 28, 120};
        stage->ladders[1] = (Rect2D){640, 392, 28, 120};
        stage->ladders[2] = (Rect2D){340, 332, 28, 180};
        stage->ladder_count = 3;

        stage->plates[0] = (Plate){.rect = {190, 382, 54, 10}, .type = 4, .active = false, .value = 0, .label = "IN A=0"};
        stage->plates[1] = (Plate){.rect = {730, 382, 54, 10}, .type = 4, .active = false, .value = 1, .label = "IN B=0"};
        stage->plates[2] = (Plate){.rect = {450, 322, 54, 10}, .type = 4, .active = true,  .value = 2, .label = "IN C=1"};
        stage->plate_count = 3;

        stage->exit_terminal = (Rect2D){870, 464, 36, 48};
        stage->hazards[0] = (Hazard){{200, 486, 26, 26}, 0, 80.0f, 150.0f, 420.0f, 486.0f, 0.0f, true};
        stage->hazards[1] = (Hazard){{560, 486, 26, 26}, 0, -80.0f, 460.0f, 780.0f, 486.0f, 0.0f, true};
        stage->hazard_count = 2;

        addCollectible(game, 0, 118, 354, 4);
        addCollectible(game, 0, 814, 354, 5);
        addCollectible(game, 1, 540, 298, -1);
    } else {
        snprintf(stage->code, sizeof(stage->code), "P.E. 1");
        snprintf(stage->subtitle, sizeof(stage->subtitle), "AGILITY");
        snprintf(stage->title, sizeof(stage->title), "P.E. 1: Hurdles & Agility Course");
        snprintf(stage->hint, sizeof(stage->hint), "Jump hurdles, ring the balcony BELL, cross the pit in 70s!");
        stage->time_limit = 70.0f;
        stage->time_remaining = 70.0f;

        addPlatform(stage, 0, 512, 460, 64);
        addPlatform(stage, 740, 512, 220, 64);
        addPlatform(stage, 120, 372, 290, 20);
        addMovingPlatform(stage, 470, 412, 90, 18, 470.0f, 630.0f, 80.0f, 1.0f);
        addMovingPlatform(stage, 635, 456, 90, 18, 420.0f, 635.0f, 95.0f, -1.0f);
        addHurdle(stage, 160, 484, 14, 28);
        addHurdle(stage, 290, 484, 14, 28);

        stage->ladders[0] = (Rect2D){370, 372, 28, 140};
        stage->ladder_count = 1;

        stage->plates[0] = (Plate){.rect = {210, 362, 46, 10}, .type = 5, .label = "BELL"};
        stage->plate_count = 1;

        stage->exit_terminal = (Rect2D){870, 464, 36, 48};
        stage->hazards[0] = (Hazard){{200, 486, 26, 26}, 0, 65.0f, 185.0f, 280.0f, 486.0f, 0.0f, true};
        stage->hazards[1] = (Hazard){{760, 486, 26, 26}, 0, 70.0f, 750.0f, 850.0f, 486.0f, 0.0f, true};
        stage->hazard_count = 2;

        addCollectible(game, 0, 160, 334, 6);
        addCollectible(game, 1, 325, 338, -1);
    }

    game->isko.x = game->isko.spawn_x;
    game->isko.y = game->isko.spawn_y;
}

/* ---------- Gameplay Helpers (One Job Each) ---------- */

static void handleLadderAndJump(GameState *game, const InputState *input, float gravity, float dt) {
    Stage *stage = &game->stage;
    Player *player = &game->isko;
    Rect2D player_box = {player->x, player->y, player->w, player->h};

    bool touching_ladder = false;
    for (int i = 0; i < stage->ladder_count; i++) {
        if (boxesOverlap(player_box, stage->ladders[i])) {
            touching_ladder = true;
        }
    }

    if (touching_ladder && (input->up || input->down)) {
        player->on_ladder = true;
        player->vy = 0.0f;
    } else if (!touching_ladder) {
        player->on_ladder = false;
    }

    if (input->left) {
        player->vx = -215.0f;
        player->facing_right = false;
    } else if (input->right) {
        player->vx = 215.0f;
        player->facing_right = true;
    } else {
        player->vx = 0.0f;
    }

    float jump_speed = (stage->gravity_scale < 0.8f) ? -375.0f : -435.0f;

    if (player->on_ladder) {
        if (input->up) player->vy = -165.0f;
        else if (input->down) player->vy = 165.0f;
        else player->vy = 0.0f;

        if (input->jump && !input->up) {
            player->on_ladder = false;
            player->vy = jump_speed;
            game->play_sfx[SFX_JUMP] = true;
        }
    } else {
        if (input->jump && player->is_grounded) {
            player->vy = jump_speed;
            player->is_grounded = false;
            game->play_sfx[SFX_JUMP] = true;
        }
        player->vy = clampFloat(player->vy + gravity * dt, -800.0f, 680.0f);
    }
}

static void updateMovingPlatforms(Stage *stage, Player *player, float dt) {
    for (int i = 0; i < stage->platform_count; i++) {
        Platform *plat = &stage->platforms[i];
        if (!plat->is_moving) {
            continue;
        }

        float old_x = plat->rect.x;
        plat->rect.x += plat->move_dir * plat->move_speed * dt;
        if (plat->rect.x <= plat->min_x) {
            plat->rect.x = plat->min_x;
            plat->move_dir = 1.0f;
        } else if (plat->rect.x >= plat->max_x) {
            plat->rect.x = plat->max_x;
            plat->move_dir = -1.0f;
        }

        float delta_x = plat->rect.x - old_x;
        bool on_top = player->is_grounded &&
                      (player->x + player->w > old_x) &&
                      (player->x < old_x + plat->rect.w) &&
                      (fabsf((player->y + player->h) - plat->rect.y) <= 4.0f);
        if (on_top) {
            player->x = clampFloat(player->x + delta_x, 4.0f, (float)SCREEN_W - 4.0f - player->w);
        }
    }
}

static void movePlayerAndCrateX(Stage *stage, Player *player, float gravity, float dt) {
    float prev_x = player->x;
    player->x = clampFloat(player->x + player->vx * dt, 4.0f, (float)SCREEN_W - 4.0f - player->w);
    Rect2D player_box = {player->x, player->y, player->w, player->h};

    for (int i = 0; i < stage->platform_count; i++) {
        Rect2D plat = stage->platforms[i].rect;
        if (boxesOverlap(player_box, plat) && plat.h >= 24.0f) {
            if (prev_x + player->w <= plat.x + 2.0f) {
                player->x = plat.x - player->w;
            } else {
                player->x = plat.x + plat.w;
            }
            player_box.x = player->x;
        }
    }

    if (!stage->has_crate) {
        return;
    }

    if (boxesOverlap(player_box, stage->crate)) {
        if (prev_x + player->w <= stage->crate.x + 6.0f) {
            stage->crate.x = player->x + player->w;
        } else if (prev_x >= stage->crate.x + stage->crate.w - 6.0f) {
            stage->crate.x = player->x - stage->crate.w;
        }
        stage->crate.x = clampFloat(stage->crate.x, 8.0f, (float)SCREEN_W - 8.0f - stage->crate.w);
    }

    stage->crate_vy += gravity * dt;
    stage->crate.y += stage->crate_vy * dt;
    for (int i = 0; i < stage->platform_count; i++) {
        Rect2D plat = stage->platforms[i].rect;
        if (boxesOverlap(stage->crate, plat) && stage->crate_vy >= 0.0f) {
            stage->crate.y = plat.y - stage->crate.h;
            stage->crate_vy = 0.0f;
        }
    }
}

static void movePlayerY(GameState *game, Stage *stage, Player *player, float prev_y, float dt) {
    player->y = fmaxf(player->y + player->vy * dt, (float)HUD_H);
    player->is_grounded = false;
    Rect2D player_box = {player->x, player->y, player->w, player->h};

    if (!player->on_ladder || player->vy >= 0.0f) {
        for (int i = 0; i < stage->platform_count; i++) {
            Rect2D plat = stage->platforms[i].rect;
            if (!boxesOverlap(player_box, plat)) {
                continue;
            }
            if (player->vy >= 0.0f && prev_y + player->h <= plat.y + 8.0f) {
                player->y = plat.y - player->h;
                player->vy = 0.0f;
                player->is_grounded = true;
                player_box.y = player->y;
            } else if (plat.h >= 24.0f && player->vy < 0.0f && prev_y >= plat.y + plat.h - 6.0f) {
                player->y = plat.y + plat.h;
                player->vy = 0.0f;
                player_box.y = player->y;
            }
        }
    }

    if (player->y > (float)SCREEN_H + 12.0f) {
        applyPenalty(game, 0.75f, 1);
        player->x = player->spawn_x;
        player->y = player->spawn_y;
        player->vx = 0.0f;
        player->vy = 0.0f;
        showBanner(stage, "FELL INTO PIT! +0.75 GWA & 1 Slip!");
    }

    player->anim_timer += dt;
    if (player->anim_timer >= 0.12f) {
        player->anim_frame = (player->anim_frame + 1) % 2;
        player->anim_timer = 0.0f;
    }
}

static void updateCollectibles(GameState *game, Stage *stage, Player *player, float dt) {
    Rect2D player_box = {player->x, player->y, player->w, player->h};

    for (int i = 0; i < stage->item_count; i++) {
        Collectible *item = &stage->items[i];
        item->phase += dt * 4.0f;

        if (item->collected || !boxesOverlap(player_box, item->rect)) {
            continue;
        }

        item->collected = true;
        if (item->kind == 0) {
            if (!game->paper_flags[item->paper_id]) {
                game->paper_flags[item->paper_id] = true;
                player->papers++;
            }
            applyRecovery(game, 0.05f, 500);
            game->play_sfx[SFX_PAPER] = true;
            showBanner(stage, "PAPER COLLECTED! +500 PTS & -0.05 GWA!");
        } else {
            applyRecovery(game, 0.25f, 150);
            game->play_sfx[SFX_COFFEE] = true;
            showBanner(stage, "UP COFFEE! -0.25 GWA Recovery!");
        }
    }
}

static void updateHazards(GameState *game, Stage *stage, Player *player, float prev_y, float dt) {
    Rect2D player_box = {player->x, player->y, player->w, player->h};

    for (int i = 0; i < stage->hazard_count; i++) {
        Hazard *haz = &stage->hazards[i];
        if (!haz->alive) {
            continue;
        }

        haz->phase += dt * 3.0f;
        haz->rect.x += haz->vx * dt;

        if (haz->rect.x <= haz->min_x) {
            haz->rect.x = haz->min_x;
            haz->vx = fabsf(haz->vx);
        }
        if (haz->rect.x + haz->rect.w >= haz->max_x) {
            haz->rect.x = haz->max_x - haz->rect.w;
            haz->vx = -fabsf(haz->vx);
        }
        if (haz->kind == 1) {
            haz->rect.y = haz->base_y + sinf(haz->phase) * 28.0f;
        }

        if (!boxesOverlap(player_box, haz->rect)) {
            continue;
        }

        bool stomped_from_above = (haz->kind == 0) && (player->vy > 0.0f) && (prev_y + player->h <= haz->rect.y + 14.0f);
        if (stomped_from_above) {
            haz->alive = false;
            player->vy = -310.0f;
            player->stomp_streak++;
            float heal = (player->stomp_streak >= 2) ? 0.25f : 0.10f;
            applyRecovery(game, heal, 100 * player->stomp_streak);
            game->play_sfx[SFX_STOMP] = true;
            showBanner(stage, "STRESS CLEARED! GWA Improved!");
        } else if (player->invuln_timer <= 0.0f) {
            applyPenalty(game, 0.50f, 1);
            player->vy = -220.0f;
        }
    }
}

static void checkPlatesAndExit(GameState *game, Stage *stage, Player *player, const InputState *input, float dt) {
    Rect2D player_box = {player->x, player->y, player->w, player->h};

    for (int i = 0; i < stage->plate_count; i++) {
        Plate *plate = &stage->plates[i];
        if (plate->cooldown > 0.0f) {
            plate->cooldown -= dt;
        }

        Rect2D trigger_zone = {plate->rect.x, plate->rect.y - 6.0f, plate->rect.w, plate->rect.h + 8.0f};
        bool player_on = boxesOverlap(player_box, trigger_zone);

        if (plate->type == 0 && !plate->active && stage->has_crate && boxesOverlap(stage->crate, trigger_zone)) {
            plate->active = true;
            stage->terminal_online = true;
            game->play_sfx[SFX_SWITCH] = true;
            showBanner(stage, "TERMINAL ONLINE! Head to the Exit!");
        } else if (plate->type == 1 && !plate->active && player_on) {
            plate->active = true;
            stage->seq_step = 1;
            game->play_sfx[SFX_SWITCH] = true;
            showBanner(stage, "Plate [1] Online! Activate Plate [2]!");
        } else if (plate->type == 2 && !plate->active && player_on) {
            if (stage->seq_step >= 1) {
                plate->active = true;
                stage->terminal_online = true;
                stage->seq_step = 2;
                game->play_sfx[SFX_SWITCH] = true;
                showBanner(stage, "SEQUENCE COMPLETE! Lab Exit ONLINE!");
            } else if (plate->cooldown <= 0.0f) {
                plate->cooldown = 1.5f;
                showBanner(stage, "Activate Plate [1] first!");
            }
        } else if (plate->type == 3 && player_on) {
            if (!plate->is_correct) {
                if (plate->cooldown <= 0.0f) {
                    applyPenalty(game, 0.50f, 1);
                    plate->cooldown = 1.5f;
                    showBanner(stage, "FALSE ROOT! x=1 fails! +0.50 GWA & 1 Slip!");
                }
            } else if (!plate->active) {
                plate->active = true;
                game->total_score += 200;
                game->play_sfx[SFX_SWITCH] = true;

                bool all_roots_found = true;
                for (int j = 0; j < stage->plate_count; j++) {
                    if (stage->plates[j].type == 3 && stage->plates[j].is_correct && !stage->plates[j].active) {
                        all_roots_found = false;
                    }
                }
                if (all_roots_found) {
                    stage->terminal_online = true;
                    showBanner(stage, "ROOTS x=2, x=3 SOLVED! Exit ONLINE!");
                } else {
                    showBanner(stage, "ROOT FOUND (+200 PTS)! Find the other root!");
                }
            }
        } else if (plate->type == 4 && player_on) {
            bool fresh_step = !plate->player_was_on && (plate->cooldown <= 0.0f);
            if (input->interact || fresh_step) {
                int idx = plate->value;
                stage->logic_inputs[idx] = !stage->logic_inputs[idx];
                plate->active = stage->logic_inputs[idx];
                plate->cooldown = 0.25f;
                snprintf(plate->label, sizeof(plate->label), "IN %c=%d", 'A' + idx, plate->active ? 1 : 0);
                game->play_sfx[SFX_SWITCH] = true;

                stage->terminal_online = evaluateLogicCircuit(stage);
                if (stage->terminal_online) {
                    showBanner(stage, "OUT = 1 (TRUE)! Exit Terminal ONLINE!");
                } else {
                    showBanner(stage, "OUT = 0 (FALSE). Adjust A, B, or C!");
                }
            }
        } else if (plate->type == 5 && !plate->active && player_on) {
            plate->active = true;
            stage->terminal_online = true;
            game->play_sfx[SFX_SWITCH] = true;
            showBanner(stage, "BELL RUNG! Cross the moving platforms!");
        }

        plate->player_was_on = player_on;
    }

    if (player->gwa >= 4.999f || player->slips >= 3) {
        player->gwa = 5.00f;
        game->play_sfx[SFX_GAMEOVER] = true;
        game->gameover_cursor = 0;
        game->scene = SCENE_GAMEOVER;
        return;
    }

    if (stage->terminal_online && boxesOverlap(player_box, stage->exit_terminal)) {
        int time_bonus = (int)floorf(stage->time_remaining) * 10;
        int gwa_bonus = (int)roundf((5.00f - player->gwa) * 150.0f);
        game->total_score += time_bonus + gwa_bonus;
        game->play_sfx[SFX_CLEAR] = true;

        if (stage->course_id >= TOTAL_STAGES) {
            if (game->total_score > game->best_score) game->best_score = game->total_score;
            if (player->gwa < game->best_gwa) game->best_gwa = player->gwa;
            game->unlocked_stage = TOTAL_STAGES;
            saveProfile(game, SAVE_PATH);
            game->scene = SCENE_VICTORY;
        } else {
            if (stage->course_id + 1 > game->unlocked_stage) {
                game->unlocked_stage = stage->course_id + 1;
                saveProfile(game, SAVE_PATH);
            }
            loadStageRoom(game, stage->course_id + 1);
            game->scene = SCENE_BRIEFING;
        }
    }
}

void updateStageGameplay(GameState *game, const InputState *input, float dt) {
    Stage *stage = &game->stage;
    Player *player = &game->isko;

    if (dt > 0.05f) dt = 0.05f;
    if (stage->banner_timer > 0.0f) stage->banner_timer -= dt;
    if (player->invuln_timer > 0.0f) player->invuln_timer -= dt;

    stage->time_remaining -= dt;
    if (stage->time_remaining <= 0.0f) {
        stage->time_remaining = 0.0f;
        player->gwa = 5.00f;
        player->slips = 3;
        game->play_sfx[SFX_GAMEOVER] = true;
        game->gameover_cursor = 0;
        game->scene = SCENE_GAMEOVER;
        return;
    }

    float gravity = 980.0f * stage->gravity_scale;
    float prev_y = player->y;

    updateMovingPlatforms(stage, player, dt);
    handleLadderAndJump(game, input, gravity, dt);
    movePlayerAndCrateX(stage, player, gravity, dt);
    movePlayerY(game, stage, player, prev_y, dt);
    updateCollectibles(game, stage, player, dt);
    updateHazards(game, stage, player, prev_y, dt);
    checkPlatesAndExit(game, stage, player, input, dt);
}
