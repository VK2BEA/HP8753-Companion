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
	gtk_widget_set_sensitive( GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frm_GPIB_Controller_Identifier")), !bPID );
	gtk_widget_set_sensitive( GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frm_GPIB_HP8753_Identifier")), !bPID );
	gtk_widget_set_sensitive( GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frm_GPIB_Controler_CardNo")), bPID );
	gtk_widget_set_sensitive( GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frm_GPIB_HP8753_PID")), bPID );
}

/*!     \brief  Callback / GPIB page / Use GBIB name or controller # and device PID
 *
 * Callback when the GPIB controller name / controller # & PID is changed on the GPIB notebook page.
 * Either the entry in the /etc/gpib.conf gives the controller number and GPIB PIG of the device
 * or these are specified explicitly.
 *
 * \param  wEditable    pointer to analyze learn string button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_Toggle_UseGPIB_SlotAndID (GtkToggleButton* wToggle, tGlobal *pGlobal) {
 	pGlobal->flags.bGPIB_UseCardNoAndPID = gtk_toggle_button_get_active( wToggle);
	setUseGPIBcardNoAndPID( pGlobal, pGlobal->flags.bGPIB_UseCardNoAndPID );
	postDataToGPIBThread (TG_SETUP_GPIB, NULL);
}

/*!     \brief  Callback / GPIB page / Controller name GtkEditable
 *
 * Callback when the GPIB controller name is changed on the GPIB notebook page.
 * Either the entry in the /etc/gpib.conf gives the controller number and GPIB PIG of the device
 * or these are specified explicitly.
 *
 * \param  wEditable    pointer to analyze learn string button widget
 * \param  tGlobal	    pointer global data
 */
void
CP_Entry_GPIBname_HP8753 (GtkEditable* wEditable, tGlobal *pGlobal)
{
	g_free( pGlobal->sGPIBdeviceName );
	pGlobal->sGPIBdeviceName = g_strdup( gtk_editable_get_chars( wEditable, 0, -1 ) );
	postDataToGPIBThread (TG_SETUP_GPIB, NULL);
}

/*!     \brief  Callback / GPIB page / Controller (minor device number) selection GtkSpinButton
 *
 * Callback when the GPIB controller number GtkSpinButton on the GPIB notebook page is pressed.
 * Either the entry in the /etc/gpib.conf gives the controller number and GPIB PIG of the device
 * or these are specified explicitly.
 *
 * \param  wSpin      pointer to analyze learn string button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_Spin_GPIBcontrollerCard (GtkSpinButton* wSpin, tGlobal *pGlobal) {
	pGlobal->GPIBcontrollerIndex = (gint)gtk_spin_button_get_value( wSpin );
	postDataToGPIBThread (TG_SETUP_GPIB, NULL);
}

/*!     \brief  Callback / GPIB page / PID selection GtkSpinButton
 *
 * Callback when the PID GtkSpinButton on the GPIB notebook page is pressed.
 * Either the entry in the /etc/gpib.conf gives the controller number and GPIB PIG of the device
 * or these are specified explicitly.
 *
 * \param  wSpin      pointer to analyze learn string button widget
 * \param  tGlobal	    pointer global data
 */
void
CB_Spin_GPIB_HP8753_PID (GtkSpinButton* wSpin, tGlobal *pGlobal) {
	pGlobal->GPIBdevicePID = (gint)gtk_spin_button_get_value( wSpin );
	postDataToGPIBThread (TG_SETUP_GPIB, NULL);
}

