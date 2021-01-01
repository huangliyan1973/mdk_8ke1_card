/*
 * q921.c
 *
 *  Created on: 2020年10月19日
 *      Author: hly66
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "lwip/sys.h"
#include "main.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "ds26518.h"
#include "mtp.h"
#include "eeprom.h"
#include "server_interface.h"
#include "8ke1_debug.h"

#define MTP_DEBUG                1

#define Q921_INIT(fr, l_sapi, l_tei) \
	do { \
		(fr)->h.sapi = l_sapi; \
		(fr)->h.ea1 = 0; \
		(fr)->h.ea2 = 1; \
		(fr)->h.tei = l_tei; \
	} while (0)

#define Q921_CLEAR_INIT(fr, l_sapi, l_tei) \
	do { \
		memset((fr), 0, sizeof(*(fr))); \
		Q921_INIT((fr), (l_sapi), (l_tei)); \
	} while (0)

#define Q921_INC(j) (j) = (((j) + 1) % 128)
#define Q921_DEC(j) (j) = (((j) - 1) % 128)

static void stop_t203(mtp2_t *m);
static void start_t200(mtp2_t *m);
static void q921_establish_data_link(mtp2_t *m);

static inline int Q921_ADD(int a, int b)
{
	return (a + b) % 128;
}
/*!
 * \internal
 * \brief Convert Q.921 TEI management message type to a string.
 *
 * \param message Q.921 TEI management message type to convert.
 *
 * \return TEI management message type name string
 */
static const char *q921_tei_mgmt2str(enum q921_tei_identity message)
{
	switch (message) {
	case Q921_TEI_IDENTITY_REQUEST:
		return "TEI Identity Request";
	case Q921_TEI_IDENTITY_ASSIGNED:
		return "TEI Identity Assigned";
	case Q921_TEI_IDENTITY_CHECK_REQUEST:
		return "TEI Identity Check Request";
	case Q921_TEI_IDENTITY_REMOVE:
		return "TEI Identity Remove";
	case Q921_TEI_IDENTITY_DENIED:
		return "TEI Identity Denied";
	case Q921_TEI_IDENTITY_CHECK_RESPONSE:
		return "TEI Identity Check Response";
	case Q921_TEI_IDENTITY_VERIFY:
		return "TEI Identity Verify";
	}

	return "Unknown";
}

/*!
 * \internal
 * \brief Convert Q.921 state to a string.
 *
 * \param state Q.921 state to convert.
 *
 * \return State name string
 */
static const char *q921_state2str(enum q921_state state)
{
	switch (state) {
	case Q921_TEI_UNASSIGNED:
		return "TEI unassigned";
	case Q921_ASSIGN_AWAITING_TEI:
		return "Assign awaiting TEI";
	case Q921_ESTABLISH_AWAITING_TEI:
		return "Establish awaiting TEI";
	case Q921_TEI_ASSIGNED:
		return "TEI assigned";
	case Q921_AWAITING_ESTABLISHMENT:
		return "Awaiting establishment";
	case Q921_AWAITING_RELEASE:
		return "Awaiting release";
	case Q921_MULTI_FRAME_ESTABLISHED:
		return "Multi-frame established";
	case Q921_TIMER_RECOVERY:
		return "Timer recovery";
	}

	return "Unknown state";
}

static void q921_setstate(mtp2_t *m, int newstate)
{
#if 0
	CARD_DEBUGF(MTP_DEBUG, ("%d E1 Changing from state %d(%s) to %d(%s)\r\n",
					m->e1_no, m->q921_state, q921_state2str(m->q921_state),
					newstate, q921_state2str(newstate)));
#endif
	m->q921_state = newstate;
}

void q931_dl_event(mtp2_t *m, enum Q931_DL_EVENT event)
{
	/*XXX Need to impliment! */
}

static void q921_discard_iqueue(mtp2_t *m)
{
	m->retrans_last_acked = 0x7f;
	m->retrans_last_sent = 0x7f;
	m->retrans_seq = -1;
	m->tx_len = 0;
}

void q921_pick_frame(mtp2_t *m)
{
	if(m->s_t != m->s_h){
		memcpy(m->tx_buffer, m->u_s_frame[m->s_t].buf, m->u_s_frame[m->s_t].len);
		m->tx_len = m->u_s_frame[m->s_t].len;
		m->s_t = (m->s_t + 1) % U_S_PCK_BUFF_SIZE;
		return;
	}

	if(m->retrans_seq != -1) {
		/* Need send I frame. */
		if (m->q921_state != Q921_MULTI_FRAME_ESTABLISHED) {
			CARD_DEBUGF(MTP_DEBUG, (
				"%d E1: TEI=%d Just queued I-frame since in state %d(%s)\r\n",
				m->e1_no, m->tei,
				m->q921_state, q921_state2str(m->q921_state)));
			return;
		}
		if (m->peer_rx_busy) {
			CARD_DEBUGF(MTP_DEBUG, (
				"%d E1: TEI=%d Just queued I-frame due to peer busy condition\r\n",
				m->e1_no, m->tei));
			return;
		}
		if (m->v_s == Q921_ADD(m->v_a, 7)) {
			CARD_DEBUGF(MTP_DEBUG, (
				"%d E1:TEI=%d Couldn't transmit I-frame at this time due to window shut\r\n",
					m->e1_no,m->tei));
			return;
		}

		memcpy(m->tx_buffer,
			   m->retrans_buf[m->retrans_seq].buf,
			   m->retrans_buf[m->retrans_seq].len);
		m->tx_len = m->retrans_buf[m->retrans_seq].len;

		q921_i *h = (q921_i *)m->tx_buffer;

		h->h.tei = m->tei;
		h->n_s = m->v_s;
		h->n_r = m->v_r;
		h->ft = 0;
		h->p_f = 0;

		Q921_INC(m->v_s);
		if(m->retrans_seq == m->retrans_last_sent) {
			/* Retransmission done. */
			m->retrans_seq = -1;
		} else {
			/* Move to the next one. */
			Q921_INC(m->retrans_seq);
		}
		m->acknowledge_pending = 0;
		if (!m->t200_timer) {
			stop_t203(m);
			start_t200(m);
		}
	}
}

static void q921_transmit(mtp2_t *m, q921_h *h, int len)
{
	if(len > U_S_PCK_SIZE)
			return;

	memcpy(m->u_s_frame[m->s_h].buf, h, len);
	m->u_s_frame[m->s_h].len = len;

	m->s_h = (m->s_h + 1) % U_S_PCK_BUFF_SIZE;
}

static void q921_mdl_send(mtp2_t *m, enum q921_tei_identity message, int ri, int ai, int iscommand)
{
	uint8_t  buf[10];

	q921_u *f = (q921_u *)buf;

	Q921_INIT(f, Q921_SAPI_LAYER2_MANAGEMENT, Q921_TEI_GROUP);
	f->h.c_r = (m->pri_mode == PRI_NETWORK) ? iscommand : !iscommand;
	f->ft = Q921_FRAMETYPE_U;
	f->data[0] = 0x0f;	/* Management entity */
	f->data[1] = (ri >> 8) & 0xff;
	f->data[2] = ri & 0xff;
	f->data[3] = message;
	f->data[4] = (ai << 1) | 1;

	CARD_DEBUGF(MTP_DEBUG, (
		"%d Sending MDL message: %d(%s), TEI=%d\r\n",
		m->e1_no, message, q921_tei_mgmt2str(message), ai));

	q921_transmit(m, (q921_h *)f, 8);
}

#if 0
ctrl->timers[PRI_TIMER_T200] = 1000;		/* Time between SABME's */
	ctrl->timers[PRI_TIMER_T201] = ctrl->timers[PRI_TIMER_T200];/* Time between TEI Identity Checks (Default same as T200) */
	ctrl->timers[PRI_TIMER_T202] = 2 * 1000;	/* Min time between transmission of TEI Identity request messages */
	ctrl->timers[PRI_TIMER_T203] = 10 * 1000;	/* Max time without exchanging packets */

#endif

static void t202_expire(void *data)
{
    static u16_t ri = 1;
	mtp2_t *m = (mtp2_t *)data;

	/* Start the TEI request timer. */
    sys_timeout(2000, t202_expire, m);

	++m->n202_counter;

	/* Max numer of transmissions of the TEI identity request message  = 3 */
	if (m->n202_counter > 3) {
		sys_untimeout(t202_expire, m);
		CARD_DEBUGF(MTP_DEBUG, ("%d E1 Unable to receive TEI from network in state %d(%s)!\r\n",
			m->e1_no, m->q921_state, q921_state2str(m->q921_state)));

		switch (m->q921_state) {
			case Q921_ASSIGN_AWAITING_TEI:
				break;
			case Q921_ESTABLISH_AWAITING_TEI:
				q921_discard_iqueue(m);
				/* DL-RELEASE indication */
				//TODO: 需要通知应用层结束呼叫
				//q931_dl_event(link, Q931_DL_EVENT_DL_RELEASE_IND);
				break;
			default:
				break;
		}
		q921_setstate(m, Q921_TEI_UNASSIGNED);
		return ;
	}

	/* Send TEI request */
	m->ri = (ri++ % 65535);
	q921_mdl_send(m, Q921_TEI_IDENTITY_REQUEST, m->ri, Q921_TEI_GROUP, 1);
}

static void q921_tei_request(mtp2_t *m)
{
	m->n202_counter = 0;
	t202_expire(m);
}

static void q921_tei_remove(mtp2_t *m, int tei)
{
	/*
	 * Q.921 Section 5.3.2 says we should send the remove message
	 * twice, in case of message loss.
	 */
	q921_mdl_send(m, Q921_TEI_IDENTITY_REMOVE, 0, tei, 1);
	q921_mdl_send(m, Q921_TEI_IDENTITY_REMOVE, 0, tei, 1);
}

static void q921_send_dm(mtp2_t *m, int fbit) /* DM表示Layer2 处于拆线状态，无法执行多帧操作 */
{
	q921_h h;

	Q921_CLEAR_INIT(&h, m->sapi, m->tei);
	h.u.m3 = 0;	/* M3 = 0 */
	h.u.m2 = 3;	/* M2 = 3 */
	h.u.p_f = fbit;	/* Final set appropriately */
	h.u.ft = Q921_FRAMETYPE_U;
	switch (m->pri_mode) {
	case PRI_NETWORK:
		h.h.c_r = 0;
		break;
	case PRI_CPE:
		h.h.c_r = 1;
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("Don't know how to DM on a type %d node\r\n", m->pri_mode));
		return;
	}

	q921_transmit(m, &h, 3);
}

static void q921_send_disc(mtp2_t *m, int pbit) /*结束多帧操作时发送DISC*/
{
	q921_h h;

	Q921_CLEAR_INIT(&h, m->sapi, m->tei);
	h.u.m3 = 2;	/* M3 = 2 */
	h.u.m2 = 0;	/* M2 = 0 */
	h.u.p_f = pbit;	/* Poll set appropriately */
	h.u.ft = Q921_FRAMETYPE_U;
	switch (m->pri_mode) {
	case PRI_NETWORK:
		h.h.c_r = 0;
		break;
	case PRI_CPE:
		h.h.c_r = 1;
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("Don't know how to DISC on a type %d node\r\n", m->pri_mode));
		return;
	}

	CARD_DEBUGF(MTP_DEBUG, ("%d E1 TEI=%d Sending DISC\r\n", m->e1_no,m->tei));

	q921_transmit(m, &h, 3);
}

static void q921_send_ua(mtp2_t *m, int fbit)
{
	q921_h h;

	Q921_CLEAR_INIT(&h, m->sapi, m->tei);
	h.u.m3 = 3;		/* M3 = 3 */
	h.u.m2 = 0;		/* M2 = 0 */
	h.u.p_f = fbit;	/* Final set appropriately */
	h.u.ft = Q921_FRAMETYPE_U;
	switch (m->pri_mode) {
	case PRI_NETWORK:
		h.h.c_r = 0;
		break;
	case PRI_CPE:
		h.h.c_r = 1;
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("Don't know how to DISC on a type %d node\r\n", m->pri_mode));
		return;
	}

	CARD_DEBUGF(MTP_DEBUG, ("%d E1 TEI=%d Sending UA\r\n", m->e1_no, m->tei));

	q921_transmit(m, &h, 3);
}

static void q921_send_sabme(mtp2_t *m)
{
	q921_h h;

	Q921_CLEAR_INIT(&h, m->sapi, m->tei);

	h.u.m3 = 3;	/* M3 = 3 */
	h.u.m2 = 3;	/* M2 = 3 */
	h.u.p_f = 1;	/* Poll bit set */
	h.u.ft = Q921_FRAMETYPE_U;
	switch (m->pri_mode) {
	case PRI_NETWORK:
		h.h.c_r = 1;
		break;
	case PRI_CPE:
		h.h.c_r = 0;
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("Don't know how to DISC on a type %d node\r\n", m->pri_mode));
		return;
	}

	CARD_DEBUGF(MTP_DEBUG, ("%d E1 TEI=%d Sending SABME\r\n", m->e1_no, m->tei));

	q921_transmit(m, &h, 3);
}

static int q921_ack_packet(mtp2_t *m, int num)
{
#if 0
	struct q921_frame *f;
	struct q921_frame *prev;

	for (prev = NULL, f = link->tx_queue; f; prev = f, f = f->next) {
		if (f->status != Q921_TX_FRAME_SENT) {
			break;
		}
		if (f->h.n_s == num) {
			/* Cancel each packet as necessary */
			/* That's our packet */
			if (prev)
				prev->next = f->next;
			else
				link->tx_queue = f->next;

			vLog(MONITOR_NOTICE,
				"-- ACKing N(S)=%d, tx_queue head is N(S)=%d (-1 is empty, -2 is not transmitted)\n",
				f->h.n_s,
				link->tx_queue
					? link->tx_queue->status == Q921_TX_FRAME_SENT
						? link->tx_queue->h.n_s
						: -2
					: -1);
			/* Update v_a */
			vPortFree(f);
			return 1;
		}
	}
#endif
	return 1;
}

static void t203_expire(void *data);
static void t200_expire(void *data);

#define restart_t200(m) 	reschedule_t200(m)

static void reschedule_t200(mtp2_t *m)
{
	CARD_DEBUGF(MTP_DEBUG, ("-- Restarting T200 timer\r\n"));

    sys_untimeout(t200_expire, m);
    sys_timeout(1000, t200_expire, m);
}

static void start_t203(mtp2_t *m)
{
	CARD_DEBUGF(MTP_DEBUG, ("-- Starting T203 timer\r\n"));
    sys_untimeout(t203_expire, m);
    sys_timeout(10000, t203_expire, m);
}

static void stop_t203(mtp2_t *m)
{
    CARD_DEBUGF(MTP_DEBUG, ("-- Stopping T203 timer\r\n"));
    sys_untimeout(t203_expire, m);
}

static void start_t200(mtp2_t *m)
{
	CARD_DEBUGF(MTP_DEBUG, ("-- Starting T200 timer\r\n"));
	sys_untimeout(t200_expire, m);
    sys_timeout(1000, t200_expire, m);
}

static void stop_t200(mtp2_t *m)
{
    CARD_DEBUGF(MTP_DEBUG, ("-- Stopping T200 timer\r\n"));
    sys_untimeout(t200_expire, m);
}

/*!
 * \internal
 * \brief Initiate bringing up layer 2 link.
 *
 * \param link Layer 2 link to bring up.
 *
 * \return Nothing
 */
static void kick_start_link(mtp2_t *m)
{

	switch (m->q921_state) {
	case Q921_TEI_UNASSIGNED:
		q921_setstate(m, Q921_ESTABLISH_AWAITING_TEI);
		q921_tei_request(m);
		break;
	case Q921_ASSIGN_AWAITING_TEI:
		q921_setstate(m, Q921_ESTABLISH_AWAITING_TEI);
		break;
	case Q921_TEI_ASSIGNED:
		q921_discard_iqueue(m);
		q921_establish_data_link(m);
		m->l3_initiated = 1;
		q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
		break;
	default:
		break;
	}
}

static void restart_timer_expire(void *data)
{
	mtp2_t *m = (mtp2_t *)data;

	switch (m->q921_state) {
	case Q921_TEI_UNASSIGNED:
	case Q921_ASSIGN_AWAITING_TEI:
	case Q921_TEI_ASSIGNED:
		/* Try to bring layer 2 up. */
		kick_start_link(m);
		break;
	default:
		break;
	}
}

static void restart_timer_stop(mtp2_t *m)
{
    sys_untimeout(restart_timer_expire, m);
}

/*! \note Only call on the transition to state Q921_TEI_ASSIGNED or already there. */
static void restart_timer_start(mtp2_t *m)
{
	CARD_DEBUGF(MTP_DEBUG, ("%d E1 SAPI/TEI=%d/%d Starting link restart delay timer\r\n",
			m->e1_no, m->sapi, m->tei));

	sys_untimeout(restart_timer_expire, m);
    sys_timeout(1000, restart_timer_expire, m);
}

/*! \note Only call on the transition to state Q921_TEI_ASSIGNED or already there. */
static int q921_check_delay_restart(mtp2_t *m)
{
	int ev = 0;
		/*
		 * For PTP links:
		 * This is where we act a bit like L3 instead of L2, since we've
		 * got an L3 that depends on us keeping L2 automatically alive
		 * and happy.
		 *
		 * For PTMP links:
		 * We can optionally keep L2 automatically alive and happy.
		 */
		restart_timer_start(m);

		switch (m->q921_state) {
		case Q921_MULTI_FRAME_ESTABLISHED:
		case Q921_TIMER_RECOVERY:
			/* Notify the upper layer that layer 2 went down. */
			/*
			ctrl->schedev = 1;
			ctrl->ev.gen.e = PRI_EVENT_DCHAN_DOWN;
			ev = &ctrl->ev;
			*/
			break;
		default:
			ev = -1;
			break;
		}

	return ev;
}

void q921_bring_layer2_up(mtp2_t *m)
{
	kick_start_link(m);
}

static void q921_clear_exception_conditions(mtp2_t *m)
{
	m->own_rx_busy = 0;
	m->peer_rx_busy = 0;
	m->reject_exception = 0;
	m->acknowledge_pending = 0;
}

static void q921_dump_pri(mtp2_t *m, char direction_tag);

void q921_dump(mtp2_t *m, q921_h *h, int len, int debugflags, int txrx)
{
	int x;
	const char *type;
	char direction_tag;

	direction_tag = txrx ? '>' : '<';

	CARD_DEBUGF(MTP_DEBUG, ("\r\n"));
	if (debugflags) {
		q921_dump_pri(m, direction_tag);
	}

#if 0
	if (debugflags) {
		char buf[500] = {0};
		int buflen = 0;

		for (x=0;x<len;x++)
			buflen += sprintf(buf + buflen, "%02x ", h->raw[x]);
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c [ %s]\r\n", m->e1_no, direction_tag, buf);

	}
#endif

	if (debugflags) {
		switch (h->h.data[0] & Q921_FRAMETYPE_MASK) {
		case 0:
		case 2:
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c Informational frame:\r\n", m->e1_no, direction_tag));
			break;
		case 1:
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c Supervisory frame:\r\n", m->e1_no, direction_tag));
			break;
		case 3:
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c Unnumbered frame:\r\n", m->e1_no, direction_tag));
			break;
		}
#if 0
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c SAPI: %02d  C/R: %d TEI: %d\r\n",
			m->e1_no,
			direction_tag,
			h->h.sapi,
			h->h.c_r,
			h->h.tei);
#endif

		switch (h->h.data[0] & Q921_FRAMETYPE_MASK) {
		case 0:
		case 2:
			/* Informational frame */
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c N(S): %03d   0: %d\r\n",
				m->e1_no,
				direction_tag,
				h->i.n_s,
				h->i.ft));
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c N(R): %03d   P: %d\r\n",
				m->e1_no,
				direction_tag,
				h->i.n_r,
				h->i.p_f));
#if 0
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c %d bytes of data\r\n",
				m->e1_no,
				direction_tag,
				len - 2);
#endif
			break;
		case 1:
			/* Supervisory frame */
			type = "???";
			switch (h->s.ss) {
			case 0:
				type = "RR";
				break;
			case 1:
				type = "RNR";
				break;
			case 2:
				type = "REJ";
				break;
			}
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c N(R): %03d    P/F: %d [ %s ]\r\n",
				m->e1_no,
				direction_tag,
				h->s.n_r,
				h->s.p_f,
				type));
#if 0
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c N(R): %03d P/F: %d\r\n",
				m->e1_no,
				direction_tag,
				h->s.n_r,
				h->s.p_f);
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c %d bytes of data\r\n",
				m->e1_no,
				direction_tag,
				len - 2);
#endif
			break;
		case 3:
			/* Unnumbered frame */
			type = "???";
			if (h->u.ft == 3) {
				switch (h->u.m3) {
				case 0:
					if (h->u.m2 == 3)
						type = "DM";
					else if (h->u.m2 == 0)
						type = "UI";
					break;
				case 2:
					if (h->u.m2 == 0)
						type = "DISC";
					break;
				case 3:
					if (h->u.m2 == 3)
						type = "SABME";
					else if (h->u.m2 == 0)
						type = "UA";
					break;
				case 4:
					if (h->u.m2 == 1)
						type = "FRMR";
					break;
				case 5:
					if (h->u.m2 == 3)
						type = "XID";
					break;
				default:
					break;
				}
			}
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c P/F: %d [ %s ]\r\n",
				m->e1_no,
				direction_tag,
				h->u.p_f,
				type));
#if 0
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c %d bytes of data\r\n",
				m->e1_no,
				direction_tag,
				len - 1);
#endif
			break;
		}

		if ((h->u.ft == 3) && (h->u.m3 == 0) && (h->u.m2 == 0) && (h->u.data[0] == 0x0f)) {
			int ri;
			u8_t *action;

			/* TEI management related */
			type = q921_tei_mgmt2str(h->u.data[3]);
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c MDL Message: %d(%s)\r\n", m->e1_no,direction_tag, h->u.data[3], type));
			ri = (h->u.data[1] << 8) | h->u.data[2];
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c Ri: %d\r\n", m->e1_no,direction_tag, ri));
			action = &h->u.data[4];
			for (x = len - (action - (u8_t *) h); 0 < x; --x, ++action) {
				CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c Ai: %d E:%d\r\n",
					m->e1_no,direction_tag, (*action >> 1) & 0x7f, *action & 0x01));
			}
		}
	}
}

static void q921_dump_pri(mtp2_t *m, char direction_tag)
{

//	CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c TEI: %d State %d(%s)\r\n",
//		m->e1_no, direction_tag, m->tei, m->q921_state, q921_state2str(m->q921_state));
	CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c V(A)=%d, V(S)=%d, V(R)=%d\r\n",
		m->e1_no, direction_tag, m->v_a, m->v_s, m->v_r));
#if 0
	CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c K=%d, RC=%d, l3_initiated=%d, reject_except=%d, ack_pend=%d\r\n",
		m->e1_no, direction_tag, 7, m->RC, m->l3_initiated,
		m->reject_exception, m->acknowledge_pending);
	CARD_DEBUGF(MTP_DEBUG, ("%d E1: %c T200_id=%d, N200=%d, T203_id=%d\r\n",
		m->e1_no, direction_tag, m->t200_timer, 3, m->t203_timer);
#endif
}

static void q921_mdl_ignore(mtp2_t *m, q921_u *h, const char *reason)
{
	CARD_DEBUGF(MTP_DEBUG, ("%d E1: Ignoring MDL message: %d(%s)  %s\n",
		m->e1_no, h->data[3], q921_tei_mgmt2str(h->data[3]), reason));
}

static int q921_mdl_receive(mtp2_t *m, q921_u *h, int len)
{
    static u16_t cr = 1;
	int tei;

	if (len <= &h->data[0] - (u8_t *) h) {
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Received short MDL frame\r\n", m->e1_no));
		return -1;
	}
	if (h->data[0] != 0x0f) {
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Received MDL with unsupported management entity %02x\r\n",
			m->e1_no,h->data[0]));
		return -1;
	}
	if (len <= &h->data[4] - (u8_t *) h) {
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Received short MDL message\r\n",m->e1_no));
		return -1;
	}
	if (h->data[3] != Q921_TEI_IDENTITY_CHECK_RESPONSE
		&& !(h->data[4] & 0x01)) {
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Received MDL message: %d(%s) with Ai E bit not set.\r\n",
			m->e1_no,h->data[3], q921_tei_mgmt2str(h->data[3])));
		return -1;
	}

	CARD_DEBUGF(MTP_DEBUG, ("%d E1: Received MDL message: %d(%s)\n",
		m->e1_no, h->data[3], q921_tei_mgmt2str(h->data[3])));

	if (m->pri_mode == PRI_NETWORK) {
		/*
		 * We are not managing automatic TEI's in this mode so we can
		 * ignore MDL messages from the CPE.
		 */
		q921_mdl_ignore(m, h, "We are in NT-PTP mode.\r\n");
		return -1;
	}

	//ri = (h->data[1] << 8) | h->data[2];
	tei = (h->data[4] >> 1);

	switch (h->data[3]) {
	case Q921_TEI_IDENTITY_REQUEST:
	case Q921_TEI_IDENTITY_CHECK_RESPONSE:
	case Q921_TEI_IDENTITY_VERIFY:
		q921_mdl_ignore(m, h, "We are not in NT-PTMP mode.\r\n");
		return -1;
	case Q921_TEI_IDENTITY_ASSIGNED:
		if (m->pri_mode == PRI_NETWORK) {
			/* We should not be receiving this message. */
			q921_mdl_ignore(m, h, "We are the network.\r\n");
		}
		return -1;
	case Q921_TEI_IDENTITY_CHECK_REQUEST:
		if (m->pri_mode == PRI_NETWORK) {
			/* We should not be receiving this message. */
			q921_mdl_ignore(m, h, "We are the network.\r\n");
			return -1;
		}

		/* If it's addressed to the group TEI or to our TEI specifically, we respond */
		if (tei == Q921_TEI_GROUP || tei == m->tei) {
			q921_mdl_send(m, Q921_TEI_IDENTITY_CHECK_RESPONSE, cr++ % 65535, m->tei, 1);
		}
		break;
	case Q921_TEI_IDENTITY_REMOVE:
		if (m->pri_mode == PRI_NETWORK) {
			/* We should not be receiving this message. */
			q921_mdl_ignore(m, h, "We are the network.\r\n");

		}
		return -1;
	}
	return 0;	/* Do we need to return something??? */
}

/* This is the equivalent of a DL-DATA request, as well as the I-frame queued up outcome */
int q921_transmit_iframe(mtp2_t *m, void *buf, int len /*, int cr*/)
{
	q921_i iframe;

	/* Figure B.7/Q.921 Page 70 */
	switch (m->q921_state) {
	case Q921_TEI_ASSIGNED:
		/* If we aren't in a state compatiable with DL-DATA requests, start getting us there here */
		restart_timer_stop(m);
		q921_establish_data_link(m);
		m->l3_initiated = 1;
		q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
		/* For all rest, we've done the work to get us up prior to this and fall through */
	case Q921_ESTABLISH_AWAITING_TEI:
	case Q921_TIMER_RECOVERY:
	case Q921_AWAITING_ESTABLISHMENT:
	case Q921_MULTI_FRAME_ESTABLISHED:

		Q921_INIT(&iframe, m->sapi, m->tei);
		iframe.h.c_r = (m->pri_mode == PRI_NETWORK) ? 1 : 0;
#if 0
		switch (m->pri_mode) {
		case PRI_NETWORK:
			if (cr)
				iframe.h.c_r = 1;
			else
				iframe.h.c_r = 0;
			break;
		case PRI_CPE:
			if (cr)
				iframe.h.c_r = 0;
			else
				iframe.h.c_r = 1;
			break;
		}
#endif
		/* Put new frame on queue tail. */
		//f->len = len + 4;
		//memcpy(i->h.data, buf, len);

		int index = (m->retrans_last_sent + 1) % 128;
		if(index == m->retrans_last_acked) {
			CARD_DEBUGF(MTP_DEBUG, ("Q921 retransmit buffer full, we will lost on link '%d'.\r\n", m->e1_no));
			break;
		}

		memcpy(m->retrans_buf[index].buf, &iframe, 4);
		memcpy(&m->retrans_buf[index].buf[4], buf, len);

		m->retrans_buf[index].len = len + 4;
		m->retrans_last_sent = index;

		if (m->retrans_seq == -1){
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: queued I-frame , retrans_last_sent=%d\r\n",
					m->e1_no, m->retrans_last_sent));
			m->retrans_seq = m->retrans_last_sent;
		}
		break;
	case Q921_TEI_UNASSIGNED:
	case Q921_ASSIGN_AWAITING_TEI:
	case Q921_AWAITING_RELEASE:
	default:
		CARD_DEBUGF(MTP_DEBUG, ("Cannot transmit frames in state %d(%s)\n",
			m->q921_state, q921_state2str(m->q921_state)));
		break;
	}
	return 0;
}

/* This is sending a DL-UNIT-DATA request */
int q921_transmit_uiframe(mtp2_t *m, void *buf, int len)
{
	uint8_t ubuf[512];
	q921_h *h = (void *)&ubuf[0];

	if (len >= 512) {
		CARD_DEBUGF(MTP_DEBUG, ("Requested to send UI-frame larger than 512 bytes!\n"));
		return -1;
	}

	memset(ubuf, 0, sizeof(ubuf));
	h->h.sapi = 0;
	h->h.ea1 = 0;
	h->h.ea2 = 1;
	h->h.tei = m->tei;
	h->u.m3 = 0;
	h->u.m2 = 0;
	h->u.p_f = 0;	/* Poll bit set */
	h->u.ft = Q921_FRAMETYPE_U;

	switch (m->pri_mode) {
	case PRI_NETWORK:
		h->h.c_r = 1;
		break;
	case PRI_CPE:
		h->h.c_r = 0;
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("Don't know how to UI-frame on a type %d node\n", m->pri_mode));
		return -1;
	}

	memcpy(h->u.data, buf, len);

	q921_transmit(m, h, len + 3);

	return 0;
}

static void q921_reject(mtp2_t *m, int pf)
{
	q921_h h;

	Q921_CLEAR_INIT(&h, m->sapi, m->tei);
	h.s.x0 = 0;	/* Always 0 */
	h.s.ss = 2;	/* Reject */
	h.s.ft = 1;	/* Frametype (01) */
	h.s.n_r = m->v_r;	/* Where to start retransmission N(R) */
	h.s.p_f = pf;
	switch (m->pri_mode) {
	case PRI_NETWORK:
		h.h.c_r = 0;
		break;
	case PRI_CPE:
		h.h.c_r = 1;
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("Don't know how to REJ on a type %d node\n", m->pri_mode));
		return;
	}
	CARD_DEBUGF(MTP_DEBUG, ("%d E1:TEI=%d Sending REJ N(R)=%d\n", m->e1_no,m->tei, m->v_r));

	q921_transmit(m, &h, 4);
}

static void q921_rr(mtp2_t *m, int pbit, int cmd)
{
	q921_h h;

	Q921_CLEAR_INIT(&h, m->sapi, m->tei);
	h.s.x0 = 0;	/* Always 0 */
	h.s.ss = 0; /* Receive Ready */
	h.s.ft = 1;	/* Frametype (01) */
	h.s.n_r = m->v_r;	/* N(R) */
	h.s.p_f = pbit;		/* Poll/Final set appropriately */
	switch (m->pri_mode) {
	case PRI_NETWORK:
		if (cmd)
			h.h.c_r = 1;
		else
			h.h.c_r = 0;
		break;
	case PRI_CPE:
		if (cmd)
			h.h.c_r = 0;
		else
			h.h.c_r = 1;
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("Don't know how to RR on a type %d node\r\n", m->pri_mode));
		return;
	}
#if 0	/* Don't flood debug trace with RR if not really looking at Q.921 layer. */
	if (ctrl->debug & PRI_DEBUG_Q921_STATE) {
		pri_message(ctrl, "TEI=%d Sending RR N(R)=%d\n", link->tei, link->v_r);
	}
#endif
	q921_transmit(m, &h, 4);
}

static void transmit_enquiry(mtp2_t *m)
{
	if (!m->own_rx_busy) {
		q921_rr(m, 1, 1);
		m->acknowledge_pending = 0;
		start_t200(m);
	}
}

static void q921_mdl_handle_ptp_error(mtp2_t *m, char error)
{
	switch (error) {
	case 'J':
		/*
		 * This is for the transition to Q921_AWAITING_ESTABLISHMENT.
		 * The event is genereated here rather than where the MDL_ERROR
		 * 'J' is posted because of the potential event conflict with
		 * incoming I-frame information passed to Q.931.
		 */
		/*
		ctrl->schedev = 1;
		ctrl->ev.gen.e = PRI_EVENT_DCHAN_DOWN;
		*/
		break;
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	case 'G':
	case 'H':
	case 'I':
	case 'K':
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: PTP MDL can't handle error of type %c\r\n",m->e1_no, error));
		break;
	}
}

static void q921_mdl_handle_error(mtp2_t *m, char error)
{
	q921_mdl_handle_ptp_error(m, error);
}

static void q921_mdl_handle_error_callback(void *data)
{
	mtp2_t *m = (mtp2_t *)data;

	q921_mdl_handle_error(m, m->mdl_error);

	m->mdl_error = 0;

}

static void q921_mdl_error(mtp2_t *m, char error)
{
	int is_debug_q921_state = 1;

	switch (error) {
	case 'A':
		CARD_DEBUGF(MTP_DEBUG, ("%d E1:"
				"TEI=%d MDL-ERROR (A): Got supervisory frame with F=1 in state %d(%s)\r\n",
			m->e1_no,m->tei, m->q921_state, q921_state2str(m->q921_state)));
		break;
	case 'B':
	case 'E':
		CARD_DEBUGF(MTP_DEBUG, ("%d E1:TEI=%d MDL-ERROR (%c): DM (F=%c) in state %d(%s)\r\n",
			m->e1_no,m->tei, error, (error == 'B') ? '1' : '0',
			m->q921_state, q921_state2str(m->q921_state)));
		break;
	case 'C':
	case 'D':
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: TEI=%d MDL-ERROR (%c): UA (F=%c) in state %d(%s)\r\n",
				m->e1_no,m->tei, error, (error == 'C') ? '1' : '0',
				m->q921_state, q921_state2str(m->q921_state)));

		break;
	case 'F':
		/*
		 * The peer is restarting the link.
		 * Some reasons this might happen:
		 * 1) Our link establishment requests collided.
		 * 2) They got reset.
		 * 3) They could not talk to us for some reason because
		 * their T200 timer expired N200 times.
		 * 4) They got an MDL-ERROR (J).
		 */
		if (is_debug_q921_state) {
			/*
			 * This message is rather annoying and is normal for
			 * reasons 1-3 above.
			 */
			CARD_DEBUGF(MTP_DEBUG, ("%d E1:TEI=%d MDL-ERROR (F): SABME in state %d(%s)\r\n",
				m->e1_no,m->tei, m->q921_state, q921_state2str(m->q921_state)));
		}
		break;
	case 'G':
		/* We could not get a response from the peer. */
		if (is_debug_q921_state) {
			CARD_DEBUGF(MTP_DEBUG, ("%d E1:TEI=%d MDL-ERROR (G): T200 expired N200 times sending SABME in state %d(%s)\r\n",
				m->e1_no,m->tei, m->q921_state, q921_state2str(m->q921_state)));
		}
		break;
	case 'H':
		/* We could not get a response from the peer. */
		if (is_debug_q921_state) {
			CARD_DEBUGF(MTP_DEBUG, ("%d E1:TEI=%d MDL-ERROR (H): T200 expired N200 times sending DISC in state %d(%s)\r\n",
					m->e1_no,m->tei, m->q921_state, q921_state2str(m->q921_state)));
		}
		break;
	case 'I':
		/* We could not get a response from the peer. */
		if (is_debug_q921_state) {
			CARD_DEBUGF(MTP_DEBUG, ("%d E1:TEI=%d MDL-ERROR (I): T200 expired N200 times sending RR/RNR in state %d(%s)\r\n",
					m->e1_no,m->tei, m->q921_state, q921_state2str(m->q921_state)));
		}
		break;
	case 'J':
		/* N(R) not within ack window. */
		CARD_DEBUGF(MTP_DEBUG, ("%d E1:TEI=%d MDL-ERROR (J): N(R) error in state %d(%s)\r\n",
				m->e1_no,m->tei, m->q921_state, q921_state2str(m->q921_state)));
		break;
	case 'K':
		/*
		 * Received a frame reject frame.
		 * The other end does not like what we are doing at all for some reason.
		 */
		CARD_DEBUGF(MTP_DEBUG, ("%d E1:TEI=%d MDL-ERROR (K): FRMR in state %d(%s)\r\n",
				m->e1_no,m->tei, m->q921_state, q921_state2str(m->q921_state)));
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("%d E1:TEI=%d MDL-ERROR (%c): in state %d(%s)\r\n",
				m->e1_no,m->tei, error, m->q921_state, q921_state2str(m->q921_state)));
		break;
	}

	if (m->mdl_error) {
		/* This should not happen. */
		CARD_DEBUGF(MTP_DEBUG, ("%d E1:Trying to queue MDL-ERROR (%c) when MDL-ERROR (%c) is already scheduled\r\n",
			m->e1_no,error, m->mdl_error));
		return;
	}
	m->mdl_error = error;
	//m->mdl_timer = sched_add(q921_sched, 1, q921_mdl_handle_error_callback, m);
	sys_timeout(2, q921_mdl_handle_error_callback, m);
}

static void t200_expire(void *data)
{
	mtp2_t *m = (mtp2_t *)data;

	m->t200_timer = 0;

	switch (m->q921_state) {
	case Q921_MULTI_FRAME_ESTABLISHED:
		m->RC = 0;
		transmit_enquiry(m);
		m->RC++;
		q921_setstate(m, Q921_TIMER_RECOVERY);
		break;
	case Q921_TIMER_RECOVERY:
		/* SDL Flow Figure B.8/Q.921 Page 81 */
		if (m->RC != 3) { //3次超时
#if 0
			if (m->v_s == m->v_a) {
				transmit_enquiry(m);
			}
#else
			/* We are chosing to enquiry by default (to reduce risk of T200 timer errors at the other
			 * side, instead of retransmission of the last I-frame we sent */
			transmit_enquiry(m);
#endif
			m->RC++;
		} else {
			q921_mdl_error(m, 'I');
			q921_establish_data_link(m);
			m->l3_initiated = 0;
			q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
			//告诉第3层链路中断
			//ToDo...
		}
		break;
	case Q921_AWAITING_ESTABLISHMENT:
		if (m->RC != 3) {
			m->RC++;
			q921_send_sabme(m);
			start_t200(m);
		} else {
			q921_check_delay_restart(m);
			q921_discard_iqueue(m);
			//q921_mdl_error(m, 'G');
			q921_setstate(m, Q921_TEI_ASSIGNED);
			/* DL-RELEASE indication */
			//q931_dl_event(m, Q931_DL_EVENT_DL_RELEASE_IND);
		}
		break;
	case Q921_AWAITING_RELEASE:
		if (m->RC != 3) {
			++m->RC;
			q921_send_disc(m,1);
			start_t200(m);
		} else {
			q921_check_delay_restart(m);
			q921_mdl_error(m, 'H');
			/* DL-RELEASE confirm */
			//q931_dl_event(m, Q931_DL_EVENT_DL_RELEASE_CONFIRM);
			q921_setstate(m, Q921_TEI_ASSIGNED);
		}
		break;
	default:
		/* Looks like someone forgot to stop the T200 timer. */
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: T200 expired in state %d(%s)\r\n",
			m->e1_no,m->q921_state, q921_state2str(m->q921_state)));
		break;
	}
}

static void t203_expire(void *data)
{
	mtp2_t *m = (mtp2_t *)data;

	switch (m->q921_state) {
	case Q921_MULTI_FRAME_ESTABLISHED:
		transmit_enquiry(m);
		m->RC = 0;
		q921_setstate(m, Q921_TIMER_RECOVERY);
		break;
	default:
		/* Looks like someone forgot to stop the T203 timer. */
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: T203 expired in state %d(%s)\r\n",
			m->e1_no, m->q921_state, q921_state2str(m->q921_state)));
		break;
	}
}

static void q921_mdl_remove(mtp2_t *m)
{
	int mdl_free_me;

	CARD_DEBUGF(MTP_DEBUG, ("%d E1:MDL-REMOVE: Removing TEI %d\r\n", m->e1_no,m->tei));
	mdl_free_me = 0;

	switch (m->q921_state) {
	case Q921_TEI_ASSIGNED:
		q921_discard_iqueue(m);
		q921_setstate(m, Q921_TEI_UNASSIGNED);
		break;
	case Q921_AWAITING_ESTABLISHMENT:
		q921_discard_iqueue(m);
		/* DL-RELEASE indication */
		q931_dl_event(m, Q931_DL_EVENT_DL_RELEASE_IND);
		stop_t200(m);
		q921_setstate(m, Q921_TEI_UNASSIGNED);
		break;
	case Q921_AWAITING_RELEASE:
		q921_discard_iqueue(m);
		/* DL-RELEASE confirm */
		q931_dl_event(m, Q931_DL_EVENT_DL_RELEASE_CONFIRM);
		stop_t200(m);
		q921_setstate(m, Q921_TEI_UNASSIGNED);
		break;
	case Q921_MULTI_FRAME_ESTABLISHED:
		q921_discard_iqueue(m);
		/* DL-RELEASE indication */
		q931_dl_event(m, Q931_DL_EVENT_DL_RELEASE_IND);
		stop_t200(m);
		stop_t203(m);
		q921_setstate(m, Q921_TEI_UNASSIGNED);
		break;
	case Q921_TIMER_RECOVERY:
		q921_discard_iqueue(m);
		/* DL-RELEASE indication */
		q931_dl_event(m, Q931_DL_EVENT_DL_RELEASE_IND);
		stop_t200(m);
		q921_setstate(m, Q921_TEI_UNASSIGNED);
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: MDL-REMOVE when in state %d(%s)\r\n",
			m->e1_no,m->q921_state, q921_state2str(m->q921_state)));
		return;
	}

	q931_dl_event(m, Q931_DL_EVENT_TEI_REMOVAL);

	/*
	 * Negate the TEI value so debug messages will display a
	 * negated TEI when it is actually unassigned.
	 */
	m->tei = -m->tei;

	m->mdl_free_me = mdl_free_me;
}

#define Q921_TEI_CHECK_MAX_POLLS	2

static void t201_expire(void *data)
{
	mtp2_t *m = (mtp2_t *)m;

	/* Start the TEI check timer. */
	sys_timeout(1000, t201_expire, m);

	++m->t201_expirycnt;
	if (Q921_TEI_CHECK_MAX_POLLS < m->t201_expirycnt) {
		sys_untimeout(t201_expire, m);

		switch (m->tei_check) {
		case Q921_TEI_CHECK_DEAD:
			m->tei_check = Q921_TEI_CHECK_NONE;
			q921_tei_remove(m, m->tei);
			q921_mdl_remove(m);
			break;
		default:
			m->tei_check = Q921_TEI_CHECK_NONE;
			break;
		}

		return;
	}

	if (m->t201_expirycnt == 1) {
		/* First poll.  Setup TEI check state. */
		if (m->q921_state < Q921_TEI_ASSIGNED) {
			/* We do not have a TEI. */
			m->tei_check = Q921_TEI_CHECK_NONE;
		} else {
			/* Mark TEI as dead until proved otherwise. */
			m->tei_check = Q921_TEI_CHECK_DEAD;
		}
	} else {
		/* Subsequent polls.  Setup for new TEI check poll. */
		switch (m->tei_check) {
		case Q921_TEI_CHECK_REPLY:
			m->tei_check = Q921_TEI_CHECK_DEAD_REPLY;
			break;
		default:
			break;
		}
	}
	q921_mdl_send(m, Q921_TEI_IDENTITY_CHECK_REQUEST, 0, Q921_TEI_GROUP, 1);

}

static int is_command(mtp2_t *m, q921_h *h)
{
	int command = 0;
	int c_r = h->s.h.c_r;

	if ((m->pri_mode == PRI_NETWORK && c_r == 0) ||
		(m->pri_mode == PRI_CPE && c_r == 1))
		command = 1;

	return command;
}

/**
static void q921_clear_exception_conditions(mtp2_t *m)
{
	m->own_rx_busy = 0;
	m->peer_rx_busy = 0;
	m->reject_exception = 0;
	m->acknowledge_pending = 0;
}
**/

static int q921_sabme_rx(mtp2_t *m, q921_h *h)
{
	enum Q931_DL_EVENT delay_q931_dl_event;

	switch (m->q921_state) {
	case Q921_TIMER_RECOVERY:
		/* Timer recovery state handling is same as multiframe established */
	case Q921_MULTI_FRAME_ESTABLISHED:
		/* Send Unnumbered Acknowledgement */
		q921_send_ua(m, h->u.p_f);
		q921_clear_exception_conditions(m);
		//q921_mdl_error(m, 'F');
		if (m->v_s != m->v_a) {
			q921_discard_iqueue(m);
			/* DL-ESTABLISH indication */
			delay_q931_dl_event = Q931_DL_EVENT_DL_ESTABLISH_IND;
		} else {
			delay_q931_dl_event = Q931_DL_EVENT_NONE;
		}
		stop_t200(m);
		start_t203(m);
		m->v_s = m->v_a = m->v_r = 0;
#if 0
		//q921_setstate(m, Q921_MULTI_FRAME_ESTABLISHED);
		if (delay_q931_dl_event != Q931_DL_EVENT_NONE) {
			/* Delayed because Q.931 could send STATUS messages. */
			q931_dl_event(m, delay_q931_dl_event);
		}
#endif
		break;
	case Q921_TEI_ASSIGNED:
		restart_timer_stop(m);
		q921_send_ua(m, h->u.p_f);
		q921_clear_exception_conditions(m);
		m->v_s = m->v_a = m->v_r = 0;
		/* DL-ESTABLISH indication */
		delay_q931_dl_event = Q931_DL_EVENT_DL_ESTABLISH_IND;

		start_t203(m);
		q921_setstate(m, Q921_MULTI_FRAME_ESTABLISHED);
		if (delay_q931_dl_event != Q931_DL_EVENT_NONE) {
			/* Delayed because Q.931 could send STATUS messages. */
			q931_dl_event(m, delay_q931_dl_event);
		}
		break;
	case Q921_AWAITING_ESTABLISHMENT:
		q921_send_ua(m, h->u.p_f);
		break;
	case Q921_AWAITING_RELEASE:
		q921_send_dm(m, h->u.p_f);
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("Cannot handle SABME in state %d(%s)\n",
			m->q921_state, q921_state2str(m->q921_state)));
		break;
	}

	return 0;
}

static int q921_disc_rx(mtp2_t *m, q921_h *h)
{
	int res = 0;
	CARD_DEBUGF(MTP_DEBUG, ("%d E1: TEI=%d Got DISC\r\n", m->e1_no,m->tei));

	switch (m->q921_state) {
	case Q921_TEI_ASSIGNED:
	case Q921_AWAITING_ESTABLISHMENT:
		q921_send_dm(m, h->u.p_f);
		break;
	case Q921_AWAITING_RELEASE:
		q921_send_ua(m, h->u.p_f);
		break;
	case Q921_MULTI_FRAME_ESTABLISHED:
	case Q921_TIMER_RECOVERY:
		//res = q921_check_delay_restart(m);
		restart_timer_start(m);
		q921_discard_iqueue(m);
		q921_send_ua(m, h->u.p_f);
		/* DL-RELEASE indication */
		q931_dl_event(m, Q931_DL_EVENT_DL_RELEASE_IND);
		stop_t200(m);
		if (m->q921_state == Q921_MULTI_FRAME_ESTABLISHED)
			stop_t203(m);
		q921_setstate(m, Q921_TEI_ASSIGNED);
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Don't know what to do with DISC in state %d(%s)\r\n",
			m->e1_no,m->q921_state, q921_state2str(m->q921_state)));
		break;
	}

	return res;
}

#if 0
static void q921_mdl_handle_network_error(mtp2_t *m, char error)
{
	switch (error) {
	case 'C':
	case 'D':
	case 'G':
	case 'H':
		q921_mdl_remove(m);
		break;
	case 'A':
	case 'B':
	case 'E':
	case 'F':
	case 'I':
	case 'J':
	case 'K':
		break;
	default:
		vLog(MONITOR_ERR, "%d E1: Network MDL can't handle error of type %c\r\n", m->e1_no,error);
		break;
	}
}

static void q921_mdl_handle_cpe_error(mtp2_t *m, char error)
{
	switch (error) {
	case 'C':
	case 'D':
	case 'G':
	case 'H':
		q921_mdl_remove(m);
		break;
	case 'A':
	case 'B':
	case 'E':
	case 'F':
	case 'I':
	case 'J':
	case 'K':
		break;
	default:
		vLog(MONITOR_ERR, "%d E1: CPE MDL can't handle error of type %c\r\n",m->e1_no, error);
		break;
	}
}
#endif

static int q921_ua_rx(mtp2_t *m, q921_h *h)
{
	enum Q931_DL_EVENT delay_q931_dl_event;

	CARD_DEBUGF(MTP_DEBUG, ("%d E1:TEI=%d Got UA\r\n", m->e1_no,m->tei));

	switch (m->q921_state) {
	case Q921_TEI_ASSIGNED:
	case Q921_MULTI_FRAME_ESTABLISHED:
	case Q921_TIMER_RECOVERY:
		if (h->u.p_f) {
			q921_mdl_error(m, 'C');
		} else {
			q921_mdl_error(m, 'D');
		}
		break;
	case Q921_AWAITING_ESTABLISHMENT:
		if (!h->u.p_f) {
			q921_mdl_error(m, 'D');
			break;
		}

		delay_q931_dl_event = Q931_DL_EVENT_NONE;
		if (!m->l3_initiated) {
			if (m->v_s != m->v_a) {
				q921_discard_iqueue(m);
				/* DL-ESTABLISH indication */
				delay_q931_dl_event = Q931_DL_EVENT_DL_ESTABLISH_IND;
			}
		} else {
			m->l3_initiated = 0;
			/* DL-ESTABLISH confirm */
			delay_q931_dl_event = Q931_DL_EVENT_DL_ESTABLISH_CONFIRM;
		}
		/**
		if (PTP_MODE(ctrl)) {
			ctrl->ev.gen.e = PRI_EVENT_DCHAN_UP;
			res = &ctrl->ev;
		}
		**/
		stop_t200(m);
		start_t203(m);

		m->v_r = m->v_s = m->v_a = 0;

		q921_setstate(m, Q921_MULTI_FRAME_ESTABLISHED);
		if (delay_q931_dl_event != Q931_DL_EVENT_NONE) {
			/* Delayed because Q.931 could send STATUS messages. */
			q931_dl_event(m, delay_q931_dl_event);
		}
		break;
	case Q921_AWAITING_RELEASE:
		if (!h->u.p_f) {
			q921_mdl_error(m, 'D');
		} else {
			//res = q921_check_delay_restart(link);
			/* DL-RELEASE confirm */
			q931_dl_event(m, Q931_DL_EVENT_DL_RELEASE_CONFIRM);
			stop_t200(m);
			q921_setstate(m, Q921_TEI_ASSIGNED);
		}
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Don't know what to do with UA in state %d(%s)\r\n",
			m->e1_no, m->q921_state, q921_state2str(m->q921_state)));
		break;
	}

	return 0;
}

static void q921_enquiry_response(mtp2_t *m)
{
	if (m->own_rx_busy) {
		/* XXX : TODO later sometime */
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Implement me %s: own_rx_busy\r\n", m->e1_no, __FUNCTION__));
		//q921_rnr(link);
	} else {
		q921_rr(m, 1, 0);
	}

	m->acknowledge_pending = 0;
}

static void n_r_error_recovery(mtp2_t *m)
{
	q921_mdl_error(m, 'J');

	q921_establish_data_link(m);

	m->l3_initiated = 0;
}

static void update_v_a(mtp2_t *m, int n_r)
{
	int idealcnt = 0, realcnt = 0;
	int x;

	CARD_DEBUGF(MTP_DEBUG, ("%d E1: -- Got ACK for N(S)=%d to (but not including) N(S)=%d\r\n",m->e1_no,m->v_a, n_r));
	for (x = m->v_a; x != n_r; Q921_INC(x)) {
		idealcnt++;
		realcnt += q921_ack_packet(m, x);
	}
	if (idealcnt != realcnt) {
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Ideally should have ack'd %d frames, but actually ack'd %d.  This is not good.\r\n",
				m->e1_no, idealcnt, realcnt));
		//q921_dump_iqueue_info(m);
	}

	m->v_a = n_r;
	m->retrans_last_acked = m->v_a;
}

/*! \brief Is V(A) <= N(R) <= V(S) ? */
static int n_r_is_valid(mtp2_t *m, int n_r)
{
	int x;

	for (x = m->v_a; x != n_r && x != m->v_s; Q921_INC(x)) {
	}
	if (x != n_r) {
		return 0;
	} else {
		return 1;
	}
}

static int q921_invoke_retransmission(mtp2_t *m, int n_r)
{
#if 0
	struct q921_frame *f;
	struct pri *ctrl;

	ctrl = link->ctrl;

	/*
	 * All acked frames should already have been removed from the queue.
	 * Push back all sent frames.
	 */
	for (f = link->tx_queue; f && f->status == Q921_TX_FRAME_SENT; f = f->next) {
		f->status = Q921_TX_FRAME_PUSHED_BACK;

		/* Sanity check: Is V(A) <= N(S) <= V(S)? */
		if (!n_r_is_valid(link, f->h.n_s)) {
			pri_error(ctrl,
				"Tx Q frame with invalid N(S)=%d.  Must be (V(A)=%d) <= N(S) <= (V(S)=%d)\n",
				f->h.n_s, link->v_a, link->v_s);
		}
	}
	link->v_s = n_r;
	return q921_send_queued_iframes(link);
#endif
	m->v_s = n_r;
	m->retrans_seq = n_r;
	CARD_DEBUGF(MTP_DEBUG, ("%d E1: Start retrans misson, retrans_seq=%d\r\n", m->e1_no,m->retrans_seq));
	return 0;
}

static int timer_recovery_rr_rej_rx(mtp2_t *m, q921_h *h)
{
	/* Figure B.7/Q.921 Page 74 */
	m->peer_rx_busy = 0;

	if (is_command(m, h)) {
		if (h->s.p_f) {
			/* Enquiry response */
			q921_enquiry_response(m);
		}
		if (n_r_is_valid(m, h->s.n_r)) {
			update_v_a(m, h->s.n_r);
		} else {
			goto n_r_error_out;
		}
	} else {
		if (!h->s.p_f) {
			if (n_r_is_valid(m, h->s.n_r)) {
				update_v_a(m, h->s.n_r);
			} else {
				goto n_r_error_out;
			}
		} else {
			if (n_r_is_valid(m, h->s.n_r)) {
				update_v_a(m, h->s.n_r);
				stop_t200(m);
				start_t203(m);
				if(m->v_a == 0 && m->v_r == 0 && m->v_s == 0){
					;
				}else{
					q921_invoke_retransmission(m, h->s.n_r);
				}
				q921_setstate(m, Q921_MULTI_FRAME_ESTABLISHED);
			} else {
				goto n_r_error_out;
			}
		}
	}
	return 0;
n_r_error_out:
	n_r_error_recovery(m);
	q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
	return -1;
}

static int q921_rr_rx(mtp2_t *m, q921_h *h)
{
	int res = 0;
#if 0	/* Don't flood debug trace with RR if not really looking at Q.921 layer. */
	if (ctrl->debug & PRI_DEBUG_Q921_STATE) {
		pri_message(ctrl, "TEI=%d Got RR N(R)=%d\n", link->tei, h->s.n_r);
	}
#endif

	switch (m->q921_state) {
	case Q921_TIMER_RECOVERY:
		res = timer_recovery_rr_rej_rx(m, h);
		break;
	case Q921_MULTI_FRAME_ESTABLISHED:
		/* Figure B.7/Q.921 Page 74 */
		m->peer_rx_busy = 0;

		if (is_command(m, h)) {
			if (h->s.p_f) {
				/* Enquiry response */
				q921_enquiry_response(m);
			}
		} else {
			if (h->s.p_f) {
				q921_mdl_error(m, 'A');
			}
		}

		if (!n_r_is_valid(m, h->s.n_r)) {
			n_r_error_recovery(m);
			q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
		} else {
			if (h->s.n_r == m->v_s) {
				update_v_a(m, h->s.n_r);
				stop_t200(m);
				start_t203(m);
			} else {
				if (h->s.n_r != m->v_a) {
					/* Need to check the validity of n_r as well.. */
					update_v_a(m, h->s.n_r);
					restart_t200(m);
				}
			}
		}
		break;
	case Q921_TEI_ASSIGNED:
	case Q921_AWAITING_ESTABLISHMENT:
	case Q921_AWAITING_RELEASE:
		/*
		 * Ignore this frame.
		 * We likely got reset and the other end has not realized it yet.
		 */
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Don't know what to do with RR in state %d(%s)\r\n",
			m->e1_no, m->q921_state, q921_state2str(m->q921_state)));
		break;
	}

	return res;
}

static int q921_rej_rx(mtp2_t *m, q921_h *h)
{
	int res = 0;

	CARD_DEBUGF(MTP_DEBUG, ("%d E1: TEI=%d Got REJ N(R)=%d\r\n", m->e1_no, m->tei, h->s.n_r));

	switch (m->q921_state) {
	case Q921_TIMER_RECOVERY:
		res = timer_recovery_rr_rej_rx(m, h);
		break;
	case Q921_MULTI_FRAME_ESTABLISHED:
		/* Figure B.7/Q.921 Page 74 */
		m->peer_rx_busy = 0;

		if (is_command(m, h)) {
			if (h->s.p_f) {
				/* Enquiry response */
				q921_enquiry_response(m);
			}
		} else {
			if (h->s.p_f) {
				q921_mdl_error(m, 'A');
			}
		}

		if (!n_r_is_valid(m, h->s.n_r)) {
			n_r_error_recovery(m);
			q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
		} else {
			update_v_a(m, h->s.n_r);
			stop_t200(m);
			start_t203(m);
			q921_invoke_retransmission(m, h->s.n_r);
		}
		break;
	case Q921_TEI_ASSIGNED:
	case Q921_AWAITING_ESTABLISHMENT:
	case Q921_AWAITING_RELEASE:
		/*
		 * Ignore this frame.
		 * We likely got reset and the other end has not realized it yet.
		 */
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Don't know what to do with REJ in state %d(%s)\r\n",
			m->e1_no, m->q921_state, q921_state2str(m->q921_state)));
		break;
	}

	return res;
}

static int q921_frmr_rx(mtp2_t *m, q921_h *h)
{
	int res = 0;

	CARD_DEBUGF(MTP_DEBUG, ("%d E1: TEI=%d Got FRMR\n", m->e1_no, m->tei));

	switch (m->q921_state) {
	case Q921_TIMER_RECOVERY:
	case Q921_MULTI_FRAME_ESTABLISHED:
		q921_mdl_error(m, 'K');
		q921_establish_data_link(m);
		m->l3_initiated = 0;
		q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
		/**
		if (PTP_MODE(ctrl)) {
			ctrl->ev.gen.e = PRI_EVENT_DCHAN_DOWN;
			res = &ctrl->ev;
		}
		**/
		break;
	case Q921_TEI_ASSIGNED:
	case Q921_AWAITING_ESTABLISHMENT:
	case Q921_AWAITING_RELEASE:
		/*
		 * Ignore this frame.
		 * We likely got reset and the other end has not realized it yet.
		 */
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: TEI=%d Ignoring FRMR.\r\n", m->e1_no, m->tei));
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Don't know what to do with FRMR in state %d(%s)\r\n",
			m->e1_no, m->q921_state, q921_state2str(m->q921_state)));
		break;
	}

	return res;
}

static int q921_iframe_rx(mtp2_t *m, q921_h *h, int len)
{
	//int res = 0;
	int delay_q931_receive;

	switch (m->q921_state) {
	case Q921_TIMER_RECOVERY:
	case Q921_MULTI_FRAME_ESTABLISHED:
		delay_q931_receive = 0;
		/* FIXME: Verify that it's a command ... */
		if (m->own_rx_busy) {
			/* DEVIATION: Handle own rx busy */
		} else if (h->i.n_s == m->v_r) {
			Q921_INC(m->v_r);

			m->reject_exception = 0;

			/*
			 * Dump Q.931 message where Q.921 says to queue it to Q.931 so if
			 * Q.921 is dumping its frames they will be in the correct order.
			 */
			//q931_dump(m, h->h.tei, (q931_h *) h->i.data, len - 4, 0);
			delay_q931_receive = 1;

			if (h->i.p_f) {
				q921_rr(m, 1, 0);
				m->acknowledge_pending = 0;
			} else {
				m->acknowledge_pending = 1;
			}
		} else {
			if (m->reject_exception) {
				if (h->i.p_f) {
					q921_rr(m, 1, 0);
					m->acknowledge_pending = 0;
				}
			} else {
				m->reject_exception = 1;
				q921_reject(m, h->i.p_f);
				m->acknowledge_pending = 0;
			}
		}

		if (!n_r_is_valid(m, h->i.n_r)) {
			n_r_error_recovery(m);
			q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
		} else {
			if (m->q921_state == Q921_TIMER_RECOVERY) {
				update_v_a(m, h->i.n_r);
			} else {
				if (m->peer_rx_busy) {
					update_v_a(m, h->i.n_r);
				} else {
					if (h->i.n_r == m->v_s) {
						update_v_a(m, h->i.n_r);
						stop_t200(m);
						start_t203(m);
					} else {
						if (h->i.n_r != m->v_a) {
							update_v_a(m, h->i.n_r);
							reschedule_t200(m);
						}
					}
				}
			}
		}
		if (delay_q931_receive) {
			/* Q.921 has finished processing the frame so we can give it to Q.931 now. */
			/* XXX */
#if 0
			res = q931_receive(m, (q931_h *) h->i.data, len - 2);
#endif
			/*
			if (res != -1 && (res & Q931_RES_HAVEEVENT)) {
				eres = &ctrl->ev;
			}
			*/
		}
		break;
	case Q921_TEI_ASSIGNED:
	case Q921_AWAITING_ESTABLISHMENT:
	case Q921_AWAITING_RELEASE:
		/*
		 * Ignore this frame.
		 * We likely got reset and the other end has not realized it yet.
		 */
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Don't know what to do with an I-frame in state %d(%s)\n",
			m->e1_no, m->q921_state, q921_state2str(m->q921_state)));
		break;
	}

	return 0;
}

static int q921_dm_rx(mtp2_t *m, q921_h *h)
{
	int res = 0;

	CARD_DEBUGF(MTP_DEBUG, ("%d E1: TEI=%d Got DM\r\n",m->e1_no, m->tei));

	switch (m->q921_state) {
	case Q921_TEI_ASSIGNED:
		if (h->u.p_f)
			break;
		/* else */
		restart_timer_stop(m);
		q921_establish_data_link(m);
		m->l3_initiated = 1;
		q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
		break;
	case Q921_AWAITING_ESTABLISHMENT:
		if (!h->u.p_f)
			break;

		res = q921_check_delay_restart(m);
		q921_discard_iqueue(m);
		/* DL-RELEASE indication */
		q931_dl_event(m, Q931_DL_EVENT_DL_RELEASE_IND);
		stop_t200(m);
		q921_setstate(m, Q921_TEI_ASSIGNED);
		break;
	case Q921_AWAITING_RELEASE:
		if (!h->u.p_f)
			break;
		res = q921_check_delay_restart(m);
		/* DL-RELEASE confirm */
		q931_dl_event(m, Q931_DL_EVENT_DL_RELEASE_CONFIRM);
		stop_t200(m);
		q921_setstate(m, Q921_TEI_ASSIGNED);
		break;
	case Q921_MULTI_FRAME_ESTABLISHED:
		if (h->u.p_f) {
			q921_mdl_error(m, 'B');
			break;
		}

		q921_mdl_error(m, 'E');
		q921_establish_data_link(m);
		m->l3_initiated = 0;
		q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
		/*
		if (PTP_MODE(ctrl)) {
			ctrl->ev.gen.e = PRI_EVENT_DCHAN_DOWN;
			res = &ctrl->ev;
		}
		*/
		break;
	case Q921_TIMER_RECOVERY:
		if (h->u.p_f) {
			q921_mdl_error(m, 'B');
		} else {
			q921_mdl_error(m, 'E');
		}
		q921_establish_data_link(m);
		m->l3_initiated = 0;
		q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
		/*
		if (PTP_MODE(ctrl)) {
			ctrl->ev.gen.e = PRI_EVENT_DCHAN_DOWN;
			res = &ctrl->ev;
		}
		*/
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Don't know what to do with DM frame in state %d(%s)\n",
			m->e1_no, m->q921_state, q921_state2str(m->q921_state)));
		break;
	}

	return res;
}

static int q921_rnr_rx(mtp2_t *m, q921_h *h)
{
	int res = 0;

	CARD_DEBUGF(MTP_DEBUG, ("%d E1: TEI=%d Got RNR N(R)=%d\n", m->e1_no, m->tei, h->s.n_r));

	switch (m->q921_state) {
	case Q921_MULTI_FRAME_ESTABLISHED:
		m->peer_rx_busy = 1;
		if (!is_command(m, h)) {
			if (h->s.p_f) {
				q921_mdl_error(m, 'A');
			}
		} else {
			if (h->s.p_f) {
				q921_enquiry_response(m);
			}
		}

		if (!n_r_is_valid(m, h->s.n_r)) {
			n_r_error_recovery(m);
			q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
		} else {
			update_v_a(m, h->s.n_r);
			stop_t203(m);
			restart_t200(m);
		}
		break;
	case Q921_TIMER_RECOVERY:
		/* Q.921 Figure B.8 Q921 (Sheet 6 of 9) Page 85 */
		m->peer_rx_busy = 1;
		if (is_command(m, h)) {
			if (h->s.p_f) {
				q921_enquiry_response(m);
			}
			if (n_r_is_valid(m, h->s.n_r)) {
				update_v_a(m, h->s.n_r);
				break;
			} else {
				n_r_error_recovery(m);
				q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
				break;
			}
		} else {
			if (h->s.p_f) {
				if (n_r_is_valid(m, h->s.n_r)) {
					update_v_a(m, h->s.n_r);
					restart_t200(m);
					q921_invoke_retransmission(m, h->s.n_r);
					q921_setstate(m, Q921_MULTI_FRAME_ESTABLISHED);
					break;
				} else {
					n_r_error_recovery(m);
					q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
					break;
				}
			} else {
				if (n_r_is_valid(m, h->s.n_r)) {
					update_v_a(m, h->s.n_r);
					break;
				} else {
					n_r_error_recovery(m);
					q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
				}
			}
		}
		break;
	case Q921_TEI_ASSIGNED:
	case Q921_AWAITING_ESTABLISHMENT:
	case Q921_AWAITING_RELEASE:
		/*
		 * Ignore this frame.
		 * We likely got reset and the other end has not realized it yet.
		 */
		break;
	default:
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Don't know what to do with RNR in state %d(%s)\r\n",
			m->e1_no, m->q921_state, q921_state2str(m->q921_state)));
		break;
	}

	return res;
}

static void q921_acknowledge_pending_check(mtp2_t *m)
{
	if (m->acknowledge_pending) {
		m->acknowledge_pending = 0;
		q921_rr(m, 0, 0);
	}
}

static void q921_statemachine_check(mtp2_t *m)
{
	switch (m->q921_state) {
	case Q921_MULTI_FRAME_ESTABLISHED:
		//q921_send_queued_iframes(m);
		q921_acknowledge_pending_check(m);
		break;
	case Q921_TIMER_RECOVERY:
		q921_acknowledge_pending_check(m);
		break;
	default:
		break;
	}
}

static int __q921_receive_qualified(mtp2_t *m, q921_h *h, int len)
{
	int res;

	switch (h->h.data[0] & Q921_FRAMETYPE_MASK) {
	case 0:
	case 2:
		res = q921_iframe_rx(m, h, len);
		break;
	case 1:
		switch ((h->s.x0 << 2) | h->s.ss) {
		case 0x00:
			res = q921_rr_rx(m, h);
			break;
 		case 0x01:
			res = q921_rnr_rx(m, h);
			break;
 		case 0x02:
			res = q921_rej_rx(m, h);
			break;
		default:
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: !! XXX Unknown Supervisory frame x0=%d ss=%d, pf=%d, N(R)=%d, V(A)=%d, V(S)=%d XXX\r\n",
				m->e1_no, h->s.x0, h->s.ss, h->s.p_f, h->s.n_r, m->v_a, m->v_s));
			break;
		}
		break;
	case 3:
		if (len < 3) {
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: !! Received short unnumbered frame\r\n", m->e1_no));
			break;
		}
		switch ((h->u.m3 << 2) | h->u.m2) {
		case 0x03:
			res = q921_dm_rx(m, h);
			break;
		case 0x00:
			/* UI-frame XXX*/
#if 0
			q931_dump(m, h->h.tei, (q931_h *) h->u.data, len - 3, 0);
			res = q931_receive(m, (q931_h *) h->u.data, len - 3);
#endif
			break;
		case 0x08:
			res = q921_disc_rx(m, h);
			break;
		case 0x0F:
			/* SABME */
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: TEI=%d Got SABME from %s peer.\r\n",
					m->e1_no, m->tei, h->h.c_r ? "network" : "cpe"));
			if (h->h.c_r) {
				//ctrl->remotetype = PRI_NETWORK;
				if (m->pri_mode == PRI_NETWORK) {
					/* We can't both be networks */
					//res = pri_mkerror(ctrl, "We think we're the network, but they think they're the network, too.");
					break;
				}
			} else {
				//ctrl->remotetype = PRI_CPE;
				if (m->pri_mode == PRI_CPE) {
					/* We can't both be CPE */
					//ev = pri_mkerror(ctrl, "We think we're the CPE, but they think they're the CPE too.\n");
					break;
				}
			}
			res = q921_sabme_rx(m, h);
			break;
		case 0x0C:
			res = q921_ua_rx(m, h);
			break;
		case 0x11:
			res = q921_frmr_rx(m, h);
			break;
		case 0x17:
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: !! XID frames not supported\r\n", m->e1_no));
			break;
		default:
			CARD_DEBUGF(MTP_DEBUG, ("%d E1: !! Don't know what to do with u-frame (m3=%d, m2=%d)\r\n",
				m->e1_no,h->u.m3, h->u.m2));
			break;
		}
		break;
	}

	q921_statemachine_check(m);

	return res;
}

static void q921_establish_data_link(mtp2_t *m)
{
	q921_discard_iqueue(m);
	q921_clear_exception_conditions(m);
	m->RC = 0;
	m->s_h = m->s_t = 0;
	stop_t203(m);
	reschedule_t200(m);
	q921_send_sabme(m);
}

void q921_start(mtp2_t *m)
{
	/* PTP mode, no need for TEI management junk */
	q921_establish_data_link(m);
	m->l3_initiated = 1;
	q921_setstate(m, Q921_AWAITING_ESTABLISHMENT);
}

int q921_receive(mtp2_t *m, q921_h *h, int len)
{
	int ret = 0;
	int debug = 1;

	q921_dump(m, h, len, debug, 0);

	/* Check some reject conditions -- Start by rejecting improper ea's */
	if (h->h.ea1 || !h->h.ea2) {
		return -2;
	}

	if (h->h.sapi == Q921_SAPI_LAYER2_MANAGEMENT) {
		return q921_mdl_receive(m, &h->u, len);
	}

	if (h->h.tei == Q921_TEI_GROUP && h->h.sapi != Q921_SAPI_CALL_CTRL) {
		CARD_DEBUGF(MTP_DEBUG, ("%d E1: Do not handle group messages to services other than MDL or CALL CTRL\r\n",
				m->e1_no));
		return -1;
	}


	if( h->h.sapi == m->sapi
			&& (h->h.tei == m->tei || h->h.tei == Q921_TEI_GROUP)) {
		ret = __q921_receive_qualified(m, h, len);
	}

	return ret;
}

