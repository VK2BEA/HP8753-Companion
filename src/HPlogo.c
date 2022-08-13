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
#include "hp8753.h"
#include "GTKplot.h"
#include <math.h>

#define DECAL_WIDTH 270.0

// HP logo converted from SVG to cairo
void cairo_renderHewlettPackardLogo(cairo_t *cr) {
#define HP_LOGO_HEIGHT	501.0
#define HP_LOGO_WIDTH  2557.0
	cairo_pattern_t *pattern;
	cairo_save( cr ); {
		cairo_scale( cr, 112.0 / HP_LOGO_WIDTH, 112.0 / HP_LOGO_WIDTH );
		cairo_translate( cr , 0.0, -HP_LOGO_HEIGHT);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		pattern = cairo_pattern_create_rgba(0,0.270588,0.552941,1);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_new_path(cr);
		cairo_move_to(cr, 757.648438, 499.691406);
		cairo_curve_to(cr, 726.820313, 499.691406, 506.722656, 499.691406, 426.679688, 499.691406);
		cairo_line_to(cr, 439.117188, 465.621094);
		cairo_curve_to(cr, 581.886719, 456.964844, 644.625, 337.996094, 644.625, 246.058594);
		cairo_curve_to(cr, 644.625, 148.714844, 566.75, 36.773438, 425.0625, 34.074219);
		cairo_line_to(cr, 436.964844, 0.0078125);
		cairo_curve_to(cr, 515.375, 0.0078125, 739.265625, 0.0078125, 764.140625, 0.0078125);
		cairo_curve_to(cr, 790.097656, 0.0078125, 814.433594, 24.34375, 814.433594, 47.054688);
		cairo_curve_to(cr, 814.433594, 80.046875, 814.433594, 370.449219, 814.433594, 430.480469);
		cairo_curve_to(cr, 814.433594, 475.355469, 796.585938, 499.691406, 757.648438, 499.691406);
		cairo_close_path(cr);
		cairo_move_to(cr, 757.648438, 499.691406);
		cairo_set_tolerance(cr, 0.1);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
		cairo_fill_preserve(cr);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		pattern = cairo_pattern_create_rgba(0,0.270588,0.552941,1);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_new_path(cr);
		cairo_move_to(cr, 315.824219, 499.691406);
		cairo_curve_to(cr, 236.328125, 499.691406, 87.605469, 499.691406, 56.785156, 499.691406);
		cairo_curve_to(cr, 17.847656, 499.691406, 0, 475.355469, 0, 430.472656);
		cairo_curve_to(cr, 0, 370.441406, 0, 80.039063, 0, 47.046875);
		cairo_curve_to(cr, 0, 24.335938, 24.335938, 0, 50.292969, 0);
		cairo_curve_to(cr, 76.25, 0, 246.601563, 0, 325.015625, 0);
		cairo_line_to(cr, 313.660156, 33.527344);
		cairo_curve_to(cr, 231.457031, 65.976563, 168.1875, 148.722656, 168.1875, 246.058594);
		cairo_curve_to(cr, 168.1875, 337.996094, 222.265625, 427.226563, 328.804688, 462.375);
		cairo_close_path(cr);
		cairo_move_to(cr, 315.824219, 499.691406);
		cairo_set_tolerance(cr, 0.1);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
		cairo_fill_preserve(cr);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		pattern = cairo_pattern_create_rgba(0,0.270588,0.552941,1);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_new_path(cr);
		cairo_move_to(cr, 413.707031, 207.664063);
		cairo_curve_to(cr, 410.460938, 217.398438, 365.035156, 345.023438, 365.035156, 345.023438);
		cairo_line_to(cr, 312.03125, 345.023438);
		cairo_line_to(cr, 370.441406, 180.625);
		cairo_line_to(cr, 341.242188, 180.625);
		cairo_line_to(cr, 282.839844, 345.023438);
		cairo_line_to(cr, 231.464844, 345.023438);
		cairo_line_to(cr, 354.222656, 0);
		cairo_line_to(cr, 405.597656, 0);
		cairo_line_to(cr, 351.523438, 151.960938);
		cairo_curve_to(cr, 351.523438, 151.960938, 375.316406, 151.960938, 389.914063, 151.960938);
		cairo_curve_to(cr, 408.296875, 151.960938, 421.28125, 165.480469, 421.28125, 181.164063);
		cairo_curve_to(cr, 421.28125, 188.734375, 415.871094, 201.71875, 413.707031, 207.664063);
		cairo_close_path(cr);
		cairo_move_to(cr, 413.707031, 207.664063);
		cairo_set_tolerance(cr, 0.1);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
		cairo_fill_preserve(cr);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		pattern = cairo_pattern_create_rgba(0,0.270588,0.552941,1);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_new_path(cr);
		cairo_move_to(cr, 585.140625, 207.125);
		cairo_curve_to(cr, 581.894531, 216.863281, 551.613281, 302.304688, 546.203125, 316.902344);
		cairo_curve_to(cr, 540.792969, 331.503906, 533.230469, 352.597656, 508.347656, 352.597656);
		cairo_line_to(cr, 450.488281, 352.597656);
		cairo_line_to(cr, 397.496094, 499.691406);
		cairo_line_to(cr, 344.5, 499.691406);
		cairo_line_to(cr, 468.34375, 151.960938);
		cairo_line_to(cr, 561.355469, 151.960938);
		cairo_curve_to(cr, 579.203125, 151.960938, 592.722656, 165.480469, 592.722656, 181.164063);
		cairo_curve_to(cr, 592.707031, 187.652344, 588.925781, 196.84375, 585.140625, 207.125);
		cairo_close_path(cr);
		cairo_move_to(cr, 511.59375, 180.625);
		cairo_line_to(cr, 460.761719, 323.941406);
		cairo_line_to(cr, 489.964844, 323.941406);
		cairo_line_to(cr, 540.792969, 180.625);
		cairo_close_path(cr);
		cairo_move_to(cr, 511.59375, 180.625);
		cairo_set_tolerance(cr, 0.1);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
		cairo_fill_preserve(cr);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		pattern = cairo_pattern_create_rgba(0.0,0.0,0.0,1);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_new_path(cr);
		cairo_move_to(cr, 1091.613281, 343.40625);
		cairo_line_to(cr, 1002.917969, 343.40625);
		cairo_line_to(cr, 1002.917969, 403.972656);
		cairo_line_to(cr, 1091.613281, 403.972656);
		cairo_curve_to(cr, 1104.050781, 403.972656, 1112.160156, 389.371094, 1112.160156, 373.6875);
		cairo_curve_to(cr, 1112.160156, 358.542969, 1104.050781, 343.40625, 1091.613281, 343.40625);
		cairo_close_path(cr);
		cairo_move_to(cr, 1271.15625, 331.503906);
		cairo_line_to(cr, 1231.136719, 423.980469);
		cairo_line_to(cr, 1311.175781, 423.980469);
		cairo_close_path(cr);
		cairo_move_to(cr, 1968.769531, 333.667969);
		cairo_line_to(cr, 1928.75, 423.980469);
		cairo_line_to(cr, 2007.707031, 423.980469);
		cairo_close_path(cr);
		cairo_move_to(cr, 2248.363281, 343.941406);
		cairo_line_to(cr, 2161.832031, 343.941406);
		cairo_line_to(cr, 2161.832031, 406.136719);
		cairo_line_to(cr, 2246.734375, 406.136719);
		cairo_curve_to(cr, 2262.417969, 406.136719, 2271.074219, 390.453125, 2271.074219, 375.316406);
		cairo_curve_to(cr, 2271.074219, 359.625, 2260.800781, 343.941406, 2248.363281, 343.941406);
		cairo_close_path(cr);
		cairo_move_to(cr, 2505.234375, 347.1875);
		cairo_curve_to(cr, 2499.828125, 342.316406, 2492.800781, 340.695313, 2484.691406, 340.695313);
		cairo_line_to(cr, 2401.40625, 340.695313);
		cairo_line_to(cr, 2401.40625, 460.210938);
		cairo_line_to(cr, 2484.691406, 460.210938);
		cairo_curve_to(cr, 2492.261719, 460.210938, 2499.828125, 459.128906, 2504.699219, 454.800781);
		cairo_curve_to(cr, 2511.191406, 448.3125, 2514.429688, 431.546875, 2514.429688, 398.554688);
		cairo_curve_to(cr, 2514.429688, 366.117188, 2513.347656, 353.132813, 2505.234375, 347.1875);
		cairo_close_path(cr);
		cairo_move_to(cr, 1106.75, 443.992188);
		cairo_line_to(cr, 1002.917969, 443.992188);
		cairo_line_to(cr, 1002.917969, 499.691406);
		cairo_line_to(cr, 960.199219, 499.691406);
		cairo_line_to(cr, 960.199219, 303.929688);
		cairo_line_to(cr, 1101.886719, 303.929688);
		cairo_curve_to(cr, 1135.953125, 303.929688, 1155.425781, 331.511719, 1155.425781, 374.769531);
		cairo_curve_to(cr, 1155.425781, 414.246094, 1135.960938, 443.992188, 1106.75, 443.992188);
		cairo_close_path(cr);
		cairo_move_to(cr, 1344.160156, 499.691406);
		cairo_line_to(cr, 1327.933594, 462.375);
		cairo_line_to(cr, 1214.363281, 462.375);
		cairo_line_to(cr, 1197.601563, 499.691406);
		cairo_line_to(cr, 1152.710938, 499.691406);
		cairo_line_to(cr, 1238.148438, 303.929688);
		cairo_line_to(cr, 1278.714844, 303.929688);
		cairo_curve_to(cr, 1297.640625, 303.929688, 1306.296875, 310.957031, 1312.78125, 324.476563);
		cairo_curve_to(cr, 1317.105469, 333.667969, 1390.109375, 499.691406, 1390.109375, 499.691406);
		cairo_close_path(cr);
		cairo_move_to(cr, 1601.035156, 484.011719);
		cairo_curve_to(cr, 1586.433594, 498.613281, 1576.699219, 501.320313, 1513.96875, 501.320313);
		cairo_curve_to(cr, 1453.394531, 501.320313, 1442.574219, 500.238281, 1427.4375, 484.011719);
		cairo_curve_to(cr, 1413.375, 469.410156, 1410.671875, 445.609375, 1410.671875, 401.261719);
		cairo_curve_to(cr, 1410.671875, 358.542969, 1413.375, 334.75, 1427.4375, 320.148438);
		cairo_curve_to(cr, 1442.574219, 303.921875, 1453.394531, 302.839844, 1513.96875, 302.839844);
		cairo_curve_to(cr, 1576.699219, 302.839844, 1586.433594, 305.542969, 1601.035156, 320.148438);
		cairo_curve_to(cr, 1607.527344, 326.632813, 1611.308594, 337.996094, 1611.308594, 350.433594);
		cairo_line_to(cr, 1611.308594, 362.871094);
		cairo_line_to(cr, 1570.207031, 362.871094);
		cairo_curve_to(cr, 1570.207031, 362.871094, 1569.660156, 350.96875, 1566.425781, 346.644531);
		cairo_curve_to(cr, 1562.640625, 341.777344, 1553.980469, 339.613281, 1513.421875, 339.613281);
		cairo_curve_to(cr, 1470.703125, 339.613281, 1464.210938, 340.152344, 1459.882813, 345.5625);
		cairo_curve_to(cr, 1455.558594, 350.433594, 1453.929688, 361.242188, 1453.929688, 401.261719);
		cairo_curve_to(cr, 1453.929688, 441.828125, 1455.558594, 452.644531, 1459.882813, 457.511719);
		cairo_curve_to(cr, 1464.210938, 462.917969, 1470.695313, 463.464844, 1513.421875, 463.464844);
		cairo_curve_to(cr, 1553.980469, 463.464844, 1562.632813, 461.300781, 1566.425781, 456.429688);
		cairo_curve_to(cr, 1569.660156, 452.101563, 1569.125, 446.15625, 1569.125, 440.199219);
		cairo_line_to(cr, 1611.308594, 440.199219);
		cairo_line_to(cr, 1611.308594, 453.71875);
		cairo_curve_to(cr, 1611.308594, 466.164063, 1607.527344, 477.519531, 1601.035156, 484.011719);
		cairo_close_path(cr);
		cairo_move_to(cr, 1760.03125, 395.855469);
		cairo_line_to(cr, 1836.28125, 473.191406);
		cairo_line_to(cr, 1836.28125, 499.691406);
		cairo_line_to(cr, 1804.378906, 499.691406);
		cairo_line_to(cr, 1722.175781, 416.945313);
		cairo_line_to(cr, 1694.59375, 416.945313);
		cairo_line_to(cr, 1694.59375, 499.691406);
		cairo_line_to(cr, 1651.871094, 499.691406);
		cairo_line_to(cr, 1651.871094, 303.929688);
		cairo_line_to(cr, 1694.59375, 303.929688);
		cairo_line_to(cr, 1694.59375, 375.316406);
		cairo_line_to(cr, 1722.175781, 375.316406);
		cairo_line_to(cr, 1794.640625, 303.929688);
		cairo_line_to(cr, 1852.507813, 303.929688);
		cairo_close_path(cr);
		cairo_move_to(cr, 2041.78125, 499.691406);
		cairo_line_to(cr, 2024.480469, 462.375);
		cairo_line_to(cr, 1911.992188, 462.375);
		cairo_line_to(cr, 1895.226563, 499.691406);
		cairo_line_to(cr, 1861.15625, 499.691406);
		cairo_line_to(cr, 1861.15625, 473.191406);
		cairo_line_to(cr, 1936.328125, 303.921875);
		cairo_line_to(cr, 1975.8125, 303.921875);
		cairo_curve_to(cr, 1995.277344, 303.921875, 2003.933594, 310.949219, 2009.878906, 324.46875);
		cairo_curve_to(cr, 2014.207031, 333.660156, 2087.214844, 499.6875, 2087.214844, 499.6875);
		cairo_line_to(cr, 2041.78125, 499.6875);
		cairo_close_path(cr);
		cairo_move_to(cr, 2292.171875, 438.046875);
		cairo_curve_to(cr, 2301.363281, 444.535156, 2307.308594, 454.808594, 2307.308594, 465.082031);
		cairo_line_to(cr, 2307.308594, 499.691406);
		cairo_line_to(cr, 2265.125, 499.691406);
		cairo_line_to(cr, 2265.125, 469.945313);
		cairo_curve_to(cr, 2265.125, 459.128906, 2255.925781, 445.074219, 2233.761719, 445.074219);
		cairo_line_to(cr, 2161.839844, 445.074219);
		cairo_line_to(cr, 2161.839844, 499.691406);
		cairo_line_to(cr, 2119.117188, 499.691406);
		cairo_line_to(cr, 2119.117188, 303.929688);
		cairo_line_to(cr, 2257.015625, 303.929688);
		cairo_curve_to(cr, 2291.082031, 303.929688, 2315.425781, 335.832031, 2315.425781, 378.558594);
		cairo_curve_to(cr, 2315.425781, 406.136719, 2307.308594, 427.765625, 2292.171875, 438.046875);
		cairo_close_path(cr);
		cairo_move_to(cr, 2534.4375, 489.417969);
		cairo_curve_to(cr, 2521.464844, 499.691406, 2507.398438, 499.691406, 2497.125, 499.691406);
		cairo_line_to(cr, 2358.683594, 499.691406);
		cairo_line_to(cr, 2358.683594, 303.929688);
		cairo_line_to(cr, 2497.125, 303.929688);
		cairo_curve_to(cr, 2508.480469, 303.929688, 2524.710938, 305.550781, 2534.4375, 314.203125);
		cairo_curve_to(cr, 2544.175781, 322.859375, 2556.609375, 332.59375, 2556.609375, 397.488281);
		cairo_curve_to(cr, 2556.609375, 462.382813, 2549.046875, 478.601563, 2534.4375, 489.417969);
		cairo_close_path(cr);
		cairo_move_to(cr, 2534.4375, 489.417969);
		cairo_set_tolerance(cr, 0.1);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
		cairo_fill_preserve(cr);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		pattern = cairo_pattern_create_rgba(0.0,0.0,0.0,1);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_new_path(cr);
		cairo_move_to(cr, 2553.910156, 0);
		cairo_line_to(cr, 2553.910156, 41.640625);
		cairo_line_to(cr, 2488.472656, 41.640625);
		cairo_line_to(cr, 2488.472656, 195.769531);
		cairo_line_to(cr, 2445.75, 195.769531);
		cairo_line_to(cr, 2445.75, 41.640625);
		cairo_line_to(cr, 2380.859375, 41.640625);
		cairo_line_to(cr, 2380.859375, 0);
		cairo_close_path(cr);
		cairo_move_to(cr, 2553.910156, 0);
		cairo_set_tolerance(cr, 0.1);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
		cairo_fill_preserve(cr);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		pattern = cairo_pattern_create_rgba(0.0,0.0,0.0,1);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_new_path(cr);
		cairo_move_to(cr, 2346.785156, 0);
		cairo_line_to(cr, 2346.785156, 41.640625);
		cairo_line_to(cr, 2281.347656, 41.640625);
		cairo_line_to(cr, 2281.347656, 195.769531);
		cairo_line_to(cr, 2238.625, 195.769531);
		cairo_line_to(cr, 2238.625, 41.640625);
		cairo_line_to(cr, 2173.730469, 41.640625);
		cairo_line_to(cr, 2173.730469, 0);
		cairo_close_path(cr);
		cairo_move_to(cr, 2346.785156, 0);
		cairo_set_tolerance(cr, 0.1);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
		cairo_fill_preserve(cr);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		pattern = cairo_pattern_create_rgba(0.0,0.0,0.0,1);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_new_path(cr);
		cairo_move_to(cr, 1987.695313, 195.769531);
		cairo_curve_to(cr, 1969.851563, 195.769531, 1953.085938, 185.496094, 1953.085938, 166.570313);
		cairo_curve_to(cr, 1953.085938, 90.3125, 1953.085938, 60.574219, 1953.085938, 29.210938);
		cairo_curve_to(cr, 1953.085938, 12.445313, 1967.148438, 0.0078125, 1987.695313, 0.0078125);
		cairo_curve_to(cr, 1999.597656, 0.0078125, 2134.25, 0.0078125, 2134.25, 0.0078125);
		cairo_line_to(cr, 2134.25, 41.644531);
		cairo_line_to(cr, 1995.808594, 41.644531);
		cairo_line_to(cr, 1995.808594, 76.257813);
		cairo_line_to(cr, 2116.949219, 76.257813);
		cairo_line_to(cr, 2116.949219, 113.566406);
		cairo_line_to(cr, 1995.808594, 113.566406);
		cairo_line_to(cr, 1995.808594, 154.125);
		cairo_line_to(cr, 2139.660156, 154.125);
		cairo_line_to(cr, 2139.660156, 195.765625);
		cairo_curve_to(cr, 2139.660156, 195.765625, 2025.554688, 195.765625, 1987.695313, 195.765625);
		cairo_close_path(cr);
		cairo_move_to(cr, 1987.695313, 195.769531);
		cairo_set_tolerance(cr, 0.1);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
		cairo_fill_preserve(cr);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		pattern = cairo_pattern_create_rgba(0.0,0.0,0.0,1);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_new_path(cr);
		cairo_move_to(cr, 1799.507813, 195.769531);
		cairo_curve_to(cr, 1781.660156, 195.769531, 1764.894531, 185.496094, 1764.894531, 166.570313);
		cairo_curve_to(cr, 1764.894531, 90.3125, 1764.894531, 0.0078125, 1764.894531, 0.0078125);
		cairo_line_to(cr, 1807.617188, 0.0078125);
		cairo_line_to(cr, 1807.617188, 154.132813);
		cairo_line_to(cr, 1930.914063, 154.132813);
		cairo_line_to(cr, 1930.914063, 195.769531);
		cairo_curve_to(cr, 1930.914063, 195.769531, 1837.363281, 195.769531, 1799.507813, 195.769531);
		cairo_close_path(cr);
		cairo_move_to(cr, 1799.507813, 195.769531);
		cairo_set_tolerance(cr, 0.1);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
		cairo_fill_preserve(cr);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		pattern = cairo_pattern_create_rgba(0.0,0.0,0.0,1);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_new_path(cr);
		cairo_move_to(cr, 1667.554688, 195.769531);
		cairo_curve_to(cr, 1667.554688, 195.769531, 1642.671875, 195.769531, 1633.480469, 195.769531);
		cairo_curve_to(cr, 1624.828125, 195.769531, 1612.390625, 187.652344, 1608.0625, 173.058594);
		cairo_curve_to(cr, 1604.28125, 157.914063, 1578.316406, 57.328125, 1578.316406, 57.328125);
		cairo_line_to(cr, 1540.460938, 195.769531);
		cairo_curve_to(cr, 1528.5625, 195.769531, 1517.75, 195.769531, 1508.015625, 195.769531);
		cairo_curve_to(cr, 1498.277344, 195.769531, 1486.921875, 189.28125, 1479.894531, 168.1875);
		cairo_curve_to(cr, 1473.402344, 147.640625, 1427.4375, 0.0078125, 1427.4375, 0.0078125);
		cairo_line_to(cr, 1471.777344, 0.0078125);
		cairo_line_to(cr, 1512.878906, 135.203125);
		cairo_line_to(cr, 1555.0625, 0.0078125);
		cairo_line_to(cr, 1602.652344, 0.0078125);
		cairo_line_to(cr, 1644.292969, 135.203125);
		cairo_line_to(cr, 1687.558594, 0.0078125);
		cairo_line_to(cr, 1732.980469, 0.0078125);
		cairo_close_path(cr);
		cairo_move_to(cr, 1667.554688, 195.769531);
		cairo_set_tolerance(cr, 0.1);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
		cairo_fill_preserve(cr);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		pattern = cairo_pattern_create_rgba(0.0,0.0,0.0,1);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_new_path(cr);
		cairo_move_to(cr, 1248.4375, 195.769531);
		cairo_curve_to(cr, 1230.046875, 195.769531, 1213.828125, 185.496094, 1213.828125, 166.570313);
		cairo_curve_to(cr, 1213.828125, 90.3125, 1213.828125, 60.574219, 1213.828125, 29.210938);
		cairo_curve_to(cr, 1213.828125, 12.445313, 1227.347656, 0.0078125, 1248.4375, 0.0078125);
		cairo_curve_to(cr, 1260.339844, 0.0078125, 1394.996094, 0.0078125, 1394.996094, 0.0078125);
		cairo_line_to(cr, 1394.996094, 41.644531);
		cairo_line_to(cr, 1256.554688, 41.644531);
		cairo_line_to(cr, 1256.554688, 76.257813);
		cairo_line_to(cr, 1377.695313, 76.257813);
		cairo_line_to(cr, 1377.695313, 113.566406);
		cairo_line_to(cr, 1256.554688, 113.566406);
		cairo_line_to(cr, 1256.554688, 154.125);
		cairo_line_to(cr, 1400.40625, 154.125);
		cairo_line_to(cr, 1400.40625, 195.765625);
		cairo_curve_to(cr, 1400.40625, 195.765625, 1286.292969, 195.765625, 1248.4375, 195.765625);
		cairo_close_path(cr);
		cairo_move_to(cr, 1248.4375, 195.769531);
		cairo_set_tolerance(cr, 0.1);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
		cairo_fill_preserve(cr);

		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		pattern = cairo_pattern_create_rgba(0.0,0.0,0.0,1);
		cairo_set_source(cr, pattern);
		cairo_pattern_destroy(pattern);
		cairo_new_path(cr);
		cairo_move_to(cr, 1164.078125, 0);
		cairo_line_to(cr, 1164.078125, 195.769531);
		cairo_line_to(cr, 1121.351563, 195.769531);
		cairo_line_to(cr, 1121.351563, 113.566406);
		cairo_line_to(cr, 1002.917969, 113.566406);
		cairo_line_to(cr, 1002.917969, 195.769531);
		cairo_line_to(cr, 960.199219, 195.769531);
		cairo_line_to(cr, 960.199219, 0);
		cairo_line_to(cr, 1002.917969, 0);
		cairo_line_to(cr, 1002.917969, 76.25);
		cairo_line_to(cr, 1121.351563, 76.25);
		cairo_line_to(cr, 1121.351563, 0);
		cairo_close_path(cr);
		cairo_move_to(cr, 1164.078125, 0);
		cairo_set_tolerance(cr, 0.1);
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
		cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
		cairo_fill_preserve(cr);
	} cairo_restore( cr );
}

void
drawHPlogo (cairo_t *cr, gchar *sProduct, gdouble centreX, gdouble lowerLeftY, gdouble scale)
{

	cairo_save( cr ); {
		// make the current point 0.0, 0.0
		cairo_translate( cr , centreX, lowerLeftY);
		// scale so that 1.0 is 100pt horizontally
		cairo_scale( cr, 2.83 * scale, -2.83 * scale );
		cairo_translate( cr , -DECAL_WIDTH/2.0, 0.0);

        cairo_renderHewlettPackardLogo(cr);

        cairo_set_font_size( cr, 10.0 );
        rightJustifiedCairoText( cr, "300 kHz - 3 GHz", DECAL_WIDTH, -14.0);
        leftJustifiedCairoText( cr, "NETWORK ANALYZER", DECAL_WIDTH/2.0, 0.0 );

        cairo_set_font_size( cr, 12.0 );
        leftJustifiedCairoText( cr, sProduct ? sProduct : "8753", DECAL_WIDTH/2.0, -13.0 );

        // draw tram lines
		cairo_set_line_width( cr, 0.20 );
        cairo_move_to( cr, 0.0, -30.0 );
        cairo_rel_line_to( cr, DECAL_WIDTH, 0.0 );
        cairo_move_to( cr, 0.0, 8.0 );
        cairo_rel_line_to( cr, DECAL_WIDTH, 0.0 );
        cairo_stroke( cr );

	} cairo_restore( cr );
}
