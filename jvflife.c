#include <conio.h>
#include <graph.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define GRID_X 128
#define GRID_Y  48
#define MAX_X (GRID_X-1)
#define MAX_Y (GRID_Y-1)

typedef short idx;
typedef char  val;
typedef unsigned char byte;

void jvf_init() {
  inp(0x02F0);
}


byte rand_byte() {
  return rand() >> 8;
}

val  grid_a[GRID_Y*GRID_X];
val  grid_b[GRID_Y*GRID_X];
val* grid_r = grid_a;
val* grid_w = grid_b;

#define IDX(x,y) ((y)*GRID_X + (x))

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

void draw_border() {
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

void grid_flip() {
  val* tmp;
  tmp = grid_r;
  grid_r = grid_w;
  grid_w = tmp;
  
  _fmemcpy(vmem + BUF_HALF_OFF,
           &vmem_buf[0],
           BUF_HALF_SIZE);
  _fmemcpy(vmem + ILACE_OFF + BUF_HALF_OFF,
           &vmem_buf[BUF_HALF_SIZE],
           BUF_HALF_SIZE);
}

void grid_set(idx x, idx y, val v) {
  unsigned short off;
  byte mask;

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

void grid_seed() {
  idx x,y;
  for (y=0; y<GRID_Y; y++) {
    for (x=0; x<GRID_X; x++) {
      grid_set(x, y, rand_byte() % 2);
    }
  }
  grid_flip();
}

#define SET_ALIVE(x,y) grid_set((x), (y), (nghb == 3) || ((nghb == 2) && self))
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

  // corners

  self = GR(0,0);
  nghb = GR(MAX_X,MAX_Y) + GR(0,MAX_Y) + GR(1,MAX_Y)
       + GR(MAX_X,    0)               + GR(1,    0)
       + GR(MAX_X,    1) + GR(0,    1) + GR(1,    1);
  SET_ALIVE(0,0);

  self = GR(MAX_X, 0);
  nghb = GR(MAX_X-1,MAX_Y) + GR(MAX_X,MAX_Y) + GR(0,MAX_Y)
       + GR(MAX_X-1,    0)                   + GR(0,    0)
       + GR(MAX_X-1,    1) + GR(MAX_X,    1) + GR(0,    1);
  SET_ALIVE(MAX_X, 0);

  self = GR(0, MAX_Y);
  nghb = GR(MAX_X, MAX_Y-1) + GR(0, MAX_Y-1) + GR(1, MAX_Y-1)
       + GR(MAX_X,   MAX_Y)                  + GR(1,   MAX_Y)
       + GR(MAX_X,       0) + GR(0,       0) + GR(1,       0);
  SET_ALIVE(0, MAX_Y);

  self = GR(MAX_X, MAX_Y);
  nghb = GR(MAX_X-1, MAX_Y-1) + GR(MAX_X, MAX_Y-1) + GR(0, MAX_Y-1)
       + GR(MAX_X-1,   MAX_Y)                      + GR(0,   MAX_Y)
       + GR(MAX_X-1,       0) + GR(MAX_X,       0) + GR(0,       0);
  SET_ALIVE(MAX_X, MAX_Y);

  // left and right borders
  for (y=1; y<MAX_Y; y++) {
    ym = y-1;
    yp = y+1;

    self = GR(0,y);
    nghb = GR(MAX_X, ym) + GR(0, ym) + GR(1, ym)
         + GR(MAX_X,  y)             + GR(1,  y)
         + GR(MAX_X, yp) + GR(0, yp) + GR(1, yp);
    SET_ALIVE(0,y);

    self = GR(MAX_X,y);
    nghb = GR(MAX_X-1, ym) + GR(MAX_X, ym) + GR(0, ym)
         + GR(MAX_X-1,  y)                 + GR(0,  y)
         + GR(MAX_X-1, yp) + GR(MAX_X, yp) + GR(0, yp);
    SET_ALIVE(MAX_X,y);
  }

  // top and bottom borders
  for (x=1; x<MAX_X; x++) {
    xm = x-1;
    xp = x+1;

    self = GR(x,0);
    nghb = GR(xm, MAX_Y) + GR(x, MAX_Y) + GR(xp, MAX_Y)
         + GR(xm,     0)                + GR(xp,     0)
         + GR(xm,     1) + GR(x,     1) + GR(xp,     1);
    SET_ALIVE(x,0);

    self = GR(x,MAX_Y);
    nghb = GR(xm, MAX_Y-1) + GR(x, MAX_Y-1) + GR(xp, MAX_Y-1)
         + GR(xm,   MAX_Y)                  + GR(xp,   MAX_Y)
         + GR(xm,       0) + GR(x,       0) + GR(xp,       0);
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

  if (rand_byte() < 16) spawn_glider();

  grid_flip();
}

int main() {
  jvf_init();
  srand(time(NULL));
  _setvideomode(_MRES4COLOR);
  draw_border();
  grid_seed();
  for (;;) {
    step();
    if (kbhit()) break;
  }
  _setvideomode(_DEFAULTMODE);
  return 0;
}
