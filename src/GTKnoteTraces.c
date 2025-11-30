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


/*!     \brief  Callback when characters in "Title" entry widget are changed
*
* Callback (NTRA 1) when characters in "Title" entry widget are changed
*
* \param  wEditable   Editable widget
* \param  udata       unused
*/
void
CB_entry_Title_Changed (GtkEditable* wEditable, gpointer udata)
{
    gchar *sTitle = gtk_editable_get_chars(wEditable, 0, -1);
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wEditable ), "data");

    g_free( pGlobal->HP8753.sTitle );
    pGlobal->HP8753.sTitle = sTitle;
    gtk_widget_queue_draw(GTK_WIDGET(pGlobal->widgets[ eW_drawingArea_Plot_A ]));
    gtk_widget_queue_draw(GTK_WIDGET(pGlobal->widgets[ eW_drawingArea_Plot_B ]));
}

/*!     \brief  Callback when plot type is changed (HPGL/High Resolution)
*
* Callback (NTRA 2) when plot type is changed (HPGL/High Resolution)
*
* \param  wCheckButton  Check button widget
* \param  udata         unused
*/
void
CB_cbtn_PlotType ( GtkCheckButton* wCheckButtonHPGL, gpointer udata) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wCheckButtonHPGL ), "data");

    pGlobal->HP8753.flags.bShowHPGLplot = gtk_check_button_get_active( wCheckButtonHPGL );
    postDataToMainLoop(TM_REFRESH_TRACE, 0);
    postDataToMainLoop(TM_REFRESH_TRACE, (void*) 1);
}

/*!     \brief  Callback when the connected GtkComboBoxText with GtkEntry looses focus
 *
 * We use this to deselect the text in the entry widget
 *
 * \param   controller  controller for the GtkComboBox
 * \return  gpGlobal    gpointer to  the global data  (passed as user data)
 */
static void
CB_edit_Unfocus( GtkEventControllerFocus *controller, gpointer gpGlobal  ) {
    // This function will be called when the widget or its children lose focus.
    // You can access the widget the controller is attached to via
    GtkWidget *wEntry = gtk_event_controller_get_widget( GTK_EVENT_CONTROLLER( controller ));
    gtk_editable_select_region(GTK_EDITABLE( GTK_ENTRY( wEntry) ), 0, 0);
    gtk_widget_grab_focus( GTK_WIDGET( ((tGlobal *)gpGlobal)->widgets[ eW_frm_Project]  ) );
}

/*!     \brief  Initialize the widgets and callbacks on the 'Traces' page of the notebook
 *
 * Initialize the widgets and callbacks on the 'Traces' page of the notebook
 *
 * \param  pGlobal  pointer to global data
 */
void
initializeNotebookPageTraces( tGlobal *pGlobal, tInitFn purpose )
{
    GtkEventController *focus_controller;

    if( purpose == eUpdateWidgets || purpose == eInitAll ) {
        gtk_label_set_label ( GTK_LABEL( pGlobal->widgets[ eW_nbTrace_lbl_Time ] ),
                pGlobal->pTraceAbstract ? pGlobal->pTraceAbstract->sDateTime : "");

        gtk_text_buffer_set_text( gtk_text_view_get_buffer(
                GTK_TEXT_VIEW( pGlobal->widgets[ eW_nbTrace_txtV_TraceNote ] )),
                pGlobal->pTraceAbstract ? pGlobal->pTraceAbstract->sNote : "", STRLENGTH);

        gtk_entry_buffer_set_text( gtk_entry_get_buffer( GTK_ENTRY( pGlobal->widgets[ eW_nbTrace_entry_Title ] ) ),
                pGlobal->pTraceAbstract ? pGlobal->pTraceAbstract->sTitle : "", STRLENGTH );
        gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ pGlobal->HP8753.flags.bShowHPGLplot
                                  && pGlobal->HP8753.flags.bHPGLdataValid ? eW_nbTrace_rbtn_PlotTypeHPGL : eW_nbTrace_rbtn_PlotTypeHighRes ] ),
                TRUE );
#if ! GTK_CHECK_VERSION(4, 20, 0)
        // change date/Time color based on dark or light theme
        if( gtk_widget_has_css_class( GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_lbl_Time ] ),  pGlobal->flags.bDarkTheme ? "italicBlue" : "italicLightBlue" ) ) {
            // remove the old class
            gtk_widget_remove_css_class( GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_lbl_Time ] ), pGlobal->flags.bDarkTheme ? "italicBlue" : "italicLightBlue" );
            // add the new one
            gtk_widget_add_css_class( GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_lbl_Time ] ), pGlobal->flags.bDarkTheme ? "italicLightBlue" : "italicBlue" );
        }
#endif
    }

    if( purpose == eInitCallbacks || purpose == eInitAll ) {
        gtk_widget_set_visible( pGlobal->widgets[ eW_nbTrace_box_PlotType ], FALSE );
        // callback  NTRA 1 - change the entry widget of the 'Trace Profile' ComboBoxText
        g_signal_connect ( pGlobal->widgets[ eW_nbTrace_entry_Title ] , "changed",
                G_CALLBACK (CB_entry_Title_Changed), NULL );

        // callback NTRA 2 - connect signal from check box HPGL
        // Since there are only two checkboxes in this radio group we need only handle callbacks from one
        g_signal_connect ( pGlobal->widgets[ eW_nbTrace_rbtn_PlotTypeHPGL ], "toggled",
                G_CALLBACK ( CB_cbtn_PlotType ), NULL );

        focus_controller = gtk_event_controller_focus_new();
        gtk_widget_add_controller(  pGlobal->widgets[ eW_nbTrace_entry_Title ], focus_controller);
        g_signal_connect(focus_controller, "leave", G_CALLBACK( CB_edit_Unfocus ), pGlobal );
    }
}
