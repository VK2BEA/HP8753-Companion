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
/*!     \brief  Callback to draw plots for printing
 *
 *
 * \param  operation  pointer to GTK print operation structure
 * \param  context	  pointer to GTK print context structure
 * \param  pageNo	  page number
 * \param  pGlobal    pointer to data
 */
#define HEADER_HEIGHT 10
static void
CB_PrintDrawPage (GtkPrintOperation *operation,
           GtkPrintContext   *context,
           gint               pageNo,
		   tGlobal *          pGlobal)
{
	  cairo_t *cr;
	  gdouble height, width;
#define PRINT_MARGIN    (72.0 * 0.10)     // 0.10 inches
	  cr = gtk_print_context_get_cairo_context (context);
	  height = gtk_print_context_get_height( context );
	  width = gtk_print_context_get_width (context);
	  if( pageNo == 0)
		  plotA ( width, height, PRINT_MARGIN, cr, pGlobal);
	  else
		  plotB ( width, height, PRINT_MARGIN, cr, pGlobal);
}

/*!     \brief  Callback when printing commences
 *
 * Increment page number if it is the second plot
 *
 * \param  printOp  pointer to GTK print operation structure
 * \param  context	pointer to GTK print context structure
 * \param  pGlobal  pointer to data
 */
static void
CB_PrintBegin (GtkPrintOperation *printOp,
           GtkPrintContext   *context, tGlobal *pGlobal)
{
	gint pageNos = 1;
	gboolean bHPGL = (pGlobal->HP8753.flags.bShowHPGLplot && pGlobal->HP8753.flags.bHPGLdataValid);
	if( (pGlobal->HP8753.flags.bDualChannel && pGlobal->HP8753.flags.bSplitChannels)
	        && !bHPGL )
		pageNos = 2;
	gtk_print_operation_set_n_pages( printOp, pageNos );
}

/*!     \brief  Callback when printing completes
 *
 *
 * \param  printOp  pointer to GTK print operation structure
 * \param  context	pointer to GTK print context structure
 * \param  pGlobal  pointer to data
 */
static void
CB_PrintDone (GtkPrintOperation *printOp,
        GtkPrintContext   *context, tGlobal *pGlobal)
{

}


static void
CB_PrintRequestPageSetup(GtkPrintOperation* printOp,
                            GtkPrintContext* context, gint page_number, GtkPageSetup* setup,
							tGlobal *pGlobal)
{
#if 0
	GtkPageOrientation orientation;
	orientation = gtk_page_setup_get_orientation(setup);

	GtkPaperSize *paperSize = gtk_paper_size_new ( GTK_PAPER_NAME_LETTER );
	gtk_page_setup_set_paper_size( setup, paperSize );
#endif
}

/*!     \brief  Callback when print button pressed
 *
 * Callback (PR 1) - setup the required callbacks and print settings
 * n.b signal is connected in the main dialog initialization ( initializeMainDialog() )
 *
 * \param  wButton  button widget
 * \param  udata    unused
 */
void
CB_btn_Print (GtkButton *wButton, gpointer udata)
{
	  GtkPrintOperation *printOp;
	  GtkPrintOperationResult res;

	    tGlobal *pGlobal = (tGlobal *)g_object_get_data(G_OBJECT( wButton ), "data");

	  printOp = gtk_print_operation_new ();

	  if (pGlobal->printSettings != NULL)
	    gtk_print_operation_set_print_settings (printOp, pGlobal->printSettings);

	  if (pGlobal->pageSetup != NULL)
		  gtk_print_operation_set_default_page_setup (printOp, pGlobal->pageSetup);

	  g_signal_connect(printOp, "begin_print", G_CALLBACK (CB_PrintBegin), pGlobal);
	  g_signal_connect(printOp, "draw_page", G_CALLBACK (CB_PrintDrawPage), pGlobal);
	  g_signal_connect(printOp, "request-page-setup", G_CALLBACK(CB_PrintRequestPageSetup), pGlobal);
	  g_signal_connect(printOp, "done", G_CALLBACK(CB_PrintDone), pGlobal);

	  gtk_print_operation_set_embed_page_setup ( printOp, TRUE );
	  gtk_print_operation_set_use_full_page ( printOp, FALSE );

	 gtk_print_operation_set_n_pages ( printOp, 2 );

	  res = gtk_print_operation_run (printOp, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
			  GTK_WINDOW( pGlobal->widgets[ eW_hp8753_main ] ), NULL);

	  if (res == GTK_PRINT_OPERATION_RESULT_APPLY)
	    {
	      if (pGlobal->printSettings != NULL)
	        g_object_unref (pGlobal->printSettings);
	      pGlobal->printSettings = g_object_ref (gtk_print_operation_get_print_settings (printOp));

	      if (pGlobal->pageSetup != NULL)
	        g_object_unref (pGlobal->pageSetup);
	      pGlobal->pageSetup = g_object_ref (gtk_print_operation_get_default_page_setup (printOp));
	    }

	  g_object_unref (printOp);
}


