/*
 * Copyright (c) 2022 Michael G. Katzmann
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*! \file bspline_ctrl.c
 * This file contains the functions to calculate control points from a list of
 * points for drawing bezier curves.
 *
 * Adapted from code by:
 *  @author Bernhard R. Fischer
 *  https://www.cypherpunk.at/2015/12/calculating-control-points-for-bsplines/
 *  @version 2015/11/30
 *
 */

#include <cairo/cairo.h>
#include <glib-2.0/glib.h>
#include <hp8753.h>
#include <math.h>

// This factor defines the "curviness". Play with it!
#define CURVE_F 0.25

/*! This function calculates the angle of line in respect to the coordinate
 * system.
 * @param g Pointer to a line.
 * @return Returns the angle in radians.
 */
double
angle(const tLine *g)
{
   return atan2(g->B.i - g->A.i, g->B.r - g->A.r);
}


/*! This function calculates the control points. It takes two lines g and l as
 * arguments but it takes three lines into account for calculation. This is
 * line g (P0/P1), line h (P1/P2), and line l (P2/P3). The control points being
 * calculated are actually those for the middle line h, this is from P1 to P2.
 * Line g is the predecessor and line l the successor of line h.
 * @param g Pointer to first line.
 * @param l Pointer to third line (the second line is connecting g and l).
 * @param p1 Pointer to memory of first control point. This will receive the
 * coordinates.
 * @param p2 Pointer to memory of second control point.
 */
void
bezierControlPoints(const tLine *g, const tLine *l, tComplex *p1, tComplex *p2)
{
   tLine h;
   double f = CURVE_F;
   double lgt, a;

   // calculate length of line (P1/P2)
   lgt = sqrt(pow(g->B.r - l->A.r, 2) + pow(g->B.i - l->A.i, 2));

   // end point of 1st tangent
   h.B = l->A;
   // start point of tangent at same distance as end point along 'g'
   h.A.r = g->B.r - lgt * cos(angle(g));
   h.A.i = g->B.i - lgt * sin(angle(g));

   // angle of tangent
   a = angle(&h);
   // 1st control point on tangent at distance 'lgt * f'
   p1->r = g->B.r + lgt * cos(a) * f;
   p1->i = g->B.i + lgt * sin(a) * f;

   // start point of 2nd tangent
   h.A = g->B;
   // end point of tangent at same distance as start point along 'l'
   h.B.r = l->A.r + lgt * cos(angle(l));
   h.B.i = l->A.i + lgt * sin(angle(l));

   // angle of tangent
   a = angle(&h);
   // 2nd control point on tangent at distance 'lgt * f'
   p2->r = l->A.r - lgt * cos(a) * f;
   p2->i = l->A.i - lgt * sin(a) * f;
}


void
drawBezierSpline(cairo_t *ctx, const tComplex *pt, gint cnt)
{
   // Helping variables for lines.
   tLine g, l;
   // Variables for control points.
   tComplex c1, c2;

   // Draw bezier curve through all points.
   cairo_move_to(ctx, pt[0].r, pt[0].i);
   for (int i = 1; i < cnt; i++)
   {
      g.A = pt[(i + cnt - 2) % cnt];
      g.B = pt[(i + cnt - 1) % cnt];
      l.A = pt[(i + cnt + 0) % cnt];
      l.B = pt[(i + cnt + 1) % cnt];

      // Calculate controls points for points pt[i-1] and pt[i].
      bezierControlPoints(&g, &l, &c1, &c2);

      // Handle special case because points are not connected in a loop.
      if (i == 1) c1 = g.B;
      if (i == cnt - 1) c2 = l.A;

      // Create Cairo curve path.
      cairo_curve_to(ctx, c1.r, c1.i, c2.r, c2.i, pt[i].r, pt[i].i);
   }
   // Actually draw curve.
   cairo_stroke(ctx);
}
/*
static tComplex
pointInLine(tComplex A, tComplex B, gdouble T) {
	tComplex C;
 	 C.r = A.r - ((A.r - B.r) * T);
 	 C.i = A.i - ((A.i - B.i) * T);
 return C;
}
*/
static tComplex
pointInLine(tLine line, gdouble frac) {
	tComplex C;
 	 C.r = line.A.r - ((line.A.r - line.B.r) * frac);
 	 C.i = line.A.i - ((line.A.i - line.B.i) * frac);
 return C;
}

tComplex
bezierInterpolate(
    tComplex pt0,  tComplex pt1, 
    tComplex ctl0, tComplex ctl1, gdouble fr ) {

    tComplex point1, point2, point3, point4, point5;
    tLine lineP0C0, lineC0C1, lineC1P1, line4, line5, line6;

    // see https://en.wikipedia.org/wiki/File:B%C3%A9zier_3_big.gif

    lineP0C0.A = pt0;
    lineP0C0.B = ctl0;
    point1 = pointInLine(lineP0C0, fr);
    lineC0C1.A = ctl0;
    lineC0C1.B = ctl1;
    point2 = pointInLine(lineC0C1, fr);
    lineC1P1.A = ctl1;
    lineC1P1.B = pt1;
    point3 = pointInLine(lineC1P1, fr);

    line4.A = point1;
    line4.B = point2;
    point4 = pointInLine(line4, fr);
    line5.A = point2;
    line5.B = point3;
    point5 = pointInLine(line5, fr);
    
    line6.A = point4;
    line6.B = point5;
    return pointInLine(line6, fr);
}

// given an array of n complex points, return the interpolated value
// for non-integer position
//
gint
splineInterpolate( gint npoints, tComplex curve[], double samplePoint, tComplex *result ) {

	tLine g, l;
	tComplex c1, c2;
	gdouble dummy;

	// We first construct the cubic bezier curve for points around
	// our sample point (we actually need two below and two above),
	// then interpolate that curve for the value

	// if insufficient number of points just do linear interpolation
    if( npoints < 4 ) {
    }

    // we find two points below and two points above sample
	gint n = (gint) ceil( samplePoint );

	// if ceil is 0, then the sample must have been 0.0 (existing sample)
	// sample should not have been less than 0 but if it is return something
	// that won't break things
	if( n <= 0 ) {
		*result = curve[0];
		return( TRUE );
	} else if( n >= npoints) {
		// this shouldn't happen
		*result = curve[npoints-1];
		return( FALSE );
	}

    g.A = curve[(n + npoints - 2) % npoints];
    g.B = curve[(n + npoints - 1) % npoints];
    l.A = curve[(n + npoints + 0) % npoints];
    l.B = curve[(n + npoints + 1) % npoints];

    // Calculate controls points for points pt[i-1] and pt[i].
    bezierControlPoints(&g, &l, &c1, &c2);
    // Fix control points at the curve ends because points are not connected in a loop
    if (n == 1)
    	c1 = g.B;
    if (n == npoints - 1)
    	c2 = l.A;

    // now interpolate the bspline to find the point corresponding to sample
    *result = bezierInterpolate( curve[n-1], curve[n], c1, c2, modf( samplePoint, &dummy ) );
    return TRUE;
}

tColor hsv2rgb(tColor HSV)
{
    tColor RGB;
    double H = HSV.HSV.h, S = HSV.HSV.s, V = HSV.HSV.v,
            P, Q, T, fract;

    (H == 360.0)?(H = 0.0):(H /= 60.0);
    fract = H - floor(H);

    P = V*(1.0 - S);
    Q = V*(1.0 - S*fract);
    T = V*(1.0 - S*(1. - fract));

    if      (0.0 <= H && H < 1.0)
        RGB = (tColor){{.r = V, .g = T, .b = P}};
    else if (1.0 <= H && H < 2.0)
        RGB = (tColor){{.r = Q, .g = V, .b = P}};
    else if (2.0 <= H && H < 3.0)
        RGB = (tColor){{.r = P, .g = V, .b = T}};
    else if (3.0 <= H && H < 4.0)
        RGB = (tColor){{.r = P, .g = Q, .b = V}};
    else if (4.0 <= H && H < 5.0)
        RGB = (tColor){{.r = T, .g = P, .b = V}};
    else if (5.0 <= H && H < 6.0)
        RGB = (tColor){{.r = V, .g = P, .b = Q}};
    else
        RGB = (tColor){{.r = 0.0, .g = 0.0, .b = 0.0}};

    return RGB;
}

gint
getTimeStamp( gchar **psDateTime ) {
	g_free( *psDateTime );
	GDateTime *dt = g_date_time_new_now_local();
	*psDateTime = g_date_time_format(dt, "%e %b %Y %H:%M:%S");
	g_date_time_unref(dt);

	return 0;
}

gchar *
doubleToStringWithSpaces( gdouble value, gchar *sUnits ) {
	GString *strFreq = g_string_new(NULL);
	gint iDB, afterDP, beforeDP;
	g_string_printf ( strFreq, "%.6f", value );
	iDB = strFreq->len - (6 + 1);	// this must be where the DP is initially
	beforeDP = iDB;
	afterDP = strFreq->len - (iDB + 1);
	for( int i = iDB+1+3; afterDP > 3; afterDP -= 3, i += 4 ) {
		g_string_insert_c( strFreq, i, ' ' );
	}
	for( int i = iDB-3; beforeDP > 3; beforeDP -= 3, i -= 3 ) {
		g_string_insert_c( strFreq, i, ' ' );
	}
	if( sUnits ) {
		g_string_append_c( strFreq, ' ' );
		g_string_append( strFreq, sUnits );
	}
	return( g_string_free( strFreq, FALSE ));
}
