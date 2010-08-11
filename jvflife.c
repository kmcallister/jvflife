// jvflife.c
// Game of Life for the JVF 2010-A LED display
//
// Written by Keegan McAllister <keegan _at_ t0rch.org>
//
// I release this code into the public domain with
// no restrictions whatsoever.

#include <conio.h>
#include <stdlib.h>
#include <time.h>
#include <i86.h>


// JVF 2010-A: 128x48 pixels

#define GRID_X 128
#define GRID_Y  48
#define MAX_X (GRID_X-1)
#define MAX_Y (GRID_Y-1)

typedef short idx;
typedef char  val;
typedef unsigned char byte;

byte rand_byte() {
  return rand() >> 8;
}


// Grid of cells.  We read from one buffer
// while updating the other.
val  grid_a[GRID_Y*GRID_X];
val  grid_b[GRID_Y*GRID_X];
val* grid_r = grid_a;
val* grid_w = grid_b;

#define IDX(x,y) ((y)*GRID_X + (x))


////////////////////////////////////////////////////////////
// Interfacing to the LED hardware
////////////////////////////////////////////////////////////


// 4 pixels per byte = 2 bits per pixel
// This allows 4-level greyscale, but we use monochrome only.
#define SCANLINE (GRID_X/4)
#define BUF_SIZE (GRID_Y*SCANLINE)

byte led_buf[BUF_SIZE];

// We communicate with the LED hardware on this I/O port.
#define CONFIG_PORT 0x02f0

// Parameters for DMA transmission.
byte dma_chan;
byte dma_mode;
byte dma_addr2_port;
byte dma_mask;
byte dma_done_mask;
byte dma_base_port;
byte dma_count_port;

// Initialize hardware and read DMA parameters.
void led_init() {
  byte config_byte = inp(CONFIG_PORT);

  // bit 0x40 tells us the DMA channel
  dma_chan = (config_byte & 0x40) ? 3 : 1;

  if (dma_chan == 3) {
    dma_mode       = 0x4B;
    dma_addr2_port = 0x82;
    dma_done_mask  = (1 << 3);
    dma_mask       = 3;
    dma_base_port  = 6;
    dma_count_port = 7;
  } else {
    dma_mode       = 0x49;
    dma_addr2_port = 0x83;
    dma_done_mask  = (1 << 1);
    dma_mask       = 1;
    dma_base_port  = 2;
    dma_count_port = 3;
  }
}

// Ports used to communicate with the first
// DMA controller, regardless of channel.
#define DMA0_STATUS  0x08
#define DMA0_MASK    0x0A
#define DMA0_MODE    0x0B
#define DMA0_COUNTER 0x0C

// Write a line of pixels from a given starting address
// to the LED hardware.
void dma_write(byte far* line) {
  unsigned int seg, off, addr, count;

  // set the DMA mode
  outp(DMA0_MODE, dma_mode);

  // reset counter; value is ignored
  outp(DMA0_COUNTER, 0xFF);

  // break the far pointer into segment and offset components
  seg = FP_SEG(line);
  off = FP_OFF(line);

  // compute the low 16 (of 20) bits of the *physical* address
  addr = ((seg & 0x0FFF) << 4) + off;

  // write the 20-bit physical address to the DMA controller
  outp(dma_base_port ,  addr &   0xFF);
  outp(dma_base_port , (addr & 0xFF00) >>  8);
  outp(dma_addr2_port, (seg  & 0xF000) >> 12);

  // counter is an inclusive upper bound, so write one less than
  // the number of bytes to transfer
  count = SCANLINE-1;
  outp(dma_count_port,  count &   0xFF);
  outp(dma_count_port, (count & 0xFF00) >> 8);

  // set the mask, commencing transmission
  outp(DMA0_MASK, dma_mask);

  // tell the LED hardware to begin reading?
  // exact purpose unclear
  outp(CONFIG_PORT, 0x8f);
  outp(CONFIG_PORT, 0x0f);
  outp(CONFIG_PORT,    7);
}

// Check whether DMA is finished.
inline byte dma_finished() {
  byte v = inp(DMA0_STATUS);
  return (v & dma_done_mask);
}

// The LED hardware keeps track of which line 
// to set with the next DMA transfer.

// Reset to the first line.
inline void line_reset() {
  outp(CONFIG_PORT, 5);
  outp(CONFIG_PORT, 7);
}

// Increment to the next line.
inline void line_incr() {
  outp(CONFIG_PORT, 6);
  outp(CONFIG_PORT, 7);
}

// Write all lines to the LED hardware.
void led_update() {
  idx y;

  line_reset();
  for (y=0; y<GRID_Y; y++) {
    dma_write(&led_buf[SCANLINE*y]);

    while (!dma_finished()) { }

    if (y < MAX_Y) line_incr();
  }
}


////////////////////////////////////////////////////////////
// Computing the cellular automaton
////////////////////////////////////////////////////////////


// Set a pixel in the grid, and in the
// LED output buffer.
void grid_set(idx x, idx y, val v) {
  unsigned short off;
  byte mask;

  grid_w[IDX(x,y)] = v ? 1 : 0;

  // four pixels per byte,
  // leftmost is most significant
  off = SCANLINE*y + (x >> 2);
  mask = 0xC0 >> ((x&3) << 1);
  if (v) {
    led_buf[off] |= mask;
  } else {
    led_buf[off] &= ~mask;
  }
}

// Flip the double-buffered grid, and write
// to the LED hardware.
void grid_flip() {
  val* tmp;
  tmp = grid_r;
  grid_r = grid_w;
  grid_w = tmp;

  led_update();
}

// Initialize the grid with randomness.
void grid_init() {
  idx x,y;
  for (y=0; y<GRID_Y; y++) {
    for (x=0; x<GRID_X; x++) {
      grid_set(x, y, rand_byte() & 1);
    }
  }
  grid_flip();
}

// Rule selection:
#define B(n) ((1 << (n)))
short born = B(3) | B(6);  // neighbor-counts where a cell is born
short live = B(2) | B(3);  // neighbor-counts where a cell survives
// Above is the HighLife rule.

// Occasionally we spawn a glider to keep things interesting.

val glider[9] = { 0, 0, 1, 1, 0, 1, 0, 1, 1 };

void spawn_glider() {
  idx xo,yo,dx,dy,ex,ey;
  byte refl, rx, ry;
  xo = rand_byte() % (GRID_X-2);
  yo = rand_byte() % (GRID_Y-2);
  refl = rand_byte();
  rx = refl & 1;  // reflect on x-axis?
  ry = refl & 2;  // reflect on y-axis?
  for (dy=0; dy<3; dy++) {
    for (dx=0; dx<3; dx++) {
      ex = rx ? (2-dx) : dx;
      ey = ry ? (2-dy) : dy;
      grid_set(xo+dx, yo+dy, glider[ey*3+ex]);
    }
  }
}

#define SET_ALIVE(x,y) grid_set((x), (y), (self ? live : born) & B(nghb))
#define GR(x,y) grid_r[IDX((x),(y))]

// One step of the cellular automaton.
void step() {
  idx x,y,xm,xp,ym,yp;
  val self, nghb;

  // implement wrapping with inversion
  // for projective-plane geometry
  // don't wrap the corners

  // corners

  self = GR(0,0);
  nghb = GR(1,0) + GR(0,1) + GR(1,1);
  SET_ALIVE(0,0);

  self = GR(MAX_X, 0);
  nghb = GR(MAX_X-1, 0) + GR(MAX_X-1, 1) + GR(MAX_X, 1);

  self = GR(0, MAX_Y);
  nghb = GR(0, MAX_Y-1) + GR(1, MAX_Y-1) + GR(1, MAX_Y);
  SET_ALIVE(0, MAX_Y);

  self = GR(MAX_X, MAX_Y);
  nghb = GR(MAX_X-1, MAX_Y-1) + GR(MAX_X, MAX_Y-1) + GR(MAX_X-1, MAX_Y);
  SET_ALIVE(MAX_X, MAX_Y);

  // left and right borders
  for (y=1; y<MAX_Y; y++) {
    ym = y-1;
    yp = y+1;

    self = GR(0,y);
    nghb = GR(MAX_X, MAX_Y-ym) + GR(0, ym) + GR(1, ym)
         + GR(MAX_X, MAX_Y- y)             + GR(1,  y)
         + GR(MAX_X, MAX_Y-yp) + GR(0, yp) + GR(1, yp);
    SET_ALIVE(0,y);

    self = GR(MAX_X,y);
    nghb = GR(MAX_X-1, ym) + GR(MAX_X, ym) + GR(0, MAX_Y-ym)
         + GR(MAX_X-1,  y)                 + GR(0, MAX_Y- y)
         + GR(MAX_X-1, yp) + GR(MAX_X, yp) + GR(0, MAX_Y-yp);
    SET_ALIVE(MAX_X,y);
  }

  // top and bottom borders
  for (x=1; x<MAX_X; x++) {
    xm = x-1;
    xp = x+1;

    self = GR(x,0);
    nghb = GR(MAX_X-xm, MAX_Y) + GR(MAX_X-x, MAX_Y) + GR(MAX_X-xp, MAX_Y)
         + GR(xm,     0)                + GR(xp,     0)
         + GR(xm,     1) + GR(x,     1) + GR(xp,     1);
    SET_ALIVE(x,0);

    self = GR(x,MAX_Y);
    nghb = GR(xm, MAX_Y-1) + GR(x, MAX_Y-1) + GR(xp, MAX_Y-1)
         + GR(xm,   MAX_Y)                  + GR(xp,   MAX_Y)
         + GR(MAX_X-xm,       0) + GR(MAX_X-x,       0) + GR(MAX_X-xp,       0);
    SET_ALIVE(x,MAX_Y);
  }

  // interior
  for (y=1; y<MAX_Y; y++) {
    for (x=1; x<MAX_X; x++) {
      xm = x-1;
      xp = x+1;
      ym = y-1;
      yp = y+1;

      self = GR(x,y);
      nghb = GR(xm, ym) + GR(x, ym) + GR(xp, ym)
           + GR(xm,  y)             + GR(xp,  y)
           + GR(xm, yp) + GR(x, yp) + GR(xp, yp);
      SET_ALIVE(x,y);
    }
  }

  // With low probability, spawn a glider.
  if (rand_byte() < 4) spawn_glider();

  grid_flip();
}


////////////////////////////////////////////////////////////
// Entry point
////////////////////////////////////////////////////////////


int main() {
  srand(time(NULL));
  led_init();
  grid_init();

  for (;;) {
    step();

    // Exit on keypress
    if (kbhit()) break;
  }

  return 0;
}
