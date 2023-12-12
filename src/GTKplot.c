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

/*! \file GTKplot.c
 *  \brief Plot traces and information onto the two GtkDrawingArea widgets
 *
 * Two GtkDrawingArea widgets represent the two channels of the HP8753.
 * Data obtained from the network analyzer is used to mimic the display onto these two widgets.
 * The drawing contexts of the PNG, PDF Cairo objects are substituted for the widget's in
 * order to print or save to image.
 *
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

const	gchar *formatSymbols[] = { "dB", "°", "s", "U", "U", "U", "U", "", "U", "U" };
const	gchar *formatSmithOrPolarSymbols[][2] = { {"U", "°"}, {"dB", "°"}, {"U", "U"}, {"Ω", "Ω"}, {"S", "S"} };
const	gchar *sweepSymbols[] = { "Hz", "Hz", "Hz", "s", "dBm" };

/*!     \brief  find the width of the string in the current font
 *
 * Determine the width of the string in the current font and scaling
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param sLabel	NULL terminated string to show
 *
 */
gdouble
stringWidthCairoText(cairo_t *cr, gchar *sLabel)
{
	cairo_text_extents_t extents;
	cairo_text_extents (cr, sLabel, &extents);
	return( extents.x_advance );
}

/*!     \brief  Mirror the Cairo text vertically
 *
 * The drawing space is fliped to move 0,0 to the bottom left. This has the
 * side effect of also flipping the way text is rendered.
 * This routine flips the text back to normal.
 *
 * \ingroup drawing
 *
 * \param cr	pointer to cairo context
 *
 */
void
flipCairoText( cairo_t *cr )
{
	cairo_matrix_t font_matrix;
	cairo_get_font_matrix( cr, &font_matrix );
	font_matrix.yy = -font_matrix.yy ;
	cairo_set_font_matrix( cr, &font_matrix );
}

/*!     \brief  Mirror the Cairo coordinate system vertically
 *
 * The drawing space is fliped to move 0,0 to the bottom left.
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param stGrid	pointer to paramaters calculated for the grid setup
 *
 */
void
flipVertical( cairo_t *cr, tGridParameters *pGrid ) {
	cairo_matrix_t font_matrix;
	// first make 0, 0 the bottom left corner and sane direction
	cairo_translate( cr, 0.0, pGrid->areaHeight);
	cairo_scale( cr, 1.0, -1.0);
	// we need to flip the font back (otherwise it will be upside down)
	cairo_get_font_matrix( cr, &font_matrix );
	font_matrix.yy = -font_matrix.yy ;
	cairo_set_font_matrix( cr, &font_matrix );
}

/*!     \brief  Change the font size
 *
 * Set the font size using the current scaling
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param fSize		font size (scaled)
 *
 */
void
setCairoFontSize( cairo_t *cr, gdouble fSize ) {
	cairo_matrix_t fMatrix = {0, .xx=fSize, .yy=-fSize};
	cairo_set_font_matrix(cr, &fMatrix);
}

/*!     \brief  Render a text string right justified from the specified point
 *
 * Show the string on the widget left justified (just a convienience to wrap move-to and show)
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param sLabel	NULL terminated string to show
 * \param x			x position of the right edge of the label
 * \param y			y position of the bottom of the label
 *
 */
void
leftJustifiedCairoText(cairo_t *cr, gchar *sLabel, gdouble x, gdouble y)
{
	cairo_move_to(cr, x, y );
	cairo_show_text (cr, sLabel);
}

/*!     \brief  Render a text string right justified from the specified point
 *
 * Show the string on the widget right justified
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param sLabel	NULL terminated string to show
 * \param x			x position of the right edge of the label
 * \param y			y position of the bottom of the label
 *
 */
void
rightJustifiedCairoText(cairo_t *cr, gchar *sLabel, gdouble x, gdouble y)
{
	cairo_move_to(cr, x - stringWidthCairoText(cr, sLabel), y );
	cairo_show_text (cr, sLabel);
}

/*!     \brief  Render a text string center justified around the specified point
 *
 * Show the string on the widget center justified
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param sLabel	NULL terminated string to show
 * \param x			x position of the right edge of the label
 * \param y			y position of the bottom of the label
 *
 */
void
centreJustifiedCairoText(cairo_t *cr, gchar *sLabel, gdouble x, gdouble y)
{
	cairo_move_to(cr, x - stringWidthCairoText(cr, sLabel)/2.0, y);
	cairo_show_text (cr, sLabel);
}

/*!     \brief  Render a text string center justified around the specified point (clearing the background)
 *
 * Show the string on the widget center justified
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param sLabel	NULL terminated string to show
 * \param x			x position of the right edge of the label
 * \param y			y position of the bottom of the label
 *
 */
void
centreJustifiedCairoTextWithClear(cairo_t *cr, gchar *label, gdouble x, gdouble y)
{
	cairo_text_extents_t extents;
	GdkRGBA  white;

    gdk_rgba_parse (&white,  "white");

	cairo_text_extents (cr, label, &extents);

	cairo_save( cr );
    gdk_cairo_set_source_rgba (cr, &white );
    cairo_new_path( cr );
	cairo_rectangle( cr, x - (extents.width + extents.x_bearing)/2,
			y - (extents.height + extents.y_bearing)*3/2, (extents.width + extents.x_bearing),
			extents.height + extents.y_bearing );
	cairo_stroke_preserve(cr);
	cairo_fill( cr );
	cairo_restore( cr );

	cairo_move_to(cr, x - (extents.width + extents.x_bearing)/2,
			y - (extents.height + extents.y_bearing)*3/2);
	cairo_show_text (cr, label);

}

/*!     \brief  Render two text strings right and left justified around the specified point
 *
 * Show the string on the widget center justified
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param sLabelL	NULL terminated string to show right justified to the left
 * \param sLabelR	NULL terminated string to show left justified on the right
 * \param nLine		line number (for a sequence of lines)
 * \param x			x position of the right edge of the label
 * \param y1stLine	y position of the bottom of the fist line of the label
 * \param ePos		orientaion of line stack (line one above or below line 2)
 *
 */
void
filmCreditsCairoText( cairo_t *cr, gchar *sLabelL, gchar *sLabelR, gint nLine,
		gdouble x, gdouble y1stLine, tTxtPosn ePos ) {
	gdouble y, line_height;
	cairo_text_extents_t extents;
	cairo_save( cr ); {
		cairo_text_extents ( cr, "|", &extents );
		line_height = extents.height - extents.y_bearing;

		// down or up
		if( ePos == eTopLeft || ePos == eTopRight )
			y = y1stLine - ( nLine + 1.0 ) * line_height;
		else
			y = y1stLine + ( nLine + 1.0 ) * line_height;

		cairo_move_to( cr, x - stringWidthCairoText(cr, sLabelL), y );
		cairo_show_text (cr, sLabelL);
		cairo_move_to( cr, x, y );
		cairo_show_text (cr, sLabelR);

	} cairo_restore( cr );
}


/*!     \brief  Render string left justified around the specified point
 *
 * Show the string on the widget center justified
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param sLabel
 * \param nLine		line number (for a sequence of lines)
 * \param x			x position of the right edge of the label
 * \param y1stLine	y position of the bottom of the fist line of the label
 * \param ePos		orientaion of line stack (line one above or below line 2)
 *
 */
void multiLineText(
		cairo_t *cr, gchar *sLabel, gint nLine, gdouble lineSpacing,
		gdouble x, gdouble y1stLine, tTxtPosn ePos ) {
	gdouble y;

	cairo_save( cr ); {
		if( ePos == eTopLeft || ePos == eTopRight )
			y = y1stLine - ( nLine + 1.5 ) * lineSpacing;
		else
			y = y1stLine + ( nLine + 1.5 ) * lineSpacing;

		if( ePos == eTopRight || ePos == eBottomRight ) {
			cairo_move_to( cr, x - stringWidthCairoText( cr, sLabel), y );
		} else {
			cairo_move_to( cr, x, y );
		}
		cairo_show_text (cr, sLabel);
	} cairo_restore( cr );
}

/*!     \brief  Set the trace color
 *
 * Set the trace color based on channel and overlay
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param bOverlay	TRUE if overlaying both channels
 * \param channel	which channel we are drawing
 */
void
setTraceColor( cairo_t *cr,gboolean bOverlay, eChannel channel ) {
	if( bOverlay ) {
		if ( channel == eCH_ONE ) {
		    gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorTrace1   ] );
		} else {
            gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorTrace2   ] );
		}
	} else {
        gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorTraceSeparate  ] );
	}
}


#define MICRO "µ"

#define PREFIX_START (-24)
/* Smallest power of then for which there is a prefix defined.
 If the set of prefixes will be extended, change this constant
 and update the table "prefix". */

/*!     \brief  Format double in engineering notation
 *
 * Return the malloced formatted string and (optionally separately) unit prefix
 *
 * \ingroup drawing
 *
 * \param value		pointer to cairo context
 * \param digits	precision of the mantissa
 * \param eVariant	normal is '10.7 M', separate is '10.7' & 'M', numeric as '10.7e6'
 * \return			the malloced string holding the formatted value
 */
gchar*
engNotation(gdouble value, gint digits, tEngNotation eVariant, gchar **sPrefix) {
	gint expof10;
	gchar *sResult, *sSign, *sMantissa;

	static gchar *prefix[] = {
			"y", "z", "a", "f", "p", "n", MICRO, "m", "", "k",
			"M", "G", "T", "P", "E", "Z", "Y" };
#define PREFIX_END (PREFIX_START+(int)((sizeof(prefix)/sizeof(char *)-1)*3))

	if (value < 0.0) {
		sSign = "-";
		value = -value;
	} else {
		sSign = "";
	}

	if( value == 0.0 ) {
		if( eVariant == eENG_SEPARATE && sPrefix )
			*sPrefix = "";
		return( g_strdup_printf( "0" ) );
	}

	expof10 = lrint(floor(log10(value)));
    if (expof10 > 0)
        expof10 = (expof10 / 3) * 3;
    else
        expof10 = ((-expof10 + 3) / 3) * (-3);

    value *= pow(10.0, -expof10);

    if (value >= 1000.0) {
        value /= 1000.0;
        expof10 += 3;
    }

    // This is to strip off trailing '0' or '.'
    int sc=1;
    for( int i=digits; i > 0; i-- ) sc *= 10;
    sMantissa = g_strdup_printf("%f", round( value * sc ) / sc);
    for( int i=strlen( sMantissa )-1; i >= 0; i-- ) {
        if( sMantissa[ i ] == '0' ) {
        	sMantissa[ i ] = 0;
        } else {
        	if( sMantissa[ i ] == '.' ) {
        		sMantissa[ i ] = 0;
        	}
        	break;
        }
    }


    if (eVariant == eNEG_NUMERIC || (expof10 < PREFIX_START) || (expof10 > PREFIX_END)) {
        sResult = g_strdup_printf( "%s%se%d", sSign, sMantissa, expof10);
        if( sPrefix )
        	*sPrefix = "";
	} else if( eVariant == eENG_SEPARATE ) {
        sResult = g_strdup_printf( "%s%s", sSign, sMantissa );
        if( sPrefix )
        	*sPrefix = prefix[(expof10 - PREFIX_START) / 3];
    }
    else {
        sResult = g_strdup_printf( "%s%s %s", sSign, sMantissa,
                prefix[(expof10 - PREFIX_START) / 3]);
    }
    g_free( sMantissa );
    return sResult;
}


/*!     \brief  Determine grid layout
 *
 * Determine paramaters for the grid layout based on the grid types,
 * number of displayed channels, if the sources are coupled and if markers are shown.
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param pGlobal	pointer to global data
 * \param channel	which channel
 * \param pGrid		pointer to structure in which we set the values
 */
void
determineGridPosition( cairo_t *cr, tGlobal *pGlobal, eChannel channel, tGridParameters *pGrid ){
	tGrid gtOne, gtTwo;

	gtOne = gridType[ pGlobal->HP8753.channels[eCH_ONE].format != INVALID ? pGlobal->HP8753.channels[eCH_ONE].format : eFMT_LOGM ];
	gtTwo = gridType[ pGlobal->HP8753.channels[eCH_TWO].format != INVALID ? pGlobal->HP8753.channels[eCH_TWO].format : eFMT_LOGM ];

	if( pGlobal->HP8753.flags.bDualChannel
			&& !pGlobal->HP8753.flags.bSplitChannels ) {
		pGrid->overlay.bCartesian = ( gtOne == eGridCartesian && gtTwo == eGridCartesian );
		pGrid->overlay.bPolar = ( gtOne == eGridPolar && gtTwo == eGridPolar );
		pGrid->overlay.bSmith = ( gtOne == eGridSmith && gtTwo == eGridSmith );
		pGrid->overlay.bAny = TRUE;
		pGrid->overlay.bPolarWithDiferentScaling = pGrid->overlay.bPolar
				& (pGlobal->HP8753.channels[ eCH_ONE ].scaleVal != pGlobal->HP8753.channels[ eCH_TWO ].scaleVal);
		pGrid->overlay.bSmithWithDiferentScaling = pGrid->overlay.bSmith
				& (pGlobal->HP8753.channels[ eCH_ONE ].scaleVal != pGlobal->HP8753.channels[ eCH_TWO ].scaleVal);
		pGrid->overlay.bPolarSmith = ( gtOne == eGridPolar && gtTwo == eGridSmith )
				|| ( gtOne == eGridSmith && gtTwo == eGridPolar );
	} else {
		pGrid->overlay.bAny = FALSE;
		pGrid->overlay.bCartesian = FALSE;
		pGrid->overlay.bPolar = FALSE;
		pGrid->overlay.bSmith = FALSE;
	}
	pGrid->bSourceCoupled = pGlobal->HP8753.flags.bSourceCoupled;

	pGrid->leftGridPosn   = PERCENT(pGrid->areaWidth,  5.0);
	if( pGrid->overlay.bCartesian  )
		pGrid->rightGridPosn  = PERCENT(pGrid->areaWidth,  5.0);
	else
		pGrid->rightGridPosn  = PERCENT(pGrid->areaWidth,  2.0);

	if( pGrid->overlay.bAny  && !pGrid->bSourceCoupled )
		pGrid->bottomGridPosn = PERCENT(pGrid->areaHeight,  8.0);
	else
		pGrid->bottomGridPosn = PERCENT(pGrid->areaHeight,  5.0);

	pGrid->topGridPosn  = PERCENT(pGrid->areaHeight, 12.0);


	// pGrid->fontSize = fontSize( (gdouble)pGrid->areaHeight, (gdouble)pGrid->areaWidth );
	pGrid->fontSize = (gdouble)pGrid->areaHeight / 50.0;

	pGrid->textMargin	= pGrid->fontSize / 2.0;

	if( pGlobal->HP8753.channels[ channel ].chFlags.bMkrs ) {
		pGrid->makerAreaWidth = pGrid->fontSize * 10.0;
		pGrid->rightGridPosn += pGrid->makerAreaWidth;
	} else {
		pGrid->makerAreaWidth = 0.0;
	}

	pGrid->gridWidth  = (gdouble)pGrid->areaWidth  - (pGrid->leftGridPosn + pGrid->rightGridPosn);
	pGrid->gridHeight = (gdouble)pGrid->areaHeight - (pGrid->topGridPosn + pGrid->bottomGridPosn);

	pGrid->lineSpacing = (gdouble)pGrid->gridHeight / 32.0;

	pGrid->scale = 1.0;

	cairo_get_matrix( cr, &pGrid->initialMatrix );
}


/*!     \brief  Display some channel setting at the top of the display
 *
 * Show the channel settings at the top of the display.
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param pGrid		pointer to grid paramaters
 * \param channel	which channel we are displaying information for
 * \param pGlobal	global data structure
 *
 * \return TRUE for success
 */
static gboolean
showStatusInformation (cairo_t *cr, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal)
{

	double xOffset = 0.0;
	gdouble perDiv, refVal;

	gchar sInfo[ INFO_LEN ];
	tFormat eFormat;
	tChannel *pChannel = &pGlobal->HP8753.channels[channel];
	gdouble lineSpacing;

	cairo_save(cr); {

		setCairoFontSize(cr, pGrid->fontSize);
		lineSpacing = pGrid->lineSpacing * 1.15;

		eFormat = pChannel->format;
		// dB per division
		perDiv = pChannel->scaleVal;
		// dB at reference line
		refVal = pChannel->scaleRefVal;
		if( !pChannel->chFlags.bValidData ) {
			perDiv = 10.0;
			refVal = 0.0;
		}

		if( pGrid->overlay.bAny ) {
			if ( channel == 0 ) {
			    gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorTrace1   ] );
			} else {
			    gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorTrace2   ] );
				xOffset = 5.0 * pGrid->gridWidth / NHGRIDS;
			}
		} else {
		    gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorTraceSeparate   ] );
		}
		multiLineText( cr, optMeasurementType[ pChannel->measurementType ].desc,
				1, lineSpacing, pGrid->leftGridPosn + xOffset, pGrid->areaHeight, eTopLeft );
		multiLineText( cr, optFormat[ pChannel->format ].desc,
				1, lineSpacing, pGrid->leftGridPosn + xOffset + 1.5 * (pGrid->gridWidth / NHGRIDS), pGrid->areaHeight, eTopLeft );
		gchar *tStr = engNotation( refVal, 2, eENG_NORMAL, NULL );
		if(eFormat == eFMT_POLAR || eFormat == eFMT_SMITH )
			g_snprintf( sInfo, INFO_LEN, "%s%s FS", tStr, formatSymbols[eFormat]);
		else
			g_snprintf( sInfo, INFO_LEN, "Ref. %s%s", tStr, formatSymbols[eFormat]);
		multiLineText( cr, sInfo, 2, lineSpacing, pGrid->leftGridPosn + xOffset, pGrid->areaHeight, eTopLeft );
		g_free( tStr );
		tStr = engNotation( perDiv, 2,  eENG_NORMAL, NULL );
		g_snprintf( sInfo, INFO_LEN, "%s%s/div", perDiv == 0 ? "10" : tStr, formatSymbols[eFormat]);
		if(	!(eFormat == eFMT_POLAR || eFormat == eFMT_SMITH) )
			multiLineText( cr, sInfo, 2, lineSpacing,
					pGrid->leftGridPosn + xOffset + 1.5 * (pGrid->gridWidth / NHGRIDS), pGrid->areaHeight, eTopLeft );
		// IF BW
		gchar *sTemp = engNotation( pGlobal->HP8753.channels[channel].IFbandwidth, 0, eENG_NORMAL, NULL );

		if( pGlobal->HP8753.flags.bSourceCoupled && pGrid->overlay.bAny && channel == eCH_TWO )
			sInfo[0] = 0;	// don't give redundant information
		else
			g_snprintf(sInfo, INFO_LEN, "IF BW  %sHz", sTemp);

		multiLineText( cr, sInfo,
				!pGlobal->HP8753.flags.bSourceCoupled && pGrid->overlay.bAny ? channel + 1: 2,
						lineSpacing, pGrid->leftGridPosn + pGrid->gridWidth, pGrid->areaHeight, eTopRight );
		g_free( sTemp );
		g_free( tStr );
	} cairo_restore( cr);

	return TRUE;
}

/*!     \brief  format the label on the stimulus (x) axis
 *
 * Context sensitive labels (Hz for frequency, s for time etc)
 *
 * \ingroup drawing
 *
 * \param value			value of stimulus to display
 * \param sweepType		sweep type which will determine what units to display
 *
 */
gchar *
formatXaxisLabel ( gdouble value, tSweepType sweepType ) {
	gchar *sLabel = NULL;
	gchar *pf;
	gchar *tStr;

	switch ( sweepType ) {
	case eSWP_CWTIME:
		tStr = engNotation( value, 2, eENG_SEPARATE, &pf );
		sLabel = g_strdup_printf("%s %ss", tStr, pf);
		g_free ( tStr );
		break;
	case eSWP_PWR:
		sLabel = g_strdup_printf("%.3f dbm", value);
		break;
	case eSWP_LOGFREQ:
	case eSWP_LINFREQ:
	case eSWP_LSTFREQ:
	default:
		sLabel = doubleToStringWithSpaces( value / 1e6, "MHz" );
		break;
	}
	return sLabel;
}

/*!     \brief  Display labels on the stimulus (x) axis
 *
 * Context sensitive labels (Hz for frequency, s for time etc)
 *
 * \ingroup drawing
 *
 * \param cr			pointer to cairo context
 * \param value			value of stimulus to display
 * \param x				x position of the right edge of the label
 * \param y				y position of the bottom of the label
 * \param sweepType		sweep type which will determine what units to display
 * \param justification	text justification
 *
 */
#define LBL_LENGTH	30
void
showXaxisLabel ( cairo_t *cr, gdouble value, gdouble x, gdouble y, tSweepType sweepType, tTxtPosn justification ) {
	gchar *sLabel = NULL;
	gchar *pf;
	gchar *tStr;

	switch ( sweepType ) {
	case eSWP_CWTIME:
		tStr = engNotation( value, 2, eENG_SEPARATE, &pf );
		sLabel = g_strdup_printf("%s %ss", tStr, pf);
		g_free ( tStr );
		break;
	case eSWP_PWR:
		sLabel = g_strdup_printf("%.3f dbm", value);
		break;
	case eSWP_LOGFREQ:
	case eSWP_LINFREQ:
	case eSWP_LSTFREQ:
	default:
		sLabel = doubleToStringWithSpaces( value / 1e6, "MHz" );
		break;
	}

	switch( justification ) {
	case eLeft:
	default:
		cairo_move_to(cr, x, y );
		cairo_show_text (cr, sLabel);
		break;
	case eRight:
		rightJustifiedCairoText( cr, sLabel, x, y );
		break;
	case eCenter:
		centreJustifiedCairoText( cr, sLabel, x, y );
		break;
	}
	g_free( sLabel );
}

/*!     \brief  Display the stimulus information
 *
 * Display the stimulus information on the X axis
 *
 * \param cr	     pointer to cairo structure
 * \param pGrid      pointer to grid structure
 * \param channel	 channel to show stimulus information
 * \param sTime   	 pointer to time string
 * \return           TRUE
 */
gboolean
showStimulusInformation (cairo_t *cr, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal) {

	gdouble logStart, logStop, centerStimulus, perDiv;
	gdouble posXstart, posXperDiv, posXcenter, posXspan, posXstop, posY;
	gchar *sLabel, *sOtherChLabel;

	tChannel *pChannel = &pGlobal->HP8753.channels[channel];

	// If we are coupled, overlaying and have already shown this .. don't do anything
	if( pGrid->overlay.bAny && pGrid->bSourceCoupled && channel != 0 )
		return TRUE;

	setTraceColor( cr, pGrid->overlay.bAny, channel );

	// x labels
	switch ( pChannel->sweepType ) {
	case eSWP_LINFREQ:
	case eSWP_LSTFREQ:
	default:
		centerStimulus = (pChannel->sweepStop-pChannel->sweepStart) / 2.0 + pChannel->sweepStart;
		break;
	case eSWP_LOGFREQ:
		logStart = log10( pChannel->sweepStart );
		logStop  = log10( pChannel->sweepStop );
	    centerStimulus = pow( 10.0, logStart + (logStop-logStart) / 2.0 );
		break;
	}
	perDiv = (pChannel->sweepStop-pChannel->sweepStart) / NHGRIDS;

	posXstop   = pGrid->areaWidth - pGrid->rightGridPosn;
	posXstart  = pGrid->leftGridPosn;
	posXcenter = pGrid->leftGridPosn + pGrid->gridWidth / 2.0;
	posXperDiv = pGrid->leftGridPosn + pGrid->gridWidth * 3.50 / 10;
	posXspan   = pGrid->leftGridPosn + pGrid->gridWidth * 7.75 / 10;
	posY = pGrid->bottomGridPosn - 1.4 * pGrid->fontSize;

	if( pGrid->overlay.bAny && !pGrid->bSourceCoupled && channel == eCH_TWO) {
		posY -= pGrid->lineSpacing;
	}

	cairo_save( cr ); {
		setCairoFontSize(cr, pGrid->fontSize);
		// Start label at the right
		// This is kind of messy...  if there are two source lines, then right align the start labels so that the longest is
		// left aligned at the start. If only one source line, simply left align
		if( pGrid->overlay.bAny && !pGrid->bSourceCoupled ) {
			sLabel = formatXaxisLabel ( pChannel->sweepStart, pChannel->sweepType );
			sOtherChLabel = formatXaxisLabel ( pGlobal->HP8753.channels[ (channel + 1) % eNUM_CH ].sweepStart, pChannel->sweepType );
			rightJustifiedCairoText(cr, sLabel, posXstart
					+ MAX( stringWidthCairoText( cr, sLabel), stringWidthCairoText( cr, sOtherChLabel) ), posY);
			g_free( sLabel );
			g_free( sOtherChLabel );
		} else {
			showXaxisLabel ( cr, pChannel->sweepStart, posXstart, posY, pChannel->sweepType, eLeft );
		}
		// Stop label at the right
		showXaxisLabel ( cr, pChannel->sweepStop, posXstop, posY, pChannel->sweepType, eRight );

		switch ( pGlobal->HP8753.channels[channel].sweepType ) {
		case eSWP_LOGFREQ:
		case eSWP_LINFREQ:
		case eSWP_LSTFREQ:
			if( pGrid->overlay.bAny && !pGrid->bSourceCoupled )
				showXaxisLabel ( cr, centerStimulus, posXcenter + pGrid->gridWidth * 1.0 / 10.0, posY, pChannel->sweepType, eRight );
			else
				showXaxisLabel ( cr, centerStimulus, posXcenter, posY, pChannel->sweepType, eCenter );
			break;
		case eSWP_CWTIME:
		case eSWP_PWR:
			if( pGrid->overlay.bAny && !pGrid->bSourceCoupled )
				showXaxisLabel ( cr, pChannel->CWfrequency, posXcenter + pGrid->gridWidth * 1.0 / 10.0, posY, pChannel->sweepType, eRight );
			else
				showXaxisLabel ( cr, pChannel->CWfrequency, posXcenter, posY, eSWP_LINFREQ, eCenter );
			break;
		default:
			break;
		}

		gchar label[ BUFFER_SIZE_100 ];

		cairo_select_font_face(cr, STIMULUS_LEGEND_FONT, CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL );
		setCairoFontSize(cr, pGrid->fontSize * 0.8); // slighly smaller font
		// if the sources are coupled or we do not have an overlay, use distinct color
		if( !pGrid->overlay.bAny || pGrid->bSourceCoupled )
		    gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorTextSpanPerDivCoupled   ] );

		if( pChannel->sweepType == eSWP_LINFREQ &&
				( pChannel->format != eFMT_SMITH && pChannel->format != eFMT_POLAR)) {
			g_snprintf(label, 20, "%g MHz/div", perDiv / 1.0e6);
			rightJustifiedCairoText( cr, label, posXperDiv, posY );
		}

		gchar *sSpan = engNotation( pChannel->sweepStop - pChannel->sweepStart, 2, eENG_NORMAL, NULL);
		g_snprintf( label, BUFFER_SIZE_100, "%s%s%s",
				!pGrid->overlay.bAny || channel == eCH_ONE ? "Span: " : "",
				sSpan, sweepSymbols[pChannel->sweepType]);
		rightJustifiedCairoText( cr, label, posXspan, posY );
		g_free( sSpan );

	} cairo_restore( cr );

	return TRUE;
}

/*!     \brief  Display title and time
 *
 * Display title and time on plot
 *
 * \param cr	     pointer to cairo structure
 * \param pGrid      pointer to grid structure
 * \param sTitle	 pointer to title string
 * \param sTime   	 pointer to time string
 */
void
showTitleAndTime( cairo_t *cr, tGridParameters *pGrid, gchar *sTitle, gchar *sTime)
{
	cairo_save( cr ); {
		cairo_reset_clip( cr );
		cairo_set_matrix (cr, &pGrid->initialMatrix);
		gdk_cairo_set_source_rgba (cr, &plotElementColors[ eColorTextTitle ] );

		cairo_select_font_face(cr, LABEL_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
		setCairoFontSize(cr, pGrid->fontSize * 1.3); // initially 10 pixels

		cairo_move_to(cr, pGrid->leftGridPosn, pGrid->areaHeight-(pGrid->lineSpacing * 1.3) );
		cairo_show_text (cr, sTitle);

		cairo_select_font_face(cr, LABEL_FONT, CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
		setCairoFontSize(cr, pGrid->fontSize * 0.8); // initially 10 pixels
		rightJustifiedCairoText( cr,
				sTime, pGrid->areaWidth - pGrid->rightGridPosn,
				pGrid->areaHeight-(pGrid->lineSpacing * 1.0) );

	} cairo_restore( cr );
}

/*!     \brief  Find the linearly interpolated response to the stimulus
 *
 * Given a arbitary stimulus value (that may or may not correspond to a sample stimulus)
 * return the linearly interpolated response
 *
 * \param nStart	 start sample (of segment)
 * \param nEnd       end sample
 * \param pChannel	 pointer to channel structure
 * \param stimulus	 stimulus value
 * \return			 interpolated response
 */
gdouble
calculateSegmentLinearlyInterpolatedResponse( gint nStart, gint nEnd, tChannel *pChannel, gdouble stimulus ) {
    gint nHead = nStart, nTail = nEnd;
    gint nMid = (nHead + nTail) / 2;
    gdouble xFract = 0.0;

    while ( nHead <= nTail )  {
        if ( pChannel->stimulusPoints[nMid] < stimulus ) {
        	nHead = nMid + 1;
        } else if ( pChannel->stimulusPoints[nMid] == stimulus ) {
        	return pChannel->responsePoints[nMid].r;
        } else {
        	nTail = nMid - 1;
        }
        nMid = (nHead + nTail)/2;
    }

    if( nHead > nTail ) {
    	xFract = (stimulus - pChannel->stimulusPoints[nMid]) /
    			(pChannel->stimulusPoints[nMid+1] - pChannel->stimulusPoints[nMid]);
    	return LIN_INTERP( pChannel->responsePoints[nMid].r,
    				pChannel->responsePoints[nMid+1].r, xFract );
    }
    return 0.0;
}


/*!     \brief  Plot the first channel
 *
 * Draw the plot for the first channel onto either the drawing area or to another
 * cairo device (printing, image etc)
 *
 * \param areaWidth	 width (in points) of the cairo drawing area
 * \param areaHeight height (in points) of the cairo drawing area
 * \param margin     margin (in points )
 * \param cr		 pointer to cairo structure
 * \param pGlobal	 pointer to the global data structure
 * \return			 FALSE
 */
gboolean plotA (guint areaWidth, guint areaHeight, gdouble margin, cairo_t *cr, tGlobal *pGlobal)
{
    // If we have dual display and it is not split, we show both traces on this DrawingArea
    gboolean bOverlay = !(pGlobal->HP8753.flags.bShowHPGLplot && pGlobal->HP8753.flags.bHPGLdataValid) &&
    		( pGlobal->HP8753.flags.bDualChannel && !pGlobal->HP8753.flags.bSplitChannels );

    cairo_translate( cr, margin, margin );

    // adjust area to accomodate margin
    areaWidth  -= 2 * margin;
    areaHeight -= 2 * margin;

    tGridParameters grid = {.areaWidth = areaWidth, .areaHeight = areaHeight, .margin = margin, 0};

#if 0
    pGlobal->HP8753.channels[ 0 ].format = HP8753C_FMT_SMITH;
    pGlobal->HP8753.channels[ 0 ].scaleVal = 1.0;
#endif

	flipVertical( cr, &grid );

	if( pGlobal->HP8753.flags.bShowHPGLplot && pGlobal->HP8753.flags.bHPGLdataValid ) {
		// Screenshot from HPGL
		plotScreen ( cr, areaHeight, areaWidth, pGlobal);
	} else {
		// Plot derived from data
		determineGridPosition( cr, pGlobal, eCH_ONE, &grid );

		if( !pGlobal->HP8753.channels[ eCH_ONE ].chFlags.bValidData ) {
			// cairo_move_to( cr, stGrid.areaWidth * 0.2, stGrid.areaHeight * .50 );
			drawHPlogo ( cr, pGlobal->HP8753.sProduct, grid.areaWidth / 2.0, grid.areaHeight * .20, grid.fontSize / 18.0 );
			return TRUE;
		}

		showStatusInformation (cr, &grid, eCH_ONE, pGlobal);

		switch ( pGlobal->HP8753.channels[ 0 ].format ) {
		case eFMT_LOGM:
		case eFMT_PHASE:
		case eFMT_DELAY:
		case eFMT_LINM:
		case eFMT_REAL:
		case eFMT_IMAG:
		case eFMT_SWR:
			plotCartesianGrid(cr, &grid, eCH_ONE, pGlobal);
			break;
		case eFMT_SMITH:
			plotSmithGrid( cr, TRUE, &grid, eCH_ONE, pGlobal);
			break;
		case eFMT_POLAR:
			plotPolarGrid (cr, TRUE, &grid, eCH_ONE, pGlobal);
			break;
		}

		if ( bOverlay ) {
			showStatusInformation (cr, &grid, eCH_TWO, pGlobal);

			switch ( pGlobal->HP8753.channels[ eCH_TWO ].format ) {
			case eFMT_LOGM:
			case eFMT_PHASE:
			case eFMT_DELAY:
			case eFMT_LINM:
			case eFMT_REAL:
			case eFMT_IMAG:
			case eFMT_SWR:
				plotCartesianGrid(cr, &grid, eCH_TWO, pGlobal);
				break;
			case eFMT_SMITH:
				plotSmithGrid(cr, TRUE, &grid, eCH_TWO, pGlobal);
				break;
			case eFMT_POLAR:
				plotPolarGrid(cr, TRUE, &grid, 1, pGlobal);
				break;
			}
		}

		switch ( pGlobal->HP8753.channels[ eCH_ONE ].format ) {
		case eFMT_LOGM:
		case eFMT_PHASE:
		case eFMT_DELAY:
		case eFMT_LINM:
		case eFMT_REAL:
		case eFMT_IMAG:
		case eFMT_SWR:
			plotCartesianTrace (cr, &grid, eCH_ONE, pGlobal);
			break;
		case eFMT_SMITH:
		case eFMT_POLAR:
			plotSmithAndPolarTrace (cr, &grid, eCH_ONE, pGlobal);
			break;
		}

		if ( bOverlay ) {
			switch ( pGlobal->HP8753.channels[ eCH_TWO ].format ) {
			case eFMT_LOGM:
			case eFMT_PHASE:
			case eFMT_DELAY:
			case eFMT_LINM:
			case eFMT_REAL:
			case eFMT_IMAG:
			case eFMT_SWR:
				plotCartesianTrace (cr, &grid, eCH_TWO, pGlobal);
				break;
			case eFMT_SMITH:
			case eFMT_POLAR:
				plotSmithAndPolarTrace (cr, &grid, eCH_TWO, pGlobal);
				break;
			}
		}
	}
    return FALSE;
}

/*!     \brief  Signal received to draw the first drawing area
 *
 * Draw the plot for area A
 *
 * \param widget	pointer to GtkDrawingArea widget
 * \param cr		pointer to cairo structure
 * \param pGlobal	pointer to the global data structure
 * \return			FALSE
 */
gboolean CB_DrawingArea_A_Draw (GtkWidget *widget, cairo_t *cr, tGlobal *pGlobal)
{

	guint areaWidth   = gtk_widget_get_allocated_width (widget);
    guint areaHeight  = gtk_widget_get_allocated_height (widget);

//  gtk_render_background ( gtk_widget_get_style_context (widget), cr, 0, 0, areaWidth, areaHeight );

    if( pGlobal->HP8753.channels[ eCH_ONE ].chFlags.bValidData ||
    		(pGlobal->HP8753.flags.bShowHPGLplot && pGlobal->HP8753.flags.bHPGLdataValid)) {
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0 );
		cairo_paint( cr );
    }

    return plotA ( areaWidth,  areaHeight, 0, cr, pGlobal);
}

/*!     \brief  Plot the second channel
 *
 * Draw the plot for the second channel onto either the drawing area or to another
 * cairo device (printing, image etc)
 *
 * \param areaWidth	 width (in points) of the cairo drawing area
 * \param areaHeight height (in points) of the cairo drawing area
 * \param margin     margin (used for PDF and print)
 * \param cr		 pointer to cairo structure
 * \param pGlobal	 pointer to the global data structure
 * \return			 FALSE
 */
gboolean plotB (guint areaWidth, guint areaHeight, gdouble margin, cairo_t *cr, tGlobal *pGlobal)
{
    cairo_translate( cr, margin, margin );

    // adjust area to accomodate margin
    areaWidth  -= 2 * margin;
    areaHeight -= 2 * margin;

    tGridParameters grid = {.areaWidth = areaWidth, .areaHeight = areaHeight, .margin = margin, 0};

	if( !pGlobal->HP8753.channels[ eCH_TWO ].chFlags.bValidData )
		return FALSE;

	flipVertical( cr, &grid );

	determineGridPosition( cr, pGlobal, eCH_TWO, &grid );
	showStatusInformation (cr, &grid, eCH_TWO, pGlobal);

	switch ( pGlobal->HP8753.channels[ eCH_TWO ].format ) {
	case eFMT_LOGM:
	case eFMT_PHASE:
	case eFMT_DELAY:
	case eFMT_LINM:
	case eFMT_REAL:
	case eFMT_IMAG:
	case eFMT_SWR:
		plotCartesianGrid(cr, &grid, eCH_TWO, pGlobal);
		plotCartesianTrace (cr, &grid, eCH_TWO, pGlobal);
		break;
	case eFMT_SMITH:
        plotSmithGrid( cr, TRUE, &grid, eCH_TWO, pGlobal);
        plotSmithAndPolarTrace (cr, &grid, eCH_TWO, pGlobal);
        break;
	case eFMT_POLAR:
		plotPolarGrid( cr, TRUE, &grid, eCH_TWO, pGlobal);
		plotSmithAndPolarTrace (cr, &grid, eCH_TWO, pGlobal);
 		break;
	}

    return FALSE;
}

/*!     \brief  Signal received to draw the first drawing area
 *
 * Draw the plot for area B
 *
 * \param widget	pointer to GtkDrawingArea widget
 * \param cr		pointer to cairo structure
 * \param pGlobal	pointer to the global data structure
 * \return			FALSE
 */
gboolean
CB_DrawingArea_B_Draw (GtkWidget *widget, cairo_t *cr, tGlobal *pGlobal)
{
	// get with and height in points (1/72")
	guint areaWidth   = gtk_widget_get_allocated_width  (widget);
    guint areaHeight  = gtk_widget_get_allocated_height (widget);

    // clear the screen
	cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0 );
	cairo_paint( cr );

    return plotB ( areaWidth,  areaHeight, 0.0, cr, pGlobal);
}

/*!     \brief  Act on mouse movement, enty or exit into the drawing area
 *
 * Show dynamic marker based on mouse position in the plot area
 *
 * \param widget	pointer to GtkDrawingArea widget
 * \param event		pointer to GdkEvent for the mouse
 * \param pGlobal	pointer to the global data structure
 * \return			FALSE
 */
gboolean CB_DrawingArea_A_MouseAction(GtkWidget *widget, GdkEvent *event, tGlobal *pGlobal) {
	GdkEventMotion* e;
	GdkEventCrossing* ec;

	for( gint channel=0; channel < MAX_CHANNELS; channel++ )
		switch (event->type ) {
		case GDK_ENTER_NOTIFY:
			ec=(GdkEventCrossing*)event;
			pGlobal->mousePosition[channel].r = ec->x;
			pGlobal->mousePosition[channel].i = ec->y;
			break;
		case GDK_MOTION_NOTIFY:
			e=(GdkEventMotion*)event;
			pGlobal->mousePosition[channel].r = e->x;
			pGlobal->mousePosition[channel].i = e->y;
			break;
		case GDK_LEAVE_NOTIFY:
			pGlobal->mousePosition[channel].r = 0.0;
			pGlobal->mousePosition[channel].i = 0.0;
			break;
		default:
			break;
		}

	gtk_widget_queue_draw( widget );
	gtk_widget_queue_draw( GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DrawingArea_Plot_B") ));
	return FALSE;

}

/*!     \brief  Act on mouse movement, enty or exit into the drawing area
 *
 * Show dynamic marker based on mouse position in the plot area
 *
 * \param widget	pointer to GtkDrawingArea widget
 * \param event		pointer to GdkEvent for the mouse
 * \param pGlobal	pointer to the global data structure
 * \return			FALSE
 */
gboolean
CB_DrawingArea_B_MouseAction(GtkWidget *widget,  GdkEvent *event, tGlobal *pGlobal) {
	GdkEventMotion* e;
	GdkEventCrossing* ec;

	for( eChannel channel=eCH_ONE; channel < eNUM_CH; channel++ )
		switch (event->type ) {
		case GDK_ENTER_NOTIFY:
			ec=(GdkEventCrossing*)event;
			pGlobal->mousePosition[channel].r = ec->x;
			pGlobal->mousePosition[channel].i = ec->y;
			break;
		case GDK_MOTION_NOTIFY:
			e=(GdkEventMotion*)event;
			pGlobal->mousePosition[channel].r = e->x;
			pGlobal->mousePosition[channel].i = e->y;
			break;
		case GDK_LEAVE_NOTIFY:
			pGlobal->mousePosition[channel].r = 0.0;
			pGlobal->mousePosition[channel].i = 0.0;
			break;
		default:
			break;
		}

	gtk_widget_queue_draw( widget );
	gtk_widget_queue_draw( GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DrawingArea_Plot_A") ));
	return FALSE;

}

