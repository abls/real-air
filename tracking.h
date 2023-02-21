#ifndef TRACKING_H
#define TRACKING_H

#include "cglm/cglm.h"

int tracking_start();
const void tracking_get(versor out);
const void tracking_set(versor ref);
void tracking_stop();

#endif
