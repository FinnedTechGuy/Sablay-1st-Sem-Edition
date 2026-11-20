#include "game.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static Texture2D makeTex(Image img) {
    Texture2D t = LoadTextureFromImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    return t;
}

static Texture2D loadBg(const char *path, Color fallback) {
    if (FileExists(path)) {
        Texture2D t = LoadTexture(path);
        if (IsTextureValid(t)) { SetTextureFilter(t, TEXTURE_FILTER_BILINEAR); return t; }
    }
    return makeTex(GenImageColor(1, 1, fallback));
}

static Texture2D buildIskoSprite(int pose) {
    Image img = GenImageColor(16, 20, BLANK);
    Color cap = {34, 102, 212, 255}, skin = {244, 198, 158, 255};
    Color shirt = {206, 56, 104, 255}, pants = {32, 66, 144, 255};
    ImageDrawRectangle(&img, 4, 1, 8, 4, cap); ImageDrawRectangle(&img, 4, 4, 10, 1, cap);
    ImageDrawRectangle(&img, 4, 5, 8, 5, skin); ImageDrawPixel(&img, 9, 6, BLACK);
    ImageDrawRectangle(&img, 4, 10, 8, 5, shirt);
    ImageDrawRectangle(&img, 2, (pose >= 2) ? 8 : 10, 2, 4, skin);
    ImageDrawRectangle(&img, 12, (pose == 3) ? 11 : (pose == 2 ? 8 : 10), 2, 4, skin);
    ImageDrawRectangle(&img, pose == 1 ? 3 : 4, 15, pose == 1 ? 4 : 3, 5, pants);
    ImageDrawRectangle(&img, 9, 15, pose == 1 ? 4 : 3, pose == 1 ? 4 : 5, pants);
    return makeTex(img);
}

static Sound makeTone(float f0, float f1, float dur, int wave) {
    const unsigned int sr = 22050, n = (unsigned int)(dur * 22050);
    short *buf = (short *)MemAlloc(n * sizeof(short));
    float ph = 0;
    for (unsigned int i = 0; i < n && buf; i++) {
        float t = (float)i / n;
        ph += (f0 + (f1 - f0) * t) / sr;
        if (ph >= 1) ph -= floorf(ph);
        float s = wave == 0 ? (ph < 0.5f ? 1.0f : -1.0f)
                : (wave == 1 ? (1.0f - 4.0f * fabsf(ph - 0.5f))
                             : (((i * 1103515245u) & 0x4000u) ? 0.8f : -0.8f));
        buf[i] = (short)(s * (1.0f - t) * 8500.0f);
    }
    Wave w = {n, sr, 16, 1, buf};
    Sound snd = LoadSoundFromWave(w);
    UnloadWave(w);
    return snd;
}

static void outText(const char *t, int x, int y, int sz, Color fg, Color bg) {
    for (int dy = -2; dy <= 2; dy += 2)
        for (int dx = -2; dx <= 2; dx += 2)
            if (dx || dy) DrawText(t, x + dx, y + dy, sz, bg);
    DrawText(t, x + 3, y + 3, sz, bg);
    DrawText(t, x, y, sz, fg);
}
static void centerText(const char *t, int y, int sz, Color fg, Color bg) {
    outText(t, (SCREEN_W - MeasureText(t, sz)) / 2, y, sz, fg, bg);
}
static void drawCursorList(const char **items, int n, int cur, int y0, int gap, Color hi) {
    for (int i = 0; i < n; i++) {
        const char *line = TextFormat("%s %s", cur == i ? ">" : " ", items[i]);
        int x = (SCREEN_W - MeasureText(line, 26)) / 2, y = y0 + i * gap;
        outText(line, x - 1, y - 1, 26, BLACK, BLACK);
        outText(line, x + 1, y + 1, 26, BLACK, BLACK);
        outText(line, x, y, 26, cur == i ? hi : WHITE, BLACK);
    }
}
static void drawEthnicBox(Rectangle r) {
    DrawRectangleRec(r, (Color){123, 17, 19, 255});
    DrawRectangleLinesEx(r, 4, (Color){244, 196, 48, 255});
    for (int x = (int)r.x + 8; x < (int)(r.x + r.width) - 12; x += 16) {
        DrawRectangle(x, (int)r.y + 4, 8, 6, GOLD); DrawRectangle(x + 2, (int)r.y + 5, 4, 4, GREEN);
        DrawRectangle(x, (int)(r.y + r.height) - 10, 8, 6, GOLD); DrawRectangle(x + 2, (int)(r.y + r.height) - 9, 4, 4, GREEN);
    }
}

int main(int argc, char **argv) {
    bool smoke = (argc > 1 && strcmp(argv[1], "--smoke-test") == 0);
    GameState g = {0};
    loadProfile(&g, SAVE_PATH);
    g.settings_back = SCENE_MENU;

    if (smoke) SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(SCREEN_W, SCREEN_H, "SABLAY - Week 3 Milestone (NSTP 1 & Science 10)");
    SetExitKey(KEY_NULL); SetTargetFPS(60);
    InitAudioDevice();
    bool audio_ok = IsAudioDeviceReady();
    if (audio_ok) SetMasterVolume((float)g.master_volume / 100.0f);

    Texture2D bg = loadBg("assets/campus_bg.jpg", (Color){42, 68, 96, 255}), isko[4];
    Texture2D stage_bg[2] = {
        loadBg("assets/stage1_bg.jpg", (Color){126, 188, 232, 255}),
        loadBg("assets/stage2_bg.jpg", (Color){22, 28, 66, 255})
    };
    Image gimg = GenImageColor(16, 16, (Color){126, 78, 44, 255});
    ImageDrawRectangle(&gimg, 0, 0, 16, 5, (Color){62, 164, 58, 255});
    ImageDrawRectangle(&gimg, 0, 0, 16, 2, (Color){106, 206, 82, 255});
    Texture2D tile = makeTex(gimg);
    for (int i = 0; i < 4; i++) isko[i] = buildIskoSprite(i);

    Sound sfx[10];
    if (audio_ok) {
        static const float f0[10] = {440,520,260,660,480,360,340,220,523,300};
        static const float f1[10] = {520,780,620,1180,960,720,140,80,1046,95};
        static const float du[10] = {0.06f,0.12f,0.14f,0.18f,0.22f,0.15f,0.12f,0.24f,0.36f,0.42f};
        static const int   wv[10] = {0,0,0,1,1,0,2,2,1,0};
        for (int i = 0; i < 10; i++) sfx[i] = makeTone(f0[i], f1[i], du[i], wv[i]);
    }

    int frames = 0;
    while (!WindowShouldClose() && !g.should_exit) {
        float dt = GetFrameTime();
        if (dt <= 0) dt = 1.0f / 60.0f;
        g.global_timer += dt;

        if (IsKeyPressed(KEY_F9)) { g.isko.gwa = 5; g.isko.slips = 3; g.paused = false; g.over_cur = 0; g.scene = SCENE_GAMEOVER; g.sfx |= SFX_GAMEOVER; }
        if (IsKeyPressed(KEY_F10)) {
            if (g.isko.gwa >= 4.99f) g.isko.gwa = 1.85f;
            if (g.total_score <= 0) g.total_score = 3240;
            if (g.total_score > g.best_score) g.best_score = g.total_score;
            if (g.isko.gwa < g.best_gwa) g.best_gwa = g.isko.gwa;
            saveProfile(&g, SAVE_PATH); g.paused = false; g.scene = SCENE_VICTORY; g.sfx |= SFX_CLEAR;
        }

        /* Scene Update */
        bool up = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W), dn = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
        bool ok = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E);
        if (g.scene == SCENE_MENU) {
            if (up) { g.menu_cur = (g.menu_cur + 3) % 4; g.sfx |= SFX_MOVE; }
            if (dn) { g.menu_cur = (g.menu_cur + 1) % 4; g.sfx |= SFX_MOVE; }
            if (ok) {
                g.sfx |= SFX_SELECT;
                if (g.menu_cur < 2) { startNewRun(&g, g.menu_cur == 0 ? 1 : g.unlocked_stage); g.scene = SCENE_BRIEFING; }
                else if (g.menu_cur == 2) { g.set_cur = 0; g.settings_back = SCENE_MENU; g.scene = SCENE_SETTINGS; }
                else g.should_exit = true;
            }
        } else if (g.scene == SCENE_SETTINGS) {
            if (up) { g.set_cur = (g.set_cur + 2) % 3; g.sfx |= SFX_MOVE; }
            if (dn) { g.set_cur = (g.set_cur + 1) % 3; g.sfx |= SFX_MOVE; }
            int d = (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) ? 10 : ((IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) ? -10 : 0);
            if (d && g.set_cur == 0) {
                g.master_volume = g.master_volume + d < 0 ? 0 : (g.master_volume + d > 100 ? 100 : g.master_volume + d);
                if (audio_ok) SetMasterVolume((float)g.master_volume / 100.0f);
                g.sfx |= SFX_MOVE;
            } else if (d && g.set_cur == 1) {
                g.sfx_volume = g.sfx_volume + d < 0 ? 0 : (g.sfx_volume + d > 100 ? 100 : g.sfx_volume + d);
                g.sfx |= SFX_MOVE;
            }
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || (IsKeyPressed(KEY_SPACE) && g.set_cur == 2)) {
                saveProfile(&g, SAVE_PATH); g.sfx |= SFX_SELECT; g.scene = g.settings_back;
            }
        } else if (g.scene == SCENE_BRIEFING) {
            if (ok) { g.sfx |= SFX_SELECT; g.scene = SCENE_GAME; }
            else if (IsKeyPressed(KEY_ESCAPE)) g.scene = SCENE_MENU;
        } else if (g.scene == SCENE_GAME) {
            bool gear = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), (Rectangle){SCREEN_W - 44, 12, 36, 36});
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P) || gear) { g.paused = !g.paused; g.pause_cur = 0; g.sfx |= SFX_MOVE; }
            else if (g.paused) {
                if (up) { g.pause_cur = (g.pause_cur + 3) % 4; g.sfx |= SFX_MOVE; }
                if (dn) { g.pause_cur = (g.pause_cur + 1) % 4; g.sfx |= SFX_MOVE; }
                if (ok) {
                    g.sfx |= SFX_SELECT;
                    if (g.pause_cur == 0) g.paused = false;
                    else if (g.pause_cur == 1) { g.set_cur = 0; g.settings_back = SCENE_GAME; g.scene = SCENE_SETTINGS; }
                    else if (g.pause_cur == 2) { g.paused = false; retakeCurrentStage(&g); }
                    else { g.paused = false; g.scene = SCENE_MENU; }
                }
            } else {
                InputState in = {IsKeyDown(KEY_A)||IsKeyDown(KEY_LEFT), IsKeyDown(KEY_D)||IsKeyDown(KEY_RIGHT),
                                 IsKeyDown(KEY_W)||IsKeyDown(KEY_UP), IsKeyDown(KEY_S)||IsKeyDown(KEY_DOWN),
                                 IsKeyPressed(KEY_W)||IsKeyPressed(KEY_SPACE)||IsKeyPressed(KEY_UP)};
                updateStageGameplay(&g, &in, dt);
            }
        } else if (g.scene == SCENE_GAMEOVER) {
            if (up || dn) { g.over_cur = 1 - g.over_cur; g.sfx |= SFX_MOVE; }
            if (ok) {
                g.sfx |= SFX_SELECT;
                if (g.over_cur == 0) { retakeCurrentStage(&g); g.scene = SCENE_GAME; }
                else g.scene = SCENE_MENU;
            }
        } else if (ok) { g.sfx |= SFX_SELECT; g.scene = SCENE_MENU; }

        if (audio_ok && g.sfx) {
            for (int i = 0; i < 10; i++)
                if ((g.sfx & (1u << i)) && IsSoundValid(sfx[i])) { SetSoundVolume(sfx[i], (float)g.sfx_volume / 100.0f); PlaySound(sfx[i]); }
        }
        g.sfx = 0;

        /* Draw Active Screen */
        BeginDrawing();
        ClearBackground(BLACK);
        Rectangle bg_src = {0, 0, (float)bg.width, (float)bg.height}, bg_dst = {0, 0, SCREEN_W, SCREEN_H};

        if (g.scene == SCENE_MENU) {
            DrawTexturePro(bg, bg_src, bg_dst, (Vector2){0, 0}, 0, WHITE);
            const char *m[4] = {"NEW GAME", "CONTINUE", "SETTINGS", "EXIT"};
            drawCursorList(m, 4, g.menu_cur, 352, 40, GOLD);
            DrawRectangle(0, SCREEN_H - 44, SCREEN_W, 44, (Color){16, 16, 22, 220});
            outText(TextFormat("BEST SCORE: %04d", g.best_score), 22, SCREEN_H - 32, 22, GOLD, BLACK);
            const char *gt = TextFormat("GWA: %.2f %s", (double)g.best_gwa, laudeTag(g.best_gwa, true));
            outText(gt, SCREEN_W - MeasureText(gt, 22) - 22, SCREEN_H - 32, 22, (Color){116, 244, 204, 255}, BLACK);
        } else if (g.scene == SCENE_SETTINGS) {
            DrawTexturePro(bg, bg_src, bg_dst, (Vector2){0, 0}, 0, (Color){185, 185, 195, 255});
            DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){12, 14, 22, 145});
            centerText("AUDIO SETTINGS", 78, 44, GOLD, MAROON);
            DrawRectangle((SCREEN_W - 560) / 2, 164, 560, 240, (Color){20, 22, 34, 225});
            const char *r[3] = {TextFormat("MASTER VOLUME: [ < %3d%% > ]", g.master_volume),
                                TextFormat("SFX VOLUME:    [ < %3d%% > ]", g.sfx_volume), "BACK"};
            drawCursorList(r, 3, g.set_cur, 204, 64, GOLD);
            centerText("[LEFT / RIGHT]: ADJUST | [ENTER / ESC]: SAVE & EXIT", SCREEN_H - 34, 18, RAYWHITE, BLACK);
        } else if (g.scene == SCENE_BRIEFING) {
            DrawTexturePro(bg, bg_src, bg_dst, (Vector2){0, 0}, 0, (Color){130, 130, 145, 255});
            drawEthnicBox((Rectangle){(SCREEN_W - 680) / 2.0f, 120, 680, 320});
            DrawRectangle((SCREEN_W - 656) / 2, 138, 656, 284, (Color){24, 24, 34, 245});
            centerText(TextFormat("COURSE %d OF %d - %s (%s)", g.stage.course_id, TOTAL_STAGES, g.stage.code, g.stage.subtitle), 156, 20, SKYBLUE, BLACK);
            centerText(g.stage.title, 196, 26, GOLD, BLACK);
            centerText(g.stage.hint, 254, 17, RAYWHITE, BLACK);
            centerText(TextFormat("TIME: %.0fs | GRAVITY: %.2fx | GWA: %.2f", (double)g.stage.time_limit, (double)g.stage.gravity_scale, (double)g.isko.gwa), 312, 18, GREEN, BLACK);
            centerText("PRESS [ENTER] OR [E] TO BEGIN COURSE", 376, 22, GOLD, BLACK);
        } else if (g.scene == SCENE_GAME) {
            const Stage *s = &g.stage; const Player *p = &g.isko;
            Texture2D sbg = stage_bg[(s->course_id == 2) ? 1 : 0];
            DrawTexturePro(sbg, (Rectangle){0, 0, (float)sbg.width, (float)sbg.height},
                           (Rectangle){0, HUD_H, SCREEN_W, SCREEN_H - HUD_H}, (Vector2){0, 0}, 0, WHITE);
            for (int i = 0; i < s->platform_count; i++) {
                Rect2D br = s->platforms[i];
                if (br.y >= 510) {
                    for (int tx = (int)br.x; tx < (int)(br.x + br.w); tx += TILE_SIZE)
                        for (int ty = (int)br.y; ty < (int)(br.y + br.h); ty += TILE_SIZE)
                            DrawTexturePro(tile, (Rectangle){0,0,16,16}, (Rectangle){(float)tx,(float)ty,TILE_SIZE,TILE_SIZE}, (Vector2){0,0}, 0, WHITE);
                } else {
                    DrawRectangleRec((Rectangle){br.x, br.y, br.w, br.h}, (Color){138, 92, 54, 255});
                    DrawRectangle((int)br.x, (int)br.y, (int)br.w, 5, (Color){68, 174, 72, 255});
                    DrawRectangleLinesEx((Rectangle){br.x, br.y, br.w, br.h}, 2, BLACK);
                }
            }
            for (int i = 0; i < s->ladder_count; i++) {
                Rect2D l = s->ladders[i];
                DrawRectangle((int)l.x, (int)l.y, 4, (int)l.h, GOLD); DrawRectangle((int)(l.x + l.w - 4), (int)l.y, 4, (int)l.h, GOLD);
                for (int y = (int)l.y + 8; y < (int)(l.y + l.h); y += 14) DrawRectangle((int)l.x + 4, y, (int)l.w - 8, 4, GOLD);
            }
            for (int i = 0; i < s->plate_count; i++) {
                const Plate *pl = &s->plates[i];
                DrawRectangle((int)pl->rect.x, (int)pl->rect.y + (pl->active ? 3 : 0), (int)pl->rect.w, (int)pl->rect.h - (pl->active ? 3 : 0), pl->active ? GREEN : RED);
                outText(pl->label, (int)pl->rect.x, (int)pl->rect.y - 18, 13, WHITE, BLACK);
            }
            Color ec = s->terminal_online ? GREEN : RED;
            DrawRectangle((int)s->exit_terminal.x, (int)s->exit_terminal.y, 36, 48, (Color){34, 40, 52, 255});
            DrawRectangleLines((int)s->exit_terminal.x, (int)s->exit_terminal.y, 36, 48, ec);
            DrawRectangle((int)s->exit_terminal.x + 6, (int)s->exit_terminal.y + 8, 24, 18, ec);
            outText(s->terminal_online ? "EXIT ON" : "LOCKED", (int)s->exit_terminal.x - 6, (int)s->exit_terminal.y - 18, 13, ec, BLACK);
            if (s->has_crate) {
                DrawRectangle((int)s->crate.x, (int)s->crate.y, (int)s->crate.w, (int)s->crate.h, (Color){178, 116, 54, 255});
                DrawRectangleLines((int)s->crate.x, (int)s->crate.y, (int)s->crate.w, (int)s->crate.h, (Color){92, 52, 18, 255});
                DrawLine((int)s->crate.x, (int)s->crate.y, (int)(s->crate.x + s->crate.w), (int)(s->crate.y + s->crate.h), (Color){92, 52, 18, 255});
            }
            for (int i = 0; i < s->item_count; i++) {
                if (s->items[i].collected) continue;
                int x = (int)s->items[i].rect.x, y = (int)(s->items[i].rect.y + sinf(s->items[i].phase) * 4);
                if (s->items[i].kind == 0) { DrawRectangle(x, y, 20, 24, RAYWHITE); DrawRectangleLines(x, y, 20, 24, BLACK); DrawRectangle(x + 4, y + 5, 10, 3, MAROON); }
                else { DrawRectangle(x, y + 4, 18, 16, WHITE); DrawRectangle(x + 2, y + 6, 14, 4, BROWN); }
            }
            for (int i = 0; i < s->hazard_count; i++) {
                if (!s->hazards[i].alive) continue;
                Rect2D h = s->hazards[i].rect;
                if (s->hazards[i].kind == 1) DrawCircle((int)(h.x + 13), (int)(h.y + 13), 12, PURPLE);
                else { DrawRectangle((int)h.x + 2, (int)h.y + 2, 22, 14, (Color){148,56,36,255}); DrawRectangle((int)h.x + 6, (int)h.y + 16, 14, 10, (Color){224,188,142,255}); }
            }
            if (p->invuln_timer <= 0 || ((int)(p->invuln_timer * 14) % 2 == 0)) {
                Texture2D tx = isko[p->on_ladder ? 3 : (!p->is_grounded ? 2 : (fabsf(p->vx) > 10 ? p->anim_frame % 2 : 0))];
                DrawTexturePro(tx, (Rectangle){0, 0, (p->facing_right ? 1.0f : -1.0f) * tx.width, (float)tx.height}, (Rectangle){p->x - 2, p->y, p->w + 4, p->h}, (Vector2){0, 0}, 0, WHITE);
            }
            if (s->banner_timer > 0) { DrawRectangle((SCREEN_W - 560) / 2, HUD_H + 8, 560, 28, (Color){12,14,22,200}); centerText(s->banner, HUD_H + 13, 17, GOLD, BLACK); }
            outText("WASD/ARROWS: Move | SPACE: Jump | ESC/P: Pause | F9/F10: Demo", 20, SCREEN_H - 20, 12, RAYWHITE, BLACK);

            for (int y = 0; y < HUD_H; y += 8) for (int x = 0; x < SCREEN_W; x += 8)
                DrawRectangle(x, y, 8, 8, ((x / 8 + y / 8) % 2 == 0) ? (Color){214,218,224,255} : (Color){196,202,210,255});
            outText(s->code, 16, 10, 22, (Color){44,184,255,255}, BLACK); outText(s->subtitle, 16, 36, 16, (Color){76,218,255,255}, BLACK);
            outText(TextFormat("GWA: %.2f", (double)p->gwa), 146, 10, 20, p->gwa > 3 ? RED : (p->gwa > 1.75f ? YELLOW : GREEN), BLACK);
            outText(TextFormat("SCORE: %05d", g.total_score), 146, 36, 18, GOLD, BLACK);
            outText(TextFormat("SLIPS: %d/3", p->slips), 290, 12, 16, p->slips >= 2 ? RED : WHITE, BLACK);
            centerText(TextFormat("TIME: %02ds", (int)ceilf(s->time_remaining)), 18, 28, GOLD, BLACK);
            outText(TextFormat("PAPERS: %d/%d", p->papers, TOTAL_PAPERS), SCREEN_W - 216, 20, 22, WHITE, BLACK);
            DrawCircle(SCREEN_W - 28, 30, 12, DARKGRAY); DrawCircle(SCREEN_W - 28, 30, 5, LIGHTGRAY);
            if (g.paused) {
                DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){10, 10, 16, 175});
                drawEthnicBox((Rectangle){(SCREEN_W - 340) / 2.0f, 160, 340, 240});
                centerText("PAUSED", 184, 30, GOLD, BLACK);
                const char *po[4] = {"RESUME", "SETTINGS", "RETRY STAGE", "MAIN MENU"};
                drawCursorList(po, 4, g.pause_cur, 236, 36, GOLD);
            }
        } else if (g.scene == SCENE_GAMEOVER) {
            DrawTexturePro(bg, bg_src, bg_dst, (Vector2){0, 0}, 0, (Color){110, 100, 108, 255});
            DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){14, 8, 10, 150});
            Rectangle r = {(SCREEN_W - 560) / 2.0f, 76, 560, 310};
            DrawRectangleRec(r, (Color){248, 242, 226, 255}); DrawRectangleLinesEx(r, 6, (Color){118, 16, 22, 255});
            int cx = SCREEN_W / 2;
            DrawText("UNIVERSITY OF THE PHILIPPINES", cx - MeasureText("UNIVERSITY OF THE PHILIPPINES", 20) / 2, 102, 20, BLACK);
            DrawText("OFFICE OF THE UNIVERSITY REGISTRAR", cx - MeasureText("OFFICE OF THE UNIVERSITY REGISTRAR", 15) / 2, 128, 15, DARKGRAY);
            DrawText("NOTICE OF ACADEMIC DISMISSAL", cx - MeasureText("NOTICE OF ACADEMIC DISMISSAL", 24) / 2, 172, 24, MAROON);
            DrawText("Scholastic Deficiency: GWA 5.00", cx - MeasureText("Scholastic Deficiency: GWA 5.00", 20) / 2, 218, 20, BLACK);
            DrawText("Your GWA has left the chat.", cx - MeasureText("Your GWA has left the chat.", 16) / 2, 248, 16, DARKGRAY);
            DrawRectangleLinesEx((Rectangle){(float)cx - 92, 292, 184, 52}, 5, RED);
            DrawText("DROPPED", cx - MeasureText("DROPPED", 30) / 2, 304, 30, RED);
            DrawRectangle((SCREEN_W - 320) / 2, 412, 320, 106, BLACK);
            const char *go[2] = {"RETAKE COURSE", "MAIN MENU"};
            drawCursorList(go, 2, g.over_cur, 430, 42, ORANGE);
        } else {
            DrawTexturePro(bg, bg_src, bg_dst, (Vector2){0, 0}, 0, WHITE);
            Color cc[4] = {MAROON, GOLD, GREEN, SKYBLUE};
            for (int i = 0; i < 48; i++) DrawRectangle((i * 73 + (int)(g.global_timer * 45)) % SCREEN_W, (i * 47 + (int)(g.global_timer * (60 + (i % 5) * 14))) % SCREEN_H, 8, 8, cc[i % 4]);
            DrawRectangle(0, 0, SCREEN_W, 18, MAROON); DrawRectangle(0, 18, SCREEN_W, 6, GOLD);
            DrawRectangle(90, 96, SCREEN_W - 180, 356, (Color){16, 18, 28, 200});
            centerText("WEEK 3 CLEARED!", 126, 44, GOLD, MAROON);
            centerText(TextFormat("FINAL GWA: %.2f %s", (double)g.isko.gwa, laudeTag(g.isko.gwa, false)), 196, 28, SKYBLUE, BLACK);
            centerText(laudeQuote(g.isko.gwa), 258, 20, RAYWHITE, BLACK);
            centerText(TextFormat("Final Score: %06d", g.total_score), 324, 30, GOLD, BLACK);
            centerText("PRESS [ENTER] TO RETURN TO MAIN MENU", SCREEN_H - 36, 22, WHITE, BLACK);
        }
        EndDrawing();
        if (smoke && ++frames >= 5) g.should_exit = true;
    }

    UnloadTexture(bg); UnloadTexture(stage_bg[0]); UnloadTexture(stage_bg[1]); UnloadTexture(tile);
    for (int i = 0; i < 4; i++) UnloadTexture(isko[i]);
    if (audio_ok) { for (int i = 0; i < 10; i++) UnloadSound(sfx[i]); CloseAudioDevice(); }
    CloseWindow();
    return 0;
}
