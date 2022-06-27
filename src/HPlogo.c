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
#include <cairo/cairo.h>
#include <hp8753.h>
#include <math.h>

#define LOGO_WIDTH 280.0
void
drawHPlogo (cairo_t *cr, gchar *sProduct, gdouble centreX, gdouble lowerLeftY, gdouble scale)
{
	cairo_matrix_t	fontMatrix;

	cairo_save( cr ); {
		// make the current point 0.0, 0.0
		cairo_translate( cr , centreX, lowerLeftY);


		// scale so that 1.0 is 100pt horizontally
		cairo_scale( cr, 2.83 * scale, -2.83 * scale );

		cairo_translate( cr , -LOGO_WIDTH/2.0, 0.0);

		// text part of the logo
		// we don't have the right font but we stretch this out a little
		// and it's close

		// setCairoColor( cr, eColorWhite );
		// cairo_rectangle(cr, -10.0, -35.0, LOGO_WIDTH+20, 48.0 );
		// cairo_fill( cr );

		setCairoColor( cr, eColorBlack );
        cairo_select_font_face(cr, HP_LOGO_FONT,
            CAIRO_FONT_SLANT_NORMAL,  CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size( cr, 10.0 );
		cairo_get_font_matrix ( cr, &fontMatrix);
		fontMatrix.xx *= 1.4;
		cairo_set_font_matrix ( cr, &fontMatrix);

        cairo_move_to( cr, 40.0, -14.0 );
        cairo_show_text ( cr, "HEWLETT" );
        cairo_move_to( cr, 40.0, 0.0 );
        cairo_show_text ( cr, "PACKARD" );

        cairo_set_font_size( cr, 10.0 );
        cairo_move_to( cr, 195.0, -14.0 );
        cairo_show_text ( cr, "300 kHz - 3 GHz" );
        cairo_move_to( cr, LOGO_WIDTH/2.0, 0.0 );
        cairo_show_text ( cr, "NETWORK ANALYZER" );
        cairo_set_font_size( cr, 12.0 );
        cairo_move_to( cr, LOGO_WIDTH/2.0, -13 );
        cairo_show_text ( cr, sProduct ? sProduct : "8753" );

		cairo_set_line_width( cr, 0.20 );
        cairo_move_to( cr, 0.0, -30.0 );
        cairo_rel_line_to( cr, LOGO_WIDTH, 0.0 );
        cairo_move_to( cr, 0.0, 8.0 );
        cairo_rel_line_to( cr, LOGO_WIDTH, 0.0 );
        cairo_stroke( cr );

        // the graphical logo in blue
        // The lines and curves are taken from an SVG
        // which is why they are a scaled a little odd
		setCairoColor( cr, eColorBlue );
		// lower left is  0.0
		cairo_new_path( cr );
		cairo_set_line_width( cr, 0.0 );

		// Left side
		cairo_move_to( cr, 14.157283, -22.122142 );
		cairo_rel_line_to( cr, -11.890374, 0.0 );
		cairo_rel_curve_to( cr, -1.26612, 0.0, -2.292703, 1.068564, -2.292703, 2.385483 );
		cairo_rel_line_to( cr, 0.0, 17.3609 );
		cairo_rel_curve_to( cr, 0.0, 1.316567, 1.026583, 2.38513, 2.292703, 2.38513 );
		cairo_rel_line_to( cr, 11.401072, 0.0 );
		cairo_rel_line_to( cr, 0.67063, -1.772355 );
		cairo_rel_curve_to( cr, -4.044244, -0.945445, -7.0739, -4.849989, -7.0739, -9.522883);
		cairo_rel_curve_to( cr, 0.0, -4.362803, 2.641953, -8.056386, 6.285089, -9.297106 );
		cairo_close_path( cr );
		cairo_rel_line_to( cr, 0.607483,-1.539169 );
		//h
        cairo_move_to( cr, 18.247742, -13.472737 );
        cairo_rel_move_to( cr, -2.3495, 6.587772 );
        cairo_rel_line_to( cr, -2.362553, 0.0 );
        cairo_rel_line_to( cr, 2.584097, -7.248877 );
        cairo_rel_line_to( cr, -1.21717, 0.0 );
        cairo_rel_line_to( cr, -2.584803, 7.248877 );
        cairo_rel_line_to( cr, -2.385582, 0.0);
        cairo_rel_line_to( cr, 5.430661,-15.229416 );
        cairo_rel_line_to( cr, 2.385483, 0.0 );
        cairo_rel_line_to( cr, -2.392186, 6.707011 );
        cairo_rel_line_to( cr, 1.60655, 0.0 );
        cairo_rel_curve_to( cr, 1.156758, 0.0, 1.631244, 0.906286, 1.284464, 1.934633 );
		// p
		cairo_move_to( cr, 24.248844, -15.444764 );
		cairo_rel_line_to( cr, -3.880202, 0.0 );
		cairo_rel_line_to( cr, -5.506156, 15.45731 );
		cairo_rel_line_to( cr, 2.385131, 0.0 );
		cairo_rel_line_to( cr, 2.346325, -6.586361 );
		cairo_rel_line_to( cr, 2.384072, 0.0 );
		cairo_rel_curve_to( cr, 0.528461, 0.0, 1.193094, -0.15875, 1.468614,-0.945091 );
		cairo_rel_curve_to( cr, 0.274108, -0.787047, 1.7653, -4.912783, 2.178755,-6.15315 );
		cairo_rel_curve_to( cr, 0.41275, -1.239308, -0.573969, -1.772708, -1.376539, -1.772708 );
		cairo_rel_move_to( cr, -3.008841, 7.611533 );
		cairo_rel_line_to( cr, -1.197681, 0.0 );
		cairo_rel_line_to( cr, 2.25037, -6.31825 );
		cairo_rel_line_to( cr, 1.198033, 0.0 );
		cairo_close_path( cr );
		// Right side
		cairo_move_to( cr, 19.110283, -1.581304 );
		cairo_rel_curve_to( cr, 4.956881, 0.0, 8.975372, -4.259791, 8.975372, -9.515474 );
		cairo_rel_curve_to( cr, 0.0, -5.255331, -4.018491, -9.516533, -8.975372, -9.516533 );
		cairo_rel_curve_to( cr, -0.185208, 0.0, -0.368652, 0.006, -0.550333, 0.01799 );
		cairo_rel_line_to( cr, 0.48895,-1.526822 );
		cairo_rel_line_to( cr, 14.222236, 0.0 );
		cairo_rel_curve_to( cr, 1.265414, 0.0, 2.291644, 1.068564, 2.291644, 2.385483 );
		cairo_rel_line_to( cr, 0.0, 17.3609 );
		cairo_rel_curve_to( cr, 0.0, 1.316567, -1.02623, 2.38513, -2.291644, 2.38513 );
		cairo_rel_line_to( cr, -14.71093, 0.0);
		cairo_close_path( cr );

        cairo_fill( cr );
	} cairo_restore( cr );
}
