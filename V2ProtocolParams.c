/* This file is based on the excellent LUFA Library, which is:
  Copyright (C) Dean Camera, 2013. See www.lufa-lib.org
  
  Note it has been modified for the SAM3U by NewAE Technology Inc.,
  changes Copyright (C) NewAE Technology Inc, 2015. Changes have
  generally basterdized and reduced the functionality of the original
  code, so please see original code if using this to port. 
*/


/*
  Copyright 2013  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  V2Protocol parameter handler, to process V2 Protocol device parameters.
 */

#define  INCLUDE_FROM_V2PROTOCOL_PARAMS_C
#include "V2ProtocolParams.h"

#include <asf.h>
/* Non-Volatile Parameter Values for EEPROM storage */
//static uint8_t EEPROM_Reset_Polarity = 0x01;

/* Non-Volatile Parameter Values for EEPROM storage */
//static uint8_t EEPROM_SCK_Duration   = 0x06;

/* Volatile Parameter Values for RAM storage */
static ParameterItem_t ParameterTable[] =
	{
		{ .ParamID          = PARAM_BUILD_NUMBER_LOW,
		  .ParamPrivileges  = PARAM_PRIV_READ,
		  .ParamValue       = 0                                  },

		{ .ParamID          = PARAM_BUILD_NUMBER_HIGH,
		  .ParamPrivileges  = PARAM_PRIV_READ,
		  .ParamValue       = 0                                  },

		{ .ParamID          = PARAM_HW_VER,
		  .ParamPrivileges  = PARAM_PRIV_READ,
		  .ParamValue       = 0x00                               },

		{ .ParamID          = PARAM_SW_MAJOR,
		  .ParamPrivileges  = PARAM_PRIV_READ,
		  .ParamValue       = 0x01                               },

		{ .ParamID          = PARAM_SW_MINOR,
		  .ParamPrivileges  = PARAM_PRIV_READ,
		  .ParamValue       = FIRMWARE_VERSION_MINOR             },

		{ .ParamID          = PARAM_VTARGET,
		  .ParamPrivileges  = PARAM_PRIV_READ,
		  .ParamValue       = (uint8_t)(3.3 * 10)                },

		{ .ParamID          = PARAM_SCK_DURATION,
		  .ParamPrivileges  = PARAM_PRIV_READ | PARAM_PRIV_WRITE,
		  .ParamValue       = 6                                  },

		{ .ParamID          = PARAM_RESET_POLARITY,
		  .ParamPrivileges  = PARAM_PRIV_READ | PARAM_PRIV_WRITE,
		  .ParamValue       = 0x01                               },

		{ .ParamID          = PARAM_STATUS_TGT_CONN,
		  .ParamPrivileges  = PARAM_PRIV_READ,
		  .ParamValue       = STATUS_ISP_READY                   },

		{ .ParamID          = PARAM_DISCHARGEDELAY,
		  .ParamPrivileges  = PARAM_PRIV_READ | PARAM_PRIV_WRITE,
		  .ParamValue       = 0x00                               },
	};

/** Retrieves the host PC read/write privileges for a given parameter in the parameter table. This should
 *  be called before calls to \ref V2Params_GetParameterValue() or \ref V2Params_SetParameterValue() when
 *  getting or setting parameter values in response to requests from the host.
 *
 *  \param[in] ParamID  Parameter ID whose privileges are to be retrieved from the table
 *
 *  \return Privileges for the requested parameter, as a mask of \c PARAM_PRIV_* masks
 */
uint8_t V2Params_GetParameterPrivileges(const uint8_t ParamID)
{
	ParameterItem_t* const ParamInfo = V2Params_GetParamFromTable(ParamID);

	if (ParamInfo == NULL)
	  return 0;

	return ParamInfo->ParamPrivileges;
}

/** Retrieves the current value for a given parameter in the parameter table.
 *
 *  \note This function does not first check for read privileges - if the value is being sent to the host via a
 *        GET PARAM command, \ref V2Params_GetParameterPrivileges() should be called first to ensure that the
 *        parameter is host-readable.
 *
 *  \param[in] ParamID  Parameter ID whose value is to be retrieved from the table
 *
 *  \return Current value of the parameter in the table, or 0 if not found
 */
uint8_t V2Params_GetParameterValue(const uint8_t ParamID)
{
	ParameterItem_t* const ParamInfo = V2Params_GetParamFromTable(ParamID);

	if (ParamInfo == NULL)
	  return 0;

	return ParamInfo->ParamValue;
}

/** Sets the value for a given parameter in the parameter table.
 *
 *  \note This function does not first check for write privileges - if the value is being sourced from the host
 *        via a SET PARAM command, \ref V2Params_GetParameterPrivileges() should be called first to ensure that the
 *        parameter is host-writable.
 *
 *  \param[in] ParamID  Parameter ID whose value is to be set in the table
 *  \param[in] Value    New value to set the parameter to
 *
 *  \return Pointer to the associated parameter information from the parameter table if found, NULL otherwise
 */
void V2Params_SetParameterValue(const uint8_t ParamID,
                                const uint8_t Value)
{
	ParameterItem_t* const ParamInfo = V2Params_GetParamFromTable(ParamID);

	if (ParamInfo == NULL)
	  return;

	ParamInfo->ParamValue = Value;
}

/** Retrieves a parameter entry (including ID, value and privileges) from the parameter table that matches the given
 *  parameter ID.
 *
 *  \param[in] ParamID  Parameter ID to find in the table
 *
 *  \return Pointer to the associated parameter information from the parameter table if found, NULL otherwise
 */
static ParameterItem_t* const V2Params_GetParamFromTable(const uint8_t ParamID)
{
	ParameterItem_t* CurrTableItem = ParameterTable;

	/* Find the parameter in the parameter table if present */
	for (uint8_t TableIndex = 0; TableIndex < TABLE_PARAM_COUNT; TableIndex++)
	{
		if (ParamID == CurrTableItem->ParamID)
		  return CurrTableItem;

		CurrTableItem++;
	}

	return NULL;
}

