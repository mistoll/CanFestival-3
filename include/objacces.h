/*
This file is part of CanFestival, a library implementing CanOpen Stack. 

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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/** @file
 *  @brief Responsible for accessing the object dictionary.
 *
 *  This file contains functions for accessing the object dictionary and
 *  variables that are contained by the object dictionary.
 *  Accessing the object dictionary contains setting local variables
 *  as PDOs and accessing (read/write) all entries of the object dictionary
 *  @warning Only the basic entries of an object dictionary are included
 *           at the moment.
 */

/** @defgroup od Object Dictionary Management
 *  @brief The Object Dictionary is the heart of each CANopen device containing all communication and application objects.
 *  @ingroup userapi
 */
 
#ifndef __objacces_h__
#define __objacces_h__

#include <applicfg.h>
#include "data.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Print MSG_WAR (s) if error to the access to the object dictionary occurs.
 * 
 * You must uncomment the lines in the file objaccess.c :\n
 * //#define DEBUG_CAN\n
 * //#define DEBUG_WAR_CONSOLE_ON\n
 * //#define DEBUG_ERR_CONSOLE_ON\n\n
 * Beware that sometimes, we force the sizeDataDict or sizeDataGiven to 0, when we wants to use
 * this function but we do not have the access to the right value. One example is
 * getSDOerror(). So do not take attention to these variables if they are null.
 * @param index
 * @param subIndex
 * @param sizeDataDict Size of the data defined in the dictionary
 * @param sizeDataGiven Size data given by the user.
 * @param code error code to print. (SDO abort code. See file def.h)
 * @return
 */ 

//We need the function implementation for linking
//Only a placeholder with a define isnt enough!
static inline UNS8 accessDictionaryError(UNS16 index, UNS8 subIndex,
	UNS32 sizeDataDict, UNS32 sizeDataGiven, UNS32 code)
{
	#ifdef DEBUG_WAR_CONSOLE_ON
	MSG_WAR(0x2B09,"Dictionary index : ", index);
	MSG_WAR(0X2B10,"           subindex : ", subIndex);
	switch (code) {
		case  OD_NO_SUCH_OBJECT:
		MSG_WAR(0x2B11,"Index not found ", index);
		break;
		case OD_NO_SUCH_SUBINDEX :
		MSG_WAR(0x2B12,"SubIndex not found ", subIndex);
		break;
		case OD_WRITE_NOT_ALLOWED :
		MSG_WAR(0x2B13,"Write not allowed, data is read only ", index);
		break;
		case OD_LENGTH_DATA_INVALID :
		MSG_WAR(0x2B14,"Conflict size data. Should be (bytes)  : ", sizeDataDict);
		MSG_WAR(0x2B15,"But you have given the size  : ", sizeDataGiven);
		break;
		case OD_NOT_MAPPABLE :
		MSG_WAR(0x2B16,"Not mappable data in a PDO at index    : ", index);
		break;
		case OD_VALUE_TOO_LOW :
		MSG_WAR(0x2B17,"Value range error : value too low. SDOabort : ", code);
		break;
		case OD_VALUE_TOO_HIGH :
		MSG_WAR(0x2B18,"Value range error : value too high. SDOabort : ", code);
		break;
		default :
		MSG_WAR(0x2B20, "Unknown error code : ", code);
	}
	#endif

	return 0;
}


/* _getODentry() Reads an entry from the object dictionary.\n
 * 
 *    use getODentry() macro to read from object and endianize
 *    use readLocalDict() macro to read from object and not endianize   
 *
 * @code
 * // Example usage:
 * UNS8  *pbData;
 * UNS8 length;
 * UNS32 returnValue;
 *
 * returnValue = getODentry( (UNS16)0x100B, (UNS8)1, 
 * (void * *)&pbData, (UNS8 *)&length );
 * if( returnValue != SUCCESSFUL )
 * {
 *     // error handling
 * }
 * @endcode 
 * @param *d Pointer to a CAN object data structure
 * @param wIndex The index in the object dictionary where you want to read
 *               an entry
 * @param bSubindex The subindex of the Index. e.g. mostly subindex 0 is
 *                  used to tell you how many valid entries you can find
 *                  in this index. Look at the canopen standard for further
 *                  information
 * @param *pDestData Pointer to the pointer which points to the variable where
 *                   the value of this object dictionary entry should be copied
 * @param pExpectedSize This function writes the size of the copied value (in Byte)
 *                      into this variable.
 * @param *pDataType Pointer to the type of the data. See objdictdef.h
 * @param CheckAccess if other than 0, do not read if the data is Write Only
 *                    [Not used today. Put always 0].
 * @param Endianize  When not 0, data is endianized into network byte order
 *                   when 0, data is not endianized and copied in machine native
 *                   endianness 
 * @return 
 * - OD_SUCCESSFUL is returned upon success. 
 * - SDO abort code is returned if error occurs . (See file def.h)
 */

static inline UNS32 _findODentry( CO_Data* d,
	UNS16 wIndex,
	UNS8 bSubindex,
	const subindex** result)
{ /* DO NOT USE MSG_ERR because the macro may send a PDO -> infinite
loop if it fails. */
	UNS32 errorCode;
	const indextable *ptrTable;
	ODCallback_t *Callback;
	
	ptrTable = (*d->scanIndexOD)(wIndex, &errorCode, &Callback);

	if (errorCode != OD_SUCCESSFUL)
		return errorCode;
		
	if( ptrTable->bSubCount <= bSubindex ) {
		/* Subindex not found */
		accessDictionaryError(wIndex, bSubindex, 0, 0, OD_NO_SUCH_SUBINDEX);
		return OD_NO_SUCH_SUBINDEX;
	}
	*result = &ptrTable->pSubindex[bSubindex];
	return OD_SUCCESSFUL;
}

static inline uint8_t _checkODentryAccess(const CO_Data* d,
	const subindex *ptrTable, UNS16 wIndex, UNS8 bSubindex) {
	if (ptrTable->bAccessType & WO) {
		MSG_WAR(0x2B30, "Access Type : ", ptrTable->pSubindex[bSubindex].bAccessType);
		accessDictionaryError(wIndex, bSubindex, 0, 0, OD_READ_NOT_ALLOWED);
		return TRUE;
	}
	return FALSE;
}

static inline UNS32 _copyODentry( CO_Data* d,
	const subindex *ptrTable,
	UNS8 bSubindex,
	void * pDestData,
	UNS8 dataType,
	UNS32 * pExpectedSize,
	UNS8 endianize)
{
	UNS32 szData;
	if (ptrTable->size > *pExpectedSize) {
		/* Requested variable is too large to fit into a transfer line, inform    *
			* the caller about the real size of the requested variable.              */
		*pExpectedSize = ptrTable->size;
		return SDOABT_OUT_OF_MEMORY;
	}

	szData = ptrTable->size;
	
#  ifdef CANOPEN_BIG_ENDIAN
	if(endianize && dataType > boolean && !(
		dataType >= visible_string &&
		dataType <= domain)) {
		/* data must be transmited with low byte first */
		UNS8 i, j = 0;
		MSG_WAR(boolean, "data type ", dataType);
		MSG_WAR(visible_string, "data type ", dataType);
		for ( i = szData ; i > 0 ; i--) {
			MSG_WAR(i," ", j);
			((UNS8*)pDestData)[j++] =
			((UNS8*)ptrTable->pObject)[i-1];
		}
		*pExpectedSize = szData;
	}
	else /* no endianisation change */
#  endif

	if(dataType != visible_string) {
		memcpy(pDestData, ptrTable->pObject,szData);
		*pExpectedSize = szData;
	}else{
		/* TODO : CONFORM TO DS-301 :
		*  - stop using NULL terminated strings
		*  - store string size in td_subindex
		* */
		/* Copy null terminated string to user, and return discovered size */
		UNS8 *ptr = (UNS8*)ptrTable->pObject;
		UNS8 *ptr_start = ptr;
		/* *pExpectedSize IS < szData . if null, use szData */
		UNS8 *ptr_end = ptr + (*pExpectedSize ? *pExpectedSize : szData) ;
		UNS8 *ptr_dest = (UNS8*)pDestData;
		while( *ptr && ptr < ptr_end){
			*(ptr_dest++) = *(ptr++);
		}

		*pExpectedSize = (UNS32) (ptr - ptr_start);
		/* terminate string if not maximum length */
		if (*pExpectedSize < szData)
		*(ptr) = 0;
	}

	return OD_SUCCESSFUL;
}
/** 
 * @ingroup od
 * @brief getODentry() to read from object and endianize
 * @param OD Pointer to a CAN object data structure
 * @param wIndex The index in the object dictionary where you want to read
 *                an entry
 * @param bSubindex The subindex of the Index. e.g. mostly subindex 0 is
 *                  used to tell you how many valid entries you can find
 *                  in this index. Look at the canopen standard for further
 *                  information
 * @param *pDestData Pointer to the pointer which points to the variable where
 *                   the value of this object dictionary entry should be copied
 * @param pExpectedSize This function writes the size of the copied value (in Byte)
 *                      into this variable.
 * @param *pDataType Pointer to the type of the data. See objdictdef.h
 * @param checkAccess Flag that indicate if a check rights must be perfomed (0 : no , other than 0 : yes)
 * @param endianize  Set to 1 : endianized into network byte order 
 * @return 
 * - OD_SUCCESSFUL is returned upon success. 
 * - SDO abort code is returned if error occurs . (See file def.h)
 */
static inline UNS32 getODentry(CO_Data* OD, UNS16 wIndex, UNS8 bSubindex, void* pDestData, UNS32* pExpectedSize,
		          UNS8* pDataType,  UNS8 checkAccess) {
	UNS32 err;
	const subindex* ptrTable = NULL;
	err = _findODentry(OD,wIndex, bSubindex, &ptrTable);
	if (err != OD_SUCCESSFUL)
		return err;
	if (checkAccess)
		if (!_checkODentryAccess(OD, ptrTable, wIndex, bSubindex))
			return OD_READ_NOT_ALLOWED;
	
	*pDataType = ptrTable->bDataType;
	err = _copyODentry(OD,ptrTable, bSubindex, pDestData, *pDataType, pExpectedSize, TRUE);
	return err;
}
/** 
 * @ingroup od
 * @brief readLocalDict() reads an entry from the object dictionary, but in 
 * contrast to getODentry(), readLocalDict() doesn't endianize entry and reads
 * entry in machine native endianness. 
 * @param OD Pointer to a CAN object data structure
 * @param wIndex The index in the object dictionary where you want to read
 *                an entry
 * @param bSubindex The subindex of the Index. e.g. mostly subindex 0 is
 *                  used to tell you how many valid entries you can find
 *                  in this index. Look at the canopen standard for further
 *                  information
 * @param *pDestData Pointer to the pointer which points to the variable where
 *                   the value of this object dictionary entry should be copied
 * @param pExpectedSize This function writes the size of the copied value (in Byte)
 *                      into this variable.
 * @param *pDataType Pointer to the type of the data. See objdictdef.h
 * @param checkAccess if other than 0, do not read if the data is Write Only
 *                    [Not used today. Put always 0].
 * @param endianize Set to 0, data is not endianized and copied in machine native
 *                  endianness 
 * @return 
 * - OD_SUCCESSFUL is returned upon success. 
 * - SDO abort code is returned if error occurs . (See file def.h)
 */
#define readLocalDict( OD, wIndex, bSubindex, pDestData, pExpectedSize, \
		          pDataType,  checkAccess)                         \
       _getODentry( OD, wIndex, bSubindex, pDestData, pExpectedSize, \
		          pDataType,  checkAccess, 0)

/* By this function you can write an entry into the object dictionary
 * @param *d Pointer to a CAN object data structure
 * @param wIndex The index in the object dictionary where you want to write
 *               an entry
 * @param bSubindex The subindex of the Index. e.g. mostly subindex 0 is
 *                  used to tell you how many valid entries you can find
 *                  in this index. Look at the canopen standard for further
 *                  information
 * @param *pSourceData Pointer to the variable that holds the value that should
 *                     be copied into the object dictionary
 * @param *pExpectedSize The size of the value (in Byte).
 * @param checkAccess Flag that indicate if a check rights must be perfomed (0 : no , other than 0 : yes)
 * @param endianize When not 0, data is endianized into network byte order
 *                  when 0, data is not endianized and copied in machine native
 *                  endianness   
 * @return 
 * - OD_SUCCESSFUL is returned upon success. 
 * - SDO abort code is returned if error occurs . (See file def.h)
 */
UNS32 _setODentry( CO_Data* d,
                   UNS16 wIndex,
                   UNS8 bSubindex,
                   void * pSourceData,
                   UNS32 * pExpectedSize,
                   UNS8 checkAccess,
                   UNS8 endianize);

/**
 * @ingroup od
 * @brief setODentry converts SourceData from network byte order to machine native 
 * format, and writes that to OD.
 * @code
 * // Example usage:
 * UNS8 B;
 * B = 0xFF; // set transmission type
 *
 * retcode = setODentry( (UNS16)0x1800, (UNS8)2, &B, sizeof(UNS8), 1 );
 * @endcode
 * @param d Pointer to a CAN object data structure
 * @param wIndex The index in the object dictionary where you want to write
 *               an entry
 * @param bSubindex The subindex of the Index. e.g. mostly subindex 0 is
 *                  used to tell you how many valid entries you can find
 *                  in this index. Look at the canopen standard for further
 *                  information
 * @param *pSourceData Pointer to the variable that holds the value that should
 *                     be copied into the object dictionary
 * @param *pExpectedSize The size of the value (in Byte).
 * @param checkAccess Flag that indicate if a check rights must be perfomed (0 : no , other than 0 : yes)
 * @param endianize Set to 1 : endianized into network byte order
 * @return 
 * - OD_SUCCESSFUL is returned upon success. 
 * - SDO abort code is returned if error occurs . (See file def.h)
 */
#define setODentry( d, wIndex, bSubindex, pSourceData, pExpectedSize, \
                  checkAccess) \
       _setODentry( d, wIndex, bSubindex, pSourceData, pExpectedSize, \
                  checkAccess, 1)

/** @fn UNS32 writeLocalDict(d, wIndex, bSubindex, pSourceData, pExpectedSize, checkAccess)
 * @ingroup od
 * @hideinitializer
 * @brief Writes machine native SourceData to OD.
 * @param d Pointer to a CAN object data structure
 * @param wIndex The index in the object dictionary where you want to write
 *               an entry
 * @param bSubindex The subindex of the Index. e.g. mostly subindex 0 is
 *                  used to tell you how many valid entries you can find
 *                  in this index. Look at the canopen standard for further
 *                  information
 * @param *pSourceData Pointer to the variable that holds the value that should
 *                     be copied into the object dictionary
 * @param *pExpectedSize The size of the value (in Byte).
 * @param checkAccess Flag that indicate if a check rights must be perfomed (0 : no , other than 0 : yes)
 * @param endianize Data is not endianized and copied in machine native endianness 
 * @return 
 * - OD_SUCCESSFUL is returned upon success. 
 * - SDO abort code is returned if error occurs . (See file def.h)
 * \n\n
 * @code
 * // Example usage:
 * UNS8 B;
 * B = 0xFF; // set transmission type
 *
 * retcode = writeLocalDict( (UNS16)0x1800, (UNS8)2, &B, sizeof(UNS8), 1 );
 * @endcode
 */
#define writeLocalDict( d, wIndex, bSubindex, pSourceData, pExpectedSize, checkAccess) \
       _setODentry( d, wIndex, bSubindex, pSourceData, pExpectedSize, checkAccess, 0)



/**
 * @brief Scan the index of object dictionary. Used only by setODentry and getODentry.
 * @param *d Pointer to a CAN object data structure
 * @param wIndex
 * @param *errorCode :  OD_SUCCESSFUL if index foundor SDO abort code. (See file def.h)
 * @param **Callback
 * @return NULL if index not found. Else : return the table part of the object dictionary.
 */
 const indextable * scanIndexOD (CO_Data* d, UNS16 wIndex, UNS32 *errorCode, ODCallback_t **Callback);

UNS32 RegisterSetODentryCallBack(CO_Data* d, UNS16 wIndex, UNS8 bSubindex, ODCallback_t Callback);

#ifdef __cplusplus
}
#endif

#endif /* __objacces_h__ */
