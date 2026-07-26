#ifndef XWA_FLIGHT_AI_PAIORDER_H
#define XWA_FLIGHT_AI_PAIORDER_H

#include "xwa/flight/ai/pai.h"

#ifdef __cplusplus
extern "C" {
#endif

char paiorder_nullhandler(void);
char paiorder_updatecourseorder(void);
void paiorder_selectorderslot(uint8_t orderSlot);
char paiorder_pickedupobjectorder(void);
char paiorder_checkhyperorder(void);
char paiorder_componentgoneorder(void);
char paiorder_mothershiporder(void);
char paiorder_flyhomeorder(void);
char paiorder_flyhomeotherregionorder(void);
char paiorder_enterhangarorder(void);
char paiorder_repaironeselforder(void);
char paiorder_playergoneorder(void);
char paiorder_ontailorder(void);
char paiorder_avoidstarshiporder(void);
char paiorder_underattackorder(void);
char paiorder_breakofforder(void);
char paiorder_alwaysorder(void);
char paiorder_avoidhitorder(void);
char paiorder_rocketsonboardorder(void);
char paiorder_lookforcrafttoboardorder(void);
char paiorder_awaitboardorder(void);
char paiorder_returnboardorder(void);
char paiorder_abortboardorder(void);
char paiorder_abortmissionorder(void);
char paiorder_makedisabledorder(void);
char paiorder_selfcaptureorder(void);
char paiorder_transfercargoorder(void);
char paiorder_lookforparkorder(void);
char paiorder_abortmotherwaitorder(void);
char paiorder_waitgootherorder(void);
char paiorder_stopgohomeorder(void);
char paiorder_completegohomeorder(void);
char paiorder_completegootherorder(void);
char paiorder_checkconditionalorder(void);
char paiorder_stillattackorder(void);
char paiorder_completefolloworder(void);
char paiorder_leadergohomeorder(void);
char paiorder_leaderdeadorder(void);
char paiorder_evasiveorder(void);
char paiorder_resumemissionorder(void);
char paiorder_waitrunorder(void);
char paiorder_hyperspaceorder(void);
char paiorder_killselforder(void);
char paiorder_waitforallcreateorder(void);
char paiorder_waitforallreturnorder(void);
char paiorder_checkdeliverorder(void);
char paiorder_dropoffdestorder(void);
char paiorder_checkreleaseorder(void);
char paiorder_orderswitchorder(void);
char paiorder_startoverorder(void);
char paiorder_changesidesorder(void);
char paiorder_disappearorder(void);
char paiorder_targetfromplayerorder(void);
char paiorder_commandfromplayerorder(void);
char paiorder_inspectedorder(void);
char paiorder_UpdateInspectionVisibility(unsigned int inspectorObjIdx, unsigned int targetObjIdx,
										 unsigned int range);

#ifdef __cplusplus
}
#endif

#endif
