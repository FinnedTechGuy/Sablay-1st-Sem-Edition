#include "game.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---------- Asset & Texture Helpers ---------- */

static Texture2D makePixelTex(Image img) {
    Texture2D tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    return tex;
}

static Texture2D loadFilteredTex(const char *path, int filter) {
    Texture2D tex = LoadTexture(path);
    SetTextureFilter(tex, filter);
    return tex;
}

static Texture2D buildGrassTile(void) {
    Image img = GenImageColor(16, 16, (Color){126, 78, 44, 255});
    ImageDrawRectangle(&img, 0, 0, 16, 5, (Color){62, 164, 58, 255});
    ImageDrawRectangle(&img, 0, 0, 16, 2, (Color){106, 206, 82, 255});
    return makePixelTex(img);
}

static Sound makeTone(float start_freq, float end_freq, float duration, int wave_type) {
    const unsigned int sample_rate = 22050;
    const unsigned int sample_count = (unsigned int)(duration * 22050.0f);
    short *samples = (short *)MemAlloc(sample_count * sizeof(short));
    float phase = 0.0f;

    for (unsigned int i = 0; i < sample_count && samples != NULL; i++) {
        float progress = (float)i / (float)sample_count;
        float freq = start_freq + (end_freq - start_freq) * progress;
        phase += freq / (float)sample_rate;
        if (phase >= 1.0f) {
            phase -= floorf(phase);
        }

        float value = 0.0f;
        if (wave_type == 0) {
            value = (phase < 0.5f) ? 1.0f : -1.0f;
        } else if (wave_type == 1) {
            value = 1.0f - 4.0f * fabsf(phase - 0.5f);
        } else {
            value = ((i * 1103515245u) & 0x4000u) ? 0.8f : -0.8f;
        }

        samples[i] = (short)(value * (1.0f - progress) * 8500.0f);
    }

    Wave wave = {sample_count, sample_rate, 16, 1, samples};
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

/* ---------- UI Text & Box Helpers ---------- */

static void drawOutlinedText(const char *text, int x, int y, int size, Color fg, Color outline) {
    for (int dy = -2; dy <= 2; dy += 2) {
        for (int dx = -2; dx <= 2; dx += 2) {
            if (dx != 0 || dy != 0) {
                DrawText(text, x + dx, y + dy, size, outline);
            }
        }
    }
    DrawText(text, x + 3, y + 3, size, outline);
    DrawText(text, x, y, size, fg);
}

static void drawCenteredText(const char *text, int y, int size, Color fg, Color outline) {
    int x = (SCREEN_W - MeasureText(text, size)) / 2;
    drawOutlinedText(text, x, y, size, fg, outline);
}

static void drawMenuOptions(const char **items, int count, int cursor, int y_start, int gap, Color highlight) {
    for (int i = 0; i < count; i++) {
        const char *prefix = (cursor == i) ? ">" : " ";
        const char *line = TextFormat("%s %s", prefix, items[i]);
        int x = (SCREEN_W - MeasureText(line, 26)) / 2;
        int y = y_start + i * gap;
        Color color = (cursor == i) ? highlight : WHITE;

        drawOutlinedText(line, x - 1, y - 1, 26, BLACK, BLACK);
        drawOutlinedText(line, x + 1, y + 1, 26, BLACK, BLACK);
        drawOutlinedText(line, x, y, 26, color, BLACK);
    }
}

static void drawSablayBox(Rectangle box) {
    DrawRectangleRec(box, (Color){123, 17, 19, 255});
    DrawRectangleLinesEx(box, 4, (Color){244, 196, 48, 255});
    for (int x = (int)box.x + 8; x < (int)(box.x + box.width) - 12; x += 16) {
        DrawRectangle(x, (int)box.y + 4, 8, 6, GOLD);
        DrawRectangle(x + 2, (int)box.y + 5, 4, 4, GREEN);
        DrawRectangle(x, (int)(box.y + box.height) - 10, 8, 6, GOLD);
        DrawRectangle(x + 2, (int)(box.y + box.height) - 9, 4, 4, GREEN);
    }
}

/* ---------- Screen Input Handlers ---------- */

static void updateMenuInput(GameState *game, bool up, bool down, bool confirm) {
    if (up) {
        game->menu_cursor = (game->menu_cursor + 3) % 4;
        game->play_sfx[SFX_MOVE] = true;
    }
    if (down) {
        game->menu_cursor = (game->menu_cursor + 1) % 4;
        game->play_sfx[SFX_MOVE] = true;
    }
    if (!confirm) {
        return;
    }

    game->play_sfx[SFX_SELECT] = true;
    if (game->menu_cursor == 0) {
        startNewRun(game, 1);
        game->scene = SCENE_BRIEFING;
    } else if (game->menu_cursor == 1) {
        startNewRun(game, game->unlocked_stage);
        game->scene = SCENE_BRIEFING;
    } else if (game->menu_cursor == 2) {
        game->settings_cursor = 0;
        game->settings_back = SCENE_MENU;
        game->scene = SCENE_SETTINGS;
    } else {
        game->should_exit = true;
    }
}

static void updateSettingsInput(GameState *game, bool audio_ok, bool up, bool down) {
    if (up) {
        game->settings_cursor = (game->settings_cursor + 2) % 3;
        game->play_sfx[SFX_MOVE] = true;
    }
    if (down) {
        game->settings_cursor = (game->settings_cursor + 1) % 3;
        game->play_sfx[SFX_MOVE] = true;
    }

    int delta = 0;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) delta = 10;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) delta = -10;

    if (delta != 0 && game->settings_cursor == 0) {
        game->master_volume += delta;
        if (game->master_volume < 0) game->master_volume = 0;
        if (game->master_volume > 100) game->master_volume = 100;
        if (audio_ok) SetMasterVolume((float)game->master_volume / 100.0f);
        game->play_sfx[SFX_MOVE] = true;
    } else if (delta != 0 && game->settings_cursor == 1) {
        game->sfx_volume += delta;
        if (game->sfx_volume < 0) game->sfx_volume = 0;
        if (game->sfx_volume > 100) game->sfx_volume = 100;
        game->play_sfx[SFX_MOVE] = true;
    }

    bool exit_settings = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) ||
                         (IsKeyPressed(KEY_SPACE) && game->settings_cursor == 2);
    if (exit_settings) {
        saveProfile(game, SAVE_PATH);
        game->play_sfx[SFX_SELECT] = true;
        game->scene = game->settings_back;
    }
}

static void updateGameInput(GameState *game, bool up, bool down, bool confirm, float dt) {
    Rectangle gear_btn = {SCREEN_W - 44, 12, 36, 36};
    bool clicked_gear = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                        CheckCollisionPointRec(GetMousePosition(), gear_btn);

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P) || clicked_gear) {
        game->paused = !game->paused;
        game->pause_cursor = 0;
        game->play_sfx[SFX_MOVE] = true;
        return;
    }

    if (game->paused) {
        if (up) {
            game->pause_cursor = (game->pause_cursor + 3) % 4;
            game->play_sfx[SFX_MOVE] = true;
        }
        if (down) {
            game->pause_cursor = (game->pause_cursor + 1) % 4;
            game->play_sfx[SFX_MOVE] = true;
        }
        if (!confirm) {
            return;
        }

        game->play_sfx[SFX_SELECT] = true;
        if (game->pause_cursor == 0) {
            game->paused = false;
        } else if (game->pause_cursor == 1) {
            game->settings_cursor = 0;
            game->settings_back = SCENE_GAME;
            game->scene = SCENE_SETTINGS;
        } else if (game->pause_cursor == 2) {
            game->paused = false;
            retakeCurrentStage(game);
        } else {
            game->paused = false;
            game->scene = SCENE_MENU;
        }
        return;
    }

    InputState input = {
        .left     = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT),
        .right    = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT),
        .up       = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP),
        .down     = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN),
        .jump     = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_UP),
        .interact = IsKeyPressed(KEY_E)
    };
    updateStageGameplay(game, &input, dt);
}

/* ---------- Screen Draw Helpers ---------- */

static void drawFullBg(Texture2D bg, Color tint) {
    Rectangle src = {0, 0, (float)bg.width, (float)bg.height};
    Rectangle dst = {0, 0, SCREEN_W, SCREEN_H};
    DrawTexturePro(bg, src, dst, (Vector2){0, 0}, 0.0f, tint);
}

static void drawMenuScreen(const GameState *game, Texture2D bg) {
    drawFullBg(bg, WHITE);
    const char *options[4] = {"NEW GAME", "CONTINUE", "SETTINGS", "EXIT"};
    drawMenuOptions(options, 4, game->menu_cursor, 352, 40, GOLD);

    DrawRectangle(0, SCREEN_H - 44, SCREEN_W, 44, (Color){16, 16, 22, 220});
    drawOutlinedText(TextFormat("BEST SCORE: %04d", game->best_score), 22, SCREEN_H - 32, 22, GOLD, BLACK);
    const char *gwa_text = TextFormat("GWA: %.2f %s", (double)game->best_gwa, laudeTag(game->best_gwa, true));
    drawOutlinedText(gwa_text, SCREEN_W - MeasureText(gwa_text, 22) - 22, SCREEN_H - 32, 22, (Color){116, 244, 204, 255}, BLACK);
}

static void drawSettingsScreen(const GameState *game, Texture2D bg) {
    drawFullBg(bg, (Color){185, 185, 195, 255});
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){12, 14, 22, 145});
    drawCenteredText("AUDIO SETTINGS", 78, 44, GOLD, MAROON);

    DrawRectangle((SCREEN_W - 560) / 2, 164, 560, 240, (Color){20, 22, 34, 225});
    const char *rows[3] = {
        TextFormat("MASTER VOLUME: [ < %3d%% > ]", game->master_volume),
        TextFormat("SFX VOLUME:    [ < %3d%% > ]", game->sfx_volume),
        "BACK"
    };
    drawMenuOptions(rows, 3, game->settings_cursor, 204, 64, GOLD);
    drawCenteredText("[LEFT / RIGHT]: ADJUST | [ENTER / ESC]: SAVE & EXIT", SCREEN_H - 34, 18, RAYWHITE, BLACK);
}

static void drawBriefingScreen(const GameState *game, Texture2D bg) {
    drawFullBg(bg, (Color){130, 130, 145, 255});
    drawSablayBox((Rectangle){(SCREEN_W - 680) / 2.0f, 120, 680, 320});
    DrawRectangle((SCREEN_W - 656) / 2, 138, 656, 284, (Color){24, 24, 34, 245});

    drawCenteredText(TextFormat("COURSE %d OF %d - %s (%s)", game->stage.course_id, TOTAL_STAGES, game->stage.code, game->stage.subtitle), 156, 20, SKYBLUE, BLACK);
    drawCenteredText(game->stage.title, 196, 26, GOLD, BLACK);
    drawCenteredText(game->stage.hint, 254, 17, RAYWHITE, BLACK);
    drawCenteredText(TextFormat("TIME: %.0fs | GRAVITY: %.2fx | GWA: %.2f", (double)game->stage.time_limit, (double)game->stage.gravity_scale, (double)game->isko.gwa), 312, 18, GREEN, BLACK);
    drawCenteredText("PRESS [ENTER] OR [E] TO BEGIN COURSE", 376, 22, GOLD, BLACK);
}

static void drawGameScreen(const GameState *game, Texture2D stage_bg[2], Texture2D tile, Texture2D isko[4]) {
    const Stage *stage = &game->stage;
    const Player *player = &game->isko;

    if (stage->course_id <= 2) {
        Texture2D sbg = stage_bg[stage->course_id - 1];
        DrawTexturePro(sbg, (Rectangle){0, 0, (float)sbg.width, (float)sbg.height},
                       (Rectangle){0, HUD_H, SCREEN_W, SCREEN_H - HUD_H}, (Vector2){0, 0}, 0.0f, WHITE);
    } else if (stage->course_id == 3) {
        DrawRectangleGradientV(0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, (Color){222, 214, 194, 255}, (Color){192, 182, 158, 255});
        DrawRectangle(286, 80, 388, 68, (Color){26, 78, 48, 255});
        DrawRectangleLinesEx((Rectangle){286, 80, 388, 68}, 4, (Color){126, 82, 42, 255});
        drawCenteredText("x^2 - 5x + 6 = 0", 90, 22, RAYWHITE, BLACK);
        drawCenteredText("Factor & step on the 2 true roots!", 118, 15, GOLD, BLACK);
    } else if (stage->course_id == 4) {
        DrawRectangleGradientV(0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, (Color){18, 24, 46, 255}, (Color){28, 38, 72, 255});
        bool out_ok = evaluateLogicCircuit(stage);
        Color trace = out_ok ? GREEN : RED;
        DrawRectangle(215, 78, 530, 68, (Color){14, 18, 32, 225});
        DrawRectangleLinesEx((Rectangle){215, 78, 530, 68}, 3, trace);
        DrawLineEx((Vector2){745, 112}, (Vector2){888, 112}, 4, trace);
        DrawLineEx((Vector2){888, 112}, (Vector2){888, 464}, 4, trace);
        drawCenteredText("OUT = (A AND B) OR (NOT C)", 88, 20, SKYBLUE, BLACK);
        drawCenteredText(TextFormat("A=%d   B=%d   C=%d   ->   OUT = %d (%s)",
                                    stage->logic_inputs[0] ? 1 : 0,
                                    stage->logic_inputs[1] ? 1 : 0,
                                    stage->logic_inputs[2] ? 1 : 0,
                                    out_ok ? 1 : 0, out_ok ? "TRUE" : "FALSE"),
                         116, 17, out_ok ? GREEN : ORANGE, BLACK);
    } else {
        DrawRectangleGradientV(0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, (Color){118, 186, 236, 255}, (Color){186, 224, 248, 255});
    }

    for (int i = 0; i < stage->platform_count; i++) {
        Platform p = stage->platforms[i];
        Rect2D plat = p.rect;
        if (p.is_hurdle) {
            DrawRectangle((int)plat.x, (int)plat.y, (int)plat.w, (int)plat.h, WHITE);
            DrawRectangle((int)plat.x, (int)plat.y, (int)plat.w, 8, RED);
            DrawRectangle((int)plat.x, (int)plat.y + 16, (int)plat.w, 6, RED);
            DrawRectangleLinesEx((Rectangle){plat.x, plat.y, plat.w, plat.h}, 2, BLACK);
        } else if (p.is_moving) {
            DrawRectangleRec((Rectangle){plat.x, plat.y, plat.w, plat.h}, (Color){36, 108, 204, 255});
            DrawRectangle((int)plat.x, (int)plat.y, (int)plat.w, 5, SKYBLUE);
            DrawRectangleLinesEx((Rectangle){plat.x, plat.y, plat.w, plat.h}, 2, BLACK);
        } else if (plat.y >= 510.0f) {
            for (int tx = (int)plat.x; tx < (int)(plat.x + plat.w); tx += TILE_SIZE) {
                for (int ty = (int)plat.y; ty < (int)(plat.y + plat.h); ty += TILE_SIZE) {
                    DrawTexturePro(tile, (Rectangle){0, 0, 16, 16}, (Rectangle){(float)tx, (float)ty, TILE_SIZE, TILE_SIZE}, (Vector2){0, 0}, 0.0f, WHITE);
                }
            }
        } else {
            DrawRectangleRec((Rectangle){plat.x, plat.y, plat.w, plat.h}, (Color){138, 92, 54, 255});
            DrawRectangle((int)plat.x, (int)plat.y, (int)plat.w, 5, (Color){68, 174, 72, 255});
            DrawRectangleLinesEx((Rectangle){plat.x, plat.y, plat.w, plat.h}, 2, BLACK);
        }
    }

    for (int i = 0; i < stage->ladder_count; i++) {
        Rect2D lad = stage->ladders[i];
        DrawRectangle((int)lad.x, (int)lad.y, 4, (int)lad.h, GOLD);
        DrawRectangle((int)(lad.x + lad.w - 4), (int)lad.y, 4, (int)lad.h, GOLD);
        for (int y = (int)lad.y + 8; y < (int)(lad.y + lad.h); y += 14) {
            DrawRectangle((int)lad.x + 4, y, (int)lad.w - 8, 4, GOLD);
        }
    }

    for (int i = 0; i < stage->plate_count; i++) {
        const Plate *plate = &stage->plates[i];
        int press_offset = plate->active ? 3 : 0;
        Color c = plate->active ? GREEN : RED;
        DrawRectangle((int)plate->rect.x, (int)plate->rect.y + press_offset, (int)plate->rect.w, (int)plate->rect.h - press_offset, c);
        drawOutlinedText(plate->label, (int)plate->rect.x, (int)plate->rect.y - 18, 13, WHITE, BLACK);
    }

    Color exit_color = stage->terminal_online ? GREEN : RED;
    DrawRectangle((int)stage->exit_terminal.x, (int)stage->exit_terminal.y, 36, 48, (Color){34, 40, 52, 255});
    DrawRectangleLines((int)stage->exit_terminal.x, (int)stage->exit_terminal.y, 36, 48, exit_color);
    DrawRectangle((int)stage->exit_terminal.x + 6, (int)stage->exit_terminal.y + 8, 24, 18, exit_color);
    drawOutlinedText(stage->terminal_online ? "EXIT ON" : "LOCKED", (int)stage->exit_terminal.x - 6, (int)stage->exit_terminal.y - 18, 13, exit_color, BLACK);

    if (stage->has_crate) {
        DrawRectangle((int)stage->crate.x, (int)stage->crate.y, (int)stage->crate.w, (int)stage->crate.h, (Color){178, 116, 54, 255});
        DrawRectangleLines((int)stage->crate.x, (int)stage->crate.y, (int)stage->crate.w, (int)stage->crate.h, (Color){92, 52, 18, 255});
        DrawLine((int)stage->crate.x, (int)stage->crate.y, (int)(stage->crate.x + stage->crate.w), (int)(stage->crate.y + stage->crate.h), (Color){92, 52, 18, 255});
    }

    for (int i = 0; i < stage->item_count; i++) {
        if (stage->items[i].collected) continue;
        int x = (int)stage->items[i].rect.x;
        int y = (int)(stage->items[i].rect.y + sinf(stage->items[i].phase) * 4.0f);
        if (stage->items[i].kind == 0) {
            DrawRectangle(x, y, 20, 24, RAYWHITE);
            DrawRectangleLines(x, y, 20, 24, BLACK);
            DrawRectangle(x + 4, y + 5, 10, 3, MAROON);
        } else {
            DrawRectangle(x, y + 4, 18, 16, WHITE);
            DrawRectangle(x + 2, y + 6, 14, 4, BROWN);
        }
    }

    for (int i = 0; i < stage->hazard_count; i++) {
        if (!stage->hazards[i].alive) continue;
        Rect2D h = stage->hazards[i].rect;
        if (stage->hazards[i].kind == 1) {
            DrawCircle((int)(h.x + 13), (int)(h.y + 13), 12, PURPLE);
        } else {
            DrawRectangle((int)h.x + 2, (int)h.y + 2, 22, 14, (Color){148, 56, 36, 255});
            DrawRectangle((int)h.x + 6, (int)h.y + 16, 14, 10, (Color){224, 188, 142, 255});
        }
    }

    bool blink_visible = (player->invuln_timer <= 0.0f) || (((int)(player->invuln_timer * 14.0f) % 2) == 0);
    if (blink_visible) {
        int pose = 0; /* 0 = idle, 1 = run1, 2 = run2, 3 = jump */
        if (!player->is_grounded && !player->on_ladder) {
            pose = 3;
        } else if (player->on_ladder) {
            pose = (fabsf(player->vy) > 10.0f) ? (1 + player->anim_frame) : 3;
        } else if (fabsf(player->vx) > 10.0f) {
            pose = 1 + player->anim_frame;
        }

        Texture2D tx = isko[pose];
        float src_w = player->facing_right ? (float)tx.width : -(float)tx.width;
        float draw_w = (float)tx.width;
        float draw_h = (float)tx.height;
        float draw_x = player->x - (draw_w - player->w) * 0.5f;
        float draw_y = player->y + player->h - draw_h;
        DrawTexturePro(tx, (Rectangle){0, 0, src_w, draw_h},
                       (Rectangle){draw_x, draw_y, draw_w, draw_h}, (Vector2){0, 0}, 0.0f, WHITE);
    }

    if (stage->banner_timer > 0.0f) {
        DrawRectangle((SCREEN_W - 560) / 2, HUD_H + 8, 560, 28, (Color){12, 14, 22, 200});
        drawCenteredText(stage->banner, HUD_H + 13, 17, GOLD, BLACK);
    }
    drawOutlinedText("WASD/ARROWS: Move | SPACE: Jump | ESC/P: Pause", 20, SCREEN_H - 20, 12, RAYWHITE, BLACK);

    /* Top 64px checkerboard HUD */
    for (int y = 0; y < HUD_H; y += 8) {
        for (int x = 0; x < SCREEN_W; x += 8) {
            bool even_tile = ((x / 8 + y / 8) % 2) == 0;
            Color cell_color = even_tile ? (Color){214, 218, 224, 255} : (Color){196, 202, 210, 255};
            DrawRectangle(x, y, 8, 8, cell_color);
        }
    }

    Color gwa_color = GREEN;
    if (player->gwa > 3.0f) gwa_color = RED;
    else if (player->gwa > 1.75f) gwa_color = YELLOW;

    drawOutlinedText(stage->code, 16, 10, 22, (Color){44, 184, 255, 255}, BLACK);
    drawOutlinedText(stage->subtitle, 16, 36, 16, (Color){76, 218, 255, 255}, BLACK);
    drawOutlinedText(TextFormat("GWA: %.2f", (double)player->gwa), 146, 10, 20, gwa_color, BLACK);
    drawOutlinedText(TextFormat("SCORE: %05d", game->total_score), 146, 36, 18, GOLD, BLACK);
    drawOutlinedText(TextFormat("SLIPS: %d/3", player->slips), 290, 12, 16, (player->slips >= 2) ? RED : WHITE, BLACK);
    drawCenteredText(TextFormat("TIME: %02ds", (int)ceilf(stage->time_remaining)), 18, 28, GOLD, BLACK);
    drawOutlinedText(TextFormat("PAPERS: %d/%d", player->papers, TOTAL_PAPERS), SCREEN_W - 216, 20, 22, WHITE, BLACK);
    DrawCircle(SCREEN_W - 28, 30, 12, DARKGRAY);
    DrawCircle(SCREEN_W - 28, 30, 5, LIGHTGRAY);

    if (game->paused) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){10, 10, 16, 175});
        drawSablayBox((Rectangle){(SCREEN_W - 340) / 2.0f, 160, 340, 240});
        drawCenteredText("PAUSED", 184, 30, GOLD, BLACK);
        const char *pause_opts[4] = {"RESUME", "SETTINGS", "RETRY STAGE", "MAIN MENU"};
        drawMenuOptions(pause_opts, 4, game->pause_cursor, 236, 36, GOLD);
    }
}

static void drawGameOverScreen(const GameState *game, Texture2D bg) {
    drawFullBg(bg, (Color){110, 100, 108, 255});
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){14, 8, 10, 150});

    Rectangle modal = {(SCREEN_W - 560) / 2.0f, 76, 560, 310};
    DrawRectangleRec(modal, (Color){248, 242, 226, 255});
    DrawRectangleLinesEx(modal, 6, (Color){118, 16, 22, 255});

    int cx = SCREEN_W / 2;
    DrawText("UNIVERSITY OF THE PHILIPPINES", cx - MeasureText("UNIVERSITY OF THE PHILIPPINES", 20) / 2, 102, 20, BLACK);
    DrawText("OFFICE OF THE UNIVERSITY REGISTRAR", cx - MeasureText("OFFICE OF THE UNIVERSITY REGISTRAR", 15) / 2, 128, 15, DARKGRAY);
    DrawText("NOTICE OF ACADEMIC DISMISSAL", cx - MeasureText("NOTICE OF ACADEMIC DISMISSAL", 24) / 2, 172, 24, MAROON);
    DrawText("Scholastic Deficiency: GWA 5.00", cx - MeasureText("Scholastic Deficiency: GWA 5.00", 20) / 2, 218, 20, BLACK);
    DrawText("Your GWA has left the chat.", cx - MeasureText("Your GWA has left the chat.", 16) / 2, 248, 16, DARKGRAY);
    DrawRectangleLinesEx((Rectangle){(float)cx - 92, 292, 184, 52}, 5, RED);
    DrawText("DROPPED", cx - MeasureText("DROPPED", 30) / 2, 304, 30, RED);

    DrawRectangle((SCREEN_W - 320) / 2, 412, 320, 106, BLACK);
    const char *options[2] = {"RETAKE COURSE", "MAIN MENU"};
    drawMenuOptions(options, 2, game->gameover_cursor, 430, 42, ORANGE);
}

static void drawVictoryScreen(const GameState *game, Texture2D bg) {
    drawFullBg(bg, WHITE);
    Color confetti[4] = {MAROON, GOLD, GREEN, SKYBLUE};
    for (int i = 0; i < 48; i++) {
        int x = (i * 73 + (int)(game->global_timer * 45.0f)) % SCREEN_W;
        int y = (i * 47 + (int)(game->global_timer * (60.0f + (float)(i % 5) * 14.0f))) % SCREEN_H;
        DrawRectangle(x, y, 8, 8, confetti[i % 4]);
    }
    DrawRectangle(0, 0, SCREEN_W, 18, MAROON);
    DrawRectangle(0, 18, SCREEN_W, 6, GOLD);

    DrawRectangle(90, 96, SCREEN_W - 180, 356, (Color){16, 18, 28, 200});
    drawCenteredText("STAGES 1-5 CLEARED!", 126, 44, GOLD, MAROON);
    drawCenteredText(TextFormat("FINAL GWA: %.2f %s", (double)game->isko.gwa, laudeTag(game->isko.gwa, false)), 196, 28, SKYBLUE, BLACK);
    drawCenteredText(laudeQuote(game->isko.gwa), 258, 20, RAYWHITE, BLACK);
    drawCenteredText(TextFormat("Final Score: %06d", game->total_score), 324, 30, GOLD, BLACK);
    drawCenteredText("PRESS [ENTER] TO RETURN TO MAIN MENU", SCREEN_H - 36, 22, WHITE, BLACK);
}

/* ---------- Main Entry Point ---------- */

int main(int argc, char **argv) {
    bool smoke = (argc > 1 && strcmp(argv[1], "--smoke-test") == 0);
    GameState game = {0};
    loadProfile(&game, SAVE_PATH);
    game.settings_back = SCENE_MENU;

    if (smoke) SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(SCREEN_W, SCREEN_H, "SABLAY - 1st Semester Edition");
    if (FileExists("assets/icon.png")) {
        Image icon = LoadImage("assets/icon.png");
        ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        SetWindowIcon(icon);
        UnloadImage(icon);
    }
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);

    InitAudioDevice();
    bool audio_ok = IsAudioDeviceReady();
    if (audio_ok) SetMasterVolume((float)game.master_volume / 100.0f);

    Texture2D bg = loadFilteredTex("assets/campus_bg.jpg", TEXTURE_FILTER_BILINEAR);
    Texture2D stage_bg[2] = {
        loadFilteredTex("assets/stage1_bg.jpg", TEXTURE_FILTER_BILINEAR),
        loadFilteredTex("assets/stage2_bg.jpg", TEXTURE_FILTER_BILINEAR)
    };
    Texture2D tile = buildGrassTile();
    Texture2D isko[4] = {
        loadFilteredTex("assets/isko_idle.png", TEXTURE_FILTER_POINT),
        loadFilteredTex("assets/isko_run1.png", TEXTURE_FILTER_POINT),
        loadFilteredTex("assets/isko_run2.png", TEXTURE_FILTER_POINT),
        loadFilteredTex("assets/isko_jump.png", TEXTURE_FILTER_POINT)
    };

    Sound sfx[SFX_COUNT] = {0};
    if (audio_ok) {
        static const float f0[SFX_COUNT]  = {440, 520, 260, 660, 480, 360, 340, 220, 523, 300};
        static const float f1[SFX_COUNT]  = {520, 780, 620, 1180, 960, 720, 140, 80, 1046, 95};
        static const float dur[SFX_COUNT] = {0.06f, 0.12f, 0.14f, 0.18f, 0.22f, 0.15f, 0.12f, 0.24f, 0.36f, 0.42f};
        static const int wave[SFX_COUNT]  = {0, 0, 0, 1, 1, 0, 2, 2, 1, 0};
        for (int i = 0; i < SFX_COUNT; i++) {
            sfx[i] = makeTone(f0[i], f1[i], dur[i], wave[i]);
        }
    }

    int frames = 0;
    while (!WindowShouldClose() && !game.should_exit) {
        float dt = GetFrameTime();
        if (dt <= 0.0f) dt = 1.0f / 60.0f;
        game.global_timer += dt;

        bool up = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
        bool down = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
        bool confirm = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E);

        if (game.scene == SCENE_MENU) {
            updateMenuInput(&game, up, down, confirm);
        } else if (game.scene == SCENE_SETTINGS) {
            updateSettingsInput(&game, audio_ok, up, down);
        } else if (game.scene == SCENE_BRIEFING) {
            if (confirm) {
                game.play_sfx[SFX_SELECT] = true;
                game.scene = SCENE_GAME;
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                game.scene = SCENE_MENU;
            }
        } else if (game.scene == SCENE_GAME) {
            updateGameInput(&game, up, down, confirm, dt);
        } else if (game.scene == SCENE_GAMEOVER) {
            if (up || down) {
                game.gameover_cursor = 1 - game.gameover_cursor;
                game.play_sfx[SFX_MOVE] = true;
            }
            if (confirm) {
                game.play_sfx[SFX_SELECT] = true;
                if (game.gameover_cursor == 0) {
                    retakeCurrentStage(&game);
                    game.scene = SCENE_GAME;
                } else {
                    game.scene = SCENE_MENU;
                }
            }
        } else if (confirm) {
            game.play_sfx[SFX_SELECT] = true;
            game.scene = SCENE_MENU;
        }

        for (int i = 0; i < SFX_COUNT; i++) {
            if (audio_ok && game.play_sfx[i] && IsSoundValid(sfx[i])) {
                SetSoundVolume(sfx[i], (float)game.sfx_volume / 100.0f);
                PlaySound(sfx[i]);
            }
            game.play_sfx[i] = false;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        if (game.scene == SCENE_MENU) drawMenuScreen(&game, bg);
        else if (game.scene == SCENE_SETTINGS) drawSettingsScreen(&game, bg);
        else if (game.scene == SCENE_BRIEFING) drawBriefingScreen(&game, bg);
        else if (game.scene == SCENE_GAME) drawGameScreen(&game, stage_bg, tile, isko);
        else if (game.scene == SCENE_GAMEOVER) drawGameOverScreen(&game, bg);
        else drawVictoryScreen(&game, bg);
        EndDrawing();

        if (smoke && ++frames >= 5) {
            game.should_exit = true;
        }
    }

    UnloadTexture(bg);
    UnloadTexture(stage_bg[0]);
    UnloadTexture(stage_bg[1]);
    UnloadTexture(tile);
    for (int i = 0; i < 4; i++) {
        UnloadTexture(isko[i]);
    }
    if (audio_ok) {
        for (int i = 0; i < SFX_COUNT; i++) {
            UnloadSound(sfx[i]);
        }
        CloseAudioDevice();
    }
    CloseWindow();
    return 0;
}
