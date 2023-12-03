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
#include <cairo/cairo-pdf.h>
#include <glib-2.0/glib.h>
#include "hp8753.h"
#include "calibrationKit.h"

#include "messageEvent.h"


// Standard ... white / black  / red
//              green / yellow / blue
//              magenta / cyan
GdkRGBA HPGLpensFactory[ NUM_HPGL_PENS ] = {
        { 1.00, 1.00, 1.00, 1.0 }, { 0.00, 0.00, 0.00, 1.0 }, { 0.75, 0.00, 0.00, 1.0 },
        { 0.00, 0.75, 0.00, 1.0 }, { 0.75, 0.75, 0.00, 1.0 }, { 0.00, 0.00, 0.75, 1.0 },
        { 0.75, 0.00, 0.75, 1.0 }, { 0.00, 0.75, 0.75, 1.0 }, { 0.00, 0.00, 0.00, 1.0 },
        { 0.00, 0.00, 0.00, 1.0 }, { 0.00, 0.00, 0.00, 1.0 }
};
GdkRGBA HPGLpens[ NUM_HPGL_PENS ] = {0};

GdkRGBA plotElementColorsFactory[ eMAX_COLORS ] = {
        [eColorTrace1]                 = {0.00, 0.39, 0.00, 1.0},  // dark blue
        [eColorTrace2]                 = {0.00, 0.00, 0.55, 1.0},  // dark green
        [eColorTraceSeparate]          = {0.00, 0.00, 0.00, 1.0},  // black
        [eColorGrid]                   = {0.51, 0.51, 0.84, 1.0},  // light blue
        [eColorGridPolarOverlay]       = {0.72, 0.52, 0.04, 1.0},  // dark golden rod
        [eColorSmithGridAnnotations]   = {0.50, 0.50, 0.50, 1.0},  // grey
        [eColorTextSpanPerDivCoupled]  = {0.00, 0.00, 1.00, 1.0},
        [eColorTextTitle]              = {0.00, 0.00, 0.00, 1.0},
        [eColorRefLine1]               = {1.00, 0.00, 0.00, 1.0},
        [eColorRefLine2]               = {1.00, 0.00, 0.00, 1.0},
        [eColorLiveMkrCursor]          = {1.00, 0.00, 0.00, 1.0},
        [eColorLiveMkrFreqTicks]       = {0.00, 0.00, 1.00, 1.0}
};
GdkRGBA plotElementColors[ eMAX_COLORS ];

/*!     \brief  Color change for high resolution plot
 *
 * Select a new color for the element of the high res. plot
 * listed in the associated combo box
 *
 * \param  wColorBtn    color button widget
 * \param  pGlobal      pointer to data
 */
void
CB_ColorBtn_HighResColor( GtkColorButton *wColorBtn , tGlobal *pGlobal)
{
    GdkRGBA color;
    GtkComboBox *wComboHiResColor = GTK_COMBO_BOX(g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_CB_HiResColor"));
    gchar *sId = (gchar *)gtk_combo_box_get_active_id( wComboHiResColor );
    gint id = 0;
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER( wColorBtn ), &color );
    if( sId == NULL )
        return;
    id = atoi( sId );
    if( id < eMAX_COLORS ) {
        plotElementColors[ id ] = color;
        if( !pGlobal->HP8753.flags.bShowHPGLplot || !pGlobal->HP8753.flags.bHPGLdataValid ) {
            gtk_widget_queue_draw( GTK_WIDGET( g_hash_table_lookup(pGlobal->widgetHashTable,
                                    (gconstpointer )"WID_DrawingArea_Plot_A")));
            gtk_widget_queue_draw( GTK_WIDGET( g_hash_table_lookup(pGlobal->widgetHashTable,
                                    (gconstpointer )"WID_DrawingArea_Plot_B")));
        }
    }
}

/*!     \brief  Color change for HPGL plot
 *
 * Select a new color for the element of the HPGL plot
 * listed in the associated combo box
 *
 * \param  wColorBtn    color button widget
 * \param  pGlobal      pointer to data
 */
void
CB_ColorBtn_HPGLcolor( GtkColorButton *wColorBtn , tGlobal *pGlobal)
{
    GdkRGBA color;
    GtkComboBox *wComboHPGLcolor = GTK_COMBO_BOX(g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_CB_HPGLcolor"));
    gchar *sId = (gchar *)gtk_combo_box_get_active_id( wComboHPGLcolor );
    gint id = 0;
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER( wColorBtn ), &color );
    if( sId == NULL )
        return;
    id = atoi( sId );
    if( id < NUM_HPGL_PENS ) {
        HPGLpens[ id ] = color;
        if( pGlobal->HP8753.flags.bHPGLdataValid && pGlobal->HP8753.flags.bShowHPGLplot ) {
            gtk_widget_queue_draw( GTK_WIDGET( g_hash_table_lookup(pGlobal->widgetHashTable,
                                    (gconstpointer )"WID_DrawingArea_Plot_A")));
            gtk_widget_queue_draw( GTK_WIDGET( g_hash_table_lookup(pGlobal->widgetHashTable,
                                    (gconstpointer )"WID_DrawingArea_Plot_B")));
        }
    }
}


/*!     \brief  Callback selection of GtkComboBoxText for what hi-res color to change
 *
 * Callback when the hi-res element GtkComboBoxText is changed (select which element to change color)
 *
 * \param  wHiResColorCombo    pointer to GtkComboBox widget
 * \param  tGlobal            pointer global data
 */
void
CB_NC_ComboBox_HiResColor (GtkComboBoxText *wHiResColorCombo, tGlobal *pGlobal) {
    gint id = 0;
    gchar *sId = (gchar *)gtk_combo_box_get_active_id(GTK_COMBO_BOX(wHiResColorCombo));
    GtkColorChooser *wColor = GTK_COLOR_CHOOSER(
            g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_CP_HiResColor"));
    if( sId == NULL )
        return;
    id = atoi( sId );

    if( id < eMAX_COLORS )
        gtk_color_chooser_set_rgba( wColor, &plotElementColors[ id ] );
}

/*!     \brief  Callback selection of GtkComboBoxText for what HPGL color to change
 *
 * Callback when the HPGL element GtkComboBoxText is changed (select which pen to change color)
 *
 * \param  wHiResColorCombo    pointer to GtkComboBox widget
 * \param  tGlobal            pointer global data
 */
void
CB_NC_ComboBox_HPGLcolor (GtkComboBoxText *wHPGLcolorCombo, tGlobal *pGlobal) {
    gint id = 0;
    gchar *sId = (gchar *)gtk_combo_box_get_active_id(GTK_COMBO_BOX(wHPGLcolorCombo));
    GtkColorChooser *wColor = GTK_COLOR_CHOOSER(
            g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_CP_HPGLcolor"));
    if( sId == NULL )
        return;
    id = atoi( sId );

    if( id < NUM_HPGL_PENS )
        gtk_color_chooser_set_rgba( wColor, &HPGLpens[ id ] );
}

/*!     \brief  Callback selection of GtkComboBoxText for what hi-res color to change
 *
 * Callback when the hi-res element GtkComboBoxText is changed (to apply ne color
 *
 * \param  wHiResColorCombo    pointer to GtkComboBox widget
 * \param  tGlobal            pointer global data
 */
gint
setNotePageColorButton (tGlobal *pGlobal, gboolean bHiResOrHPGL ) {
    gint id;
    GtkComboBox *wComboColorElement;
    GtkColorChooser *wColorChooser;
    gint rtn = ERROR;

    if( bHiResOrHPGL ) {
        wComboColorElement
            = GTK_COMBO_BOX(g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_CB_HiResColor"));
        wColorChooser
            = GTK_COLOR_CHOOSER( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_CP_HiResColor"));
    } else {
        wComboColorElement
            = GTK_COMBO_BOX(g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_CB_HPGLcolor"));
        wColorChooser
            = GTK_COLOR_CHOOSER( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_CP_HPGLcolor"));
    }

    gchar *sId = (gchar *)gtk_combo_box_get_active_id( wComboColorElement );
    if( sId ) {
        id = atoi( sId );
        if( id < ( bHiResOrHPGL ? eMAX_COLORS : NUM_HPGL_PENS )) {
            gtk_color_chooser_set_rgba( wColorChooser, bHiResOrHPGL ? &plotElementColors[ id ] : &HPGLpens[ id ]);
            rtn = OK;
        }

    }

    return rtn;
}

/*!     \brief  Callback for button to reset colors to factory
 *
 * Callback for button to reset colors to factory
 *
 * \param  wButton  pointer to GtkButton widget
 * \param  tGlobal  pointer global data
 */
void
CB_NC_BtnResetColors( GtkButton *wButton, tGlobal *pGlobal ) {
    for( int i=0; i < NUM_HPGL_PENS; i++ ) {
        HPGLpens[ i ] = HPGLpensFactory[ i ];
    }
    for( int i=0; i < eMAX_COLORS; i++ ) {
        plotElementColors[ i ] = plotElementColorsFactory[ i ];
    }
    gtk_widget_queue_draw( GTK_WIDGET( g_hash_table_lookup(pGlobal->widgetHashTable,
                            (gconstpointer )"WID_DrawingArea_Plot_A")));
    gtk_widget_queue_draw( GTK_WIDGET( g_hash_table_lookup(pGlobal->widgetHashTable,
                            (gconstpointer )"WID_DrawingArea_Plot_B")));
}

