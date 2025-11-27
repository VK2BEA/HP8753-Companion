/*
 * Copyright (c) 2022-2026 Michael G. Katzmann
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

/**
 * @file GTKnoteColor.c
 * @brief Provides supporting code for the GtkNoteBook page 'Color'
 *
 * @author Michael G. Katzmann
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


// Standard ... (pen 0 to 7) from HP8753D Quick Reference Guide (p4-4)
//              white, cyan, magenta,
//              blue, yellow, green.
//              red, black,

//              grey, brown, orange

GdkRGBA HPGLpensFactory[ NUM_HPGL_PENS ] = {
        { 1.00, 1.00, 1.00, 1.0 }, { 0.00, 0.75, 0.75, 1.0 }, { 0.75, 0.00, 0.75, 1.0 },
        { 0.00, 0.00, 0.75, 1.0 }, { 0.75, 0.75, 0.00, 1.0 }, { 0.00, 0.75, 0.00, 1.0 },
        { 0.75, 0.00, 0.00, 1.0 }, { 0.00, 0.00, 0.00, 1.0 },

        { 0.25, 0.25, 0.25, 1.0 }, { 0.59, 0.29, 0.00, 1.0 }, { 1.00, 0.65, 0.00, 1.0 }
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

/*!     \brief  Callback for color change of element in high resolution plot
 *
 * Callback (NCO 1) to select a new color for the element of the high res. plot
 * listed in the associated combo box
 *
 * \param  wColorBtn    color button widget
 * \param  pGlobal      pointer to data
 */
void
CB_nbColor_colbtn_element( GtkColorDialogButton *wColorBtn , gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wColorBtn ), "data");

    guint id = gtk_drop_down_get_selected( GTK_DROP_DOWN( pGlobal->widgets[ eW_nbColor_dd_elementHR ]) );

    if( id < eMAX_COLORS ) {
        plotElementColors[ id ] = *gtk_color_dialog_button_get_rgba (GTK_COLOR_DIALOG_BUTTON( wColorBtn ) );
        if( !pGlobal->HP8753.flags.bShowHPGLplot || !pGlobal->HP8753.flags.bHPGLdataValid ) {
            gtk_widget_queue_draw( GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_A ] ));
            gtk_widget_queue_draw( GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_B ] ));
        }
    }
}

/*!     \brief  Color change callback for HPGL pen color
 *
 * Callback (NCO 2) to select a new color an HPGL pen
 * listed in the associated combo box
 *
 * \param  wColorBtn    color button widget
 * \param  pGlobal      pointer to data
 */
void
CB_nbColor_colbtn_HPGLpen( GtkColorDialogButton *wColorBtn , gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wColorBtn ), "data");

    guint id = gtk_drop_down_get_selected( GTK_DROP_DOWN( pGlobal->widgets[ eW_nbColor_dd_HPGLpen ]) );

    if( id < NUM_HPGL_PENS ) {
        HPGLpens[ id ] = *gtk_color_dialog_button_get_rgba (GTK_COLOR_DIALOG_BUTTON( wColorBtn ) );
        if( pGlobal->HP8753.flags.bHPGLdataValid && pGlobal->HP8753.flags.bShowHPGLplot ) {
            gtk_widget_queue_draw( GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_A ] ));
        }
    }
}


/*!     \brief  Callback selection of GtkDropDown for what hi-res color to change
 *
 * Callback (NCO 3) when the hi-res element GtkDropDown is changed (select which element to change color)
 *
 * \param  wHiResColorCombo   pointer to GtkDropDown widget
 * \param  tGlobal            pointer global data
 */
void
CB_nbColor_dd_elementHR (GtkDropDown *wHiResColorDropDown, gpointer udata ) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wHiResColorDropDown ), "data");

    guint id = gtk_drop_down_get_selected( GTK_DROP_DOWN( wHiResColorDropDown ) );

    if( id < eMAX_COLORS )
        gtk_color_dialog_button_set_rgba( GTK_COLOR_DIALOG_BUTTON( pGlobal->widgets[ eW_nbColor_colbtn_element ] ),
                &plotElementColors[ id ] );
}

/*!     \brief  Callback selection of GtkComboBoxText for what HPGL color to change
 *
 * Callback (NCO 4) when the HPGL element GtkComboBoxText is changed (select which pen to change color)
 *
 * \param  wHiResColorCombo    pointer to GtkComboBox widget
 * \param  tGlobal            pointer global data
 */
void
CB_nbColor_dd_HPGLpen (GtkDropDown *wHPGLcolorDropDown, gpointer udata) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wHPGLcolorDropDown ), "data");

    guint id = gtk_drop_down_get_selected( GTK_DROP_DOWN( wHPGLcolorDropDown ) );

    if( id < NUM_HPGL_PENS )
        gtk_color_dialog_button_set_rgba( GTK_COLOR_DIALOG_BUTTON( pGlobal->widgets[ eW_nbColor_colbtn_HPGLpen ] ),
                &HPGLpens[ id ] );
}

/*!     \brief  Set the color dialog button color
 *
 *
 * \param  pGlobal   pointer global data
 * \param  gboolean  which color dialog button are we dealing with
 */
gint
setNotePageColorButton (tGlobal *pGlobal, gboolean bHiResOrHPGL ) {
    guint id;
    GtkDropDown *wComboColorElement;
    GtkColorDialogButton *wColorChooser;
    gint rtn = ERROR;

    if( bHiResOrHPGL ) {
        wComboColorElement
            = GTK_DROP_DOWN( pGlobal->widgets[ eW_nbColor_dd_elementHR ]);
        wColorChooser
            = GTK_COLOR_DIALOG_BUTTON( pGlobal->widgets[ eW_nbColor_colbtn_element ] );
    } else {
        wComboColorElement
            = GTK_DROP_DOWN( pGlobal->widgets[ eW_nbColor_dd_HPGLpen ]);
        wColorChooser
            = GTK_COLOR_DIALOG_BUTTON( pGlobal->widgets[ eW_nbColor_colbtn_HPGLpen ] );
    }

    id = gtk_drop_down_get_selected( wComboColorElement );

    if( id != GTK_INVALID_LIST_POSITION && id < ( bHiResOrHPGL ? eMAX_COLORS : NUM_HPGL_PENS )) {
        gtk_color_dialog_button_set_rgba( wColorChooser, bHiResOrHPGL ? &plotElementColors[ id ] : &HPGLpens[ id ]);
        rtn = OK;
    }


    return rtn;
}

/*!     \brief  Callback for button to reset colors to factory
 *
 * Callback (NCO 5) for button to reset colors to factory
 *
 * \param  wButton  pointer to GtkButton widget
 * \param  udata    unuded
 */
void
CB_nbColor_btn_ResetColors( GtkButton *wButton, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wButton ), "data");
    guint id;

    for( int i=0; i < NUM_HPGL_PENS; i++ ) {
        HPGLpens[ i ] = HPGLpensFactory[ i ];
    }
    for( int i=0; i < eMAX_COLORS; i++ ) {
        plotElementColors[ i ] = plotElementColorsFactory[ i ];
    }
    gtk_widget_queue_draw( GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_A ] ) );

    gtk_widget_queue_draw( GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_B ] ) );


    // Reset the colors on the notebook page

    id = gtk_drop_down_get_selected( GTK_DROP_DOWN( pGlobal->widgets[ eW_nbColor_dd_elementHR ] ) );
    gtk_color_dialog_button_set_rgba( GTK_COLOR_DIALOG_BUTTON( pGlobal->widgets[ eW_nbColor_colbtn_element ] ),
            &plotElementColors[ id ] );

    id = gtk_drop_down_get_selected( GTK_DROP_DOWN( pGlobal->widgets[ eW_nbColor_dd_HPGLpen ] ) );
    gtk_color_dialog_button_set_rgba( GTK_COLOR_DIALOG_BUTTON( pGlobal->widgets[ eW_nbColor_colbtn_HPGLpen ] ),
            &plotElementColors[ id ] );

}

/*!     \brief  Initialize the widgets on the Color notebook page
 *
 * Initialize the widgets on the Color notebook page page
 *
 * \param  pGlobal      pointer to global data
 */
void
initializeNotebookPageColor( tGlobal *pGlobal, tInitFn purpose )
{
    // Color Dialog Buttons
    gpointer wcdb_ColorElementHR  = pGlobal->widgets[ eW_nbColor_colbtn_element ];
    gpointer wcdb_ColorHPGLpen    = pGlobal->widgets[ eW_nbColor_colbtn_HPGLpen ];
    // drop down to select what HR elemen and what Pen the color relates to
    gpointer wdd_colorElementHR  = pGlobal->widgets[ eW_nbColor_dd_elementHR ];
    gpointer wdd_colorHPGLpen    = pGlobal->widgets[ eW_nbColor_dd_HPGLpen ];

    if( purpose == eUpdateWidgets || purpose == eInitAll ) {
        // We use the trace1 color and first HPGL pen (P0 is actually pen park)
        // These selections are not saved
        gtk_drop_down_set_selected( GTK_DROP_DOWN( wdd_colorElementHR ),  0 );
        gtk_drop_down_set_selected( GTK_DROP_DOWN( wdd_colorHPGLpen ), 1 );

        gtk_color_dialog_button_set_rgba( wcdb_ColorElementHR,  &plotElementColors[ eColorTrace1 ] );
        gtk_color_dialog_button_set_rgba( wcdb_ColorHPGLpen,  &HPGLpens[ 1 ] );

        // Set the color buttons on the color note page
        // according to the element displayed in the combo boxes
        setNotePageColorButton (pGlobal, TRUE );
        setNotePageColorButton (pGlobal, FALSE );
    }
    if( purpose == eInitCallbacks || purpose == eInitAll ) {
        // NCO 1 - connect signal for callback on changing
        //          color of element on high resolution plot
        g_signal_connect ( wcdb_ColorElementHR, "notify::rgba",
                G_CALLBACK (CB_nbColor_colbtn_element), NULL);

        // NCO 2 - connect signal for callback on changing color of HPGL pen
        g_signal_connect ( wcdb_ColorHPGLpen, "notify::rgba",
                G_CALLBACK (CB_nbColor_colbtn_HPGLpen), NULL);

        // NCO 3 - connect signal for callback when user changes drop down
        //          to select high resolution element color
        g_signal_connect( wdd_colorElementHR, "notify::selected",
                G_CALLBACK( CB_nbColor_dd_elementHR ), NULL);

        // NCO 4 - connect signal for callback when user changes
        //          drop down to select HPGL pen number
        g_signal_connect( wdd_colorHPGLpen, "notify::selected",
                G_CALLBACK( CB_nbColor_dd_HPGLpen ), NULL);

        // NCO 5 - connect signal for reset color button
        g_signal_connect ( pGlobal->widgets[ eW_nbColor_btn_Reset ], "clicked",
                G_CALLBACK (CB_nbColor_btn_ResetColors), NULL );
    }
}

