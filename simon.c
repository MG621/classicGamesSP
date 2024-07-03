
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
  0x03,			// screen color

  0x11,0x30,0x27,0x00,	// background palette 0
  0x1C,0x20,0x2C,0x00,	// background palette 1
  0x00,0x10,0x20,0x00,	// background palette 2
  0x06,0x16,0x26,0x00,   // background palette 3

  0x30,0x16,0x07,0x00,	// sprite palette 0
  0x30,0x2A,0x0A,0x00,	// sprite palette 1
  0x30,0x21,0x01,0x00,	// sprite palette 2
  0x30,0x28,0x18	// sprite palette 3
};

#define MAX_SCORE 32

int pattern[MAX_SCORE];
int score;
int rng_seed;
int i;
int x;
int y;
int z;
int frame_counter;
bool game_over = false;

void play_sound_pulse1(word period, byte duty, byte decay, byte length) {
  APU_ENABLE(0x01);
  APU_PULSE_DECAY(0, period, duty, decay, length);
}

void play_sound_pulse2(word period, byte duty, byte decay, byte length) {
  APU_ENABLE(0x02);
  APU_PULSE_DECAY(1, period, duty, decay, length);
}

void play_sound_effect(int sound_type) {
  switch(sound_type) {
    case 1:
      play_sound_pulse1(1000, 0x80, 0x0F, 0x0A);
      break;
    case 2:
      play_sound_pulse1(750, 0x80, 0x0F, 0x0A);
      break;
    case 3:
      play_sound_pulse2(500, 0x80, 0x0F, 0x0A);
      break;
    case 4:
      play_sound_pulse2(250, 0x80, 0x0F, 0x0A);
      break;
    case 5:
      play_sound_pulse1(1500, 0x80, 0x0F, 0x0A);
      break;
    default:
      break;
  }
}


void display_instructions(){
  vram_adr(NTADR_A(2,4));
  vram_write("UP GREEN", 8);
  vram_adr(NTADR_A(2,5));
  vram_write("DOWN BLUE", 9);
  vram_adr(NTADR_A(2,6));
  vram_write("LEFT YELLOW", 11);
  vram_adr(NTADR_A(2,7));
  vram_write("RIGHT RED", 9);  
}

#define TILE_SIZE 16

typedef struct {
  byte x;
  byte y;
  byte tile;
  byte attr;
} Sprite;

// Define the sprite data for the Simon device
Sprite simon_sprites[4] = {
  {112, 104, 0x03, 0x01},  // Top-left (green)
  {128, 104, 0x03, 0x00},  // Top-right (red)
  {112, 120, 0x03, 0x03},  // Bottom-left (yellow)
  {128, 120, 0x03, 0x02}   // Bottom-right (blue)
};

// Function to draw the sprites
void draw_simon_sprites() {
  for (i = 0; i < 4; i++) {
    oam_spr(simon_sprites[i].x, simon_sprites[i].y, simon_sprites[i].tile, simon_sprites[i].attr, i * 4);
  }
}

void flash_color(int color) {
  oam_spr(simon_sprites[color].x, simon_sprites[color].y, simon_sprites[color].tile - 2, simon_sprites[color].attr, color * 4);
}

void display_color(int input){
  ppu_off();
  play_sound_effect(input);
  switch(input){
    case 1:
      //vram_adr(NTADR_A(13,14));
      //vram_write("GREEN", 5);
      flash_color(input - 1);
      break;
    case 2:
      //vram_adr(NTADR_A(13,14));
      //vram_write("RED", 3);
      flash_color(input - 1);
      break;
    case 3:
      //vram_adr(NTADR_A(13,14));
      //vram_write("YELLOW", 6);
      flash_color(input - 1);
      break;
    case 4:
      //vram_adr(NTADR_A(13,14));
      //vram_write("BLUE", 4);
      flash_color(input - 1);
      break;
    default:
      vram_adr(NTADR_A(13,14));
      vram_write("ERROR!", 6);
      break;
  }
  ppu_on_all();
  delay(60);
  vram_adr(NTADR_A(13, 14)); // Clear the color
  vram_fill(0x20, 6);
  draw_simon_sprites();
}


void display_sequence(){
  for(x = 0; x <= score; x++){
    display_color(pattern[x]);
  }
}


void add_color(){
  pattern[score] = rand() % 4 + 1;
}

void display_score() {
  char score_str[3];
  itoa(score, score_str, 10);
  ppu_off();
  vram_adr(NTADR_A(2, 25));
  vram_fill(0x20, 16);
  vram_adr(NTADR_A(2,25));
  vram_write("SCORE: ", 7);
  vram_write(score_str, strlen(score_str));
  ppu_on_all();
}

void display_game_over(){
  ppu_off();
  vram_adr(NTADR_A(11,9));
  vram_write("GAME OVER", 9);
  vram_adr(NTADR_A(7,11));
  vram_write("PRESS B TO RESTART", 18);
  ppu_on_all();
}

void init(){
  score = 0;
  x = 0;
  y = 0;
  frame_counter = 0;
  game_over = false;
  for(z = 0; z < MAX_SCORE; z++){
    pattern[z] = 0;
  }
  display_score();
}

void restart(){
  ppu_off();
  vram_adr(NTADR_A(11, 9));
  vram_fill(0x20, 16);
  vram_adr(NTADR_A(7, 11));
  vram_fill(0x20, 32);
  init();  
  add_color();
  ppu_on_all();
  display_sequence();
}

void win(){
  ppu_off();
  vram_adr(NTADR_A(11,9));
  vram_write("YOU WIN!", 8);
  vram_adr(NTADR_A(7,11));
  vram_write("PRESS B TO RESTART", 18);
  ppu_on_all();
}

void check_match(int input){
  frame_counter = 0;
  if(input != pattern[y]){
    play_sound_effect(5);
    game_over = true;
    display_game_over();
    return;
  } else{
    display_color(input);
    y++;
  }
  if(y > score){
    y = 0;
    score++;
    if(score >= MAX_SCORE){
      game_over = true;
      win();
      return;
    }
    add_color();
    display_score();
    display_sequence(); 
  }
}

void display_title(){
  vram_adr(NTADR_A(13,2));
  vram_write("SIMON", 5);
}

void seed_rng(int seed) {
  rng_seed = seed;
  srand(rng_seed);
}

// setup PPU and tables
void setup_graphics() {
  // clear sprites
  oam_clear();
  // set palette colors
  pal_all(PALETTE);
}

void main(void)
{
  setup_graphics();
  apu_init();
  seed_rng(56);
  init();
  ppu_off();
  display_title();
  display_instructions();
  display_score();
  add_color();
  ppu_on_all();
  draw_simon_sprites();
  display_sequence();
  while (1) {
    char pad = pad_poll(0);
    if(!game_over){
      frame_counter++;
      if (frame_counter > 300) { // 300 frames = 5 seconds
        game_over = true;
        play_sound_effect(5); // Play buzz sound
        display_game_over();
      }
      if(pad & PAD_UP){
        check_match(1);
        //display_color(1);
      }
      if(pad & PAD_RIGHT){
        check_match(2);
        //display_color(4);
      }
      if(pad & PAD_LEFT){
        check_match(3);
        //display_color(3);
      }
      if(pad & PAD_DOWN){
        check_match(4);
        //display_color(2);
      } 
    } else{
        if(pad & PAD_B){
          restart();
        }
    }
    ppu_wait_frame();
  }
}
