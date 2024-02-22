#pragma once

struct point {
  float x, y;
};

#define DISTP(p1, p2) (sqrt(pow(p1.x-p2.x, 2) + pow(p1.y-p2.y, 2)))

// Rotate a point about a pivot given radian amount.
struct point rotateAbout(struct point, struct point, float);
struct point dilateAbout(struct point, struct point, float);
