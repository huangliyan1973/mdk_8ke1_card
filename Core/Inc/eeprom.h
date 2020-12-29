#ifndef INC_EEPROM_H_
#define INC_EEPROM_H_

#include "lwip/arch.h"

#define E1_CARDS      16
#define E1_PORT_PER_CARD	8
/*port type */
#define SS7_PORT      0
#define ISDN_PORT     1
/*CRC4 */
#define CRC4_DISABLE    0
#define CRC4_ENABLE     1
/* NO.1 PORT */
#define NO1_ENABLE      1
#define NO1_DISABLE     0

typedef struct {
	u8_t	e1_enable[E1_CARDS];
	u8_t	e1_l2_alarm_enable[E1_CARDS];
	u8_t	e1_port_type[E1_CARDS];
	u8_t	isdn_port_type[E1_CARDS];
	u8_t	pll_src[E1_CARDS];
	u8_t 	crc4_enable[E1_CARDS];
	u8_t 	no1_enable[E1_CARDS];
    u8_t 	mtp2_error_check[E1_CARDS];

	u8_t 	tone_cadence0[18];
	u8_t 	tone_cadence1[18];
	u8_t 	tone_cadence2[18];
	u8_t 	tone_cadence3[18];
	u8_t 	tone_cadence4[18];
	u8_t 	tone_cadence5[18];
	u8_t 	tone_cadence6[18];
	u8_t 	tone_cadence7[18];

	u8_t 	reason_to_tone[16];
	u8_t 	dtmf_mark_space[2];
	u8_t 	tone_src;
    
    struct {
        u8_t type;
        u8_t pc1[3];
        u8_t pc2[3];      
    }pc_magic[E1_PORT_PER_CARD];
    
    u32_t version;
} e1_params_t;

typedef struct {
	u8_t 	init_flag;
	u8_t 	led_status[8];
	u8_t 	e1_l1_alarm;
	u8_t 	e1_l2_alarm;
	u8_t 	cpu_loading;
	u8_t 	loopback_flag[8];
	u32_t 		timestamp;
} ram_params_t;

typedef struct {
	u8_t 	conf_grp;
	u8_t 	first_abcd;
	u8_t 	chk_times;
	u8_t 	ls_in;
	u8_t 	old_mfc_par;
	u8_t 	mfc_value;
} slot_t;

extern e1_params_t	e1_params;
extern ram_params_t ram_params;

extern void update_eeprom(void);
extern void reload_eeprom(void);
extern void init_eeprom(void);

#endif /* INC_EEPROM_H_ */
