#include <conio.h>
#include <graph.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dos.h>

void draw_border() {
  _rectangle(_GFILLINTERIOR,  29,  42, 306,  49);
  _rectangle(_GFILLINTERIOR,  29, 150, 306, 157);
  _rectangle(_GFILLINTERIOR,  29,  50,  36, 149);
  _rectangle(_GFILLINTERIOR, 299,  50, 306, 149);
}

#define GRID_X 128
#define GRID_Y  48

#define ORIG_X  37
#define ORIG_Y  50

#define DELAY_MS 33

typedef short coord;
typedef short idx;
typedef char  val;

val grid_a[GRID_Y*GRID_X];
val grid_b[GRID_Y*GRID_X];
val* grid_r = grid_a;
val* grid_w = grid_b;

#define IDX(x,y) ((y)*GRID_X + (x))

void grid_flip() {
  val* tmp;
  tmp = grid_r;
  grid_r = grid_w;
  grid_w = tmp;
}

void grid_set(idx x, idx y, val v) {
  coord xp, yp;

  grid_w[IDX(x,y)] = v;
  xp = ORIG_X + 2*x;
  yp = ORIG_Y + 2*y;
  _setcolor(v ? 3 : 0);
  _rectangle(_GFILLINTERIOR, xp, yp, xp+1, yp+1);
}

void grid_seed() {
  idx x,y;
  for (y=0; y<GRID_Y; y++) {
    for (x=0; x<GRID_X; x++) {
      grid_set(x, y, rand() & 0x100 ? 1 : 0);
    }
  }
  grid_flip();
}

val grid_get(idx x, idx y) {
  if      (x <  0     ) x += GRID_X;
  else if (x >= GRID_X) x -= GRID_X;
  if      (y <  0     ) y += GRID_Y;
  else if (y >= GRID_Y) y -= GRID_Y;

  return grid_r[IDX(x,y)];
}

void step() {
  idx x,y;
  for (y=0; y<GRID_Y; y++) {
    for (x=0; x<GRID_X; x++) {
      val self = grid_get(x, y);
      val nghb
        = grid_get(x-1, y-1)
        + grid_get(x  , y-1)
        + grid_get(x+1, y-1)
        + grid_get(x-1, y  )
        + grid_get(x+1, y  )
        + grid_get(x-1, y+1)
        + grid_get(x  , y+1)
        + grid_get(x+1, y+1);
      grid_set(x, y, (nghb == 3) || ((nghb == 2) && self));
    }
  }
  grid_flip();
}

int main() {
  srand(time(NULL));
  _setvideomode(_MRES4COLOR);
  draw_border();
  grid_seed();
  for (;;) {
    step();
    //delay(DELAY_MS);
    if (kbhit()) break;
  }
  _setvideomode(_DEFAULTMODE);
  return 0;
}
