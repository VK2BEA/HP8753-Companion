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


/*!     \brief  Display the polar grid
 *
 * If the plot is polar, draw the grid and legends.
 * If there is an overlay of two polar grids of the same scale show both,
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
plotPolarGrid (cairo_t *cr, gboolean bAnnotate, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal)
{

	double centerX, centerY;
    double radiusInitial, gammaScale = 1.0;
    double rangeMultiplier;
    double secondRadius = 0.0;
	gchar label[ BUFFER_SIZE_20 ];

    int i;

    double range;

    // don't duplicate
    if( pGrid->overlay.bPolar && !pGrid->overlay.bPolarWithDiferentScaling && channel == eCH_TWO)
    	return FALSE;

    cairo_save( cr );
	{
		showStimulusInformation (cr, pGrid, channel, pGlobal);
		cairo_new_path( cr );
		//The origin is now in the middle of the polar circle
		cairo_translate(cr, pGrid->leftMargin + pGrid->gridWidth/2.0, pGrid->bottomMargin + pGrid->gridHeight/2.0);

		centerX = 0.0;
		centerY = 0.0;

		// gamma for full scale
		if( pGlobal->HP8753.channels[channel].scaleVal != 0.0 )
			gammaScale = pGlobal->HP8753.channels[channel].scaleVal;

		radiusInitial = MIN (pGrid->gridHeight, pGrid->gridWidth) / 2.0;
		pGrid->scale = radiusInitial/gammaScale;

		// scale so that new radius = 1.0 gives the normal
		// polar plot size
		cairo_scale( cr, pGrid->scale, pGrid->scale );
		cairo_set_line_width (cr, LINE_THICKNESS/pGrid->scale);

		if( pGrid->overlay.bAny && channel == eCH_TWO ) {
		    gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorGridPolarOverlay   ] );
		} else {
		    gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorGrid   ] );
		}
	#define ONEonSQRT2 0.70710678118

		cairo_arc (cr, centerX, centerY, UNIT_CIRCLE * gammaScale, 0, 2 * G_PI);
		cairo_stroke ( cr );

		modf(log( gammaScale )/log(2.0), &range);
		rangeMultiplier = pow(2.0, range);

		cairo_save( cr );
		double dashed[] = {0.04, 0.02};
		for( i=0; i < sizeof(dashed) / sizeof(double); i++)
			dashed[i] *= gammaScale;;
		cairo_set_dash(cr, dashed, sizeof(dashed) / sizeof(double), channel == eCH_ONE ? 0 : dashed[1] );

		for( i=1; UNIT_CIRCLE * i / 5 * rangeMultiplier < UNIT_CIRCLE * gammaScale; i++ ) {
			double radius = UNIT_CIRCLE * i / 5 * rangeMultiplier;
			cairo_arc (cr, centerX, centerY, radius, 0, 2 * G_PI);
			cairo_new_sub_path ( cr );
			if( i == 2 )
				secondRadius = radius;
		}
		cairo_stroke( cr );
		cairo_restore( cr );

		cairo_arc (cr, centerX, centerY, UNIT_CIRCLE, 0, 2 * G_PI);
		cairo_stroke (cr);

		double inner = secondRadius * ONEonSQRT2;
		double outerRel = (UNIT_CIRCLE  * gammaScale - secondRadius) * ONEonSQRT2;
		cairo_move_to( cr, centerX + inner, centerY + inner );
		cairo_rel_line_to( cr, outerRel, outerRel );
		cairo_move_to( cr, centerX - inner, centerY + inner );
		cairo_rel_line_to( cr, -outerRel, outerRel);
		cairo_move_to( cr, centerX + inner, centerY - inner );
		cairo_rel_line_to( cr, outerRel, -outerRel );
		cairo_move_to( cr, centerX - inner, centerY - inner );
		cairo_rel_line_to( cr, -outerRel, -outerRel );
		cairo_move_to( cr, centerX - UNIT_CIRCLE  * gammaScale, centerY );
		cairo_rel_line_to( cr, (UNIT_CIRCLE  * gammaScale) * 2.0, 0.0 );
		cairo_move_to( cr, centerX, centerY - UNIT_CIRCLE  * gammaScale );
		cairo_rel_line_to( cr, 0.0, (UNIT_CIRCLE  * gammaScale) * 2.0 );

		cairo_stroke (cr);

		// if we overlay polar & polar (but with different scales) or
		// Smith & polar, display legend in colors matching the trace
		setTraceColor( cr, pGrid->overlay.bPolarWithDiferentScaling
				|| pGrid->overlay.bPolarSmith, channel );

		setCairoFontSize(cr, pGrid->fontSize / pGrid->scale); // initially 10 pixels

		if( bAnnotate ) {
			for( i=1; UNIT_CIRCLE * i / 5 * rangeMultiplier < UNIT_CIRCLE * gammaScale; i++ ) {
				double radius = UNIT_CIRCLE * i / 5 * rangeMultiplier;
				g_snprintf(label, BUFFER_SIZE_20, round( radius ) != radius ? "%.1f" : "%.0f", radius);
				cairo_arc (cr, centerX, centerY, radius, 0, 2 * G_PI);
				// if we bunch, only label every second circle
				if( UNIT_CIRCLE * 8.5 / 5.0 * rangeMultiplier > UNIT_CIRCLE * gammaScale
						|| (i % 2) == 1 ) {
					// if we overlay polar & polar (but with different scales) and are on ch2 or
					// Smith & polar, do not overwrite the legend from ch1
					if ( (pGrid->overlay.bPolarWithDiferentScaling && channel == eCH_TWO)
							|| pGrid->overlay.bPolarSmith )
						centreJustifiedCairoTextWithClear( cr, label, radius, centerY + (pGrid->fontSize / pGrid->scale) * 1.5 );
					else
						centreJustifiedCairoTextWithClear( cr, label, radius, centerY );
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
gboolean
showPolarCursorInfo(
		cairo_t *cr, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal,
		gdouble real, gdouble imag, gdouble frequency )
{
#define SPACELEFT 40

	gdouble mag, angle;
	gdouble xTextPos, yTextPos;
	gchar *label;
	gchar sValue[ BUFFER_SIZE_100 ], *sPrefix="";

	mag = sqrt( SQU( real ) + SQU( imag ) );
	angle = 180.0 * atan2( imag, real) / G_PI;

	// We use this font because it has the relevant Unicode glyphs for gamma and degrees
	cairo_select_font_face(cr, CURSOR_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

	xTextPos = pGrid->areaWidth * 0.05 + pGrid->leftMargin;

	if( !pGrid->overlay.bAny ) {
		yTextPos = pGrid->bottomMargin * 1.1;
	} else {
		if ( channel == eCH_ONE ) {
			yTextPos = pGrid->gridHeight + pGrid->bottomMargin - 4 * pGrid->fontSize;
		} else {
			yTextPos = pGrid->bottomMargin * 1.1;
		}
	}

	setTraceColor( cr, pGrid->overlay.bAny, channel );

	label = g_strdup_printf(" %4.3f U ∠ %5.3f°", mag, angle);
	filmCreditsCairoText( cr, "", label, 0,  xTextPos,  yTextPos, eBottomLeft );
	g_free( label );

	label = engNotation( frequency, 2, eENG_SEPARATE, &sPrefix );
	g_snprintf( sValue, BUFFER_SIZE_100, " %s %sHz", label, sPrefix);
	filmCreditsCairoText( cr, "Freq =", sValue, 1,  xTextPos,  yTextPos, eBottomLeft );
	g_free( label );

    return TRUE;
}

