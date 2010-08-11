#include <conio.h>
#include <graph.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <i86.h>

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

val  grid_a[GRID_Y*GRID_X];
val  grid_b[GRID_Y*GRID_X];
val* grid_r = grid_a;
val* grid_w = grid_b;

#define IDX(x,y) ((y)*GRID_X + (x))



// Define for a CGA graphics simulation;
// undef  to draw to real LED hardware.
#undef DRAW_CGA

#ifdef DRAW_CGA



// CGA 320x200 interlaced mode
// 80 bytes per scanline
// 4 pixels per byte
// grid starts 52 pixels = 26 scanline pairs from top
// grid starts 40 pixels = 20 bytes from left
// draw 2x2 grid of pixels for one LED
//   => half of one byte, on each of a pair of lines

#define SCANLINE 80
#define BUF_HALF_SIZE (GRID_Y*SCANLINE)
#define BUF_HALF_OFF  (26*SCANLINE)

byte vmem_buf[BUF_HALF_SIZE*2];
byte far* vmem = (byte far*) 0xB8000000L;
#define ILACE_OFF 0x2000

void graphics_init() {
  _setvideomode(_MRES4COLOR);

  _rectangle(_GFILLINTERIOR,  29,  42, 306,  49);
  _rectangle(_GFILLINTERIOR,  29, 150, 306, 157);
  _rectangle(_GFILLINTERIOR,  29,  50,  36, 149);
  _rectangle(_GFILLINTERIOR, 299,  50, 306, 149);

  _fmemcpy(&vmem_buf[0],
           vmem + BUF_HALF_OFF,
           BUF_HALF_SIZE);
  _fmemcpy(&vmem_buf[BUF_HALF_SIZE],
           vmem + ILACE_OFF + BUF_HALF_OFF,
           BUF_HALF_SIZE);
}

void graphics_update() {
  _fmemcpy(vmem + BUF_HALF_OFF,
           &vmem_buf[0],
           BUF_HALF_SIZE);
  _fmemcpy(vmem + ILACE_OFF + BUF_HALF_OFF,
           &vmem_buf[BUF_HALF_SIZE],
           BUF_HALF_SIZE);
}

void graphics_finalize() {
  _setvideomode(_DEFAULTMODE);
}

void grid_set(idx x, idx y, val v) {
  unsigned short off;
  byte mask;

  v = v ? 1 : 0;

  grid_w[IDX(x,y)] = v;

  off = SCANLINE*y + 10 + (x >> 1);
  mask = (x&1) ? 0x0F : 0xF0;
  if (v) {
    vmem_buf[off] |= mask;
    vmem_buf[off+BUF_HALF_SIZE] |= mask;
  } else {
    mask ^= 0xFF;
    vmem_buf[off] &= mask;
    vmem_buf[off+BUF_HALF_SIZE] &= mask;
  }
}



#else



// Draw to the real LED hardware

#define SCANLINE (GRID_X/4)
#define BUF_SIZE (GRID_Y*SCANLINE)

#define CONFIG_PORT 0x02f0

byte led_buf[BUF_SIZE];

byte dma_chan;
byte dma_mode;
byte dma_addr2_port;
byte dma_mask;
byte dma_done_mask;
byte dma_base_port;
byte dma_count_port;

void graphics_init() {
  byte config_byte = inp(CONFIG_PORT);

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

  //printf("Initialized DMA on channel %d\n", dma_chan);
}

void graphics_finalize() { }

#define DMA0_STATUS  0x08
#define DMA0_MASK    0x0A
#define DMA0_MODE    0x0B
#define DMA0_COUNTER 0x0C

void dma_write(byte far* line) {
  unsigned int seg, off, addr, count;

  outp(DMA0_MODE,    dma_mode);
  outp(DMA0_COUNTER, 0xFF    );

  seg = FP_SEG(line);
  off = FP_OFF(line);

  addr = ((seg & 0x0FFF) << 4) + off;
  outp(dma_base_port ,  addr &   0xFF);
  outp(dma_base_port , (addr & 0xFF00) >>  8);
  outp(dma_addr2_port, (seg  & 0xF000) >> 12);

  count = SCANLINE-1;
  outp(dma_count_port,  count &   0xFF);
  outp(dma_count_port, (count & 0xFF00) >> 8);

  outp(DMA0_MASK, dma_mask);

  outp(CONFIG_PORT, 0x8f);
  outp(CONFIG_PORT, 0x0f);
  outp(CONFIG_PORT,    7);
}

inline byte dma_finished() {
  byte v = inp(DMA0_STATUS);
  return (v & dma_done_mask);
}

inline void lineaddr_reset() {
  outp(CONFIG_PORT, 5);
  outp(CONFIG_PORT, 7);
}

inline void lineaddr_incr() {
  outp(CONFIG_PORT, 6);
  outp(CONFIG_PORT, 7);
}

void graphics_update() {
  idx y;

  lineaddr_reset();
  for (y=0; y<GRID_Y; y++) {
    dma_write(&led_buf[SCANLINE*y]);
    while (!dma_finished());
    lineaddr_incr();
  }
}

void grid_set(idx x, idx y, val v) {
  unsigned short off;
  byte mask;

  v = v ? 1 : 0;

  grid_w[IDX(x,y)] = v;

  off = SCANLINE*y + (x >> 2);
  mask = 0xC0 >> ((x&3) << 1);
  if (v) {
    led_buf[off] |= mask;
  } else {
    led_buf[off] &= ~mask;
  }
}

#endif




void grid_flip() {
  val* tmp;
  tmp = grid_r;
  grid_r = grid_w;
  grid_w = tmp;

  graphics_update();
}

void grid_init() {
  idx x,y;
  for (y=0; y<GRID_Y; y++) {
    for (x=0; x<GRID_X; x++) {
      grid_set(x, y, rand_byte() & 1);
    }
  }
  grid_flip();
}

// rule selection
#define B(n) ((1 << (n)))
// HighLife
short born = B(3) | B(6);
short live = B(2) | B(3);

#define SET_ALIVE(x,y) grid_set((x), (y), (self ? live : born) & B(nghb))
#define GR(x,y) grid_r[IDX((x),(y))]

val glider[9] = { 0, 0, 1, 1, 0, 1, 0, 1, 1 };

void spawn_glider() {
  idx xo,yo,dx,dy,ex,ey;
  byte refl, rx, ry;
  xo = rand_byte() % (GRID_X-2);
  yo = rand_byte() % (GRID_Y-2);
  refl = rand_byte();
  rx = refl & 1;
  ry = refl & 2;
  for (dy=0; dy<3; dy++) {
    for (dx=0; dx<3; dx++) {
      ex = rx ? (2-dx) : dx;
      ey = ry ? (2-dy) : dy;
      grid_set(xo+dx, yo+dy, glider[ey*3+ex]);
    }
  }
}

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

  if (rand_byte() < 4) spawn_glider();

  grid_flip();
}

int main() {
  srand(time(NULL));
  graphics_init();
  grid_init();
  for (;;) {
    step();
    if (kbhit()) break;
  }
  graphics_finalize();
  return 0;
}
