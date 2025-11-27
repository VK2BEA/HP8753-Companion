/*
 * widgetID.h
 *
 *  Created on: Nov 12, 2025
 *      Author: michael
 */

#ifndef WIDGETID_H_
#define WIDGETID_H_

typedef enum {
    // Main application dialog
    eW_hp8753_main,
    eW_box_main,
    // Drawing Area A
    eW_frame_Plot_A,
    eW_drawingArea_Plot_A,
    // Drawing Area B
    eW_frame_Plot_B,
    eW_drawingArea_Plot_B,
    // Box of controls on left
    eW_box_Controls,
    // Get trace button box
    eW_box_GetTrace,
    eW_btn_GetTrace,
    // Trace Plot print / PDF / PNG
    eW_btn_Print,
    eW_btn_PDF,
    eW_btn_PNG,
    eW_btn_SVG,
    // Notebook
    eW_notebook,
    // Page: Calibration
    eW_nbCal_txtV_CalibrationNote,
    eW_nbCal_box_CalInfo,
    eW_nbCal_txtV_CalInfoCh1,
    eW_nbCal_txtV_CalInfoCh2,
    // Page: Trace
    eW_nbTrace_entry_Title,
    eW_nbTrace_buf_Title,
    eW_nbTrace_box_PlotType,
    eW_nbTrace_lbl_Time,
    eW_nbTrace_rbtn_PlotTypeHighRes,
    eW_nbTrace_rbtn_PlotTypeHPGL,
    eW_nbTrace_txtV_TraceNote,
    // Page: Data
    eW_nbData_btn_S2P,
    eW_nbData_btn_S1P,
    eW_nbData_btn_CSV,
    // Page: Options
    eW_nbOpts_cbtn_SmithBezier,
    eW_nbOpts_cbtn_ShowDateTime,
    eW_nbOpts_cbtn_SmithGBnotRX,
    eW_nbOpts_cbtn_DeltaMarkerAbsolute,
    eW_nbOpts_cbtn_DoNotRetrieveHPGL,
    eW_nbOpts_cbtn_ShowHPlogo,
    eW_nbOpts_btn_AnalyzeLS,
    eW_nbOpts_lbl_Firmware,
    eW_nbOpts_rbtn_PDF_A4,
    eW_nbOpts_rbtn_PDF_LTR,
    eW_nbOpts_rbtn_PDF_A3,
    eW_nbOpts_rbtn_PDF_TBL,
    // Page: GPIB
    eW_nbGPIB_entry_HP8753_name,
    eW_nbGPIB_buf_HP8753_name,
    eW_nbGPIB_frame_HP8753_name,
    eW_nbGPIB_spin_minorDeviceNo,
    eW_nbGPIB_frame_minorDeviceNo,
    eW_nbGPIB_spin_HP8753_PID,
    eW_nbGPIB_frame_HP8753_PID,
    eW_nbGPIB_cbtn_UseGPIB_PID,
    eW_nbGPIB_rbtn_interfaceGPIB,
    eW_nbGPIB_rbtn_interfaceUSBTMC,
    eW_nbGPIB_rbtn_interfacePrologix,
    // Page: Cal. Kits
    eW_nbCalKit_cbt_Kit,
    eW_nbCalKit_lbl_Desc,
    eW_nbCalKit_btn_SendKit,
    eW_nbCalKit_btn_ImportXKT,
    eW_nbCalKit_btn_DeleteKit,
    eW_nbCalKit_cbtn_SaveUserKit,
    // Page: Color
    eW_nbColor_dd_elementHR,
    eW_nbColor_colbtn_element,
    eW_nbColor_dd_HPGLpen,
    eW_nbColor_colbtn_HPGLpen,
    eW_nbColor_btn_Reset,
    // Setup, calibration & trace data
    eW_frm_Project,
    eW_cbt_Project,
    eW_entry_Project,
    eW_buf_Project,
    eW_cbt_CalProfile,
    eW_entry_CalProfile,
    eW_buf_CalProfile,
    eW_cbt_TraceProfile,
    eW_entry_TraceProfile,
    eW_buf_TraceProfile,
    eW_rbtn_Cal,
    eW_rbtn_Traces,
    eW_box_SaveRecallDelete,
    eW_btn_Save,
    eW_btn_Recall,
    eW_btn_Delete,
    // Status notification label
    eW_lbl_Status,

    // Rename dialog
    eW_dlg_Rename,
    // radio - verb
    eW_DR_rbtn_Rename,
    eW_DR_rbtn_Move,
    eW_DR_rbtn_Copy,
    // radio - noun
    eW_DR_rbtn_Project,
    eW_DR_rbtn_Calibration,
    eW_DR_rbtn_Trace,
    // name
    eW_DR_entry_From,
    eW_DR_entry_To,
    eW_DR_cbt_Project,
    // buttons
    eW_DR_btn_OK,
    eW_DR_btn_Cancel,
    // Labels
    eW_DR_lbl_From,
    eW_DR_lbl_To,

    eW_Splash,
    eW_lbl_Version,

    eW_N_WIDGETS
} widgetIDs;

#endif /* WIDGETID_H_ */



