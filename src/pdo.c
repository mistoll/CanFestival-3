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
#include "pdo.h"
#include "objacces.h"
#include "canfestival.h"
#include "sysdep.h"

/*!
** @file   pdo.c
** @author Edouard TISSERANT and Francis DUPIN
** @date   Tue Jun  5 09:32:32 2007
**
** @brief
**
**
*/
static inline void copyBits (UNS8 NbBits, const UNS8 * SrcByteIndex, UNS8 SrcBitIndex, UNS8 * DestByteIndex, UNS8 DestBitIndex);



/** 
 * @brief Copy all the data to transmit in process_var
 * Prepare the PDO defined at index to be sent
 * *pwCobId : returns the value of the cobid. (subindex 1)
 * @param *d Pointer on a CAN object data structure
 * @param numPdo The PDO number
 * @param *pdo Pointer on a CAN message structure
 * @return 0 or 0xFF if error.
 */
static UNS8 buildPDO(CO_Data* d, UNS8 numPdo, Message *pdo)
{
	const subindex *TPDO_com = (d->objdict + d->firstIndex->PDO_TRS + numPdo)->pSubindex;
	const subindex *TPDO_map = (d->objdict + d->firstIndex->PDO_TRS_MAP + numPdo)->pSubindex;

	UNS8 prp_j = 0x00;
	UNS8 offset = 0x00;
	UNS8 mappingCount = *(UNS8 *) TPDO_map[0].pObject;

	pdo->cob_id = (UNS16) UNS16_LE(*(UNS32*)TPDO_com[1].pObject & 0x7FF);
	pdo->rtr = NOT_A_REQUEST;

	MSG_WAR (0x3009, "  PDO CobId is : ", *(UNS32 *) TPDO_com[1].pObject);
	MSG_WAR (0x300D, "  Number of objects mapped : ", mappingCount);

	do
	{
		/* pointer fo the var which holds the mapping parameter of an mapping entry  */
		UNS32 mappingParameter = *(UNS32 *) TPDO_map[prp_j + 1].pObject;
		UNS16 index = (UNS16) ((mappingParameter) >> 16);
		
		// UNS32 because obdict size is 32-bit (standard)
		UNS8 Size = (UNS8) (mappingParameter & 0x000000FF);     /* Size in bits */

		/* get variable only if Size != 0 and Size is lower than remaining bits in the PDO */
		if (Size && ((offset + Size) <= 64))
		{
			UNS8 ByteSize = 1 + ((Size - 1) >> 3);        /*1->8 => 1 ; 9->16 => 2, ... */
			UNS8 subIndex =	(UNS8) (((mappingParameter) >> 8) & 0x000000FF);

			MSG_WAR (0x300F, "  got mapping parameter : ", mappingParameter);
			MSG_WAR (0x3051, "    sub-index : ", prp_j + 1);
			
			const subindex* it = NULL;
			if (_findODentry(d, index, subIndex, &it) != OD_SUCCESSFUL)
			//if (getODentry (d, index, subIndex, tmp, &ByteSize, &dataType, 0) != OD_SUCCESSFUL)
			{
				MSG_ERR (0x1013, " Couldn't find mapped variable at index-subindex-size : ", (UNS32) (mappingParameter));
				return 0xFF;
			}
			if (it->size > (UNS32)ByteSize) {
				MSG_ERR (0x1013, " Size of entry is to large at index-subindex-size : ", (UNS32) (mappingParameter));
				return 0xFF;
			}
			
			copyBits ((UNS8) Size, ((UNS8 *) it->pObject), 0, (UNS8 *) & pdo->data[offset >> 3], (UNS8)(offset % 8));
			offset += Size;
		}
		prp_j++;
	}
	while (prp_j < mappingCount);

	pdo->len = (UNS8)(1 + ((offset - 1) >> 3));

	MSG_WAR (0x3015, "  End scan mapped variable", 0);

	return 0;
}

/*!
**
**
** @param d
** @param cobId
**
** @return
**/
UNS8
sendPDOrequest (CO_Data * d, UNS16 RPDOIndex)
{
  UNS16 *pwCobId;
  UNS16 offset = d->firstIndex->PDO_RCV;
  UNS16 lastIndex = d->lastIndex->PDO_RCV;

  if (!d->CurrentCommunicationState.csPDO)
    {
      return 0;
    }

  /* Sending the request only if the cobid have been found on the PDO
     receive */
  /* part dictionary */

  MSG_WAR (0x3930, "sendPDOrequest RPDO Index : ", RPDOIndex);

  if (offset && RPDOIndex >= 0x1400)
    {
      offset += RPDOIndex - 0x1400;
      if (offset <= lastIndex)
        {
          /* get the CobId */
          pwCobId = d->objdict[offset].pSubindex[1].pObject;

          MSG_WAR (0x3930, "sendPDOrequest cobId is : ", *pwCobId);
          {
            Message pdo;
            pdo.cob_id = UNS16_LE(*pwCobId);
            pdo.rtr = REQUEST;
            pdo.len = 0;
            return canSend (d->canHandle, &pdo);
          }
        }
    }
  MSG_ERR (0x1931, "sendPDOrequest : RPDO Index not found : ", RPDOIndex);
  return 0xFF;
}


/*!
**
**
** @param d
** @param m
**
** @return
**/
UNS8
proceedPDO (CO_Data * d, Message * m)
{
  UNS8 numPdo;
  UNS8 numMap;                  /* Number of the mapped varable */
  UNS8 *pMappingCount = NULL;   /* count of mapped objects... */
  /* pointer to the var which is mapped to a pdo... */
  /*  void *     pMappedAppObject = NULL;   */
  /* pointer fo the var which holds the mapping parameter of an
     mapping entry */
  UNS32 *pMappingParameter = NULL;
  UNS8 *pTransmissionType = NULL;       /* pointer to the transmission
                                           type */
  UNS16 *pwCobId = NULL;
  UNS8 Size;
  UNS8 offset;
  UNS8 status;
  UNS32 objDict;
  UNS16 offsetObjdict;
  UNS16 lastIndex;

  status = state2;

  MSG_WAR (0x3935, "proceedPDO, cobID : ", (UNS16_LE(m->cob_id) & 0x7ff));
  offset = 0x00;
  numPdo = 0;
  numMap = 0;
  if ((*m).rtr == NOT_A_REQUEST)
    { 
      offsetObjdict = d->firstIndex->PDO_RCV;
      lastIndex = d->lastIndex->PDO_RCV;

      if (offsetObjdict)
        while (offsetObjdict <= lastIndex)
          {
            switch (status)
              {

              case state2:
                pwCobId = d->objdict[offsetObjdict].pSubindex[1].pObject;
                if (*pwCobId == UNS16_LE(m->cob_id))
                  {
                    /* The cobId is recognized */
                    status = state4;
                    MSG_WAR (0x3936, "cobId found at index ",
                             0x1400 + numPdo);
                    break;
                  }
                else
                  {
                    /* received cobId does not match */
                    numPdo++;
                    offsetObjdict++;
                    status = state2;
                    break;
                  }

              case state4:     /* Get Mapped Objects Number */
                /* The cobId of the message received has been found in the
                   dictionnary. */
                offsetObjdict = d->firstIndex->PDO_RCV_MAP;
                lastIndex = d->lastIndex->PDO_RCV_MAP;
                pMappingCount =
                  (UNS8 *) (d->objdict + offsetObjdict +
                            numPdo)->pSubindex[0].pObject;
                numMap = 0;
                while (numMap < *pMappingCount)
                  {
                    UNS8 tmp[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
                    UNS32 ByteSize;
                    pMappingParameter =
                      (UNS32 *) (d->objdict + offsetObjdict +
                                 numPdo)->pSubindex[numMap + 1].pObject;
                    if (pMappingParameter == NULL)
                      {
                        MSG_ERR (0x1937, "Couldn't get mapping parameter : ",
                                 numMap + 1);
                        return 0xFF;
                      }
                    /* Get the addresse of the mapped variable. */
                    /* detail of *pMappingParameter : */
                    /* The 16 hight bits contains the index, the medium 8 bits
                       contains the subindex, */
                    /* and the lower 8 bits contains the size of the mapped
                       variable. */

                    Size = (UNS8) (*pMappingParameter & (UNS32) 0x000000FF);

                    /* set variable only if Size != 0 and 
                     * Size is lower than remaining bits in the PDO */
                    if (Size && ((offset + Size) <= (m->len << 3)))
                      {
                        /* copy bit per bit in little endian */
                        copyBits (Size, (UNS8 *) & m->data[offset >> 3], offset % 8, ((UNS8 *) tmp), 0);
                        /*1->8 => 1 ; 9->16 =>2, ... */
                        ByteSize = (UNS32)(1 + ((Size - 1) >> 3));

                        objDict =
                          setODentry (d, (UNS16) ((*pMappingParameter) >> 16),
                                      (UNS8) (((*pMappingParameter) >> 8) &
                                              0xFF), tmp, &ByteSize, 0);

                        if (objDict != OD_SUCCESSFUL)
                          {
                            MSG_ERR (0x1938,
                                     "error accessing to the mapped var : ",
                                     numMap + 1);
                            MSG_WAR (0x2939, "         Mapped at index : ",
                                     (*pMappingParameter) >> 16);
                            MSG_WAR (0x2940, "                subindex : ",
                                     ((*pMappingParameter) >> 8) & 0xFF);
                            return 0xFF;
                          }

                        MSG_WAR (0x3942,
                                 "Variable updated by PDO cobid : ",
                                 UNS16_LE(m->cob_id));
                        MSG_WAR (0x3943, "         Mapped at index : ",
                                 (*pMappingParameter) >> 16);
                        MSG_WAR (0x3944, "                subindex : ",
                                 ((*pMappingParameter) >> 8) & 0xFF);
                        offset += Size;
                      }
                    numMap++;
                  }             /* end loop while on mapped variables */
                if (d->RxPDO_EventTimers)
                {
                    TIMEVAL EventTimerDuration = *(UNS16 *)d->objdict[offsetObjdict].pSubindex[5].pObject;
                    if(EventTimerDuration){
                        DelAlarm (d->RxPDO_EventTimers[numPdo]);
                        d->RxPDO_EventTimers[numPdo] = SetAlarm (d, numPdo, d->RxPDO_EventTimers_Handler,
                        MS_TO_TIMEVAL (EventTimerDuration), 0);
                    }
                }
                return 0;

              }                 /* end switch status */
          }                     /* end while */
    }                           /* end if Donnees */
  else if ((*m).rtr == REQUEST)
    {
      MSG_WAR (0x3946, "Receive a PDO request cobId : ", UNS16_LE(m->cob_id));
      status = state1;
      offsetObjdict = d->firstIndex->PDO_TRS;
      lastIndex = d->lastIndex->PDO_TRS;
      if (offsetObjdict)
        while (offsetObjdict <= lastIndex)
          {
            /* study of all PDO stored in the objects dictionary */

            switch (status)
              {

              case state1:     /* check the CobId */
                /* get CobId of the dictionary which match to the received PDO
                 */
                pwCobId =
                   (d->objdict +
                             offsetObjdict)->pSubindex[1].pObject;
                if (*pwCobId == UNS16_LE(m->cob_id))
                  {
                    status = state4;
                    break;
                  }
                else
                  {
                    numPdo++;
                    offsetObjdict++;
                  }
                status = state1;
                break;


              case state4:     /* check transmission type */
                pTransmissionType =
                  (UNS8 *) d->objdict[offsetObjdict].pSubindex[2].pObject;
                /* If PDO is to be sampled and send on RTR, do it */
                if ((*pTransmissionType == TRANS_RTR))
                  {
                    status = state5;
                    break;
                  }
                /* RTR_SYNC means data prepared at SYNC, transmitted on RTR */
                else if ((*pTransmissionType == TRANS_RTR_SYNC))
                  {
                    if (d->PDO_status[numPdo].
                        transmit_type_parameter & PDO_RTR_SYNC_READY)
                      {
                        /*Data ready, just send */
                        canSend (d->canHandle,
                                 &d->PDO_status[numPdo].last_message);
                        return 0;
                      }
                    else
                      {
                        /* if SYNC did never occur, transmit current data */
                        /* DS301 do not tell what to do in such a case... */
                        MSG_ERR (0x1947,
                                 "Not ready RTR_SYNC TPDO send current data : ",
                                 UNS16_LE(m->cob_id));
                        status = state5;
                      }
                    break;
                  }
                else if ((*pTransmissionType == TRANS_EVENT_PROFILE) ||
                         (*pTransmissionType == TRANS_EVENT_SPECIFIC))
                  {
                    /* Zap all timers and inhibit flag */
                    d->PDO_status[numPdo].event_timer =
                      DelAlarm (d->PDO_status[numPdo].event_timer);
                    d->PDO_status[numPdo].inhibit_timer =
                      DelAlarm (d->PDO_status[numPdo].inhibit_timer);
                    d->PDO_status[numPdo].transmit_type_parameter &=
                      ~PDO_INHIBITED;
                    /* Call  PDOEventTimerAlarm for this TPDO, 
                     * this will trigger emission et reset timers */
                    PDOEventTimerAlarm (d, numPdo);
                    return 0;
                  }
                else
                  {
                    /* The requested PDO is not to send on request. So, does
                       nothing. */
                    MSG_WAR (0x2947, "PDO is not to send on request : ",
                             UNS16_LE(m->cob_id));
                    return 0xFF;
                  }

              case state5:     /* build and send requested PDO */
                {
                  Message pdo;
                  if (buildPDO (d, numPdo, &pdo))
                    {
                      MSG_ERR (0x1948, " Couldn't build TPDO number : ", numPdo);
                      return 0xFF;
                    }
                  canSend (d->canHandle, &pdo);
                  return 0;
                }
              }                 /* end switch status */
          }                     /* end while */
    }                           /* end if Requete */

  return 0;
}

/*!
**
**
** @param NbBits
** @param SrcByteIndex
** @param SrcBitIndex
** @param DestByteIndex
** @param DestBitIndex
**/
static inline void copyBits (UNS8 NbBits, const UNS8 * SrcByteIndex, UNS8 SrcBitIndex,
          UNS8 * DestByteIndex, UNS8 DestBitIndex)
{
	// Use memcpy for aligned copy (Optimization for speed)
	if (SrcBitIndex == 0 && DestBitIndex == 0 && (NbBits & 0x7) == 0) {
		memcpy(DestByteIndex, SrcByteIndex, NbBits >> 3);
		return;
	}
	
	/* This loop copy as many bits that it can each time, crossing */
	/* successively bytes */
	// boundaries from LSB to MSB.
	while (NbBits > 0)
	{
		/* Bit missalignement between src and dest */
		INTEGER8 Vect = DestBitIndex - SrcBitIndex;

		/* We can now get src and align it to dest */
		UNS8 Aligned = Vect > 0 ? *SrcByteIndex << Vect : *SrcByteIndex >> -Vect;

		/* Compute the nb of bit we will be able to copy */
		UNS8 BoudaryLimit = (Vect > 0 ? 8 - DestBitIndex : 8 - SrcBitIndex);
		UNS8 BitsToCopy = BoudaryLimit > NbBits ? NbBits : BoudaryLimit;

		/* Create a mask that will serve in: */
		UNS8 Mask =
			((0xff << (DestBitIndex + BitsToCopy)) |
			(0xff >> (8 - DestBitIndex)));

		/* - Filtering src */
		UNS8 Filtered = Aligned & ~Mask;

		/* - and erase bits where we write, preserve where we don't */
		*DestByteIndex &= Mask;

		/* Then write. */
		*DestByteIndex |= Filtered;

		/*Compute next time cursors for src */
		if ((SrcBitIndex += BitsToCopy) > 7)      /* cross boundary ? */
		{
			SrcBitIndex = 0;	/* First bit */
			SrcByteIndex += 1;	/* Next byte */
		}


		/*Compute next time cursors for dest */
		if ((DestBitIndex += BitsToCopy) > 7)
		{
			DestBitIndex = 0;	/* First bit */
			DestByteIndex += 1;	/* Next byte */
		}

		/*And decrement counter. */
		NbBits -= BitsToCopy;
	}
}

static inline uint8_t sendPdo(CO_Data * d, UNS32 pdoNum, Message * pdo)
{
	MSG_WAR (0x396D, "sendPDO cobId :", UNS16_LE(pdo->cob_id));
	MSG_WAR (0x396E, "     Nb octets  : ", pdo->len);

	if (canSend (d->canHandle, pdo)) {
		/*store_as_last_message */
		d->PDO_status[pdoNum].last_message = *pdo;
		return 1;
	} 
	return 0;
}


UNS8
sendOnePDOevent (CO_Data * d, UNS8 pdoNum)
{
	Message pdo;
	const Message* last_message;
	s_PDO_status* PDO_status = &d->PDO_status[pdoNum];
	if (!d->CurrentCommunicationState.csPDO ||
		(PDO_status->transmit_type_parameter & PDO_INHIBITED))
	{
		return 0;
	}
  
	if (buildPDO (d, pdoNum, &pdo))
	{
		MSG_ERR (0x3907, " Couldn't build TPDO number : ",
				pdoNum);
		return 0;
	}
	
	/*Compare new and old PDO */
	last_message = &PDO_status->last_message;
	if (last_message->cob_id == pdo.cob_id
		&& last_message->len == pdo.len
		&& memcmp(last_message->data,
					pdo.data, pdo.len) == 0)
	{
		/* No changes -> go to next pdo */
		return 0;
	}
	else
	{
		UNS16 offsetObjdict;
		const indextable* objentry;
		TIMEVAL EventTimerDuration;
		TIMEVAL InhibitTimerDuration;

		MSG_WAR (0x306A, "Changes TPDO number : ", pdoNum);
	
		offsetObjdict = (UNS16) (d->firstIndex->PDO_TRS + pdoNum);
		objentry = &d->objdict[offsetObjdict];
		/* Changes detected -> transmit message */
		EventTimerDuration = *(UNS16 *) objentry->pSubindex[5].pObject;
		InhibitTimerDuration = *(UNS16 *) objentry->pSubindex[3].pObject;

		/* Start both event_timer and inhibit_timer */
		if (EventTimerDuration)
		{
			DelAlarm (PDO_status->event_timer);
			PDO_status->event_timer = SetAlarm (d, pdoNum, &PDOEventTimerAlarm,
						MS_TO_TIMEVAL (EventTimerDuration), 0);
		}

		if (InhibitTimerDuration)
		{
			DelAlarm (PDO_status->inhibit_timer);
			PDO_status->inhibit_timer =	SetAlarm (d, pdoNum, &PDOInhibitTimerAlarm,
						US_TO_TIMEVAL (InhibitTimerDuration * 100), 0);
			/* and inhibit TPDO */
			PDO_status->transmit_type_parameter |= PDO_INHIBITED;
		}

		if (!sendPdo(d, pdoNum, &pdo))
			return 0;
	}
	return 1;
}

void
PDOEventTimerAlarm (CO_Data * d, UNS32 pdoNum)
{
  /* This is needed to avoid deletion of re-attribuated timer */
  d->PDO_status[pdoNum].event_timer = TIMER_NONE;
  /* force emission of PDO by artificially changing last emitted */
  d->PDO_status[pdoNum].last_message.cob_id = 0;
  sendOnePDOevent (d, (UNS8) pdoNum);
}

void
PDOInhibitTimerAlarm (CO_Data * d, UNS32 pdoNum)
{
  /* This is needed to avoid deletion of re-attribuated timer */
  d->PDO_status[pdoNum].inhibit_timer = TIMER_NONE;
  /* Remove inhibit flag */
  d->PDO_status[pdoNum].transmit_type_parameter &= ~PDO_INHIBITED;
  sendOnePDOevent (d, (UNS8) pdoNum);
}

void
_RxPDO_EventTimers_Handler(CO_Data *d, UNS32 pdoNum)
{
}

/*!
**
**
** @param d
** @param isSyncEvent
**
** @return
**/

UNS8 _sendSyncPDOevent (CO_Data * d)
{
	UNS8 pdoNum = 0x00;           /* number of the actual processed pdo-nr. */
	UNS8 *pTransmissionType = NULL;
	UNS16 offsetObjdict = d->firstIndex->PDO_TRS;
	UNS16 offsetObjdictMap = d->firstIndex->PDO_TRS_MAP;
	UNS16 lastIndex = d->lastIndex->PDO_TRS;

	if (!d->CurrentCommunicationState.csPDO)
	{
		return 0;
	}


	/* study all PDO stored in the objects dictionary */
	if (offsetObjdict)
	{
		Message pdo;/* = Message_Initializer;*/
		
		for (;offsetObjdict <= lastIndex; pdoNum++, offsetObjdict++, offsetObjdictMap++)
		{
			
			if ( /* bSubCount always 5 with objdictedit -> check disabled */
			/*d->objdict[offsetObjdict].bSubCount < 5 ||*/
			/* check if TPDO is not valid */
			*(UNS32 *) d->objdict[offsetObjdict].pSubindex[1].
			pObject & 0x80000000)
			{
				MSG_WAR (0x3960, "Not a valid PDO ", 0x1800 + pdoNum);
				/*Go next TPDO */
				continue;;
			}
			/* get the PDO transmission type */
			pTransmissionType =
			(UNS8 *) d->objdict[offsetObjdict].pSubindex[2].pObject;
			MSG_WAR (0x3962, "Reading PDO at index : ", 0x1800 + pdoNum);

			/* check if transmission type is SYNCRONOUS */
			/* message transmited every n SYNC with n=TransmissionType */
			if ((*pTransmissionType >= TRANS_SYNC_MIN) &&
			(*pTransmissionType <= TRANS_SYNC_MAX) &&
			(++d->PDO_status[pdoNum].transmit_type_parameter ==
			*pTransmissionType))
			{
				/*Reset count of SYNC */
				d->PDO_status[pdoNum].transmit_type_parameter = 0;
				MSG_WAR (0x3964, "  PDO is on SYNCHRO. Trans type : ",
				*pTransmissionType);
				memset(&pdo, 0, sizeof(pdo));
				/*{
				Message msg_init = Message_Initializer;
				pdo = msg_init;
				}*/
				if (buildPDO (d, pdoNum, &pdo))
				{
					MSG_ERR (0x1906, " Couldn't build TPDO number : ",
					pdoNum);
					continue;
				}
				sendPdo(d, pdoNum, &pdo);
				
				/* If transmission RTR, with data sampled on SYNC */
			}
			else if ((*pTransmissionType == TRANS_RTR_SYNC))
			{
				if (buildPDO
				(d, pdoNum, &d->PDO_status[pdoNum].last_message))
				{
					MSG_ERR (0x1966, " Couldn't build TPDO number : ",
					pdoNum);
					d->PDO_status[pdoNum].transmit_type_parameter &=
					~PDO_RTR_SYNC_READY;
				}
				else
				{
					d->PDO_status[pdoNum].transmit_type_parameter |=
					PDO_RTR_SYNC_READY;
				}
				/* If transmission on Event and not inhibited, check for changes */
			}
			else
			if ((*pTransmissionType == TRANS_SYNC_ACYCLIC))
			{
				sendOnePDOevent(d, pdoNum);
			}
			else
			{
				MSG_WAR (0x306C,
				"  PDO is not on EVENT or synchro or not at this SYNC. Trans type : ",
				*pTransmissionType);
			}
	
		}                       /* end for */
	}
	return 0;
}

UNS8 sendAsyncPDOevent (CO_Data * d)
{
	UNS8 pdoNum = 0x00;           /* number of the actual processed pdo-nr. */
	UNS8 transmissionType = NULL;
	UNS16 offsetObjdict = d->firstIndex->PDO_TRS;
	UNS16 offsetObjdictMap = d->firstIndex->PDO_TRS_MAP;
	UNS16 lastIndex = d->lastIndex->PDO_TRS;

	if (!d->CurrentCommunicationState.csPDO)
	{
		return 0;
	}


	/* study all PDO stored in the objects dictionary */
	if (offsetObjdict)
	{
		for (;offsetObjdict <= lastIndex; pdoNum++, offsetObjdict++, offsetObjdictMap++)
		{
			const subindex* pdoSub = d->objdict[offsetObjdict].pSubindex;
			if ( /* bSubCount always 5 with objdictedit -> check disabled */
			/*d->objdict[offsetObjdict].bSubCount < 5 ||*/
			/* check if TPDO is not valid */
				(*(UNS32 *) pdoSub[1].pObject) & 0x80000000)
			{
				MSG_WAR (0x3960, "Not a valid PDO ", 0x1800 + pdoNum);
				/*Go next TPDO */
				continue;;
			}
			/* get the PDO transmission type */
			transmissionType = *(UNS8 *)pdoSub[2].pObject;
			MSG_WAR (0x3962, "Reading PDO at index : ", 0x1800 + pdoNum);

			/* check if transmission type is SYNCRONOUS */
			/* message transmited every n SYNC with n=TransmissionType */
			if ((transmissionType == TRANS_EVENT_PROFILE || transmissionType == TRANS_EVENT_SPECIFIC)
				&& !(d->PDO_status[pdoNum].transmit_type_parameter & PDO_INHIBITED))
			{
				sendOnePDOevent(d, pdoNum);
			}
			else
			{
				MSG_WAR (0x306C,
				"  PDO is not on EVENT or synchro or not at this SYNC. Trans type : ",
				*pTransmissionType);
			}
	
		}                       /* end for */
	}
	return 0;
}

/*!
**
**
** @param d
** @param OD_entry
** @param bSubindex
** @return always 0
**/

static UNS32
TPDO_Communication_Parameter_Callback (CO_Data * d,
                                       const indextable * OD_entry,
                                       UNS8 bSubindex)
{
  /* If PDO are actives */
  if (d->CurrentCommunicationState.csPDO)
    switch (bSubindex)
      {
      case 2:                  /* Changed transmition type */
      case 3:                  /* Changed inhibit time */
      case 5:                  /* Changed event time */
        {
          const indextable *TPDO_com = d->objdict + d->firstIndex->PDO_TRS;
          UNS8 numPdo = (UNS8) (OD_entry - TPDO_com);    /* number of the actual processed pdo-nr. */

          /* Zap all timers and inhibit flag */
          d->PDO_status[numPdo].event_timer =
            DelAlarm (d->PDO_status[numPdo].event_timer);
          d->PDO_status[numPdo].inhibit_timer =
            DelAlarm (d->PDO_status[numPdo].inhibit_timer);
          d->PDO_status[numPdo].transmit_type_parameter = 0;
          /* Call  PDOEventTimerAlarm for this TPDO, this will trigger emission et reset timers */
          PDOEventTimerAlarm (d, numPdo);
          return 0;
        }

      default:                 /* other subindex are ignored */
        break;
      }
  return 0;
}

void
PDOInit (CO_Data * d)
{
  /* For each TPDO mapping parameters */
  UNS16 pdoIndex = 0x1800;      /* OD index of TDPO */

  UNS16 offsetObjdict = d->firstIndex->PDO_TRS;
  UNS16 lastIndex = d->lastIndex->PDO_TRS;
  if (offsetObjdict)
    while (offsetObjdict <= lastIndex)
      {
        /* Assign callbacks to sensible TPDO mapping subindexes */
        UNS32 errorCode;
        ODCallback_t *CallbackList;
        /* Find callback list */
        scanIndexOD (d, pdoIndex, &errorCode, &CallbackList);
        if (errorCode == OD_SUCCESSFUL && CallbackList)
          {
            /*Assign callbacks to corresponding subindex */
            /* Transmission type */
            CallbackList[2] = &TPDO_Communication_Parameter_Callback;
            /* Inhibit time */
            CallbackList[3] = &TPDO_Communication_Parameter_Callback;
            /* Event timer */
            CallbackList[5] = &TPDO_Communication_Parameter_Callback;
          }
        pdoIndex++;
        offsetObjdict++;
      }

  /* Trigger a non-sync event */
  sendAsyncPDOevent (d);
}

void
PDOStop (CO_Data * d)
{
  /* For each TPDO mapping parameters */
  UNS8 pdoNum = 0x00;           /* number of the actual processed pdo-nr. */
  UNS16 offsetObjdict = d->firstIndex->PDO_TRS;
  UNS16 lastIndex = d->lastIndex->PDO_TRS;
  if (offsetObjdict)
    while (offsetObjdict <= lastIndex)
      {
        /* Delete TPDO timers */
        d->PDO_status[pdoNum].event_timer =
          DelAlarm (d->PDO_status[pdoNum].event_timer);
        d->PDO_status[pdoNum].inhibit_timer =
          DelAlarm (d->PDO_status[pdoNum].inhibit_timer);
        /* Reset transmit type parameter */
        d->PDO_status[pdoNum].transmit_type_parameter = 0;
        d->PDO_status[pdoNum].last_message.cob_id = 0;
        pdoNum++;
        offsetObjdict++;
      }
}
