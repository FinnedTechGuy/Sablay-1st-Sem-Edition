#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

#define SCREEN_W     960
#define SCREEN_H     576
#define HUD_H        64
#define TILE_SIZE    32
#define TOTAL_STAGES 2
#define TOTAL_PAPERS 3
#define SAVE_PATH    "save_profile.txt"

typedef struct { float x, y, w, h; } Rect2D;

typedef enum {
    SCENE_MENU = 0, SCENE_SETTINGS, SCENE_BRIEFING,
    SCENE_GAME, SCENE_GAMEOVER, SCENE_VICTORY
} SceneState;

enum {
    SFX_MOVE = 1 << 0, SFX_SELECT = 1 << 1, SFX_JUMP = 1 << 2,
    SFX_PAPER = 1 << 3, SFX_COFFEE = 1 << 4, SFX_SWITCH = 1 << 5,
    SFX_STOMP = 1 << 6, SFX_HURT = 1 << 7, SFX_CLEAR = 1 << 8,
    SFX_GAMEOVER = 1 << 9
};

typedef struct {
    float x, y, vx, vy, w, h, spawn_x, spawn_y;
    bool is_grounded, on_ladder, facing_right;
    float gwa, invuln_timer, anim_timer;
    int slips, papers, stomp_streak, anim_frame;
} Player;

typedef struct {
    Rect2D rect;
    int type; /* 0:crate, 1:seq1, 2:seq2 */
    bool active;
    float cooldown;
    char label[12];
} Plate;

typedef struct {
    Rect2D rect;
    int kind; /* 0:stress, 1:orb */
    float vx, min_x, max_x, base_y, phase;
    bool alive;
} Hazard;

typedef struct {
    Rect2D rect;
    int kind; /* 0:paper, 1:coffee */
    bool collected;
    float phase;
    int paper_id;
} Collectible;

typedef struct {
    int course_id, seq_step;
    char code[12], subtitle[12], title[48], hint[88], banner[64];
    float gravity_scale, time_limit, time_remaining, banner_timer;
    bool terminal_online;
    Rect2D exit_terminal;

    Rect2D platforms[12]; int platform_count;
    Rect2D ladders[4];    int ladder_count;
    Rect2D crate;         float crate_vy; bool has_crate;
    Plate plates[4];      int plate_count;
    Hazard hazards[4];    int hazard_count;
    Collectible items[4]; int item_count;
} Stage;

typedef struct { bool left, right, up, down, jump; } InputState;

typedef struct {
    SceneState scene, settings_back;
    bool should_exit, paused;
    int best_score, unlocked_stage, master_volume, sfx_volume;
    float best_gwa, entry_gwa, global_timer;
    int total_score, entry_score, entry_papers;
    bool paper_flags[TOTAL_PAPERS], entry_paper_flags[TOTAL_PAPERS];
    int menu_cur, set_cur, over_cur, pause_cur;
    uint32_t sfx;
    Player isko;
    Stage stage;
} GameState;

bool loadProfile(GameState *g, const char *path);
bool saveProfile(const GameState *g, const char *path);
void startNewRun(GameState *g, int start_stage);
void retakeCurrentStage(GameState *g);
void applyPenalty(GameState *g, float dgwa, int dslips);
void applyRecovery(GameState *g, float dgwa, int pts);
const char *laudeTag(float gwa, bool short_tag);
const char *laudeQuote(float gwa);
void loadStageRoom(GameState *g, int course_id);
void updateStageGameplay(GameState *g, const InputState *in, float dt);

#endif
