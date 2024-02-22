#include "aux.h"
#include "macro.h"
#include <math.h>

struct point rotateAbout(struct point pivot, struct point org, float rad)  {
  float newAng = atan((pivot.y-org.y)/(pivot.x-org.x)) + rad;
  if(pivot.x > org.x) newAng += PI;
  return (struct point){.x=pivot.x + DISTP(pivot, org) * cos(newAng), .y=pivot.y + DISTP(pivot, org) * sin(newAng)};
}

struct point dilateAbout(struct point pivot, struct point org, float scale) {
  return (struct point){.x=pivot.x - (pivot.x-org.x) * scale, .y=pivot.y - (pivot.y - org.y) * scale};  
}

