# classicGamesSP

[Open this project in 8bitworkshop](http://8bitworkshop.com/redir.html?platform=nes&githubURL=https%3A%2F%2Fgithub.com%2FMG621%2FclassicGamesSP&file=numberGuessing.c)

A collection of three classic-style games written in **C** targeting the **Nintendo Entertainment System (NES / 6502 architecture)**, developed as a Computer Science senior project using the [8bitworkshop](https://8bitworkshop.com/) IDE.

---

## Games Included

### 1. Number Guessing Game (`numberGuessing.c`)
* **Overview:** A text- and input-driven guessing game where the player narrows down a hidden target number using controller input and on-screen feedback.
* **Key Concepts:** Controller polling, pseudo-random number generation (PRNG) on 8-bit hardware, and PPU nametable text rendering.

### 2. Simon Memory Game (`simon.c`)
* **Overview:** A pattern-recognition and memory game inspired by the classic electronic toy. Players observe an increasingly complex sequence of visual/audio cues and replicate them via D-pad and button inputs.
* **Key Concepts:** State machine logic, frame-based timing delays (NMI synchronization), audio register manipulation (APU), and palette/tile swapping.

### 3. Side-Scrolling Space Shooter (`sidescroller.c`)
* **Overview:** A classic arcade-style horizontal shoot-'em-up featuring player movement, projectile firing, enemy wave spawning, and collision detection.
* **Key Concepts:** Hardware sprite management (OAM), real-time bounding-box collision detection, background scrolling, and frame-budget optimization to prevent slowdown.

---

## Technical Highlights & Architecture

Developing for the NES requires working directly within strict 1980s hardware constraints without standard modern OS abstractions:

* **Language & Toolchain:** Written in C and compiled for the MOS 6502 microprocessor (via `cc65` inside 8bitworkshop).
* **Memory & Resource Management:** Operates within 2 KB of internal work RAM and 2 KB of video RAM (VRAM), requiring tight control over variable allocation and static memory layout.
* **Graphics & Hardware Registers:** Direct interaction with the Picture Processing Unit (PPU) for tile rendering, color palettes, and hardware sprite positioning.
* **Game Loop & Timing:** Frame-locked execution synchronized to the NES Vertical Blank (VBlank) interrupt to ensure deterministic game speed and tear-free rendering.

---

## How to Run

### Option 1: Instant Browser Play (Recommended)
Click the **[Open this project in 8bitworkshop](http://8bitworkshop.com/redir.html?platform=nes&githubURL=https%3A%2F%2Fgithub.com%2FMG621%2FclassicGamesSP&file=numberGuessing.c)** link at the top of this page to compile and run the ROMs directly in your browser. Use the file dropdown in the top-left corner of the IDE to switch between the three games.

### Option 2: Local Emulator
1. Compile the desired `.c` source file using the `cc65` toolchain targeting `nes` to generate a `.nes` ROM file.
2. Load the resulting `.nes` ROM into any standard NES emulator (e.g., FCEUX, Mesen, Nestopia).

---

## Controls (Standard NES Controller)
* **D-Pad:** Navigate menus / Move ship / Input sequence
* **A / B Buttons:** Confirm selection / Fire weapon
* **Start:** Begin game / Pause
