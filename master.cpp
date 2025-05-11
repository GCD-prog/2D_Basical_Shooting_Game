#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <math.h> 

// std::min, std::max の代替マクロ
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

// --- 定数定義 ---
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 540

#define PLAYER_SHOT_MAX 50 
#define ENEMY_MAX 30       
#define ENEMY_SHOT_MAX 150 
#define ITEM_MAX 10        
#define EXPLOSION_MAX 50   

#define PLAYER_SPEED 4
#define PLAYER_SHOT_SPEED 10
#define ENEMY_SHOT_SPEED_BASE 3 
#define PLAYER_INITIAL_LIFE 3
#define PLAYER_INITIAL_BOMBS 2
#define PLAYER_RESPAWN_INVINCIBLE_TIME 2000 
#define PLAYER_SHOT_COOL_TIME 150          
#define BOMB_DURATION 1500                 
#define BOMB_DAMAGE 150                    

// --- グローバル変数 ---
HWND hwnd;
HINSTANCE hinst;

HBITMAP	hBack_Bitmap;     
HBITMAP hChar_Bitmap;     
HBITMAP hCharInv_Bitmap;  
HBITMAP hPTama_Bitmap;    
HBITMAP hETama_Bitmap;    
HBITMAP hTeki_Bitmap[3];  
HBITMAP hScreen_Bitmap;   
HBITMAP hItem_Bitmap[2];  
HBITMAP hExplosion_Bitmap[2]; 
HBITMAP hLifeIcon_Bitmap; 

HDC win_hdc;	
HDC Back_DC;	      
HDC Char_DC;	      
HDC CharInv_DC;       
HDC PTama_DC;	      
HDC ETama_DC;         
HDC Teki_DC[3];       
HDC Screen_DC;	      
HDC Item_DC[2];       
HDC Explosion_DC[2];  
HDC LifeIcon_DC;      

enum GameState {
    TITLE,
    PLAYING,
    GAME_OVER
};
enum GameState current_state = TITLE; 
DWORD game_over_display_start_time = 0; 

typedef struct { 
	int x, y;
	int life;
	int bombs;
	int shot_power; 
	BOOL invincible; 
	DWORD invincible_start_time;
    DWORD last_shot_time; 
} player_info;
player_info player;

typedef struct {
	int x, y;
	int flg; 
} SHOT;
SHOT PlayerShot[PLAYER_SHOT_MAX]; 

typedef struct {
	int x, y;
	int hp;
	int type;         
	int move_pattern; 
	float vx, vy;     
	int shot_interval; 
	DWORD last_shot_time; 
	int flg;
} ENEMY;
ENEMY Enemy[ENEMY_MAX];

typedef struct {
	int x, y;
    float vx, vy; 
	int flg;
} ESHOT;
ESHOT EShot[ENEMY_SHOT_MAX];

typedef struct {
    int x, y;
    int type; 
    int flg;
} ITEM;
ITEM Item[ITEM_MAX];

typedef struct {
    int x, y;
    int type;      
    int current_frame;
    int total_frames; 
    int frame_delay;  
    int frame_delay_counter;
    int flg;
} EXPLOSION;
EXPLOSION Explosion[EXPLOSION_MAX];

typedef struct {
	int sy;
} Screen_info;
Screen_info screen;

long score = 0;
long hi_score = 0; 

int current_stage_number = 1;
DWORD stage_start_time_ms;

BOOL is_bomb_active = FALSE; 
DWORD bomb_activation_time_ms;

// --- 関数プロトタイプ宣言 ---
void InitGame();

void ResetPlayerStatus(BOOL full_reset); 


void UpdateGameLogic();

void DrawGameScreen();
void DrawTitleScreen();
void DrawGameOverScreen();

void LoadGameResources();
void ReleaseGameResources();

void SpawnEnemyWave(); 

BOOL LoadGameBitmapResource(HDC* target_dc_ptr, HBITMAP* target_hbmp_ptr, const char* filename, int width, int height);

void FirePlayerShot(); // ProcessPlayerInput から呼ばれるため先に宣言

void ActivateBomb();   // ProcessPlayerInput から呼ばれるため先に宣言

void FireEnemyShot(int enemy_idx); 

void CreateExplosion(int x, int y, int type); // 様々な場所から呼ばれる

void HandlePlayerHit(); // UpdateGameLogic から呼ばれる

void ProcessPlayerInput();

// --- 関数定義 ---

void DisplayFPS_Score_UI(HDC hdc)
{

    static DWORD fps_val = 0;
    static DWORD frame_counter = 0;
    static DWORD last_fps_update_time = 0;
    char buffer[256];
    int i; 

    frame_counter++;
    DWORD current_time_ms = GetTickCount();

    if (current_time_ms - last_fps_update_time > 1000) {
        fps_val = frame_counter;
        frame_counter = 0;
        last_fps_update_time = current_time_ms;
    }

    wsprintf(buffer, "FPS:%03ld STAGE:%d SCORE:%07ld HI:%07ld", fps_val, current_stage_number, score, hi_score);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 0));
    TextOut(hdc, 5, 5, buffer, strlen(buffer));

    for (i = 0; i < player.life; ++i) {
        TransparentBlt(hdc, 10 + i * (16 + 2), SCREEN_HEIGHT - 25, 16, 16, LifeIcon_DC, 0, 0, 16, 16, RGB(0,0,0));
    }
    wsprintf(buffer, "BOMB: %d", player.bombs);
    SetTextColor(hdc, RGB(255, 100, 100));
    TextOut(hdc, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 25, buffer, strlen(buffer));

}

void InitGame()
{

    int i; 
    srand((unsigned int)time(NULL));
    ResetPlayerStatus(TRUE); 
    score = 0;
    current_stage_number = 1;
    stage_start_time_ms = GetTickCount();
    is_bomb_active = FALSE;

    for (i = 0; i < PLAYER_SHOT_MAX; ++i) PlayerShot[i].flg = 0;
    for (i = 0; i < ENEMY_MAX; ++i) Enemy[i].flg = 0;
    for (i = 0; i < ENEMY_SHOT_MAX; ++i) EShot[i].flg = 0;
    for (i = 0; i < ITEM_MAX; ++i) Item[i].flg = 0;
    for (i = 0; i < EXPLOSION_MAX; ++i) Explosion[i].flg = 0;
    
    screen.sy = SCREEN_HEIGHT; 
    current_state = PLAYING;

}

void ResetPlayerStatus(BOOL full_reset)
{

    player.x = SCREEN_WIDTH / 2 - 16;
    player.y = SCREEN_HEIGHT - 80;
    player.invincible = TRUE;
    player.invincible_start_time = GetTickCount();
    player.last_shot_time = 0;

    if (full_reset) { 
        player.life = PLAYER_INITIAL_LIFE;
        player.bombs = PLAYER_INITIAL_BOMBS;
        player.shot_power = 0;
    } else { 
        player.shot_power = MAX(0, player.shot_power - 1); 
    }

}

void CreateExplosion(int x, int y, int type)
{

    int i; 
    for (i = 0; i < EXPLOSION_MAX; ++i) {
        if (Explosion[i].flg == 0) {
            Explosion[i].flg = 1;
            Explosion[i].x = x; 
            Explosion[i].y = y;
            Explosion[i].type = type;
            Explosion[i].current_frame = 0;
            Explosion[i].total_frames = 4; 
            Explosion[i].frame_delay_counter = 0;
            
            if (type == 0) { 
                Explosion[i].frame_delay = 2;  
            } else { 
                Explosion[i].frame_delay = 3;  
            }
            return;
        }
    }

}

void HandlePlayerHit() {

    if (player.invincible) return;

    player.life--;
    CreateExplosion(player.x, player.y, 1); 
    
    if (player.life <= 0) {
        current_state = GAME_OVER;
        game_over_display_start_time = GetTickCount();
        if (score > hi_score) hi_score = score;
    } else {
        ResetPlayerStatus(FALSE); 
    }

}

void ActivateBomb(){

    int i, k;

    if (player.bombs > 0 && !is_bomb_active) {
        player.bombs--;
        is_bomb_active = TRUE;
        bomb_activation_time_ms = GetTickCount();

        for (i = 0; i < ENEMY_SHOT_MAX; ++i) {
            if (EShot[i].flg) {
                CreateExplosion(EShot[i].x, EShot[i].y, 0); 
                EShot[i].flg = 0;
            }
        }

        for (i = 0; i < ENEMY_MAX; ++i) {
            if (Enemy[i].flg) {
                Enemy[i].hp -= BOMB_DAMAGE; 
                if (Enemy[i].hp <= 0) {
                    score += (Enemy[i].type + 1) * 150; 
                    CreateExplosion(Enemy[i].x, Enemy[i].y, 1);
                    if (rand() % (5 - Enemy[i].type) == 0) { 
                         for (k = 0; k < ITEM_MAX; ++k) {
                            if (Item[k].flg == 0) {
                                Item[k].flg = 1;
                                Item[k].x = Enemy[i].x + 8;
                                Item[k].y = Enemy[i].y + 8;
                                Item[k].type = rand() % 2; 
                                break;
                            }
                        }
                    }
                    Enemy[i].flg = 0;
                }
            }
        }
    }

}


void FirePlayerShot()
{

    DWORD current_time_ms = GetTickCount();
    int i; 

    if (current_time_ms - player.last_shot_time < PLAYER_SHOT_COOL_TIME) 
		return;

    player.last_shot_time = current_time_ms;

    for (i = 0; i < PLAYER_SHOT_MAX; ++i) {
        if (PlayerShot[i].flg == 0) {
            PlayerShot[i].flg = 1;
            PlayerShot[i].x = player.x + 16 - 8; 
            PlayerShot[i].y = player.y - 10;     
            break;
        }
    }

    if (player.shot_power >= 1) { 
        for (i = 0; i < PLAYER_SHOT_MAX; ++i) { 
            if (PlayerShot[i].flg == 0) {
                PlayerShot[i].flg = 1;
                PlayerShot[i].x = player.x + 16 - 8 - 12; 
                PlayerShot[i].y = player.y;
                break;
            }
        }
        for (i = 0; i < PLAYER_SHOT_MAX; ++i) { 
            if (PlayerShot[i].flg == 0) {
                PlayerShot[i].flg = 1;
                PlayerShot[i].x = player.x + 16 - 8 + 12; 
                PlayerShot[i].y = player.y;
                break;
            }
        }
    }
    if (player.shot_power >= 2) { 
        for (i = 0; i < PLAYER_SHOT_MAX; ++i) { 
            if (PlayerShot[i].flg == 0) {
                PlayerShot[i].flg = 1;
                PlayerShot[i].x = player.x + 16 - 8 - 24; 
                PlayerShot[i].y = player.y + 5; 
                break;
            }
        }
        for (i = 0; i < PLAYER_SHOT_MAX; ++i) { 
            if (PlayerShot[i].flg == 0) {
                PlayerShot[i].flg = 1;
                PlayerShot[i].x = player.x + 16 - 8 + 24; 
                PlayerShot[i].y = player.y + 5; 
                break;
            }
        }
    }

}

void ProcessPlayerInput()
{
    static BOOL x_key_pressed_last_frame = FALSE;
    BOOL x_key_currently_pressed; 

    if (GetAsyncKeyState(VK_UP) & 0x8000) {
        player.y -= PLAYER_SPEED;
        if (player.y < 0) player.y = 0;
    }
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
        player.y += PLAYER_SPEED;
        if (player.y > SCREEN_HEIGHT - 32) player.y = SCREEN_HEIGHT - 32; 
    }
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
        player.x -= PLAYER_SPEED;
        if (player.x < 0) player.x = 0;
    }
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
        player.x += PLAYER_SPEED;
        if (player.x > SCREEN_WIDTH - 32) player.x = SCREEN_WIDTH - 32; 
    }

    if (GetAsyncKeyState('Z') & 0x8000) {
        FirePlayerShot(); 
    }

    x_key_currently_pressed = (GetAsyncKeyState('X') & 0x8000) != 0;

    if (x_key_currently_pressed && !x_key_pressed_last_frame) {
        ActivateBomb();
    }
    x_key_pressed_last_frame = x_key_currently_pressed;

}


void FireEnemyShot(int enemy_idx)
{

    ENEMY* current_enemy = &Enemy[enemy_idx];
    DWORD current_time_ms = GetTickCount();
    int i, j; 
    float dx, dy, dist, base_speed, angle, angle_step, start_angle, spread_angle; 
    int shots_fired_count; 
    int num_burst_shots, ways; 

    if (current_enemy->flg == 0 || current_enemy->shot_interval == 0) return;

    if (current_time_ms - current_enemy->last_shot_time < (DWORD)current_enemy->shot_interval) return;

    current_enemy->last_shot_time = current_time_ms;
    base_speed = ENEMY_SHOT_SPEED_BASE + current_stage_number * 0.15f; 
    if (base_speed > ENEMY_SHOT_SPEED_BASE + 1.5f) base_speed = ENEMY_SHOT_SPEED_BASE + 1.5f; 

    shots_fired_count = 0; 

    if (current_enemy->type == 1) {
        num_burst_shots = 1 + current_stage_number / 3; 
        if (num_burst_shots > 3) num_burst_shots = 3;     

        for (j = 0; j < num_burst_shots && shots_fired_count < 4; ++j) { 
            for (i = 0; i < ENEMY_SHOT_MAX; ++i) {
                if (EShot[i].flg == 0) {
                    EShot[i].flg = 1;
                    EShot[i].x = current_enemy->x + 16 - 4;
                    EShot[i].y = current_enemy->y + 32 + j*2; 

                    dx = (float)(player.x + 16) - (current_enemy->x + 16);
                    dy = (float)(player.y + 16) - (current_enemy->y + 16);
                    dist = sqrtf(dx * dx + dy * dy);

                    if (dist > 0) {
                        if (num_burst_shots == 3) {
                             spread_angle = 0.20f * (j - 1) ; 
                        } else {
                             spread_angle = 0.0f; 
                        }
                        EShot[i].vx = ((dx / dist) * cosf(spread_angle) - (dy / dist) * sinf(spread_angle)) * base_speed;
                        EShot[i].vy = ((dx / dist) * sinf(spread_angle) + (dy / dist) * cosf(spread_angle)) * base_speed;
                    } else {
                        EShot[i].vx = 0;
                        EShot[i].vy = base_speed;
                    }
                    shots_fired_count++;
                    if (shots_fired_count >= num_burst_shots) break; 
                }
            }
             if (shots_fired_count >= num_burst_shots) break;
        }

    } else if (current_enemy->type == 2) {
        ways = 3 + current_stage_number / 2; 
        if (ways > 5) ways = 5; 
        if (ways % 2 == 0) ways++; 

        angle_step = 0.30f; 
        start_angle = -(angle_step * (ways / 2));

        for (j = 0; j < ways && shots_fired_count < 7; ++j) { 
            for (i = 0; i < ENEMY_SHOT_MAX; ++i) {
                if (EShot[i].flg == 0) {
                    EShot[i].flg = 1;
                    EShot[i].x = current_enemy->x + 16 - 4;
                    EShot[i].y = current_enemy->y + 32;

                    angle = start_angle + j * angle_step;

                    EShot[i].vx = sinf(angle) * base_speed; 
                    EShot[i].vy = cosf(angle) * base_speed;
                    shots_fired_count++;
                    if (shots_fired_count >= ways) break;
                }
            }
            if (shots_fired_count >= ways) break;
        }

    } else if (current_enemy->type == 0) {
        if (rand() % 8 == 0 && shots_fired_count < 1) { 
            for (i = 0; i < ENEMY_SHOT_MAX; ++i) {
                if (EShot[i].flg == 0) {
                    EShot[i].flg = 1;
                    EShot[i].x = current_enemy->x + 16 - 4;
                    EShot[i].y = current_enemy->y + 32;
                    EShot[i].vx = 0;
                    EShot[i].vy = base_speed * 0.6f; 
                    shots_fired_count++;
                    break;
                }
            }
        }
    }

}

void SpawnEnemyWave()
{

    DWORD time_elapsed_in_stage_ms = GetTickCount() - stage_start_time_ms;
    int i;
    BOOL enemy_exists;
    int spawn_check_interval; 

    spawn_check_interval = 2500 - current_stage_number * 200;
    if (spawn_check_interval < 1000) spawn_check_interval = 1000;

    if ((long)(time_elapsed_in_stage_ms / spawn_check_interval) > (long)((time_elapsed_in_stage_ms - (1000/60)) / spawn_check_interval) ) {
        if (rand() % (4 - MIN(current_stage_number, 3)) == 0) { 
            for (i = 0; i < ENEMY_MAX; ++i) {
                if (Enemy[i].flg == 0) {
                    Enemy[i].flg = 1;
                    Enemy[i].type = 0;
                    Enemy[i].x = rand() % (SCREEN_WIDTH - 32);
                    Enemy[i].y = -32;
                    Enemy[i].hp = 30 + current_stage_number * 10;
                    Enemy[i].vx = 0;
                    Enemy[i].vy = 1.5f + current_stage_number * 0.1f;
                    Enemy[i].shot_interval = 2500 + rand() % 1000; 
                    Enemy[i].last_shot_time = GetTickCount() + rand() % Enemy[i].shot_interval; 
                    break;
                }
            }
        }
    }

    if (time_elapsed_in_stage_ms > 3000 &&
        (long)(time_elapsed_in_stage_ms / (spawn_check_interval + 500)) > (long)((time_elapsed_in_stage_ms - (1000/60)) / (spawn_check_interval + 500)) ) {
         if (rand() % (5 - MIN(current_stage_number, 3)) == 0) { 
            for (i = 0; i < ENEMY_MAX; ++i) {
                if (Enemy[i].flg == 0) {
                    Enemy[i].flg = 1;
                    Enemy[i].type = 1;
                    Enemy[i].x = rand() % (SCREEN_WIDTH - 32);
                    Enemy[i].y = -32;
                    Enemy[i].hp = 50 + current_stage_number * 15;
                    Enemy[i].vx = 0;
                    Enemy[i].vy = 1.0f + current_stage_number * 0.05f;
                    Enemy[i].shot_interval = 2000 - current_stage_number * 100; 
                    if (Enemy[i].shot_interval < 800) Enemy[i].shot_interval = 800; 
                    Enemy[i].last_shot_time = GetTickCount() + rand() % Enemy[i].shot_interval;
                    break;
                }
            }
        }
    }

    if (time_elapsed_in_stage_ms > 8000 &&
        (long)(time_elapsed_in_stage_ms / (spawn_check_interval + 2000)) > (long)((time_elapsed_in_stage_ms - (1000/60)) / (spawn_check_interval + 2000))) {
        if (rand() % (7 - MIN(current_stage_number, 2)) == 0) { 
            for (i = 0; i < ENEMY_MAX; ++i) {
                if (Enemy[i].flg == 0) {
                    Enemy[i].flg = 1;
                    Enemy[i].type = 2;
                    Enemy[i].x = SCREEN_WIDTH / 3 + rand()%(SCREEN_WIDTH/3); 
                    Enemy[i].y = -64;
                    Enemy[i].hp = 200 + current_stage_number * 50;
                    Enemy[i].vx = (rand() % 2 == 0 ? 1.0f : -1.0f) * (0.3f + current_stage_number*0.01f); 
                    Enemy[i].vy = 0.5f + current_stage_number * 0.02f; 
                    Enemy[i].shot_interval = 2200 - current_stage_number * 150; 
                    if (Enemy[i].shot_interval < 1000) Enemy[i].shot_interval = 1000; 
                    Enemy[i].last_shot_time = GetTickCount() + rand() % Enemy[i].shot_interval;
                    break;
                }
            }
        }
    }

    if (time_elapsed_in_stage_ms > (DWORD)(35000 + (current_stage_number-1)*7000) ) {
        enemy_exists = FALSE;
        for(i=0; i < ENEMY_MAX; ++i) {
            if(Enemy[i].flg) {
                enemy_exists = TRUE;
                break;
            }
        }
        
        if (!enemy_exists) {
            current_stage_number++;
            stage_start_time_ms = GetTickCount();
        }
    }

}


void UpdateGameLogic()
{

    int i, k, item_idx; 
    RECT shot_r, enemy_r, intersect_r, player_r, eshot_r, player_pickup_r, item_r; 

    if (player.invincible) {
        if (GetTickCount() - player.invincible_start_time > PLAYER_RESPAWN_INVINCIBLE_TIME) {
            player.invincible = FALSE;
        }
    }

    if (is_bomb_active) {
        if (GetTickCount() - bomb_activation_time_ms > BOMB_DURATION) {
            is_bomb_active = FALSE;
        }
    }

    for (i = 0; i < PLAYER_SHOT_MAX; ++i) {
        if (PlayerShot[i].flg) {
            PlayerShot[i].y -= PLAYER_SHOT_SPEED;
            if (PlayerShot[i].y < -16) PlayerShot[i].flg = 0; 
        }
    }

    SpawnEnemyWave();

    for (i = 0; i < ENEMY_MAX; ++i) {
        if (Enemy[i].flg) {
            Enemy[i].x += (int)Enemy[i].vx;
            Enemy[i].y += (int)Enemy[i].vy;

            if (Enemy[i].type == 2) {
                if (Enemy[i].x < 0 || Enemy[i].x > SCREEN_WIDTH - 32) {
                    Enemy[i].vx *= -1;
                    Enemy[i].x = MAX(0, MIN(Enemy[i].x, SCREEN_WIDTH - 32));
                }
            }
            
            if (Enemy[i].y > SCREEN_HEIGHT || Enemy[i].x < -64 || Enemy[i].x > SCREEN_WIDTH + 32) { 
                Enemy[i].flg = 0;
            }
            FireEnemyShot(i); 
        }
    }

    for (i = 0; i < ENEMY_SHOT_MAX; ++i) {
        if (EShot[i].flg) {
            EShot[i].x += (int)EShot[i].vx;
            EShot[i].y += (int)EShot[i].vy;
            if (EShot[i].y < -16 || EShot[i].y > SCREEN_HEIGHT || EShot[i].x < -16 || EShot[i].x > SCREEN_WIDTH) {
                EShot[i].flg = 0;
            }
        }
    }
    
    for (i = 0; i < ITEM_MAX; ++i) {
        if (Item[i].flg) {
            Item[i].y += 2; 
            if (Item[i].y > SCREEN_HEIGHT) Item[i].flg = 0;
        }
    }

    for (i = 0; i < EXPLOSION_MAX; ++i) {
        if (Explosion[i].flg) {
            Explosion[i].frame_delay_counter++;
            if (Explosion[i].frame_delay_counter >= Explosion[i].frame_delay) {
                Explosion[i].frame_delay_counter = 0;
                Explosion[i].current_frame++;
                if (Explosion[i].current_frame >= Explosion[i].total_frames) {
                    Explosion[i].flg = 0; 
                }
            }
        }
    }

    for (i = 0; i < PLAYER_SHOT_MAX; ++i) {
        if (PlayerShot[i].flg) {
            shot_r.left = PlayerShot[i].x; shot_r.top = PlayerShot[i].y;
            shot_r.right = PlayerShot[i].x + 16; shot_r.bottom = PlayerShot[i].y + 16;

            for (k = 0; k < ENEMY_MAX; ++k) {
                if (Enemy[k].flg) {
                    enemy_r.left = Enemy[k].x; enemy_r.top = Enemy[k].y;
                    enemy_r.right = Enemy[k].x + 32; enemy_r.bottom = Enemy[k].y + 32;

                    if (IntersectRect(&intersect_r, &shot_r, &enemy_r)) {
                        PlayerShot[i].flg = 0;
                        Enemy[k].hp -= (10 + player.shot_power * 10); 
                        CreateExplosion(PlayerShot[i].x, PlayerShot[i].y, 0); 
                        if (Enemy[k].hp <= 0) {
                            score += (Enemy[k].type + 1) * 100;
                            Enemy[k].flg = 0;
                            CreateExplosion(Enemy[k].x, Enemy[k].y, 1); 
                            if (rand() % (5 - Enemy[k].type) == 0) { 
                                for (item_idx = 0; item_idx < ITEM_MAX; ++item_idx) {
                                    if (Item[item_idx].flg == 0) {
                                        Item[item_idx].flg = 1;
                                        Item[item_idx].x = Enemy[k].x + 8;
                                        Item[item_idx].y = Enemy[k].y + 8;
                                        Item[item_idx].type = rand() % 2; 
                                        break;
                                    }
                                }
                            }
                        }
                        break; 
                    }
                }
            }
        }
    }

    if (!player.invincible) {
        player_r.left = player.x + 8; player_r.top = player.y + 8;
        player_r.right = player.x + 32 - 8; player_r.bottom = player.y + 32 - 8; 

        for (i = 0; i < ENEMY_SHOT_MAX; ++i) {
            if (EShot[i].flg) {
                eshot_r.left = EShot[i].x; eshot_r.top = EShot[i].y;
                eshot_r.right = EShot[i].x + 8; eshot_r.bottom = EShot[i].y + 8; 
                
                if (IntersectRect(&intersect_r, &player_r, &eshot_r)) {
                    EShot[i].flg = 0;
                    HandlePlayerHit();
                    break; 
                }
            }
        }
        for (k = 0; k < ENEMY_MAX; ++k) {
            if (Enemy[k].flg) {
                enemy_r.left = Enemy[k].x + 4; enemy_r.top = Enemy[k].y + 4;
                enemy_r.right = Enemy[k].x + 32 - 4; enemy_r.bottom = Enemy[k].y + 32 - 4;
                
                if (IntersectRect(&intersect_r, &player_r, &enemy_r)) {
                    HandlePlayerHit(); 
                    break;
                }
            }
        }
    }
    
    player_pickup_r.left = player.x; player_pickup_r.top = player.y;
    player_pickup_r.right = player.x + 32; player_pickup_r.bottom = player.y + 32; 
    for (i = 0; i < ITEM_MAX; ++i) {
        if (Item[i].flg) {
            item_r.left = Item[i].x; item_r.top = Item[i].y;
            item_r.right = Item[i].x + 16; item_r.bottom = Item[i].y + 16;
            
            if (IntersectRect(&intersect_r, &player_pickup_r, &item_r)) {
                Item[i].flg = 0;
                score += 100; 
                if (Item[i].type == 0) { 
                    if (player.shot_power < 2) player.shot_power++;
                } else if (Item[i].type == 1) { 
                    if (player.bombs < 5) player.bombs++; 
                }
            }
        }
    }

}

void DrawGameScreen()
{

    int i; 
    int explosion_sprite_width, explosion_sprite_height, frame_x_on_sheet, frame_y_on_sheet, draw_x, draw_y; 
    DWORD time_since_bomb_activation; 
    HBRUSH hBrush; 
    RECT rc_fill; 

    BitBlt(Back_DC, 0, 0 - screen.sy, SCREEN_WIDTH, SCREEN_HEIGHT, Screen_DC, 0, 0, SRCCOPY);
    BitBlt(Back_DC, 0, SCREEN_HEIGHT - screen.sy, SCREEN_WIDTH, SCREEN_HEIGHT, Screen_DC, 0, 0, SRCCOPY);
    screen.sy -= 2; 
    if (screen.sy <= 0) screen.sy = SCREEN_HEIGHT;

    for (i = 0; i < ITEM_MAX; ++i) {
        if (Item[i].flg) {
            TransparentBlt(Back_DC, Item[i].x, Item[i].y, 16, 16, Item_DC[Item[i].type], 0, 0, 16, 16, RGB(0,0,0));
        }
    }

    for (i = 0; i < ENEMY_MAX; ++i) {
        if (Enemy[i].flg) {
            TransparentBlt(Back_DC, Enemy[i].x, Enemy[i].y, 32, 32, Teki_DC[Enemy[i].type % 3], 0, 0, 32, 32, RGB(0,0,0));
        }
    }
    
    for (i = 0; i < ENEMY_SHOT_MAX; ++i) {
        if (EShot[i].flg) {
            TransparentBlt(Back_DC, EShot[i].x, EShot[i].y, 8, 8, ETama_DC, 0, 0, 8, 8, RGB(0,0,0));
        }
    }

    for (i = 0; i < PLAYER_SHOT_MAX; ++i) {
        if (PlayerShot[i].flg) {
            TransparentBlt(Back_DC, PlayerShot[i].x, PlayerShot[i].y, 16, 16, PTama_DC, 0, 0, 16, 16, RGB(0,0,0));
        }
    }
    
    if (!(player.invincible && (GetTickCount() / 150 % 2 == 0))) {
        TransparentBlt(Back_DC, player.x, player.y, 32, 32, Char_DC, 0, 0, 32, 32, RGB(0,0,0));
    }

    for (i = 0; i < EXPLOSION_MAX; ++i) {
        if (Explosion[i].flg) {
            explosion_sprite_width = 32;  
            explosion_sprite_height = 32; 

            frame_x_on_sheet = Explosion[i].current_frame * explosion_sprite_width;
            frame_y_on_sheet = 0; 

            draw_x = Explosion[i].x - explosion_sprite_width / 2; 
            draw_y = Explosion[i].y - explosion_sprite_height / 2;
            
            TransparentBlt(Back_DC,
                           draw_x, draw_y,
                           explosion_sprite_width, explosion_sprite_height, 
                           Explosion_DC[Explosion[i].type], 
                           frame_x_on_sheet, frame_y_on_sheet, 
                           explosion_sprite_width, explosion_sprite_height, 
                           RGB(0,0,0)); 
        }
    }

    if (is_bomb_active) {
        time_since_bomb_activation = GetTickCount() - bomb_activation_time_ms;
        if (time_since_bomb_activation < BOMB_DURATION) {
            if ((time_since_bomb_activation / 100) % 2 == 0) { 
                hBrush = CreateSolidBrush(RGB(220, 220, 255)); 
                rc_fill.left = 0; rc_fill.top = 0;
                rc_fill.right = SCREEN_WIDTH; rc_fill.bottom = SCREEN_HEIGHT;
                FillRect(Back_DC, &rc_fill, hBrush);
                DeleteObject(hBrush);
            }
        }
    }

    DisplayFPS_Score_UI(Back_DC);

}

void DrawTitleScreen() {

    HFONT hFontTitle, hFontMsg, hOldFont; 
    char hi_score_text[50]; 
    RECT fill_area_rect; 

    fill_area_rect.left = 0;
    fill_area_rect.top = 0;
    fill_area_rect.right = SCREEN_WIDTH;
    fill_area_rect.bottom = SCREEN_HEIGHT;
    FillRect(Back_DC, &fill_area_rect, (HBRUSH)GetStockObject(BLACK_BRUSH));

    SetBkMode(Back_DC, TRANSPARENT);
    
    hFontTitle = CreateFont(60, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Impact");
    hFontMsg = CreateFont(30, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "MS Gothic");
    hOldFont = (HFONT)SelectObject(Back_DC, hFontTitle);

    SetTextColor(Back_DC, RGB(50, 150, 255));
    TextOut(Back_DC, SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/3, "バチクソSTG改", strlen("バチクソSTG改"));
    
    SelectObject(Back_DC, hFontMsg);
    SetTextColor(Back_DC, RGB(255, 255, 255));
    if(GetTickCount() / 500 % 2 == 0) { 
        TextOut(Back_DC, SCREEN_WIDTH/2 - 130, SCREEN_HEIGHT/2 + 50, "Press Z to Start", strlen("Press Z to Start"));
    }
    sprintf(hi_score_text, "HI-SCORE: %07ld", hi_score); 
    TextOut(Back_DC, SCREEN_WIDTH/2 - 120, SCREEN_HEIGHT/2 + 100, hi_score_text, strlen(hi_score_text));


    SelectObject(Back_DC, hOldFont); 
    DeleteObject(hFontTitle);
    DeleteObject(hFontMsg);

    if (GetAsyncKeyState('Z') & 0x8000) {
        InitGame(); 
    }

}

void DrawGameOverScreen()
{

    HFONT hFontGameOver, hFontScore, hOldFont; 
    char score_buf[100]; 

    SetBkMode(Back_DC, TRANSPARENT);
    hFontGameOver = CreateFont(72, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "Impact");
    hFontScore = CreateFont(40, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, "MS Gothic");
    hOldFont = (HFONT)SelectObject(Back_DC, hFontGameOver);

    SetTextColor(Back_DC, RGB(255, 0, 0));
    TextOut(Back_DC, SCREEN_WIDTH/2 - 200, SCREEN_HEIGHT/3, "GAME OVER", strlen("GAME OVER"));
    
    SelectObject(Back_DC, hFontScore);
    sprintf(score_buf, "YOUR SCORE: %07ld", score); 
    SetTextColor(Back_DC, RGB(255, 255, 0));
    TextOut(Back_DC, SCREEN_WIDTH/2 - 180, SCREEN_HEIGHT/2 + 20, score_buf, strlen(score_buf));

    if (GetTickCount() - game_over_display_start_time > 2000) { 
        if(GetTickCount() / 500 % 2 == 0) {
             SetTextColor(Back_DC, RGB(200, 200, 200));
             TextOut(Back_DC, SCREEN_WIDTH/2 - 150, SCREEN_HEIGHT/2 + 90, "Press Z to Title", strlen("Press Z to Title"));
        }
        if (GetAsyncKeyState('Z') & 0x8000) {
            current_state = TITLE;
        }
    }
    
    SelectObject(Back_DC, hOldFont);
    DeleteObject(hFontGameOver);
    DeleteObject(hFontScore);

}


void GameLoopRender()
{

    static DWORD last_frame_time = 0;
    DWORD current_time = GetTickCount();

    if (current_time - last_frame_time < 1000 / 80) { 
		// 約80FPSを目指す (約12.5ms per frame)
        // 非常に短いSleepは精度が低いので、ここでは何もしないビジーループの形に近い
        // Sleep(1); // CPU負荷を少し下げるため、入れても良い
        return;
    }

    last_frame_time = current_time;
				
    switch (current_state) {

        case TITLE:
            DrawTitleScreen();
            break;

        case PLAYING:
            ProcessPlayerInput();
            UpdateGameLogic();
            DrawGameScreen();
            break;

        case GAME_OVER:
            DrawGameScreen();    // ゲームオーバー時も背景や敵は描画し続ける（動きは止めるなど調整可）
            DrawGameOverScreen(); // その上にゲームオーバー表示

            break;

    }

    // バックバッファを表画面にコピー
    BitBlt(win_hdc, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Back_DC, 0, 0, SRCCOPY);

}

BOOL LoadGameBitmapResource(HDC* target_dc_ptr, HBITMAP* target_hbmp_ptr, const char* filename, int width, int height)
{

    char err_msg[256]; 
    *target_hbmp_ptr = (HBITMAP)LoadImage(hinst, filename, IMAGE_BITMAP, width, height, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

    if (*target_hbmp_ptr == NULL) {
        sprintf(err_msg, "Bitmap Load Error: %s (Error Code: %lu)", filename, GetLastError()); 
        MessageBox(hwnd, err_msg, "Error", MB_ICONERROR | MB_OK);
        return FALSE;
    }
    
    if (*target_dc_ptr == NULL) *target_dc_ptr = CreateCompatibleDC(win_hdc); 
    SelectObject(*target_dc_ptr, *target_hbmp_ptr);

    return TRUE;

}


void LoadGameResources()
{

    if (win_hdc == NULL && hwnd != NULL) win_hdc = GetDC(hwnd);

    if (win_hdc == NULL) { 
		MessageBox(NULL, "win_hdc is NULL in LoadGameResources", "Critical Error", MB_OK); 
		
		PostQuitMessage(1); 
		return; 
	}

    LoadGameBitmapResource(&Char_DC, &hChar_Bitmap, "Img\\player.bmp", 32, 32);
    LoadGameBitmapResource(&PTama_DC, &hPTama_Bitmap, "Img\\player_shot.bmp", 16, 16);
    LoadGameBitmapResource(&ETama_DC, &hETama_Bitmap, "Img\\enemy_shot.bmp", 8, 8);
    LoadGameBitmapResource(&Teki_DC[0], &hTeki_Bitmap[0], "Img\\enemy1.bmp", 32, 32);
    LoadGameBitmapResource(&Teki_DC[1], &hTeki_Bitmap[1], "Img\\enemy2.bmp", 32, 32);
    LoadGameBitmapResource(&Teki_DC[2], &hTeki_Bitmap[2], "Img\\enemy3.bmp", 32, 32);
    LoadGameBitmapResource(&Screen_DC, &hScreen_Bitmap, "Img\\background.bmp", SCREEN_WIDTH, SCREEN_HEIGHT);
    LoadGameBitmapResource(&Item_DC[0], &hItem_Bitmap[0], "Img\\item_powerup.bmp", 16, 16);
    LoadGameBitmapResource(&Item_DC[1], &hItem_Bitmap[1], "Img\\item_bomb.bmp", 16, 16);
    LoadGameBitmapResource(&Explosion_DC[0], &hExplosion_Bitmap[0], "Img\\explosion_small_strip.bmp", 128, 32); 
    LoadGameBitmapResource(&Explosion_DC[1], &hExplosion_Bitmap[1], "Img\\explosion_large_strip.bmp", 128, 32); 
    LoadGameBitmapResource(&LifeIcon_DC, &hLifeIcon_Bitmap, "Img\\life_icon.bmp", 16, 16);

}

void ReleaseGameResources()
{

    int i; 

    if (hBack_Bitmap) {
        DeleteObject(hBack_Bitmap);
        hBack_Bitmap = NULL;
    }

    if (hChar_Bitmap) {
        DeleteObject(hChar_Bitmap);
        hChar_Bitmap = NULL;
    }

    if (hPTama_Bitmap) {
        DeleteObject(hPTama_Bitmap);
        hPTama_Bitmap = NULL;
    }

    if (hETama_Bitmap) {
        DeleteObject(hETama_Bitmap);
        hETama_Bitmap = NULL;
    }

    for (i = 0; i < 3; ++i) {
        if (hTeki_Bitmap[i]) {
            DeleteObject(hTeki_Bitmap[i]);
            hTeki_Bitmap[i] = NULL;
        }
    }

    if (hScreen_Bitmap) {
        DeleteObject(hScreen_Bitmap);
        hScreen_Bitmap = NULL;
    }

    for (i = 0; i < 2; ++i) {
        if (hItem_Bitmap[i]) {
            DeleteObject(hItem_Bitmap[i]);
            hItem_Bitmap[i] = NULL;
        }
    }

    for (i = 0; i < 2; ++i) {
        if (hExplosion_Bitmap[i]) {
            DeleteObject(hExplosion_Bitmap[i]);
            hExplosion_Bitmap[i] = NULL;
        }
    }

    if (hLifeIcon_Bitmap) {
        DeleteObject(hLifeIcon_Bitmap);
        hLifeIcon_Bitmap = NULL;
    }

    if (Back_DC) {
        DeleteDC(Back_DC);
        Back_DC = NULL;
    }

    if (Char_DC) {
        DeleteDC(Char_DC);
        Char_DC = NULL;
    }

    if (PTama_DC) {
        DeleteDC(PTama_DC);
        PTama_DC = NULL;
    }

    if (ETama_DC) {
        DeleteDC(ETama_DC);
        ETama_DC = NULL;
    }

    for (i = 0; i < 3; ++i) {
        if (Teki_DC[i]) {
            DeleteDC(Teki_DC[i]);
            Teki_DC[i] = NULL;
        }
    }

    if (Screen_DC) {
        DeleteDC(Screen_DC);
        Screen_DC = NULL;
    }

    for (i = 0; i < 2; ++i) {
        if (Item_DC[i]) {
            DeleteDC(Item_DC[i]);
            Item_DC[i] = NULL;
        }
    }

    for (i = 0; i < 2; ++i) {
        if (Explosion_DC[i]) {
            DeleteDC(Explosion_DC[i]);
            Explosion_DC[i] = NULL;
        }
    }

    if (LifeIcon_DC) {
        DeleteDC(LifeIcon_DC);
        LifeIcon_DC = NULL;
    }

    if (hwnd && win_hdc) { 
        ReleaseDC(hwnd, win_hdc);
    }

    win_hdc = NULL;

}

int Window_Center(HWND hWndParam)
{

	RECT desktop;
	RECT window_rect;
	
	GetWindowRect(GetDesktopWindow(), (LPRECT)&desktop);
	GetWindowRect(hWndParam, (LPRECT)&window_rect); 

	SetWindowPos(
		hWndParam, HWND_TOP, 
		(desktop.right - (window_rect.right-window_rect.left)) / 2,
		(desktop.bottom - (window_rect.bottom-window_rect.top)) / 2,
		(window_rect.right-window_rect.left), (window_rect.bottom-window_rect.top),
		SWP_SHOWWINDOW);

	return 0;

}

LRESULT CALLBACK WindowFunc(HWND hwnd_param,UINT message,WPARAM wParam,LPARAM lParam) 
{ 

	switch(message){

		case WM_DESTROY:

            ReleaseGameResources(); 
			PostQuitMessage(0);
			break;

		default:

			return DefWindowProc(hwnd_param,message,wParam,lParam); 

	}

	return 0;

}

int WINAPI WinMain(HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpszArgs, int nWinMode)
{
    WNDCLASSEX wc;
    char className[]="BachikusoSTGModifiedC"; 
    MSG msg;                    

    hinst = hThisInst; 

    wc.cbSize=sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;                                         
    wc.lpfnWndProc = WindowFunc;
    wc.cbClsExtra = 0; wc.cbWndExtra = 0;
    wc.hInstance=hThisInst;
    wc.hIcon=LoadIcon(NULL,IDI_APPLICATION); wc.hIconSm=LoadIcon(NULL,IDI_APPLICATION);
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); 
    wc.lpszMenuName = NULL;	wc.lpszClassName = className;                           
    
    if(!RegisterClassEx(&wc)) return 0;
    
    hwnd = CreateWindow( 
        className, "バチクソ シューティングゲーム",
        (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX) | WS_VISIBLE, 
        CW_USEDEFAULT, CW_USEDEFAULT, 
        SCREEN_WIDTH + GetSystemMetrics(SM_CXFIXEDFRAME)*2, 
        SCREEN_HEIGHT + GetSystemMetrics(SM_CYFIXEDFRAME)*2 + GetSystemMetrics(SM_CYCAPTION), 
        NULL, NULL, hThisInst, NULL);                    

    if (!hwnd) { MessageBox(NULL, "ウィンドウ作成失敗", "エラー", MB_OK); return 0; }

    Window_Center(hwnd); 
    UpdateWindow(hwnd); 

    win_hdc=GetDC(hwnd); 

    Back_DC=CreateCompatibleDC(win_hdc);
    hBack_Bitmap=CreateCompatibleBitmap(win_hdc, SCREEN_WIDTH, SCREEN_HEIGHT);
    SelectObject(Back_DC, hBack_Bitmap);
    
    LoadGameResources(); 

    current_state = TITLE; 

    while(TRUE){ 
        if(PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)){

            if(!GetMessage(&msg,NULL,0,0)) break; 

            TranslateMessage(&msg);
            DispatchMessage(&msg);

        }else{

            GameLoopRender(); 

        }
    }

    return (int)msg.wParam;

}
