/*
 * calibrationKit.h
 *
 *  Created on: Jul 29, 2022
 *      Author: michael
 */

#ifndef CALIBRATIONKIT_H_
#define CALIBRATIONKIT_H_

#define MAX_HP8753_STANDARDS	8

typedef enum {
	eStdTypeOpen, eStdTypeShort, eStdTypeFixedLoad, eStdTypeThru,
	eStdTypeSlidingLoad, eStdTypeArbitraryImpedanceLoad,
	eStdTypeUnknown
} tCalibrationStdType;

typedef enum {
	eConnectorTypeCoaxial, eConnectorTypeWaveguide,
	eConnectorTypeUnknown
} tCalibrationConnectorType;

typedef enum {
	eKitClassSA, eKitClassSB, eKitClassSC,
	eKitClassForwardThru,eKitClassForwardMatch,
	eKitClassReverseThru,eKitClassReverseMatch,
	eKitClassIsolation,
	eKitClassTRLreflect, eKitClassTRLline, eKitClassTRLthru, eKitClassTRLmatch,
	eKitClassUnknown
} tCalibrationClassID;

#define ORDER_OPEN_CORR_POLYNIMIAL	4
typedef 	struct {
	gint number;
	tCalibrationStdType type;
	gchar *label;
	gchar *description;
	GList *portConnectorIDs;
	guint64   maxFreqHz;
	guint64   minFreqHz;
	gdouble L[ORDER_OPEN_CORR_POLYNIMIAL];
	gdouble C[ORDER_OPEN_CORR_POLYNIMIAL];
	struct {
		gdouble offsetDelay;
		gdouble offsetLoss;
		gdouble offsetZ0;
	} offset;
	struct {
		gdouble real;
		gdouble imag;
	} terminationImpedance;

	guint64	valid;
	guint64 offsetValid;
	guint64 terminationImpedanceValid;
} tXKTstandard;

typedef 	struct {
	tCalibrationConnectorType type;
	gchar *gender;
	gchar *family;
	guint64 maxFreqHz;
	guint64 minFreqHz;
	guint cutoffFreqHz;
	gdouble heightWidthRatio;
	gdouble systemZ0;

	guint64	valid;
} tXKTconnector;

typedef 	struct {
	tCalibrationClassID classID;
	gchar *StandardsList;
	gchar *label;
	guint64	valid;
} tXKTkitClass;

typedef struct {
	gchar *sCalKitLabel;
	gchar *sCalKitVersion;
	gchar *sCalKitDescription;

	gchar *sTRLRefPlane;
	gchar *sTRLZref;
	gchar *sLRLAutoCharacterization;

	GList *lStandardList;
	GList *lConnectorList;
	GList *lKitClasses;

	gint nStandard;

} tXKTcalKit;

typedef enum {
	eTOP=0, eUNKNOWN=1, eLEAF=2,

	// Top level element
	eXKT_CalKit,

		// 2nd level elements //CalKit/
		eXKT_CalKitLabel, eXKT_CalKitVersion, eXKT_CalKitDescription,
		eXKT_ConnectorList, eXKT_StandardList, eXKT_KitClasses,
		eXKT_TRLRefPlane, eXKT_TRLZref, eXKT_LRLAutoCharacterization,

			// 3rd level elements for //CalKit/StandardList
			eXKT_FixedLoadStandard, eXKT_SlidingLoadStandard, eXKT_ArbitraryImpedanceStandard,
			eXKT_OpenStandard, eXKT_ShortStandard, eXKT_ThruStandard,
				// 4rd level elements for one of the standards like //CalKit/ConnectorList/ShortStandard
				eXKT_Label, eXKT_Description, eXKT_PortConnectorIDs, eXKT_StandardNumber,
				eXKT_L0, eXKT_L1, eXKT_L2, eXKT_L3, eXKT_C0, eXKT_C1, eXKT_C2, eXKT_C3, eXKT_Offset, eXKT_TerminationImpedance,
					// 5th level elements for offset on one of the standards like //CalKit/ShortStandard/Offset
					eXKT_OffsetDelay, eXKT_OffsetLoss, eXKT_OffsetZ0,
					// 5th level elements for offset on one of the standards like //CalKit/ShortStandard/TerminationImpedance
					eXKT_Real, eXKT_Imag,

			// 3rd level elements for //CalKit/ConnectorList
			eXKT_Coaxial, eXKT_Waveguide,
				// 4rd level elements for one of the connectors like //CalKit/ConnectorList/Coaxial
				eXKT_Family, eXKT_Gender, eXKT_MaximumFrequencyHz, eXKT_MinimumFrequencyHz,
				eXKT_CutoffFrequencyHz, eXKT_HeightWidthRatio, eXKT_SystemZ0,

			// 3rd level elements for //CalKit/eKitClasses
			eXKT_KitClassID, eXKT_StandardsList, eXKT_KitClassLabel
} tXKTstate;

typedef struct {
	gchar *sName;
	tXKTstate element;
} tXKTelement;

typedef struct {
	gchar *sName;
	tCalibrationClassID element;
} tXKTclassID;

#define MAX_CAL_CLASSES    22
#define MAX_CAL_STANDARDS   8
#define MAX_CAL_LABEL_SIZE 10
#define MAX_CALKIT_LABEL_SIZE 40
#define MAX_CALKIT_DESCRIPTION_SIZE 250

// order is important... corresponds to position in array calibrationClasses in tHP8753calibrationKit
typedef enum {
	eHP8753calClassResponse, eHP8753calClassResponseAndIsolation,
	eHP8753calClassS11A, eHP8753calClassS11B, eHP8753calClassS11C,
	eHP8753calClassS22A, eHP8753calClassS22B, eHP8753calClassS22C,
	eHP8753calClassFwdTrans, eHP8753calClassFwdMatch, eHP8753calClassRevTrans, eHP8753calClassRevMatch,

	eHP8753calClassTRLreflectFwdMatch, eHP8753calClassTRLreflectRevMatch,
	eHP8753calClassTRLlineFwdMatch, eHP8753calClassTRLlineFwdTrans,
	eHP8753calClassTRLlineRevMatch, eHP8753calClassTRLlineRevTrans,
	eHP8753calClassTRLthruFwdMatch, eHP8753calClassTRLthruFwdTrans,
	eHP8753calClassTRLthruRevMatch, eHP8753calClassTRLthruRevTrans
} tHP8753calClasses;

typedef struct {
	gchar standards[ MAX_CAL_STANDARDS * 2 + 1];	// 1,2,3,4 etc
	gchar label[ MAX_CAL_LABEL_SIZE + 1];
	gboolean bSpecified;
} tHP8753calibrationClass;


typedef struct {
	tCalibrationStdType calibrationType;
	tCalibrationConnectorType connectorType;

	gchar label[ MAX_CAL_LABEL_SIZE + 1];
	guint64   maxFreqHz;
	guint64   minFreqHz;
	gdouble L[ORDER_OPEN_CORR_POLYNIMIAL];
	gdouble C[ORDER_OPEN_CORR_POLYNIMIAL];

	gdouble offsetDelay;
	gdouble offsetLoss;
	gdouble offsetZ0;

	gdouble	arbitraryZ0;
	gboolean bSpecified;
} tHP8753calibrationStandard;

typedef struct {
	gchar label[ MAX_CALKIT_LABEL_SIZE ];	// this is larger than the label size for the HP8753
	gchar description[ MAX_CALKIT_DESCRIPTION_SIZE ];
	tHP8753calibrationStandard calibrationStandards[ MAX_CAL_STANDARDS ];
	tHP8753calibrationClass calibrationClasses[ MAX_CAL_CLASSES ];
} tHP8753calibrationKit;

typedef struct {
	gchar *sLabel;
	gchar *sDescription;
} tCalibrationKitIdentifier;

extern gint parseCalibrationKit( gchar *sFileName, tHP8753calibrationKit *pHP8753calibrationKit );

#endif /* CALIBRATIONKIT_H_ */
