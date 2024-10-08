
#include <stdlib.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>

// include NESLIB header
#include "neslib.h"

// include CC65 NES Header (PPU)
#include <nes.h>

// link the pattern table into CHR ROM
//#link "chr_generic.s"

// BCD arithmetic support
#include "bcd.h"
//#link "bcd.c"

// VRAM update buffer
#include "vrambuf.h"
//#link "vrambuf.c"

#include "apu.h"
//#link "apu.c"

/*{pal:"nes",layout:"nes"}*/
const char PALETTE[32] = { 
  0x0F,			// screen color

  0x0C,0x30,0x2C,0x00,	// background palette 0
  0x1C,0x20,0x2C,0x00,	// background palette 1
  0x00,0x10,0x20,0x00,	// background palette 2
  0x06,0x16,0x26,0x00,   // background palette 3

  0x16,0x35,0x24,0x00,	// sprite palette 0
  0x00,0x37,0x25,0x00,	// sprite palette 1
  0x0D,0x2D,0x3A,0x00,	// sprite palette 2
  0x0D,0x27,0x2A	// sprite palette 3
};

#define TILE_SIZE 16
#define PLAYER_SPEED 1
#define MAX_BULLETS 6
#define MAX_STARS 15
#define STAR_TILE 0x18
#define SPAWN_DELAY 250

typedef struct {
  byte x;
  byte y;
  byte tile;
  byte attr;
  byte active;
  char direction;
} Sprite;

typedef struct {
  byte x;
  byte y;
  byte active;
} Star;

const char melody[] = {
  0x300, 0x08,
  0x300, 0x08,
  0x200, 0x08,
  0x100, 0x08,
  0x200, 0x08,
  0x300, 0x08,
  0x050, 0x08,
  0x100, 0x08,
  0x200, 0x08,
  0x300, 0x08,
  0x050, 0x08,
  0x100, 0x08,
  0x150, 0x08,
  0x100, 0x08,
  0x200, 0x08,
};

const int note_table_41[64] = {
4318, 4076, 3847, 3631, 3427, 3235, 3053, 2882, 2720, 2567, 2423, 2287, 2159, 2037, 1923, 1815, 1713, 1617, 1526, 1440, 1360, 1283, 1211, 1143, 1079, 1018, 961, 907, 856, 808, 763, 720, 679, 641, 605, 571, 539, 509, 480, 453, 428, 403, 381, 359, 339, 320, 302, 285, 269, 254, 240, 226, 213, 201, 190, 179, 169, 160, 151, 142, 134, 126, 119, 113, };

// Namespace(bias=1.0, freq=111860.8, length=64, maxbits=13, upper=49)
// 440.5 1.79281159771 49
const int note_table_49[64] = {
4304, 4062, 3834, 3619, 3416, 3224, 3043, 2872, 2711, 2559, 2415, 2279, 2151, 2031, 1917, 1809, 1707, 1611, 1521, 1436, 1355, 1279, 1207, 1139, 1075, 1015, 958, 904, 853, 805, 760, 717, 677, 639, 603, 569, 537, 507, 478, 451, 426, 402, 379, 358, 338, 319, 301, 284, 268, 253, 239, 225, 213, 201, 189, 179, 168, 159, 150, 142, 134, 126, 119, 112, };

// Namespace(bias=1.0, freq=111860.8, length=64, maxbits=12, upper=63)
// 443.6 14.2328382554 63
const int note_table_63[64] = {
2137, 4034, 3807, 3593, 3392, 3201, 3022, 2852, 2692, 2541, 2398, 2263, 2136, 2016, 1903, 1796, 1695, 1600, 1510, 1425, 1345, 1270, 1199, 1131, 1068, 1008, 951, 898, 847, 800, 755, 712, 672, 634, 599, 565, 533, 503, 475, 448, 423, 399, 377, 356, 336, 317, 299, 282, 266, 251, 237, 224, 211, 199, 188, 177, 167, 158, 149, 141, 133, 125, 118, 111, };

// Namespace(bias=-1.0, freq=55930.4, length=64, maxbits=12, upper=53)
// 443.7 8.47550713772 53
const int note_table_tri[64] = {
2138, 2018, 1905, 1798, 1697, 1602, 1512, 1427, 1347, 1272, 1200, 1133, 1069, 1009, 953, 899, 849, 801, 756, 714, 674, 636, 601, 567, 535, 505, 477, 450, 425, 401, 379, 358, 338, 319, 301, 284, 268, 253, 239, 226, 213, 201, 190, 179, 169, 160, 151, 142, 135, 127, 120, 113, 107, 101, 95, 90, 85, 80, 76, 72, 68, 64, 60, 57, };


#define NOTE_TABLE note_table_49
#define BASS_NOTE 36

byte music_index = 0;
byte cur_duration = 0;

const byte music1[]; // music data -- see end of file
const byte* music_ptr = music1;

byte next_music_byte() {
  return *music_ptr++;
}

void play_music() {
  static byte chs = 0;
  if (music_ptr) {
    // run out duration timer yet?
    while (cur_duration == 0) {
      // fetch next byte in score
      byte note = next_music_byte();
      // is this a note?
      if ((note & 0x80) == 0) {
        // pulse plays higher notes, triangle for lower if it's free
        if (note >= BASS_NOTE || (chs & 4)) {
          int period = NOTE_TABLE[note & 63];
          // see which pulse generator is free
          if (!(chs & 1)) {
            APU_PULSE_DECAY(0, period, DUTY_25, 2, 10);
            chs |= 1;
          } else if (!(chs & 2)) {
            APU_PULSE_DECAY(1, period, DUTY_25, 2, 10);
            chs |= 2;
          }
        } else {
          int period = note_table_tri[note & 63];
          APU_TRIANGLE_LENGTH(period, 15);
          chs |= 4;
        }
      } else {
        // end of score marker
        if (note == 0xff)
          music_ptr = NULL;
        // set duration until next note
        cur_duration = note & 63;
        // reset channel used mask
        chs = 0;
      }
    }
    cur_duration--;
  }
}

void start_music(const byte* music) {
  music_ptr = music;
  cur_duration = 0;
}

Sprite player = {128, 120, 0x1F, 0x03, 1, 0};
Sprite bullets[MAX_BULLETS];
Sprite enemy = {140, 100, 0x01, 0x04, 1, 0};
Sprite item = {0, 0, 0x15, 0x03, 1, 0};
byte frame_counter = 0;
byte i = 0;
byte j = 0;
byte direction = 0;
byte score[6] = {'0','0','0','0','0','0'};
Star stars[MAX_STARS];
byte scroll_x = 0;
byte player_x = 120;
byte enemy_timer = 0; // Timer to count frames
byte enemy_spawn_pending = 0; // Flag to indicate that an enemy spawn is pending
int item_timer = 0;
byte item_spawn_pending = 0;

void play_sound_pulse1(word period, byte duty, byte decay, byte length) {
  APU_ENABLE(0x01);
  APU_PULSE_DECAY(0, period, duty, decay, length);
}

void play_sound_pulse2(word period, byte duty, byte decay, byte length) {
  APU_ENABLE(0x02);
  APU_PULSE_DECAY(1, period, duty, decay, length);
}

void play_sound_triangle(word period, byte length) {
  APU_ENABLE(0x03);
  APU_TRIANGLE_LENGTH(period, length);
}

void play_sound_noise(byte period, byte decay, byte length) {
  APU_ENABLE(0x04);
  APU_NOISE_DECAY(period, decay, length);
}

// setup PPU and tables
void setup_graphics() {
  // clear sprites
  oam_clear();
  // set palette colors
  pal_all(PALETTE);
}

void update_hud() {
  // Update the score display on screen
  ppu_off();
  vram_adr(NTADR_A(2, 2));  // Position the HUD in a fixed position
  vram_write("1UP", 3);
  vram_adr(NTADR_A(2, 3));  // Update the score in the fixed position
  vram_write(score, 6);
  ppu_on_all();
}

// Function to initialize stars
void init_stars() {
  for (i = 0; i < MAX_STARS; i++) {
    stars[i].x = rand() % 256;
    stars[i].y = rand() % 240;
    stars[i].active = 1;
  }
}

void rand_enemy(){
  enemy.x = rand() % 256;
  enemy.y = rand() % 240;
  enemy.active = 1;
  enemy_timer = 0; // Reset the timer when a new enemy is spawned
  enemy_spawn_pending = 0; // Clear the spawn pending flag
}

void rand_item(){
  item.x = rand() % 256;
  item.y = rand() % 240;
  item.active = 1;
  item_timer = 0;
  item_spawn_pending = 0;
}

void update_stars() {
  byte oam_index = 16; // Start index for stars
  for (i = 0; i < MAX_STARS; i++) {
    if (stars[i].active) {
      byte star_x = stars[i].x - scroll_x;
      if (star_x >= 255) {
        star_x += 256;  // Wrap around
      }
      oam_index = oam_spr(star_x, stars[i].y, STAR_TILE, 0x02, oam_index);
    }
  }
}

void update_enemy() {
  if (enemy.active) {
    byte enemy_x = enemy.x - scroll_x;
    if (enemy_x >= 255 || enemy_x <= 0) {
      enemy.active = 0; // Deactivate enemy if off-screen
      enemy.x = 0;
      enemy.y = 0;
    } else {
      oam_spr(enemy_x, enemy.y, enemy.tile, enemy.attr, 8);
    }
  }
}

void update_background_scroll() {
  char pad = pad_poll(0);
  for(i = 0; i < MAX_STARS; i++){
    if (player.direction == PAD_LEFT && pad == PAD_LEFT) {
      stars[i].x++;
    } else if (player.direction == PAD_RIGHT && pad == PAD_RIGHT) {
      stars[i].x--;
    }
  }
  if (player.direction == PAD_LEFT && pad == PAD_LEFT) {
    enemy.x++;
    item.x++;
  } else if (player.direction == PAD_RIGHT && pad == PAD_RIGHT) {
    enemy.x--;
    item.x--;
  }
}

void increment_score(char* points) {
  int carry = 0;
  int len = strlen(points);
  int index = 6;  // The length of the score array (6 digits)

  // Start from the least significant digit (rightmost)
  for (i = 0; i < len; i++) {
    // Convert the score array and points to integers, and add them
    int sum = (score[index - 1 - i] - '0') + (points[len - 1 - i] - '0') + carry;
    carry = sum / 10;  // Calculate carry
    score[index - 1 - i] = (sum % 10) + '0';  // Store the result back in score
  }

  // Handle any remaining carry
  for (i = len; i < 6 && carry > 0; i++) {
    int sum = (score[index - 1 - i] - '0') + carry;
    carry = sum / 10;
    score[index - 1 - i] = (sum % 10) + '0';
  }
  update_hud();
}

void shoot_bullet() {
  
  ppu_off();
  for (i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) {
      bullets[i].x = player.x;
      bullets[i].y = player.y;
      bullets[i].tile = 0xAF;
      bullets[i].attr = 0x01;
      bullets[i].active = 1;
      bullets[i].direction = player.direction;
      break;
    }
  }
  ppu_on_all();
}

void get_item(){
  if (item.active){ 
    if(player.x < item.x + 8 && player.x + 8 > item.x &&
       player.y < item.y + 8 && player.y + 8 > item.y){
    item.active = 0;
    item.x = 0;
    item.y = 0;     
    increment_score("300");
    play_sound_pulse1(0x200, 0x80, 0x0A, 0x01);
   }
  }
}

void update_bullets() {
  for (i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      switch (bullets[i].direction) {
        case PAD_LEFT: bullets[i].x -= 1; break;
        case PAD_RIGHT: bullets[i].x += 1; break;
        case PAD_UP: bullets[i].y -= 1; break;
        case PAD_DOWN: bullets[i].y += 1; break;
      }
      // Collision check with enemy
      if (enemy.active && 
          bullets[i].x < enemy.x + 8 && bullets[i].x + 8 > enemy.x &&
    	  bullets[i].y < enemy.y + 8 && bullets[i].y + 8 > enemy.y) {
        enemy.active = 0; // Deactivate enemy
        bullets[i].active = 0; // Deactivate bullet
        enemy.x = 0;
        enemy.y = 0;
        increment_score("500"); // Increase score
        play_sound_pulse2(0x800, 0x80, 0x0F, 0x01);
      }
      if(bullets[i].x >= 255 || bullets[i].y >= 240 || bullets[i].x <= 0 || bullets[i].y <= 0){
        bullets[i].active = 0; // Deactivate bullet if it goes off screen
      }
      oam_spr(bullets[i].x, bullets[i].y, bullets[i].tile, bullets[i].attr, i + 32);
    }
  }
}

void move_up_and_down(){
  if(enemy.active){
    if (direction && enemy.y < 120) {
      enemy.tile = 0x1D;
      enemy.y++;
      // Change direction when reaching the right boundary
      if (enemy.y >= 120) {
        direction = 0; // Start moving left
      }
    } 
    // Move left
    else if (!direction && enemy.y > 80) {
      enemy.tile = 0x1C;
      enemy.y--;
      // Change direction when reaching the left boundary
      if (enemy.y <= 80) {
        direction = 1; // Start moving right
      }
    }
  }
}
void main()
{
  setup_graphics();
  apu_init();
  music_ptr = 0;
  // Call this function in your setup code
  init_stars();
  // enable rendering
  update_hud();
  rand_item();
  ppu_on_all();
  frame_counter = 0;
  player.direction = PAD_RIGHT;
  // infinite loop
  while(1) {
    char pad = pad_poll(0);
    //for(j = 0; j < 17; j++){
      //play_sound_pulse1(melody[j], 0x10, 0x02, 0x0F);
    //}
    if (!music_ptr) start_music(music1);
    waitvsync();
    play_music();
    
    oam_spr(player.x, player.y, player.tile, player.attr, 4);
    oam_spr(enemy.x, enemy.y, enemy.tile, enemy.attr, 8);
    oam_spr(item.x, item.y, item.tile, item.attr, 16);
    get_item();
    frame_counter++;
    if(frame_counter >= PLAYER_SPEED){
      move_up_and_down();
      if(pad & PAD_LEFT){
        player.direction = PAD_LEFT;
        player.tile = 0x1E;
      }
      if(pad & PAD_RIGHT){
        player.direction = PAD_RIGHT;
        player.tile = 0x1F;
      }
      if(pad & PAD_UP){
        player.y--;
        player.direction = PAD_UP;
        player.tile = 0x1C;
      }
      if(pad & PAD_DOWN){
        player.y++;
        player.direction = PAD_DOWN;
        player.tile = 0x1D;
      }
      if(pad & PAD_A){
        shoot_bullet();
        play_sound_pulse1(0x075, 0x40, 0x06, 0x01);
        //play_sound_pulse2(0x075, 0x40, 0x06, 0x01);
        //play_sound_noise(0x0F, 0x02, 0x0F);
        //play_sound_triangle(0x200, 0x0F);
      }
      update_background_scroll();
      update_stars();
      update_enemy();
      frame_counter = 0;
      // Check if the enemy is off-screen or destroyed
      if (!enemy.active && !enemy_spawn_pending) {
        enemy_spawn_pending = 1; // Set flag to indicate pending spawn
        enemy_timer = 0; // Reset the timer
      }

      // Handle enemy spawning delay
      if (enemy_spawn_pending) {
        enemy_timer++;
        if (enemy_timer >= SPAWN_DELAY) {
          rand_enemy(); // Spawn new enemy
        }
      }
      // Check if the enemy is off-screen or destroyed
      if (!item.active && !item_spawn_pending) {
        item_spawn_pending = 1; // Set flag to indicate pending spawn
        item_timer = 0; // Reset the timer
      }

      // Handle enemy spawning delay
      if (item_spawn_pending) {
        item_timer++;
        if (item_timer >= 600) {
          rand_item(); // Spawn new enemy
        }
      }
    }
    update_bullets();
  }
}

const byte music1[] = {
0x2a,0x1e,0x95,0x33,0x27,0x9f,0x31,0x25,0x8a,0x2f,0x23,0x8b,0x2e,0x22,0x89,0x2c,0x20,0x8b,0x2a,0x1e,0x95,0x28,0x1c,0x8a,0x27,0x1b,0x95,0x25,0x19,0x8b,0x23,0x17,0x89,0x22,0x12,0x8b,0x23,0x8a,0x25,0x8b,0x27,0x89,0x28,0x1e,0x8c,0x2a,0x1c,0x89,0x2c,0x1b,0x8c,0x2e,0x19,0x89,0x2f,0x17,0x95,0x27,0x23,0x1e,0x95,0x12,0x95,0x23,0x1e,0x27,0x95,0x2f,0x33,0x17,0x95,0x2f,0x33,0x27,0x94,0x12,0x95,0x33,0x36,0x23,0x95,0x38,0x32,0x17,0x8b,0x33,0x36,0x89,0x1e,0x27,0x23,0x8b,0x38,0x32,0x8a,0x33,0x36,0x12,0x8b,0x32,0x38,0x89,0x36,0x33,0x27,0x95,0x3d,0x37,0x10,0x8b,0x3b,0x38,0x8a,0x20,0x23,0x28,0x8b,0x37,0x3d,0x89,0x38,0x3b,0x10,0x8b,0x37,0x3d,0x8a,0x3b,0x38,0x28,0x8b,0x36,0x33,0x8a,0x17,0x95,0x2a,0x1e,0x23,0x8b,0x2c,0x89,0x2e,0x12,0x8b,0x2f,0x8a,0x31,0x27,0x23,0x8b,0x32,0x89,0x2f,0x33,0x17,0x95,0x33,0x2f,0x1e,0x95,0x12,0x94,0x36,0x33,0x1e,0x95,0x32,0x38,0x17,0x8b,0x36,0x33,0x89,0x23,0x1e,0x27,0x8b,0x38,0x32,0x8a,0x33,0x36,0x18,0x8b,0x33,0x36,0x8a,0x33,0x36,0x2a,0x95,0x36,0x19,0x8b,0x31,0x2e,0x89,0x25,0x22,0x2a,0x8b,0x36,0x8a,0x2f,0x35,0x19,0x8b,0x36,0x89,0x38,0x2f,0x29,0x95,0x2e,0x36,0x1e,0x95,0x1c,0x28,0x95,0x27,0x1b,0x8b,0x2a,0x8a,0x2e,0x25,0x19,0x8b,0x34,0x89,0x33,0x2f,0x23,0x95,0x33,0x2f,0x1e,0x95,0x12,0x94,0x33,0x36,0x23,0x95,0x32,0x38,0x17,0x8b,0x33,0x36,0x8a,0x23,0x27,0x1e,0x8b,0x38,0x32,0x89,0x36,0x33,0x12,0x8b,0x32,0x38,0x8a,0x33,0x36,0x1e,0x94,0x37,0x3d,0x10,0x8b,0x3b,0x38,0x8a,0x23,0x28,0x20,0x8b,0x3d,0x37,0x89,0x3b,0x38,0x1c,0x8b,0x38,0x3b,0x8a,0x3a,0x3d,0x38,0x8b,0x3f,0x37,0x3a,0x89,0x27,0x1b,0x8b,0x33,0x8a,0x37,0x22,0x16,0x8b,0x3a,0x8a,0x3f,0x0f,0x1b,0x95,0x3a,0x8b,0x3b,0x8a,0x3d,0x37,0x1c,0x8b,0x3b,0x38,0x89,0x23,0x28,0x20,0x8b,0x3d,0x37,0x8a,0x38,0x3b,0x10,0x8b,0x3d,0x37,0x89,0x38,0x3b,0x20,0x8b,0x36,0x33,0x8a,0x17,0x23,0x8b,0x38,0x34,0x8a,0x33,0x36,0x1e,0x8b,0x31,0x34,0x8a,0x2f,0x33,0x17,0x8b,0x36,0x33,0x8a,0x23,0x27,0x1e,0x8b,0x33,0x36,0x8a,0x36,0x30,0x12,0x8a,0x34,0x31,0x8a,0x1e,0x28,0x22,0x8b,0x30,0x36,0x89,0x34,0x31,0x12,0x8b,0x2e,0x28,0x33,0x8a,0x31,0x28,0x2e,0x95,0x27,0x2f,0x1e,0x95,0x12,0x95,0x14,0x94,0x16,0x95,0x2f,0x33,0x17,0x95,0x2f,0x33,0x27,0x94,0x12,0x95,0x36,0x33,0x23,0x95,0x32,0x38,0x17,0x8a,0x33,0x36,0x8a,0x23,0x27,0x1e,0x8b,0x32,0x38,0x8a,0x33,0x36,0x12,0x8b,0x38,0x32,0x8a,0x36,0x33,0x27,0x95,0x3d,0x37,0x10,0x8a,0x3b,0x38,0x8a,0x23,0x28,0x20,0x8b,0x37,0x3d,0x89,0x3b,0x38,0x10,0x8b,0x37,0x3d,0x8a,0x38,0x3b,0x23,0x8b,0x36,0x33,0x8a,0x17,0x95,0x2a,0x1e,0x23,0x8b,0x2c,0x8a,0x2e,0x12,0x8b,0x2f,0x89,0x31,0x23,0x1e,0x8b,0x32,0x8a,0x33,0x2f,0x17,0x95,0x2f,0x33,0x1e,0x94,0x12,0x95,0x36,0x33,0x27,0x95,0x38,0x32,0x17,0x8a,0x36,0x33,0x8a,0x23,0x1e,0x27,0x8b,0x38,0x32,0x89,0x33,0x36,0x18,0x8c,0x36,0x33,0x89,0x33,0x36,0x21,0x95,0x36,0x19,0x8b,0x2e,0x31,0x8a,0x25,0x2a,0x22,0x8b,0x36,0x89,0x35,0x2f,0x19,0x8b,0x36,0x8a,0x2f,0x38,0x25,0x94,0x36,0x2e,0x2a,0x95,0x1c,0x28,0x95,0x27,0x1b,0x8c,0x2a,0x89,0x2e,0x25,0x19,0x8b,0x34,0x8a,0x33,0x2f,0x23,0x95,0x2f,0x33,0x1e,0x94,0x12,0x95,0x33,0x36,0x23,0x95,0x32,0x38,0x17,0x8a,0x33,0x36,0x8a,0x23,0x1e,0x27,0x8b,0x32,0x38,0x89,0x36,0x33,0x12,0x8c,0x32,0x38,0x89,0x33,0x36,0x1e,0x95,0x37,0x3d,0x10,0x8b,0x3b,0x38,0x8a,0x23,0x20,0x28,0x8a,0x3d,0x37,0x8a,0x38,0x3b,0x1c,0x8b,0x3b,0x38,0x8a,0x3d,0x38,0x3a,0x8b,0x3a,0x3f,0x37,0x89,0x27,0x1b,0x8b,0x33,0x8a,0x37,0x22,0x16,0x8b,0x3a,0x8a,0x3f,0x0f,0x1b,0x95,0x3a,0x8b,0x3b,0x8a,0x3d,0x37,0x1c,0x8b,0x3b,0x38,0x8a,0x20,0x28,0x23,0x8b,0x3d,0x37,0x89,0x38,0x3b,0x10,0x8b,0x3d,0x37,0x8a,0x38,0x3b,0x20,0x8b,0x33,0x36,0x89,0x17,0x23,0x8b,0x38,0x34,0x8a,0x36,0x33,0x23,0x8b,0x34,0x31,0x89,0x2f,0x33,0x17,0x8c,0x36,0x33,0x89,0x1e,0x27,0x23,0x8c,0x33,0x36,0x89,0x36,0x30,0x12,0x8b,0x34,0x31,0x8a,0x1e,0x28,0x22,0x8a,0x30,0x36,0x8a,0x34,0x31,0x1e,0x8b,0x28,0x2e,0x33,0x89,0x28,0x31,0x2e,0x95,0x27,0x2f,0x17,0x95,0x12,0x1e,0x95,0x3b,0x36,0x33,0x96,0x2a,0x8b,0x2b,0x8a,0x2c,0x1e,0x12,0x8b,0x2d,0x89,0x2e,0x28,0x1e,0x8b,0x2e,0x31,0x8a,0x16,0x22,0x8b,0x36,0x89,0x34,0x1e,0x22,0x8b,0x31,0x8a,0x2c,0x25,0x19,0x8b,0x2d,0x89,0x2e,0x28,0x25,0x8b,0x33,0x2e,0x8a,0x25,0x19,0x8c,0x31,0x89,0x2c,0x1a,0x26,0x8b,0x2e,0x8a,0x2f,0x1b,0x27,0x8b,0x2a,0x89,0x2c,0x23,0x1e,0x8b,0x2e,0x8a,0x2f,0x12,0x1e,0x8b,0x30,0x89,0x31,0x27,0x23,0x8b,0x32,0x8a,0x33,0x17,0x23,0x8b,0x32,0x8a,0x33,0x1e,0x23,0x8b,0x38,0x33,0x8a,0x1e,0x12,0x8b,0x36,0x8a,0x31,0x1e,0x23,0x8b,0x33,0x89,0x34,0x25,0x19,0x8b,0x3d,0x89,0x1e,0x28,0x22,0x8b,0x33,0x89,0x34,0x12,0x1e,0x8b,0x3d,0x8a,0x1e,0x22,0x28,0x8a,0x33,0x8a,0x34,0x19,0x25,0x8b,0x3d,0x89,0x28,0x1e,0x22,0x8b,0x3b,0x89,0x3a,0x1e,0x12,0x8b,0x38,0x8a,0x36,0x22,0x16,0x8b,0x34,0x8a,0x33,0x23,0x17,0x8b,0x3b,0x8a,0x1e,0x23,0x27,0x8a,0x32,0x8a,0x33,0x12,0x1e,0x8b,0x3b,0x89,0x27,0x23,0x1e,0x8b,0x32,0x89,0x33,0x23,0x17,0x8b,0x3b,0x89,0x1e,0x23,0x27,0x8b,0x38,0x8a,0x36,0x12,0x1e,0x8b,0x33,0x89,0x31,0x1e,0x27,0x8b,0x2f,0x8a,0x2e,0x20,0x14,0x8b,0x2f,0x8a,0x30,0x2a,0x24,0x8b,0x30,0x38,0x8a,0x24,0x18,0x8b,0x36,0x89,0x33,0x24,0x20,0x8b,0x30,0x8a,0x2e,0x27,0x1b,0x8b,0x2f,0x89,0x30,0x24,0x2a,0x8b,0x30,0x38,0x8a,0x20,0x14,0x8b,0x33,0x36,0x8a,0x38,0x30,0x24,0x8b,0x33,0x36,0x89,0x34,0x31,0x19,0x8b,0x30,0x33,0x89,0x31,0x34,0x25,0x8b,0x2c,0x89,0x1c,0x8b,0x30,0x8a,0x31,0x20,0x28,0x8a,0x34,0x8a,0x38,0x19,0x8b,0x33,0x89,0x34,0x25,0x20,0x8b,0x31,0x8a,0x25,0x20,0x1c,0x8b,0x2c,0x8a,0x28,0x8b,0x25,0x8a,0x26,0x1d,0x8a,0x29,0x20,0x8a,0x2c,0x23,0x8b,0x2f,0x26,0x89,0x32,0x29,0x8b,0x32,0x29,0x94,0x32,0x29,0x94,0x35,0x2c,0x8a,0x38,0x2f,0x8a,0x3b,0x32,0x8a,0x35,0x3e,0xa8,0x2f,0x3f,0x36,0x8b,0x3b,0x8a,0x36,0x8b,0x33,0x94,0x2f,0x8a,0x27,0x8b,0x2a,0x89,0x2a,0x2e,0x28,0x8b,0x31,0x2a,0x2e,0x8a,0x1e,0x12,0x8b,0x2f,0x2a,0x27,0x8a,0x17,0x23,0x95,0x2a,0x8b,0x2b,0x8a,0x2c,0x1e,0x12,0x8a,0x2d,0x8a,0x2e,0x25,0x1e,0x8b,0x31,0x2e,0x89,0x22,0x16,0x8b,0x36,0x8a,0x34,0x1e,0x22,0x8a,0x31,0x8a,0x2c,0x25,0x19,0x8b,0x2d,0x89,0x2e,0x28,0x25,0x8b,0x33,0x2e,0x8a,0x19,0x25,0x8a,0x31,0x8a,0x2c,0x1a,0x26,0x8b,0x2e,0x89,0x2f,0x27,0x1b,0x8b,0x2a,0x89,0x2c,0x23,0x1e,0x8b,0x2e,0x8a,0x2f,0x1e,0x12,0x8b,0x30,0x89,0x31,0x27,0x23,0x8b,0x32,0x8a,0x33,0x17,0x23,0x8b,0x32,0x8a,0x33,0x1e,0x23,0x8b,0x33,0x38,0x8a,0x12,0x1e,0x8b,0x36,0x8a,0x31,0x27,0x1e,0x8b,0x33,0x89,0x34,0x25,0x19,0x8b,0x3d,0x89,0x28,0x1e,0x22,0x8b,0x33,0x8a,0x34,0x1e,0x12,0x8b,0x3d,0x89,0x1e,0x22,0x28,0x8b,0x33,0x89,0x34,0x19,0x25,0x8b,0x3d,0x8a,0x22,0x1e,0x28,0x8b,0x3b,0x89,0x3a,0x12,0x1e,0x8b,0x38,0x89,0x36,0x22,0x16,0x8b,0x34,0x8a,0x33,0x23,0x17,0x8b,0x3b,0x8a,0x1e,0x27,0x23,0x8b,0x32,0x8a,0x33,0x1e,0x12,0x8a,0x3b,0x8a,0x23,0x1e,0x27,0x8b,0x32,0x89,0x33,0x23,0x17,0x8b,0x3b,0x8a,0x27,0x1e,0x23,0x8a,0x38,0x8a,0x36,0x12,0x1e,0x8b,0x33,0x8a,0x31,0x1e,0x23,0x8b,0x2f,0x89,0x2e,0x14,0x20,0x8b,0x2f,0x8a,0x30,0x24,0x20,0x8b,0x38,0x30,0x89,0x18,0x24,0x8c,0x36,0x8a,0x33,0x24,0x20,0x8a,0x30,0x8a,0x2e,0x27,0x1b,0x8b,0x2f,0x89,0x30,0x24,0x20,0x8b,0x38,0x30,0x8a,0x14,0x20,0x8b,0x36,0x33,0x8a,0x30,0x38,0x20,0x8b,0x36,0x33,0x8a,0x31,0x34,0x19,0x8b,0x33,0x30,0x89,0x34,0x31,0x25,0x8b,0x2c,0x8a,0x1c,0x8a,0x30,0x8a,0x31,0x28,0x20,0x8b,0x34,0x89,0x38,0x19,0x8b,0x33,0x8a,0x34,0x25,0x20,0x8b,0x31,0x8a,0x25,0x20,0x1c,0x8b,0x2c,0x89,0x28,0x8b,0x25,0x8a,0x26,0x1d,0x8b,0x29,0x20,0x89,0x2c,0x23,0x8b,0x2f,0x26,0x8a,0x32,0x29,0x8a,0x32,0x29,0x95,0x32,0x29,0x94,0x35,0x2c,0x8a,0x38,0x2f,0x8b,0x3b,0x32,0x89,0x3e,0x35,0xa9,0x36,0x3f,0x2f,0x8b,0x3b,0x8a,0x36,0x8b,0x33,0x95,0x2f,0x89,0x27,0x8b,0x2a,0x89,0x28,0x31,0x2a,0x8b,0x2e,0x31,0x28,0x8a,0x12,0x1e,0x8b,0x27,0x2a,0x2f,0x8a,0x17,0x23,0x95,0x2a,0x94,0x33,0x2f,0x17,0x94,0x2f,0x33,0x1e,0x95,0x12,0x94,0x33,0x36,0x23,0x95,0x38,0x32,0x17,0x8a,0x33,0x36,0x8a,0x27,0x1e,0x23,0x8b,0x32,0x38,0x8a,0x33,0x36,0x12,0x8b,0x32,0x38,0x89,0x33,0x36,0x27,0x95,0x3d,0x37,0x10,0x8b,0x38,0x3b,0x8a,0x28,0x20,0x23,0x8b,0x3d,0x37,0x89,0x3b,0x38,0x10,0x8b,0x3d,0x37,0x8a,0x38,0x3b,0x23,0x8b,0x36,0x33,0x8a,0x17,0x95,0x2a,0x1e,0x23,0x8b,0x2c,0x89,0x2e,0x12,0x8b,0x2f,0x8a,0x31,0x23,0x1e,0x8b,0x32,0x89,0x33,0x2f,0x17,0x94,0x2f,0x33,0x1e,0x95,0x12,0x94,0x33,0x36,0x27,0x95,0x38,0x32,0x17,0x8b,0x36,0x33,0x8a,0x23,0x27,0x1e,0x8b,0x32,0x38,0x8a,0x33,0x36,0x18,0x8b,0x36,0x33,0x8a,0x33,0x36,0x2a,0x95,0x36,0x19,0x8b,0x31,0x2e,0x89,0x22,0x2a,0x25,0x8b,0x36,0x89,0x35,0x2f,0x19,0x8b,0x36,0x8a,0x2f,0x38,0x25,0x95,0x2e,0x36,0x2a,0x95,0x28,0x1c,0x95,0x27,0x1b,0x8b,0x2a,0x8a,0x2e,0x25,0x19,0x8b,0x34,0x89,0x33,0x2f,0x17,0x95,0x2f,0x33,0x23,0x94,0x12,0x95,0x33,0x36,0x1e,0x95,0x38,0x32,0x17,0x8b,0x36,0x33,0x8a,0x23,0x27,0x1e,0x8b,0x32,0x38,0x8a,0x33,0x36,0x12,0x8b,0x38,0x32,0x8a,0x33,0x36,0x1e,0x95,0x3d,0x37,0x10,0x8b,0x3b,0x38,0x89,0x23,0x28,0x20,0x8b,0x37,0x3d,0x8a,0x38,0x3b,0x1c,0x8b,0x38,0x3b,0x89,0x38,0x3d,0x3a,0x8b,0x3f,0x3a,0x37,0x8a,0x1b,0x27,0x8b,0x33,0x8a,0x37,0x22,0x16,0x8b,0x3a,0x8a,0x3f,0x0f,0x1b,0x95,0x3a,0x8b,0x3b,0x89,0x3d,0x37,0x1c,0x8b,0x38,0x3b,0x8a,0x28,0x20,0x23,0x8a,0x3d,0x37,0x8a,0x38,0x3b,0x10,0x8b,0x3d,0x37,0x89,0x3b,0x38,0x20,0x8b,0x33,0x36,0x8a,0x23,0x17,0x8b,0x38,0x34,0x8a,0x33,0x36,0x27,0x8b,0x34,0x31,0x8a,0x33,0x2f,0x23,0x8b,0x36,0x33,0x8a,0x1e,0x23,0x27,0x8b,0x33,0x36,0x8a,0x36,0x30,0x12,0x8b,0x34,0x31,0x89,0x1e,0x28,0x22,0x8b,0x30,0x36,0x89,0x31,0x34,0x1e,0x8b,0x2e,0x28,0x33,0x8a,0x28,0x31,0x2e,0x95,0x2f,0x27,0x17,0x95,0x12,0x1e,0x95,0x36,0x33,0x3b,0xa9,0x34,0x28,0x95,0x34,0x28,0xa0,0x2f,0x23,0x8a,0x34,0x28,0x8b,0x36,0x2a,0x8a,0x38,0x2c,0x94,0x2c,0x38,0xa1,0x2f,0x23,0x89,0x28,0x34,0x8b,0x2c,0x38,0x8a,0x39,0x3b,0x2f,0x8b,0x3f,0x39,0x36,0x94,0x33,0x39,0x3d,0x8a,0x17,0x23,0x8b,0x3b,0x33,0x39,0x89,0x33,0x2f,0x2d,0x95,0x34,0x2c,0x2f,0x95,0x1c,0x10,0x95,0x12,0x1e,0x95,0x2f,0x14,0x20,0x8b,0x30,0x89,0x31,0x21,0x15,0x8b,0x39,0x8a,0x28,0x2d,0x25,0x8b,0x36,0x8a,0x30,0x1e,0x2a,0x8b,0x39,0x89,0x1f,0x2b,0x8b,0x36,0x8a,0x2f,0x2c,0x20,0x8b,0x34,0x8a,0x38,0x23,0x28,0x8b,0x3d,0x89,0x1c,0x28,0x8b,0x3b,0x8a,0x38,0x23,0x28,0x8b,0x34,0x8a,0x33,0x17,0x23,0x8b,0x3b,0x89,0x36,0x2d,0x23,0x8b,0x33,0x8a,0x31,0x1e,0x2a,0x8b,0x33,0x89,0x1f,0x2b,0x8c,0x2f,0x89,0x34,0x20,0x2c,0x8b,0x34,0x8a,0x38,0x28,0x1c,0x8b,0x3b,0x89,0x40,0x23,0x17,0x8b,0x3d,0x8a,0x3b,0x14,0x20,0x8b,0x38,0x8a,0x31,0x15,0x21,0x8b,0x39,0x89,0x28,0x25,0x2d,0x8b,0x36,0x8a,0x30,0x1e,0x2a,0x8b,0x39,0x8a,0x2b,0x1f,0x8b,0x36,0x89,0x2f,0x2c,0x20,0x8b,0x34,0x8a,0x38,0x28,0x2c,0x8b,0x3d,0x8a,0x28,0x1c,0x8b,0x3b,0x89,0x38,0x28,0x2c,0x8b,0x34,0x8a,0x36,0x19,0x25,0x8b,0x38,0x8a,0x36,0x28,0x2e,0x8b,0x34,0x89,0x33,0x12,0x1e,0x8b,0x34,0x8a,0x2a,0x2e,0x28,0x8b,0x31,0x89,0x2f,0x27,0x2a,0x81,0x23,0x94,0x32,0x38,0x1d,0x8b,0x33,0x36,0x1e,0x8a,0x33,0x3b,0x17,0x95,0x2f,0x32,0x20,0x94,0x31,0x15,0x21,0x8b,0x39,0x8a,0x2d,0x28,0x25,0x8b,0x36,0x8a,0x30,0x1e,0x2a,0x8b,0x39,0x8a,0x1f,0x2b,0x8b,0x36,0x89,0x20,0x2c,0x8b,0x2f,0x8a,0x34,0x23,0x2c,0x8b,0x38,0x8a,0x3d,0x28,0x1c,0x8b,0x3b,0x8a,0x38,0x2c,0x28,0x8b,0x34,0x89,0x33,0x17,0x23,0x8b,0x3b,0x8a,0x36,0x23,0x2d,0x8b,0x33,0x8a,0x31,0x1e,0x2a,0x8b,0x33,0x8a,0x2b,0x1f,0x8b,0x2f,0x89,0x2c,0x20,0x8b,0x34,0x8a,0x38,0x28,0x1c,0x8b,0x3b,0x8a,0x38,0x40,0x17,0x8b,0x3d,0x8a,0x32,0x3b,0x20,0x8b,0x38,0x89,0x31,0x15,0x21,0x8b,0x39,0x8a,0x25,0x19,0x8b,0x36,0x8a,0x30,0x2a,0x1e,0x8b,0x39,0x8a,0x21,0x2d,0x8b,0x36,0x89,0x2c,0x20,0x8b,0x2f,0x8a,0x34,0x28,0x1c,0x8b,0x38,0x89,0x3d,0x23,0x17,0x8b,0x3b,0x8a,0x38,0x28,0x1c,0x8b,0x2f,0x89,0x2e,0x19,0x25,0x8b,0x31,0x34,0x8a,0x12,0x1e,0x95,0x2d,0x0b,0x17,0x8b,0x33,0x36,0x95,0x34,0x2c,0x8a,0x1c,0x10,0x96,0x38,0x1c,0x28,0x8b,0x34,0x89,0x36,0x23,0x17,0x8b,0x38,0x8b,0x2f,0x14,0x20,0x8b,0x30,0x89,0x31,0x21,0x15,0x8b,0x39,0x8a,0x28,0x25,0x2d,0x8b,0x36,0x89,0x30,0x2a,0x1e,0x8b,0x39,0x8a,0x1f,0x2b,0x8b,0x36,0x8a,0x2f,0x2c,0x20,0x8b,0x34,0x89,0x38,0x23,0x28,0x8b,0x3d,0x8a,0x1c,0x28,0x8b,0x3b,0x8a,0x38,0x23,0x28,0x8b,0x34,0x89,0x33,0x17,0x23,0x8b,0x3b,0x8a,0x36,0x2d,0x27,0x8b,0x33,0x8a,0x31,0x2a,0x1e,0x8b,0x33,0x89,0x2b,0x1f,0x8b,0x2f,0x8a,0x34,0x20,0x2c,0x8b,0x34,0x8a,0x38,0x28,0x1c,0x8b,0x3b,0x8a,0x40,0x17,0x23,0x8b,0x3d,0x89,0x3b,0x20,0x14,0x8c,0x38,0x89,0x31,0x15,0x21,0x8b,0x39,0x8a,0x2d,0x25,0x28,0x8b,0x36,0x8a,0x30,0x1e,0x2a,0x8b,0x39,0x8a,0x2b,0x1f,0x8b,0x36,0x8a,0x2f,0x2c,0x20,0x8b,0x34,0x8a,0x38,0x23,0x28,0x8b,0x3d,0x8a,0x28,0x1c,0x8b,0x3b,0x89,0x38,0x23,0x28,0x8c,0x34,0x89,0x36,0x19,0x25,0x8b,0x38,0x8a,0x36,0x28,0x2e,0x8b,0x34,0x89,0x33,0x12,0x1e,0x8c,0x34,0x89,0x2e,0x28,0x2a,0x8c,0x31,0x89,0x2f,0x2a,0x27,0x81,0x23,0x94,0x32,0x38,0x1d,0x8c,0x33,0x36,0x1e,0x89,0x3b,0x33,0x17,0x95,0x2f,0x32,0x20,0x95,0x31,0x21,0x15,0x8b,0x39,0x8a,0x2d,0x28,0x25,0x8b,0x36,0x8a,0x30,0x2a,0x1e,0x8b,0x39,0x8a,0x1f,0x2b,0x8b,0x36,0x8a,0x2c,0x20,0x8b,0x2f,0x89,0x34,0x23,0x28,0x8c,0x38,0x89,0x3d,0x28,0x1c,0x8b,0x3b,0x8a,0x38,0x28,0x23,0x8b,0x34,0x8a,0x33,0x17,0x23,0x8b,0x3b,0x8a,0x36,0x2d,0x27,0x8b,0x33,0x8a,0x31,0x2a,0x1e,0x8b,0x33,0x8a,0x2b,0x1f,0x8b,0x2f,0x89,0x2c,0x20,0x8c,0x34,0x89,0x38,0x1c,0x28,0x8b,0x3b,0x8a,0x40,0x38,0x23,0x8b,0x3d,0x8a,0x32,0x3b,0x14,0x8b,0x38,0x8a,0x31,0x15,0x21,0x8b,0x39,0x8a,0x19,0x25,0x8b,0x36,0x8a,0x30,0x2a,0x1e,0x8b,0x39,0x8a,0x2d,0x21,0x8b,0x36,0x8a,0x2c,0x20,0x8b,0x2f,0x89,0x34,0x1c,0x28,0x8b,0x38,0x8a,0x3d,0x23,0x17,0x8b,0x3b,0x89,0x38,0x1c,0x28,0x8c,0x2f,0x89,0x2e,0x19,0x25,0x8b,0x31,0x34,0x8a,0x12,0x1e,0x95,0x2d,0x0b,0x17,0x8c,0x33,0x36,0x95,0x2c,0x34,0x8a,0x1c,0x10,0x95,0x17,0x96,0x10,0x8b,0x3b,0x2f,0x8a,0x31,0x3d,0x8b,0x3e,0x32,0x8a,0x3f,0x33,0x39,0x94,0x3b,0x2f,0x23,0x8b,0x39,0x3d,0x31,0x8a,0x23,0x17,0x95,0x2d,0x23,0x27,0x95,0x3f,0x39,0x33,0x94,0x3b,0x2f,0x2d,0x8b,0x3d,0x39,0x31,0x8a,0x12,0x1e,0x8b,0x3b,0x2f,0x17,0x8a,0x31,0x3d,0x22,0x8b,0x3f,0x33,0x15,0x8a,0x38,0x40,0x34,0x95,0x38,0x3b,0x2f,0x8b,0x31,0x38,0x3d,0x89,0x1c,0x28,0x95,0x28,0x2c,0x23,0x95,0x34,0x40,0x38,0x95,0x38,0x2f,0x3b,0x8b,0x31,0x3d,0x38,0x89,0x28,0x1c,0x95,0x29,0x1d,0x95,0x1e,0x2a,0x95,0x3d,0x27,0x23,0x8b,0x39,0x3b,0x33,0x89,0x23,0x17,0x95,0x2d,0x27,0x23,0x95,0x2a,0x1e,0x95,0x3d,0x23,0x27,0x8b,0x33,0x3b,0x39,0x89,0x23,0x17,0x95,0x27,0x23,0x2d,0x95,0x1c,0x28,0x95,0x3d,0x28,0x2c,0x8b,0x34,0x3b,0x38,0x89,0x23,0x17,0x95,0x28,0x23,0x2c,0x95,0x1c,0x28,0x95,0x3d,0x28,0x23,0x8b,0x38,0x34,0x3b,0x8a,0x20,0x2c,0x8b,0x2f,0x3b,0x89,0x3d,0x31,0x1f,0x8b,0x32,0x3e,0x8a,0x3f,0x39,0x33,0x95,0x3b,0x2f,0x27,0x8b,0x39,0x31,0x3d,0x8a,0x23,0x17,0x94,0x27,0x2d,0x23,0x95,0x33,0x39,0x3f,0x95,0x3b,0x2f,0x27,0x8b,0x3d,0x31,0x39,0x8a,0x12,0x1e,0x8b,0x3b,0x2f,0x23,0x89,0x3d,0x31,0x22,0x8b,0x33,0x3f,0x15,0x8a,0x34,0x40,0x38,0x95,0x3b,0x38,0x2f,0x8b,0x3d,0x31,0x38,0x8a,0x1c,0x28,0x95,0x28,0x23,0x2c,0x95,0x20,0x2c,0x8c,0x34,0x8a,0x38,0x28,0x1c,0x8b,0x3b,0x8a,0x40,0x38,0x23,0x8b,0x3d,0x8a,0x32,0x3b,0x20,0x8b,0x38,0x8a,0x31,0x15,0x21,0x8b,0x39,0x8a,0x19,0x25,0x8b,0x36,0x8a,0x30,0x1e,0x2a,0x8b,0x39,0x8a,0x2d,0x21,0x8b,0x36,0x8a,0x2c,0x20,0x8b,0x2f,0x8a,0x34,0x28,0x1c,0x8b,0x38,0x89,0x3d,0x17,0x23,0x8b,0x3b,0x8a,0x38,0x1c,0x28,0x8b,0x2f,0x8a,0x2e,0x19,0x25,0x8b,0x34,0x31,0x89,0x1e,0x12,0x95,0x2d,0x17,0x0b,0x8b,0x36,0x33,0x95,0x2c,0x34,0x8a,0x10,0x1c,0x95,0x17,0x23,0x96,0x20,0x2c,0x8b,0x2f,0x3b,0x89,0x31,0x3d,0x2b,0x8b,0x32,0x3e,0x89,0x39,0x3f,0x33,0x95,0x2f,0x3b,0x23,0x8b,0x39,0x31,0x3d,0x8a,0x23,0x17,0x95,0x2d,0x23,0x27,0x95,0x3f,0x39,0x33,0x95,0x2f,0x3b,0x23,0x8b,0x31,0x3d,0x39,0x89,0x1e,0x12,0x8c,0x2f,0x3b,0x23,0x89,0x31,0x3d,0x22,0x8b,0x3f,0x33,0x15,0x89,0x40,0x38,0x34,0x95,0x3b,0x2f,0x38,0x8b,0x3d,0x31,0x38,0x8a,0x1c,0x28,0x95,0x23,0x28,0x2c,0x95,0x38,0x34,0x40,0x95,0x3b,0x38,0x2f,0x8b,0x31,0x3d,0x38,0x89,0x1c,0x28,0x95,0x29,0x1d,0x95,0x2a,0x1e,0x95,0x3d,0x27,0x23,0x8b,0x39,0x3b,0x33,0x89,0x17,0x23,0x95,0x2d,0x27,0x23,0x95,0x1e,0x2a,0x95,0x3d,0x27,0x23,0x8b,0x33,0x39,0x3b,0x8a,0x23,0x17,0x94,0x2d,0x23,0x27,0x95,0x1c,0x28,0x95,0x3d,0x2c,0x28,0x8b,0x34,0x3b,0x38,0x89,0x17,0x23,0x95,0x2c,0x28,0x23,0x95,0x1c,0x28,0x94,0x3d,0x23,0x2c,0x8b,0x38,0x34,0x3b,0x8a,0x2c,0x20,0x8b,0x2f,0x3b,0x89,0x31,0x3d,0x1f,0x8b,0x32,0x3e,0x8a,0x3f,0x39,0x33,0x94,0x3b,0x2f,0x2d,0x8b,0x31,0x39,0x3d,0x8a,0x23,0x17,0x95,0x2d,0x27,0x23,0x95,0x39,0x3f,0x33,0x95,0x3b,0x2f,0x27,0x8b,0x31,0x39,0x3d,0x89,0x12,0x1e,0x8b,0x3b,0x2f,0x17,0x8a,0x31,0x3d,0x22,0x8b,0x33,0x3f,0x15,0x89,0x34,0x40,0x38,0x95,0x3b,0x38,0x2f,0x8b,0x3d,0x31,0x38,0x8a,0x28,0x1c,0x95,0x28,0x23,0x2c,0x95,0x20,0x2c,0x8b,0x34,0x89,0x38,0x1c,0x28,0x8b,0x3b,0x8a,0x38,0x40,0x23,0x8b,0x3d,0x8a,0x3b,0x32,0x20,0x8b,0x38,0x89,0x31,0x15,0x21,0x8b,0x39,0x8a,0x19,0x25,0x8b,0x36,0x8a,0x30,0x1e,0x2a,0x8b,0x39,0x89,0x2d,0x21,0x8b,0x36,0x8a,0x20,0x2c,0x8b,0x2f,0x8a,0x34,0x28,0x1c,0x8a,0x38,0x8a,0x3d,0x23,0x17,0x8b,0x3b,0x8a,0x38,0x28,0x1c,0x8b,0x2f,0x89,0x2e,0x25,0x19,0x8c,0x31,0x34,0x89,0x1e,0x12,0x95,0x2d,0x17,0x0b,0x8c,0x33,0x36,0x95,0x34,0x2c,0x8a,0x1c,0x10,0x95,0x17,0x96,0x40,0x34,0x3b,0xff
};
