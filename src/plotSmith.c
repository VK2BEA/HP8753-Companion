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

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <cairo/cairo.h>
#include <glib-2.0/glib.h>
#include <hp8753.h>
#include <GTKplot.h>
#include <math.h>

/*!     \brief  Search for stimulus sample corresponding to the frequency (list segments)
 *
 * When the stimulus sweep is discontinuous (as with list segments), the sample number cannot be
 * directly determined from the stimulus and the start and end stimulus values. We must search
 * for the stimulus value in each segment.
 *
 * \ingroup drawing
 *
 * \param nSegmentStart        first sample in the segment
 * \param nEnd                 last sample in the segment
 * \param pChannel             which channel
 * \param stimulusTarget       the stimulus we are looking for within the segment
 * \param pSampleIndex         pointer to return the index to the sample corresponding to the stimulus target
 * \param sampleIndexFraction  pointer to return the fraction of the index to the sample corresponding to the stimulus target
 * \return			0
 */
gboolean
searchForStimulusValueInSegment( gint nSegmentStart, gint nSegmentEnd, tChannel *pChannel,
		                gdouble stimulusTarget, gdouble *pSamplePoint ) {
    gint nHead = nSegmentStart, nTail = nSegmentEnd;
    gint nMid = (nHead + nTail) / 2;

    while ( nHead <= nTail )  {
        if ( pChannel->stimulusPoints[nMid] < stimulusTarget ) {
        	nHead = nMid + 1;
        } else if ( pChannel->stimulusPoints[nMid] == stimulusTarget ) {
        	*pSamplePoint = (double)nMid;
        	return TRUE;
        } else {
        	nTail = nMid - 1;
        }
        nMid = (nHead + nTail)/2;
    }

    if( nHead > nTail ) {
		*pSamplePoint = (gdouble)nMid + (stimulusTarget - pChannel->stimulusPoints[nMid]) /
    			(pChannel->stimulusPoints[nMid+1] - pChannel->stimulusPoints[nMid]);
    	return TRUE;
    } else {
    	return FALSE;
    }
}

#define RtoD(x) ((x) * 180 / G_PI)
/*!     \brief  Angle where two circles intersect
 *
 * Determine where two circles intersect
 * This is needed to determine the angle that the legend for reactive impeadance has
 * on Smith charts (intersection of reactive line and outer circle)
 *
 * \ingroup drawing
 *
 * \param x1		center of circle 1
 * \param y1		center of circle 1
 * \param r1		radius of circle 1
 * \param x2		center of circle 2
 * \param y2		center of circle 2
 * \param r2		radius of circle 2
 * \param angleIntercept1	pointer to douple to receive first angle of intercept
 * \param angleIntercept2	pointer to douple to receive second angle of intercept
 * \return			0
 */
int
angleOfCircleIntersects( double x1, double y1, double r1, double x2, double y2, double r2,
		double *angleIntercept1, double *angleIntercept2 ) {
	double d, l, h;
	double intersect1x, intersect1y, intersect2x, intersect2y;

	d = sqrt( SQU( x1 - x2 ) + SQU( y1 - y2 ) );

	// if the circles do not intersect
	if( d + r1 < r2 || d + r2 < r1 || (r1 + r2 < d) )
		return ERROR;

	l = ( SQU( r1 ) - SQU( r2 ) + SQU( d ) ) / (2.0 * d);
	h = sqrt( SQU( r1 ) - SQU( l ) );

	intersect1x = ( l / d ) * ( x2 - x1 ) + ( h / d ) * ( y2 - y1 ) + x1;
	intersect2x = ( l / d ) * ( x2 - x1 ) - ( h / d ) * ( y2 - y1 ) + x1;

	intersect1y = ( l / d ) * ( y2 - y1 ) - ( h / d ) * ( x2 - x1 ) + y1;
	intersect2y = ( l / d ) * ( y2 - y1 ) + ( h / d ) * ( x2 - x1 ) + y1;

	*angleIntercept1 = atan2( intersect1y - y1, intersect1x - x1 );
	*angleIntercept2 = atan2( intersect2y - y1, intersect2x - x1 );

	return 0;
}

/*!     \brief  Display the Smith chart grid
 *
 * If the plot is in Smith chart form, draw the grid and legends.
 * If there is an overlay of two Smith grids of the same scale show both,
 * otherwise actual grid is only drawn once.
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param pGrid		pointer to grid parameters
 * \pparam channel	which channel
 * \param pGlobal	pointer to global data
 * \return			TRUE
 *
 */
gboolean
plotSmithGrid (cairo_t *cr, gboolean bAnnotate, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal)
{
	gchar label[ BUFFER_SIZE_20 ];
    double centerX, centerY, radiusInitial, gammaScale = 1.0;
    GtkStyleContext  __attribute__((unused)) *context;
	gdouble iptr;
	gboolean bAdmitance = pGlobal->HP8753.channels[channel].chFlags.bAdmitanceSmith || pGlobal->flags.bAdmitanceSmith;

	double Xcircles[] = {5.0, 2.0, 1.0, 0.5, 0.2};
    double Rcircles[] = {
    		 5.00,  2.00,  1.00,  0.50,  0.20, 	// within the normal smith chart
			 0.0, 					   			// The outer ring of the conventional smith chart
			-0.25, -0.40, -0.50, -0.60, -0.80, 	// negative resistance to the left of the plane ( < -1.0)
    		-1.20, -1.50, -1.70,    			// negative resistance to the right of the plane
			-2.00, -2.20, -2.50, -3.00, -4.00, -7.00
    };



	// gamma for full scale
	if( pGlobal->HP8753.channels[channel].scaleVal != 0.0 )
		gammaScale = pGlobal->HP8753.channels[channel].scaleVal;

    // gtk_render_background(context, cr, 0, 0, width, height);
	cairo_save( cr );
	{
		showStimulusInformation (cr, pGrid, channel, pGlobal);
		cairo_new_path( cr );
		//The origin is now in the middle of the gamma (reflection) circle
		cairo_translate(cr, pGrid->leftGridPosn + pGrid->gridWidth/2.0, pGrid->bottomGridPosn + pGrid->gridHeight/2.0);

		radiusInitial = MIN (pGrid->gridHeight, pGrid->gridWidth) / 2.0;
		pGrid->scale = radiusInitial/gammaScale;

		centerX = 0.0;
		centerY = 0.0;

		// scale so that new radius = 1.0 gives the normal
		// smith chart size
		cairo_scale( cr, pGrid->scale, pGrid->scale );

#define SMITH_LINE_THICKNESS	0.002
		// draw outer circle
		cairo_set_line_width (cr, SMITH_LINE_THICKNESS * gammaScale);
		cairo_arc (cr, 0,0, 1.0, 0, 2 * G_PI);

        if( pGrid->overlay.bSmithWithDiferentScaling && channel == eCH_TWO ) {
            gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorGridPolarOverlay   ] );
        } else {
            gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorGrid   ] );
        }

		cairo_new_path(cr);
    	cairo_arc(cr, centerX, centerY, UNIT_CIRCLE * gammaScale, 0, 2 * G_PI);
    	cairo_stroke (cr);
		cairo_arc(cr, centerX, centerY, UNIT_CIRCLE * gammaScale, 0, 2 * G_PI);
		cairo_clip(cr);
		// resistance circles

		for( int i=0; i < sizeof( Rcircles)/sizeof(double); i++ ) {
			double radius, x;
			// r circles
			// radius is 1/(r+1), center is r/(r+1), 0
			radius = 1.0 / (Rcircles[i] + 1.0) * UNIT_CIRCLE;
			x = Rcircles[i] / (Rcircles[i] + 1.0);
			if( bAdmitance )
				x = -x;
#ifdef FIFTY_OHM_GREEN
			if( Rcircles[i] == 1.0 ) {
#else
			if( FALSE ) {
#endif
				cairo_stroke( cr );
				cairo_save( cr );
				cairo_set_line_width (cr, SMITH_LINE_THICKNESS * 2 * gammaScale);
				setCairoColor (cr, COLOR_50ohm_SMITH );
				cairo_arc (cr, x, centerY, fabs(radius), 0, 2 * G_PI);
				cairo_stroke( cr );
				cairo_restore( cr );

			} else {
				cairo_arc (cr, x, centerY, fabs(radius), 0, 2 * G_PI);
				cairo_new_sub_path ( cr );
			}
		}

		cairo_move_to( cr, centerX - UNIT_CIRCLE * gammaScale, centerY );
		cairo_line_to( cr, centerX + UNIT_CIRCLE * gammaScale, centerY );
		cairo_move_to( cr, 1.0, centerY - UNIT_CIRCLE * gammaScale );
		cairo_line_to( cr, 1.0, centerY + UNIT_CIRCLE * gammaScale );
		cairo_stroke (cr);
    // reactance curves
	//  gdk_cairo_set_source_rgba (cr, &brown );
    	// x circles
    	// radius is 1/x, center is 1.0, +/- 1.0/x
		for( int i=0; i < sizeof( Xcircles)/sizeof(double); i++ ) {
			double reactance, x = 1.0, y;
			reactance = 1.0 / Xcircles[i];
			y = 1.0 / Xcircles[i];
			if( bAdmitance )
				x = -1.0;
			cairo_arc (cr, x, y, reactance,  0.0, G_PI*2.0);
			cairo_new_sub_path ( cr );
			cairo_arc (cr, x, -y, reactance, 0.0, G_PI*2.0);
			cairo_new_sub_path ( cr );
		}
		cairo_stroke (cr);

		if( bAnnotate ) {
			// labels
			// resistance first
			cairo_reset_clip( cr );
			gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorSmithGridAnnotations ] );
			setCairoFontSize(cr, pGrid->fontSize * 0.75 / pGrid->scale);

			double lastRad = 1000.0;
			for( int i=0; i < sizeof( Rcircles)/sizeof(double); i++ ) {
				double radius, x;
				// r circles
				// radius is 1/(r+1), center is r/(r+1), 0
				radius = 1.0 / (Rcircles[i] + 1.0) * UNIT_CIRCLE;
				x = Rcircles[i] / (Rcircles[i] + 1.0);
				if( !bAdmitance ) {
					g_snprintf(label, BUFFER_SIZE_20,
							modf( round(Rcircles[i] * 50), &iptr ) > 0.01 ? "%.1f" : "%.0f",
									Rcircles[i] * 50.0);
				} else {
					if( Rcircles[i] == 0.0 )
						label[0] = 0;
					else
						g_snprintf(label, BUFFER_SIZE_20,
								modf( round(Rcircles[i] * 50), &iptr ) > 0.01 ? "%.1fm" : "%.0fm",
										1000.0 / (Rcircles[i] * 50.0));
				}
				if( fabs( x-radius ) < 1.1 * (UNIT_CIRCLE * gammaScale) ) // && (Rcircles[i] != 0.0 && Rcircles[i] != 1.0) )
					if ( fabs(lastRad - radius) > (UNIT_CIRCLE * gammaScale) / 15.0 ){
						lastRad = radius;
						centreJustifiedCairoTextWithClear( cr, label, x - radius, centerY );
					}
			}

			// reactance next
			for( int i=0; i < sizeof( Xcircles)/sizeof(double); i++ ) {
				double reactance, y, angleOfIntersection1, angleOfIntersection2;
				reactance = 1.0 / Xcircles[i];
				y = 1.0 / Xcircles[i];

				if( angleOfCircleIntersects(
						centerX, centerY, UNIT_CIRCLE * gammaScale,
						bAdmitance ? -1.0 : 1.0, y, reactance,
						&angleOfIntersection1, &angleOfIntersection2 ) == 0) {

					if( !bAdmitance )
						g_snprintf(label, BUFFER_SIZE_20,
								modf( round(Xcircles[i] * 50.0), &iptr ) > 0.01 ? "-j%.1f" : "-j%.0f",
										Xcircles[i] * 50.0);
					else
						g_snprintf(label, BUFFER_SIZE_20,
								modf( round(Xcircles[i] * 50.0), &iptr ) > 0.01 ? "-j%.1fm" : "-j%.0fm",
										1000.0 * Xcircles[i] / 50.0);
					cairo_save( cr );
					if ( fabs( angleOfIntersection1 ) > 0.1 ) {
						cairo_rotate( cr, angleOfIntersection1 - G_PI/2.0);
						centreJustifiedCairoTextWithClear( cr, label+1, 0.0, UNIT_CIRCLE * gammaScale);
						cairo_rotate( cr, -2.0 * angleOfIntersection1);
						centreJustifiedCairoTextWithClear( cr, label, 0.0, UNIT_CIRCLE * gammaScale);
					}
					if ( !bAdmitance || fabs( angleOfIntersection2 ) < G_PI - 0.1 ) {
						cairo_restore( cr ); cairo_save( cr );
						cairo_rotate( cr, angleOfIntersection2 - G_PI/2.0);
						centreJustifiedCairoTextWithClear( cr, label+1, 0.0, UNIT_CIRCLE * gammaScale);
						cairo_rotate( cr, -2.0 * angleOfIntersection2);
						centreJustifiedCairoTextWithClear( cr, label, 0.0, UNIT_CIRCLE * gammaScale);
					}
					cairo_restore( cr );
				}
			}
		}
	}
	cairo_restore( cr );
	pGrid->scale = 1.0;
	return TRUE;
}


/*!     \brief  Render a text string right justified from the specified point
 *
 * Show the string on the widget right justified
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param pGrid		pointer to grid settings
 * \param channel	channel of which the information is to be shown
 * \param pGlobal	pouinter to global data
 * \param gammaReal	the x position in gamma space of the point to display
 * \param imagReal	the y position in gamma space of the point to display
 * \param frequency	the frequency at which the sample was made
 *
 * \return	TRUE on sucess
 */
static gboolean
showSmithCursorInfo(
		cairo_t *cr, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal,
		gdouble gammaReal, gdouble gammaImag, gdouble frequency )
{
#define SPACELEFT 40

	gdouble VSWR;
	gdouble gammaMag, gammaAngle, returnLoss;
	gdouble xTextPos, yTextPos, CapacitanceOrInductance;
	gdouble r, x, g, b;
	gchar *label;
	gchar sValue[ BUFFER_SIZE_100 ], *sPrefix="", *sUnit;

	gdouble CWfrequency = pGlobal->HP8753.channels[ channel ].CWfrequency;
	gboolean bUseCWfrequncy = pGlobal->HP8753.channels[ channel ].sweepType == eSWP_CWTIME
	        || pGlobal->HP8753.channels[ channel ].sweepType == eSWP_PWR;

	gammaMag = sqrt( SQU( gammaReal ) + SQU( gammaImag ) );
	returnLoss = -20.0 * log10( gammaMag );
	VSWR = (1+gammaMag)/(1-gammaMag);

	gammaAngle = 180.0 * atan2( gammaImag, gammaReal) / G_PI;

	r = (1.0 - SQU( gammaReal ) - SQU( gammaImag )) / (SQU( 1.0 - gammaReal ) + SQU( gammaImag ));
	x = (2.0 * gammaImag ) / (SQU( 1 - gammaReal ) + SQU( gammaImag ));

	g = (1.0 - SQU( gammaReal ) - SQU( gammaImag )) / (1.0 + SQU( gammaReal ) + (2.0 * gammaReal ) + SQU( gammaImag ));
	b = (-2.0 * gammaImag ) / (1.0 + SQU( gammaReal ) + (2.0 * gammaReal ) + SQU( gammaImag ));

	// We use this font because it has the relevant Unicode glyphs for gamma and degrees
	cairo_select_font_face(cr, CURSOR_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

	xTextPos = pGrid->areaWidth * 0.05 + pGrid->leftGridPosn;

	if( !pGrid->overlay.bAny ) {
		yTextPos = pGrid->bottomGridPosn * 1.1;
	} else {
		if ( channel == eCH_ONE ) {
			yTextPos = pGrid->gridHeight + pGrid->bottomGridPosn - 9 * pGrid->fontSize;
		} else {
			yTextPos = pGrid->bottomGridPosn - 0.7 * pGrid->fontSize ;
		}
		if( gridType[ pGlobal->HP8753.channels[ (channel + 1) % eNUM_CH ].format ] != eGridCartesian )
			xTextPos -= pGrid->areaWidth * 0.04;
	}

	setTraceColor( cr, pGrid->overlay.bAny, channel );

	if( x < 0 ) {
		if( !pGlobal->flags.bAdmitanceSmith )
			CapacitanceOrInductance = 1.0 / ((-x * Z0) * 2.0 * G_PI * (bUseCWfrequncy ? CWfrequency : frequency));
		else
			CapacitanceOrInductance = (b / Z0) / (2.0 * G_PI * (bUseCWfrequncy ? CWfrequency : frequency));
		sUnit = "F";
	} else {
		if( !pGlobal->flags.bAdmitanceSmith )
			CapacitanceOrInductance = (x * Z0) / (2.0 * G_PI * (bUseCWfrequncy ? CWfrequency : frequency));
		else
			CapacitanceOrInductance = 1.0 / ((-b / Z0) * (2.0 * G_PI * (bUseCWfrequncy ? CWfrequency : frequency)));
		sUnit = "H";
	}
	label = engNotation( CapacitanceOrInductance, 2, eENG_SEPARATE, &sPrefix );
	if( !pGlobal->flags.bAdmitanceSmith )
		g_snprintf( sValue, BUFFER_SIZE_100, " %.2f Ω + %s %s%s", r * Z0, label, sPrefix, sUnit);
	else
		g_snprintf( sValue, BUFFER_SIZE_100, " %.2f Ω ∥ %s %s%s", Z0 / g, label, sPrefix, sUnit);
	filmCreditsCairoText( cr, "", sValue, 0,  xTextPos,  yTextPos, eBottomLeft );
	g_free( label );

	if( pGlobal->flags.bAdmitanceSmith )
		label = g_strdup_printf( x >= 0.0 ? " %.2f mS + j %.2f mS" : " %.2f mS - j %.2f mS", g * 1000.0 / Z0, fabs(b * 1000.0 / Z0));
	else
		label = g_strdup_printf( x >= 0.0 ? " %.2f + j %.2f Ω" : " %.2f - j %.2f Ω", r * Z0, fabs(x * Z0));
	filmCreditsCairoText( cr, pGlobal->flags.bAdmitanceSmith ? "Y =" : "Z =", label, 1,  xTextPos,  yTextPos, eBottomLeft );
	g_free( label );

	label = g_strdup_printf(" %4.3f ∠ %5.3f°", gammaMag, gammaAngle);
	filmCreditsCairoText( cr, "Γ =", label, 2,  xTextPos,  yTextPos, eBottomLeft );
	g_free( label );

	label = g_strdup_printf(" %.3f", VSWR);
	filmCreditsCairoText( cr, "SWR =", label, 3,  xTextPos, yTextPos, eBottomLeft );
	g_free( label );

	label = g_strdup_printf(" %.2f dB", returnLoss);
	filmCreditsCairoText( cr, "RL =", label, 4,  xTextPos,  yTextPos, eBottomLeft );
	g_free( label );

    label = engNotation( frequency, 2, eENG_SEPARATE, &sPrefix );

	switch ( pGlobal->HP8753.channels[ channel ].sweepType ) {
	case eSWP_LINFREQ:
	case eSWP_LOGFREQ:
	case eSWP_LSTFREQ:
	default:
	    g_snprintf( sValue, BUFFER_SIZE_100, " %s %sHz", label, sPrefix);
	    filmCreditsCairoText( cr, "Freq =", sValue, 5,  xTextPos,  yTextPos, eBottomLeft );
	    break;
	case eSWP_CWTIME:
        g_snprintf( sValue, BUFFER_SIZE_100, " %s %ss", label, sPrefix);
        filmCreditsCairoText( cr, "Time =", sValue, 5,  xTextPos,  yTextPos, eBottomLeft );
	    break;
	case eSWP_PWR:
        g_snprintf( sValue, BUFFER_SIZE_100, " %s %sdBm", label, sPrefix);
        filmCreditsCairoText( cr, "Power =", sValue, 5,  xTextPos,  yTextPos, eBottomLeft );
	    break;
	}


	g_free( label );

    return TRUE;
}

/*!     \brief  Display the trace on the polar or Smith grid
 *
 * Plot the trace as a series of connected lines or Bezier curves.
 * If the sweep is a series of segments, then these are traced independently.
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param pGrid		pointer to grid parameters
 * \pparam channel	which channel
 * \param pGlobal	pointer to global data
 * \return			TRUE
 *
 */
gboolean
plotSmithAndPolarTrace (cairo_t *cr, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal)
{
	gdouble xu, xl, yl, yu, xMouse, sweepValue, xFract, samplePoint;
	gdouble logFreqStart, logFreqStop;
	gdouble gammaReal, gammaImag, gammaScale = 1.0, frequency;
	gdouble centerX, centerY, radiusInitial;
	gint 	i;

	gboolean bValidSample = FALSE;

	tChannel *pChannel = &pGlobal->HP8753.channels[channel];
    GdkRGBA solidCursorRGBA = plotElementColors[ eColorLiveMkrCursor ];
    solidCursorRGBA.alpha = 1.0;

	// gamma for full scale

	if( pChannel->scaleVal != 0.0 )
		gammaScale = pChannel->scaleVal;

	cairo_save( cr );
	{
		cairo_new_path( cr );
		// Translate coordinate system so that 0,0 is in the center of the Smith chart (gamma 0)
		// Scale for 1.0 being the gamma scale (1.0 for usual smith chart view)
		cairo_translate(cr, pGrid->leftGridPosn + pGrid->gridWidth/2.0, pGrid->bottomGridPosn + pGrid->gridHeight/2.0);
		radiusInitial = MIN (pGrid->gridHeight, pGrid->gridWidth) / 2.0;
		pGrid->scale = radiusInitial/gammaScale;
		cairo_scale( cr, pGrid->scale, pGrid->scale );
		centerX = 0.0;
		centerY = 0.0;
#define CLIP_MARGIN 1.1
		// clip to the outer circle
		cairo_arc(cr, centerX, centerY, UNIT_CIRCLE * gammaScale * CLIP_MARGIN, 0, 2 * G_PI);
		cairo_clip(cr);

		gint npoints = pChannel->nPoints;
		if( npoints ) {

			setTraceColor( cr, pGrid->overlay.bAny, channel );
			cairo_set_line_width (cr, SMITH_LINE_THICKNESS * 1.25 * gammaScale);	// we have already scaled (1 is the size of the outer circle)

			// Draw markers (if there are any)
			drawMarkers( cr, pGlobal, pGrid, channel, 0.0, 1.0 );

			// Draw trace
			// Use Bezier splines to give better interpolation
			if( pGlobal->flags.bSmithSpline ) {
				// If we are using list sweep with all segments, then plot each segment
				// separately, otherwise plot one one curve
				if( pChannel->sweepType == eSWP_LSTFREQ && pChannel->chFlags.bAllSegments ) {
					// Draw trace for sweep type list frequency (all segments)
					for ( int seg=0, startPoint=0; seg < pChannel->nSegments; seg++ ) {
						drawBezierSpline(cr, &pChannel->responsePoints[ startPoint ],
								pChannel->segments[ seg ].nPoints);
						startPoint += pChannel->segments[ seg ].nPoints;
					}
				} else {
					// Draw trace for all sweep types except list frequency (all segments)
					drawBezierSpline(cr, pChannel->responsePoints, npoints);
				}
			} else {
				// linear interpolation
				if( pChannel->sweepType == eSWP_LSTFREQ && pChannel->chFlags.bAllSegments ) {
					// Draw trace for sweep type list frequency (all segments)
					for ( int seg=0, startPoint=0; seg < pChannel->nSegments; seg++ ) {
						cairo_new_path( cr );
						for ( int i=startPoint; i < startPoint+pChannel->segments[ seg ].nPoints; i++ ) {
							gammaReal = pChannel->responsePoints[i].r;
							gammaImag = pChannel->responsePoints[i].i;
							if ( i == 0 )
								cairo_move_to(cr, gammaReal, gammaImag);
							else
								cairo_line_to(cr, gammaReal, gammaImag);
						}
						cairo_stroke (cr);
						startPoint += pChannel->segments[ seg ].nPoints;
					}
				} else {
					// Draw trace for all sweep types except list frequency (all segments)
					for ( int i=0; i < npoints; i++ ) {
						gammaReal = pChannel->responsePoints[i].r;
						gammaImag = pChannel->responsePoints[i].i;
						if ( i == 0 )
							cairo_move_to(cr, gammaReal, gammaImag);
						else
							cairo_line_to(cr, gammaReal, gammaImag);
					}
					cairo_stroke (cr);
				}
			}

			// If the mouse cursor has an X co-ordinate that is between the start and stop stimulus
		    // on the stimulus legend, then highlight the corresponding response point on the trace

			if( pGrid->overlay.bAny )
				xMouse = pGlobal->mousePosition[eCH_ONE].r;
			else
				xMouse = pGlobal->mousePosition[channel].r;

            if( pGlobal->flags.bHoldLiveMarker )
                xMouse = pGlobal->mouseXpercentHeld * pGrid->areaWidth;

			if ( xMouse >= pGrid->leftGridPosn && xMouse <= pGrid->gridWidth+pGrid->leftGridPosn ) {
				xFract = (xMouse-pGrid->leftGridPosn) / pGrid->gridWidth;
				cairo_reset_clip( cr);
				// find out what sample corresponds to the x mouse position
				// This is straightforward unless we are using the list frequency sweep with all segments

				// find the point on the trace (gammaReal, gammaImag) corresponding to
				// the frequency represented by the cursor position on the screen
				if( pChannel->sweepType == eSWP_LSTFREQ && pChannel->chFlags.bAllSegments ) {
					// Determine what frequency the X coordinate of the mouse cursor corresponds to
					sweepValue = LIN_INTERP(pChannel->sweepStart, pChannel->sweepStop, xFract);
					// find what segment contains the frequency (if any)
					for( int seg=0, segStartSample=0; seg < pChannel->nSegments; seg++ ) {
						// see if we even have this sample frequency
						if( sweepValue >= pChannel->segments[seg].startFreq && sweepValue <= pChannel->segments[seg].stopFreq ) {
							// the frequency is in this sample.. find out where
							searchForStimulusValueInSegment( segStartSample, segStartSample+pChannel->segments[seg].nPoints,
									pChannel, sweepValue, &samplePoint );

							if ( pGlobal->flags.bSmithSpline ){
								tComplex result;
								// interpolate within this segment
								splineInterpolate( pChannel->segments[seg].nPoints,
											&(pChannel->responsePoints[segStartSample]), samplePoint-segStartSample, &result );
								gammaReal = result.r; gammaImag = result.i;
							} else {
								gint sampleLow, sampleHigh;
								sampleLow = (gint)floor(samplePoint); sampleHigh = (gint)ceil(samplePoint);
								xl = pChannel->responsePoints[sampleLow].r;
								xu = pChannel->responsePoints[sampleHigh].r;
								gammaReal = LIN_INTERP( xl, xu, (samplePoint-sampleLow));
								yl = pChannel->responsePoints[sampleLow].i;
								yu = pChannel->responsePoints[sampleHigh].i;
								gammaImag = LIN_INTERP( yl, yu, (samplePoint-sampleLow));
							}
							bValidSample = TRUE;
							break;
						}
						segStartSample += pChannel->segments[seg].nPoints;
					}
				} else {
					// all sweep formats other than list frequency (all segments)
					samplePoint = (npoints-1) * xFract;
					if ( pGlobal->flags.bSmithSpline ){
						tComplex result;
						splineInterpolate( npoints, pGlobal->HP8753.channels[channel].responsePoints, samplePoint, &result );
						gammaReal = result.r; gammaImag = result.i;
					} else {
						gint sampleLow, sampleHigh;
						sampleLow = (gint)floor(samplePoint); sampleHigh = (gint)ceil(samplePoint);
						xl = pChannel->responsePoints[sampleLow].r;
						xu = pChannel->responsePoints[sampleHigh].r;
						gammaReal = LIN_INTERP( xl, xu, (samplePoint-sampleLow));
						yl = pChannel->responsePoints[sampleLow].i;
						yu = pChannel->responsePoints[sampleHigh].i;
						gammaImag = LIN_INTERP( yl, yu, (samplePoint-sampleLow));
					}
					bValidSample = TRUE;
				}

				// draw circle around response point on trace
				gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorLiveMkrCursor ] );
				cairo_set_line_width (cr, (pGrid->areaWidth / 1000.0 * 3.0) / pGrid->scale);
				cairo_new_path( cr );
				if (bValidSample) {
					cairo_arc( cr, gammaReal, gammaImag, UNIT_CIRCLE * gammaScale/50.0, 0.0, 2 * G_PI );
					cairo_stroke(cr);
	                gdk_cairo_set_source_rgba (cr, &solidCursorRGBA );
                    cairo_arc( cr, gammaReal, gammaImag, UNIT_CIRCLE * gammaScale/210.0, 0.0, 2 * G_PI );
                    cairo_fill(cr);
				}

				// return to the initial transform
				cairo_set_matrix (cr, &pGrid->initialMatrix);
				gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorLiveMkrFreqTicks ] );
				cairo_set_line_width (cr, 0.5);

				// draw frequency / seconds tick marks
				if( pGlobal->HP8753.channels[channel].sweepType == eSWP_LOGFREQ ) {
					gdouble logStartFreq, logStopFreq, logSpan, startOffset, intLog, xGrid;

					logStartFreq = log10( pChannel->sweepStart );
					logStopFreq = log10( pChannel->sweepStop );
					logSpan = logStopFreq - logStartFreq;
					startOffset = modf( logStartFreq, &intLog );
					// find the start grid
					for( i=1; i < NUM_LOG_GRIDS && logGrids[ i ] < startOffset; i++ );
					// i is now the index to logGrids of the next grid
					for( double decades = 0.0;; i++ ) {
						// We repeat the sequence of nine grids each decade
						if( i >= sizeof(logGrids)/sizeof(double) ) {
							i = 1;
							++decades;
						}
						// break when we do all the grids
						if( logGrids[ i ] - startOffset + decades > logStopFreq )
							break;
						else {
							xGrid = (logGrids[ i ] - startOffset + decades) / logSpan * pGrid->gridWidth;
							cairo_move_to(cr, pGrid->leftGridPosn + xGrid, pGrid->bottomGridPosn + (gdouble)pGrid->gridHeight/NVGRIDS/8.0);
							cairo_rel_line_to( cr, 0.0, -(gdouble)pGrid->gridHeight/NVGRIDS/4.0 );
							cairo_stroke( cr );
						}
					}
				} else {
					for (int i=0; i <= NHGRIDS; i++ ) {
						cairo_move_to( cr, pGrid->leftGridPosn + (pGrid->gridWidth / NHGRIDS * i),
								 pGrid->bottomGridPosn + (gdouble)pGrid->gridHeight/NVGRIDS/8.0 );
						cairo_rel_line_to( cr, 0.0, -(gdouble)pGrid->gridHeight/NVGRIDS/4.0 );
						cairo_stroke( cr );
					}
				}

				gdk_cairo_set_source_rgba (cr, &solidCursorRGBA );
				cairo_set_line_width (cr, pGrid->areaWidth / 1000.0 * 3.0);
				// the actual xMouse position must be rescaled and translated
				// because 0,0 is at the center of the smith chart
				cairo_move_to( cr, xMouse,
						pGrid->bottomGridPosn );
				cairo_rel_line_to( cr, 0, -(gdouble)pGrid->gridHeight/NVGRIDS/8 );
				cairo_stroke(cr);

				switch( pChannel->sweepType ) {
				case eSWP_LINFREQ:
                case eSWP_PWR:      // actually power sweep
                case eSWP_CWTIME:   // actually time sweep
				default:
					frequency = LIN_INTERP(pChannel->sweepStart,
							pChannel->sweepStop, xFract);
					break;
				case eSWP_LOGFREQ:
					logFreqStart = log10( pChannel->sweepStart );
					logFreqStop = log10( pChannel->sweepStop );
					frequency = pow( 10.0, logFreqStart + (logFreqStop-logFreqStart) * xFract );
					break;
				}
				setCairoFontSize(cr, pGrid->fontSize); // initially 10 pixels
				if( bValidSample ) {
					if( pChannel->format == eFMT_SMITH )
						showSmithCursorInfo( cr, pGrid, channel, pGlobal, gammaReal, gammaImag, frequency );
					else
						showPolarCursorInfo( cr, pGrid, channel, pGlobal, gammaReal, gammaImag, frequency );
				}
			}
		}

		if( channel == eCH_ONE || !pGlobal->HP8753.flags.bDualChannel )
			showTitleAndTime( cr, pGrid, pGlobal->HP8753.sTitle,
					pGlobal->flags.bShowDateTime ? pGlobal->HP8753.dateTime : "" );
	}
	cairo_restore( cr );
	pGrid->scale = 1.0;
	return TRUE;
}

