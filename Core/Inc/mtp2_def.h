#ifndef MTP2_DEF_IF_H_
#define MTP2_DEF_IF_H_

#define MTP2_STATE_IDLE							0
#define MTP2_STATE_STOP							1
#define MTP2_STATE_ASSIGN						2
#define MTP2_STATE_READY						3
#define MTP2_STATE_NOT_READY					4
#define MTP2_STATE_WORKING						5
#define MTP2_STATE_ERROR						6
#define MTP2_STATE_WAITING						7
#define MTP2_STATE_STATE_CONGEST				8

#define LED_OUTOF_SERVICE			   		 0
#define LED_ALIGNMENT				   		 1
#define	LED_CONGEST					   		 2
#define LED_IN_SERVICE				   		 3

#define ALARM_IN_SERVICE			   		 0
#define ALARM_CONGEST				   		 1
#define ALARM_SDBUF_FULL			   		 2
#define ALARM_CANT_RETRIEVE_BSNT	   		 3
#define ALARM_CANT_RETRIEVE_FSNC	   		 4

#define LKALARM_EXECACK				   		 5
#define LKALARM_RCONGEST			   		 6
#define LKALARM_EXECERROR			   		 7
#define LKALARM_WORDDELAY			   		 8
#define LKALARM_RTBFULL				   		 9
#define LKALARM_WORKING				   		 10
#define LKALARM_WORKING2			   		 11
#define LKALARM_POC					   		 12
#define LKALARM_CONGEST_ST			   		 13
#define LKALARM_CONGEST_ST2			   		 14
#define LKALARM_L3					   		 15
#define LKALARM_RV_LSSU				   		 16


#endif
