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
#include <gdk/gdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cairo/cairo.h>
#include <glib-2.0/glib.h>
#include <math.h>
#include <ctype.h>

#include "hp8753.h"
#include "messageEvent.h"

// Blast ... the combobox is deprecated without a suitable replacement
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/*
 *          ESC:    abort - stop reading / writing to HP8970
 *    Shift ESC:    abort, then reinitialize the GPIB devices
 *     Ctrl ESC:    abort & send a GPIB clear to the HP8970
 *      Alt ESC:    clear measurement plot
 *
 *           F1:    show help scrren
 *
 *           F2:    send all settings to the HP8970 (useful if HP8970 has been preset or re-powered)
 *
 *           F12:   enlarge to max screen height
 *     Shift F12: make default size
 */

/*!     \brief  Callback button press
 *
 * Callback (MD1) button press
 *
 * \param  self        pointer to GtkEventControllerKey widget
 * \param  keyval      The released key
 * \param  keycode     The raw code of the released key.
 * \param  state       The bitmask, representing the state of modifier keys and pointer buttons
 * \param  udata       user data (unused)
 * \return             FALSE if we want other routines to handle
 */
static gboolean
CB_KeyPressed (GtkEventControllerKey *self, guint keyval, guint keycode,
               GdkModifierType state, gpointer udata)
{

    tGlobal *pGlobal = (tGlobal*) g_object_get_data ( G_OBJECT(self), "data");
    GtkWidget *wApplication, *wFramePlotB, *wControls;

       // What's the size of the monitor
       wApplication = GTK_WIDGET( pGlobal->widgets[ eW_hp8753_main ] );

       switch( keyval ) {
       case GDK_KEY_F1:
           switch (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK))
           {
           case GDK_SHIFT_MASK:
               break;
           case GDK_CONTROL_MASK:
               break;
           case GDK_ALT_MASK:
               break;
           case GDK_SUPER_MASK:
               break;
           default:
#if GTK_CHECK_VERSION(4,10,0)
                GtkUriLauncher *launcher = gtk_uri_launcher_new ("help:hp8753");
                gtk_uri_launcher_launch (launcher, GTK_WINDOW(gtk_widget_get_root( wApplication )), NULL, NULL, NULL);
                g_object_unref (launcher);
#endif
               break;
           }
           break;
       case GDK_KEY_F2:
           showRenameMoveCopyDialog( pGlobal );
           break;

       case GDK_KEY_F3:
           g_signal_emit_by_name (GTK_BUTTON( pGlobal->widgets[ eW_box_GetTrace ]), "clicked", 0);
           break;

       case GDK_KEY_F4:
           switch (state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK))
               {
               case GDK_SHIFT_MASK:
                   GtkRequisition min = {0}, nat = {0};
                   gtk_widget_get_preferred_size( pGlobal->widgets[ eW_hp8753_main ], &min, &nat );
                   g_print( "width = %d / %d & height = %d / %d\n", nat.width, min.width, nat.height, min.height );
                   gtk_widget_set_size_request( pGlobal->widgets[ eW_hp8753_main ], 1116, 647 );
                   gtk_window_set_default_size ( GTK_WINDOW( pGlobal->widgets[ eW_hp8753_main ] ), -1, -1 );
                   while (g_main_context_pending (g_main_context_default ()))
                       g_main_context_iteration (NULL, TRUE);
                   break;
               case GDK_CONTROL_MASK:
                   GtkWidget *wApplicationWindow = pGlobal->widgets[ eW_hp8753_main ];
                   GdkRectangle geometry= {0};
                   GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(wApplicationWindow));
                   GdkDisplay *display = gdk_surface_get_display (surface);
                   GdkMonitor *monitor = gdk_display_get_monitor_at_surface( display, surface );
                   gdk_monitor_get_geometry( monitor, &geometry );
                   gtk_window_set_default_size(GTK_WINDOW(wApplicationWindow), geometry.height*1.5, geometry.height);
                   while (g_main_context_pending (g_main_context_default ()))
                       g_main_context_iteration (NULL, TRUE);
                   break;
               case GDK_ALT_MASK:
                   // Don't use this (its used by the desktop)
                   break;
               case GDK_SUPER_MASK:
                   break;
               case 0:
                   break;
               }
           break;
       case GDK_KEY_KP_Add:
           gtk_window_fullscreen( GTK_WINDOW( wApplication ));
           break;

       case GDK_KEY_KP_Subtract:
           gtk_window_unfullscreen( GTK_WINDOW( wApplication ));
           break;

       case GDK_KEY_F9:
           wFramePlotB = GTK_WIDGET( pGlobal->widgets[ eW_frame_Plot_B] );
           wControls = GTK_WIDGET( pGlobal->widgets[ eW_box_Controls ]  );

           switch( state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK) )
           {
           case GDK_SHIFT_MASK:
               gtk_widget_set_visible( wControls, TRUE );
               break;
           case GDK_SUPER_MASK:
               gtk_widget_set_visible( wControls, FALSE );
               break;
           default:
               gtk_widget_set_visible( wControls, !gtk_widget_get_visible( wControls ) );
               break;
           }
           visibilityFramePlot_B (pGlobal, gtk_widget_get_visible(wFramePlotB) | 0x02);
           break;

       case GDK_KEY_F11:
           switch( state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK) )
           {
           case GDK_SHIFT_MASK:
               postDataToGPIBThread( TG_UTILITY, NULL );
               break;
           case GDK_SUPER_MASK:
               postDataToGPIBThread( TG_EXPERIMENT, NULL );
               break;
           default:
               break;
           }
           break;

       case GDK_KEY_F12:
           switch( state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK) )
           {
           case GDK_SHIFT_MASK:
               pGlobal->flags.bNoGPIBtimeout = TRUE;
               postInfo( "No GPIB timeouts" );
               break;
           default:
               pGlobal->flags.bNoGPIBtimeout = FALSE;
               postInfo( "Normal GPIB timeouts" );
               break;
           }
           break;

       case GDK_KEY_Escape:
           switch ( state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK) ) {
           default:
                    postDataToGPIBThread (TG_ABORT, NULL);
                    break;
           case GDK_SHIFT_MASK:
                    postDataToGPIBThread (TG_SETUP_GPIB, NULL);
                    break;
           }
           break;
       default:
           return FALSE;     // pass the event on to the underlying widgets
           break;
    }
    return TRUE; // nothing more to see here
}

/*!     \brief  Callback button Release
 *
 * Callback (MD2) button Release
 *
 * \param  self        pointer to GtkEventControllerKey widget
 * \param  keyval      The released key
 * \param  keycode     The raw code of the released key.
 * \param  state       The bitmask, representing the state of modifier keys and pointer buttons
 * \param  udata       user data (unused
 */
static void
CB_KeyReleased (GtkEventControllerKey *self, guint keyval, guint keycode,
                              GdkModifierType state, gpointer udata) {
}


/*!     \brief  Setup when main widget is created
 *
 * Callback MD3 - Setup when main widget is created (hide plot B initially and create a text style)
 * realize signal from GtkApplicationWindow
 *
 * \param wApplicationWindow GtkApplicationWindow widget pointer
 * \param pGlobal pointer to global data
 */
void
CB_hp8753_main_Realize (GtkApplicationWindow * wApplicationWindow, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wApplicationWindow ), "data");

    visibilityFramePlot_B( pGlobal, FALSE );
}

/*!     \brief  Callback for mouse movement in the plot area (GtkDrawingArea)
 *
 * Callback (MD4) for mouse movement in the plot area which will set the live markers if we have valid data.
 * We also get a callback for entry and exit of the drawing area
 *
 * \param eventGesture    event controller for this mouse action
 * \param x               x position of mouse
 * \param y               y position of mouse
 * \param action          GDK_MOTION_NOTIFY or GDK_ENTER_NOTIFY or GDK_LEAVE_NOTIFY
 */
void CB_on_drawingAreaMouseMotion ( GtkEventControllerMotion* eventGesture, gdouble x,  gdouble y, gpointer action )
{
    GtkDrawingArea *wDrawingArea= GTK_DRAWING_AREA( gtk_event_controller_get_widget( GTK_EVENT_CONTROLLER( eventGesture ) ));
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wDrawingArea))), "data");


    for( gint channel=0; channel < MAX_CHANNELS; channel++ )
        switch ( GPOINTER_TO_INT( action ) ) {
        case GDK_ENTER_NOTIFY:
        case GDK_MOTION_NOTIFY:
            pGlobal->mousePosition[channel].r = x;
            pGlobal->mousePosition[channel].i = y;
            break;
        case GDK_LEAVE_NOTIFY:
            pGlobal->mousePosition[channel].r = 0.0;
            pGlobal->mousePosition[channel].i = 0.0;
            break;
        default:
            break;
        }

    gtk_widget_queue_draw ( GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_A ] ) );
    gtk_widget_queue_draw ( GTK_WIDGET( pGlobal->widgets[ eW_drawingArea_Plot_B ] ) );
}

/*!     \brief  Callback from "Get Trace" button
*
* Callback (MD5) from brief  Callback from "Get Trace" button
*
* \param  wButton     Button widget
* \param  udata       unused
*/
void
CB_btn_GetTrace (GtkButton *wButton, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wButton ), "data");

    // Release held live marker
    pGlobal->flags.bHoldLiveMarker = FALSE;

    if( pGlobal->flags.bDoNotRetrieveHPGLdata ) {
        gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_nbTrace_rbtn_PlotTypeHighRes ] ), TRUE);
        g_free( pGlobal->HP8753.plotHPGL );
        pGlobal->HP8753.plotHPGL = NULL;
    }
    postDataToGPIBThread (TG_RETRIEVE_TRACE_from_HP8753, NULL);
    gtk_widget_set_sensitive (GTK_WIDGET( pGlobal->widgets[ eW_box_SaveRecallDelete ] ), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET( pGlobal->widgets[ eW_box_GetTrace ] ), FALSE);
    // Show the trace notebook page
    gtk_notebook_set_current_page ( pGlobal->widgets[ eW_notebook ], NPAGE_TRACE );
}

/*!     \brief  Sensitize the Recall/Save/Delete button
 *
 * Sensitize the Recall/Save/Delete button depending upon
 * whether the functions can be performed.
 * e.g. we can't save date if we have not any available.
 *
 * \param  wCalibration pointer to radio button widget
 * \param   pGlobal      global data
 * \return              TRUE if the calibration or trace profile found
 */
gboolean
sensitizeRecallSaveDeleteButtons( tGlobal *pGlobal ) {

    gboolean bFound = FALSE;
    GtkButton       *wBtnRecall = GTK_BUTTON( pGlobal->widgets[ eW_btn_Recall ] );
    GtkButton       *wBtnSave   = GTK_BUTTON( pGlobal->widgets[ eW_btn_Save ] );
    GtkButton       *wBtnDelete = GTK_BUTTON( pGlobal->widgets[ eW_btn_Delete ] );
    gchar           *sString;
    GtkComboBoxText *wComboText = GTK_COMBO_BOX_TEXT( pGlobal->widgets[
                pGlobal->flags.bCalibrationOrTrace ? eW_cbt_CalProfile : eW_cbt_TraceProfile ] );

    sString = gtk_combo_box_text_get_active_text(wComboText);
    bFound = setGtkComboBox( GTK_COMBO_BOX( wComboText ), sString );

    gtk_widget_set_sensitive( GTK_WIDGET( wBtnRecall ), bFound );

    if( pGlobal->flags.bCalibrationOrTrace ) {
        gtk_widget_set_sensitive( GTK_WIDGET( wBtnSave ), strlen(sString) );
    } else {
        gtk_widget_set_sensitive(
            GTK_WIDGET( wBtnSave ), strlen(sString)
                && (pGlobal->HP8753.channels[ eCH_ONE ].chFlags.bValidData
                        || pGlobal->HP8753.channels[ eCH_TWO ].chFlags.bValidData) );
    }
    gtk_widget_set_sensitive( GTK_WIDGET( wBtnDelete ), bFound );

    g_free( sString );
    return bFound;
}

/*!     \brief  Callback when user selects a new project from the dropdown box
 *
 * Callback (MD6) when user selects a new project from the dropdown box
 *
 * \param  wComboBoxProject     pointer to GtkCOmpoboxText widget
 * \param  udate                unused
 */
void
CB_cbt_ProjectName( GtkComboBoxText *wComboBoxProject, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wComboBoxProject ), "data");

    gint n = gtk_combo_box_get_active( GTK_COMBO_BOX( wComboBoxProject ));
    GtkWidget *wCalCombo = GTK_WIDGET( pGlobal->widgets[ eW_cbt_CalProfile ] );
    GtkWidget *wTraceCombo = GTK_WIDGET( pGlobal->widgets[ eW_cbt_TraceProfile ] );
    gchar *sProfileName = NULL;

    if( n != INVALID ) {
    }
    GtkCheckButton *wRadioCal = GTK_CHECK_BUTTON( pGlobal->widgets[ eW_rbtn_Cal ] );
    GtkNotebook *wNotebook = GTK_NOTEBOOK(  pGlobal->widgets[ eW_notebook ] );

    // Ususlly the not will be set in CB_EditableCalibrationProfileName and CB_EditableTraceProfileName
    // but if there is no profile the note will be left in the last state

    sProfileName = gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT(wCalCombo));
    if( strlen( sProfileName ) == 0 ) {
        GtkWidget *wCalNote = GTK_WIDGET( pGlobal->widgets[ eW_nbCal_txtV_CalibrationNote ] );
        gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW( wCalNote) ), "", STRLENGTH);
    }
    g_free( sProfileName );
    sProfileName = gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT(wTraceCombo));
    if( strlen( sProfileName ) == 0 ) {
        GtkWidget *wTraceNote = GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_txtV_TraceNote ] );
        gtk_text_buffer_set_text( gtk_text_view_get_buffer( GTK_TEXT_VIEW( wTraceNote) ), "", STRLENGTH);
    }
    g_free( sProfileName );

    if( gtk_check_button_get_active( GTK_CHECK_BUTTON( wRadioCal )) )
        gtk_notebook_set_current_page( wNotebook, NPAGE_CALIBRATION );
    else
        gtk_notebook_set_current_page( wNotebook, NPAGE_TRACE );

    sensitizeRecallSaveDeleteButtons( pGlobal );
}


/*!     \brief  Callback when user types in the ComboBoxText (editable) for the project name
 *
 * Callback (MD7) when user types in the ComboBoxText (editable) for the project name.
 * If the profile name matches an existing profile .. display the info for that profile
 *
 * \param  wEditable    pointer to GtkCheckBoxText widget
 * \param  udate        unused
 */
void
CB_editable_ProjectName( GtkEditable *wEditable, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wEditable ), "data");

    gchar *pProjectName = gtk_editable_get_chars( wEditable, 0, -1 ); // and allocated string
    g_free(pGlobal->sProject);
    if( strlen( pProjectName ) )
        pGlobal->sProject = pProjectName;
    else {
        pGlobal->sProject = (gchar *)0;
        g_free( pProjectName );
    }
    populateCalComboBoxWidget( pGlobal );
    populateTraceComboBoxWidget( pGlobal );
}

/*!     \brief  Callback Calibration/Setup selection of GtkComboBoxText
 *
 * Callback (MD8) when the Calibration/Setup profile GtkComboBoxText is changed
 *
 * \param  wCalSelection      pointer to GtkComboBox widget
 * \param  udate              unused
 */
void
CB_cbt_CalibrationProfileName (GtkComboBoxText *wCalSelection, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wCalSelection ), "data");

    gchar *sCalProfileName = NULL;
    tProjectAndName *pProjectAndName;
    sCalProfileName = gtk_combo_box_text_get_active_text(wCalSelection);
    for( GList *l = pGlobal->pCalList; l != NULL; l = l->next ){
        pProjectAndName = &((tHP8753cal *)l->data)->projectAndName;

        if( g_strcmp0(  pProjectAndName->sProject, pGlobal->sProject ) == 0 ) {
            pProjectAndName->bbFlags.bSelected = FALSE;
            if( g_strcmp0(  pProjectAndName->sName, sCalProfileName ) == 0 ) {
                pProjectAndName->bbFlags.bSelected = TRUE;
            }
        }
    }
    g_free( sCalProfileName );
}


/*!     \brief  Callback when user types in the ComboBoxText (editable) for the calibration profile name
 *
 * Callback (MD9) when user types in the ComboBoxText (editable) for the calibration profile name.
 * Sensitize the 'save' and 'delete' buttons if the text matches a saved profile, otherwise
 * desensitize the buttons.
 * If the profile name matches an existing profile .. display the info for that profile
 *
 * \param  wEditable    pointer to GtkCheckBoxText widget
 * \param  udate              unused
 */
void
CB_editable_CalibrationProfileName( GtkEditable *wEditable, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wEditable ), "data");

    GtkWidget *wCalNote = GTK_WIDGET( pGlobal->widgets[ eW_nbCal_txtV_CalibrationNote ] );
    GtkTextBuffer* wTBnote =  gtk_text_view_get_buffer( GTK_TEXT_VIEW( wCalNote ));
            gtk_notebook_set_current_page ( GTK_NOTEBOOK( pGlobal->widgets[ eW_notebook ] ),
                    NPAGE_CALIBRATION );
    GtkComboBoxText *wCalibrationComboBox =
            GTK_COMBO_BOX_TEXT(gtk_widget_get_parent( gtk_widget_get_parent( GTK_WIDGET(wEditable))));
    gchar *sCalProfleName = gtk_combo_box_text_get_active_text(wCalibrationComboBox);

    g_signal_handlers_block_by_func(G_OBJECT(wCalibrationComboBox), CB_cbt_CalibrationProfileName, NULL);
    g_signal_handlers_block_by_func(G_OBJECT(wEditable), CB_editable_CalibrationProfileName, NULL);

    // setGtkComboBox will return TRUE if the profile exists
    sensitizeRecallSaveDeleteButtons( pGlobal );

    if( setGtkComboBox( GTK_COMBO_BOX( wCalibrationComboBox ), sCalProfleName ) ) {
        pGlobal->pCalibrationAbstract = selectCalibrationProfile( pGlobal, pGlobal->sProject,
                sCalProfleName );

        // show the calibration information relevent to the selection
        showCalInfo( pGlobal->pCalibrationAbstract, pGlobal );
        gtk_widget_set_sensitive ( pGlobal->widgets[ eW_nbCal_box_CalInfo ], FALSE);
        gtk_text_buffer_set_text( wTBnote, pGlobal->pCalibrationAbstract->sNote, STRLENGTH);
    } else {
        // Clear the calibration information area (as the edit box text does not relate to a saved profile
        gtk_text_buffer_set_text(
                gtk_text_view_get_buffer( GTK_TEXT_VIEW( pGlobal->widgets[ eW_nbCal_txtV_CalInfoCh1 ] )), "", -1 );
        gtk_text_buffer_set_text(
                gtk_text_view_get_buffer( GTK_TEXT_VIEW( pGlobal->widgets[ eW_nbCal_txtV_CalInfoCh2 ] )), "", -1 );
    }

    gtk_widget_add_css_class( GTK_WIDGET( wCalNote ), "italicFont" );

    g_signal_handlers_unblock_by_func(G_OBJECT(wCalibrationComboBox), CB_cbt_CalibrationProfileName, NULL);
    g_signal_handlers_unblock_by_func(G_OBJECT(wEditable), CB_editable_CalibrationProfileName, NULL);

    g_free( sCalProfleName );
}

/*!     \brief  Callback Trace selection of GtkComboBoxText
 *
 * Callback (MD10) when the Trace profile GtkComboBoxText is changed
 *
 * \param  wTraceSelection    pointer to GtkComboBox widget
 * \param  udate              unused
 */
void
CB_cbt_TraceProfileName( GtkComboBoxText *wTraceSelection, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wTraceSelection ), "data");

    tProjectAndName *pProjectAndName;
    gchar *sTraceProfileName = NULL;
    sTraceProfileName = gtk_combo_box_text_get_active_text(wTraceSelection);

    for( GList *l = pGlobal->pTraceList; l != NULL; l = l->next ){
        pProjectAndName = &(((tHP8753traceAbstract *)(l->data))->projectAndName);

        if( g_strcmp0(  pProjectAndName->sProject, pGlobal->sProject ) == 0 ) {
            pProjectAndName->bbFlags.bSelected = FALSE;

            if( g_strcmp0(  pProjectAndName->sName, sTraceProfileName ) == 0 ) {
                pProjectAndName->bbFlags.bSelected = TRUE;
            }
        }
    }
    g_free( sTraceProfileName );
}

/*!     \brief  Callback when user types in the ComboBoxText (editable) for the trace name
 *
 * Callback (MD11) when user types in the ComboBoxText (editable) for the trace name
 *
 * \param  wEditable    pointer to GtkCheckBoxText widget
 * \param  udata        unused
 */
void
CB_editable_TraceProfileName( GtkEditable *wEditable, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wEditable ), "data");

    GtkWidget *wTraceNote = GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_txtV_TraceNote ] );
    GtkTextBuffer* wTBnote =  gtk_text_view_get_buffer( GTK_TEXT_VIEW( wTraceNote));
    GtkEntry* wTEtitle = GTK_ENTRY( pGlobal->widgets[ eW_nbTrace_entry_Title ] );
    GtkComboBoxText *wTraceComboBox =
            GTK_COMBO_BOX_TEXT(gtk_widget_get_parent( gtk_widget_get_parent( GTK_WIDGET(wEditable))));
    gchar *sTraceProfileName = gtk_combo_box_text_get_active_text(wTraceComboBox);

    g_signal_handlers_block_by_func(G_OBJECT(wTraceComboBox), CB_cbt_TraceProfileName, NULL);
    g_signal_handlers_block_by_func(G_OBJECT(wEditable), CB_editable_TraceProfileName, NULL);

    gtk_notebook_set_current_page ( GTK_NOTEBOOK( pGlobal->widgets[ eW_notebook ]), NPAGE_TRACE );

    sensitizeRecallSaveDeleteButtons( pGlobal );
    // setGtkComboBox will return TRUE if the profile exists

    if( setGtkComboBox( GTK_COMBO_BOX( wTraceComboBox ), sTraceProfileName ) ) {
        pGlobal->pTraceAbstract = selectTraceProfile( pGlobal, pGlobal->sProject, sTraceProfileName );

        // show the trace information relevant to the selection
        gtk_text_buffer_set_text( wTBnote, pGlobal->pTraceAbstract->sNote, STRLENGTH);

        // Block signals while we populate the widgets programmatically
        // So the title on the currently displayed plot doesn't change
        g_signal_handlers_block_by_func(G_OBJECT(wTEtitle), CB_entry_Title_Changed, NULL);
        // Don't update pGlobal->HP8753.sTitle until we 'recall' otherwise resizing will show the wrong title on the plot
        // pGlobal->HP8753.sTitle = titleAbstract->sTitle;
        gtk_entry_buffer_set_text( gtk_entry_get_buffer( GTK_ENTRY( wTEtitle ) ),
                pGlobal->pTraceAbstract ? pGlobal->pTraceAbstract->sTitle : "", -1);

        g_signal_handlers_unblock_by_func(G_OBJECT(wTEtitle), CB_entry_Title_Changed, NULL);

        // We are using the CSS to detect whether we have a dark or light desktop
        // see the code in hp8753c on_activate
        // change date/Time color based on dark or light theme
        gtk_label_set_label ( GTK_LABEL( pGlobal->widgets[ eW_nbTrace_lbl_Time ] ), pGlobal->pTraceAbstract->sDateTime );
    }

    gtk_widget_add_css_class( GTK_WIDGET( wTEtitle ), "italicFont" );
    gtk_widget_add_css_class( GTK_WIDGET( wTraceNote ), "italicFont" );

    g_signal_handlers_unblock_by_func(G_OBJECT(wTraceComboBox), CB_cbt_TraceProfileName, NULL);
    g_signal_handlers_unblock_by_func(G_OBJECT(wEditable), CB_editable_TraceProfileName, NULL);

    g_free( sTraceProfileName );
}

/*!     \brief  Callback when selects Calibration or Trace GtkRadioButton
 *
 * Callback (MD12) when selects Calibration or Trace GtkRadioButton.
 * This will also sensitize the appropriate GtkComboxBoxText &
 * change the GtkNoteBook page to the calibration or trace information / notes.
 *
 * \param  wCalibration pointer to radio button widget
 * \param  udata        unused
 */
void
CB_rbtn_Calibration ( GtkCheckButton *wCalibration, gpointer udata ) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wCalibration ), "data");

    gboolean bFound = FALSE;
    GtkWidget *wCalibrationProfile = GTK_WIDGET( pGlobal->widgets[ eW_cbt_CalProfile ] );
    GtkWidget *wTraceProfile = GTK_WIDGET( pGlobal->widgets[ eW_cbt_TraceProfile ] );
    GtkButton       *wBtnRecall = GTK_BUTTON( pGlobal->widgets[ eW_btn_Recall ] );
    GtkButton       *wBtnDelete = GTK_BUTTON( pGlobal->widgets[ eW_btn_Delete ] );
    GtkButton       *wBtnSave = GTK_BUTTON( pGlobal->widgets[ eW_btn_Save ] );
    GtkNotebook     *wNotebook = GTK_NOTEBOOK( pGlobal->widgets[ eW_notebook ] );

    if( gtk_check_button_get_active( GTK_CHECK_BUTTON( wCalibration )) == 0) {
        return;
    }
    pGlobal->flags.bCalibrationOrTrace = TRUE;
    pGlobal->flags.bProject = FALSE;

    gtk_widget_set_sensitive( wCalibrationProfile,  TRUE );
    gtk_widget_set_sensitive( wTraceProfile, FALSE );

    if ( pGlobal->pCalibrationAbstract )
        bFound = setGtkComboBox( GTK_COMBO_BOX(wCalibrationProfile),
                pGlobal->pCalibrationAbstract->projectAndName.sName );

    gtk_widget_set_sensitive( GTK_WIDGET( wBtnRecall ), bFound );
    gtk_widget_set_sensitive( GTK_WIDGET( wBtnDelete ), bFound );
    gtk_widget_set_sensitive( GTK_WIDGET( wBtnSave ), pGlobal->pCalibrationAbstract != NULL );

    gtk_notebook_set_current_page( wNotebook, NPAGE_CALIBRATION );

    gtk_button_set_label( wBtnRecall, "restore ⚖" );
    gtk_button_set_label( wBtnSave, "save ⚖" );
    gtk_button_set_label( wBtnDelete, "delete ⚖" );

    sensitizeRecallSaveDeleteButtons( pGlobal ) ;
}

/*!     \brief  Callback when selects Trace GtkCheckButton (in ragio group)
 *
 * Callback (MD13) when selects Trace GtkRadioButton.
 * This will also sensitize the appropriate GtkComboxBoxText &
 * change the GtkNoteBook page to the calibration or trace information / notes.
 *
 * \param  wTrace pointer to radio button widget
 * \param  tGlobal      pointer global data
 */
void
CB_rbtn_Traces ( GtkCheckButton *wTrace, gpointer udata) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wTrace ), "data");

    gboolean bFound = FALSE;
    GtkWidget *wCalibrationProfile = GTK_WIDGET( pGlobal->widgets[ eW_cbt_CalProfile ] );
    GtkWidget *wTraceProfile = GTK_WIDGET( pGlobal->widgets[ eW_cbt_TraceProfile ] );
    GtkButton *wBtnRecall = GTK_BUTTON( pGlobal->widgets[ eW_btn_Recall ] );
    GtkButton *wBtnDelete = GTK_BUTTON( pGlobal->widgets[ eW_btn_Delete ] );
    GtkButton *wBtnSave = GTK_BUTTON( pGlobal->widgets[ eW_btn_Save ] );
    GtkNotebook *wNotebook = GTK_NOTEBOOK( pGlobal->widgets[ eW_notebook ] );

    if( gtk_check_button_get_active( GTK_CHECK_BUTTON( wTrace )) == 0) {
        return;
    }
    pGlobal->flags.bCalibrationOrTrace = FALSE;
    pGlobal->flags.bProject = FALSE;

    gtk_widget_set_sensitive( wCalibrationProfile,  FALSE );
    gtk_widget_set_sensitive( wTraceProfile, TRUE );

    if ( pGlobal->pTraceAbstract )
        bFound = setGtkComboBox( GTK_COMBO_BOX(wTraceProfile),
                pGlobal->pTraceAbstract->projectAndName.sName );

    gtk_widget_set_sensitive( GTK_WIDGET( wBtnRecall ), bFound );
    gtk_widget_set_sensitive( GTK_WIDGET( wBtnDelete ), bFound );

    gtk_widget_set_sensitive(
        GTK_WIDGET( wBtnSave ), pGlobal->pTraceAbstract &&
        (globalData.HP8753.channels[ eCH_ONE ].chFlags.bValidData
                    || globalData.HP8753.channels[ eCH_TWO ].chFlags.bValidData) );

    gtk_notebook_set_current_page( wNotebook,
            pGlobal->flags.bCalibrationOrTrace ? NPAGE_CALIBRATION:NPAGE_TRACE);

    gtk_button_set_label( wBtnRecall, "recall 📈" );
    gtk_button_set_label( wBtnSave, "save 📈" );
    gtk_button_set_label( wBtnDelete, "delete 📈" );

    sensitizeRecallSaveDeleteButtons( pGlobal ) ;
}


/*!     \brief  Callback from alert dialog when attempting to overwrite file
*
* Callback from alert dialog when attempting to overwrite file
*
* \param  source_object     GtkFileDialog object
* \param  res               result of opening file for write
* \param  gpGlobal          pointer to global data
*/
void
CB_adlg_Remove_choice( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    GtkAlertDialog *dialog = GTK_ALERT_DIALOG (source_object);
    tGlobal *pGlobal = (tGlobal *)gpGlobal;
    gchar *name=NULL;

    int button = gtk_alert_dialog_choose_finish (dialog, res, NULL);

    // Did we cancel?
    if ( button == 0 ) {
        gtk_label_set_text( pGlobal->widgets[ eW_lbl_Status], "Cancelled");
        return;
    }


    if( pGlobal->flags.bCalibrationOrTrace ) {
        name = pGlobal->pCalibrationAbstract->projectAndName.sName;
    } else {
        name = pGlobal->pTraceAbstract->projectAndName.sName;
    }

    if( deleteDBentry( pGlobal, pGlobal->sProject, name,
            pGlobal->flags.bCalibrationOrTrace ? eDB_CALandSETUP : eDB_TRACE ) == 0 ) {
        // deleteDBentry deletes the actual item, so we have to locate the list entry now
        if( pGlobal->flags.bCalibrationOrTrace ) {
            pGlobal->pCalibrationAbstract = NULL;

            populateCalComboBoxWidget( pGlobal );
            // We've deleted the calibration profile from this project
            // so we are not looking at, so just select the first cal item
            gtk_combo_box_set_active( GTK_COMBO_BOX( pGlobal->widgets[ eW_cbt_CalProfile] ), 0);
            // also update the calibration pointer to the first profile in the list
            // that matches the project
            pGlobal->pCalibrationAbstract = selectFirstCalibrationProfileInProject( pGlobal );
        } else {
            pGlobal->pTraceAbstract = NULL;

            populateTraceComboBoxWidget( pGlobal );
            // We've deleted the calibration profile from this project
            // so we are not looking at, so just select the first cal item
            gtk_combo_box_set_active( GTK_COMBO_BOX( GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_cbt_TraceProfile]  )), 0);
            // also update the calibration pointer to the first profile in the list
            // that matches the project
            pGlobal->pTraceAbstract = selectFirstTraceProfileInProject( pGlobal );
        }

#if 0
        // If there are no more calibrations and traces in this project - also delete the project
        if( isProjectEmpty( pGlobal, pGlobal->sProject ) ) {
            GList *pThisProject = g_list_find_custom (pGlobal->pProjectList, pGlobal->sProject, (GCompareFunc) strcmp );
            if( pThisProject ) {
                g_free(  pThisProject->data );
                pGlobal->pProjectList = g_list_remove( pGlobal->pProjectList, pThisProject->data );
                populateProjectComboBoxWidget( pGlobal );
            }
        }
#endif
    }
}


/*!     \brief  Callback from alert dialog when attempting to overwrite file
*
* Callback from alert dialog when attempting to overwrite file
*
* \param  source_object     GtkFileDialog object
* \param  res               result of opening file for write
* \param  gpGlobal          pointer to global data
*/
static gint
saveCalibrationOrTrace( gboolean bCalibrationOrTrace, tGlobal *pGlobal ) {

    GtkComboBoxText *wComboBoxTextProfile;
    gchar *sProfileName, *sNote;
    GtkWidget *wTraceNote = NULL;
    GtkTextBuffer* wTBnote;
    GtkTextIter start, end;
    tProjectAndName projectAndName;
    gint saveStatus = ERROR;

    if( bCalibrationOrTrace ) {
        wComboBoxTextProfile = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_cbt_CalProfile ] );
        wTBnote = gtk_text_view_get_buffer( GTK_TEXT_VIEW( pGlobal->widgets[ eW_nbCal_txtV_CalibrationNote ]));

    } else {
        wComboBoxTextProfile = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_cbt_TraceProfile ] );
        wTraceNote = GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_txtV_TraceNote ] );
        wTBnote = gtk_text_view_get_buffer( GTK_TEXT_VIEW( wTraceNote ));
    }
    // This may be a new name or one selected from the combobox list
    sProfileName = gtk_combo_box_text_get_active_text( wComboBoxTextProfile );
    projectAndName.sProject = pGlobal->sProject;
    projectAndName.sName = sProfileName;
    projectAndName.bbFlags.bSelected = FALSE;

    // The text in the note field
    gtk_text_buffer_get_start_iter(wTBnote, &start);
    gtk_text_buffer_get_end_iter(wTBnote, &end);
    sNote = gtk_text_buffer_get_text(wTBnote, &start, &end, FALSE);


    if( pGlobal->flags.bCalibrationOrTrace ) {
        g_free( pGlobal->HP8753cal.sNote );
        pGlobal->HP8753cal.sNote = sNote;
        // send message to GPIB tread to get calibration data.
        // If this completes correctly, the main thrtead receives a message that will save the data
        postDataToGPIBThread (TG_RETRIEVE_SETUPandCAL_from_HP8753, g_strdup( sProfileName ));
        sensitiseControlsInUse( pGlobal, FALSE );
        // The abstract list is updated if the data is retrieved from the analyzer correctly
    } else {
        g_free( pGlobal->HP8753.sNote );
        pGlobal->HP8753.sNote = sNote;
        saveStatus = saveTraceData(pGlobal, pGlobal->sProject, sProfileName);
        // add to the list
        GList *liTraceAbstract = g_list_find_custom( pGlobal->pTraceList, &projectAndName,
                (GCompareFunc)compareTraceItemsForFind );
        if( liTraceAbstract ) {
            // This is an existing profile ... just update the abstract
            tHP8753traceAbstract *pTraceAbstract = (tHP8753traceAbstract *)liTraceAbstract->data;
            g_free( pTraceAbstract->sTitle );
            pTraceAbstract->sTitle = g_strdup( pGlobal->HP8753.sTitle );
            g_free( pTraceAbstract->sNote );
            pTraceAbstract->sNote = g_strdup( pGlobal->HP8753.sNote );
            g_free( pTraceAbstract->sDateTime );
            pTraceAbstract->sDateTime = g_strdup( pGlobal->HP8753.dateTime );
        } else {
            // This is a new profile ... create the abstract
            tHP8753traceAbstract *pTraceAbstract = g_new0( tHP8753traceAbstract, 1 );
            pTraceAbstract->projectAndName.sProject = g_strdup( pGlobal->sProject );
            pTraceAbstract->projectAndName.sName = g_strdup( sProfileName );
            pTraceAbstract->sTitle = g_strdup( pGlobal->HP8753.sTitle );
            pTraceAbstract->sNote = g_strdup( pGlobal->HP8753.sNote );
            pTraceAbstract->sDateTime = g_strdup( pGlobal->HP8753.dateTime );
            pGlobal->pTraceList = g_list_prepend(pGlobal->pTraceList, pTraceAbstract);
            pGlobal->pTraceList = g_list_sort (pGlobal->pTraceList, (GCompareFunc)compareTraceItemsForSort);
            gtk_combo_box_text_remove_all ( wComboBoxTextProfile  );
            for( GList *l = pGlobal->pTraceList; l != NULL; l = l->next ){
                if( g_strcmp0( ((tHP8753traceAbstract *)l->data)->projectAndName.sProject, pGlobal->sProject ) == 0 )
                    gtk_combo_box_text_append_text( wComboBoxTextProfile, ((tHP8753traceAbstract *)l->data)->projectAndName.sName );
            }
            if( !g_list_find_custom (pGlobal->pProjectList, pGlobal->sProject, (GCompareFunc) strcmp ) ) {
                // This is also a new project
                pGlobal->pProjectList = g_list_prepend( pGlobal->pProjectList, g_strdup( pGlobal->sProject ) );
                pGlobal->pProjectList = g_list_sort (pGlobal->pProjectList, (GCompareFunc)g_strcmp0);
                populateProjectComboBoxWidget( pGlobal );
            }
        }
        pGlobal->pTraceAbstract = g_list_find_custom( pGlobal->pTraceList, &projectAndName, (GCompareFunc)compareTraceItemsForFind )->data;

        gtk_widget_set_sensitive( pGlobal->widgets[ eW_btn_Recall ] , TRUE);
        gtk_widget_set_sensitive(  pGlobal->widgets[ eW_btn_Delete ], TRUE);

        if( saveStatus == 0 ) { // successful save
            // Restore the color of the title entry window
            gtk_widget_remove_css_class( GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_entry_Title ] ), "italicFont" );
            gtk_widget_remove_css_class( GTK_WIDGET( wTraceNote ), "italicFont" );
        }
        gtk_label_set_text( pGlobal->widgets[ eW_lbl_Status], "Saved");
    }
    return( saveStatus );
}

/*!     \brief  Callback from alert dialog when attempting to overwrite file
*
* Callback from alert dialog when attempting to overwrite file
*
* \param  source_object     GtkFileDialog object
* \param  res               result of opening file for write
* \param  gpGlobal          pointer to global data
*/
static void
CB_adlg_Overwrite_choice( GObject *source_object, GAsyncResult *res, gpointer gpGlobal ) {
    GtkAlertDialog *dialog = GTK_ALERT_DIALOG (source_object);
    tGlobal *pGlobal = (tGlobal *)gpGlobal;

    int button = gtk_alert_dialog_choose_finish (dialog, res, NULL);

    // Did we cancel?
    if ( button == 0 ) {
        gtk_label_set_text( pGlobal->widgets[ eW_lbl_Status], "Cancelled");
        return;
    }

    // OK, lets do it
    saveCalibrationOrTrace( pGlobal->flags.bCalibrationOrTrace, pGlobal );
}

/*!     \brief  Callback for Save button
*
* Callback (MD14) for Save button
*
* \param  wButton     pointer to button widget
* \param  udata       unused
*/
void
CB_btn_Save (GtkButton *wButton, gpointer udata)
{
    GtkComboBoxText *wComboBoxTextProfile;
    gchar *sProfileName;

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wButton ), "data");

    if( pGlobal->flags.bCalibrationOrTrace ) {
        wComboBoxTextProfile = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_cbt_CalProfile ] );
    } else {
        wComboBoxTextProfile = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_cbt_TraceProfile ] );
    }

    // This may be a new name or one selected from the combobox list
    sProfileName = gtk_combo_box_text_get_active_text( wComboBoxTextProfile );

    if ( strlen( sProfileName ) != 0 ) {
        gboolean bFound;
        tProjectAndName projectAndName = { pGlobal->sProject, sProfileName, {FALSE, FALSE} };

        // Look to see if we are replacing an existing profile
        if( pGlobal->flags.bCalibrationOrTrace ) {
            bFound = (g_list_find_custom( pGlobal->pCalList, &projectAndName,
                    (GCompareFunc)compareCalItemsForFind ) != NULL);
        } else {
            bFound = (g_list_find_custom( pGlobal->pTraceList, &projectAndName,
                    (GCompareFunc)compareTraceItemsForFind ) != NULL);
        }

        // Warn before overwriting
        if( bFound ) {
            GtkAlertDialog *dialog = gtk_alert_dialog_new ("Caution");
            const char* buttons[] = {"Cancel", "Proceed", NULL};
            gtk_alert_dialog_set_detail(dialog, "This profile already exists.\n\nAre you sure you want to replace it?");
            gtk_alert_dialog_set_buttons (dialog, buttons);
            gtk_alert_dialog_set_cancel_button (dialog, 0);   // (ESC) equivalent to button 0 (Cancel)
            gtk_alert_dialog_set_default_button (dialog, 1);  // (Enter) equivalent to button 1 (Proceed)
            gtk_window_present (GTK_WINDOW (pGlobal->widgets[ eW_hp8753_main ]));
            gtk_alert_dialog_choose (dialog, GTK_WINDOW (pGlobal->widgets[ eW_hp8753_main ]), NULL, CB_adlg_Overwrite_choice, (gpointer) pGlobal);
        } else {
            saveCalibrationOrTrace( pGlobal->flags.bCalibrationOrTrace, pGlobal );
        }
    } else {
        gtk_label_set_text( pGlobal->widgets[ eW_lbl_Status], "Please provide profile name.");
    }
    g_free( sProfileName );
}

/*!     \brief  Callback for 'Recall' button
 *
 * Callback (MD15) for 'Recall' button
 *
 * \param  wRecallBtn   pointer button widget
 * \param udata         unused
 */
void
CB_btn_Recall (GtkButton *wRecallBtn, gpointer udata)
{
    GtkComboBoxText *cbSetup;
    gchar *name;
    gint rtn;

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wRecallBtn ), "data");

    // Release live marker
    pGlobal->flags.bHoldLiveMarker = FALSE;

    if( pGlobal->flags.bCalibrationOrTrace )
        cbSetup = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_cbt_CalProfile ] );
    else
        cbSetup = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_cbt_TraceProfile ] );
    name = gtk_combo_box_text_get_active_text( cbSetup );

    if ( name && strlen( name ) != 0 ) {
        if( pGlobal->flags.bCalibrationOrTrace ) {
            if( recoverCalibrationAndSetup( pGlobal, pGlobal->sProject, (gchar *)name ) != ERROR ) {
                GtkTextBuffer* wTBnote =  gtk_text_view_get_buffer( GTK_TEXT_VIEW( pGlobal->widgets[ eW_nbCal_txtV_CalibrationNote ] ));
                gtk_text_buffer_set_text( wTBnote,
                        pGlobal->HP8753cal.sNote ? pGlobal->HP8753cal.sNote : "", STRLENGTH );
                GtkWidget *wCalNote = pGlobal->widgets[ eW_nbCal_txtV_CalibrationNote ];
                gtk_widget_remove_css_class( GTK_WIDGET( wCalNote ), "italicFont" );
                postDataToGPIBThread (TG_SEND_SETUPandCAL_to_HP8753, NULL );
            }
            sensitiseControlsInUse( pGlobal, FALSE );
            gtk_widget_set_sensitive ( GTK_WIDGET( pGlobal->widgets[ eW_nbCal_box_CalInfo ] ), TRUE);
        } else {
            if( (rtn = recoverTraceData( pGlobal, pGlobal->sProject, (gchar *)name )) != ERROR ) {
                if( rtn == FALSE )
                    clearHP8753traces( &pGlobal->HP8753 );
                GtkWidget *wTraceNote = GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_txtV_TraceNote ]);
                GtkTextBuffer* wTBnote =  gtk_text_view_get_buffer( GTK_TEXT_VIEW( wTraceNote ));
                gtk_text_buffer_set_text( wTBnote, pGlobal->HP8753.sNote ? pGlobal->HP8753.sNote : "", STRLENGTH );
                GtkWidget *wEntryTitle = GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_entry_Title ] );
                gtk_entry_buffer_set_text( gtk_entry_get_buffer( GTK_ENTRY( wEntryTitle ) ), pGlobal->HP8753.sTitle ? pGlobal->HP8753.sTitle : "", -1);
                if (!pGlobal->HP8753.flags.bDualChannel || !pGlobal->HP8753.flags.bSplitChannels) {
                    postDataToMainLoop(TM_REFRESH_TRACE, 0);
                } else {
                    postDataToMainLoop(TM_REFRESH_TRACE, 0);
                    postDataToMainLoop(TM_REFRESH_TRACE, (void*) 1);
                }

                // Show whichever trace was showing when saved (High Resolution or HPGL)
                GtkWidget *wRadioHPGLplot = GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_rbtn_PlotTypeHPGL ] );
                GtkWidget *wRadioHIRESplot = GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_rbtn_PlotTypeHighRes ] );
                GtkWidget *wBoxPlotType = GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_box_PlotType ] );

                if( pGlobal->HP8753.plotHPGL && pGlobal->HP8753.flags.bShowHPGLplot )
                    gtk_check_button_set_active( GTK_CHECK_BUTTON(wRadioHPGLplot), TRUE );
                else
                    gtk_check_button_set_active( GTK_CHECK_BUTTON(wRadioHIRESplot), TRUE );

                if( pGlobal->HP8753.plotHPGL == NULL )
                    gtk_widget_hide (GTK_WIDGET( wBoxPlotType ));
                else
                    gtk_widget_show (GTK_WIDGET( wBoxPlotType ));

                // Restore the color of the title entry window
                gtk_widget_remove_css_class( GTK_WIDGET( wEntryTitle ), "italicFont" );
                gtk_widget_remove_css_class( GTK_WIDGET( wTraceNote ), "italicFont" );
            }
        }
        gtk_notebook_set_current_page ( GTK_NOTEBOOK( pGlobal->widgets[ eW_notebook ] ),
                pGlobal->flags.bCalibrationOrTrace ? NPAGE_CALIBRATION : NPAGE_TRACE );
    } else {
        gtk_label_set_text( GTK_LABEL( pGlobal->widgets[ eW_lbl_Status ] ), "Please provide profile name.");
    }
    g_free( name );

}

/*!     \brief  Callback for Delete button
*gtkcombobox selects charactes
* Callback (MD16) for Delete button
*
* \param  wButton     pointer to button widget
* \param  udata       unused
*/
void
CB_btn_Delete (GtkButton *wButton, gpointer udata)
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wButton ), "data");

    gchar *sanitizedName = NULL, *name=NULL;
    gchar *sQuestion = NULL;

    if( pGlobal->flags.bCalibrationOrTrace ) {
        name = pGlobal->pCalibrationAbstract->projectAndName.sName;
        sanitizedName = g_markup_escape_text ( name, -1 );
        sQuestion = g_strdup_printf(
                "You look as though you know what you are doing but..."
                "\n\t\t\t\t\t...are you sure you want to delete the:\n\n"
                "\t\"%s\"\n\n⚖️ calibration profile?", sanitizedName);
    } else {
        name = pGlobal->pTraceAbstract->projectAndName.sName;
        sanitizedName = g_markup_escape_text ( name, -1 );
        sQuestion = g_strdup_printf(
                "You look as though you know what you are doing but..."
                "\n\t\t\t\t\t...are you sure you want to delete the:\n\n"
                "\t\"%s\"\n\n📈 trace profile?", sanitizedName);
    }

    if ( sanitizedName != NULL && strlen( sanitizedName ) != 0 ) {

        GtkAlertDialog *dialog = gtk_alert_dialog_new ("Caution");
        const char* buttons[] = {"Cancel", "Proceed", NULL};
        gtk_alert_dialog_set_detail(dialog, sQuestion);
        gtk_alert_dialog_set_buttons (dialog, buttons);
        gtk_alert_dialog_set_cancel_button (dialog, 0);   // (ESC) equivalent to button 0 (Cancel)
        gtk_alert_dialog_set_default_button (dialog, 1);  // (Enter) equivalent to button 1 (Proceed)
        gtk_window_present (GTK_WINDOW (pGlobal->widgets[ eW_hp8753_main ]));
        gtk_alert_dialog_choose (dialog, GTK_WINDOW (pGlobal->widgets[ eW_hp8753_main ]), NULL, CB_adlg_Remove_choice, (gpointer) pGlobal);

    } else {
        gtk_label_set_text( GTK_LABEL( pGlobal->widgets[ eW_lbl_Status] ), "Please provide profile name.");
    }

    g_free( sanitizedName );
    g_free( sQuestion );
}

/*!     \brief  Callback / GtkNotebook page changed
 *
 * Callback (MD17) when the GtkNotebook page has changed (Calibration/Trace/Data/Options/GPIB/Cal. Kits)
 *
 * \param  wNotebook    pointer to GtkNotebook widget
 * \param  wPage        pointer to the current page (box or scrolled widget)
 * \param  nPage        which page was selected
 * \param  udata        unused
 */
void
CB_notebook_Select( GtkNotebook *wNotebook, GtkWidget* wPage,
          guint nPage, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wNotebook ), "data");

    if( nPage == 0 ) {
        g_signal_emit_by_name( GTK_BUTTON( pGlobal->widgets[ eW_rbtn_Cal ] ), "activate", NULL );
    } else if ( nPage == 1 ) {
        g_signal_emit_by_name( GTK_BUTTON( pGlobal->widgets[ eW_rbtn_Traces ] ), "activate", NULL );
        // This ruins the arrow tab selection .. so never mind
        // gtk_entry_grab_focus_without_selecting (GTK_ENTRY( pGlobal->widgets[ eW_nbTrace_entry_Title ]));
    }
}

/*!     \brief  Callback when the "Get Trace" button is destroyed.
 *
 * Callback when the "Get Trace" button is destroyed. We use this because it occurs befor
 * the notebook is destroyed. We need to stop the signal that changes the
 * cal/trace radio button because it is triggered during shutdown.
 *
 * \param pGlobal pointer to global data
 * \return        number of projects
 */
void
CB_btn_GetTrace_Destroy (
  GtkWidget* wWidget,  gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wWidget ), "data");
    g_signal_handlers_block_by_func(G_OBJECT(pGlobal->widgets[ eW_notebook ]), CB_notebook_Select, NULL);
}

/*!     \brief  Populate the project combo box widget
 *
 * populate the project combobox widget
 *
 * \param pGlobal pointer to global data
 * \return        number of projects
 */
int
populateProjectComboBoxWidget( tGlobal * pGlobal ) {
    GList *l;
    gint nProjects = 0, nPos=0;
    GtkComboBoxText *wComboBoxProject = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_cbt_Project ] );

    gtk_combo_box_text_remove_all( wComboBoxProject );
    for( l = pGlobal->pProjectList, nPos = 0, nProjects = 0; l != NULL; l = l->next, nProjects++ ){
        gtk_combo_box_text_append_text( wComboBoxProject, (gchar *)l->data );
        if( g_strcmp0( (gchar *)l->data, pGlobal->sProject) == 0 )
            nPos = nProjects;
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX(wComboBoxProject), nPos );
    return( nProjects );
}


/*!     \brief  Populate the setup & trace combo box widgets
 *
 * populate the setup & trace combobox widgets based on the selected project
 *
 * \param pGlobal pointer to global data
 * \return        number of setups in this project
 */
int
populateCalComboBoxWidget( tGlobal *pGlobal ) {
    GtkComboBoxText *wComboBoxCalibration = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_cbt_CalProfile ]  );
    GtkNotebook *wNotebook = GTK_NOTEBOOK( pGlobal->widgets[ eW_notebook ] );

    gint nPos, nItems;
    gboolean bFound;
    GList *l;

    // Block signals while we populate the widgets programmatically
    g_signal_handlers_block_by_func(G_OBJECT(wComboBoxCalibration), CB_cbt_CalibrationProfileName, NULL);
    g_signal_handlers_block_by_func(G_OBJECT(wNotebook), CB_notebook_Select, NULL);

    gtk_combo_box_text_remove_all( wComboBoxCalibration );
    gtk_editable_set_text( GTK_EDITABLE( gtk_combo_box_get_child(GTK_COMBO_BOX( wComboBoxCalibration )) ), "" );

    for( l = pGlobal->pCalList, nPos=0, nItems=0, bFound=FALSE; l != NULL; l = l->next ){
        tProjectAndName *pProjectAndName = &(((tHP8753cal *)l->data)->projectAndName);
        if( g_strcmp0(  pProjectAndName->sProject, pGlobal->sProject ) == 0 ) {
            nItems++;
            gtk_combo_box_text_append_text( wComboBoxCalibration, pProjectAndName->sName );
            if( !bFound ) {
                if( pProjectAndName->bbFlags.bSelected )
                    bFound = TRUE;
                else
                    nPos++;
            }
        }
    }

    if( !bFound && nItems > 0 ) {

    }
    if( bFound ) {
        gtk_combo_box_set_active( GTK_COMBO_BOX(wComboBoxCalibration), nPos);
    } else if ( nItems > 0 ) {
        gtk_combo_box_set_active( GTK_COMBO_BOX(wComboBoxCalibration), 0);
        ((tHP8753cal *)(pGlobal->pCalList->data))->projectAndName.bbFlags.bSelected = TRUE;
    } else {
        gtk_entry_buffer_set_text( gtk_entry_get_buffer( GTK_ENTRY( wComboBoxCalibration) ), "", -1 );
    }

    // Re-enable "changed" signals for drop comboboxes
    g_signal_handlers_unblock_by_func(G_OBJECT(wComboBoxCalibration), CB_cbt_CalibrationProfileName, NULL );
    g_signal_handlers_unblock_by_func(G_OBJECT(wNotebook), CB_notebook_Select, NULL );

    return( gtk_tree_model_iter_n_children(gtk_combo_box_get_model( GTK_COMBO_BOX(wComboBoxCalibration)), NULL) );
}

/*!     \brief  Populate the setup & trace combo box widgets
 *
 * populate the setup & trace combobox widgets based on the selected project
 *
 * \param pGlobal pointer to global data
 * \return        number of traces in this project
 */
int
populateTraceComboBoxWidget( tGlobal *pGlobal ) {
    GtkComboBoxText *wComboBoxTrace = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_cbt_TraceProfile ]  );
    GtkNotebook *wNotebook = GTK_NOTEBOOK( pGlobal->widgets[ eW_notebook ] );

    gint nPos, nItems;
    gboolean bFound;
    GList *l;

    // Block signals while we populate the widgets programatically
    g_signal_handlers_block_by_func( G_OBJECT(wComboBoxTrace), CB_cbt_TraceProfileName, NULL );
    g_signal_handlers_block_by_func( G_OBJECT(wNotebook), CB_notebook_Select, NULL );

    // Fill in combobox with the available trace names recovered from the sqlite3 database
    gtk_combo_box_text_remove_all( wComboBoxTrace );
    gtk_editable_set_text( GTK_EDITABLE( gtk_combo_box_get_child(GTK_COMBO_BOX( wComboBoxTrace )) ), "" );

    for( l = pGlobal->pTraceList, nPos=0, nItems=0, bFound=FALSE; l != NULL; l = l->next ){
        tProjectAndName *pProjectAndName = &(((tHP8753traceAbstract *)(l->data))->projectAndName);
        if( g_strcmp0(  pProjectAndName->sProject, pGlobal->sProject ) == 0 ) {
            nItems++;
            gtk_combo_box_text_append_text( wComboBoxTrace, pProjectAndName->sName );
            if( !bFound ) {
                if( pProjectAndName->bbFlags.bSelected )
                    bFound = TRUE;
                else
                    nPos++;
            }
        }
    }
    if( bFound ) {
        gtk_combo_box_set_active( GTK_COMBO_BOX(wComboBoxTrace), nPos);
    } else if ( nItems > 0 ) {
        gtk_combo_box_set_active( GTK_COMBO_BOX(wComboBoxTrace), 0);
        ((tHP8753traceAbstract *)(pGlobal->pTraceList->data))->projectAndName.bbFlags.bSelected = TRUE;
    } else {
        gtk_entry_buffer_set_text( gtk_entry_get_buffer( GTK_ENTRY( wComboBoxTrace ) ), "", -1 );
    }

    // Re-enable "changed" signals for drop comboboxes
    g_signal_handlers_unblock_by_func(G_OBJECT(wComboBoxTrace), CB_cbt_TraceProfileName, NULL);
    g_signal_handlers_unblock_by_func(G_OBJECT(wNotebook), CB_notebook_Select, NULL);

    return( gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(wComboBoxTrace)), NULL) );
}


/*!     \brief  Callback when the connected GtkComboBoxText with GtkEntry looses focus
 *
 * We use this to deselect the text in the entry widget
 *
 * \param   controller  controller for the GtkComboBox
 * \return  gpGlobal    gpointer to  the global data  (passed as user data)
 */
static void
CB_cbt_Unfocus( GtkEventControllerFocus *controller, gpointer gpGlobal  ) {
    // This function will be called when the widget or its children lose focus.
    // You can access the widget the controller is attached to via
    GtkWidget *wCombo = gtk_event_controller_get_widget( GTK_EVENT_CONTROLLER( controller ));
    gtk_editable_select_region(GTK_EDITABLE( GTK_ENTRY( gtk_combo_box_get_child( GTK_COMBO_BOX( wCombo ) )) ), 0, 0);
    gtk_widget_grab_focus( GTK_WIDGET( ((tGlobal *)gpGlobal)->widgets[ eW_frm_Project]  ) );
}


/*!     \brief  Initialize the 'Main' dialog widgets and callbacks
 *
 * Initialize the 'Main' dialog widgets and callbacks
 *
 * \param  pGlobal  pointer to global data
 */
void
initializeMainDialog( tGlobal *pGlobal, tInitFn purpose )
{
    GtkWidget *wDrawingArea_Plot_A, *wDrawingArea_Plot_B, *wApplication;
    GtkEventController *eventMouseMotion;
    GtkEventController *focus_controller;
    GtkGesture *gesture;
    // Set the Widget Content

    if( purpose == eUpdateWidgets || purpose == eInitAll ) {
        populateProjectComboBoxWidget(pGlobal );
        populateCalComboBoxWidget( pGlobal );
        populateTraceComboBoxWidget( pGlobal );

        // fill the combo box with the setup/cal names
        GtkComboBoxText *wComboBoxProject = GTK_COMBO_BOX_TEXT( pGlobal->widgets[ eW_cbt_Project ] );

        // pGlobal->pCalibrationAbstract is initialized by inventorySavedSetupsAndCal()
        // and pGlobal->pGlobalTraceAbstract by inventorySavedTraceNames()
        // As  a side-effect this will also choose the selected calibration and trace profiles
        // by triggering the "change" signal
        if( !setGtkComboBox( GTK_COMBO_BOX(wComboBoxProject), pGlobal->sProject ) )
            gtk_combo_box_set_active (GTK_COMBO_BOX(wComboBoxProject), 0);

        // The items are italicized to show that they are not saved or retrieved yet
        gtk_widget_add_css_class( GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_entry_Title ] ), "italicFont" );
        gtk_widget_add_css_class( GTK_WIDGET( pGlobal->widgets[ eW_nbTrace_txtV_TraceNote ] ), "italicFont" );
        gtk_widget_add_css_class( GTK_WIDGET( pGlobal->widgets[ eW_nbCal_txtV_CalibrationNote ] ), "italicFont" );

        // set the calibration / trace radio button
        // and directly call the callbacks because the GUI is not yet shown (emitted signals will fall on deaf ears)
        if( pGlobal->flags.bCalibrationOrTrace ) {
            gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_rbtn_Cal ] ), TRUE );
            CB_rbtn_Calibration ( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_rbtn_Cal ] ), NULL );
            // g_signal_emit_by_name (GTK_CHECK_BUTTON( pGlobal->widgets[ eW_rbtn_Cal ] ), "clicked", 0);
        } else {
            gtk_check_button_set_active( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_rbtn_Traces ] ), TRUE );
            CB_rbtn_Traces ( GTK_CHECK_BUTTON( pGlobal->widgets[ eW_rbtn_Traces ] ), NULL );
            // g_signal_emit_by_name (GTK_CHECK_BUTTON( pGlobal->widgets[ eW_rbtn_Traces ] ), "clicked", 0);
        }
        CB_editable_TraceProfileName( GTK_EDITABLE( gtk_combo_box_get_child(pGlobal->widgets[ eW_cbt_TraceProfile ] )), NULL );
        CB_editable_CalibrationProfileName( GTK_EDITABLE( gtk_combo_box_get_child(pGlobal->widgets[ eW_cbt_CalProfile ] )), NULL );
    }
    // Define the Widget callbacks

    if( purpose == eInitCallbacks || purpose == eInitAll ) {
        wDrawingArea_Plot_A = pGlobal->widgets[ eW_drawingArea_Plot_A ];
        wDrawingArea_Plot_B = pGlobal->widgets[ eW_drawingArea_Plot_B ];
        wApplication = pGlobal->widgets[ eW_hp8753_main ];

        // callback MD1 & MD2 - keypress and release
        GtkEventController *key_controller = gtk_event_controller_key_new ();
        g_signal_connect_object (key_controller, "key-pressed",
                G_CALLBACK(CB_KeyPressed),  wApplication, G_CONNECT_SWAPPED);
        g_signal_connect_object (key_controller, "key-released",
                G_CALLBACK(CB_KeyReleased), wApplication, G_CONNECT_SWAPPED);
        gtk_widget_add_controller ( wApplication, key_controller );

        // callback MD3 - Connect the "realize" signal to our callback function
        g_signal_connect( wApplication, "realize",
                G_CALLBACK(CB_hp8753_main_Realize), NULL );

        // callback MD4 - Live marker when mouse moved in Plot area
        eventMouseMotion = gtk_event_controller_motion_new ();
        gtk_event_controller_set_propagation_phase(eventMouseMotion, GTK_PHASE_CAPTURE);
        g_signal_connect(eventMouseMotion, "motion",
                G_CALLBACK( CB_on_drawingAreaMouseMotion ), GINT_TO_POINTER( GDK_MOTION_NOTIFY ));
        g_signal_connect(eventMouseMotion, "enter",
                G_CALLBACK( CB_on_drawingAreaMouseMotion ), GINT_TO_POINTER( GDK_ENTER_NOTIFY ));
        g_signal_connect(eventMouseMotion, "leave",
                G_CALLBACK( CB_on_drawingAreaMouseMotion ), GINT_TO_POINTER( GDK_LEAVE_NOTIFY ));
        gtk_widget_add_controller (wDrawingArea_Plot_A, GTK_EVENT_CONTROLLER (eventMouseMotion));

        eventMouseMotion = gtk_event_controller_motion_new ();
        gtk_event_controller_set_propagation_phase(eventMouseMotion, GTK_PHASE_CAPTURE);
        g_signal_connect(eventMouseMotion, "motion",
                G_CALLBACK( CB_on_drawingAreaMouseMotion ), GINT_TO_POINTER( GDK_MOTION_NOTIFY ));
        g_signal_connect(eventMouseMotion, "enter",
                G_CALLBACK( CB_on_drawingAreaMouseMotion ), GINT_TO_POINTER( GDK_ENTER_NOTIFY ));
        g_signal_connect(eventMouseMotion, "leave",
                G_CALLBACK( CB_on_drawingAreaMouseMotion ), GINT_TO_POINTER( GDK_LEAVE_NOTIFY ));
        gtk_widget_add_controller (wDrawingArea_Plot_B, GTK_EVENT_CONTROLLER (eventMouseMotion));

        // callback MD5 - 'Get Trace' button
        g_signal_connect ( pGlobal->widgets[ eW_btn_GetTrace ], "clicked",
                G_CALLBACK (CB_btn_GetTrace), NULL );

        // callback MD6 - change 'Project' ComboBoxText
        // This is called if either the project ComboBoxText is changed or the entry is changed
        g_signal_connect ( pGlobal->widgets[ eW_cbt_Project ], "changed",
                G_CALLBACK (CB_cbt_ProjectName), NULL );
        // callback MD7 - change the entry widget of the 'Project' ComboBoxText
        g_signal_connect ( gtk_combo_box_get_child(pGlobal->widgets[ eW_cbt_Project ] ), "changed",
                G_CALLBACK (CB_editable_ProjectName), NULL );

        // callback MD8 - change 'Calibration Profile' ComboBoxText
        // This is called if either the Calibration Profile ComboBoxText is changed or the entry is changed
        g_signal_connect ( pGlobal->widgets[ eW_cbt_CalProfile ], "changed",
                G_CALLBACK (CB_cbt_CalibrationProfileName), NULL );
        // callback MD9 - change the entry widget of the 'Calibration Profile' ComboBoxText
        g_signal_connect ( gtk_combo_box_get_child(pGlobal->widgets[ eW_cbt_CalProfile ] ), "changed",
                G_CALLBACK (CB_editable_CalibrationProfileName), NULL );


        // callback MD10 - change 'Trace Profile' ComboBoxText
        // This is called if either the Trace Profile ComboBoxText is changed or the entry is changed
        g_signal_connect ( pGlobal->widgets[ eW_cbt_TraceProfile ], "changed",
                G_CALLBACK (CB_cbt_TraceProfileName), NULL );
        // callback MD11 - change the entry widget of the 'Calibration Profile' ComboBoxText
        g_signal_connect ( gtk_combo_box_get_child(pGlobal->widgets[ eW_cbt_TraceProfile ] ), "changed",
                G_CALLBACK (CB_editable_TraceProfileName), NULL );

        // callback MD12 - 'Calibration' radio GtkCkeckButton
        g_signal_connect ( pGlobal->widgets[ eW_rbtn_Cal ], "toggled",
                G_CALLBACK (CB_rbtn_Calibration), NULL );

        // callback MD13 - 'Traces' radio GtkCkeckButton
        g_signal_connect ( pGlobal->widgets[ eW_rbtn_Traces ], "toggled",
                G_CALLBACK (CB_rbtn_Traces), NULL );

        // callback MD14 - 'Save' button
        g_signal_connect ( pGlobal->widgets[ eW_btn_Save ], "clicked",
                G_CALLBACK (CB_btn_Save), NULL );
        gtk_widget_set_sensitive( GTK_WIDGET( pGlobal->widgets[ eW_btn_Save] ), FALSE );

        // callback MD15 - 'Recall' button
        g_signal_connect ( pGlobal->widgets[ eW_btn_Recall ], "clicked",
                G_CALLBACK (CB_btn_Recall), NULL );

        // callback MD16 - 'Delete' button
        g_signal_connect ( pGlobal->widgets[ eW_btn_Delete ], "clicked",
                G_CALLBACK (CB_btn_Delete), NULL );

        // callback MD17 - change notebook page
        g_signal_connect ( pGlobal->widgets[ eW_notebook ], "switch-page",
                G_CALLBACK (CB_notebook_Select), NULL );
        // We need a notification of destruction before the notebook because we get a signal
        // that activates the Cal / Trace radio button
        g_signal_connect ( GTK_WIDGET( pGlobal->widgets[ eW_btn_GetTrace ] ), "destroy",
                G_CALLBACK ( CB_btn_GetTrace_Destroy ), NULL );

        // callback PR 1 - connect signal from print button to callback to launch print dialog
        g_signal_connect ( pGlobal->widgets[ eW_btn_Print ], "clicked",
                G_CALLBACK (CB_btn_Print), NULL );

        // callback PD 1 - connect signal from PDF button to callback to launch print dialog
        g_signal_connect ( pGlobal->widgets[ eW_btn_PDF ], "clicked",
                G_CALLBACK (CB_btn_PDF), NULL );

        // callback PR 1 - connect signal from print button to callback to launch print dialog
        g_signal_connect ( pGlobal->widgets[ eW_btn_PNG ], "clicked",
                G_CALLBACK (CB_btn_PNG), NULL );

        // callback PR 1 - connect signal from print button to callback to launch print dialog
        g_signal_connect ( pGlobal->widgets[ eW_btn_SVG ], "clicked",
                G_CALLBACK (CB_btn_SVG), NULL );

        // Define the drawing function for the GtkDrawingArea widget
        gtk_drawing_area_set_draw_func ( GTK_DRAWING_AREA( pGlobal->widgets[ eW_drawingArea_Plot_A ] ),
                CB_drawingArea_A_Draw, pGlobal, NULL);
        gtk_drawing_area_set_draw_func ( GTK_DRAWING_AREA( pGlobal->widgets[ eW_drawingArea_Plot_B ] ),
                CB_drawingArea_B_Draw, pGlobal, NULL);
        gtk_widget_set_visible( GTK_WIDGET( pGlobal->widgets[ eW_frame_Plot_B ] ), FALSE );

        // callbacks for button presses in the drawing area
        gesture = gtk_gesture_click_new();
        gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture),  0);
        // Connect the "pressed" signal of the gesture of callback function
        g_signal_connect(gesture, "pressed",
                G_CALLBACK(CB_gesture_DrawingArea_MousePress), wDrawingArea_Plot_A);
        // Add the gesture to the widget you want to monitor for clicks
        gtk_widget_add_controller( wDrawingArea_Plot_A, GTK_EVENT_CONTROLLER(gesture));

        gesture = gtk_gesture_click_new();
        gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture),  0);
        // Connect the "pressed" signal of the gesture of callback function
        g_signal_connect(gesture, "pressed",
                G_CALLBACK(CB_gesture_DrawingArea_MousePress), wDrawingArea_Plot_B);
        // Add the gesture to the widget you want to monitor for clicks
        gtk_widget_add_controller( wDrawingArea_Plot_B, GTK_EVENT_CONTROLLER(gesture));

        // Attach the focus event controller to the first entry
        focus_controller = gtk_event_controller_focus_new();
        g_signal_connect(focus_controller, "enter",
                G_CALLBACK(CB_AppFocusIn),  wApplication);
        g_signal_connect(focus_controller, "leave",
                G_CALLBACK(CB_AppFocusOut), wApplication);
        gtk_widget_add_controller(wApplication, focus_controller);

        // Attach focus controller to entry1
        focus_controller = gtk_event_controller_focus_new();
        gtk_widget_add_controller( pGlobal->widgets[ eW_cbt_Project ], focus_controller);
        g_signal_connect(focus_controller, "leave", G_CALLBACK( CB_cbt_Unfocus ), pGlobal );
        focus_controller = gtk_event_controller_focus_new();
        gtk_widget_add_controller(  pGlobal->widgets[ eW_cbt_CalProfile ], focus_controller);
        g_signal_connect(focus_controller, "leave", G_CALLBACK( CB_cbt_Unfocus ), pGlobal );
        focus_controller = gtk_event_controller_focus_new();
        gtk_widget_add_controller(  pGlobal->widgets[ eW_cbt_TraceProfile ], focus_controller);
        g_signal_connect(focus_controller, "leave", G_CALLBACK( CB_cbt_Unfocus ), pGlobal );
    }
    sensitizeRecallSaveDeleteButtons( pGlobal ) ;
}

#pragma GCC diagnostic pop
