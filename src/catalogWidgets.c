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
#include <glib-2.0/glib.h>

#include "hp8753.h"
#include "widgetID.h"

/*!     \brief  Find a widget identified by its name (and add some data to the gobject)
 *
 * Find a widget identified by its name (and add some data to the gobject)
 *
 *
 *
 * \param  pGlobal    : pointer to global data
 * \param  builder    : pointer to GtkBuilder containing compiled widgets
 * \param  sWidgetName: name of widget to find
 * \return pointer to widget found (or zero if not found)
 */
static GtkWidget *
getWidget( tGlobal *pGlobal, GtkBuilder *builder, const gchar *sWidgetName ) {
    GtkWidget *widget = GTK_WIDGET( gtk_builder_get_object ( builder, sWidgetName ) );
    if (GTK_IS_WIDGET(widget)) {
#define WIDGET_ID_PREFIX        "WID_"
#define WIDGET_ID_PREXIX_LEN    (sizeof("WID_")-1)
        gint sequence = (sWidgetName[ WIDGET_ID_PREXIX_LEN] - '1');
        if (sequence < 0 || sequence > 9)
            sequence = INVALID;
        g_object_set_data (G_OBJECT(widget), "sequence", (gpointer) (guint64) sequence);
        g_object_set_data (G_OBJECT(widget), "data", (gpointer) pGlobal);
    }
    return widget;
}

/*!     \brief  Build a random access array of widgets that we need to address
 *
 * Build a random access array of widgets that we need to address. This will be faster to
 * asccess them than if they were in a hash table.
 *
 * \param  pGlobal    : pointer to global data
 * \param  builder    : pointer to GtkBuilder containing compiled widgets
 */
void
buildWidgetList( tGlobal *pGlobal,  GtkBuilder *builder ) {
    const static gchar *sWidgetNames[ eW_N_WIDGETS ] = {
    		// Main application dialog
            [ eW_hp8753_main ]                      = "WID_hp8753_main",
            [ eW_box_main ]                         = "WID_box_main",
            [ ew_label_Title ]                      = "WID_label_Title",
			// Drawing areas
            [ eW_box_Controls ]                     = "WID_box_Controls",
            [ eW_frame_Plot_A ]                     = "WID_frame_Plot_A",
            [ eW_drawingArea_Plot_A ]               = "WID_drawingArea_Plot_A",
            [ eW_frame_Plot_B ]                     = "WID_frame_Plot_B",
            [ eW_drawingArea_Plot_B ]               = "WID_drawingArea_Plot_B",
			// Box of controls on left
            [ eW_box_Controls ]                     = "WID_box_Controls",
			// Get trace button box
            [ eW_box_GetTrace ]                     = "WID_box_GetTrace",
            [ eW_btn_GetTrace ]                     = "WID_btn_GetTrace",
			// Trace Plot print / PDF / PNG
            [ eW_btn_Print ]                        = "WID_btn_Print",
            [ eW_btn_PDF ]                          = "WID_btn_PDF",
            [ eW_btn_PNG ]                          = "WID_btn_PNG",
            [ eW_btn_SVG ]                          = "WID_btn_SVG",
			// Notebook
            [ eW_notebook ]                         = "WID_notebook",
			// Page: Calibration
            [ eW_nbCal_txtV_CalibrationNote ]       = "WID_nbCal_txtV_CalibrationNote",
            [ eW_nbCal_box_CalInfo ]                = "WID_nbCal_box_CalInfo",
            [ eW_nbCal_txtV_CalInfoCh1 ]            = "WID_nbCal_txtV_CalInfoCh1",
            [ eW_nbCal_txtV_CalInfoCh2 ]            = "WID_nbCal_txtV_CalInfoCh2",
			// Page: Trace
			[ eW_nbTrace_entry_Title ]              = "WID_nbTrace_entry_Title",
            [ eW_nbTrace_buf_Title ]                = "WID_nbTrace_buf_Title",
            [ eW_nbTrace_box_PlotType ]             = "WID_nbTrace_box_PlotType",
            [ eW_nbTrace_lbl_Time ]                 = "WID_nbTrace_lbl_Time",
			[ eW_nbTrace_rbtn_PlotTypeHighRes ]     = "WID_nbTrace_rbtn_PlotTypeHighRes",
			[ eW_nbTrace_rbtn_PlotTypeHPGL ]        = "WID_nbTrace_rbtn_PlotTypeHPGL",
			[ eW_nbTrace_txtV_TraceNote ]           = "WID_nbTrace_txtV_TraceNote",
			// Page: Data
			[ eW_nbData_btn_S2P ]                   = "WID_nbData_btn_S2P",
			[ eW_nbData_btn_S1P ]                   = "WID_nbData_btn_S1P",
			[ eW_nbData_btn_CSV ]                   = "WID_nbData_btn_CSV",
			// Page: Options
			[ eW_nbOpts_cbtn_SmithBezier ]          = "WID_nbOpts_cbtn_SmithBezier",
			[ eW_nbOpts_cbtn_ShowDateTime ]         = "WID_nbOpts_cbtn_ShowDateTime",
			[ eW_nbOpts_cbtn_SmithGBnotRX ]         = "WID_nbOpts_cbtn_SmithGBnotRX",
			[ eW_nbOpts_cbtn_DeltaMarkerAbsolute ]  = "WID_nbOpts_cbtn_DeltaMarkerAbsolute",
			[ eW_nbOpts_cbtn_DoNotRetrieveHPGL ]    = "WID_nbOpts_cbtn_DoNotRetrieveHPGL",
			[ eW_nbOpts_cbtn_ShowHPlogo ]           = "WID_nbOpts_cbtn_ShowHPlogo",
			[ eW_nbOpts_btn_AnalyzeLS ]             = "WID_nbOpts_btn_AnalyzeLS",
			[ eW_nbOpts_lbl_Firmware ]              = "WID_nbOpts_lbl_Firmware",
			[ eW_nbOpts_rbtn_PDF_A4 ]               = "WID_nbOpts_rbtn_PDF_A4",
			[ eW_nbOpts_rbtn_PDF_LTR ]              = "WID_nbOpts_rbtn_PDF_LTR",
			[ eW_nbOpts_rbtn_PDF_A3 ]               = "WID_nbOpts_rbtn_PDF_A3",
			[ eW_nbOpts_rbtn_PDF_TBL ]              = "WID_nbOpts_rbtn_PDF_TBL",
			// Page: GPIB
			[ eW_nbGPIB_entry_HP8753_name ]         = "WID_nbGPIB_entry_HP8753_name",
            [ eW_nbGPIB_buf_HP8753_name ]           = "WID_nbGPIB_buf_HP8753_name",
            [ eW_nbGPIB_frame_HP8753_name ]         = "WID_nbGPIB_frame_HP8753_name",
			[ eW_nbGPIB_spin_minorDeviceNo ]        = "WID_nbGPIB_spin_minorDeviceNo",
            [ eW_nbGPIB_frame_minorDeviceNo ]       = "WID_nbGPIB_spin_minorDeviceNo",
			[ eW_nbGPIB_spin_HP8753_PID ]           = "WID_nbOpts_spin_HP8753_PID",
            [ eW_nbGPIB_frame_HP8753_PID ]          = "WID_nbOpts_frame_HP8753_PID",
			[ eW_nbGPIB_cbtn_UseGPIB_PID ]          = "WID_nbGPIB_cbtn_UseGPIB_PID",
			[ eW_nbGPIB_rbtn_interfaceGPIB ]        = "WID_nbGPIB_rbtn_interfaceGPIB",
			[ eW_nbGPIB_rbtn_interfaceUSBTMC ]      = "WID_nbGPIB_rbtn_interfaceUSBTMC",
			[ eW_nbGPIB_rbtn_interfacePrologix ]    = "WID_nbGPIB_rbtn_interfacePrologix",
			// Page: Cal. Kits
			[ eW_nbCalKit_cbt_Kit ]                 = "WID_nbCalKit_cbt_Kit",
            [ eW_nbCalKit_lbl_Desc ]                = "WID_nbCalKit_lbl_Desc",
            [ eW_nbCalKit_btn_SendKit ]             = "WID_nbCalKit_btn_SendKit",
            [ eW_nbCalKit_btn_ImportXKT ]           = "WID_nbCalKit_btn_ImportXKT",
            [ eW_nbCalKit_btn_DeleteKit ]           = "WID_nbCalKit_btn_DeleteKit",
            [ eW_nbCalKit_cbtn_SaveUserKit ]        = "WID_nbCalKit_cbtn_SaveUserKit",
			// Page: Color
            [ eW_nbColor_dd_elementHR ]             = "WID_nbColor_dd_elementHR",
            [ eW_nbColor_colbtn_element ]           = "WID_nbColor_colbtn_element",
            [ eW_nbColor_dd_HPGLpen ]               = "WID_nbColor_dd_HPGLpen",
            [ eW_nbColor_colbtn_HPGLpen ]           = "WID_nbColor_colbtn_HPGLpen",
            [ eW_nbColor_btn_Reset ]                = "WID_nbColor_btn_Reset",
			// Setup, calibration & trace data
            [ eW_frm_Project ]                      = "WID_frm_Project",
            [ eW_cbt_Project ]                      = "WID_cbt_Project",
            [ eW_entry_Project ]                    = "WID_entry_Project",
            [ eW_buf_Project ]                      = "WID_buf_Project",
			[ eW_cbt_CalProfile ]                   = "WID_cbt_CalProfile",
            [ eW_entry_CalProfile ]                 = "WID_entry_CalProfile",
            [ eW_buf_CalProfile ]                   = "WID_buf_CalProfile",
            [ eW_cbt_TraceProfile ]                 = "WID_cbt_TraceProfile",
            [ eW_entry_TraceProfile ]               = "WID_entry_TraceProfile",
            [ eW_buf_TraceProfile ]                 = "WID_buf_TraceProfile",
            [ eW_rbtn_Cal ]                         = "WID_rbtn_Cal",
            [ eW_rbtn_Traces ]                      = "WID_rbtn_Traces",
            [ eW_box_SaveRecallDelete ]            =  "WID_box_SaveRecallDelete",
            [ eW_btn_Save ]                         = "WID_btn_Save",
            [ eW_btn_Recall ]                       = "WID_btn_Recall",
            [ eW_btn_Delete ]                       = "WID_btn_Delete",
			// Status notification label
			[ eW_lbl_Status ]                       = "WID_lbl_Status",

			// Rename dialog
			[ eW_dlg_Rename ]                      = "WID_dlg_Rename",
            [ eW_DR_rbtn_Rename ]                   = "WID_DR_rbtn_Rename",
            [ eW_DR_rbtn_Move ]                     = "WID_DR_rbtn_Move",
            [ eW_DR_rbtn_Copy ]                     = "WID_DR_rbtn_Copy",
            [ eW_DR_rbtn_Project ]                  = "WID_DR_rbtn_Project",
            [ eW_DR_rbtn_Calibration ]              = "WID_DR_rbtn_Calibration",
            [ eW_DR_rbtn_Trace ]                    = "WID_DR_rbtn_Trace",
            [ eW_DR_entry_From ]                    = "WID_DR_entry_From",
            [ eW_DR_entry_To ]                      = "WID_DR_entry_To",
            [ eW_DR_cbt_Project ]                   = "WID_DR_cbt_Project",
            [ eW_DR_btn_OK ]                        = "WID_DR_btn_OK",
            [ eW_DR_btn_Cancel ]                    = "WID_DR_btn_Cancel",
            [ eW_DR_lbl_From ]                      = "WID_DR_lbl_From",
            [ eW_DR_lbl_To ]                        = "WID_DR_lbl_To",

            [ eW_Splash ]                           = "WID_Splash",
            [ eW_lbl_Version ]                       = "WID_lbl_Version"
    };

    for( int i=0; i < eW_N_WIDGETS; i++ )
        pGlobal->widgets[ i ] = getWidget( pGlobal, builder, sWidgetNames[ i ] );
}
