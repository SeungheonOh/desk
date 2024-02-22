#include "aux.h"
#include "macro.h"
#include <math.h>

struct point rotateAbout(struct point pivot, struct point org, float rad)  {
  float newAng = atan((pivot.y-org.y)/(pivot.x-org.x)) + rad;
  if(pivot.x > org.x) newAng += PI;
  float newX = pivot.x + DISTP(pivot, org) * cos(newAng);
  float newY = pivot.y + DISTP(pivot, org) * sin(newAng);
  return (struct point){.x=newX, .y=newY};
}

