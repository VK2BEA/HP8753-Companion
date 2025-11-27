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
 * @file GTKnoteGPIB.c
 * @brief Provides supporting code for the GtkNoteBook page 'GPIB'
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

/*!     \brief  Sensitize controls on the GPIB page
 *
 * Certain controls are sensitized depending upon circumstances
 * Either the entry in the /etc/gpib.conf gives the controller number and GPIB PIG of the device
 * or these are specified explicitly.
 *
 * \param  wEditable    pointer to analyze learn string button widget
 * \param  tGlobal	    pointer global data
 */
void setUseGPIBcardNoAndPID( tGlobal *pGlobal, gboolean bPID ) {
    gboolean bIF_GPIB = pGlobal->flags.bbGPIBinterfaceType == eGPIB;

	gtk_widget_set_sensitive( GTK_WIDGET(pGlobal->widgets[ eW_nbGPIB_frame_HP8753_name ]), bIF_GPIB && !bPID );
	gtk_widget_set_sensitive( GTK_WIDGET(pGlobal->widgets[ eW_nbGPIB_frame_minorDeviceNo ]), bPID );
	gtk_widget_set_sensitive( GTK_WIDGET(pGlobal->widgets[ eW_nbGPIB_frame_HP8753_PID ]), bIF_GPIB && bPID );
}

/*!     \brief  Callback for GPIB device name GtkEntry widget
 *
 * Callback (NGPIB 1) for GPIB device name GtkEntry widget
 *
 * \param  wDeviceName  pointer to GtkEditBuffer of the GtkEntry widget
 * \param  udata        user data
 */
void
CB_edit_GPIBname_HP8753 (GtkEditable* wDeviceName, gpointer udata )
{
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT(gtk_widget_get_root(GTK_WIDGET(wDeviceName))), "data");
    const gchar *sDeviceName = gtk_editable_get_text( wDeviceName );

	g_free( pGlobal->sGPIBdeviceName );
    pGlobal->sGPIBdeviceName = g_strdup( sDeviceName );

    if( !pGlobal->flags.bGPIB_UseCardNoAndPID ){
        postDataToGPIBThread (TG_ABORT, NULL);
        postDataToGPIBThread (TG_SETUP_GPIB, NULL);
    }
}

/*!     \brief  Callback GPIB controller minor device number / index spin
 *
 * Callback (NGPIB 2) GPIB minor device number index spin
 *
 * \param  wSpin  pointer to GtkSpinButton
 * \param  udata             user data
 */
void
CB_spin_GPIB_MinorNumber (GtkSpinButton* wSpin, gpointer udata) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wSpin ), "data");

	pGlobal->GPIBcontrollerIndex = (gint)gtk_spin_button_get_value( wSpin );
    if( pGlobal->flags.bGPIB_UseCardNoAndPID ) {
	    postDataToGPIBThread (TG_SETUP_GPIB, NULL);
    }
}

/*!     \brief  Callback GPIB device PID spin
 *
 * Callback (NGPIB 3) GPIB device PID spin
 *
 * \param  wSpin     pointer to GtkSpinButton
 * \param  udata     user data
 */
void
CB_spin_GPIB_HP8753_PID (GtkSpinButton* wSpin, gpointer udata) {
    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wSpin ), "data");

    pGlobal->GPIBdevicePID = (gint)gtk_spin_button_get_value( wSpin );

    if( pGlobal->flags.bGPIB_UseCardNoAndPID ) {
        postDataToGPIBThread (TG_SETUP_GPIB, NULL);
    }
}

/*!     \brief  Callback / GPIB page / Use GBIB name or controller # and device PID
 *
 * Callback (NGPIB 4) when the GPIB controller name / controller # & PID is changed on the GPIB notebook page.
 * Either the entry in the /etc/gpib.conf gives the controller number and GPIB PIG of the device
 * or these are specified explicitly.
 *
 * \param  wCheck    pointer to check button widget
 * \param  udata     unused
 */
void
CB_cbtn_useGPIB_minor_and_PID (GtkCheckButton* wCheck, gpointer udata) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wCheck ), "data");

    pGlobal->flags.bGPIB_UseCardNoAndPID = gtk_check_button_get_active( wCheck );
    setUseGPIBcardNoAndPID( pGlobal, pGlobal->flags.bGPIB_UseCardNoAndPID );
    postDataToGPIBThread (TG_SETUP_GPIB, NULL);
}

/*!     \brief  Callback when user selects GPIB interface
 *
 * Callback (NGPIB 5) when user selects GPIB interface
 *
 * \param  wIF_GPIB pointer to radio button widget
 * \param  udata    unused
 */
void
CB_rbtn_IF_GPIB ( GtkCheckButton *wIF_GPIB, gpointer udata ) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wIF_GPIB ), "data");

    if( gtk_check_button_get_active( GTK_CHECK_BUTTON( wIF_GPIB )) == 0)
        return;

    pGlobal->flags.bbGPIBinterfaceType = eGPIB;
    gtk_widget_set_sensitive( GTK_WIDGET(pGlobal->widgets[ eW_nbGPIB_cbtn_UseGPIB_PID ] ), TRUE );
    setUseGPIBcardNoAndPID( pGlobal, pGlobal->flags.bGPIB_UseCardNoAndPID );
    postDataToGPIBThread (TG_SETUP_GPIB, NULL);
}

/*!     \brief  Callback when user selects GPIB interface
 *
 * Callback (NGPIB 6) when user selects GPIB interface
 *
 * \param  wIF_GPIB pointer to radio button widget
 * \param  udata    unused
 */
void
CB_rbtn_IF_USBTMC ( GtkCheckButton *wIF_USBTMC, gpointer udata ) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wIF_USBTMC ), "data");

    if( gtk_check_button_get_active( GTK_CHECK_BUTTON( wIF_USBTMC )) == 0)
        return;


    pGlobal->flags.bbGPIBinterfaceType = eUSBTMC;
    gtk_widget_set_sensitive( GTK_WIDGET( pGlobal->widgets[ eW_nbGPIB_cbtn_UseGPIB_PID ] ), FALSE );
    setUseGPIBcardNoAndPID( pGlobal, TRUE );
    postDataToGPIBThread (TG_SETUP_GPIB, NULL);
}

/*!     \brief  Callback when user selects GPIB interface
 *
 * Callback (NGPIB 7) when user selects GPIB interface
 *
 * \param  wIF_Prologix pointer to radio button widget
 * \param  udata        unused
 */
void
CB_rbtn_IF_Prologix ( GtkCheckButton *wIF_Prologix, gpointer udata ) {

    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wIF_Prologix ), "data");

    if( gtk_check_button_get_active( GTK_CHECK_BUTTON( wIF_Prologix )) == 0)
        return;

    pGlobal->flags.bbGPIBinterfaceType = ePrologix;
    postDataToGPIBThread (TG_SETUP_GPIB, NULL);
}

/*!     \brief  Initialize the widgets on the GPIB page
 *
 * Initialize the widgets on the GPIB page
 *
 * \param  pGlobal      pointer to global data
 */
void
initializeNotebookPageGPIB( tGlobal *pGlobal, tInitFn purpose )
{
    if( purpose == eUpdateWidgets || purpose == eInitAll ) {
        // restore the settings to the GUI
        gtk_spin_button_set_value( pGlobal->widgets[ eW_nbGPIB_spin_minorDeviceNo ], pGlobal->GPIBcontrollerIndex );
        gtk_spin_button_set_value( pGlobal->widgets[ eW_nbGPIB_spin_HP8753_PID ], pGlobal->GPIBdevicePID );

        gtk_entry_buffer_set_text(
                    gtk_entry_get_buffer( pGlobal->widgets[ eW_nbGPIB_entry_HP8753_name ] ),
                    pGlobal->sGPIBdeviceName, STRLENGTH );

        gtk_spin_button_set_value( GTK_SPIN_BUTTON( pGlobal->widgets[ eW_nbGPIB_spin_minorDeviceNo ] ), pGlobal->GPIBcontrollerIndex);
        gtk_spin_button_set_value( GTK_SPIN_BUTTON( pGlobal->widgets[ eW_nbGPIB_spin_HP8753_PID ]), pGlobal->GPIBdevicePID);
    }
    if( purpose == eInitCallbacks || purpose == eInitAll ) {
        // NGPIB 1 - signal for callback of changing the text in the entry (GPIB device name)
        g_signal_connect(gtk_editable_get_delegate(GTK_EDITABLE(pGlobal->widgets[ eW_nbGPIB_entry_HP8753_name ] )), "changed",
                         G_CALLBACK( CB_edit_GPIBname_HP8753 ), NULL);

        // gboolean bUseGPIBcontrollerName = gtk_check_button_get_active ( pGlobal->widgets[ eW_cbutton_ControlerNameNotIdx" ) );
        gtk_check_button_set_active ( pGlobal->widgets[ eW_nbGPIB_cbtn_UseGPIB_PID ], pGlobal->flags.bGPIB_UseCardNoAndPID );

        // call back when spin changes
        g_signal_connect( pGlobal->widgets[ eW_nbGPIB_spin_HP8753_PID ], "value-changed", G_CALLBACK( CB_spin_GPIB_HP8753_PID ), NULL);
        g_signal_connect( pGlobal->widgets[ eW_nbGPIB_spin_minorDeviceNo ], "value-changed", G_CALLBACK( CB_spin_GPIB_MinorNumber ), NULL);

        g_signal_connect( pGlobal->widgets[ eW_nbGPIB_cbtn_UseGPIB_PID ],  "toggled", G_CALLBACK( CB_cbtn_useGPIB_minor_and_PID ), NULL);


        // callback when interface radio button changed
        g_signal_connect( pGlobal->widgets[ eW_nbGPIB_rbtn_interfaceGPIB ], "toggled", G_CALLBACK( CB_rbtn_IF_GPIB ), NULL);
        g_signal_connect( pGlobal->widgets[ eW_nbGPIB_rbtn_interfaceUSBTMC ], "toggled", G_CALLBACK( CB_rbtn_IF_USBTMC ), NULL);
        g_signal_connect( pGlobal->widgets[ eW_nbGPIB_rbtn_interfacePrologix ], "toggled", G_CALLBACK( CB_rbtn_IF_Prologix ), NULL);
    }
}

