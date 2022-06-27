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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>
#include <hp8753.h>
#include <math.h>

#include "smithChartPS.h"

#include "ghostscript/ierrors.h"
#include "ghostscript/iapi.h"

/* stdio functions */
static int GSDLLCALL
gsdll_stdin(void *instance, char *buf, int len)
{
    int ch;
    int count = 0;
    while (count < len) {
	ch = fgetc(stdin);
	if (ch == EOF)
	    return 0;
	*buf++ = ch;
	count++;
	if (ch == '\n')
	    break;
    }
    return count;
}

#undef GSDEBUG
static int GSDLLCALL
gsdll_stdout(void *instance, const char *str, int len)
{
#ifdef GSDEBUG
	fwrite(str, 1, len, stdout);
    fflush(stdout);
#endif
    return len;
}

static int GSDLLCALL
gsdll_stderr(void *instance, const char *str, int len)
{
#ifdef GSDEBUG
    fwrite(str, 1, len, stderr);
    fflush(stderr);
#endif
    return len;
}

static int gsRunStringCont( void *minst, gchar *string, int *pexit_code) {
	return gsapi_run_string_continue(minst, string, strlen(string), 0, pexit_code);
}

static gboolean showHRsmithStimulusInformation (void *minst, eChannel channel, tGlobal *pGlobal, gboolean bOverlay);
static void showHRsmithBandwidth( void *minst, tGlobal *pGlobal, eChannel channel, gboolean bOverlay );
static void drawSmithHRmarkers( void *minst, tGlobal *pGlobal, eChannel channel, gboolean bOverlay );
static gboolean showHRsmithStatusInformation (void *minst, eChannel channel, tGlobal *pGlobal, gboolean bOverlay);

gint
smithHighResPDF(tGlobal *pGlobal, gchar *filename, eChannel channel)
{
    gint code, code1, exit_code;
    gchar * gsargv[7];
    gint gsargc;
    gint npoints;
    void *minst = NULL;
    gchar sBuf[ BUFFER_SIZE_250 ];
    enum { eRX, eGB, eNone } eLastGrid = eNone;

    gboolean bOverlay = pGlobal->HP8753.flags.bDualChannel
    		&& pGlobal->HP8753.flags.bDualChannel
			&& pGlobal->HP8753.channels[ eCH_ONE ].format == eFMT_SMITH
			&& pGlobal->HP8753.channels[ eCH_TWO ].format == eFMT_SMITH
			&& channel == eCH_BOTH;
    gsargv[0] = "";
    gsargv[1] = "-dNOPAUSE";
    gsargv[2] = "-dBATCH";
    gsargv[3] = "-dSAFER";
    gsargv[4] = "-sDEVICE=pdfwrite";
    gsargv[5] = "-sOutputFile=out.pdf";
    gsargc=6;

	gsargv[5] = g_strdup_printf( "-sOutputFile=%s", filename );

	code = gsapi_new_instance(&minst, NULL);
	if (code < 0)
		return 1;
	code = gsapi_set_arg_encoding(minst, GS_ARG_ENCODING_UTF8);
	gsapi_set_stdio(minst, gsdll_stdin, gsdll_stdout, gsdll_stderr);
	if (code == 0)
		code = gsapi_init_with_args(minst, gsargc, gsargv);

	// The program
	gsapi_run_string(minst, smithPS, 0, &exit_code);

	for( eChannel chan = (channel != eCH_BOTH ? channel : 0); chan < eNUM_CH; chan++ ) {
		if( pGlobal->HP8753.channels[chan].chFlags.bAdmitanceSmith || pGlobal->flags.bAdmitanceSmith) {
			if( eLastGrid == eNone || eLastGrid == eRX )
				gsapi_run_string(minst, "false drawGrid", 0, &exit_code);
			eLastGrid = eGB;
		} else {
			if( eLastGrid == eNone || eLastGrid == eGB )
				gsapi_run_string(minst, "true drawGrid", 0, &exit_code);
			eLastGrid = eRX;
		}

		gsapi_run_string_begin (minst, 0, &exit_code);
		if( channel != eCH_BOTH )
			gsRunStringCont(minst, "0.0 0.0 0.0 setrgbcolor [ ",&exit_code);
		else if ( chan == 0 )	// dark green
			gsRunStringCont(minst, "0.00 0.40 0.00 setrgbcolor [ ",&exit_code);
		else	// dark blue
			gsRunStringCont(minst, "0.00 0.00 0.50 setrgbcolor [ ",&exit_code);

		npoints = pGlobal->HP8753.channels[chan].nPoints;

		for( int n=0; n < npoints; n++ ) {
			if( pGlobal->flags.bSmithSpline && n != 0 ) {
				// Helping variables for lines.
				tLine g, l;
				// Variables for control points.
				tComplex c1, c2;
			    g.A = pGlobal->HP8753.channels[chan].responsePoints[(n + npoints - 2) % npoints];
			    g.B = pGlobal->HP8753.channels[chan].responsePoints[(n + npoints - 1) % npoints];
			    l.A = pGlobal->HP8753.channels[chan].responsePoints[(n + npoints + 0) % npoints];
			    l.B = pGlobal->HP8753.channels[chan].responsePoints[(n + npoints + 1) % npoints];

			      // Calculate controls points for points pt[i-1] and pt[i].
			    bezierControlPoints(&g, &l, &c1, &c2);
			    // Handle special case because points are not connected in a loop
			    if (n == 1) c1 = g.B;
			    if (n == npoints - 1) c2 = l.A;
				g_snprintf( sBuf, BUFFER_SIZE_250, "%e %e  %e %e  %e %e ",
						c1.r, c1.i, c2.r, c2.i,
						pGlobal->HP8753.channels[chan].responsePoints[n].r,
						pGlobal->HP8753.channels[chan].responsePoints[n].i );
			} else {
				g_snprintf( sBuf, BUFFER_SIZE_250, "%e %e ",
					pGlobal->HP8753.channels[chan].responsePoints[n].r,
					pGlobal->HP8753.channels[chan].responsePoints[n].i );
			}
			gsRunStringCont( minst, sBuf, &exit_code );
		}

		gsRunStringCont( minst, pGlobal->flags.bSmithSpline ?
				"] true  traceUV " : "] false traceUV ",
				&exit_code );
		gsapi_run_string_end( minst, 0, &exit_code );
		showHRsmithStimulusInformation (minst, chan, pGlobal, bOverlay);
		drawSmithHRmarkers( minst, pGlobal, chan, bOverlay );

		if( pGlobal->HP8753.sTitle ) {
			g_snprintf( sBuf, BUFFER_SIZE_250, "(%s) showTitle", pGlobal->HP8753.sTitle );
			gsapi_run_string(minst, sBuf, 0, &exit_code);
		}

		if( pGlobal->flags.bShowDateTime ) {
			g_snprintf( sBuf, BUFFER_SIZE_250, "(%s) showDate", pGlobal->HP8753.dateTime );
			gsapi_run_string(minst, sBuf, 0, &exit_code);
		}
		if( pGlobal->HP8753.channels[ chan ].chFlags.bBandwidth )
			showHRsmithBandwidth(minst, pGlobal, chan, bOverlay );

		showHRsmithStatusInformation (minst, chan, pGlobal, bOverlay);

		if( !bOverlay || chan == eCH_TWO )
			gsapi_run_string( minst, "showpage ", 0, &exit_code );

		if( channel != eCH_BOTH )
			break;
	}

	code1 = gsapi_exit( minst );
	if ((code == 0) || (code == gs_error_Quit))
		code = code1;

	gsapi_delete_instance(minst);


    if ((code == 0) || (code == gs_error_Quit))
        return 0;
    return 1;
}

void
drawSmithHRmarkerText( void *minst,
		tGlobal *pGlobal,
		eChannel channel, gboolean bOverlay, gint mkrNo,
		gboolean bActive, gint nPosition,
		gdouble stimulus, gdouble value1, gdouble value2) {

	gchar *sValue1 = NULL, *sValue2 = NULL, *sStimulus = NULL, *sPrefix1, *sPrefix2, *sPrefixStimulus;
	tChannel *pChannel = &pGlobal->HP8753.channels[ channel ];
	gchar mkrTextPSstring[ BUFFER_SIZE_250 ];
	gint exit_code;

	if( pChannel->mkrType == eMkrLog ) {
		sValue1 = engNotation(value1, 3, eENG_SEPARATE, &sPrefix1);
	} else {
		sValue1 = g_strdup_printf( "%.2f", value1);
		sPrefix1 = "";
	}

	// Value 2
	sValue2 = engNotation(value2, 3, eENG_SEPARATE, &sPrefix2);

	// Stimulus
	if( pChannel->sweepType <= eSWP_LSTFREQ && bActive ) {
		sStimulus = doubleToStringWithSpaces( stimulus/1e6, NULL );
		sPrefixStimulus="M";
	} else {
		sStimulus = engNotation(stimulus, 3, eENG_SEPARATE, &sPrefixStimulus);
	}


	g_snprintf( mkrTextPSstring, BUFFER_SIZE_250, "%d %d (%s) (%s) (%s) (%s) %d (%s) (%s) %d markerText",
			mkrNo, bOverlay ? channel : 0, sValue1, sPrefix1, sValue2, sPrefix2, pChannel->mkrType, sStimulus,
			sPrefixStimulus, pChannel->sweepType );
	gsapi_run_string( minst, mkrTextPSstring, 0, &exit_code );

	g_free( sValue1 );
	g_free( sValue2 );
	g_free( sStimulus );

	if( pChannel->chFlags.bMkrsDelta && nPosition == 0 ) {
		g_snprintf( mkrTextPSstring, BUFFER_SIZE_250, "%d %d markerDeltaText", channel, pChannel->deltaMarker );
		gsapi_run_string( minst, mkrTextPSstring, 0, &exit_code );
	}
}


// Coordinates are translated to:
//		0,0 is bottom left of grid (cartesian) or center Polar & Smith
// Scaling for Y or radius has already occured.
static void
drawSmithHRmarkers( void *minst, tGlobal *pGlobal, eChannel channel, gboolean bOverlay ) {
	// now get the marker source values and response values
	gint mkrNo = 0, flagBit, nMkrsShown;
	gdouble	stimulus, valueR, valueI, prtStimulus, prtValueR, prtValueI;
	gdouble X=0.0, Y=0.0;
	tChannel *pChannel = &pGlobal->HP8753.channels[ channel ];
	gchar *mkrLabels[] = {"1", "2", "3", "4", ""};
	gchar mkrSymbolPSstring[ BUFFER_SIZE_250 ];
	gdouble bFixedMarker = FALSE;
	gboolean bActiveShown;
	gint exit_code;

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

			extern void smithOrPolarMarkerToXY( gdouble V1, gdouble V2, gdouble *gammaR, gdouble *gammaI, tMkrType eFormat );
			smithOrPolarMarkerToXY( valueR, valueI, &X, &Y, pChannel->mkrType );

		    if( !bFixedMarker ) {
		    	gint mkrTextPosn;
		    	g_snprintf( mkrSymbolPSstring, BUFFER_SIZE_250, "%g %g (%s) %s %s markerSymbol\n", X, Y, mkrLabels[mkrNo],
		    			(pChannel->chFlags.bMkrsDelta && mkrNo == pChannel->deltaMarker) ? "true" : "false",
		    			(mkrNo == pChannel->activeMarker ? eACTIVE_MKR : eNONACTIVE_MKR) ? "false" : "true" );
		    	gsapi_run_string( minst, mkrSymbolPSstring, 0, &exit_code );

				// Show marker details on screen
				if( bActiveShown )
					mkrTextPosn = nMkrsShown;
				else if ( mkrNo == pChannel->activeMarker )
					mkrTextPosn = 0;
				else
					mkrTextPosn = nMkrsShown + 1;

				if( pGlobal->flags.bDeltaMarkerZero
						&&mkrNo == pChannel->activeMarker
						&& pChannel->chFlags.bMkrsDelta
						&& mkrNo == pChannel->deltaMarker ) {
					drawSmithHRmarkerText( minst, pGlobal, channel, bOverlay, mkrNo,
							mkrNo == pChannel->activeMarker,
							mkrTextPosn, 0.0, 0.0, 0.0);
				} else {
					drawSmithHRmarkerText( minst, pGlobal, channel, bOverlay, mkrNo,
							mkrNo == pChannel->activeMarker,
							mkrTextPosn, prtStimulus, prtValueR, prtValueI);
				}

				if ( mkrNo == pChannel->activeMarker )
					bActiveShown = TRUE;
				nMkrsShown++;
		    } else {
		    	g_snprintf( mkrSymbolPSstring, BUFFER_SIZE_250, "%g %g (%s) %s %s markerSymbol", X, Y, mkrLabels[mkrNo],
		    			(pChannel->chFlags.bMkrsDelta && mkrNo == pChannel->deltaMarker) ? "true" : "false",
		    			(mkrNo == pChannel->activeMarker ? eACTIVE_MKR : eNONACTIVE_MKR) ? "false" : "true" );
		    	gsapi_run_string( minst, mkrSymbolPSstring, 0, &exit_code );

		    }
		}
	}

	if( pChannel->chFlags.bBandwidth ) {
		/*
		drawBandwidthText( cr, pGlobal, pGrid, channel );
		*/
	}
}

static gboolean
showHRsmithStimulusInformation (void *minst, eChannel channel, tGlobal *pGlobal, gboolean bOverlay) {

	gdouble logStart, logStop, center;
	tChannel *pChannel = &pGlobal->HP8753.channels[channel];

	gchar *startPSstring = NULL;
	gchar *centerPSstring = NULL;
	gchar *stopPSstring = NULL;
	gchar stimulusTextPSstring[ BUFFER_SIZE_250 ];
	gchar *pf, *tStr;
	gint exit_code;

	// If we are coupled, overlaying and have already shown this .. don't do anything
	if( bOverlay && pGlobal->HP8753.flags.bSourceCoupled && channel != 0 )
		return TRUE;

	// x labels (frequency)
	switch ( pChannel->sweepType ) {
	case eSWP_LINFREQ:
	case eSWP_LSTFREQ:
	default:
		center = (pChannel->sweepStop-pChannel->sweepStart) / 2.0 + pChannel->sweepStart;
		break;
	case eSWP_LOGFREQ:
		logStart = log10( pChannel->sweepStart );
		logStop  = log10( pChannel->sweepStop );
	    center = pow( 10.0, logStart + (logStop-logStart) / 2.0 );
		break;
	}

	switch ( pChannel->sweepType ) {
	case eSWP_CWTIME:
		tStr = engNotation( pChannel->sweepStart, 2, eENG_SEPARATE, &pf );
		startPSstring = g_strdup_printf("%s %ss", tStr, pf);
		g_free ( tStr );
		tStr = engNotation( pChannel->sweepStop, 2, eENG_SEPARATE, &pf );
		stopPSstring = g_strdup_printf("%s %ss", tStr, pf);
		g_free ( tStr );
		centerPSstring = doubleToStringWithSpaces( pChannel->CWfrequency / 1e6, "MHz" );
		break;
	case eSWP_PWR:
		startPSstring = g_strdup_printf("%.3f dbm", pChannel->sweepStart);
		stopPSstring = g_strdup_printf("%.3f dbm", pChannel->sweepStop);
		centerPSstring = doubleToStringWithSpaces( pChannel->CWfrequency / 1e6, "MHz" );
		break;
	case eSWP_LOGFREQ:
	case eSWP_LINFREQ:
	case eSWP_LSTFREQ:
	default:
		startPSstring = doubleToStringWithSpaces( pChannel->sweepStart/1.0e6, "MHz" );
		stopPSstring = doubleToStringWithSpaces( pChannel->sweepStop/1.0e6, "MHz" );
		centerPSstring = doubleToStringWithSpaces( center/1.0e6, "MHz");
		break;
	}

	g_snprintf( stimulusTextPSstring, BUFFER_SIZE_250, "(%s) (%s) (%s) %d %d %s stimulusText",
			startPSstring, stopPSstring, centerPSstring, pChannel->sweepType, bOverlay ? channel : 0,
			pGlobal->HP8753.flags.bSourceCoupled ? "true" : "false");

	gsapi_run_string( minst, stimulusTextPSstring, 0, &exit_code );

	g_free( startPSstring );
	g_free( stopPSstring );
	g_free( centerPSstring );

	return TRUE;
}

static void
showHRsmithBandwidth( void *minst, tGlobal *pGlobal, eChannel channel, gboolean bOverlay ) {
	gchar *sPrefix;
	tChannel *pChannel = &pGlobal->HP8753.channels[ channel ];
	gchar sWidthUnits[ INFO_LEN ];
	gchar sCenterUnits[ INFO_LEN ];
	gchar sQ[ INFO_LEN ];
	gchar sPSstring[ BUFFER_SIZE_250 ];
	gint exit_code;


	gchar *sWidth, *sCenter;
	// width
	sWidth = engNotation(pChannel->bandwidth[ BW_WIDTH ], 3, eENG_SEPARATE, &sPrefix);
	g_snprintf( sWidthUnits, INFO_LEN, " %s%s", sPrefix, "Hz");

	// Center freq
	sCenter = engNotation(pChannel->bandwidth[ BW_CENTER ], 3, eENG_SEPARATE, &sPrefix);
	g_snprintf( sCenterUnits, INFO_LEN, " %s%s", sPrefix, "Hz");


	// Q
	g_snprintf( sQ, INFO_LEN, " %.3f", pChannel->bandwidth[ BW_Q ]);

	g_snprintf( sPSstring, BUFFER_SIZE_250, "(%s) (%s) (%s) (%s) (%s) %d bandwidthText",
			sWidth, sWidthUnits, sCenter, sCenterUnits, sQ, bOverlay ? channel : 0 );

	gsapi_run_string( minst, sPSstring, 0, &exit_code );


	g_free( sCenter );
	g_free( sWidth );

}

/*!     \brief  Display some channel setting at the top of the display
 *
 * Show the channel settings at the top of the display.
 *
 * \ingroup drawing
 *
 * \param minst		pointer ghostscript instance
 * \param channel	channel
 * \param pGlobal	pointer to global data
 * \param bOverlay	TRUE if overlaying two smith plots
 *
 * \return TRUE for success
 */
static gboolean
showHRsmithStatusInformation (void *minst, eChannel channel, tGlobal *pGlobal, gboolean bOverlay)
{
	tChannel *pChannel = &pGlobal->HP8753.channels[channel];
	gchar sPSstring[ BUFFER_SIZE_250 ];
	gint exit_code;

	gchar *sIFBW = engNotation( pGlobal->HP8753.channels[channel].IFbandwidth, 0, eENG_NORMAL, NULL );

	g_snprintf( sPSstring, BUFFER_SIZE_250, "(%s) (%s) %d statusText",
			optMeasurementType[ pChannel->measurementType ].desc,
			sIFBW, bOverlay ? channel : 0 );
	gsapi_run_string( minst, sPSstring, 0, &exit_code );

	g_free( sIFBW );

	return TRUE;
}

