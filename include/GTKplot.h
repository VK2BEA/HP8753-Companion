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

#ifndef GTKPLOT_H_
#define GTKPLOT_H_

gboolean plotCartesianTrace (cairo_t *cr, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal);
gboolean plotCartesianGrid (cairo_t *cr, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal);

gboolean plotPolarGrid (cairo_t *cr, gboolean bAnnotate, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal);
gboolean plotSmithAndPolarTrace (cairo_t *cr, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal);

gboolean plotSmithGrid (cairo_t *cr, gboolean bAnnotate, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal);

gboolean plotScreen (cairo_t *cr, guint areaHeight, guint areaWidth, tGlobal *pGlobal);

gboolean showPolarCursorInfo( cairo_t *cr, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal,
                              gdouble real, gdouble imag, gdouble frequency );

void     setCairoFontSize( cairo_t *cr, gdouble fSize );
void     setCairoColor( cairo_t *cr, eColor color );
void     setTraceColor( cairo_t *cr,gboolean bOverlay, eChannel channel );
void     leftJustifiedCairoText(cairo_t *cr, gchar *sLabel, gdouble x, gdouble y);
void     rightJustifiedCairoText(cairo_t *cr, gchar *sLabel, gdouble x, gdouble y);
void     centreJustifiedCairoText(cairo_t *cr, gchar *label, gdouble x, gdouble y);
void     centreJustifiedCairoTextWithClear(cairo_t *cr, gchar *label, gdouble x, gdouble y);
void     filmCreditsCairoText( cairo_t *cr, gchar *sLabelL, gchar *sLabelR, gint nLine,
                               gdouble x, gdouble y1stLine, tTxtPosn ePos );
void     multiLineText( cairo_t *cr, gchar *sLabel, gint nLine, gdouble lineSpacing,
		                gdouble x, gdouble y1stLine, tTxtPosn ePos );
gchar   *engNotation(gdouble value, gint digits, tEngNotation eVariant, gchar **sPrefix);
gboolean showStimulusInformation (cairo_t *cr, tGridParameters *pGrid, eChannel channel, tGlobal *pGlobal);
void     showTitleAndTime( cairo_t *cr, tGridParameters *pGrid, gchar *sTitle, gchar *sTime);
gdouble  calculateSegmentLinearlyInterpolatedResponse( gint nStart, gint nEnd, tChannel *pChannel, gdouble freq );

#define NUM_LOG_GRIDS 10
extern gdouble logGrids[ NUM_LOG_GRIDS ];

#define	UNIT_CIRCLE	1.0
#define LINE_THICKNESS	(pGrid->areaHeight / 1500.0)

#define FONT_SIZE (areaHeight/50)

#endif /* GTKPLOT_H_ */
