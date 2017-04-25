/*
 ******************************************************************************
 *
 *   This File Contains PROPRIETARY and CONFIDENTIAL TRADE SECRET information
 *   which is the exclusive property of VeriSolutions, LLC.  "Trade Secret"
 *   means information constituting a trade secret within the meaning of
 *   Section 10-1-761(4) of the Georgia Trade Secrets Act of 1990, including
 *   all amendments hereafter adopted.
 *
 *   (c) 2017 by VeriSolutions, LLC.                 www.VeriSolutions.co
 *
 ******************************************************************************
 *
 *      Firmware Download to Sensor From Hub Routines
 *
 *
 *
 *
 ******************************************************************************
 */

#include "hs-common.h"

//------------------------------------------------------------------------------
//  
//
void FWDownloadFromHubBegin (struct FWB*, enum SensorKind_t);

//------------------------------------------------------------------------------
//  
//
void FWDownloadFromHubProcessChunk (struct FWC*);

//------------------------------------------------------------------------------
//  
//
bool FWDownloadFromHubInProgress();

//------------------------------------------------------------------------------
