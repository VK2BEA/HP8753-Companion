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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib-2.0/glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gpib/ib.h>
#include <locale.h>
#include <time.h>
#include <errno.h>
#include <math.h>
#include <complex.h>

#include "hp8753.h"
#include "widgetID.h"
#include "messageEvent.h"

tGlobal globalData = {
		.HP8753 = {.flags = {.bSourceCoupled = 1, .bMarkersCoupled = 1}},
		.HP8753cal = {{0}},
		.HP8753calibrationKit = {{0}},
		.flags ={0},
		0 };



static gint     optDebug = INVALID;
static gboolean bOptStandardLogging = FALSE;
static gboolean bOptQuiet = 0;
static gboolean bOptNoGPIBtimeout = 0;

static gchar    **argsRemainder = NULL;

static const GOptionEntry optionEntries[] =
{
  { "debug",           'd', 0, G_OPTION_ARG_INT,
          &optDebug, "Print diagnostic messages in journal (0-7)", NULL },
  { "stderrLogging",            's', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
			&bOptStandardLogging, "Send log data to the default device (usually stdout/stderr) rather than the journal", NULL },
  { "quiet",           'q', 0, G_OPTION_ARG_NONE,
          &bOptQuiet, "No GUI sounds", NULL },
  { "noGPIBtimeout",   't', 0, G_OPTION_ARG_NONE,
		  &bOptNoGPIBtimeout, "no GPIB timeout (for debug with HP59401A)", NULL },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &argsRemainder, "", NULL },
  { NULL }
};

/*!     \brief  Initialize widgets
 *
 * Initialize GTK Widgets from data
 *
 * \param  pGlobal      : pointer to data structure
 */

static void
initializeWidgets (tGlobal *pGlobal) {
    // widgets related to the frequency and sweep settings on the main page

    initializeMainDialog( pGlobal, eUpdateWidgets );
    initializeNotebookPageCalibration( pGlobal, eUpdateWidgets );
    initializeNotebookPageTraces( pGlobal, eUpdateWidgets );
    initializeNotebookPageData( pGlobal, eUpdateWidgets );
    initializeNotebookPageOptions( pGlobal, eUpdateWidgets );
    initializeNotebookPageGPIB( pGlobal, eUpdateWidgets );
    initializeNotebookPageCalKit( pGlobal, eUpdateWidgets );
    initializeNotebookPageColor( pGlobal, eUpdateWidgets );

    initializeMainDialog( pGlobal, eInitCallbacks );
    initializeNotebookPageCalibration( pGlobal, eInitCallbacks );
    initializeNotebookPageTraces( pGlobal, eInitCallbacks );
    initializeNotebookPageData( pGlobal, eInitCallbacks );
    initializeNotebookPageOptions( pGlobal, eInitCallbacks );
    initializeNotebookPageGPIB( pGlobal, eInitCallbacks );
    initializeNotebookPageCalKit( pGlobal, eInitCallbacks );
    initializeNotebookPageColor( pGlobal, eInitCallbacks );

    initializeRenameDialog( pGlobal, eInitCallbacks );
    initializeRenameDialog( pGlobal, eUpdateWidgets );
}


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
    GtkBuilder *builder;
    GtkWidget *wApplicationWindow;
    gboolean bShowGPIBtab = FALSE;

    if (pGlobal->flags.bRunning) {
        // gtk_window_set_screen( GTK_WINDOW( MainWindow ),
        //                       unique_message_data_get_screen( message ) );
        gtk_widget_set_visible (
                GTK_WIDGET( gtk_widget_get_root( pGlobal->widgets[ eW_hp8753_main ])), TRUE);

        gtk_window_present (GTK_WINDOW( pGlobal->widgets[ eW_hp8753_main ] ));
        return;
    } else {
        pGlobal->flags.bRunning = TRUE;
    }

    //  builder = gtk_builder_new();

    // use: 'glib-compile-resources --generate-source hp8753-GTK4.xml' to generate resource.c
    //      'sudo gtk-update-icon-cache /usr/share/icons/hicolor' to update the icon cache after icons copied
    //      'sudo cp us.heterodyne.hp8753.gschema.xml /usr/share/glib-2.0/schemas'
    //      'sudo glib-compile-schemas /usr/share/glib-2.0/schemas' for the gsettings schema

    builder = gtk_builder_new ();
    /*
     * Get the data using
     * gpointer *pGlobalData = g_object_get_data ( data, "globalData" );
     */
    GObject *dataObject = g_object_new ( G_TYPE_OBJECT, NULL);
    g_object_set_data (dataObject, "globalData", (gpointer) pGlobal);

    gtk_builder_set_current_object (builder, dataObject);
    gtk_builder_add_from_resource (builder, "/src/hp8753.ui", NULL);
    wApplicationWindow = GTK_WIDGET(gtk_builder_get_object (builder, "WID_hp8753_main"));

    // Get the widgets we will be addressing into a fast array
    buildWidgetList(pGlobal, builder);

    // Stop a hexpand from a GtkEntry in sidebar box from causing the sidebar GtkBox itself (WID_Controls) to obtain this property.
    // The sidebar should maintain its width regardless of the size of the application window
    // See ... https://gitlab.gnome.org/GNOME/gtk/-/issues/6820
    gtk_widget_set_hexpand( GTK_WIDGET( pGlobal->widgets[ eW_box_Controls ] ), FALSE );

    GtkCssProvider *cssProvider = gtk_css_provider_new ();
    gtk_css_provider_load_from_resource (cssProvider, "/src/hp8753.css");
    gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

#if GTK_CHECK_VERSION(4, 20, 0)
    // Set the prefers-color-scheme property in the CSS provider
    // so that we can use alternate colors if the user has a dark desktop
    GtkInterfaceColorScheme color_scheme = pGlobal->flags.bDarkTheme
      ? GTK_INTERFACE_COLOR_SCHEME_DARK : GTK_INTERFACE_COLOR_SCHEME_LIGHT;
    g_object_set (cssProvider, "prefers-color-scheme", color_scheme, NULL);
#endif

    g_object_unref (builder);

    gtk_window_set_default_icon_name ("hp8753");

    gtk_widget_set_visible (wApplicationWindow, TRUE);
    gtk_application_add_window (GTK_APPLICATION(app), GTK_WINDOW(wApplicationWindow));
    gtk_window_set_icon_name (GTK_WINDOW(wApplicationWindow), "hp8753");

    pGlobal->flags.bSmithSpline = TRUE;
    pGlobal->flags.bShowDateTime = TRUE;
    pGlobal->flags.bHPlogo = TRUE;
    // Set the values of the widgets to match the data
    if( recoverProgramOptions(pGlobal) != TRUE ){
        // We might need to do something if there are no retrieved options
        // ... but I don't know what that might be!
        bShowGPIBtab = TRUE;
    }

    gtk_window_set_title( GTK_WINDOW( wApplicationWindow ), "HP8753 Companion");

#ifdef __OPTIMIZE__
    g_idle_add((GSourceFunc)splashCreate, pGlobal);
#endif

    gchar *sWindowTitle = g_strdup_printf( "HP %s Vector Network Analyzer",
            pGlobal->HP8753.sProduct ? pGlobal->HP8753.sProduct : "8753" );
    gtk_window_set_title( GTK_WINDOW(wApplicationWindow), sWindowTitle );
    g_free( sWindowTitle );

    // Get cal and trace profiles from sqlite3 database
    inventoryProjects( pGlobal );
    inventorySavedSetupsAndCal( pGlobal );
    inventorySavedTraceNames( pGlobal );
    inventorySavedCalibrationKits( pGlobal );

    // set the title initially to that of the last trace saved
    if( pGlobal->pTraceAbstract != NULL )
        pGlobal->HP8753.sTitle = pGlobal->pTraceAbstract->sTitle;

    populateProjectComboBoxWidget( pGlobal );
    populateCalComboBoxWidget( pGlobal );
    populateTraceComboBoxWidget( pGlobal );

    initializeWidgets (pGlobal);

    if( bShowGPIBtab )
        gtk_notebook_set_current_page ( GTK_NOTEBOOK( pGlobal->widgets[ eW_notebook ] ), NPAGE_GPIB );
    else
        gtk_notebook_set_current_page ( GTK_NOTEBOOK( pGlobal->widgets[ eW_notebook ] ),
                    pGlobal->flags.bCalibrationOrTrace ? NPAGE_CALIBRATION : NPAGE_TRACE );

    // Start the GPIB communication thread
    pGlobal->pGThread = g_thread_new( "GPIBthread", threadGPIB, (gpointer)pGlobal );

    // Somehow we loose focus in something we've done.
    // We regain focus so that the keyboard will be active (F1 etc).
    gtk_widget_grab_focus( wApplicationWindow );
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

    pGlobal->flags.bNoGPIBtimeout = bOptNoGPIBtimeout;
    pGlobal->flags.bbDebug = optDebug < 8 ? optDebug : 7;

    // Detect if we are using a dark theme
    GSettings *settings = g_settings_new("org.gnome.desktop.interface");
    gchar *color_scheme = g_settings_get_string(settings, "color-scheme");
    pGlobal->flags.bDarkTheme = (g_strcmp0(color_scheme, "prefer-dark") == 0);
    g_free(color_scheme);
    g_object_unref(settings);

    LOG(G_LOG_LEVEL_INFO, "Starting");
    setenv ("IB_NO_ERROR", "1", 0);	// no noise
    logVersion ();

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

	saveProgramOptions( pGlobal );
	closeDB();

    // cleanup
    messageEventData *messageData = g_new0( messageEventData, 1 );
    messageData->command = TG_END;
    g_async_queue_push( pGlobal->messageQueueToGPIB, messageData );

    if (pGlobal->pGThread) {
        g_thread_join (pGlobal->pGThread);
        g_thread_unref (pGlobal->pGThread);
    }

    g_list_free_full ( g_steal_pointer (&pGlobal->pProjectList), (GDestroyNotify)g_free );
    g_list_free_full ( g_steal_pointer (&pGlobal->pTraceList), (GDestroyNotify)freeTraceListItem );
    g_list_free_full ( g_steal_pointer (&pGlobal->pCalList), (GDestroyNotify)freeCalListItem );
    g_list_free_full ( g_steal_pointer (&pGlobal->pCalKitList), (GDestroyNotify)freeCalKitIdentifierItem );

    g_free( pGlobal->HP8753cal.pHP8753_learn );
    g_free( pGlobal->sLastDirectory );
    g_free( pGlobal->sProject );

    for( eChannel channel=0; channel < eNUM_CH; channel++ ) {
        for( int i=0; i < MAX_CAL_ARRAYS; i++ )
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
    g_async_queue_unref (pGlobal->messageQueueToMain);
    g_async_queue_unref (pGlobal->messageQueueToGPIB);
    g_source_destroy (pGlobal->messageEventSource);
    g_source_unref (pGlobal->messageEventSource);

    LOG(G_LOG_LEVEL_INFO, "Ending");
}


/*!     \brief  Filter the log messages
 *
 * Filter the log messages, including those debug messages from libraries (gnome and gtk4)
 *
 * \param log_level  log level
 * \param fields     pointer to message fields
 * \param n_fields   number of fields
 * \param gpGlobal   pointer to global data
 * \return           G_LOG_WRITER_HANDLED or G_LOG_WRITER_UNHANDLED
 */
static GLogWriterOutput
filtered_log_writer_journald (GLogLevelFlags log_level,
        const GLogField *fields,
        gsize n_fields,
        gpointer gpGlobal)
{
    __attribute__((unused)) tGlobal *pGlobal = (tGlobal *)gpGlobal;

    // Messages generated by HPGLplotter do not hae the GLIB_DOMAIN field
    // glib / gtk+ messages do

    for( gint i=0; i < n_fields; i++ ) {
        if( !g_strcmp0( fields[i].key, "GLIB_DOMAIN"  )
                && g_log_writer_default_would_drop (log_level, fields[i].value) )
            return G_LOG_WRITER_UNHANDLED;
    }

    switch (log_level) {
    case G_LOG_LEVEL_ERROR:
    case G_LOG_LEVEL_CRITICAL:
    case G_LOG_LEVEL_WARNING:
    case G_LOG_LEVEL_MESSAGE:
    case G_LOG_LEVEL_INFO:
    case G_LOG_LEVEL_DEBUG:
        if( bOptStandardLogging )
            return g_log_writer_default (log_level, fields, n_fields, gpGlobal);
        else
            return g_log_writer_journald (log_level, fields, n_fields, gpGlobal);
    default:
        break;
    }

    return G_LOG_WRITER_UNHANDLED;
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

    setlocale(LC_ALL, "en_US");
    setenv("IB_NO_ERROR", "1", 0);	// no noise for GPIB library
    g_log_set_writer_func (filtered_log_writer_journald, (gpointer)&globalData, NULL);
    // g_log_set_writer_func (g_log_writer_default, NULL, NULL);
    // g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_INFO, g_log_default_handler, NULL);
    // gtk_set_debug_flags( 0 );

    // ensure only one instance of program runs ..
    app = gtk_application_new ("us.heterodyne.HP8753", G_APPLICATION_HANDLES_OPEN);
    g_application_add_main_option_entries (G_APPLICATION ( app ), optionEntries);

    g_signal_connect (app, "activate", G_CALLBACK (on_activate), (gpointer)&globalData);
    g_signal_connect (app, "startup",  G_CALLBACK  (on_startup), (gpointer)&globalData);
    g_signal_connect (app, "shutdown", G_CALLBACK (on_shutdown), (gpointer)&globalData);

    gint __attribute__((unused)) status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);

    return EXIT_SUCCESS;
}
