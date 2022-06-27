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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib-2.0/glib.h>
#include <gpib/ib.h>
#include <locale.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <complex.h>
#include <hp8753.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "messageEvent.h"

tGlobal globalData = {
		.HP8753 = {.flags = {.bSourceCoupled = 1, .bMarkersCoupled = 1}},
		{{0}},
		0 };

gboolean
keyHandler (GtkWidget *widget, GdkEventKey  *event, gpointer   user_data) {
	GdkModifierType modmask;
	guint modifier = 0;

   if (event->keyval >= GDK_KEY_F1 && event->keyval <= GDK_KEY_F12) {
      modmask = gtk_accelerator_get_default_mod_mask ();
      modifier = event->state & modmask;
// GDK_CONTROL_MASK GDK_SHIFT_MASK GDK_MOD1_MASK (ALT) GDK_SUPER_MASK

      switch( event->keyval ) {
      case GDK_KEY_F1:
#if GTK_CHECK_VERSION(3,22,0)
            // g_app_info_launch_default_for_uri( "http://engineering.nlsbph.org/Gutenberg/index.html", NULL, NULL);
    	  if( modifier == 0 )
            gtk_show_uri_on_window( NULL, "help:hp8753", gtk_get_current_event_time (), NULL );
#endif
            break;
      default:
    	  break;
      }
   }
   return FALSE;
}

/*!     \brief  Periodic timeout callback
 *
 * Periodic timeout callback
 *
 * \ingroup initialize
 *
 * \param  pGlobal : Pointer to global data
 */
gboolean timeout_callback(gpointer pGlobal)
{
	// currently do nothing
    return TRUE;
}

/*!     \brief  Display the splash screen
 *
 * Show the splash screen
 *
 * \ingroup initialize
 *
 * \param  pGlobal : Pointer to global data
 */
gint
splashCreate (gpointer *pGlobal)
{
	GtkWidget *wSplash = g_hash_table_lookup ( ((tGlobal *)pGlobal)->widgetHashTable, (gconstpointer)"WID_Splash");
	GtkWidget *wApplicationWidget = g_hash_table_lookup ( ((tGlobal *)pGlobal)->widgetHashTable, (gconstpointer)"WID_hp8753c_main");
    if( wSplash ) {
    	// this is needed for Wayland to get rid of the warning notice about transient window not attached
    	// to parent
    	gtk_window_set_transient_for( GTK_WINDOW( wSplash ), GTK_WINDOW( wApplicationWidget ));
    	gtk_window_set_position(GTK_WINDOW(wSplash), GTK_WIN_POS_CENTER_ALWAYS);
        gtk_window_present( GTK_WINDOW( wSplash ) ); // make sure we are on top
    }
    return FALSE;
}

/*!     \brief  Destroy the splash screen
 *
 * After a few seconds destroy the splash screen
 *
 * \ingroup initialize
 *
 * \param  Splash : Pointer to Splash widget to destroy
 */
gint
splashDestroy (gpointer *pGlobal)
{
	GtkWidget *wSplash = g_hash_table_lookup ( ((tGlobal *)pGlobal)->widgetHashTable, (gconstpointer)"WID_Splash");
    if( GTK_IS_WIDGET( wSplash ) ) {
        gtk_widget_destroy( wSplash );
    }
    return FALSE;
}

static gint     optDebug = 0;
static gboolean bOptQuiet = 0;
static gint     optDeviceID = INVALID;
static gchar    *sOptDeviceName = NULL;
static gint     optControllerIndex = INVALID;
static gchar    *sOptControllerName = NULL;
static gchar    **argsRemainder = NULL;

static const GOptionEntry optionEntries[] =
{
  { "debug",           'd', 0, G_OPTION_ARG_INT,
          &optDebug, "Print diagnostic messages in journal (0-7)", NULL },
  { "quiet",           'q', 0, G_OPTION_ARG_NONE,
          &bOptQuiet, "No GUI sounds", NULL },
  { "GPIBdeviceID",      'd', 0, G_OPTION_ARG_INT,
          &optDeviceID, "GPIB device ID for HP8753C", NULL },
  { "GPIBdeviceName",    'n', 0, G_OPTION_ARG_STRING,
		          &sOptDeviceName, "GPIB device name for HP8753C (in /etc/gpib.conf)", NULL },
  { "GPIBcontrollerIndex",  'c', 0, G_OPTION_ARG_INT,
          &optControllerIndex, "GPIB controller board index", NULL },
  { "GPIBcontrollerName",  'c', 0, G_OPTION_ARG_INT,
		          &sOptControllerName, "GPIB controller name (in /etc/gpib.conf)", NULL },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &argsRemainder, "", NULL },
  { NULL }
};


/*!     \brief  on_activate (activate signal callback)
 *
 * Activate the main window and add it to the appliction(show or raise the main window)
 *
 *
 * \ingroup callbacks
 *
 * \param  app      : pointer to this GApplication
 * \param  udata    : unused
 */

static void
on_activate (GApplication *app, gpointer udata)
{
	tGlobal *pGlobal= (tGlobal *)udata;

    GtkBuilder      *builder;
    GtkWidget       *wApplicationWindow;

	GSList *runner;
	const gchar *name;
	const gchar __attribute__((unused)) *wname;
	GtkWidget *widget;
	GSList *widgetList;


    if ( globalData.flags.bRunning ) {
        // gtk_window_set_screen( GTK_WINDOW( MainWindow ),
        //                       unique_message_data_get_screen( message ) );
        gtk_widget_show(gtk_widget_get_toplevel(GTK_WIDGET(g_hash_table_lookup ( globalData.widgetHashTable,
				(gconstpointer)"WID_hp8753c_main"))));
        gtk_window_present_with_time( GTK_WINDOW( g_hash_table_lookup ( globalData.widgetHashTable,
				(gconstpointer)"WID_hp8753c_main")), GDK_CURRENT_TIME /*time_*/ );
        return;
    } else {
    	globalData.flags.bRunning = TRUE;
    }

	globalData.widgetHashTable = g_hash_table_new( g_str_hash, g_str_equal );

	gboolean timer_handler( tGlobal * );

	//  builder = gtk_builder_new();
	//  status = gtk_builder_add_from_file (builder, "hp8505a.glade", NULL);
	// use: 'glib-compile-resources --generate-source resource.xml' to generate resource.c
	builder = gtk_builder_new_from_resource ("/src/hp8753.glade");
	wApplicationWindow = GTK_WIDGET(gtk_builder_get_object(builder, "WID_hp8753c_main"));

	// Get all the widgets in a list
	// Filter for the ones we use (prefixed in glade by WID_)
	// put them in a hash table so we can quickly access them when needed
	//   and can pass them as the user data to callbacks
	widgetList = gtk_builder_get_objects(builder);
 	for (runner = widgetList; runner; runner = g_slist_next(runner))
 	{
 		widget = runner->data;
 		if (GTK_IS_WIDGET(widget))
 		{
 			wname = gtk_widget_get_name(widget);
 			name = gtk_buildable_get_name(GTK_BUILDABLE(widget));
 			// g_printerr("Widget type is %s and buildable get name is %s\n", wname, name);
 			if (g_str_has_prefix (name, "WID_"))
 				g_hash_table_insert( globalData.widgetHashTable, (gchar *)name, widget);
 		}
 	}
 	g_slist_free(widgetList);

//	gtk_widget_add_events(wApplicationWindow, GDK_BUTTON_PRESS_MASK);
	// g_object_set_data(G_OBJECT(widgets->w_combo_setup), "widgets", widgets);

	// We defined the callbacks for the widgets in glade.
	// Now connect them and have them use widgetHashTable as the user data
	gtk_builder_connect_signals(builder, &globalData);
	// GTK4 maybe ... gtk_builder_set_current_object (builder, &globalData);

	g_object_unref(builder);


    GList *iconList = createIconList();
    gtk_window_set_icon_list( GTK_WINDOW(wApplicationWindow), iconList );
    g_list_free_full ( iconList, g_object_unref );

    g_signal_connect(G_OBJECT(wApplicationWindow), "key-press-event", G_CALLBACK(keyHandler), NULL);
    gtk_window_set_position(GTK_WINDOW(wApplicationWindow), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_widget_show(wApplicationWindow);
	gtk_application_add_window( GTK_APPLICATION(app), GTK_WINDOW(wApplicationWindow) );

	//  start 1 second timer
	//  g_timeout_add_seconds(1, (GSourceFunc)timer_handler, widgets);
#ifndef DEBUG
	g_timeout_add( 20, (GSourceFunc)splashCreate, pGlobal );
	g_timeout_add( 5000, (GSourceFunc)splashDestroy, pGlobal );
#endif

	pGlobal->flags.bSmithSpline = TRUE;
	pGlobal->flags.bShowDateTime = TRUE;
	recoverProgramOptions(pGlobal);

	gchar *sFWlabel = g_strdup_printf( "Firmware %d.%d", pGlobal->HP8753.analyzedLSindexes.version/100,
			pGlobal->HP8753.analyzedLSindexes.version % 100 );
	gtk_label_set_label( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Lbl_Firmware"),
			pGlobal->HP8753.analyzedLSindexes.version != 0 ? sFWlabel : "Firmware unknown");
	g_free( sFWlabel );


	// Get cal and trace profiles from sqlite3 database
	getSavedSetupsAndCal( pGlobal );
	getSavedTraceNames( pGlobal );
	// fill the combo box with the setup/cal names
	GtkComboBoxText *wComboBoxCal = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalibrationProfile") );
	GtkComboBoxText *cbComboBoxTrace = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( globalData.widgetHashTable, (gconstpointer)"WID_Combo_TraceProfile") );

	g_list_foreach ( pGlobal->pCalList, updateCalCombobox, wComboBoxCal );


	// Fill in combobox with the available trace names recoverd from the sqlite3 database
	for( GList *l = pGlobal->pTraceList; l != NULL; l = l->next ){
		gtk_combo_box_text_append_text( cbComboBoxTrace, l->data );
	}

	// choose the entry in the combo box
	// it also has the side effect of switching the radio button
	// hense it matters in which order we do this
	if( pGlobal->flags.bCalibrationOrTrace ) {
		setGtkComboBox( GTK_COMBO_BOX(cbComboBoxTrace), pGlobal->sTraceProfile );
		setGtkComboBox( GTK_COMBO_BOX(wComboBoxCal), pGlobal->sCalProfile );
	} else {
		setGtkComboBox( GTK_COMBO_BOX(wComboBoxCal), pGlobal->sCalProfile );
		setGtkComboBox( GTK_COMBO_BOX(cbComboBoxTrace), pGlobal->sTraceProfile );
	}

	gtk_entry_set_text( GTK_ENTRY(g_hash_table_lookup ( globalData.widgetHashTable, (gconstpointer) "WID_Entry_GPIBcontroller" )), globalData.sGPIBcontrollerName );
	gtk_entry_set_text( GTK_ENTRY(g_hash_table_lookup ( globalData.widgetHashTable, (gconstpointer) "WID_Entry_GPIB_HP8753" )), globalData.sGPIBdeviceName );

	GtkWidget *wRadioBtnCalibration = g_hash_table_lookup ( globalData.widgetHashTable, (gconstpointer)"WID_RadioCal");

	CB_Radio_Calibration ( GTK_RADIO_BUTTON( wRadioBtnCalibration ), &globalData );

	// Start the GPIB communication thread
	pGlobal->pGThread = g_thread_new( "GPIBthread", threadGPIB, (gpointer)&globalData );
}

/*!     \brief  Clear traces
 *
 * Clear data for traces
 *
 * \param pHP8753	pointer to tHP8753 structure holding the traces
 */
void
clearHP8753traces( tHP8753 *pHP8753 ) {

    for( eChannel channel=eCH_ONE; channel < eNUM_CH; channel++ ) {
    	pHP8753->channels[channel].sweepStart = kHz( 300.0 );
    	pHP8753->channels[channel].sweepStop = GHz( 3.0 );
    	pHP8753->channels[channel].IFbandwidth = kHz( 3.0 );
    	pHP8753->channels[channel].sweepType = eSWP_LINFREQ;
    	pHP8753->channels[channel].format = eFMT_LOGM;

	    pHP8753->channels[channel].chFlags.bBandwidth = FALSE;
	    pHP8753->channels[channel].chFlags.bCenterSpan = FALSE;
		pHP8753->channels[channel].chFlags.bMkrs = 0;
		pHP8753->channels[channel].chFlags.bMkrsDelta = FALSE;
		pHP8753->channels[channel].chFlags.bValidData = FALSE;
		pHP8753->channels[channel].chFlags.bAllSegments = FALSE;
		pHP8753->channels[channel].chFlags.bValidSegments = FALSE;

		g_free( pHP8753->channels[channel].responsePoints );
		g_free( pHP8753->channels[channel].stimulusPoints );
		pHP8753->channels[channel].responsePoints = NULL;
		pHP8753->channels[channel].stimulusPoints = NULL;
		pHP8753->channels[channel].nPoints = 0;
		pHP8753->channels[channel].nSegments = 0;
		for( gint seg=0; seg < MAX_SEGMENTS; seg++ ) {
			pHP8753->channels[channel].segments[ seg ].nPoints = 0;
			pHP8753->channels[channel].segments[ seg ].startFreq = 0.0;
			pHP8753->channels[channel].segments[ seg ].stopFreq = 0.0;
		}

		pHP8753->channels[channel].chFlags.bValidData = FALSE;
    }
    pHP8753->channels[eCH_ONE].measurementType = eMEAS_S11;
    pHP8753->channels[eCH_TWO].measurementType = eMEAS_S21;
}


/*!     \brief  on_startup (startup signal callback)
 *
 * Setup application (get configuration and create main window (but do not show it))
 *
 * \ingroup initialize
 *
 * \param  app      : pointer to this GApplication
 * \param  udata    : unused
 */
static void
on_startup (GApplication *app, gpointer udata)
{
	tGlobal *pGlobal = (tGlobal *)udata;
	gboolean bAbort = FALSE;

	pGlobal->sGPIBdeviceName     = g_strdup( DEFAULT_GPIB_HP8753C_DEVICE_NAME );
	pGlobal->sGPIBcontrollerName = g_strdup( DEFAULT_GPIB_CONTROLLER_NAME );


	pGlobal->flags.bGPIB_UseCardNoAndPID = FALSE;

	if( sOptDeviceName )  {
		pGlobal->sGPIBdeviceName = sOptDeviceName;
	}

	if( sOptControllerName )  {
		pGlobal->sGPIBcontrollerName = sOptControllerName;
	}

	if( optControllerIndex != INVALID ) {
		pGlobal->GPIBcontrollerIndex = optControllerIndex;
		pGlobal->sGPIBcontrollerName = NULL;
	}

	if( optDeviceID != INVALID ) {
		pGlobal->GPIBdevicePID = optDeviceID;
		pGlobal->sGPIBdeviceName = NULL;
	}

    /*! We use a loop source to send data back from the
     *  GPIB threads to indicate status
     */
	pGlobal->messageQueueToMain = g_async_queue_new();
	pGlobal->messageEventSource = g_source_new( &messageEventFunctions, sizeof(GSource) );
	pGlobal->messageQueueToGPIB = g_async_queue_new();

    g_source_attach( globalData.messageEventSource, NULL );

    clearHP8753traces( &pGlobal->HP8753 );

    openOrCreateDB();

	if( bAbort )
		g_application_quit (G_APPLICATION ( app ));

}
/*!     \brief  on_shutdown (shutdown signal callback)
 *
 * Cleanup on shutdown
 *
 * \param  app      : pointer to this GApplication
 * \param  userData : unused
 */
static void
on_shutdown (GApplication *app, gpointer userData)
{
	tGlobal *pGlobal = (tGlobal *)userData;
	int i;

    // cleanup
    messageEventData *messageData = g_malloc0( sizeof(messageEventData) );
    messageData->command = TG_END;
    g_async_queue_push( pGlobal->messageQueueToGPIB, messageData );

   saveProgramOptions( pGlobal );

    if( pGlobal->pGThread ) {
        g_thread_join( pGlobal->pGThread );
        g_thread_unref( pGlobal->pGThread );
    }

    closeDB();

    g_free( pGlobal->HP8753.pHP8753C_learn );
    g_free( pGlobal->HP8753cal.pHP8753C_learn );
	g_free( pGlobal->sLastDirectory );
	g_free( pGlobal->sCalProfile );
	g_free( pGlobal->sTraceProfile );

    for( eChannel channel=0; channel < eNUM_CH; channel++ ) {
        for( i=0; i < MAX_CAL_ARRAYS; i++ )
        	g_free( pGlobal->HP8753cal.perChannelCal[channel].pCalArrays[i] );
    	g_free( pGlobal->HP8753.channels[ channel ].responsePoints );
    	g_free( pGlobal->HP8753.channels[ channel ].stimulusPoints );
    }

    g_free( pGlobal->HP8753.S2P.freq );
    g_free( pGlobal->HP8753.S2P.S11 );
    g_free( pGlobal->HP8753.S2P.S21 );
    g_free( pGlobal->HP8753.S2P.S22 );
    g_free( pGlobal->HP8753.S2P.S12 );

    // Destroy queue and source
    g_async_queue_unref( pGlobal->messageQueueToMain );
    g_source_destroy( pGlobal->messageEventSource );
    g_source_unref ( pGlobal->messageEventSource );

	g_hash_table_destroy( globalData.widgetHashTable );
}

/*!     \brief  Start of program
 *
 * Start of program
 *
 * \param argc	number of arguments
 * \param argv	pointer to array of arguments
 * \return		sucess or failure error code
 */
int
main(int argc, char *argv[]) {
    GtkApplication *app;

    GMainLoop __attribute__((unused)) *loop;

    setlocale(LC_ALL, "en_US");

    // ensure only one instance of program runs ..
    app = gtk_application_new ("us.heterodyne.hp8753c", G_APPLICATION_HANDLES_OPEN);
    g_application_add_main_option_entries (G_APPLICATION ( app ), optionEntries);

    g_signal_connect (app, "activate", G_CALLBACK (on_activate), (gpointer)&globalData);
    g_signal_connect (app, "startup",  G_CALLBACK  (on_startup), (gpointer)&globalData);
    g_signal_connect (app, "shutdown", G_CALLBACK (on_shutdown), (gpointer)&globalData);

    gint __attribute__((unused)) status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);

	return EXIT_SUCCESS;
}

