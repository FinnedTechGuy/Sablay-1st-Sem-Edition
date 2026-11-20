#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

#define SCREEN_W     960
#define SCREEN_H     576
#define HUD_H        64
#define TILE_SIZE    32
#define TOTAL_STAGES 2
#define TOTAL_PAPERS 3
#define SAVE_PATH    "save_profile.txt"

typedef struct {
    float x;
    float y;
    float w;
    float h;
} Rect2D;

typedef enum {
    SCENE_MENU = 0,
    SCENE_SETTINGS,
    SCENE_BRIEFING,
    SCENE_GAME,
    SCENE_GAMEOVER,
    SCENE_VICTORY
} SceneState;

typedef enum {
    SFX_MOVE = 0,
    SFX_SELECT,
    SFX_JUMP,
    SFX_PAPER,
    SFX_COFFEE,
    SFX_SWITCH,
    SFX_STOMP,
    SFX_HURT,
    SFX_CLEAR,
    SFX_GAMEOVER,
    SFX_COUNT
} SfxId;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    float w;
    float h;
    float spawn_x;
    float spawn_y;
    bool is_grounded;
    bool on_ladder;
    bool facing_right;
    float gwa;
    float invuln_timer;
    float anim_timer;
    int anim_frame;
    int slips;
    int papers;
    int stomp_streak;
} Player;

typedef struct {
    Rect2D rect;
    int type;       /* 0 = crate plate, 1 = step 1, 2 = step 2 */
    bool active;
    float cooldown;
    char label[12];
} Plate;

typedef struct {
    Rect2D rect;
    int kind;       /* 0 = ground stress, 1 = floating orb */
    float vx;
    float min_x;
    float max_x;
    float base_y;
    float phase;
    bool alive;
} Hazard;

typedef struct {
    Rect2D rect;
    int kind;       /* 0 = paper, 1 = coffee */
    bool collected;
    float phase;
    int paper_id;
} Collectible;

typedef struct {
    int course_id;
    int seq_step;
    char code[12];
    char subtitle[12];
    char title[48];
    char hint[88];
    char banner[64];
    float gravity_scale;
    float time_limit;
    float time_remaining;
    float banner_timer;
    bool terminal_online;
    Rect2D exit_terminal;

    Rect2D platforms[12];
    int platform_count;
    Rect2D ladders[4];
    int ladder_count;
    Rect2D crate;
    float crate_vy;
    bool has_crate;
    Plate plates[4];
    int plate_count;
    Hazard hazards[4];
    int hazard_count;
    Collectible items[4];
    int item_count;
} Stage;

typedef struct {
    bool left;
    bool right;
    bool up;
    bool down;
    bool jump;
} InputState;

typedef struct {
    SceneState scene;
    SceneState settings_back;
    bool should_exit;
    bool paused;

    int best_score;
    int unlocked_stage;
    int master_volume;
    int sfx_volume;
    float best_gwa;

    float entry_gwa;
    int entry_score;
    int entry_papers;
    bool entry_paper_flags[TOTAL_PAPERS];

    int total_score;
    bool paper_flags[TOTAL_PAPERS];
    float global_timer;

    int menu_cursor;
    int settings_cursor;
    int gameover_cursor;
    int pause_cursor;

    bool play_sfx[SFX_COUNT];
    Player isko;
    Stage stage;
} GameState;

bool loadProfile(GameState *game, const char *path);
bool saveProfile(const GameState *game, const char *path);
void startNewRun(GameState *game, int start_stage);
void retakeCurrentStage(GameState *game);
void applyPenalty(GameState *game, float gwa_delta, int slip_delta);
void applyRecovery(GameState *game, float gwa_delta, int points);
const char *laudeTag(float gwa, bool short_tag);
const char *laudeQuote(float gwa);
void loadStageRoom(GameState *game, int course_id);
void updateStageGameplay(GameState *game, const InputState *input, float dt);

#endif
