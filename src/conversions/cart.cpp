/******************************************************************************
 * Project:  PROJ.4
 * Purpose:  Convert between ellipsoidal, geodetic coordinates and
 *           cartesian, geocentric coordinates.
 *
 *           Formally, this functionality is also found in the PJ_geocent.c
 *           code.
 *
 *           Actually, however, the PJ_geocent transformations are carried
 *           out in concert between 2D stubs in PJ_geocent.c and 3D code
 *           placed in pj_transform.c.
 *
 *           For pipeline-style datum shifts, we do need direct access
 *           to the full 3D interface for this functionality.
 *
 *           Hence this code, which may look like "just another PJ_geocent"
 *           but really is something substantially different.
 *
 * Author:   Thomas Knudsen, thokn@sdfe.dk
 *
 ******************************************************************************
 * Copyright (c) 2016, Thomas Knudsen / SDFE
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#define PJ_LIB__

#include "proj_internal.h"
#include <math.h>

PROJ_HEAD(cart,    "Geodetic/cartesian conversions");


/**************************************************************
                CARTESIAN / GEODETIC CONVERSIONS
***************************************************************
    This material follows:

    Bernhard Hofmann-Wellenhof & Helmut Moritz:
    Physical Geodesy, 2nd edition.
    Springer, 2005.

    chapter 5.6: Coordinate transformations
    (HM, below),

    and

    Wikipedia: Geographic Coordinate Conversion,
    https://en.wikipedia.org/wiki/Geographic_coordinate_conversion

    (WP, below).

    The cartesian-to-geodetic conversion is based on Bowring's
    celebrated method:

    B. R. Bowring:
    Transformation from spatial to geographical coordinates
    Survey Review 23(181), pp. 323-327, 1976

    (BB, below),

    but could probably use some TLC from a newer and faster
    algorithm:

    Toshio Fukushima:
    Transformation from Cartesian to Geodetic Coordinates
    Accelerated by Halley’s Method
    Journal of Geodesy, February 2006

    (TF, below).

    Close to the poles, we avoid singularities by switching to an
    approximation requiring knowledge of the geocentric radius
    at the given latitude. For this, we use an adaptation of the
    formula given in:

    Wikipedia: Earth Radius
    https://en.wikipedia.org/wiki/Earth_radius#Radius_at_a_given_geodetic_latitude
    (Derivation and commentary at https://gis.stackexchange.com/q/20200)

    (WP2, below)

    These routines are probably not as robust at those in
    geocent.c, at least thay haven't been through as heavy
    use as their geocent sisters. Some care has been taken
    to avoid singularities, but extreme cases (e.g. setting
    es, the squared eccentricity, to 1), will cause havoc.

**************************************************************/


/*********************************************************************/
static double normal_radius_of_curvature (double a, double es, double phi) {
/*********************************************************************/
    double s = sin(phi);
    if (es==0)
        return a;
    /* This is from WP.  HM formula 2-149 gives an a,b version */
    return a / sqrt (1 - es*s*s);
}

/*********************************************************************/
static double geocentric_radius (double a, double b, double phi) {
/*********************************************************************
    Return the geocentric radius at latitude phi, of an ellipsoid
    with semimajor axis a and semiminor axis b.

    This is from WP2, but uses hypot() for potentially better
    numerical robustness
***********************************************************************/
    return hypot (a*a*cos (phi), b*b*sin(phi)) / hypot (a*cos(phi), b*sin(phi));
}


/*********************************************************************/
static PJ_XYZ cartesian (PJ_LPZ geod,  PJ *P) {
/*********************************************************************/
    double N, cosphi = cos(geod.phi);
    PJ_XYZ xyz;

    N   =  normal_radius_of_curvature(P->a, P->es, geod.phi);

    /* HM formula 5-27 (z formula follows WP) */
    xyz.x = (N + geod.z) * cosphi      * cos(geod.lam);
    xyz.y = (N + geod.z) * cosphi      * sin(geod.lam);
    xyz.z = (N * (1 - P->es) + geod.z) * sin(geod.phi);

    return xyz;
}


/*********************************************************************/
static PJ_LPZ geodetic (PJ_XYZ cart,  PJ *P) {
/*********************************************************************/
    double N, p, theta, c, s;
    PJ_LPZ lpz;

    /* Perpendicular distance from point to Z-axis (HM eq. 5-28) */
    p = hypot (cart.x, cart.y);

    /* HM eq. (5-37) */
    theta  =  atan2 (cart.z * P->a,  p * P->b);

    /* HM eq. (5-36) (from BB, 1976) */
    c  =  cos(theta);
    s  =  sin(theta);
    lpz.phi  =  atan2 (cart.z + P->e2s*P->b*s*s*s,  p - P->es*P->a*c*c*c);
    if( fabs(lpz.phi) > M_HALFPI ) {
        // this happen on non-sphere ellipsoid when x,y,z is very close to 0
        // there is no single solution to the cart->geodetic conversion in
        // that case, so arbitrarily pickup phi = 0.
        lpz.phi = 0;
    }
    lpz.lam  =  atan2 (cart.y, cart.x);
    N        =  normal_radius_of_curvature (P->a, P->es, lpz.phi);


    c  =  cos(lpz.phi);
    if (fabs(c) < 1e-6) {
        /* poleward of 89.99994 deg, we avoid division by zero   */
        /* by computing the height as the cartesian z value      */
        /* minus the geocentric radius of the Earth at the given */
        /* latitude                                              */
        double r = geocentric_radius (P->a, P->b, lpz.phi);
        lpz.z = fabs (cart.z) - r;
    }
    else
        lpz.z =  p / c  -  N;

    return lpz;
}



/* In effect, 2 cartesian coordinates of a point on the ellipsoid. Rather pointless, but... */
static PJ_XY cart_forward (PJ_LP lp, PJ *P) {
    PJ_COORD point;
    point.lp = lp;
    point.lpz.z = 0;

    point.xyz = cartesian (point.lpz, P);
    return point.xy;
}

/* And the other way round. Still rather pointless, but... */
static PJ_LP cart_reverse (PJ_XY xy, PJ *P) {
    PJ_COORD point;
    point.xy = xy;
    point.xyz.z = 0;

    point.lpz = geodetic (point.xyz, P);
    return point.lp;
}



/*********************************************************************/
PJ *CONVERSION(cart,1) {
/*********************************************************************/
    P->fwd3d  =  cartesian;
    P->inv3d  =  geodetic;
    P->fwd    =  cart_forward;
    P->inv    =  cart_reverse;
    P->left   =  PJ_IO_UNITS_RADIANS;
    P->right  =  PJ_IO_UNITS_CARTESIAN;
    return P;
}
