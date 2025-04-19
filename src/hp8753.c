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
		.HP8753cal = {{0}},
		.HP8753calibrationKit = {{0}},
		.flags ={0},
		0 };

/*!     \brief  Filter key presses (looking for Function Buttons)
 *
 * Filter key presses (looking for Function Buttons). We either capture
 * the button presses or let them through to the widgets.
 *
 * \ingroup GUI
 *
 * \param widget    pointer to widget
 * \param event     pointer to keyboard event
 * \param user_data not used
 * \return          TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
 */
gboolean
keyHandler (GtkWidget *widget, GdkEventKey  *event, gpointer   user_data) {
	GdkModifierType modmask;
	guint modifier = 0;
	tGlobal *pGlobal = (tGlobal *)user_data;
	GtkWidget *wControls;
	GtkWidget *wFramePlotB;
	GtkWidget *wDrawingAreaPlotA, *wDrawingAreaPlotB;
    GtkWidget *wApplication;

	GtkButton *wGetTraceBtn;

    GtkAllocation alloc = {0};
	gint minWidth, minHeight, newHeight, newWidth;
	gdouble scale;

    gint widthApp, heightApp;
    GdkRectangle screenArea = {0};
    GdkWindowState wState;

    modmask = gtk_accelerator_get_default_mod_mask ();
    modifier = event->state & modmask; // GDK_MODIFIER_MASK;

   if ((event->keyval >= GDK_KEY_F1 && event->keyval <= GDK_KEY_F12)
           || (event->keyval >= GDK_KEY_KP_Space && event->keyval <= GDK_KEY_KP_9)
   ) {
// GDK_CONTROL_MASK GDK_SHIFT_MASK GDK_MOD1_MASK (ALT) GDK_SUPER_MASK

      // What's the size of the monitor
      wApplication = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_hp8753c_main");
      gdk_monitor_get_workarea(
           gdk_display_get_primary_monitor( gdk_display_get_default()), &screenArea);
      wState = gdk_window_get_state(gtk_widget_get_window(wApplication));

      switch( event->keyval ) {
      case GDK_KEY_F1:
#if GTK_CHECK_VERSION(3,22,0)
    	  if( modifier == 0 )
            gtk_show_uri_on_window( NULL, "help:hp8753", gtk_get_current_event_time (), NULL );
#endif
            break;

      case GDK_KEY_F2:
          showRenameMoveCopyDialog( pGlobal );
          break;

      case GDK_KEY_F3:
          wGetTraceBtn = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_btn_GetTrace");
          gtk_button_clicked( wGetTraceBtn );
          break;

      case GDK_KEY_KP_Add:
          if (wState & GDK_WINDOW_STATE_FULLSCREEN)
              break;

          wDrawingAreaPlotA = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DrawingArea_Plot_A");
          wDrawingAreaPlotB = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DrawingArea_Plot_B");

          gtk_widget_get_size_request(wDrawingAreaPlotA, &minWidth, &minHeight);
          gtk_widget_get_allocation(wDrawingAreaPlotA, &alloc);
#define SMIG    10
          // increase size by min/3
          scale = (gdouble)((3 * alloc.width + SMIG)/minWidth)/3.0 + 0.333;

          newWidth = minWidth * scale;
          newHeight = minHeight * scale;
#define MIN_WIDGET_SIZE 1
          gtk_window_resize( GTK_WINDOW(wApplication), MIN_WIDGET_SIZE, MIN_WIDGET_SIZE);

          gtk_widget_set_size_request(wDrawingAreaPlotA, newWidth, newHeight);
          gtk_widget_set_size_request(wDrawingAreaPlotB, newWidth, newHeight);
          // make sure the resize happens ASAP
          while (gtk_events_pending ())
              gtk_main_iteration ();

          gtk_widget_set_size_request(wDrawingAreaPlotA, minWidth, minHeight);
          gtk_widget_set_size_request(wDrawingAreaPlotB, minWidth, minHeight);

          gtk_window_get_size( GTK_WINDOW(wApplication), &widthApp, &heightApp );

          //        This is a bit of a fudge because we don't know about the window decoration size
          if( widthApp > (screenArea.width * 0.92) || heightApp > (screenArea.height * 0.95) ) {
              gtk_window_fullscreen( GTK_WINDOW(wApplication));
          }

          break;

      case GDK_KEY_KP_Subtract:
#define MIN_WIDGET_SIZE 1
          wDrawingAreaPlotA = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DrawingArea_Plot_A");
          wDrawingAreaPlotB = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_DrawingArea_Plot_B");

          gtk_widget_get_size_request(wDrawingAreaPlotA, &minWidth, &minHeight);
          gtk_widget_get_allocation(wDrawingAreaPlotA, &alloc);

          if (wState & GDK_WINDOW_STATE_FULLSCREEN) {
              gtk_window_unfullscreen( GTK_WINDOW(wApplication) );
              scale = ((gdouble)alloc.width)/minWidth;
          } else {
              scale = (gdouble)((3 * alloc.width + SMIG)/minWidth)/3.0 - 0.333;
          }

          if( scale < 1.0 )
              scale = 1.0;

          newWidth = minWidth * scale;
          newHeight = minHeight * scale;

#define MIN_WIDGET_SIZE 1
          gtk_window_resize( GTK_WINDOW(wApplication), MIN_WIDGET_SIZE, MIN_WIDGET_SIZE);

          gtk_widget_set_size_request(wDrawingAreaPlotA, newWidth, newHeight);
          gtk_widget_set_size_request(wDrawingAreaPlotB, newWidth, newHeight);
          // make sure the resize happens ASAP
          while (gtk_events_pending ())
              gtk_main_iteration ();

          gtk_widget_set_size_request(wDrawingAreaPlotA, minWidth, minHeight);
          gtk_widget_set_size_request(wDrawingAreaPlotB, minWidth, minHeight);

          break;

      case GDK_KEY_F9:
          wFramePlotB = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Frame_Plot_B");
          wControls = GTK_WIDGET( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Controls") );

          switch( modifier ) {
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
          if( modifier == GDK_SHIFT_MASK )
              postDataToGPIBThread( TG_UTILITY, NULL );
          else if( modifier == GDK_MOD1_MASK )
              postDataToGPIBThread( TG_EXPERIMENT, NULL );
          break;

      case GDK_KEY_F12:
    	  if( modifier == GDK_SHIFT_MASK ) {
    		  pGlobal->flags.bNoGPIBtimeout = TRUE;
    		  postInfo( "No GPIB timeouts" );
    	  } else {
    		  pGlobal->flags.bNoGPIBtimeout = FALSE;
    		  postInfo( "Normal GPIB timeouts" );
    	  }
    	  break;

      default:
          return FALSE;     // pass the event on to the underlying widgets
    	  break;
      }
   } else if( event->keyval == GDK_KEY_Escape  ) {
       switch ( modifier ) {
       case 0:
       default:
                postDataToGPIBThread (TG_ABORT, NULL);
                break;
       case GDK_SHIFT_MASK:
                postDataToGPIBThread (TG_SETUP_GPIB, NULL);
                break;
       }
   } else {
       // not a Fn or Esc key .. do with it what you will
       return FALSE;     // pass the event on to the underlying widgets
   }
   return TRUE; // nothing more to see here
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
	GtkWidget *wVersionLabel = g_hash_table_lookup ( ((tGlobal *)pGlobal)->widgetHashTable, (gconstpointer)"WID_LblVersion");
	gchar *sVersion;
	if( wSplash ) {
		sVersion = g_strdup_printf( "Version %s\t(🔨 %s)", VERSION, __DATE__ ); // Changelog date is used in RPM
		gtk_label_set_label( GTK_LABEL( wVersionLabel ), sVersion );
		g_free( sVersion );
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

static gint     optDebug = INVALID;
static gboolean bOptQuiet = 0;
static gboolean bOptNoGPIBtimeout = 0;

static gchar    **argsRemainder = NULL;

static const GOptionEntry optionEntries[] =
{
  { "debug",           'd', 0, G_OPTION_ARG_INT,
          &optDebug, "Print diagnostic messages in journal (0-7)", NULL },
  { "quiet",           'q', 0, G_OPTION_ARG_NONE,
          &bOptQuiet, "No GUI sounds", NULL },
  { "noGPIBtimeout",   't', 0, G_OPTION_ARG_NONE,
		          &bOptNoGPIBtimeout, "no GPIB timeout (for debug with HP59401A)", NULL },
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
	gboolean bShowGPIBtab = FALSE;


    if ( pGlobal->flags.bRunning ) {
        // gtk_window_set_screen( GTK_WINDOW( MainWindow ),
        //                       unique_message_data_get_screen( message ) );
        gtk_widget_show(gtk_widget_get_toplevel(GTK_WIDGET(g_hash_table_lookup ( pGlobal->widgetHashTable,
				(gconstpointer)"WID_hp8753c_main"))));
        gtk_window_present_with_time( GTK_WINDOW( g_hash_table_lookup ( pGlobal->widgetHashTable,
				(gconstpointer)"WID_hp8753c_main")), GDK_CURRENT_TIME /*time_*/ );
        return;
    } else {
    	pGlobal->flags.bRunning = TRUE;
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

    g_signal_connect(G_OBJECT(wApplicationWindow), "key-press-event", G_CALLBACK(keyHandler), pGlobal);
    gtk_window_set_position(GTK_WINDOW(wApplicationWindow), GTK_WIN_POS_CENTER);
	gtk_widget_show(wApplicationWindow);
	gtk_application_add_window( GTK_APPLICATION(app), GTK_WINDOW(wApplicationWindow) );

	//  start 1 second timer
	//  g_timeout_add_seconds(1, (GSourceFunc)timer_handler, widgets);
#ifndef DEBUG
	if( optDebug == INVALID || optDebug < 2 ) {
        g_timeout_add( 20, (GSourceFunc)splashCreate, pGlobal );
        g_timeout_add( 5000, (GSourceFunc)splashDestroy, pGlobal );
	}
#endif

	pGlobal->flags.bSmithSpline = TRUE;
	pGlobal->flags.bShowDateTime = TRUE;
	pGlobal->flags.bHPlogo = TRUE;

	if( recoverProgramOptions(pGlobal) != TRUE ){
		// We might need to do something if there are no retrieved options
		// ... but I don't know what that might be!
	    bShowGPIBtab = TRUE;
	}

	// debug level
	if( optDebug != INVALID )
	    pGlobal->flags.bbDebug = optDebug < 8 ? optDebug : 7;

	gchar *sFWlabel = g_strdup_printf( "Firmware %d.%d", pGlobal->HP8753.analyzedLSindexes.version/100,
			pGlobal->HP8753.analyzedLSindexes.version % 100 );
	gtk_label_set_label( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Lbl_Firmware"),
			pGlobal->HP8753.analyzedLSindexes.version != 0 ? sFWlabel : "Firmware unknown");

	gchar *sWindowTitle = g_strdup_printf( "HP %s Vector Network Analyzer",
			pGlobal->HP8753.sProduct ? pGlobal->HP8753.sProduct : "8753" );;
	gtk_window_set_title( GTK_WINDOW(wApplicationWindow), sWindowTitle );
	g_free( sFWlabel );
	g_free( sWindowTitle );


	// Get cal and trace profiles from sqlite3 database
	inventoryProjects( pGlobal );
	inventorySavedSetupsAndCal( pGlobal );
	inventorySavedTraceNames( pGlobal );
	inventorySavedCalibrationKits( pGlobal );

	// set the title initially to that of the last trace saved
	pGlobal->HP8753.sTitle = pGlobal->pTraceAbstract->sTitle;

	// fill the combo box with the setup/cal names
	GtkComboBoxText *wComboBoxProject = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_Project") );
	GtkComboBoxText *wComboBoxCalKit = GTK_COMBO_BOX_TEXT( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Combo_CalKit") );

	populateProjectComboBoxWidget( pGlobal );
	populateCalComboBoxWidget( pGlobal );
	populateTraceComboBoxWidget( pGlobal );

	// Set the color buttons on the color note page
	// according to the element displayed in the combo boxes
	setNotePageColorButton (pGlobal, TRUE );
    setNotePageColorButton (pGlobal, FALSE );

    // pGlobal->pCalibrationAbstract is initialized by inventorySavedSetupsAndCal()
	// and pGlobal->pGlobalTraceAbstract by inventorySavedTraceNames()
	// As  a side-effect this will also choose the selected calibration and trace profiles
	// by triggering the "change" signal
	if( !setGtkComboBox( GTK_COMBO_BOX(wComboBoxProject), pGlobal->sProject ) )
		gtk_combo_box_set_active (GTK_COMBO_BOX(wComboBoxProject), 0);

	// Fill in combobox with the available calibration kits recovered from the sqlite3 database
	for( GList *l = pGlobal->pCalKitList; l != NULL; l = l->next ){
		gtk_combo_box_text_append_text( wComboBoxCalKit, ((tCalibrationKitIdentifier *)l->data)->sLabel );
	}

	// set the calibration / trace radio button
	if( pGlobal->flags.bCalibrationOrTrace ) {
		gtk_button_clicked( GTK_BUTTON( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_RadioCal")) );
	} else {
		gtk_button_clicked( GTK_BUTTON( g_hash_table_lookup(pGlobal->widgetHashTable, (gconstpointer )"WID_RadioTraces")) );
	}

	if( g_list_length( pGlobal->pCalKitList ) > 0 ) {
		gtk_combo_box_set_active ( GTK_COMBO_BOX(wComboBoxCalKit), 0 );
		gtk_widget_set_sensitive( GTK_WIDGET( g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_Btn_CalKitDelete") ),
				TRUE);
	}

	gtk_entry_set_text( GTK_ENTRY(g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer) "WID_Entry_GPIB_HP8753" )), globalData.sGPIBdeviceName );

	GtkWidget *wRadioBtnCalibration = g_hash_table_lookup ( pGlobal->widgetHashTable, (gconstpointer)"WID_RadioCal");

	CB_Radio_Calibration ( GTK_RADIO_BUTTON( wRadioBtnCalibration ), pGlobal );

	if( bShowGPIBtab )
	    gtk_notebook_set_current_page ( GTK_NOTEBOOK( g_hash_table_lookup(pGlobal->widgetHashTable,
            (gconstpointer )"WID_Note")), NPAGE_GPIB );

	// Start the GPIB communication thread
	pGlobal->pGThread = g_thread_new( "GPIBthread", threadGPIB, (gpointer)pGlobal );
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

        pHP8753->channels[channel].chFlags.bSweepHold = FALSE;
        pHP8753->channels[channel].chFlags.bValidData = FALSE;
        pHP8753->channels[channel].chFlags.bbMkrs = 0;
        pHP8753->channels[channel].chFlags.bMkrsDelta = FALSE;
        pHP8753->channels[channel].chFlags.bCenterSpan = FALSE;
	    pHP8753->channels[channel].chFlags.bBandwidth = FALSE;
		pHP8753->channels[channel].chFlags.bAllSegments = FALSE;
		pHP8753->channels[channel].chFlags.bValidSegments = FALSE;
        pHP8753->channels[channel].chFlags.bAdmitanceSmith = FALSE;
        pHP8753->channels[channel].chFlags.bAveraging = FALSE;

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
    pHP8753->flags.bHPGLdataValid = FALSE;
}


/*!     \brief  on_startup (startup signal callback)
 *
 * Setup application (get configuration and create main window (but do not show it))
 * nb: this occures before 'activate'
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

#if !GLIB_CHECK_VERSION(2, 32, 0)
    g_thread_init(NULL);
#endif
    LOG( G_LOG_LEVEL_INFO, "Starting");
	setenv("IB_NO_ERROR", "1", 0);	// no noise
	logVersion();

	pGlobal->sGPIBdeviceName = g_strdup( DEFAULT_GPIB_HP8753C_DEVICE_NAME );

	pGlobal->flags.bGPIB_UseCardNoAndPID = FALSE;
	pGlobal->flags.bNoGPIBtimeout = bOptNoGPIBtimeout;
	pGlobal->GPIBdevicePID = DEFAULT_GPIB_DEVICE_ID;

    /*! We use a loop source to send data back from the
     *  GPIB threads to indicate status
     */
	pGlobal->messageQueueToMain = g_async_queue_new();
	pGlobal->messageEventSource = g_source_new( &messageEventFunctions, sizeof(GSource) );
	pGlobal->messageQueueToGPIB = g_async_queue_new();

    g_source_attach( globalData.messageEventSource, NULL );

    clearHP8753traces( &pGlobal->HP8753 );

    openOrCreateDB();

    for( int i=0; i < NUM_HPGL_PENS; i++ ) {
        HPGLpens[ i ] = HPGLpensFactory[ i ];
    }
    for( int i=0; i < eMAX_COLORS; i++ ) {
        plotElementColors[ i ] = plotElementColorsFactory[ i ];
    }
    pGlobal->PDFpaperSize = eLetter;

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

    g_list_free_full ( g_steal_pointer (&pGlobal->pProjectList), (GDestroyNotify)g_free );
    g_list_free_full ( g_steal_pointer (&pGlobal->pTraceList), (GDestroyNotify)freeTraceListItem );
    g_list_free_full ( g_steal_pointer (&pGlobal->pCalList), (GDestroyNotify)freeCalListItem );

    g_free( pGlobal->HP8753cal.pHP8753_learn );
	g_free( pGlobal->sLastDirectory );
	g_free( pGlobal->sProject );

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

    LOG( G_LOG_LEVEL_INFO, "Ending");
}

/*!     \brief  Start of program
 *
 * Start of program
 *
 * \param argc	number of arguments
 * \param argv	pointer to array of arguments
 * \return		success or failure error code
 */
int
main(int argc, char *argv[]) {
    GtkApplication *app;

    GMainLoop __attribute__((unused)) *loop;

    // The HP8753 formats numbers like 3.141 not, the continental European way 3,14159
    setlocale(LC_NUMERIC, "C");
    g_log_set_writer_func (g_log_writer_journald, NULL, NULL);

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

