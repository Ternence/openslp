/*-------------------------------------------------------------------------
 * Copyright (C) 2000 Caldera Systems, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 *    Neither the name of Caldera Systems nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * `AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE CALDERA
 * SYSTEMS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *-------------------------------------------------------------------------*/

/** Types and prototypes for SLP configuration property access.
 *
 * @file       slp_property.h
 * @author     Matthew Peterson, John Calcote (jcalcote@novell.com)
 * @attention  Please submit patches to http://www.openslp.org
 * @ingroup    CommonCode
 */

#ifndef SLP_PROPERTY_H_INCLUDED
#define SLP_PROPERTY_H_INCLUDED

#include "slp_linkedlist.h"

/*!@defgroup CommonCodeProp Property Access */

/*!@addtogroup CommonCodeProp
 * @ingroup CommonCode
 * @{
 */

extern SLPList G_SLPPropertyList;

typedef struct _SLPProperty
{
   SLPListItem listitem;
   /*!< @brief Make the SLPProperty class list-able.
    */
   char * propertyName;
   /*!< @brief The name of this property.
    */
   char * propertyValue;
   /*!< @brief The value of this property.
    */
} SLPProperty;

const char* SLPPropertyGet(const char* pcName); 

int SLPPropertySet(const char *pcName,                                     
                   const char *pcValue);

int SLPPropertyReadFile(const char* conffile);                             

int SLPPropertyAsBoolean(const char* property);

int SLPPropertyAsInteger(const char* property);

int SLPPropertyAsIntegerVector(const char* property, 
                               int* vector, 
                               int vectorsize);

#ifdef DEBUG
void SLPPropertyFreeAll(void);
#endif

/*! @} */

#endif   /* SLP_PROPERTY_H_INCLUDED */

/*=========================================================================*/
