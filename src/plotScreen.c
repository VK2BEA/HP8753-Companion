/*
 * HPGLplot.c
 *
 *  Created on: Nov 13, 2023
 *      Author: michael
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <glib-2.0/glib.h>
#include <errno.h>
#include <hp8753.h>

#include "messageEvent.h"

#include "HPGLplot.h"

/*!     \brief  Parse an HPGL command
 *
 * Parse an HPGL command and prepare data for plotting.
 * We create a compiled serial data set for plotting....
 * NNNNNNNN - byte count (total count of bytes for the data)
 * Line         - CHPGL_LINE2PT - identifier byte
 *                NN          - 16 bit count of bytes in line (n)
 *                NN          - 16 bit x1 position
 *                NN          - 16 bit y1 position
 *                x & y repeated for coordinate 2 to n (min of three points)
 * 2 point Line - CHPGL_LINE2PT - identifier byte
 *                NN          - 16 bit x1 position
 *                NN          - 16 bit y1 position
 *                NN          - 16 bit x2 position
 *                NN          - 16 bit y2 position
 * label        - CHPGL_LABEL or CHPGL_LABEL_REL - identifier byte
 *                NN          - 16 bit x position
 *                NN          - 16 bit y position
 *                NN          - 16 bit byte count of string (including trailling null)
 *                SSSSSSS.... - variable length string (including terminating by NULL)
 * lable size   - HPGL_CHAR_SIZE_REL - identifier byte
 *                NNNN        - float character size scaling in x (percentage of P2-P1 x)
 *                NNNN        - float character size scaling in y (percentahge of P2-P1 y)
 * pen select   - HPGL_SELECT_PEN - identifier byte
 *                N           - 8 bits identifying the pen (colour)
 * line type    - HPGL_LINE_TYPE - identifier byte
 *                N           - 8 bits identifying the line type (AFAICT this does not change)
 *
 *
 * \param  sHPGL	pointer to HPGL snippet
 * \return 0 (OK) or -1 (ERROR)
 */
gboolean
parseHPGL( gchar *sHPGL, tGlobal *pGlobal ) {
	static gboolean bPenDown = FALSE;
	static tCoord posn = {0};
	static gfloat charSizeX = 0.0, charSizeY = 0.0;
	static gint colour = 0, lineType = 0;
	guchar *secondHPGLcmd = NULL;
	// If a line is started .. we add to it
	static tCoord *currentLine = 0;
	static gint	nPointsInLine = 0;
	static gboolean	bNewPosition = FALSE;
	struct scanArrow {
		eHPGL code1;     // code CHPGL_LINE2PT
	    tCoord vert1, vert2;
	    eHPGL code2;    // code CHPGL_LINE
	    guint16 nPts;
	    tCoord arrowHead1, arrowHead2, addowHead3;
	} __attribute__((packed)) ;
	static const struct scanArrow upperScanArrow = {
	        CHPGL_LINE2PT, {77, 2432}, {77, 2492 },
	        CHPGL_LINE, 3, {65, 2474}, {77, 2492}, {88, 2474} };
	static const struct scanArrow lowerScanArrow = {
	        CHPGL_LINE2PT, {77, 384}, {77, 444 },
            CHPGL_LINE, 3, {65, 426}, {77, 444}, {88, 426}
	};

	static gint scaleX = HPGL_MAX_X;
	static gint scaleY = HPGL_MAX_Y;
    static gint scalePtX = HPGL_P1P2_X;
    static gint scalePtY = HPGL_P1P2_Y;

	gint temp1=0, temp2=0;

	// selecting pen 0 (white) indicates end of plot
	static gboolean bPresumedEnd = FALSE;

	// number of bytes used in the malloced memory
	guint HPGLserialCount;
	gint nArgs;
	gint strLength;

	// Initialize the serialized & compiled HPGL plot
	if( sHPGL == NULL ) {
		// free any previously malloced data
		g_free( pGlobal->HP8753.plotHPGL );
		pGlobal->HP8753.plotHPGL = NULL;

		// abandon any open line
		g_free( currentLine );
		nPointsInLine = 0;
		currentLine = NULL;

		bPresumedEnd = FALSE;
		// assume the pen is up
		bPenDown = FALSE;
		return 0;
	}

	// all our HPGL commands are two ACSII caharcters
	if( strlen(sHPGL) < 2 )
		return 0;

	if( pGlobal->HP8753.plotHPGL )
		HPGLserialCount = *(guint *)(pGlobal->HP8753.plotHPGL);
	else
		HPGLserialCount = sizeof( guint );	// byte count at the beginning of malloced string

	switch( ((guchar)sHPGL[0] << 8) + (guchar)sHPGL[1] ) {	// concatenate the two bytes
	case HPGL_POSN_ABS:
		// some PA lines also contain other commands so malloc memory
		// to use in such a case to hold the second command
		// e.g.: PA3084 ,2414 SR 1.472 , 2.279 ;
		//       PA3444 ,736 SP1;
		secondHPGLcmd = g_malloc0( strlen( sHPGL ) );
		// so this will work to capture all of the trailing string.
		// Normally %s will stop at a whitespace in sscanf so we use "not 0200" (everything)
		nArgs = sscanf(sHPGL+2, "%hd , %hd %[^\0200]", &posn.x, &posn.y, secondHPGLcmd);

		if( bPenDown ) {
			// make sure we have enough space .. quantized by 100 points
			currentLine = g_realloc_n( currentLine, QUANTIZE(nPointsInLine + 1, 100) + sizeof(guint16), sizeof( tCoord ) );
			currentLine[ nPointsInLine ] = posn;
			nPointsInLine++;
		}
		// we have more than just x and y .. i.e another command on the same lineHLD_LBL_YPOS_CH1
		// I don't think this should occur with HPGL ... but there you have it
		if( nArgs == 3 ) {
			*(guint *)(pGlobal->HP8753.plotHPGL) = HPGLserialCount;
			bPresumedEnd |= parseHPGL( (gchar *)secondHPGLcmd, pGlobal );
			HPGLserialCount = *(guint *)(pGlobal->HP8753.plotHPGL);
		}
		g_free( secondHPGLcmd );
		bNewPosition = TRUE;
		break;
	case HPGL_LABEL:
		strLength = strlen( sHPGL+2 );
		if( strLength == 0 )
			break;	// don't bother adding null labels ("LB;")

#define HLD_LBL_YPOS_CH2	384
#define HLD_LBL_YPOS_CH1	2432
		// Dont show the Hld if we are not in hold
		if( g_str_has_prefix( sHPGL, "LBHld\003" )  && posn.x == 0 ) {
		    if( posn.y == HLD_LBL_YPOS_CH1 ) {
		        if( ( pGlobal->HP8753.flags.bDualChannel && !pGlobal->HP8753.channels[ eCH_ONE ].chFlags.bSweepHold )
		                || ( !pGlobal->HP8753.flags.bDualChannel &&
		                        !pGlobal->HP8753.channels[ pGlobal->HP8753.activeChannel ].chFlags.bSweepHold ) ) {
		            // show the scan arrow instead of 'Hld'
		            // allocate more space if needed
		            pGlobal->HP8753.plotHPGL = g_realloc( pGlobal->HP8753.plotHPGL,
		                    QUANTIZE( HPGLserialCount + sizeof( struct scanArrow ), 1000 ) );
		            memcpy( pGlobal->HP8753.plotHPGL + HPGLserialCount, &upperScanArrow, sizeof (struct scanArrow ));
		            HPGLserialCount += sizeof (struct scanArrow );
		            break;
		        }
		    } else if( !pGlobal->HP8753.channels[ eCH_TWO ].chFlags.bSweepHold ) {
                // show the scan arrow instead of 'Hld'
                // allocate more space if needed
                pGlobal->HP8753.plotHPGL = g_realloc( pGlobal->HP8753.plotHPGL,
                        QUANTIZE( HPGLserialCount + sizeof( struct scanArrow ), 1000 ) );
                memcpy( pGlobal->HP8753.plotHPGL + HPGLserialCount, &lowerScanArrow, sizeof (struct scanArrow ));
                HPGLserialCount += sizeof (struct scanArrow );
                break;
            }
		}

		// labels from the 8753 have 003 characters .. remove them
		if( sHPGL[ 2 + strLength - 1 ] == HPGL_LINE_TERMINATOR_CHARACTER )
			sHPGL[ 2 + strLength - 1 ] = 0;
		// allocate more space if needed
		pGlobal->HP8753.plotHPGL = g_realloc( pGlobal->HP8753.plotHPGL,
				QUANTIZE( HPGLserialCount + sizeof(eHPGL) + sizeof(tCoord) + sizeof( guchar ) + strLength, 1000 ) );
		// insert the label identifier
		*(eHPGL *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = bNewPosition ? CHPGL_LABEL:CHPGL_LABEL_REL;
		HPGLserialCount += sizeof(eHPGL);
		// insert the location
		*(tCoord *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = posn;
		HPGLserialCount += sizeof(tCoord);
		// next the string length (max 255 characters)
		*(guchar *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = (guchar) strLength;
		HPGLserialCount += sizeof(guchar);
		// copy the string (and the null)
		memcpy( pGlobal->HP8753.plotHPGL + HPGLserialCount, sHPGL + 2, strLength+1);
		HPGLserialCount += strLength+1;
		bNewPosition = FALSE;
		break;
	case HPGL_PEN_UP:
		if( bPenDown ) {
			// End of a line ...
			// this concludes the line..  Add the accumulated line points to the compiled HPGL serial store
			// enlarge the buffer .. g_realloc will only do this if necessary
			pGlobal->HP8753.plotHPGL = g_realloc( pGlobal->HP8753.plotHPGL,
					QUANTIZE( HPGLserialCount + sizeof(eHPGL) + sizeof( guint16 ) + (nPointsInLine * sizeof(tCoord)), 1000 ) );
			// insert the compiled HPGL command (either CHPGL_LINE2PT or CHPGL_LINE)
			if( nPointsInLine == 2 ) {
				// if it just a two point line, we don't save the number of points .. its implicit
				*(eHPGL *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = CHPGL_LINE2PT;
				HPGLserialCount += sizeof(eHPGL);
			} else {
				*(eHPGL *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = CHPGL_LINE;
				HPGLserialCount +=sizeof(eHPGL);
				// next save the number of points in the line
				*(guint16 *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = nPointsInLine;
				HPGLserialCount += sizeof( guint16 );
			}
			memcpy( pGlobal->HP8753.plotHPGL + HPGLserialCount, currentLine, nPointsInLine * sizeof(tCoord) );
			HPGLserialCount += (nPointsInLine * sizeof(tCoord));
		}

		// We have completed the line, so dispose of the malloced memory
		g_free( currentLine );
		nPointsInLine = 0;
		currentLine = NULL;
		// Remember that the pen is up
		bPenDown = FALSE;
		break;
	case HPGL_PEN_DOWN:
		// assume we are starting a new line and save the start point
		if( !bPenDown ) {
			currentLine = g_realloc_n( currentLine, QUANTIZE(1, 100), sizeof( tCoord ) );
			currentLine[0] = posn;
			nPointsInLine = 1;
			bPenDown = TRUE;
		}
		break;
	case HPGL_CHAR_SIZE_REL:
		sscanf(sHPGL+2, "%f , %f", &charSizeX, &charSizeY);
		pGlobal->HP8753.plotHPGL = g_realloc( pGlobal->HP8753.plotHPGL,
				QUANTIZE( HPGLserialCount + sizeof(eHPGL) + (2 * sizeof(gfloat)), 1000 ) );
		// add the text size change to the compiled HPGL serialized string
		*(eHPGL *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = CHPGL_TEXT_SIZE;
		HPGLserialCount += sizeof(eHPGL);
		*(gfloat *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = charSizeX;
		HPGLserialCount += sizeof( gfloat );
		*(gfloat *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = charSizeY;
		HPGLserialCount += sizeof( gfloat );
		break;
	case HPGL_LINE_TYPE:
		sscanf(sHPGL+2, "%d", &lineType);
		pGlobal->HP8753.plotHPGL = g_realloc( pGlobal->HP8753.plotHPGL,
				QUANTIZE( HPGLserialCount + sizeof(eHPGL) + sizeof(guchar), 1000 ) );
		*(eHPGL *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = CHPGL_LINETYPE;
		HPGLserialCount += sizeof(eHPGL);
		*(guchar *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = (guchar)lineType;
		HPGLserialCount += sizeof( gchar );
		break;
	case HPGL_SELECT_PEN:
		sscanf(sHPGL+2, "%d", &colour);
		// bizarrely there is, occasionally, a pen change while the pen is down..
		// so close the current line (so the old color will be used when it is stroked)
		// and start a new line from the current point
		if( bPenDown ) {
            bPresumedEnd |= parseHPGL( "PU", pGlobal );
            HPGLserialCount = *(guint *)(pGlobal->HP8753.plotHPGL);
            bPresumedEnd |= parseHPGL( "PD", pGlobal );
		}
		pGlobal->HP8753.plotHPGL = g_realloc( pGlobal->HP8753.plotHPGL,
				QUANTIZE( HPGLserialCount + sizeof(eHPGL) + sizeof(gchar), 1000 ) );
		*(eHPGL *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = CHPGL_PEN;
		HPGLserialCount += sizeof(eHPGL);
		*(gchar *)(pGlobal->HP8753.plotHPGL + HPGLserialCount) = (gchar)colour;
		HPGLserialCount += sizeof( gchar );
		if( colour == 0 && posn.x == 0 )
		    bPresumedEnd = TRUE;
		break;
	case HPGL_SCALING_PTS:
        sscanf(sHPGL+2, "%d,%d,%d,%d", &temp1, &scalePtX, &temp2, &scalePtY);
        scalePtX -= temp1;
        scalePtY -= temp2;
        break;
	    break;
	case HPGL_SCALING:
	    sscanf(sHPGL+2, "%d,%d,%d,%d", &temp1, &scaleX, &temp2, &scaleY);
	    scaleX -= temp1;
	    scaleY -= temp2;
	    break;
    case HPGL_VELOCITY:
    case HPGL_INPUT_MASK:
	case HPGL_DEFAULT:
	case HPGL_PAGE_FEED:
	default:
		break;
	}

	// update the count
	if( pGlobal->HP8753.plotHPGL )
		*(guint *)(pGlobal->HP8753.plotHPGL) = HPGLserialCount;

	return bPresumedEnd;
}

/*!     \brief  Display the 8753 screen image
 *
 * If the plot is polar, draw the grid and legends.
 * If there is an overlay of two polar grids of the same scale show both,
 * otherwise actual grid is only drawn once.
 *
 * \ingroup drawing
 *
 * \param cr		pointer to cairo context
 * \param pGrid		pointer to grid parameters
 * \param pGlobal	pointer to global data
 * \return			TRUE
 *
 */
#define ASPECT_CORRECTION 1.070
gboolean
plotScreen (cairo_t *cr, guint areaHeight, guint areaWidth, tGlobal *pGlobal)
{

	if( pGlobal->HP8753.plotHPGL ) {
		guint HPGLserialCount = 0;
		static gfloat charSizeX = 1.0, charSizeY = 1.0;
		guint length = *((guint *)pGlobal->HP8753.plotHPGL);
		gint HPGLpen = 0;
		gint ptsInLine;
		cairo_matrix_t matrix;
		__attribute__((unused)) gint HPGLlineType = 0;

		double scaleX, scaleY;
		// There should not be a need for aspect correction but without it
		// circles are ovoid 8-(
		scaleX = (gdouble)areaWidth / (gdouble)HPGL_MAX_X * ASPECT_CORRECTION;
		scaleY = (gdouble)areaHeight / (gdouble)HPGL_MAX_Y;

		// Better center the plot in the screen
		double leftOffset  = areaWidth   /  25.0;
		double bottomOffset = areaHeight / 100.0;

		tCoord *pPoint;
		gint i;

		// There is always a character count at the head of the data
		HPGLserialCount += sizeof(guint);

		// Use a font that is monospaced (like the HP vector plotter)


	    cairo_save(cr);
	    {
			// Noto Sans Mono Light
	        cairo_select_font_face(cr, HPGL_FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

			// If we don't set the color its black ... but the HP8753 does
			gdk_cairo_set_source_rgba (cr, &HPGLpens[1] );      // black pen by default
			cairo_set_line_width (cr, areaWidth / 1000.0 * 0.75);

			do {
				// get compiled HPGL command byte
				eHPGL cmd = *((guchar *)(pGlobal->HP8753.plotHPGL+HPGLserialCount)) ;
				HPGLserialCount += sizeof( eHPGL );

				switch ( cmd ) {
				case CHPGL_LINE:
					// the points in the line are preceded by the point count
					ptsInLine = *((guint16 *)(pGlobal->HP8753.plotHPGL+HPGLserialCount));
					HPGLserialCount += sizeof( guint16 );
					pPoint = (tCoord *)(pGlobal->HP8753.plotHPGL+HPGLserialCount);
					cairo_new_path( cr );
					// move to first point
					cairo_move_to(cr, leftOffset + pPoint->x * scaleX, bottomOffset + pPoint->y * scaleY );
					// plot to each subsequent point
					for( i=1, pPoint++; i < ptsInLine; pPoint++, i++ ) {
						cairo_line_to(cr, leftOffset + pPoint->x * scaleX, bottomOffset + pPoint->y * scaleY );
					}
					cairo_stroke( cr );

					HPGLserialCount += (ptsInLine * sizeof( tCoord ));
					break;
				case CHPGL_LINE2PT:
					pPoint = (tCoord *)(pGlobal->HP8753.plotHPGL+HPGLserialCount);
					cairo_new_path( cr );
					// move to first point
					cairo_move_to(cr, leftOffset + pPoint->x * scaleX, bottomOffset + pPoint->y * scaleY );
					pPoint++;
					cairo_line_to(cr, leftOffset + pPoint->x * scaleX, bottomOffset + pPoint->y * scaleY );
					cairo_stroke( cr );

					HPGLserialCount += (2 * sizeof( tCoord ));
					break;
				case CHPGL_PEN:
					HPGLpen = *(guchar *)(pGlobal->HP8753.plotHPGL + HPGLserialCount);
					HPGLserialCount += sizeof( guchar );
					gdk_cairo_set_source_rgba (cr, &HPGLpens[ HPGLpen < NUM_HPGL_PENS ? HPGLpen : 1 ] );
					break;
				case CHPGL_LINETYPE:
					HPGLlineType = *(guchar *)(pGlobal->HP8753.plotHPGL + HPGLserialCount);
					HPGLserialCount += sizeof( guchar );
					break;
				case CHPGL_LABEL:
				case CHPGL_LABEL_REL:
					pPoint = (tCoord *)(pGlobal->HP8753.plotHPGL + HPGLserialCount);
					HPGLserialCount += sizeof( tCoord );
					if( cmd == CHPGL_LABEL )
						cairo_move_to(cr, leftOffset + pPoint->x * scaleX, bottomOffset + pPoint->y * scaleY );
					guint labelLength = *(guchar *)(pGlobal->HP8753.plotHPGL + HPGLserialCount);
					HPGLserialCount += sizeof( guchar );
					// label is null terminated
					gchar *pLabel = (gchar *)(pGlobal->HP8753.plotHPGL + HPGLserialCount);
					gchar *ptr = strchr( pLabel, '\b' );
					// If we have a backspace, then there is an underscore (number of marker)
					if( !ptr) {
						// no backspace so just print in one call
						cairo_show_text (cr, pLabel);
					} else {
						// if there is a backspace, assume an underscore and print the substring
						// up to the backspace, draw the underscore (as a line) and then the trailing substring
						gdouble x, y;
						gchar *front = g_strndup ( pLabel, ptr-pLabel);
						cairo_show_text (cr, front);
						cairo_get_current_point(cr, &x, &y);
						cairo_rel_move_to(cr, -charSizeX  * HPGL_P1P2_X * scaleX / 2000,
													-charSizeY * HPGL_P1P2_Y * scaleY / 500);
						cairo_rel_line_to(cr, -charSizeX  * HPGL_P1P2_X * scaleX / 200, 0);
						cairo_stroke(cr);
						cairo_move_to( cr, x, y);
						cairo_show_text (cr, ptr+2);
					}
					HPGLserialCount += labelLength+1;
					// display the label

					break;
				case CHPGL_TEXT_SIZE:
				    cairo_matrix_init_identity( &matrix );
					charSizeX = *(gfloat *)(pGlobal->HP8753.plotHPGL + HPGLserialCount);
					HPGLserialCount += sizeof( gfloat );
					charSizeY = *(gfloat *)(pGlobal->HP8753.plotHPGL + HPGLserialCount);
					HPGLserialCount += sizeof( gfloat );
					matrix.xx = charSizeX  * HPGL_P1P2_X * scaleX / 100.0;
					matrix.yy = -charSizeY * HPGL_P1P2_Y * scaleY / 112.0;  // Slighly reduce height compared with width
					cairo_set_font_matrix (cr, &matrix);
					break;
				default:
					break;
				}
			} while (HPGLserialCount < length);
		}
	} cairo_restore( cr );
	return TRUE;
}
