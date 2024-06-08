
#include <stdlib.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>

#include <time.h>
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

/*{pal:"nes",layout:"nes"}*/

void initialize();
void display_title();
void display_instructions();
void start_game();
void display_guess_screen();
void check_guess();
void display_result(int correct);
void seed_rng(int seed);
void title_screen();
void clear_screen();

const char PALETTE[32] = { 
  0x03,			// screen color

  0x11,0x30,0x27,0x0,	// background palette 0
  0x1c,0x20,0x2c,0x0,	// background palette 1
  0x00,0x10,0x20,0x0,	// background palette 2
  0x06,0x16,0x26,0x0,   // background palette 3

  0x16,0x35,0x24,0x0,	// sprite palette 0
  0x00,0x37,0x25,0x0,	// sprite palette 1
  0x0d,0x2d,0x3a,0x0,	// sprite palette 2
  0x0d,0x27,0x2a	// sprite palette 3
};

int correct_num;
int guess;
int guesses;
int rng_seed;
int up_frame_counter;
int down_frame_counter;
char a_pressed;
char b_pressed;
bool won;

// setup PPU and tables
void setup_graphics() {
  // clear sprites
  oam_clear();
  // set palette colors
  pal_all(PALETTE);
}

void clear_screen() {
  // Clear the screen
  vram_adr(NTADR_A(0, 0));
  vram_fill(0x20, 1024);
}

void title_screen() {
    ppu_off();
    clear_screen();
    // Display title
    vram_adr(NTADR_A(6, 9));
    vram_write("NUMBER GUESSING GAME", 20);

    // Display start instructions
    //ppu_on_all();
    vram_adr(NTADR_A(6, 18));
    vram_write("PRESS START TO BEGIN", 20);
    //ppu_on_all();
    vram_adr(NTADR_A(6, 25));
    vram_write("(C)", 3);
    vram_write("2024 MARIO GARZA", 16);
  	
    ppu_on_all();
    while (1) {
        char pad = pad_poll(0);

        if (pad & PAD_START) {
            // Start the game when Start button is pressed
            break;
        }

        ppu_wait_frame();
    }

    // Clear the screen before starting the game
    vram_adr(NTADR_A(0, 0));
    vram_fill(0x20, 1024);
}

void init(){
  correct_num = (rand() % 10) + 1;
  guess = 1;
  guesses = 0;
  up_frame_counter = 0;
  down_frame_counter = 0;
  a_pressed = 0;
  b_pressed = 0;
  won = false;
}

void display_title(){
  vram_adr(NTADR_A(2,2));
  vram_write("NUMBER GUESSING GAME", 20); 
}

void display_instructions(){
  vram_adr(NTADR_A(2,5));
  vram_write("ENTER A NUMBER (1-10) ", 22);
  vram_adr(NTADR_A(2,7));
  vram_write("USE UP/DOWN TO CHANGE", 21);
  vram_adr(NTADR_A(2,9));
  vram_write("PRESS A TO SUBMIT", 17);
}

void start_game() {
    ppu_off();
    clear_screen();
    display_title();
    display_instructions();
    display_guess_screen();
    ppu_on_all();

    while(1) {
        // Read controller input
        char pad = pad_poll(0);
        
      	if(!won) {
          if(pad & PAD_UP) {
          if(up_frame_counter == 0 || up_frame_counter > 60){
            guess++;
            if(guess > 10) guess = 1; // Wrap around
            display_guess_screen();
            if (up_frame_counter == 0) up_frame_counter = 1;
          }
          up_frame_counter++;
          } else {
            up_frame_counter = 0;
          }

          if(pad & PAD_DOWN) {
            if(down_frame_counter == 0 || down_frame_counter > 60){
              guess--;
              if(guess < 1) guess = 10; // Wrap around
              display_guess_screen();
              if (down_frame_counter == 0) down_frame_counter = 1;
            }
            down_frame_counter++;
          } else {
            down_frame_counter = 0;
          }

          if(pad & PAD_A) {
              if (!a_pressed) {
                  check_guess();
                  display_guess_screen();
                  a_pressed = 1; // Mark A as pressed
              }
          } else {
              a_pressed = 0; // Reset A button state if released
          }
        }
        
      	else {
          if(pad & PAD_B) {
            if (!b_pressed) {
                // Reset the game
                init();
              	ppu_on_all();
                vram_adr(NTADR_A(2, 12));
    		vram_fill(0x20, 32); // Clear two lines to avoid overlap
              	vram_adr(NTADR_A(2, 13));
    		vram_fill(0x20, 32); // Clear two lines to avoid overlap
                display_guess_screen();
                b_pressed = 1; // Mark B as pressed
            }
          } else {
              b_pressed = 0; // Reset B button state if released
          }
        }
      
      ppu_wait_frame();
    }
}

void display_guess_screen() {
    char guess_str[3];
    itoa(guess, guess_str, 10);    
    ppu_off();
  // Clear previous guess display
    vram_adr(NTADR_A(2, 11));
    vram_fill(0x20, 16);
    vram_adr(NTADR_A(2, 11));
    vram_write(guess_str, strlen(guess_str));
    ppu_on_all();
}

void check_guess() {
    guesses++;
    
    if(guess == correct_num) {
      	won = true;
        display_result(1);
    } else {
        display_result(0);
    }
}

void display_result(int correct) {
    ppu_off();
    vram_adr(NTADR_A(2, 12));
    vram_fill(0x20, 32); // Clear two lines to avoid overlap
    // Clear the area where the result will be displayed
    if(correct) {
        vram_adr(NTADR_A(2, 12));
        vram_write("CORRECT! YOU WIN!", 17);
        vram_adr(NTADR_A(2, 13));
        vram_write("PRESS B TO PLAY AGAIN", 21);
    } else {
        vram_adr(NTADR_A(2, 12));
        vram_write("WRONG! TRY AGAIN!", 18);
    }
    ppu_on_all();
}

// Simple RNG seed function
void seed_rng(int seed) {
    rng_seed = seed;
    srand(rng_seed);
}

void main(void)
{
  seed_rng(42);
  setup_graphics();
  init();
  title_screen();
  start_game();
}
