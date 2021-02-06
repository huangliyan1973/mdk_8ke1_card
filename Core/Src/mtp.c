#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "ds26518.h"
#include "mtp.h"
#include "sram.h"
#include "eeprom.h"
#include "server_interface.h"
#include "sched.h"

#define LOG_TAG              "mtp2"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

#define MTP_NEXT_SEQ(x) (((x) + 1) % 128)

#define T2_TIMEOUT      75000
#define T1_TIMEOUT      45000
#define T3_TIMEOUT      1500
#define T4_TIMEOUT      500
#define T7_TIMEOUT      1500
#define T17_TIMEOUT     1200

#define MTP2_POLL_ENABLE    

extern uint8_t card_id;

//static mtp2_t mtp2_state[E1_LINKS_MAX] __attribute__((at(SRAM_BASE_ADDR)));
static mtp2_t mtp2_state[1];

#if 0
static sys_mbox_t mtp_mbox;
sys_mutex_t lock_mtp_core;
#endif

static int sin_scount = 0;
static int sin_rcount = 0;
static int fisu_scount = 0;
static int fisu_rcount = 0;

static void abort_initial_alignment(mtp2_t *m);
static void start_initial_alignment(mtp2_t *m, char* reason);

static void mtp3_link_fail(mtp2_t *m, int down);

/* Q.703 timer T1 "alignment ready" (waiting for peer to end initial
	   alignment after we are done). */
static void mtp2_t1_timeout(void *arg)
{
    mtp2_t *m = (mtp2_t *)arg;
    LOG_W("MTP2 timer T1 timeout (peer failed to complete initial alignment)"
                    ", initial alignment failed on link '%d'", m->e1_no);

    abort_initial_alignment(m);
}

/* Q.703 timer T2 "not aligned" (waiting to receive O, E, or N after sending
	   O). */
static void mtp2_t2_timeout(void *arg)
{
    mtp2_t *m = (mtp2_t *)arg;
    LOG_W("MTP2 timer T2 timeout (failed to receive 'O', 'N', "
             "or 'E' after sending 'O'), initial alignment failed on link '%d'", m->e1_no);

    abort_initial_alignment(m);
}

/* Q.703 timer T3 "aligned" (waiting to receive E or N after sending E or
	   N). */
static void mtp2_t3_timeout(void *arg)
{
    mtp2_t *m = (mtp2_t *)arg;
    LOG_W("MTP2 timer T3 timeout (failed to receive 'N', "
             "or 'E' after sending 'O'), initial alignment failed on link '%d'", m->e1_no);

    abort_initial_alignment(m);
}

static unsigned char sltm_pattern[15] = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};

static void mtp2_send_sltm(mtp2_t *m)
{
    u8_t message_sltm[24];

    int dpc = 1;
    int opc = 0xe;

    message_sltm[0] = dpc & 0xff;
    message_sltm[1] = ((dpc & 0x3f00) >> 8) | ((opc & 0x0003) << 6);
    message_sltm[2] = ((opc & 0x03fc) >> 2);
    message_sltm[3] = ((opc & 0x3c00) >> 10);

    message_sltm[4] = 0x11;       /* SLTM */
    message_sltm[5] = 0xf0;       /* Length: 15 */
    memcpy(&(message_sltm[6]), sltm_pattern, sizeof(sltm_pattern));
    mtp2_queue_msu(m->e1_no, 0x81, message_sltm, 6 + sizeof(sltm_pattern));
}

static void test_5s_timeout(void *arg)
{
    mtp2_t *m = (mtp2_t *)arg;
    LOG_I("send sltm message for test!, S_SIN=%d, R_SIN=%d, S_FISU=%d, R_FISU=%d",
        sin_scount, sin_rcount, fisu_scount, fisu_rcount);
    mtp2_send_sltm(m);

    //sched_timeout(50000, test_5s_timeout, m);
}

/* Q.703 timer T4 "proving period" - proving time before ending own initial
	   alignment. */
static void mtp2_t4_timeout(void *arg)
{
    mtp2_t *m = (mtp2_t *)arg;
    LOG_I("Proving successful on link '%d'", m->e1_no);

    m->state = MTP2_READY;
    sched_timeout(T1_TIMEOUT, mtp2_t1_timeout, m);

    sched_timeout(50000, test_5s_timeout, m);
}

/* Q.703 timer T7 "excessive delay of acknowledgement" . */
static void mtp2_t7_timeout(void *arg)
{
    mtp2_t *m = (mtp2_t *)arg;
    LOG_W("T7 timeout (excessive delay of acknowledgement) on link '%d', state = %d", m->e1_no, m->state);

    mtp3_link_fail(m, 1);
}

/* Q.704 timer T17, "initial alignment restart delay". */
static void mtp2_t17_timeout(void *arg)
{
    mtp2_t *m = (mtp2_t *)arg;
    LOG_D("T17 timeout on link '%d'", m->e1_no);

    sin_scount = 0;
    sin_rcount = 0;
    start_initial_alignment(m, "t17_timeout");
}

static void mtp2_cleanup(mtp2_t *m)
{
    sched_untimeout(mtp2_t1_timeout, m);
    sched_untimeout(mtp2_t2_timeout, m);
    sched_untimeout(mtp2_t3_timeout, m);
    sched_untimeout(mtp2_t4_timeout, m);
    sched_untimeout(mtp2_t7_timeout, m);
    sched_untimeout(mtp2_t17_timeout, m);

    LOG_D("mtp2 cleanup on link '%d'", m->e1_no);
}

/* Called on link errors that occur during initial alignment (before the link
   is in service), and which should cause initial alignment to be aborted. The
   initial alignment to be re-tried after a short delay (MTP3 T17). */
static void abort_initial_alignment(mtp2_t *m)
{
    mtp2_cleanup(m);
    m->state = MTP2_DOWN;
    /* Retry the initial alignment after a small delay. */
    sched_timeout(T17_TIMEOUT, mtp2_t17_timeout, m);
    LOG_D("Aborted initial alignment on link '%d'", m->e1_no);
}

/* Called on link errors that occur after the link is brought into service and
   which must cause the link to be brought out of service. This entails
   notifying user-parts of the failure and initiating MTP3 link failover, when
   that is implemented. */
static void mtp3_link_fail(mtp2_t *m, int down)
{
    //struct mtp_event link_up_event;
    //int old_state = m->state;

    mtp2_cleanup(m);

    /* Notify user-parts. */
    /****
    if(old_state == MTP2_INSERVICE) {
        memset(&link_up_event, 0, sizeof(link_up_event));
        link_up_event.typ = MTP_EVENT_STATUS;
        link_up_event.status.link_state = MTP_EVENT_STATUS_LINK_DOWN;
        link_up_event.status.link = m->link;
        link_up_event.len = 0;
        mtp_put(m, &link_up_event);
        mtp_changeover(m);
    }
	**/

    /* For now, restart initial alignment after a small delay. */
    if (down) {
        m->state = MTP2_DOWN;
        sched_timeout(T17_TIMEOUT, mtp2_t17_timeout, m);
    } else
        m->state = MTP2_NOT_ALIGNED;

    //l4down(m);
    LOG_W("Fail on link '%d'.", m->e1_no);
}

static void start_initial_alignment(mtp2_t *m, char* reason)
{
    m->state = MTP2_NOT_ALIGNED;
    m->send_fib = 1;
    m->send_bsn = 0x7f;
    m->send_bib = 1;
    m->tx_len = 0;
    
    m->retrans_seq = -1;
    m->retrans_last_acked = 0x7f;
    m->retrans_last_sent = 0x7f;
    
    m->bsn_errors = 0;

    LOG_D("Starting initial alignment on link '%d', reason: %s.", m->e1_no, reason);
    sched_timeout(T2_TIMEOUT, mtp2_t2_timeout, m);
}

/* Find a frame to transmit and put it in the transmit buffer.

   Q.703 (11.2.2): Pick a frame in descending priority as
   1. Link status signal unit.
   2. Requested retransmission of message signal unit.
   3. New message signal unit.
   4. Fill-in signal unit.
   5. Flag [but we can always send fill-in signal unit].
*/
static void
mtp2_pick_frame(mtp2_t *m)
{
    switch(m->state) {
    case MTP2_DOWN:
        /* Send SIOS. */
        m->tx_len = 4;
        m->tx_buffer[0] = m->send_bsn | (m->send_bib << 7);
        m->tx_buffer[1] = m->retrans_last_sent | (m->send_fib << 7);
        m->tx_buffer[2] = 1;      /* Length 1, meaning LSSU. */
        m->tx_buffer[3] = 3;      /* 3 is indication 'SIOS'. */
        LOG_D("<--SIOS");
        return;

    case MTP2_NOT_ALIGNED:
        /* Send SIO. */
        m->tx_len = 4;
        m->tx_buffer[0] = m->send_bsn | (m->send_bib << 7);
        m->tx_buffer[1] = m->retrans_last_sent | (m->send_fib << 7);
        m->tx_buffer[2] = 1;      /* Length 1, meaning LSSU. */
        m->tx_buffer[3] = 0;      /* 0 is indication 'SIO'. */
        LOG_D("<--SIO");
        return;

    case MTP2_ALIGNED:
    case MTP2_PROVING:
        /* Send SIE or SIN. */
        m->tx_len = 4;
        m->tx_buffer[0] = m->send_bsn | (m->send_bib << 7);
        m->tx_buffer[1] = m->retrans_last_sent | (m->send_fib << 7);
        m->tx_buffer[2] = 1;      /* Length 1, meaning LSSU. */
        m->tx_buffer[3] = 1;
        sin_scount++;
        LOG_D("<--SIN");
        return;

    case MTP2_READY:
    case MTP2_INSERVICE:
        /* Frame selection. */

        /* If we have something in the retransmission buffer, send it. This
           also handles sending new MSUs, as they are simply appended to the
           retransmit buffer. */
        if(m->retrans_seq != -1) {
            /* Send retransmission. */
            memcpy(m->tx_buffer,
                   m->retrans_buf[m->retrans_seq].buf,
                   m->retrans_buf[m->retrans_seq].len);
            m->tx_len = m->retrans_buf[m->retrans_seq].len;
            m->tx_buffer[0] = m->send_bsn | (m->send_bib << 7);
            m->tx_buffer[1] = m->retrans_seq | (m->send_fib << 7);

            if(m->retrans_seq == m->retrans_last_sent) {
                /* Retransmission done. */
                m->retrans_seq = -1;
            } else {
                /* Move to the next one. */
                m->retrans_seq = MTP_NEXT_SEQ(m->retrans_seq);
            }

            return;
        }

        /* Send Fill-in signalling unit (FISU) if nothing else is pending. */
        m->tx_len = 3;
        m->tx_buffer[0] = m->send_bsn | (m->send_bib << 7);
        m->tx_buffer[1] = m->retrans_last_sent | (m->send_fib << 7);
        m->tx_buffer[2] = 0;      /* Length 0, meaning FISU. */
        fisu_scount++;
        return;

    default:
        LOG_E("ERROR ! Internal: Unknown MTP2 state %d on link '%d'?!?", m->state, m->e1_no);
    }
}

/* Queue an MSU (in the retransmit buffer) for sending down the link. */
void mtp2_queue_msu(u8_t e1_no, u8_t sio, u8_t *sif, int len)
{
	int i;
    mtp2_t *m = &mtp2_state[e1_no];

    if (m->state != MTP2_INSERVICE)
    {
		if (m->state != MTP2_READY)
		{
			LOG_E("ERROR ! Got MSU (sio=%x), but link not in service, discarding on link '%d'.", sio, m->e1_no);
			return;
		}
	}

	if (len < 2)
	{
		LOG_E("ERROR ! Got illegal MSU length %d < 2, dropping frame on link '%d'.", len, m->e1_no);
		return;
	}

	i = MTP_NEXT_SEQ(m->retrans_last_sent);
	if(i == m->retrans_last_acked) {
		LOG_W("MTP retransmit buffer full, MSU lost on link '%d'.", m->e1_no);
		return;
	}

	m->retrans_buf[i].buf[0] = 0; /* BSN Will be set correctly when transmitted */
	m->retrans_buf[i].buf[1] = 0; /* FSN Will be set correctly when transmitted */
	m->retrans_buf[i].buf[2] = (len >= 62 ? 63 : 1 + len);
	m->retrans_buf[i].buf[3] = sio;
	memcpy(&(m->retrans_buf[i].buf[4]), sif, len);
	m->retrans_buf[i].len = len + 4;
	m->retrans_last_sent = i;
	/* Start transmitting the new SU, unless we were already (re-)transmitting. */
	if(m->retrans_seq == -1) {
		m->retrans_seq = i;
		/* Start timer T7 "excessive delay of acknowledgement". */
        sched_timeout(T7_TIMEOUT, mtp2_t7_timeout, m);
	}
}

/* Process a received link status signal unit. */
static void mtp2_process_lssu(mtp2_t *m, u8_t *buf, int fsn, int fib)
{
    int typ;

    typ = buf[3] & 0x07;
    switch(typ) {
    case 0:                   /* Status indication 'O' */
        if (m->state == MTP2_NOT_ALIGNED) {
            
            sched_untimeout(mtp2_t2_timeout, m);
            sched_timeout(T3_TIMEOUT, mtp2_t3_timeout, m);
            m->state = MTP2_ALIGNED;
            LOG_I("Got status indication 'O' while NOT_ALIGNED on link %d. ", m->e1_no);
        
        } else if (m->state == MTP2_READY) {
            
            abort_initial_alignment(m);
        
        } else if (m->state == MTP2_INSERVICE) {
            
            LOG_W("Got status indication 'O' while INSERVICE on link %d.", m->e1_no);
            mtp3_link_fail(m, 0);
        
        }
        LOG_I("-->SIO");
        break;

    case 1:                   /* Status indication 'N' */
    case 2:                   /* Status indication 'E' */
        /* ToDo: This shouldn't really be here, I think. */
        m->send_bsn = fsn;
        m->send_bib = fib;

        sin_rcount++;

        if (m->state == MTP2_NOT_ALIGNED) {

            sched_untimeout(mtp2_t2_timeout, m);
            sched_timeout(T3_TIMEOUT, mtp2_t3_timeout, m);
            m->state = MTP2_ALIGNED;
            LOG_I("Got status indication 'N' or 'E' while NOT_ALIGNED on link %d. ", m->e1_no);
        
        } else if (m->state == MTP2_ALIGNED) {

            LOG_I("Entering proving state for link '%d'.", m->e1_no);
            sched_untimeout(mtp2_t3_timeout, m);
            sched_timeout(T4_TIMEOUT, mtp2_t4_timeout, m);
            m->state = MTP2_PROVING;
        
        } else if (m->state == MTP2_INSERVICE) {

            LOG_W("Got status indication 'N' or 'E' while INSERVICE on link '%d'.", m->e1_no);
            mtp3_link_fail(m, 0);
        }
        LOG_I("-->SIN");
        break;

    case 3:                   /* Status indication 'OS' */
        if (m->state == MTP2_ALIGNED || m->state == MTP2_PROVING) {
        
            abort_initial_alignment(m);
        
        } else if (m->state == MTP2_READY) {
            
            LOG_W("Got status indication 'OS' while READY on link '%d'.", m->e1_no);
            mtp3_link_fail(m, 1);
        
        } else if(m->state == MTP2_INSERVICE) {
            
            LOG_W("Got status indication 'OS' while INSERVICE on link '%d'.", m->e1_no);
            mtp3_link_fail(m, 1);
        
        }
        LOG_I("-->SIOS");
        break;

    case 4:                   /* Status indication 'PO' */
        /* ToDo: Not implemented. */
        /* Don't do this, as the log would explode should this actually happen
           fifo_log(LOG_NOTICE, "Status indication 'PO' not implemented.\r\n");
        */
        break;

    case 5:                   /* Status indication 'B' */
        /* ToDo: Not implemented. */
        LOG_E("Status indication 'B' not implemented.");
        break;

    default:                  /* Illegal status indication. */
        LOG_E("Got undefined LSSU status %d on link '%d'.", typ, m->e1_no);
        mtp3_link_fail(m, 0);
    }
}

//#define MTP2_BUF_DEBUG
/* Process a received frame.

   The frame has already been checked for correct crc and for being at least
   5 bytes long.
*/
static void mtp2_good_frame(mtp2_t *m, u8_t *buf, int len)
{
    int fsn, fib, bsn, bib;
    int li;

    if (m->state == MTP2_DOWN) {
        return;
    }

    bsn = buf[0] & 0x7f;
    bib = buf[0] >> 7;
    fsn = buf[1] & 0x7f;
    fib = buf[1] >> 7;

    li = buf[2] & 0x3f;
/***
    if (option_debug > 2) {
        if (m->hwmtp2 || m->hwhdlcfcs || (li > 2)) {
            char pbuf[1000], hex[30];
            int i;
            int slc;

            if(variant(m) == ITU_SS7)
                slc = (buf[7] & 0xf0) >> 4;
            else if(variant(m) == CHINA_SS7)
                slc = (buf[10] & 0xf0) >> 4;
            else
                slc = (buf[10] & 0xf0) >> 4;

            strcpy(pbuf, "");
            for(i = 0; i < li - 1 && i + 4 < len; i++) {
                sprintf(hex, " %02x", buf[i + 4]);
                strcat(pbuf, hex);
            }
            vLog(MONITOR_DEBUG, "Got MSU on link '%s/%d' sio=%d slc=%d m.sls=%d bsn=%d/%d, fsn=%d/%d, sio=%02x, len=%d:%s\r\n", m->e1_no, m->schannel+1, buf[3] & 0xf, slc, m->sls, bib, bsn, fib, fsn, buf[3], li, pbuf);
        }
    }
***/

    if (li + 3 > len) {
        LOG_W("Got unreasonable length indicator %d (len=%d) on link '%d'.",
                 li, len, m->e1_no);
        return;
    }


    if (li == 1 || li == 2) {
        /* Link status signal unit. */
        mtp2_process_lssu(m, buf, fsn, fib);
        return;
    }

    /* Process the BSN of the signal unit.
       According to Q.703 (5), only FISU and MSU should have FSN and BSN
       processing done. */
    if (m->state != MTP2_INSERVICE) {
        if (m->state == MTP2_READY) {

            sched_untimeout(mtp2_t1_timeout, m);
            sched_untimeout(mtp2_t7_timeout, m);
            
            m->send_fib = bib;
            m->send_bsn = fsn;
            m->send_bib = fib;
            m->retrans_last_acked = bsn;
            
            m->state = MTP2_INSERVICE;

        } else {

            return;
        }
    } else if(m->state == MTP2_READY) {

        sched_untimeout(mtp2_t1_timeout, m);
        sched_untimeout(mtp2_t7_timeout, m);
    
    }

    /* Process the BSN of the received frame. */
    if((m->retrans_last_acked <= m->retrans_last_sent &&
        (bsn < m->retrans_last_acked || bsn > m->retrans_last_sent)) ||
       (m->retrans_last_acked > m->retrans_last_sent &&
        (bsn < m->retrans_last_acked && bsn > m->retrans_last_sent))) {
        /* They asked for a retransmission of a sequence number not available. */
        LOG_W("Received illegal BSN=%d (retrans=%d,%d) on link '%d', len=%d, si=02%02x, state=%d, count=%d.",
                 bsn, m->retrans_last_acked, m->retrans_last_sent, m->e1_no, len, buf[3] & 0xf, m->state, m->bsn_errors);
        if (m->bsn_errors++ > 2) {
            m->bsn_errors = 0;
            mtp3_link_fail(m, 1);
        }
        return;
    }
    m->bsn_errors = 0;

    /* Reset timer T7 if new acknowledgement received (Q.703 (5.3.1) last
       paragraph). */
    if(m->retrans_last_acked != bsn) {
        sched_untimeout(mtp2_t7_timeout, m);
        m->retrans_last_acked = bsn;
        if(m->retrans_last_acked != m->retrans_last_sent) {
            sched_timeout(T7_TIMEOUT, mtp2_t7_timeout, m);
        }
    }

    if(bib != m->send_fib) {
        /* Received negative acknowledge, start re-transmission. */
        m->send_fib = bib;
        if(bsn == m->retrans_last_sent) {
            /* Nothing to re-transmit. */
            m->retrans_seq = -1;
        } else {
            m->retrans_seq = MTP_NEXT_SEQ(bsn);
        }
    }

    /* Process the signal unit content. */
    if(li == 0) {
        /* Process the FSN of the received frame. */
        if(fsn != m->send_bsn) {
            /* This indicates the loss of a message. */
            if(fib == m->send_bib) {
                /* Send a negative acknowledgement, to request retransmission. */
                m->send_bib = !m->send_bib;
            }
        }
        fisu_rcount++;
    } else {
        /* Message signal unit. */
        /* Process the FSN of the received frame. */
        if(fsn == m->send_bsn) {
            /* Q.703 (5.2.2.c.i): Redundant retransmission. */
            return;
        } else if(fsn == MTP_NEXT_SEQ(m->send_bsn)) {
            /* Q.703 (5.2.2.c.ii). */
            if(fib == m->send_bib) {
                /* Successful frame reception. Do explicit acknowledge on next frame. */
                m->send_bsn = fsn;
            } else {
                /* Drop frame waiting for retransmissions to arrive. */
                return;
            }
        } else {
            /* Q.703 (5.2.2.c.iii). Frame lost before this frame, discart it
               (will be retransmitted in-order later). */
            if(fib == m->send_bib) {
                /* Send a negative acknowledgement, to request retransmission. */
                m->send_bib = !m->send_bib;
            }
            return;
        }

        /* Length indicator (li) is number of bytes in MSU after LI, so the valid
           bytes are buf[0] through buf[(li + 3) - 1]. */
        if(li < 5) {
            LOG_W("Got short MSU (no label), li=%d on link '%d'.", li, m->e1_no);
            return;
        }
#if 0
        {
            char pbuf[512], hex[30];
            int i;
            int slc;

            switch (variant(m)) {
            case ITU_SS7:
                slc = (buf[7] & 0xf0) >> 4;
                break;
            case ANSI_SS7:
            case CHINA_SS7:
                slc = (buf[10] & 0xf0) >> 4;
                break;
            }
            strcpy(pbuf, "");
            for(i = 0; i < li - 1 && i + 4 < len; i++) {
                sprintf(hex, " %02x", buf[i + 4]);
                strcat(pbuf, hex);
            }
            CARD_DEBUGF(MTP_DEBUG, ("Got MSU on link '%d' sio=%d slc=%d m.sls=%d bsn=%d/%d, fsn=%d/%d, sio=%02x, len=%d:%s\n", 
                            m->e1_no, buf[3] & 0xf, slc, m->sls, bib, bsn, fib, fsn, buf[3], li, pbuf));
        }
#endif
        LOG_HEX("mtp2", 16, &buf[3], len-3);
        //send_ss7_msg(m->e1_no, &buf[3], len - 3);
    }
}

/* MTP2 reading of signalling units.
 * The buffer pointer is only valid until return from this function, so
 * the data must be copied out as needed.
 */
static void mtp2_read_su(mtp2_t *m, u8_t *buf, int len)
{
	if((len > MTP_MAX_PCK_SIZE) || (len < 3)) {
		LOG_W("Overlong/too short MTP2 frame %d, dropping on link '%d'", len, m->e1_no);
		return;
	}

	mtp2_good_frame(m, buf, len);
}

static void prepare_init_link(int e1_no)
{
    if (e1_no >= E1_LINKS_MAX) {
        return;
    }

    mtp2_t *m = &mtp2_state[e1_no];
    m->e1_no = e1_no;
    m->tx_len = m->rx_len = 0;

    m->retrans_seq = -1;
    m->retrans_last_acked = 0x7F;
    m->retrans_last_sent = 0x7F;

    /* for ss7 */
    m->send_fib = 1;
    m->send_bsn = 0x7F;
    m->send_bib = 1;

    /* for pri */
    m->sapi = m->tei = 0;
}

void e1_port_init(int e1_no) 
{
    if (e1_no >= E1_LINKS_MAX) {
        return;
    }

    if (!E1_PORT_ENABLE(e1_no))
        return;

    mtp2_t *m = &mtp2_state[e1_no];
    if (!CHN_NO1_PORT_ENABLE(e1_no)) {       
        prepare_init_link(e1_no);
        if (SS7_PORT_ENABLE(e1_no)) { /* ss7 */
            m->protocal = SS7_PROTO_TYPE;
            LOG_D("link '%d' start ss7 init", e1_no);
            ds26518_port_init(e1_no, CCS_TYPE);
            start_initial_alignment(m, "Initial");
        } else {  /* ISDN PRI */
            m->protocal = PRI_PROTO_TYPE;
            m->pri_mode = PRI_NETWORK_ENABLE(e1_no) ? PRI_NETWORK : PRI_CPE;
            LOG_D("link '%d' start isdn init", e1_no);
            ds26518_port_init(e1_no, CCS_TYPE);
            q921_start(m);
        }
    } else { /* china no.1 */
        LOG_D("link '%d' start china-no1 init", e1_no);
        ds26518_port_init(e1_no, CAS_TYPE);
        m->protocal = NO1_PROTO_TYPE;
    }
}

mtp2_t *get_mtp2_state(u8_t link_no)
{
    if (link_no >= E1_LINKS_MAX) {
        LOG_W("link '%d' is out of rangle", link_no);
        return NULL;
    }

    return &mtp2_state[link_no];
}

#ifndef MTP2_POLL_ENABLE

osSemaphoreId	mtp_semaphore = NULL;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    /* PA5 for DS26518 INTERRUPT */
	if (GPIO_Pin == GPIO_PIN_5) {
		//ds26518_isr();
        osSemaphoreRelease(mtp_semaphore);
	}
}

void mtp_lowlevel_scan(void *arg)
{
    (void)arg;

    for(;;) {
        if (osSemaphoreAcquire(mtp_semaphore, portMAX_DELAY) == osOK) {
            LOCK_MTP2_CORE();
            ds26518_isr();
            UNLOCK_MTP2_CORE();
        }
    }
}

static void mtp_lowlevel_init(void)
{
    osThreadAttr_t attributes;
    memset(&attributes, 0x0, sizeof(osThreadAttr_t));
    attributes.name = "mtp_lowlevel";
    attributes.stack_size = 350;
    attributes.priority = osPriorityRealtime;
    osThreadNew(mtp_lowlevel_scan, NULL, &attributes);
}

#else

static void mtp2_thread(void *arg)
{
    LWIP_UNUSED_ARG(arg);

    mtp2_t *m;
    TickType_t  xPeriod = pdMS_TO_TICKS(4); //2ms 运行一次
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();

    for (;;) {
       vTaskDelayUntil(&xLastWakeTime, xPeriod);
#if 0
       for(int i = 0; i < E1_LINKS_MAX; i++) {
           m = &mtp2_state[i];
#endif 
           m = &mtp2_state[0];
#if 0
           if ((m == NULL) || (m->protocal == NO1_PROTO_TYPE) ||
                (m->protocal == SS7_PROTO_TYPE && m->state == MTP2_DOWN) ||
                (m->protocal == PRI_PROTO_TYPE && m->q921_state == Q921_TEI_UNASSIGNED)) {
                    continue;
                }
#endif
            ds26518_tx_rx_poll(0);
			//sched_timeout_routine();
#if 0
            ds26518_tx_rx_poll(i);
       }
#endif
    }
}

#endif

void mtp_init(void)
{
#if 0
    if (sys_mbox_new(&mtp_mbox, MTP_MBOX_SIZE) != ERR_OK) {
        CARD_ASSERT("failed to create mtp thread mbox", 0);
    }

    if (sys_mutex_new(&lock_mtp_core) != ERR_OK) {
        CARD_ASSERT("failed to create lock_mtp_core", 0);
    }
#endif

    ds26518_global_init();
#if 0
    for(int index = 0; index < E1_LINKS_MAX; index++) {
        e1_port_init(index);
    }
#endif
    e1_port_init(0);

#ifndef MTP2_POLL_ENABLE
    mtp_semaphore = osSemaphoreNew(1, 1, NULL);
    mtp_lowlevel_init();
#else
    sys_thread_new("mtp2", mtp2_thread, NULL, DEFAULT_THREAD_STACKSIZE, osPriorityNormal);
#endif
}

void mtp2_command(u8_t e1_no, u8_t command)
{
    mtp2_t *m = &mtp2_state[e1_no];

    switch (command) {
        case MTP2_ACTIVE_LINK:
            start_initial_alignment(m, "User start!");
            break;
        case MTP2_DEACTIVE_LINK:
            abort_initial_alignment(m);
            break;
        case MTP2_STOP_L2:
            abort_initial_alignment(m);
            m->state = MTP2_DOWN;
            break;
        case MTP2_EMERGEN_ALIGNMENT:
            start_initial_alignment(m, "Emergen start");
            break;
    }
}

/* All below function called by ds26518 ISR routing procedure. */
void send_ccs_msg(u8_t e1_no, u8_t send_len)
{
    mtp2_t *m = &mtp2_state[e1_no];

    if (m->tx_len == 0) {
        if (m->protocal == PRI_PROTO_TYPE) {
            q921_pick_frame(m);
        } else if (m->protocal == SS7_PROTO_TYPE) {
            mtp2_pick_frame(m);
        } else {
            return;
        }

        if (m->tx_len == 0) {
            return;
        }
    }

    if (m->tx_len > send_len) {
        ds26518_tx_set(e1_no, m->tx_buffer, send_len, 0);
        m->tx_len -= send_len;

        for(u8_t i = 0; i < m->tx_len; i++) {
            m->tx_buffer[i] = m->tx_buffer[i + send_len];
        }
    } else {
        ds26518_tx_set(e1_no, m->tx_buffer, m->tx_len, 1);
        m->tx_len = 0; // Reset it for next pick.
    }

}

void rv_ccs_byte(u8_t e1_no, u8_t data)
{
    mtp2_t *m = &mtp2_state[e1_no];

#if 0
    if (m->protocal == NO1_PROTO_TYPE ||
        (m->protocal == SS7_PROTO_TYPE && m->state == MTP2_DOWN) ||
        (m->protocal == PRI_PROTO_TYPE && m->q921_state == Q921_TEI_UNASSIGNED)) {
        return;
    }
#endif
    m->rx_buf[m->rx_len++] = data;
}

void check_ccs_msg(u8_t e1_no)
{
    mtp2_t *m = &mtp2_state[e1_no];

    if (m->protocal == SS7_PROTO_TYPE) {
        /* Delete CRC16 2 byte */
        mtp2_read_su(m, m->rx_buf, m->rx_len -2);
    } else if (m->protocal == PRI_PROTO_TYPE) {
        q921_receive(m, (q921_h *)m->rx_buf, m->rx_len - 2);
    }

    m->rx_len = 0;
}

void bad_msg_rev(u8_t e1_no, u8_t err)
{
    mtp2_t *m = &mtp2_state[e1_no];

    m->rx_len = 0;

    /* err list  
     2: CRC Error.
     3: Abort.
     4: Overrun.
    */
   LOG_W("%d E1 Receive error, err_no=%d", e1_no, err);
}

u8_t read_l2_status(int e1_no)
{
    mtp2_t *m = &mtp2_state[e1_no];

    if (m->protocal == NO1_PROTO_TYPE) {
        return 1;
    }

    if (m->protocal == SS7_PROTO_TYPE && m->state == MTP2_INSERVICE) {
        return 1;
    }

    if (m->protocal == PRI_PROTO_TYPE && m->q921_state == Q921_MULTI_FRAME_ESTABLISHED) {
        return 1;
    }

    return 0;
}

