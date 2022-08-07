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


#include "hp8753.h"
#include <math.h>
#include <glib/gprintf.h>

GList *listXML = NULL;


tXKTcalKit calKit = {0};

void
freeCalKit ( gpointer uData )
{
	tXKTcalKit *pCalKit = (tXKTcalKit *) uData;

	g_free( pCalKit->sCalKitLabel );
	g_free( pCalKit->sCalKitDescription );
	g_free( pCalKit->sCalKitVersion );

	g_free( pCalKit->sTRLRefPlane );
	g_free( pCalKit->sTRLZref );
	g_free( pCalKit->sLRLAutoCharacterization );

	for( GList *thisItem = g_list_first( pCalKit->lStandardList );
			thisItem != NULL; thisItem = thisItem->next ) {
		tXKTstandard *thisStandard = (tXKTstandard *)thisItem->data;
		g_free( thisStandard->label );
		g_free( thisStandard->description );
		g_list_free_full( g_steal_pointer (&thisStandard->portConnectorIDs), g_free );
		g_free( thisItem->data );
	}

	for( GList *thisItem = g_list_first( pCalKit->lConnectorList );
			thisItem != NULL; thisItem = thisItem->next ) {
		tXKTconnector *thisConnector = (tXKTconnector *)thisItem->data;
		g_free( thisConnector->family );
		g_free( thisConnector->gender );
		g_free( thisItem->data );
	}

	for( GList *thisItem = g_list_first( pCalKit->lKitClasses );
			thisItem != NULL; thisItem = thisItem->next ) {
		tXKTkitClass *thisKitClass = (tXKTkitClass *)thisItem->data;
			g_free( thisKitClass->StandardsList );
			g_free( thisKitClass->label );
			g_free( thisItem->data );
		}

	g_list_free (g_steal_pointer (&pCalKit->lStandardList));
	g_list_free (g_steal_pointer (&pCalKit->lConnectorList));
	g_list_free (g_steal_pointer (&pCalKit->lKitClasses));

}


gint
ASCIItoInt(const gchar *pASCII, gint len) {
	int result = 0;
	gchar *sASCII = g_strndup( pASCII, len );

	if( sASCII )
		result = (gint)g_ascii_strtoll( sASCII, NULL, 10 );
	g_free( sASCII );
	return result;
}

guint64
ASCIItoUInt64(const gchar *pASCII, gint len) {
	guint64 result = 0;
	gchar *sASCII = g_strndup( pASCII, len );

	if( sASCII )
		result = (guint64)g_ascii_strtoll( sASCII, NULL, 10 );
	g_free( sASCII );
	return result;
}

gdouble
ASCIItoDouble(const gchar *pASCII, gint len) {
	gdouble result = 0.0;
	gchar *sASCII = g_strndup( pASCII, len );

	if( sASCII )
		result = strtod( sASCII, NULL );
	g_free( sASCII );

	return result;
}

const tXKTclassID kitClassIDs[] = {
		{ "SA", eKitClassSA },
		{ "SB", eKitClassSB },
		{ "SC", eKitClassSC },
		{ "FORWARD_THRU", eKitClassForwardThru },
		{ "FORWARD_MATCH", eKitClassForwardMatch },
		{ "REVERSE_THRU", eKitClassReverseThru },
		{ "REVERSE_MATCH", eKitClassReverseMatch },
		{ "ISOLATION", eKitClassIsolation },
		{ "TRL_THRU", eKitClassTRLthru },
		{ "TRL_REFLECT", eKitClassTRLreflect },
        { "TRL_LINE", eKitClassTRLline },
		{ "TRL_MATCH", eKitClassTRLmatch } ,
        { NULL, 0 }
};

const tXKTelement elementsCalKit[] = {
		{ "CalKitLabel", eXKT_CalKitLabel },
		{ "CalKitVersion", eXKT_CalKitVersion },
		{ "CalKitDescription", eXKT_CalKitDescription },
		{ "ConnectorList", eXKT_ConnectorList },
		{ "StandardList", eXKT_StandardList },
		{ "KitClasses", eXKT_KitClasses },
		{ "TRLRefPlane", eXKT_TRLRefPlane },
		{ "TRLZref", eXKT_TRLZref },
		{ "LRLAutoCharacterization", eXKT_LRLAutoCharacterization },
		{ NULL, 0 }
};

const tXKTelement elementsConnectorList[] = {
		{ "Coaxial", eXKT_Coaxial },
		{ "Waveguide", eXKT_Waveguide },
		{ NULL, 0 }
};

const tXKTelement elementsConnector[] = {
		{ "Family", eXKT_Family },
		{ "Gender", eXKT_Gender },
		{ "MaximumFrequencyHz", eXKT_MaximumFrequencyHz },
		{ "MinimumFrequencyHz", eXKT_MinimumFrequencyHz },
		{ "CutoffFrequencyHz", eXKT_CutoffFrequencyHz },
		{ "HeightWidthRadio", eXKT_HeightWidthRatio },
		{ "SystemZ0", eXKT_SystemZ0 },
		{ NULL, 0 }
};

const tXKTelement elementsStandardList[] = {
		{ "FixedLoadStandard", eXKT_FixedLoadStandard },
		{ "SlidingLoadStandard", eXKT_SlidingLoadStandard },
		{ "ArbitraryImpedanceStandard", eXKT_ArbitraryImpedanceStandard },
		{ "OpenStandard", eXKT_OpenStandard },
		{ "ShortStandard", eXKT_ShortStandard },
		{ "ThruStandard", eXKT_ThruStandard },
		{ NULL, 0 }
};

const tXKTelement elementsStandard[] = {
		{ "Label", eXKT_Label },
		{ "Description", eXKT_Description },
		{ "PortConnectorIDs", eXKT_PortConnectorIDs },
		{ "MaximumFrequencyHz", eXKT_MaximumFrequencyHz },
		{ "MinimumFrequencyHz", eXKT_MinimumFrequencyHz },
		{ "StandardNumber", eXKT_StandardNumber },
		{ "L0", eXKT_L0 },
		{ "L1", eXKT_L1 },
		{ "L2", eXKT_L2 },
		{ "L3", eXKT_L3 },
		{ "C0", eXKT_C0 },
		{ "C1", eXKT_C1 },
		{ "C2", eXKT_C2 },
		{ "C3", eXKT_C3 },
		{ "Offset", eXKT_Offset },
		{ "TerminationImpedance", eXKT_TerminationImpedance },
		{ NULL, 0 }
};

const tXKTelement elementsStdOffset[] = {
		{ "OffsetDelay", eXKT_OffsetDelay },
		{ "OffsetLoss", eXKT_OffsetLoss },
		{ "OffsetZ0", eXKT_OffsetZ0 },
		{ NULL, 0 }
};

const tXKTelement elementsStdTerminationImpedance[] = {
		{ "OffsetDelay", eXKT_Real },
		{ "OffsetLoss", eXKT_Imag },
		{ NULL, 0 }
};

const tXKTelement elementsKitClasses[] = {
		{ "KitClassID", eXKT_KitClassID },
		{ "StandardsList", eXKT_StandardsList },
		{ "KitClassLabel", eXKT_KitClassLabel },
		{ NULL, 0 }
};

#define MAX_XML_LEVELS 10
tXKTstate heirachy[ MAX_XML_LEVELS ] = {0};

gint XMLlevel = 0;

// we get the //CalKit/ConnectorList/Coaxial xpath
static void startConnectorList( GMarkupParseContext *context,
        const gchar *element_name,
        const gchar **attribute_names,
        const gchar **attribute_values,
        gpointer uData,
        GError ** error )
{
	tXKTcalKit *pCalKit = (tXKTcalKit *) uData;
	heirachy[ XMLlevel ] = eUNKNOWN;

	if( XMLlevel == 2 ) {
		for( int i=0; elementsConnectorList[i].sName; i++ ) {
			if( !strcmp( element_name, elementsConnectorList[i].sName ) ) {
				heirachy[ XMLlevel ] = elementsConnectorList[i].element;
			}
		}
		pCalKit->lConnectorList = g_list_prepend( pCalKit->lConnectorList, g_malloc0( sizeof( tXKTconnector )) );
		tCalibrationConnectorType thisConnector = eConnectorTypeUnknown;
		switch( heirachy[ XMLlevel ] ) {
		case eXKT_Coaxial:
			thisConnector = eConnectorTypeCoaxial;
			break;
		case eXKT_Waveguide:
			thisConnector = eStdTypeSlidingLoad;
			break;
		default:
			break;
		}
		((tXKTconnector *)pCalKit->lConnectorList->data)->type = thisConnector;
	} else if( XMLlevel == 3
			&& ( heirachy[ 2 ] == eXKT_Coaxial || heirachy[ 2 ] == eXKT_Waveguide ) ) {
		tXKTconnector *thisConnector = ((tXKTconnector *)pCalKit->lConnectorList->data);
		for( int i=0; elementsConnector[i].sName; i++ ) {
			if( !strcmp( element_name, elementsConnector[i].sName ) ) {
				heirachy[ XMLlevel ] = elementsConnector[i].element;
				thisConnector->valid |= (1 << i);
				break;
			}
		}
	}

	XMLlevel++;
}

static void endConnectorList( GMarkupParseContext *context,
        const gchar *element_name,
        gpointer uData,
        GError **error )
{
    XMLlevel--;
}

static void textConnectorList( GMarkupParseContext *context,
        const gchar *pText,
        gsize textLen,
        gpointer uData,
        GError **error )
{
	tXKTcalKit *pCalKit = (tXKTcalKit *) uData;
	if( XMLlevel == 4 ) {
		tXKTconnector *thisConnector = (tXKTconnector *)pCalKit->lConnectorList->data;
		switch(  heirachy[ 3 ] ) {
		case eXKT_Family:
			thisConnector->family = g_strndup( pText, textLen );
			break;
		case eXKT_Gender:
			thisConnector->gender = g_strndup( pText, textLen );
			break;
		case eXKT_MaximumFrequencyHz:
			((tXKTconnector *)pCalKit->lConnectorList->data)->maxFreqHz = ASCIItoUInt64( pText, textLen );
			break;
		case eXKT_MinimumFrequencyHz:
			((tXKTconnector *)pCalKit->lConnectorList->data)->minFreqHz = ASCIItoUInt64( pText, textLen );
			break;
		case eXKT_CutoffFrequencyHz:
			((tXKTconnector *)pCalKit->lConnectorList->data)->cutoffFreqHz = ASCIItoInt( pText, textLen );
			break;
		case eXKT_HeightWidthRatio:
			((tXKTconnector *)pCalKit->lConnectorList->data)->heightWidthRatio = ASCIItoDouble( pText, textLen );
			break;
		case eXKT_SystemZ0:
			((tXKTconnector *)pCalKit->lConnectorList->data)->systemZ0 = ASCIItoDouble( pText, textLen );
			break;
		default:
			break;
		}
	}
}

// we get the //CalKit/ConnectorList/Coaxial xpath
static void startStandardList( GMarkupParseContext *context,
        const gchar *element_name,
        const gchar **attribute_names,
        const gchar **attribute_values,
        gpointer uData,
        GError ** error )
{
	tXKTcalKit *pCalKit = (tXKTcalKit *) uData;
	heirachy[ XMLlevel ] = eUNKNOWN;

	// to get here we must already be //CalKit/StandardList/
	if( XMLlevel == 2 ) {
		for( int i=0; elementsStandardList[i].sName; i++ ) {
			if( !strcmp( element_name, elementsStandardList[i].sName ) ) {
				heirachy[ XMLlevel ] = elementsStandardList[i].element;
			}
		}
		pCalKit->lStandardList = g_list_prepend( pCalKit->lStandardList, g_malloc0( sizeof( tXKTstandard )) );
		tCalibrationStdType thisStandard = eStdTypeUnknown;
		switch( heirachy[ XMLlevel ] ) {
		case eXKT_FixedLoadStandard:
			thisStandard = eStdTypeFixedLoad;
			break;
		case eXKT_SlidingLoadStandard:
			thisStandard = eStdTypeSlidingLoad;
			break;
		case eXKT_ArbitraryImpedanceStandard:
			thisStandard = eStdTypeArbitraryImpedanceLoad;
			break;
		case eXKT_OpenStandard:
			thisStandard = eStdTypeOpen;
			break;
		case eXKT_ShortStandard:
			thisStandard = eStdTypeShort;
			break;
		case eXKT_ThruStandard:
			thisStandard = eStdTypeThru;
			break;
		default:
			break;
		}
		((tXKTstandard *)pCalKit->lStandardList->data)->type = thisStandard;
	} else if( XMLlevel == 3 && heirachy[ 2 ] != eUNKNOWN) {
		tXKTstandard *thisStandard = ((tXKTstandard *)pCalKit->lStandardList->data);
		for( int i=0; elementsStandard[i].sName; i++ ) {
			if( !strcmp( element_name, elementsStandard[i].sName ) ) {
				heirachy[ XMLlevel ] = elementsStandard[i].element;
				thisStandard->valid |= (1 << i);
				break;
			}
		}
	} else if( XMLlevel == 4 ) {
		if ( heirachy[ 3 ] == eXKT_Offset) {
			tXKTstandard *thisStandard = ((tXKTstandard *)pCalKit->lStandardList->data);
			for( int i=0; elementsStdOffset[i].sName; i++ ) {
				if( !strcmp( element_name, elementsStdOffset[i].sName ) ) {
					heirachy[ XMLlevel ] = elementsStdOffset[i].element;
					thisStandard->offsetValid |= (1 << i);
					break;
				}
			}
		} else if ( heirachy[ 3 ] == eXKT_TerminationImpedance) {
			tXKTstandard *thisStandard = ((tXKTstandard *)pCalKit->lStandardList->data);
			for( int i=0; elementsStdTerminationImpedance[i].sName; i++ ) {
				if( !strcmp( element_name, elementsStdTerminationImpedance[i].sName ) ) {
					heirachy[ XMLlevel ] = elementsStdTerminationImpedance[i].element;
					thisStandard->terminationImpedanceValid |= (1 << i);
					break;
				}
			}
		}
	}

	XMLlevel++;
}

static void endStandardList( GMarkupParseContext *context,
        const gchar *element_name,
        gpointer uData,
        GError **error )
{
    XMLlevel--;
}

static void textStandardList( GMarkupParseContext *context,
        const gchar *pText,
        gsize textLen,
        gpointer uData,
        GError **error )
{
	tXKTcalKit *pCalKit = (tXKTcalKit *) uData;

	if( XMLlevel == 4 ) {
		tXKTstandard *thisStandard = ((tXKTstandard *)pCalKit->lStandardList->data);

		switch(  heirachy[ 3 ] ) {
		case eXKT_Label:
			thisStandard->label = g_strndup( pText, textLen );
			break;
		case eXKT_Description:
			thisStandard->description = g_strndup( pText, textLen );
			break;
		case eXKT_PortConnectorIDs:
			thisStandard->portConnectorIDs = g_list_prepend( thisStandard->portConnectorIDs, g_strndup( pText, textLen ) );
			break;
		case eXKT_MaximumFrequencyHz:
			thisStandard->maxFreqHz = ASCIItoUInt64( pText, textLen );
			break;
		case eXKT_MinimumFrequencyHz:
			thisStandard->minFreqHz = ASCIItoUInt64( pText, textLen );
			break;
		case eXKT_StandardNumber:
			thisStandard->number = ASCIItoInt( pText, textLen );
			break;
		case eXKT_L0:
			thisStandard->L[0] = ASCIItoDouble( pText, textLen );
			break;
		case eXKT_L1:
			thisStandard->L[1] = ASCIItoDouble( pText, textLen );
			break;
		case eXKT_L2:
			thisStandard->L[2] = ASCIItoDouble( pText, textLen );
			break;
		case eXKT_L3:
			thisStandard->L[3] = ASCIItoDouble( pText, textLen );
			break;
		case eXKT_C0:
			thisStandard->C[0] = ASCIItoDouble( pText, textLen );
			break;
		case eXKT_C1:
			thisStandard->C[1] = ASCIItoDouble( pText, textLen );
			break;
		case eXKT_C2:
			thisStandard->C[2] = ASCIItoDouble( pText, textLen );
			break;
		case eXKT_C3:
			thisStandard->C[3] = ASCIItoDouble( pText, textLen );
			break;
		case eXKT_Offset:
		default:
			break;
		}
	} else 	if( XMLlevel == 5 ) {
		if( heirachy[ 3 ] == eXKT_Offset  ) {
			tXKTstandard *thisStandard = ((tXKTstandard *)pCalKit->lStandardList->data);

			switch(  heirachy[ 4 ] ) {
			case eXKT_OffsetDelay:
				thisStandard->offset.offsetDelay = ASCIItoDouble( pText, textLen );
				break;
			case eXKT_OffsetLoss:
				thisStandard->offset.offsetLoss = ASCIItoDouble( pText, textLen );
				break;
			case eXKT_OffsetZ0:
				thisStandard->offset.offsetZ0 = ASCIItoDouble( pText, textLen );
				break;
			default:
				break;
			}
		} else 		if( heirachy[ 3 ] == eXKT_TerminationImpedance  ) {
			tXKTstandard *thisStandard = ((tXKTstandard *)pCalKit->lStandardList->data);

			switch(  heirachy[ 4 ] ) {
			case eXKT_Real:
				thisStandard->terminationImpedance.real = ASCIItoDouble( pText, textLen );
				break;
			case eXKT_Imag:
				thisStandard->terminationImpedance.imag = ASCIItoDouble( pText, textLen );
				break;
			default:
				break;
			}
		}
	}
}

// we get the //CalKit/ConnectorList/Coaxial xpath
static void startKitClasses( GMarkupParseContext *context,
        const gchar *element_name,
        const gchar **attribute_names,
        const gchar **attribute_values,
        gpointer uData,
        GError ** error )
{
	tXKTcalKit __attribute__((unused))  *pCalKit = (tXKTcalKit *) uData;
	heirachy[ XMLlevel ] = eUNKNOWN;

	if( XMLlevel == 2 ) {
		for( int i=0; elementsKitClasses[i].sName; i++ ) {
			if( !strcmp( element_name, elementsKitClasses[i].sName ) ) {
				heirachy[ XMLlevel ] = elementsKitClasses[i].element;
				break;
			}
		}
	}

	XMLlevel++;
}


static void endKitClasses( GMarkupParseContext *context,
        const gchar *element_name,
        gpointer user_data,
        GError **error )
{
    XMLlevel--;
}

static void textKitClasses( GMarkupParseContext *context,
        const gchar *pText,
        gsize textLen,
        gpointer uData,
        GError **error )
{
	tXKTcalKit *pCalKit = (tXKTcalKit *) uData;
	gchar *kitClassID;

	if( XMLlevel == 3 ) {
		tXKTkitClass *thisKitClass = (tXKTkitClass *)pCalKit->lKitClasses->data;

		switch(  heirachy[ 2 ] ) {
		case eXKT_KitClassID:
			kitClassID = g_strndup( pText, textLen );
			thisKitClass->classID = eKitClassUnknown;
			for( int i=0; kitClassIDs[i].sName; i++ ) {
				if( !strcmp( kitClassID, kitClassIDs[i].sName ) ) {
					thisKitClass->classID = kitClassIDs[i].element;
					break;
				}
			}
			g_free( kitClassID );
			break;
		case eXKT_StandardsList:
			thisKitClass->StandardsList = g_strndup( pText, textLen );
			break;
		case eXKT_KitClassLabel:
			thisKitClass->label = g_strndup( pText, textLen );
			break;
		default:
			break;
		}
	}
}

static void err(GMarkupParseContext *context,
		GError *error,
		gpointer user_data)
{
	g_print("(err)parse xml failed: %s \n", error->message);

}

GMarkupParser parserConnectorList = {
    . start_element = startConnectorList,
    . end_element   = endConnectorList,
    . text = textConnectorList,
    . passthrough = NULL,
    . error = err
};

GMarkupParser parserStandardList = {
    . start_element = startStandardList,
    . end_element   = endStandardList,
    . text = textStandardList,
    . passthrough = NULL,
    . error = err
};

GMarkupParser parserKitClasses = {
    . start_element = startKitClasses,
    . end_element   = endKitClasses,
    . text = textKitClasses,
    . passthrough = NULL,
    . error = err
};

static void startCalKit( GMarkupParseContext *context,
        const gchar *element_name,
        const gchar **attribute_names,
        const gchar **attribute_values,
        gpointer uData,
        GError ** error )
{
	tXKTcalKit *pCalKit = (tXKTcalKit *) uData;
    switch( XMLlevel ) {
    // we should only find <CalKit>
    case 0:
		heirachy[ XMLlevel ] = eUNKNOWN;
    	if( !strcmp( element_name, "CalKit" ) ) {
    		heirachy[ XMLlevel ] = eXKT_CalKit;
		}
    	break;
    // this is metadata about the calkit, <ConnectorList>, <StandardList>, <KitClasses>,
    //         <TRLRefPlane>, <TRLZref> or <LRLAutoCharacterization>
    case 1:
    	heirachy[ XMLlevel ] = eUNKNOWN;
    	for( int i=0; elementsCalKit[i].sName; i++ ) {
    		if( !strcmp( element_name, elementsCalKit[i].sName ) ) {
    			heirachy[ XMLlevel ] = elementsCalKit[i].element;
    			break;
    		}
    	}
    	switch( heirachy[ XMLlevel ] ) {
    	case eXKT_ConnectorList:
    		g_markup_parse_context_push (context, &parserConnectorList, pCalKit);
    		break;
    	case eXKT_StandardList:
    		g_markup_parse_context_push (context, &parserStandardList, pCalKit);
    		break;
    	case eXKT_KitClasses:
    		g_markup_parse_context_push (context, &parserKitClasses, pCalKit);
    		pCalKit->lKitClasses = g_list_prepend( pCalKit->lKitClasses, g_malloc0( sizeof( tXKTkitClass )) );
    		break;
    	default:
    		break;
    	}
    	break;
    case 2:
    	// chack parent
    	heirachy[ XMLlevel ] = eUNKNOWN;
    	switch( heirachy[ 1 ] ) {
    	case( eXKT_StandardList ):
			for( int i=0; elementsStandardList[i].sName; i++ ) {
				if( !strcmp( element_name, elementsStandardList[i].sName ) ) {
					heirachy[ XMLlevel ] = elementsStandardList[i].element;
					break;
				}
			}
			break;
    	case( eXKT_KitClasses ):
			for( int i=0; elementsKitClasses[i].sName; i++ ) {
				if( !strcmp( element_name, elementsKitClasses[i].sName ) ) {
					heirachy[ XMLlevel ] = elementsKitClasses[i].element;
					break;
				}
			}
			break;
    	default:
    		break;
    	}
    	break;
    default:
    	break;
    }

    XMLlevel++;
}

static void endCalKit( GMarkupParseContext *context,
        const gchar *element_name,
        gpointer user_data,
        GError **error )
{
	if( XMLlevel == 2
			&& ( heirachy[ 1 ] == eXKT_ConnectorList
					|| heirachy[ 1 ] == eXKT_StandardList
					|| heirachy[ 1 ] == eXKT_KitClasses ) )
		g_markup_parse_context_pop (context);
    XMLlevel--;
}

static void textCalKit( GMarkupParseContext *context,
        const gchar *text,
        gsize textLen,
        gpointer uData,
        GError **error )
{
	tXKTcalKit *pCalKit = (tXKTcalKit *) uData;
	if( XMLlevel == 2 && heirachy[ 0 ] == eXKT_CalKit) {
		switch( heirachy[ 1 ] ) {
		case eXKT_CalKitLabel:
			pCalKit->sCalKitLabel = g_strndup( text, textLen );
			break;
		case eXKT_CalKitVersion:
			pCalKit->sCalKitVersion = g_strndup( text, textLen );
			break;
		case eXKT_CalKitDescription:
			pCalKit->sCalKitDescription = g_strndup( text, textLen );
			break;
		case eXKT_TRLRefPlane:
			pCalKit->sTRLRefPlane = g_strndup( text, textLen );
			break;
		case eXKT_TRLZref:
			pCalKit->sTRLZref = g_strndup( text, textLen );
			break;
		case eXKT_LRLAutoCharacterization:
			pCalKit->sLRLAutoCharacterization = g_strndup( text, textLen );
			break;
		default:
			break;
		}
	}
}



GMarkupParser parserTop = {
    . start_element = startCalKit,
    . end_element   = endCalKit,
    . text = textCalKit,
    . passthrough = NULL,
    . error = err
};


#define g_Strlcpy(x,y,z) { if( y ) g_strlcpy(x,y,z); }

gint
parseCalibrationKit( gchar *sFileName, tHP8753calibrationKit *pHP8753calibrationKit ) {
    GMarkupParseContext *context;

    gchar *sLine = NULL;
    size_t lineAllocLen = 0;
    ssize_t lineLen;
    FILE *fp;
    GError *error = NULL;
    gboolean bParseSuccess = TRUE;

    if ( !( fp = fopen( sFileName, "r" )) ) {
    	g_print("cannot open %s! \n", sFileName);
    	return -1;
    }

    context = g_markup_parse_context_new(&parserTop, 0, &calKit, freeCalKit);
    if(NULL == context) {
        g_print("context new failed! \n");
        return -1;
    }

    while ((lineLen = getline( &sLine, &lineAllocLen, fp)) != -1) {
		if( !g_markup_parse_context_parse(context, sLine, lineLen, &error) ) {
			bParseSuccess = FALSE;
			break;
		}
    }
    if( sLine ) {
    	free( sLine );
    }

    fclose(fp);

    if( !bParseSuccess ) {
    	g_error_free (error);
        g_markup_parse_context_free(context);
    	return ERROR;
    } else {
    	g_markup_parse_context_end_parse(context, &error);
    }

    // Construct composite connector descriptor from 'family' and 'gender'
    // we can then compare this to the 'PortConnectorIDs' from the 'standard' definition.
	gint nConnectors = g_list_length( calKit.lConnectorList );
	gchar *portConnectors[ nConnectors ];
	gboolean __attribute__((unused)) bValid = TRUE; // assume valid unless we find something amis
	gint i = 0;
	for( GList *thisConnector = g_list_first( calKit.lConnectorList );
			thisConnector != NULL; thisConnector = thisConnector->next, i++ ) {
		g_strstrip(((tXKTconnector *)thisConnector->data)->family );
		g_strstrip(((tXKTconnector *)thisConnector->data)->gender );
		portConnectors[ i ] = g_strdup_printf( "%s %s", ((tXKTconnector *)thisConnector->data)->family,
				((tXKTconnector *)thisConnector->data)->gender );
	}

	/// extract information needed by HP8753 from XML data
	// first clear the structure
	bzero( pHP8753calibrationKit, sizeof( tHP8753calibrationKit ) );

	g_strlcpy( pHP8753calibrationKit->label, calKit.sCalKitLabel, MAX_CALKIT_LABEL_SIZE );
	g_strlcpy( pHP8753calibrationKit->description, calKit.sCalKitDescription, MAX_CALKIT_DESCRIPTION_SIZE );

	// for each 'standard' defined in the XKT, populate the structure
	for( GList *thsItem = g_list_first( calKit.lStandardList );
			thsItem != NULL; thsItem = thsItem->next ) {
		tXKTstandard *thisStandard = (tXKTstandard *)thsItem->data;
		if( thisStandard->number > MAX_CAL_STANDARDS ) {
			bValid = FALSE;
			continue;
		}

		tHP8753calibrationStandard *pStd = &pHP8753calibrationKit->calibrationStandards[ thisStandard->number-1 ];
		pStd->bSpecified = TRUE;

		// find the connector that matches the <PortConnectorIDs>
		for( GList *thsItem = g_list_first( thisStandard->portConnectorIDs );
					thsItem != NULL; thsItem = thsItem->next ) {
			gchar *portConnectorID = (gchar *)thsItem->data;

			for( int i=0; i < nConnectors; i++ ) {
				if( !strcmp( portConnectorID, portConnectors[ i ] ) ) {
					pStd->connectorType = ((tXKTconnector *)g_list_nth( calKit.lConnectorList, i )->data)->type;
					break;
				}
			}
		}

		pStd->calibrationType = thisStandard->type;

		g_strlcpy( pStd->label, thisStandard->label, MAX_CAL_LABEL_SIZE + 1);

		pStd->maxFreqHz = thisStandard->maxFreqHz;
		pStd->minFreqHz = thisStandard->minFreqHz;
		pStd->offsetDelay = thisStandard->offset.offsetDelay;
		pStd->offsetLoss = thisStandard->offset.offsetLoss;
		pStd->offsetZ0 = thisStandard->offset.offsetZ0;
		switch ( pStd->calibrationType ) {
		case eStdTypeOpen:
			for ( int i=0; i < ORDER_OPEN_CORR_POLYNIMIAL; i++ ) {
				pStd->C[ i ] = thisStandard->C[ i ];
			}
			break;
		case eStdTypeShort:
			break;
		case eStdTypeFixedLoad:
			break;
		case eStdTypeThru:
			break;
		case eStdTypeSlidingLoad:
			break;
		case eStdTypeArbitraryImpedanceLoad:
			pStd->arbitraryZ0 = thisStandard->terminationImpedance.real;
			// HP8753 only accepts real arbitrary impedances
			if( thisStandard->terminationImpedance.imag != 0.0 )
				bValid = FALSE;
			break;
		case eStdTypeUnknown:
		default:
			bValid = FALSE;
			break;
		}
	}

	// SA, SB, SC, FORWARD_THRU, FORWARD_MATCH, REVERSE_THRU, REVERSE_MATCH, ISOLATION, TRL_THRU
	for( GList *thisItem = g_list_first( calKit.lKitClasses );
			thisItem != NULL; thisItem = thisItem->next ) {
		tXKTkitClass *thisKitClass = (tXKTkitClass *)thisItem->data;

		switch( thisKitClass->classID ) {
		case eKitClassSA:
			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11A ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11A ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11A ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);

			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS22A ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS22A ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS22A ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);
			break;
		case eKitClassSB:
			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11B ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11B ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11B ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);

			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS22B ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS22B ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS22B ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);
			break;
		case eKitClassSC:
			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11C ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11C ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11C ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);

			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS22C ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS22C ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS22C ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);
			break;
		case eKitClassForwardThru:
			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassFwdTrans ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassFwdTrans ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassFwdTrans ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);
			break;
		case eKitClassForwardMatch:
			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassFwdMatch ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassFwdMatch ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassFwdMatch ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);
			break;
		case eKitClassReverseThru:
			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassRevTrans ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassRevTrans ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassRevTrans ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);
			break;
		case eKitClassReverseMatch:
				pHP8753calibrationKit->calibrationClasses[ eHP8753calClassRevMatch ].bSpecified = TRUE;
				g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassRevMatch ].label,
						thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
				g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassRevMatch ].standards,
						thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);
				break;
		case eKitClassTRLreflect:
			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLreflectFwdMatch ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLreflectFwdMatch ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLreflectFwdMatch ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);
			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLreflectFwdMatch ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLreflectFwdMatch ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLreflectFwdMatch ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);
			break;
		case eKitClassTRLline:
			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLlineFwdTrans ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLlineFwdTrans ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLlineFwdTrans ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);

			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLlineFwdMatch ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLlineFwdMatch ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLlineFwdMatch ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);

			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLlineRevTrans ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLlineRevTrans ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLlineRevTrans ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);

			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLlineRevMatch ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLlineRevMatch ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLlineRevMatch ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);
			break;
		case eKitClassTRLthru:
			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLthruFwdTrans ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLthruFwdTrans ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLthruFwdTrans ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);

			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLthruFwdMatch ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLthruFwdMatch ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLthruFwdMatch ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);

			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLthruRevTrans ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLthruRevTrans ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLthruRevTrans ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);

			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLthruRevMatch ].bSpecified = TRUE;
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLthruRevMatch ].label,
					thisKitClass->label, MAX_CAL_LABEL_SIZE + 1);
			g_Strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassTRLthruRevMatch ].standards,
					thisKitClass->StandardsList,  MAX_CAL_STANDARDS * 2 + 1);
			break;
		case eKitClassTRLmatch:
			break;
				break;
		case eKitClassIsolation:
		case eKitClassUnknown:
				break;
		default:
			break;
		}
	}

	// The XKT has no 'Response' or 'Response+Isolation' class, so we synthesize this
	// with open, short & thru standards
	gboolean bAddComma = FALSE;
	g_strlcat( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponse ].label,
	    				"RESPONSE", MAX_CAL_STANDARDS * 2 + 1	);
    if( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11A ].bSpecified ) {
    	pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponse ].bSpecified = TRUE;
		g_strlcat( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponse ].standards,
					pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11A ].standards,
					MAX_CAL_STANDARDS * 2 + 11);
		bAddComma = TRUE;
    }
    if( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11B ].bSpecified ) {
    	pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponse ].bSpecified = TRUE;
    	if( bAddComma ) {
    		g_strlcat( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponse ].standards,
    						",", MAX_CAL_STANDARDS * 2 + 11);
    	}
		g_strlcat( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponse ].standards,
				pHP8753calibrationKit->calibrationClasses[ eHP8753calClassS11B ].standards,
				MAX_CAL_STANDARDS * 2 + 11);
		bAddComma = TRUE;
    }
    if( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassFwdTrans ].bSpecified ) {
    	pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponse ].bSpecified = TRUE;
    	if( bAddComma ) {
    		g_strlcat( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponse ].standards,
    						",", MAX_CAL_STANDARDS * 2 + 11);
    	}
		g_strlcat( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponse ].standards,
				pHP8753calibrationKit->calibrationClasses[ eHP8753calClassFwdTrans ].standards,
				MAX_CAL_STANDARDS * 2 + 11);
		bAddComma = TRUE;
    }

    // The Response + Isolation is a copy of the response plus the isolation
    if( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponse ].bSpecified ) {
    	pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponseAndIsolation ].bSpecified = TRUE;
    	g_strlcpy( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponseAndIsolation ].standards,
    			pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponse ].standards,
				MAX_CAL_STANDARDS * 2 + 1);

    	g_strlcat( pHP8753calibrationKit->calibrationClasses[ eHP8753calClassResponseAndIsolation ].label,
    	    				"RESPONSE", MAX_CAL_STANDARDS * 2 + 1	);
    }

    for( gint i=0; i < MAX_CAL_CLASSES; i++ ) {
    	if( !pHP8753calibrationKit->calibrationClasses[ i ].bSpecified )
    		continue;
    	LOG( G_LOG_LEVEL_DEBUG, "Cal class %-10s: %s\n",
    			pHP8753calibrationKit->calibrationClasses[ i ].label,
				pHP8753calibrationKit->calibrationClasses[ i ].standards);
    }

	// Free allocated memory
	for( gint i = 0; i < nConnectors; i++ )
		g_free( portConnectors[ i ] );


    // this also frees the allocated strings in the calKit structure
    g_markup_parse_context_free(context);

    return 0;
}
