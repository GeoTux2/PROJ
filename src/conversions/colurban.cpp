
#define PJ_LIB__

#include "proj.h"
#include "proj_internal.h"

PROJ_HEAD(colurban,    "Colombia Urban");


static PJ_XY forward(PJ_LP lp, PJ *P) {
    PJ_XY xy = {0.0, 0.0};
    (void) P;
    xy.x = lp.lam;
    xy.y = lp.phi;
    return xy;
}

static PJ_LP inverse(PJ_XY xy, PJ *P) {
    PJ_LP lp = {0.0, 0.0};
    (void) P;
    lp.phi = xy.y;
    lp.lam = xy.x;
    return lp;
}

PJ *CONVERSION(colurban, 0) {
    P->fwd = forward;
    P->inv = inverse;
    P->left  = PJ_IO_UNITS_RADIANS;
    P->right = PJ_IO_UNITS_CARTESIAN;

    return P;
}
