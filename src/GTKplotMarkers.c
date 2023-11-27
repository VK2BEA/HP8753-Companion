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
#include <math.h>

#define Z0 50.0

/*!     \brief  Transform polar or Smith response value to gamma (XY) values
 *
 * Transform polar or smith response value to gamma (XY) values
 *
 * \ingroup drawing
 *
 * \param V1		first value
 * \param V2		second value
 * \param gammaR	gamma R (X) transformed value
 * \param gammaR	gamma I (Y) transformed value
 * \param eFormat	format of values to transform
 */
void
smithOrPolarMarkerToXY( gdouble V1, gdouble V2, gdouble *gammaR, gdouble *gammaI, tMkrType eFormat ) {

	gdouble rNorm = V1 / Z0, xNorm = V2 / Z0;
	gdouble gNorm = V1 * Z0, bNorm = V2 * Z0;

	switch ( eFormat ){
	case eMkrLinear:
		*gammaR = V1 * cos( DEG2RAD(V2) );
		*gammaI = V1 * sin( DEG2RAD(V2) );
		// gamma and phase
		break;
	case eMkrLog:
		*gammaR = pow( 10.0, V1 / 20.0) * cos( DEG2RAD(V2) );
		*gammaI = pow( 10.0, V1 / 20.0) * sin( DEG2RAD(V2) );
		// log of gamma and phase
		break;
	case eMkrReIm:
	case eMkrDefault:
		// X and Y already
		*gammaR = V1;
		*gammaI = V2;
		break;
	case eMkrRjX:
		*gammaR = ( SQU( rNorm ) - 1.0 + SQU( xNorm ) )
					/ ( SQU( rNorm + 1.0 ) + SQU( xNorm ) );
		*gammaI = ( 2.0 * xNorm )
					/ ( SQU( rNorm + 1.0 ) + SQU( xNorm ) );
		break;
	case eMkrGjB:
		*gammaR = ( 1.0 - SQU( gNorm ) - SQU( bNorm ) )
					/ ( SQU( gNorm + 1.0 ) + SQU( bNorm ) );
		*gammaI = ( -2.0 * bNorm )
					/ ( SQU( gNorm + 1.0 ) + SQU( bNorm ) );
		break;
	}
}

/*!     \brief  Draw the arrow pointer to the marker position
 *
 * Draw the arrow pointer to the marker position
 *
 * \ingroup drawing
 *
 * \param cr        pointer to the cairo structure
 * \param pGrind    pointer to the grid paramaters
 * \param sLabel    pointer to the lable to accompany the marker
 * \param tMkrStyle	up or down (active) marker style
 * \param bDelta	is it a delta marker (a small delta symbol is displayed instead of label)
 * \param x         x position od the tip of the marker arrow
 * \param y         y position od the tip of the marker arrow
 */
void
drawMarkerSymbol(cairo_t *cr, tGridParameters *pGrid, gchar *sLabel,
		tMkrStyle tMkrStyle, gboolean bDelta, gdouble x, gdouble y)
{
	cairo_text_extents_t extents;
	double size = pGrid->fontSize / pGrid->scale;

	cairo_save( cr );
	{
		// setColor ( cr, eColorGreen );
		setCairoFontSize(cr, size);

		cairo_reset_clip( cr );
		cairo_new_path( cr );
		cairo_translate( cr, x, y );

		cairo_move_to( cr, 0.0, 0.0 );

		if( tMkrStyle == eACTIVE_MKR ) {
			cairo_scale( cr, 1.0, -1.0 );
			flipCairoText( cr );
		}

		if( tMkrStyle == eFIXED_MKR ) {
			cairo_scale( cr, 0.75, 0.75 );
		}

		cairo_line_to( cr, -size/3.5, -size * 1.25 );
		cairo_line_to( cr,  size/3.5, -size * 1.25 );
		cairo_close_path( cr );
		cairo_stroke( cr );


		cairo_text_extents (cr, sLabel, &extents);
		cairo_move_to(cr, -(extents.width + extents.x_bearing)/2.0, -((extents.height + extents.y_bearing) + (size * 1.6)));

		cairo_show_text (cr, sLabel);
		if( bDelta ) {
	        cairo_select_font_face(cr, MARKER_SYMBOL_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			cairo_show_text (cr, "Δ");
		}
	}
	cairo_restore( cr );
}

/*!     \brief  Draw the text at the side describing the marker
 *
 * Draw the text at the side describing the marker
 *
 * \ingroup drawing
 *
 * \param cr        pointer to the cairo structure
 * \param pGlobal   pointer to the gloabel data
 * \param pGrid     pointer to the grid paramaters
 * \param channel	which channel
 * \param mkrNo     the marker number pertaining to this data
 * \param bActive   whether this marker is the active marker (displayed before other markers)
 * \param nPosition which position to show the marker
 * \param stimulus  stimulus value of the marker
 * \param value1    first response value
 * \param value2    second response value
 */
void
drawMarkerText( cairo_t *cr,
		tGlobal *pGlobal, tGridParameters *pGrid,
		eChannel channel, gint mkrNo,
		gboolean bActive, gint nPosition,
		gdouble stimulus, gdouble value1, gdouble value2) {

	gdouble X, Y;
	gchar *sValue = NULL, *sPrefix;
	tChannel *pChannel = &pGlobal->HP8753.channels[ channel ];
	gchar sUnits[INFO_LEN];
	const gchar *sUnitsV1, *sUnitsV2;
	gboolean bPolarOrSmith = FALSE, bUseEngNotation = TRUE;
	const gchar *mkrLabel[] = {"1:", "2:", "3:", "4:", "Δ:"};

	gdouble markerFontSize = pGrid->fontSize * 0.90;
	gdouble lineSpacing = pGrid->lineSpacing * 0.90;

	cairo_save( cr ); {
    	// reset to original canvas scaling and transformation (0,0 at bottom left)
    	cairo_set_matrix( cr, &pGrid->initialMatrix );
    	cairo_reset_clip( cr );

//		cairo_select_font_face(cr, MARKER_FONT_NARROW, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL );
		setCairoFontSize( cr, markerFontSize);

		switch( pChannel->format ) {
		case eFMT_SMITH:
		case eFMT_POLAR:
			sUnitsV1 = formatSmithOrPolarSymbols[ pChannel->mkrType ][0];
			sUnitsV2 = formatSmithOrPolarSymbols[ pChannel->mkrType ][1];
			if( pChannel->mkrType == eMkrLog )
				bUseEngNotation = FALSE;
			bPolarOrSmith = TRUE;

			break;
		case eFMT_LOGM:
			bUseEngNotation = FALSE;
	    __attribute__ ((fallthrough));
		default:
			sUnitsV1 = formatSymbols[ pChannel->format ];
			bPolarOrSmith = FALSE;
			break;
		}

		// Top left of marker text area
		X = pGrid->areaWidth - pGrid->makerAreaWidth * 0.925;
		Y = (pGrid->areaHeight - pGrid->topMargin) - ((nPosition * (bPolarOrSmith ? 3.25: 2.25) + 1.0) * lineSpacing);

		if( pGrid->overlay.bAny && channel == eCH_TWO ) {
			Y -= pGrid->gridHeight / 2.0;
		}
		// if we also show bandwidth, start lower on the screen
		if( pChannel->chFlags.bBandwidth )
			Y -= lineSpacing * 3.25;

		// Active is the first one, so subsequent text must accomodate the delta note (if delta active)
		if( nPosition != 0 && pChannel->chFlags.bMkrsDelta ) {
			Y -= lineSpacing * 1.25;
		}
		cairo_select_font_face(cr, MARKER_FONT_NARROW, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD );
    	cairo_move_to( cr, X, Y );
    	cairo_show_text( cr, mkrLabel[ mkrNo ]);
		if( pChannel->chFlags.bMkrsDelta && mkrNo == pChannel->deltaMarker )
			cairo_select_font_face(cr, MARKER_FONT_NARROW, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD );
		else
			cairo_select_font_face(cr, MARKER_FONT_NARROW, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL );

    	// X around which value and unit are given NNN.NNNN | UU
		X = pGrid->areaWidth - pGrid->makerAreaWidth * 0.25;

		// Value 1
		if( bUseEngNotation ) {
			sValue = engNotation(value1, 3, eENG_SEPARATE, &sPrefix);
		} else {
			sValue = g_strdup_printf( "%.2f", value1);
			sPrefix = "";
		}
    	g_snprintf( sUnits, INFO_LEN, " %s%s", sPrefix, sUnitsV1);
    	rightJustifiedCairoText(cr, sValue, X, Y );
		cairo_move_to( cr, X, Y );
		cairo_show_text (cr, sUnits);
		g_free( sValue );

		Y -= lineSpacing;

		if( bPolarOrSmith ) {
			// Value 2
	    	sValue = engNotation(value2, 3, eENG_SEPARATE, &sPrefix);
	    	g_snprintf( sUnits, INFO_LEN, " %s%s", sPrefix, sUnitsV2);
	    	rightJustifiedCairoText(cr, sValue, X, Y );
			cairo_move_to( cr, X, Y );
			cairo_show_text (cr, sUnits);
			g_free( sValue );
			Y -= lineSpacing;
		}

    	// Stimulus
    	// sValue = engNotation(stimulus, 3, eENG_SEPARATE, &sPrefix);
		if( pChannel->sweepType <= eSWP_LSTFREQ && bActive ) {
			sValue = doubleToStringWithSpaces( stimulus/1e6, NULL );
			sPrefix="M";
		} else {
			sValue = engNotation(stimulus, 3, eENG_SEPARATE, &sPrefix);
		}
    	g_snprintf( sUnits, INFO_LEN, " %s%s", sPrefix, sweepSymbols[pChannel->sweepType]);
    	rightJustifiedCairoText(cr, sValue, X, Y);
		cairo_move_to( cr, X, Y );
		cairo_show_text (cr, sUnits);

		g_free( sValue );

		if( pChannel->chFlags.bMkrsDelta && nPosition == 0 ) {
			X = pGrid->areaWidth - pGrid->makerAreaWidth * 0.925;
			Y -= lineSpacing * 1.25;
			cairo_move_to( cr, X, Y );
			if( pChannel->deltaMarker != FIXED_MARKER )
				sValue = g_strdup_printf( "Δ ref = %d", pChannel->deltaMarker+1 );
			else
				sValue = g_strdup( "Δ ref = Δ");
			cairo_show_text( cr, sValue);
			g_free( sValue );
		}
	} cairo_restore( cr );
}

/*!     \brief  Draw the text describing the bandwidth calculation
 *
 * Draw the text describing the bandwidth calculation
 *
 * \ingroup drawing
 *
 * \param cr        pointer to the cairo structure
 * \param pGlobal   pointer to the gloabel data
 * \param pGrid     pointer to the grid paramaters
 * \param channel	which channel
 */
void
drawBandwidthText( cairo_t *cr,
		tGlobal *pGlobal, tGridParameters *pGrid,
		eChannel channel ) {

	gdouble X, Y;
	gchar *sPrefix;
	tChannel *pChannel = &pGlobal->HP8753.channels[ channel ];
	gchar sUnits[INFO_LEN];

	gdouble markerFontSize = pGrid->fontSize * 0.90;
	gdouble lineSpacing = pGrid->lineSpacing * 0.90;

	cairo_save( cr ); {
    	// reset to original canvas scaling and transformation (0,0 at bottom left)
    	cairo_set_matrix( cr, &pGrid->initialMatrix );
    	cairo_reset_clip( cr );

		cairo_select_font_face(cr, MARKER_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL );
		setCairoFontSize( cr, markerFontSize);
		lineSpacing = markerFontSize * 1.2;

		// Bottom of text area for channel
		X = pGrid->areaWidth - pGrid->makerAreaWidth * 0.925;
		Y = (pGrid->bottomMargin + pGrid->gridHeight) - lineSpacing;

		if( pGrid->overlay.bAny && channel == eCH_TWO ) {
			Y -= pGrid->gridHeight / 2.0;
		}

		cairo_select_font_face(cr, MARKER_FONT_NARROW, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD );
    	cairo_move_to( cr, X, Y );
    	cairo_show_text( cr, "Width:");
    	cairo_move_to( cr, X, Y - lineSpacing );
    	cairo_show_text( cr, "Center:");
    	cairo_move_to( cr, X, Y - 2.0 * lineSpacing );
    	cairo_show_text( cr, "Q:");

		cairo_select_font_face(cr, MARKER_FONT_NARROW, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL );

    	// X around which value and unit are given NNN.NNNN | UU
		X = pGrid->areaWidth - pGrid->makerAreaWidth * 0.25;

		gchar *sWidth, *sCenter;
		// width
		sWidth = engNotation(pChannel->bandwidth[ BW_WIDTH ], 3, eENG_SEPARATE, &sPrefix);
    	g_snprintf( sUnits, INFO_LEN, " %s%s", sPrefix, "Hz");
    	rightJustifiedCairoText(cr, sWidth, X, Y );
		cairo_move_to( cr, X, Y );
		cairo_show_text (cr, sUnits);
		g_free( sWidth );
		Y -= lineSpacing;
		// Center freq
		sCenter = engNotation(pChannel->bandwidth[ BW_CENTER ], 3, eENG_SEPARATE, &sPrefix);
    	g_snprintf( sUnits, INFO_LEN, " %s%s", sPrefix, "Hz");
    	rightJustifiedCairoText(cr, sCenter, X, Y );
		cairo_move_to( cr, X, Y );
		cairo_show_text (cr, sUnits);
		g_free( sCenter );
		Y -= lineSpacing;
		// Q
		g_snprintf( sUnits, INFO_LEN, " %.3f", pChannel->bandwidth[ BW_Q ]);
		rightJustifiedCairoText(cr, sUnits, X, Y );

	} cairo_restore( cr );
}


// Coordinates are translated to:
//		0,0 is bottom left of grid (cartesian) or center Polar & Smith
// Scaling for Y or radius has already occured.
/*!     \brief  Draw the all the markers on the plot
 *
 * Draw the all the markers on the plot
 *
 * \ingroup drawing
 *
 * \param cr        pointer to the cairo structure
 * \param pGlobal   pointer to the gloabel data
 * \param pGrid     pointer to the grid paramaters
 * \param channel	which channel
 * \param Yoffset   if there is a 'ref value' that changes the position on the screen
 * \param Yscale    response scaling factor that changes the position on the screen
 */
void
drawMarkers( cairo_t *cr, tGlobal *pGlobal, tGridParameters *pGrid,
		eChannel channel, gdouble Yoffset, gdouble Yscale ) {
	// now get the marker source values and response values
	gint mkrNo = 0, flagBit, nMkrsShown;
	gdouble	stimulus, valueR, valueI, prtStimulus, prtValueR, prtValueI;
	gdouble X=0.0, Y=0.0;
	tChannel *pChannel = &pGlobal->HP8753.channels[ channel ];
	gchar *mkrLabels[] = {"1", "2", "3", "4", ""};
	gboolean bFixedMarker = FALSE;
	gboolean bActiveShown;

	for( mkrNo = 0, nMkrsShown=0, bActiveShown=FALSE, flagBit = 0x01; mkrNo < MAX_MKRS; mkrNo++, flagBit <<= 1 ) {
		bFixedMarker = (mkrNo == FIXED_MARKER && pChannel->chFlags.bMkrsDelta && pChannel->deltaMarker == FIXED_MARKER);
		if( (pChannel->chFlags.bMkrs & flagBit) || bFixedMarker ) {
			prtStimulus = stimulus = pChannel->numberedMarkers[ mkrNo ].sourceValue;
			prtValueR = valueR = pChannel->numberedMarkers[ mkrNo ].point.r;
			prtValueI = valueI = pChannel->numberedMarkers[ mkrNo ].point.i;

			if( pChannel->chFlags.bMkrsDelta && !bFixedMarker) {
				if(  mkrNo != pChannel->deltaMarker ) {
					stimulus += pChannel->numberedMarkers[ pChannel->deltaMarker ].sourceValue;
					valueR += pChannel->numberedMarkers[ pChannel->deltaMarker ].point.r;
					valueI += pChannel->numberedMarkers[ pChannel->deltaMarker ].point.i;
				} else {
#ifdef DELTA_MARKER_ZERO
					prtStimulus = 0.0;
					prtValueR = 0.0;
					prtValueI = 0.0;
#endif
				}
			}
			if ( pChannel->sweepType == eSWP_LOGFREQ ) {
				gdouble logSweepStart = log10(pChannel->sweepStart), logSweepStop = log10(pChannel->sweepStop);
				X = (log10(stimulus) - logSweepStart) / (logSweepStop - logSweepStart) * pGrid->gridWidth;
			} else {
				X = (stimulus - pChannel->sweepStart)/(pChannel->sweepStop - pChannel->sweepStart) * pGrid->gridWidth;
			}

		    switch ( pChannel->format ) {
			case eFMT_LOGM:
			case eFMT_PHASE:
			case eFMT_DELAY:
			case eFMT_LINM:
			case eFMT_REAL:
			case eFMT_IMAG:
			case eFMT_SWR:
				Y = (valueR - Yoffset) * Yscale;
				break;
			case eFMT_SMITH:
			case eFMT_POLAR:
				smithOrPolarMarkerToXY( valueR, valueI, &X, &Y, pChannel->mkrType );
				break;
			}


			gint mkrTextPosn;
			if( !bFixedMarker )
			drawMarkerSymbol( cr, pGrid, mkrLabels[mkrNo],
					mkrNo == pChannel->activeMarker ? eACTIVE_MKR : eNONACTIVE_MKR,
					pChannel->chFlags.bMkrsDelta && mkrNo == pChannel->deltaMarker,
					X, Y);
			else
				drawMarkerSymbol( cr, pGrid, "", eFIXED_MKR, FALSE, X, Y);
			// Show marker details on screen
			if( bActiveShown )
				mkrTextPosn = nMkrsShown;
			else if ( mkrNo == pChannel->activeMarker )
				mkrTextPosn = 0;
			else
				mkrTextPosn = nMkrsShown + 1;
			if( pGlobal->flags.bDeltaMarkerZero
					&& pChannel->chFlags.bMkrsDelta
					&& mkrNo == pChannel->deltaMarker ) {
				drawMarkerText( cr, pGlobal, pGrid, channel, mkrNo,
						mkrNo == pChannel->activeMarker,
						mkrTextPosn, 0.0, 0.0, 0.0);
			} else {
				drawMarkerText( cr, pGlobal, pGrid, channel, mkrNo,
						mkrNo == pChannel->activeMarker,
						mkrTextPosn, prtStimulus, prtValueR, prtValueI);
			}

			if ( mkrNo == pChannel->activeMarker )
				bActiveShown = TRUE;
			nMkrsShown++;

		}
	}

	if( pChannel->chFlags.bBandwidth ) {
		drawBandwidthText( cr, pGlobal, pGrid, channel );
	}
}
