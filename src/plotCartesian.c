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


gdouble logGrids[ NUM_LOG_GRIDS ] = {
		 0.0,            0.0,            0.301029995664, 0.477121254720, 0.602059991328,
		 0.698970004336, 0.778151250384, 0.845098040014, 0.903089986992, 0.954242509439
		};
/*!     \brief  Display the cartesian grid
 *
 * If the plot is cartesian, draw the grid and legends.
 * If there is an overlay of two cartesian grids, the y legend for the second channel
 * is placed on the right hand side. The actual grid is only drawn once.
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
plotCartesianGrid (cairo_t *cr, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal)
{
	gdouble perDiv, refPos, refVal, min,  __attribute__((unused)) max;
	gdouble logStartFreq, logStopFreq, logSpan, startOffset, logStartFreqDecade, xGrid;
	tChannel *pChannel = &pGlobal->HP8753.channels[channel];

	gint i;
	gchar *sYlabels[ NVGRIDS+1 ];
	cairo_text_extents_t YlabelExtents[ NVGRIDS+1 ];
	static gdouble maxYlabelWidth = 0.0;
	double yLabelScale = 1.0;

	// In calculating the maximum Y label we need to see both sides
	// So don't reset if we are overlaying and this is ch 2
	if( channel == eCH_ONE || !pGrid->overlay.bCartesian ) {
		maxYlabelWidth = 0.0;
	}

    cairo_save(cr);
    {
		setCairoFontSize(cr, pGrid->fontSize); // initially 10 pixels

		// where in the grid the reference line is (0 to 10)
		refPos = pChannel->scaleRefPos;
		// dB per division
		perDiv = pChannel->scaleVal;
		// dB at reference line
		refVal = pChannel->scaleRefVal;

		if( !pChannel->chFlags.bValidData ) {
			perDiv = 10.0;
			refPos = 5.0;
		}

		min = -refPos * perDiv + refVal;
		max = min + (NVGRIDS * perDiv);
		// We create and measure the size of tha labels so that we can
		// determine the widest label to properly place the ch2 labels
		// on the right of the grid (for a dual single plot)
		for( i=0; i < NVGRIDS+1; i++ ) {
			double yTicValue = min + (i * perDiv);
			// This avoids odd rounding error issues (0 showing as extremely small number)
			if( refVal != 0.0 && fabs( yTicValue ) < perDiv / 1.0e6 )
				yTicValue = 0.0;
			sYlabels[i] = engNotation( yTicValue, 2, eENG_NORMAL, NULL);
			cairo_text_extents (cr, sYlabels[i], &YlabelExtents[i]);
			if( YlabelExtents[i].width + YlabelExtents[i].x_bearing > maxYlabelWidth )
				maxYlabelWidth = YlabelExtents[i].width + YlabelExtents[i].x_bearing;
		}
		// If we have a larger than usual response (Y) label, then make room by expanding the margins to accomodate
		if( maxYlabelWidth + pGrid->textMargin  > pGrid->leftGridPosn   ) {
			yLabelScale = (pGrid->leftGridPosn - pGrid->textMargin) / maxYlabelWidth;
		}

		// Draw grid pattern
		// Don't redraw if we will draw over (when overlaying)
		if( channel == eCH_TWO || !pGrid->overlay.bCartesian ) {
			switch ( pChannel->sweepType ) {
			case eSWP_LOGFREQ:
				logStartFreq = log10( pChannel->sweepStart );
				logStopFreq = log10( pChannel->sweepStop );
				logSpan = logStopFreq - logStartFreq;
				startOffset = modf( logStartFreq, &logStartFreqDecade );
				// find the start grid
				for( i=1; i < sizeof(logGrids)/sizeof(double) && logGrids[ i ] < startOffset; i++ );
				// i is now the index to logGrids of the next grid
				for( double decades = 0.0;; i++ ) {
					// We repeat the sequence of nine grids each decade
					if( i >= sizeof(logGrids)/sizeof(double) ) {
						i = 1;
						++decades;
					}
					// break when we do all the grids
					if( logGrids[ i ] + logStartFreqDecade + decades > logStopFreq )
						break;
					else {
						xGrid = (logGrids[ i ] - startOffset + decades) / logSpan * pGrid->gridWidth;
						cairo_move_to(cr, pGrid->leftGridPosn + xGrid, pGrid->bottomGridPosn );
						cairo_line_to(cr, pGrid->leftGridPosn + xGrid, pGrid->areaHeight - pGrid->topGridPosn );
					}
				}
				cairo_move_to(cr, pGrid->leftGridPosn, pGrid->bottomGridPosn );
				cairo_line_to(cr, pGrid->leftGridPosn, pGrid->areaHeight - pGrid->topGridPosn );
				cairo_move_to(cr, pGrid->leftGridPosn + pGrid->gridWidth, pGrid->bottomGridPosn );
				cairo_line_to(cr, pGrid->leftGridPosn + pGrid->gridWidth, pGrid->areaHeight - pGrid->topGridPosn );
				break;
			default:
				for( i=0; i < NHGRIDS+1; i++ ) {	// Vertical grid lines
					cairo_move_to(cr, pGrid->leftGridPosn + (i * pGrid->gridWidth / NHGRIDS), pGrid->bottomGridPosn );
					cairo_line_to(cr, pGrid->leftGridPosn + (i * pGrid->gridWidth / NHGRIDS), pGrid->areaHeight - pGrid->topGridPosn );
				}
				break;
			}
			for( i=0; i < NVGRIDS+1; i++ ) {	// Horizontal grid lines
				cairo_move_to(cr, pGrid->leftGridPosn, pGrid->bottomGridPosn + (i * pGrid->gridHeight / NVGRIDS) );
				cairo_line_to(cr, pGrid->areaWidth - pGrid->rightGridPosn, pGrid->bottomGridPosn + (i * pGrid->gridHeight / NVGRIDS) );
			}

			//    gtk_style_context_get_color (context, gtk_style_context_get_state (context), &color);
			//    gdk_cairo_set_source_rgba (cr, &color);

			gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorGrid   ] );
			cairo_set_line_width (cr, pGrid->areaWidth / 1000.0 * 0.5);
			cairo_stroke (cr);
		}

		showStimulusInformation (cr, pGrid, channel, pGlobal);
		setTraceColor( cr, pGrid->overlay.bAny, channel );

		// y-axis labels
		// put bottom left of the grid at 0.0
		cairo_translate( cr, pGrid->leftGridPosn, pGrid->bottomGridPosn);

		setCairoFontSize(cr, pGrid->fontSize * yLabelScale); // initially 10 pixels

		for( i=0; i < NVGRIDS+1; i++ ) {
			if( !pGrid->overlay.bCartesian || channel == eCH_ONE ) {
				// Left side Y labels
				cairo_move_to(cr,
						- (YlabelExtents[i].width + YlabelExtents[i].x_bearing) * yLabelScale - pGrid->textMargin,
						(i * pGrid->gridHeight / NVGRIDS) - (YlabelExtents[i].height/2 + YlabelExtents[i].y_bearing));
			} else {
				cairo_move_to(cr, pGrid->gridWidth
						+ maxYlabelWidth * yLabelScale
						- (YlabelExtents[i].width + YlabelExtents[i].x_bearing) * yLabelScale + pGrid->textMargin,
						(i * pGrid->gridHeight / NVGRIDS) - (YlabelExtents[i].height/2 + YlabelExtents[i].y_bearing));
			}
			cairo_show_text (cr, sYlabels[i]);
			g_free( sYlabels[i] );
		}

		if( channel == eCH_ONE || !pGlobal->HP8753.flags.bDualChannel )
			showTitleAndTime( cr, pGrid, pGlobal->HP8753.sTitle,
					pGlobal->flags.bShowDateTime ? pGlobal->HP8753.dateTime : "" );

    }
    cairo_restore(cr);
	return TRUE;
}

/*!     \brief  Display the trace on the cartesian grid
 *
 * Plot the trace as a series of connected lines.
 * If the sweep is logarithmic, then make the appropriate transformation.
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
plotCartesianTrace (cairo_t *cr, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal)
{
	gdouble perDiv, refVal, refPos;
	gdouble sweepScale, levelScale;
	gint i;
	gdouble x, y, yl, yu, sweepValue = 0, xlabel, ylabel, xMouse, xFract;
	gdouble logFreqStart, logFreqStop;
	gint xl, xu, npoints, seg;
	gchar *sLabel = 0, sNote[ BUFFER_SIZE_100 ], *sPrefix="";
	tChannel *pChannel = &pGlobal->HP8753.channels[channel];
    GdkRGBA solidCursorRGBA = plotElementColors[ eColorLiveMkrCursor ];
    solidCursorRGBA.alpha = 1.0;

	npoints = pChannel->nPoints;
	// where in the grid the reference line is (0 to 10)
	refPos = pChannel->scaleRefPos;
	// dB per division
	perDiv = pChannel->scaleVal;
	// dB at reference line
	refVal = pChannel->scaleRefVal;
	if( !pChannel->chFlags.bValidData ) {
		perDiv = 10.0;
		refPos = 5.0;
		refVal = 0.0;
	}


	cairo_save( cr ); {
		// Draw reference line

	    gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorRefLine1   ] );
		cairo_set_line_width (cr, pGrid->areaWidth / 1000.0 * 1.5);
		cairo_move_to(cr, pGrid->leftGridPosn, pGrid->bottomGridPosn + refPos * pGrid->gridHeight / NVGRIDS);
		cairo_rel_line_to(cr, pGrid->gridWidth, 0.0);
		cairo_stroke( cr );


		if ( npoints ) {
			// put bottom left of the grid at 0.0
			cairo_translate( cr, pGrid->leftGridPosn, pGrid->bottomGridPosn);
			// clip to grid
			cairo_rectangle( cr, 0, 0, pGrid->gridWidth, pGrid->gridHeight);
			cairo_clip(cr);
			// make y scaling match grid (and reverse to normal)
			// scale x to number of points (0 at left & npoints at right)
			// cairo_scale( cr,  (gdouble)gridWidth/(gdouble)(npoints-1),
			//		gridHeight / (NVGRIDS * perDiv) );

			sweepScale = (gdouble)pGrid->gridWidth/(gdouble)(npoints-1);
			levelScale = (gdouble)pGrid->gridHeight/(NVGRIDS * perDiv);

			// translate to zero
			cairo_translate( cr, 0.0,  refPos * perDiv * levelScale);

			setTraceColor( cr, pGrid->overlay.bAny, channel );
			cairo_set_line_width (cr, pGrid->areaWidth / 1000.0);

			for ( i=0, seg=0; i < npoints; i++) {
				x = i * sweepScale;
				y = pChannel->responsePoints[i].r - refVal;

				// the stimulus sample points are non linear when all segments are displayed in list freq sweep mode
				if( (pChannel->sweepType == eSWP_LSTFREQ && pChannel->chFlags.bAllSegments)
						&& (pChannel->sweepStart != pChannel->sweepStop) ) {
					x = (gdouble)pGrid->gridWidth * (pChannel->stimulusPoints[ i ] - pChannel->sweepStart)
							/ (pChannel->sweepStop - pChannel->sweepStart);

					if( pChannel->stimulusPoints[ i ] == pChannel->segments[seg].startFreq ) {
						cairo_move_to(cr, x, y * levelScale);
						if( pChannel->segments[seg].nPoints == 1 ) {
							cairo_arc(cr, x, y * levelScale, 1.0, 0, 2*G_PI );
							cairo_stroke( cr );
							seg++;
						}
					} else if( pChannel->stimulusPoints[ i ] == pChannel->segments[seg].stopFreq ) {
						cairo_line_to(cr, x, y * levelScale);
						cairo_stroke( cr );
						seg++;
					} else  {
						cairo_line_to(cr, x, y * levelScale);
					}
				} else {
					if ( i == 0 )
						cairo_move_to(cr, x, y * levelScale);
					else
						cairo_line_to(cr, x, y * levelScale);
				}
			}
			cairo_stroke (cr);

			cairo_reset_clip( cr );
	        drawMarkers( cr, pGlobal, pGrid, channel, refVal, levelScale );

			// translate actual mouse positions to translated ones
			// if we overlay then this is always on the first GtkDrawingArea
			if( pGrid->overlay.bAny )
				xMouse = pGlobal->mousePosition[eCH_ONE].r;
			else
				xMouse = pGlobal->mousePosition[channel].r;

			if( pGlobal->flags.bHoldLiveMarker )
			    xMouse = pGlobal->mouseXpercentHeld * pGrid->areaWidth;

			if ( xMouse >= pGrid->leftGridPosn && xMouse <= pGrid->gridWidth+pGrid->leftGridPosn ) {
				gboolean bValidSample = FALSE;
				xFract = (xMouse-pGrid->leftGridPosn) / pGrid->gridWidth;
				x = (npoints-1) * xFract; y=0.0;
				xl = (gint)floor(x); xu = (gint)ceil(x);

				if( pChannel->sweepType == eSWP_LSTFREQ && pChannel->chFlags.bAllSegments ) {
					sweepValue = LIN_INTERP(pChannel->sweepStart, pChannel->sweepStop, xFract);
					for( int seg=0, nSample=0; seg < pChannel->nSegments; seg++ ) {
						// see if we even have this sample frequency
						if( sweepValue >= pChannel->segments[seg].startFreq &&
								sweepValue <= pChannel->segments[seg].stopFreq ) {
							y = calculateSegmentLinearlyInterpolatedResponse( nSample, nSample+pChannel->segments[seg].nPoints,
									pChannel, sweepValue );
							bValidSample = TRUE;
							break;
						}
						nSample += pChannel->segments[seg].nPoints;
					}
				} else {
					yl = pChannel->responsePoints[xl].r - refVal;
					yu = pChannel->responsePoints[xu].r - refVal;
					y = LIN_INTERP( yl, yu, (x-xl));
					bValidSample = TRUE;
				}

				cairo_set_line_width (cr, pGrid->areaWidth / 1000.0 * 3.0);
				gdk_cairo_set_source_rgba (cr, &solidCursorRGBA);
				cairo_move_to( cr, x * sweepScale, -(refPos * perDiv * levelScale));
				cairo_rel_line_to( cr, 0, -(gdouble)pGrid->gridHeight/NVGRIDS/8);
				cairo_stroke(cr);

				if( bValidSample ) {
				    // dot in the middle of live marker
                    cairo_arc( cr, x * sweepScale, y * levelScale, pGrid->areaWidth / 1000.0 * 1.5, 0.0, 2 * G_PI );
                    cairo_fill(cr);
                    // circle arount live market
	                gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorLiveMkrCursor ] );
                    cairo_arc( cr, x * sweepScale, y * levelScale, pGrid->areaWidth / 1000.0 * 6.4, 0.0, 2 * G_PI );
                    cairo_stroke(cr);
				}


				// return to the initial transform
				cairo_set_matrix (cr, &pGrid->initialMatrix);
				setCairoFontSize(cr, pGrid->fontSize); // initially 10 pixels

				if( bValidSample ) {
					switch( pGlobal->HP8753.channels[channel].sweepType ) {
					case eSWP_LINFREQ:
					case eSWP_LSTFREQ:
					default:
						sweepValue = LIN_INTERP(pChannel->sweepStart,
								pChannel->sweepStop, xFract);
						break;
					case eSWP_LOGFREQ:
						logFreqStart = log10( pChannel->sweepStart );
						logFreqStop = log10( pChannel->sweepStop );
						sweepValue = pow( 10.0, logFreqStart + (logFreqStop-logFreqStart) * xFract );
						break;
					}


					sLabel = engNotation( sweepValue, 2, eENG_SEPARATE, &sPrefix );
					g_snprintf( sNote, BUFFER_SIZE_100, "  %s%s", sPrefix,
							sweepSymbols[pGlobal->HP8753.channels[channel].sweepType]);
					setTraceColor( cr, pGrid->overlay.bAny, channel );
					// Where to place the text indicating the freq / value
					if( pGrid->overlay.bAny && channel == eCH_TWO )
						ylabel= pGrid->bottomGridPosn + pGrid->gridHeight * 0.09;
					else
						ylabel= pGrid->bottomGridPosn + pGrid->gridHeight;

					xlabel = pGrid->leftGridPosn + 0.095 * pGrid->gridWidth;

					filmCreditsCairoText( cr, sLabel, sNote, 0, xlabel, ylabel, eTopLeft );
					g_snprintf( sNote, BUFFER_SIZE_100, "%.1f", y);
					gchar *sUnits = g_strdup_printf( "  %s", formatSymbols[pGlobal->HP8753.channels[channel].format]);
					filmCreditsCairoText( cr, sNote, sUnits, 1, xlabel, ylabel, eTopLeft);
					g_free( sUnits );
					g_free( sLabel );
				}
			}
		}
	} cairo_restore( cr );

	return TRUE;
}
