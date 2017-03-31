#include <stdio.h>

typedef struct {
  int x;
  int y;
} Point;

Point pts[10];
int ints[10];

/* All Adds could be approximated */
void allApprox1(int arg1, Point p) {
  arg1 = p.x + p.y;
  p.x = arg1 + 3;
  p.y = p.x + 5 * p.y;
  ints[0] = arg1;
  pts[0] = p;
}

void allApprox2(int arg1, Point p) {
  arg1 = p.x + p.y;
  p.y = p.x + 5 * p.y;
  ints[p.x] = arg1;
  pts[p.x] = p;
}

void allApprox3(int arg1, Point p) {
  p.x = arg1 + 3;
  p.y = p.x + 5 * p.y;
  ints[arg1] = arg1;
  pts[arg1] = p;
}

/* none of the adds could be approximated */
void noneApprox1(int arg1, Point p) {
  arg1 += 1;
  p.x = arg1 + p.y;
  p.y = arg1 + p.x;
  ints[arg1] = 10;
  pts[p.x] = p;
  pts[p.y] = p;
}

void noneApprox2(int arg1, Point p) {
  pts[p.x + p.y] = p;
}

void whileloopNoneApprox(int arg1, Point p) {
  while (arg1 > 0) {
    arg1 += 1;
    pts[arg1] = p;
  }
}

void forloop1(int arg1, Point p) {
  int i = 0;
  for (int i = 0; i < 1000; i++) {
    pts[i] = p;
  }
}

void forloop2(int arg1, Point p) {
  int i = 0;
  for (int i = 0; i < arg1; i++) {
    pts[i] = p;
  }
}

void forloop3(int arg1, Point p) {
  int i = 0;
  for (int i = 0; i < 1000; i++) {
    ints[i] = p.x + p.y + i;
  }
}

void forloop4(int arg1, Point p) {
  int i = 0;
  for (int i = 0; i < arg1; i++) {
    ints[i] = p.x + p.y + i;
  }
}

void conditions1(int arg1, Point p){
  arg1 = arg1 + p.x;
  if(arg1 > 0) {
    ints[arg1] = 1;
  } else {
    ints[5] = 3;
  }
}

void conditions2(int arg1, Point p){
  arg1 = arg1 + p.x;
  if(arg1 > 0) {
    ints[5] = 3;
  } else {
    ints[arg1] = 1;
  }
}

void conditions3(int arg1, Point p){
  arg1 = arg1 + p.x;
  if(arg1 > 0) {
    ints[arg1] = 3;
  } else {
    ints[arg1] = 1;
  }
}

void conditions4(int arg1, Point p){
  if(arg1 > 0) {
    p.y = arg1 + p.x;
    pts[arg1] = p;
  } else {
    p.x = arg1 + p.y;
    pts[arg1] = p;
  }
}

void conditions5(int arg1, Point p){
  arg1 = arg1 + p.x + p.y;
  if(arg1 > 21) {
    ints[0] = 1;
  } else {
    ints[1] = 1;
  }
}

void conditions6(int arg1, Point p) {
  int i = 0;
  for(i = 0; i < arg1; i++) {
    if(i > 10) {
      ints[0] = 1;
    }
  }
}

void conditions7(int arg1, Point p){
  if (p.x > 10) {
    arg1++;
    p.y += 2;
  }
  if (arg1 > 10) {
    if (p.y < 1) {
      ints[3] = 6;
    }
  }
}

void extra1(int arg1, Point p) {
   int i;
   for(i = 0; i < arg1; i++) {
     if (arg1 * 1 > p.x + p.y) {
       break;
     }
   }
}

void extra2(int arg1, Point p) {
   int i;
   for(i = 0; i < arg1; i++) {
     if (arg1 * 1 > p.x + p.y) {
       continue;
     }
     i *= 2;
   }
}

int extra3(int arg1, Point p) {
  return arg1+p.x+p.y;
}

int extra4(int arg1, Point p) {
  return arg1+p.x+ints[1]+p.y;
}

void extra5(int arg1, int* address) {
  address[1] = arg1 + 123;
  ints[address[1]] = 5;
}


int main(int argc, char *argv[]) {

}
