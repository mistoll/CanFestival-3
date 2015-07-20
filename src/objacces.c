/*
  This file is part of CanFestival, a library implementing CanOpen
  Stack.

  Copyright (C): Edouard TISSERANT and Francis DUPIN

  See COPYING file for copyrights details.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
  USA
*/
/*!
** @file   objacces.c
** @author Edouard TISSERANT and Francis DUPIN
** @date   Tue Jun  5 08:55:23 2007
**
** @brief
**
**
*/




/* #define DEBUG_WAR_CONSOLE_ON */
/* #define DEBUG_ERR_CONSOLE_ON */


#include "data.h"
#include "objacces.h"

#include <board.h>


UNS32 _setODentry( CO_Data* d,
                   UNS16 wIndex,
                   UNS8 bSubindex,
                   void * pSourceData,
                   UNS32 * pExpectedSize,
                   UNS8 checkAccess,
                   UNS8 endianize)
{
  UNS32 szData;
  UNS8 dataType;
  UNS32 errorCode;
  const indextable *ptrTable;
  ODCallback_t *Callback;

  ptrTable =(*d->scanIndexOD)(wIndex, &errorCode, &Callback);
  if (errorCode != OD_SUCCESSFUL)
    return errorCode;

  if( ptrTable->bSubCount <= bSubindex ) {
    /* Subindex not found */
    accessDictionaryError(wIndex, bSubindex, 0, *pExpectedSize, OD_NO_SUCH_SUBINDEX);
    return OD_NO_SUCH_SUBINDEX;
  }
  if (checkAccess && (ptrTable->pSubindex[bSubindex].bAccessType == RO)) {
    MSG_WAR(0x2B25, "Access Type : ", ptrTable->pSubindex[bSubindex].bAccessType);
    accessDictionaryError(wIndex, bSubindex, 0, *pExpectedSize, OD_WRITE_NOT_ALLOWED);
    return OD_WRITE_NOT_ALLOWED;
  }


  dataType = ptrTable->pSubindex[bSubindex].bDataType;
  szData = ptrTable->pSubindex[bSubindex].size;

  if( *pExpectedSize == 0 ||
      *pExpectedSize == szData ||
      /* allow to store a shorter string than entry size */
      (dataType == visible_string && *pExpectedSize < szData))
    {
#ifdef CANOPEN_BIG_ENDIAN
      /* re-endianize do not occur for bool, strings time and domains */
      if(endianize && dataType > boolean && !(
            dataType >= visible_string && 
            dataType <= domain))
        {
          /* we invert the data source directly. This let us do range
            testing without */
          /* additional temp variable */
          UNS8 i;
          for ( i = 0 ; i < ( ptrTable->pSubindex[bSubindex].size >> 1)  ; i++)
            {
              UNS8 tmp =((UNS8 *)pSourceData) [(ptrTable->pSubindex[bSubindex].size - 1) - i];
              ((UNS8 *)pSourceData) [(ptrTable->pSubindex[bSubindex].size - 1) - i] = ((UNS8 *)pSourceData)[i];
              ((UNS8 *)pSourceData)[i] = tmp;
            }
        }
#endif
      errorCode = (*d->valueRangeTest)(dataType, pSourceData);
      if (errorCode) {
        accessDictionaryError(wIndex, bSubindex, szData, *pExpectedSize, errorCode);
        return errorCode;
      }
      memcpy(ptrTable->pSubindex[bSubindex].pObject,pSourceData, *pExpectedSize);
     /* TODO : CONFORM TO DS-301 : 
      *  - stop using NULL terminated strings
      *  - store string size in td_subindex 
      * */
      /* terminate visible_string with '\0' */
      if(dataType == visible_string && *pExpectedSize < szData)
        ((UNS8*)ptrTable->pSubindex[bSubindex].pObject)[*pExpectedSize] = 0;
      
      *pExpectedSize = szData;

      /* Callbacks */
      if(Callback && Callback[bSubindex]){
        errorCode = (Callback[bSubindex])(d, ptrTable, bSubindex);
        if(errorCode != OD_SUCCESSFUL)
        {
            return errorCode;
        }
       }

      /* TODO : Store dans NVRAM */
      if (ptrTable->pSubindex[bSubindex].bAccessType & TO_BE_SAVE){
        (*d->storeODSubIndex)(d, wIndex, bSubindex);
      }
      return OD_SUCCESSFUL;
    }else{
      *pExpectedSize = szData;
      accessDictionaryError(wIndex, bSubindex, szData, *pExpectedSize, OD_LENGTH_DATA_INVALID);
      return OD_LENGTH_DATA_INVALID;
    }
}

const indextable * scanIndexOD (CO_Data* d, UNS16 wIndex, UNS32 *errorCode, ODCallback_t **Callback)
{
  return (*d->scanIndexOD)(wIndex, errorCode, Callback);
}

UNS32 RegisterSetODentryCallBack(CO_Data* d, UNS16 wIndex, UNS8 bSubindex, ODCallback_t Callback)
{
UNS32 errorCode;
ODCallback_t *CallbackList;
const indextable *odentry;

  odentry = scanIndexOD (d, wIndex, &errorCode, &CallbackList);
  if(errorCode == OD_SUCCESSFUL  &&  CallbackList  &&  bSubindex < odentry->bSubCount) 
    CallbackList[bSubindex] = Callback;
  return errorCode;
}

void _storeODSubIndex (CO_Data* d, UNS16 wIndex, UNS8 bSubindex){}
