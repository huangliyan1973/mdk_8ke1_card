#include <string.h>
#include "main.h"
#include "cmsis_os.h"

#include "lwip/apps/snmp.h"
#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/ip.h"
#include "lwip/udp.h"
#include "snmp_msg.h"
#include "lwip/sys.h"
#include "snmp_asn1.h"

#include "usart.h"
#include "eeprom.h"
#include "server_interface.h"

#define LOG_TAG              "snmp"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

extern ip4_addr_t local_addr;

static const u32_t  e1_oid_base[] = { 1, 3, 6, 1, 4, 1, 1373, 1, 3, 1, 1, 1 };
static const u8_t   e1_oid_len = (u8_t)LWIP_ARRAYSIZE(e1_oid_base);

#define SNMP_8KE1_ENTERPRISE_OID_LEN    14
#define SNMP_8KE1_ENTERPRISE_OID    {1, 3, 6, 1, 4, 1, 1373, 1, 3, 1, 1, 1, 4, 0}

static const struct snmp_obj_id  snmp_8ke1_enterprise_oid_default = {SNMP_8KE1_ENTERPRISE_OID_LEN, SNMP_8KE1_ENTERPRISE_OID};
static const struct snmp_obj_id *snmp_8ke1_enterprise_oid         = &snmp_8ke1_enterprise_oid_default;


#define SNMP_8KE1_MTP2_OID_LEN    14
#define SNMP_8KE1_MTP2_OID    {1, 3, 6, 1, 4, 1, 1373, 1, 3, 1, 1, 3, 4, 0}

static const struct snmp_obj_id  snmp_8ke1_mtp2_oid_default = {SNMP_8KE1_MTP2_OID_LEN, SNMP_8KE1_MTP2_OID};
static const struct snmp_obj_id *snmp_8ke1_mtp2_oid         = &snmp_8ke1_mtp2_oid_default;


static const char *snmp_community = "public";

extern card_heart_t  hb_msg;
static struct snmp_varbind  trap_var;
static struct snmp_varbind  mtp2_trap_var;

struct snmp_msg_trap { 
  /* source IP address, raw network order format */
  ip_addr_t sip;
  /* snmp_version */
  u32_t snmp_version;

  u32_t error_index;
  u32_t error_status;

  u32_t request_id;
  /* output trap lengths used in ASN encoding */
  /* encoding pdu length */
  u16_t pdulen;
  /* encoding community length */
  u16_t comlen;
  /* encoding sequence length */
  u16_t seqlen;
  /* encoding varbinds sequence length */
  u16_t vbseqlen;
};

static u16_t snmp_trap_varbind_sum(struct snmp_msg_trap *trap, struct snmp_varbind *varbinds);
static u16_t snmp_trap_header_sum(struct snmp_msg_trap *trap, u16_t vb_len);
static err_t snmp_trap_header_enc(struct snmp_msg_trap *trap, struct snmp_pbuf_stream *pbuf_stream);
static err_t snmp_trap_varbind_enc(struct snmp_msg_trap *trap, struct snmp_pbuf_stream *pbuf_stream, struct snmp_varbind *varbinds);

#define TRAP_BUILD_EXEC(code) \
  if ((code) != ERR_OK) { \
    LWIP_DEBUGF(SNMP_DEBUG, ("SNMP error during creation of outbound trap frame!")); \
    return ERR_ARG; \
  }

void *snmp_8ke1_traps_handle;
ip4_addr_t  omc, sn0, sn1;

struct snmp_trap_dst {
  /* destination IP address in network order */
  ip_addr_t dip;
  /* set to 0 when disabled, >0 when enabled */
  u8_t enable;
};

#define SNMP_8KE1_TRAP_DESTINATIONS  3

static struct snmp_trap_dst trap_dst[SNMP_8KE1_TRAP_DESTINATIONS];

extern uint8_t  card_id;

struct value_len_map {
    void *value;
    uint16_t len;
};

static const struct value_len_map  tone_par_map[] = {
    {NULL, 0},  /* 0 */
    { (void *)e1_params.tone_cadence0,  18},
    { (void *)e1_params.tone_cadence1,  18},
    { (void *)e1_params.tone_cadence2,  18},
    { (void *)e1_params.tone_cadence3,  18},
    { (void *)e1_params.tone_cadence4,  18},
    { (void *)e1_params.tone_cadence5,  18},
    { (void *)e1_params.tone_cadence6,  18},
    { (void *)e1_params.tone_cadence7,  18},
    { (void *)e1_params.reason_to_tone, 16},
    { (void *)e1_params.dtmf_mark_space, 2},
    { (void *)&e1_params.tone_src,       1}
};


#define PARSE_EXEC(code, retValue) \
  if ((code) != ERR_OK) { \
    LWIP_DEBUGF(SNMP_DEBUG, ("Malformed ASN.1 detected.\n")); \
    snmp_stats.inasnparseerrs++; \
    return retValue; \
  }

#define PARSE_ASSERT(cond, retValue) \
  if (!(cond)) { \
    LWIP_DEBUGF(SNMP_DEBUG, ("SNMP parse assertion failed!: " # cond)); \
    snmp_stats.inasnparseerrs++; \
    return retValue; \
  }

#define BUILD_EXEC(code, retValue) \
  if ((code) != ERR_OK) { \
    LWIP_DEBUGF(SNMP_DEBUG, ("SNMP error during creation of outbound frame!: " # code)); \
    return retValue; \
  }
  
#define IF_PARSE_EXEC(code)   PARSE_EXEC(code, ERR_ARG)
#define IF_PARSE_ASSERT(code) PARSE_ASSERT(code, ERR_ARG)

static err_t snmp_parse_inbound_frame(struct snmp_request *request)
{
    struct snmp_pbuf_stream pbuf_stream;
    struct snmp_asn1_tlv tlv;
    
    s32_t parent_tlv_value_len;
    s32_t s32_value;
    err_t err;
    
    IF_PARSE_EXEC(snmp_pbuf_stream_init(&pbuf_stream, request->inbound_pbuf, 0, request->inbound_pbuf->tot_len));
    
    IF_PARSE_EXEC(snmp_asn1_dec_tlv(&pbuf_stream, &tlv));
    IF_PARSE_ASSERT((tlv.type == SNMP_ASN1_TYPE_SEQUENCE) && (tlv.value_len == pbuf_stream.length));
    parent_tlv_value_len = tlv.value_len;
    
    /*version*/
    IF_PARSE_EXEC(snmp_asn1_dec_tlv(&pbuf_stream, &tlv));
    IF_PARSE_ASSERT(tlv.type == SNMP_ASN1_TYPE_INTEGER);
    parent_tlv_value_len -= SNMP_ASN1_TLV_LENGTH(tlv);
    IF_PARSE_ASSERT(parent_tlv_value_len > 0);
    
    IF_PARSE_EXEC(snmp_asn1_dec_s32t(&pbuf_stream, tlv.value_len, &s32_value));
    if ((s32_value != SNMP_VERSION_1) && (s32_value != SNMP_VERSION_2c)){
        snmp_stats.inbadversions++;
        return ERR_ARG;
    }
    request->version = (u8_t)s32_value;
    
    /*community */
    IF_PARSE_EXEC(snmp_asn1_dec_tlv(&pbuf_stream, &tlv));
    IF_PARSE_ASSERT(tlv.type == SNMP_ASN1_TYPE_OCTET_STRING);
    parent_tlv_value_len -= SNMP_ASN1_TLV_LENGTH(tlv);
    IF_PARSE_ASSERT(parent_tlv_value_len > 0);
    
    err = snmp_asn1_dec_raw(&pbuf_stream, tlv.value_len, request->community, &request->community_strlen, SNMP_MAX_COMMUNITY_STR_LEN);
    if (err == ERR_MEM) {
        request->community_strlen = 0;
        snmp_pbuf_stream_seek(&pbuf_stream, tlv.value_len);
    }else {
        IF_PARSE_ASSERT(err == ERR_OK);
    }
    request->community[request->community_strlen] = 0;
    
    /*PDU type */
    IF_PARSE_EXEC(snmp_asn1_dec_tlv(&pbuf_stream, &tlv));
    IF_PARSE_ASSERT(tlv.value_len <= pbuf_stream.length);
    request->inbound_padding_len = pbuf_stream.length - tlv.value_len;
    parent_tlv_value_len = tlv.value_len;
    
    switch(tlv.type) {
        case (SNMP_ASN1_CLASS_CONTEXT | SNMP_ASN1_CONTENTTYPE_CONSTRUCTED | SNMP_ASN1_CONTEXT_PDU_GET_REQ):
            /* GetRequest PDU */
            snmp_stats.ingetrequests++;
            break;
        case (SNMP_ASN1_CLASS_CONTEXT | SNMP_ASN1_CONTENTTYPE_CONSTRUCTED | SNMP_ASN1_CONTEXT_PDU_GET_NEXT_REQ):
            /* GetNextRequest PDU */
            snmp_stats.ingetnexts++;
            break;
        case (SNMP_ASN1_CLASS_CONTEXT | SNMP_ASN1_CONTENTTYPE_CONSTRUCTED | SNMP_ASN1_CONTEXT_PDU_GET_BULK_REQ):
            /* GetBulkRequest PDU */
            if (request->version < SNMP_VERSION_2c) {
            /* RFC2089: invalid, drop packet */
            return ERR_ARG;
            }
            break;
        case (SNMP_ASN1_CLASS_CONTEXT | SNMP_ASN1_CONTENTTYPE_CONSTRUCTED | SNMP_ASN1_CONTEXT_PDU_SET_REQ):
            /* SetRequest PDU */
            snmp_stats.insetrequests++;
            break;
        case (SNMP_ASN1_CLASS_CONTEXT | SNMP_ASN1_CONTENTTYPE_CONSTRUCTED | SNMP_ASN1_CONTEXT_PDU_V2_TRAP):
            snmp_stats.intraps++;
            break;
        default:
            LWIP_DEBUGF(SNMP_DEBUG, ("Unknown/Invalid SNMP PDU type received: %d", tlv.type)); \
            return ERR_ARG;
    }
    request->request_type = tlv.type & SNMP_ASN1_DATATYPE_MASK;
    request->request_out_type = (SNMP_ASN1_CLASS_CONTEXT | SNMP_ASN1_CONTENTTYPE_CONSTRUCTED | SNMP_ASN1_CONTEXT_PDU_GET_RESP);
    
    /* validate community */
    if (request->community_strlen == 0) {
        snmp_stats.inbadcommunitynames++;
        return ERR_ARG;
    } else {
        if (strncmp(snmp_community, (const char *)request->community, SNMP_MAX_COMMUNITY_STR_LEN) != 0) {
            snmp_stats.inbadcommunitynames++;
            return ERR_ARG;
        }
    }
    
    /* request ID */
    IF_PARSE_EXEC(snmp_asn1_dec_tlv(&pbuf_stream, &tlv));
    IF_PARSE_ASSERT(tlv.type == SNMP_ASN1_TYPE_INTEGER);
    parent_tlv_value_len -= SNMP_ASN1_TLV_LENGTH(tlv);
    IF_PARSE_ASSERT(parent_tlv_value_len > 0);
    
    IF_PARSE_EXEC(snmp_asn1_dec_s32t(&pbuf_stream, tlv.value_len, &request->request_id));
    
    /* error status / non-repeaters */
    IF_PARSE_EXEC(snmp_asn1_dec_tlv(&pbuf_stream, &tlv));
    IF_PARSE_ASSERT(tlv.type == SNMP_ASN1_TYPE_INTEGER);
    parent_tlv_value_len -= SNMP_ASN1_TLV_LENGTH(tlv);
    IF_PARSE_ASSERT(parent_tlv_value_len > 0);
    
    IF_PARSE_EXEC(snmp_asn1_dec_s32t(&pbuf_stream, tlv.value_len, &s32_value));
    IF_PARSE_ASSERT(s32_value == SNMP_ERR_NOERROR);
    
    /* error index / max-repetitions */
    IF_PARSE_EXEC(snmp_asn1_dec_tlv(&pbuf_stream, &tlv));
    IF_PARSE_ASSERT(tlv.type == SNMP_ASN1_TYPE_INTEGER);
    parent_tlv_value_len -= SNMP_ASN1_TLV_LENGTH(tlv);
    IF_PARSE_ASSERT(parent_tlv_value_len > 0);
    
    IF_PARSE_EXEC(snmp_asn1_dec_s32t(&pbuf_stream, tlv.value_len, &request->error_index));
    IF_PARSE_ASSERT(s32_value == 0);
    
    /* varbind-list type */
    IF_PARSE_EXEC(snmp_asn1_dec_tlv(&pbuf_stream, &tlv));
    IF_PARSE_ASSERT((tlv.type == SNMP_ASN1_TYPE_SEQUENCE) && (tlv.value_len <= pbuf_stream.length));
    
    request->inbound_varbind_offset = pbuf_stream.offset;
    request->inbound_varbind_len = pbuf_stream.length - request->inbound_padding_len;
    
    snmp_vb_enumerator_init(&(request->inbound_varbind_enumerator), request->inbound_pbuf, request->inbound_varbind_offset, request->inbound_varbind_len);
    
    return ERR_OK;
}

#define OF_BUILD_EXEC(code) BUILD_EXEC(code, ERR_ARG)

static err_t snmp_prepare_outbound_frame(struct snmp_request *request)
{
    struct snmp_asn1_tlv tlv;
    struct snmp_pbuf_stream *pbuf_stream = &(request->outbound_pbuf_stream);
    
    request->outbound_pbuf = pbuf_alloc(PBUF_TRANSPORT, 1024, PBUF_RAM);
    if (request->outbound_pbuf == NULL) {
        return ERR_MEM;
    }
    
    snmp_pbuf_stream_init(pbuf_stream, request->outbound_pbuf, 0, request->outbound_pbuf->tot_len);
    
    /* sequence */
    SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_SEQUENCE, 3, 0);
    OF_BUILD_EXEC(snmp_ans1_enc_tlv(pbuf_stream, &tlv));
    
    /* version */
    SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_INTEGER, 0, 0);
    snmp_asn1_enc_s32t_cnt(request->version, &tlv.value_len);
    OF_BUILD_EXEC(snmp_ans1_enc_tlv(pbuf_stream, &tlv));
    OF_BUILD_EXEC(snmp_asn1_enc_s32t(pbuf_stream, tlv.value_len, request->version));
    
    /*community */
    SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_OCTET_STRING, 0, request->community_strlen);
    OF_BUILD_EXEC(snmp_ans1_enc_tlv(pbuf_stream, &tlv));
    OF_BUILD_EXEC(snmp_asn1_enc_raw(pbuf_stream, request->community, request->community_strlen));
    
    /* PDU sequence */
    request->outbound_pdu_offset = pbuf_stream->offset;
    SNMP_ASN1_SET_TLV_PARAMS(tlv, request->request_out_type, 3, 0);
    OF_BUILD_EXEC(snmp_ans1_enc_tlv(pbuf_stream, &tlv));
    
    /* request ID */
    SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_INTEGER, 0, 0);
    snmp_asn1_enc_s32t_cnt(request->request_id, &tlv.value_len);
    OF_BUILD_EXEC(snmp_ans1_enc_tlv(pbuf_stream, &tlv));
    OF_BUILD_EXEC(snmp_asn1_enc_s32t(pbuf_stream, tlv.value_len, request->request_id));
    
    /* error status */
    SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_INTEGER, 0, 1);
    OF_BUILD_EXEC( snmp_ans1_enc_tlv(pbuf_stream, &tlv) );
    request->outbound_error_status_offset = pbuf_stream->offset;
    OF_BUILD_EXEC( snmp_pbuf_stream_write(pbuf_stream, 0) );

    /* error index */
    SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_INTEGER, 0, 1);
    OF_BUILD_EXEC( snmp_ans1_enc_tlv(pbuf_stream, &tlv) );
    request->outbound_error_index_offset = pbuf_stream->offset;
    OF_BUILD_EXEC( snmp_pbuf_stream_write(pbuf_stream, 0) );

    /* 'VarBindList' sequence */
    SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_SEQUENCE, 3, 0);
    OF_BUILD_EXEC( snmp_ans1_enc_tlv(pbuf_stream, &tlv) );
    
    
    request->outbound_varbind_offset = pbuf_stream->offset;

    return ERR_OK;
}

static snmp_err_t snmp_oid_check(struct snmp_varbind *vb)
{
    if (((int)(vb->oid.len) - (int)e1_oid_len) < 3) {
        return SNMP_ERR_NOSUCHINSTANCE;
    }
    
    if (snmp_oid_compare(vb->oid.id, e1_oid_len, e1_oid_base, e1_oid_len) != 0) {
        return SNMP_ERR_NOSUCHINSTANCE;
    }
    
    return SNMP_ERR_NOERROR;
}


static snmp_err_t snmp_get_value(struct snmp_varbind *vb)
{
    u32_t instance;
    u32_t sub_oid;
    
    snmp_err_t  err = snmp_oid_check(vb);
    if (err != SNMP_ERR_NOERROR) {
        return err;
    }
    
    sub_oid = vb->oid.id[e1_oid_len + 1];
    instance = vb->oid.id[e1_oid_len + 2];

    //oid = vb->oid.id + e1_oid_len + 2;
    LOG_I("Snmp Get sub_oid=%d, instance=%d\n", sub_oid, instance);
    
    vb->type = SNMP_ASN1_TYPE_OCTET_STRING;
    
    switch (vb->oid.id[e1_oid_len]) {
        case 2:  /* Config */
            if (sub_oid <= 13 && sub_oid != 7) {
                if (instance < E1_CARDS) {
                    vb->value_len = 1;
                    if (sub_oid == 1) {
                        *(u8_t *)(vb->value) = e1_params.e1_enable[instance];
                    } else if (sub_oid == 2) {
                        *(u8_t *)(vb->value) = e1_params.e1_l2_alarm_enable[instance];
                    } else if (sub_oid == 3) {
                        *(u8_t *)(vb->value) = e1_params.e1_port_type[instance];
                    } else if (sub_oid == 4) {
                        *(u8_t *)(vb->value) = e1_params.isdn_port_type[instance];
                    } else if (sub_oid == 5) {
                        *(u8_t *)(vb->value) = e1_params.pll_src[instance];
                    } else if (sub_oid == 6) {
                        *(u8_t *)(vb->value) = e1_params.crc4_enable[instance];
                    } else if (sub_oid == 13) {
                        *(u8_t *)(vb->value) = e1_params.no1_enable[instance];
                    } else if (sub_oid == 8) {
                        *(u8_t *)(vb->value) = e1_params.mtp2_error_check[instance];
                    } else {
                        return SNMP_ERR_NOSUCHINSTANCE;
                    }
                } else {
                    return SNMP_ERR_NOSUCHINSTANCE;
                }
            } else if (sub_oid == 7) {
                if (instance < LWIP_ARRAYSIZE(tone_par_map)) {
                    vb->value = tone_par_map[instance].value;
                    vb->value_len = tone_par_map[instance].len;
                } else {
                    return SNMP_ERR_NOSUCHINSTANCE;
                }
            } else if (sub_oid == 16) {  
                if (instance == 1) { /* 16.1 */
                    uint8_t e1_no = (uint8_t)vb->oid.id[e1_oid_len + 3] & 0x7;
                    vb->value_len = (uint16_t)sizeof(e1_params.pc_magic[0]);
                    vb->value = (void *)(&e1_params.pc_magic[e1_no].type);
                } else {
                    return SNMP_ERR_NOSUCHINSTANCE;
                }
            }
            
            break;
        case 3:  /* Command */
            if (sub_oid == 1) {
                vb->value = (void *)&ram_params.init_flag;
                vb->value_len = 1;
            } else {
                return SNMP_ERR_NOSUCHINSTANCE;
            }
            break;
        default:
            return SNMP_ERR_NOSUCHINSTANCE;
    }
    
    return SNMP_ERR_NOERROR;
}

static snmp_err_t snmp_set_value(struct snmp_varbind *vb)
{
    u8_t instance, value;
    u32_t sub_oid;
    
    snmp_err_t  err = snmp_oid_check(vb);
    if (err != SNMP_ERR_NOERROR) {
        return err;
    }
    
    sub_oid = vb->oid.id[e1_oid_len + 1];
    instance = vb->oid.id[e1_oid_len + 2];
    value = *(u8_t *)(vb->value);

    LOG_I("Snmp set sub_oid=%d, instance=%d, value=%x\n", sub_oid, instance, value);
  
    switch (vb->oid.id[e1_oid_len]) {
        case 2: /*config */
            if (sub_oid <= 13 && sub_oid != 7) {
                if (instance < E1_CARDS) {
                    if (sub_oid == 1) {
                        if (value != e1_params.e1_enable[instance] && instance == card_id) {
                            update_e1_enable(value);
                            //e1_params.e1_enable[instance] = value ;
                        }
                        
                    } else if (sub_oid == 2) {
                        e1_params.e1_l2_alarm_enable[instance] = value ;
                    } else if (sub_oid == 3) {
                        e1_params.e1_port_type[instance] = value ;
                    } else if (sub_oid == 4) {
                        e1_params.isdn_port_type[instance] = value ;
                    } else if (sub_oid == 5) {
                        e1_params.pll_src[instance] = value ;
                    } else if (sub_oid == 6) {
                        e1_params.crc4_enable[instance] = value ;
                    } else if (sub_oid == 13) {                       
                        e1_params.no1_enable[instance] = value ;
                        if (instance == card_id) {
                            update_no1_e1(value);
                        }
                    } else if (sub_oid == 8) {
                        e1_params.mtp2_error_check[instance] = value ;
                    } else {
                        return SNMP_ERR_NOSUCHINSTANCE;
                    } 
                } else {
                    return SNMP_ERR_NOSUCHINSTANCE;
                }
            } else if (sub_oid == 7) {
                if (instance < LWIP_ARRAYSIZE(tone_par_map)) {
                    memcpy (tone_par_map[instance].value, vb->value, vb->value_len);
                } else {
                    return SNMP_ERR_NOSUCHINSTANCE;
                }
            } else if (sub_oid == 16) {  
                if (instance == 1) { /* 16.1 */
                    uint8_t e1_no = (uint8_t)vb->oid.id[e1_oid_len + 3] & 0x7;
                    memcpy ((void *)(&e1_params.pc_magic[e1_no].type), vb->value, vb->value_len);
                } else {
                    return SNMP_ERR_NOSUCHINSTANCE;
                }
            }
            break;
        case 3: /*command */
            if (sub_oid == 1) {
                ram_params.init_flag = value;
                /* Todo: 重启或EEPROM初始化 */
            } else {
                return SNMP_ERR_NOSUCHINSTANCE;
            }
            break;
        default:
            return SNMP_ERR_NOSUCHINSTANCE;
    }
    
    return SNMP_ERR_NOERROR;
}

static void snmp_process_varbind(struct snmp_request *request, struct snmp_varbind *vb)
{
    err_t err;
    
    request->error_status = snmp_get_value(vb);
    if (request->error_status == SNMP_ERR_NOERROR) {
        err = snmp_append_outbound_varbind(&(request->outbound_pbuf_stream), vb);
        if (err == ERR_OK) {
            request->error_status = SNMP_ERR_NOERROR;
        } else {
            request->error_status = SNMP_ERR_GENERROR;
        }
    } 
}

static err_t snmp_process_get_request(struct snmp_request *request)
{
    snmp_vb_enumerator_err_t err;
    struct snmp_varbind vb;
    vb.value = request->value_buffer;
    
    while (request->error_status == SNMP_ERR_NOERROR) {
        err = snmp_vb_enumerator_get_next(&request->inbound_varbind_enumerator, &vb);
        if (err == SNMP_VB_ENUMERATOR_ERR_OK) {
            if ((vb.type == SNMP_ASN1_TYPE_NULL) && (vb.value_len == 0)) {
                snmp_process_varbind(request, &vb);
            } else {
                request->error_status = SNMP_ERR_GENERROR;
            }
        } else if (err == SNMP_VB_ENUMERATOR_ERR_EOVB) {
            break;
        } else if (err == SNMP_VB_ENUMERATOR_ERR_ASN1ERROR) {
            return ERR_ARG;
        } else {
            request->error_status = SNMP_ERR_GENERROR;
        }
    }
    
    return ERR_OK;
}

static err_t snmp_complete_outbound_frame(struct snmp_request *request)
{
    struct snmp_asn1_tlv tlv;
    u16_t frame_size;
    u8_t outbound_padding = 0;
    
    if (request->request_type == SNMP_ASN1_CONTEXT_PDU_SET_REQ) {
        switch (request->error_status) {
            case SNMP_ERR_NOSUCHINSTANCE:
                request->error_status = SNMP_ERR_NOTWRITABLE;
                break;
            default:
                break;
        }
    }
    
    if (request->error_status >= SNMP_VARBIND_EXCEPTION_OFFSET) {
        LWIP_DEBUGF(SNMP_DEBUG, ("snmp_complete_outbound_frame() > Found v2 request with varbind exception code stored as error status!\n"));
        return ERR_ARG;
    }

    if ((request->error_status != SNMP_ERR_NOERROR) || (request->request_type == SNMP_ASN1_CONTEXT_PDU_SET_REQ)) {
        /* all inbound vars are returned in response without any modification for error responses and successful set requests*/
        struct snmp_pbuf_stream inbound_stream;
        OF_BUILD_EXEC( snmp_pbuf_stream_init(&inbound_stream, request->inbound_pbuf, request->inbound_varbind_offset, request->inbound_varbind_len) );
        OF_BUILD_EXEC( snmp_pbuf_stream_init(&(request->outbound_pbuf_stream), request->outbound_pbuf, request->outbound_varbind_offset, request->outbound_pbuf->tot_len - request->outbound_varbind_offset) );
        OF_BUILD_EXEC( snmp_pbuf_stream_writeto(&inbound_stream, &(request->outbound_pbuf_stream), 0) );
      }

    frame_size = request->outbound_pbuf_stream.offset;
      
    /* complete missing length in 'Message' sequence ; 'Message' tlv is located at the beginning (offset 0) */
    SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_SEQUENCE, 3, frame_size + outbound_padding - 1 - 3); /* - type - length_len(fixed, see snmp_prepare_outbound_frame()) */
    OF_BUILD_EXEC( snmp_pbuf_stream_init(&(request->outbound_pbuf_stream), request->outbound_pbuf, 0, request->outbound_pbuf->tot_len) );
    OF_BUILD_EXEC( snmp_ans1_enc_tlv(&(request->outbound_pbuf_stream), &tlv) );
      
    /* complete missing length in 'PDU' sequence */
    SNMP_ASN1_SET_TLV_PARAMS(tlv, request->request_out_type, 3,
                           frame_size - request->outbound_pdu_offset - 1 - 3); /* - type - length_len(fixed, see snmp_prepare_outbound_frame()) */
    OF_BUILD_EXEC( snmp_pbuf_stream_seek_abs(&(request->outbound_pbuf_stream), request->outbound_pdu_offset) );
    OF_BUILD_EXEC( snmp_ans1_enc_tlv(&(request->outbound_pbuf_stream), &tlv) );
 
    /* process and encode final error status */
    if (request->error_status != 0) {
        u16_t len;
        snmp_asn1_enc_s32t_cnt(request->error_status, &len);
        if (len != 1) {
          /* error, we only reserved one byte for it */
          return ERR_ARG;
        }
        OF_BUILD_EXEC( snmp_pbuf_stream_seek_abs(&(request->outbound_pbuf_stream), request->outbound_error_status_offset) );
        OF_BUILD_EXEC( snmp_asn1_enc_s32t(&(request->outbound_pbuf_stream), len, request->error_status) );

        if (request->error_index == 0) {
          /* set index to varbind where error occured (if not already set before, e.g. during GetBulk processing) */
          request->error_index = request->inbound_varbind_enumerator.varbind_count;
        }
    } else {
        if (request->request_type == SNMP_ASN1_CONTEXT_PDU_SET_REQ) {
          snmp_stats.intotalsetvars += request->inbound_varbind_enumerator.varbind_count;
        } else {
          snmp_stats.intotalreqvars += request->inbound_varbind_enumerator.varbind_count;
        }
    }

    /* encode final error index*/
    if (request->error_index != 0) {
        u16_t len;
        snmp_asn1_enc_s32t_cnt(request->error_index, &len);
        if (len != 1) {
          /* error, we only reserved one byte for it */
          return ERR_VAL;
        }
        OF_BUILD_EXEC( snmp_pbuf_stream_seek_abs(&(request->outbound_pbuf_stream), request->outbound_error_index_offset) );
        OF_BUILD_EXEC( snmp_asn1_enc_s32t(&(request->outbound_pbuf_stream), len, request->error_index) );
    }

    /* complete missing length in 'VarBindList' sequence ; 'VarBindList' tlv is located directly before varbind offset */
    SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_SEQUENCE, 3, frame_size - request->outbound_varbind_offset);
    OF_BUILD_EXEC( snmp_pbuf_stream_seek_abs(&(request->outbound_pbuf_stream), request->outbound_varbind_offset - 1 - 3) ); /* - type - length_len(fixed, see snmp_prepare_outbound_frame()) */
    OF_BUILD_EXEC( snmp_ans1_enc_tlv(&(request->outbound_pbuf_stream), &tlv) );

    pbuf_realloc(request->outbound_pbuf, frame_size + outbound_padding);

    snmp_stats.outgetresponses++;
    snmp_stats.outpkts++;

    return ERR_OK;
}

static err_t snmp_process_set_request(struct snmp_request *request)
{
    snmp_vb_enumerator_err_t err;
    struct snmp_varbind vb;
    vb.value = request->value_buffer;
    
    LWIP_DEBUGF(SNMP_DEBUG, ("SNMP set request\n"));
    
    if (request->error_status == SNMP_ERR_NOERROR) {
        snmp_vb_enumerator_init(&request->inbound_varbind_enumerator, request->inbound_pbuf, request->inbound_varbind_offset, request->inbound_varbind_len);
        while (request->error_status == SNMP_ERR_NOERROR) {
            err = snmp_vb_enumerator_get_next(&request->inbound_varbind_enumerator, &vb);
            if (err == SNMP_VB_ENUMERATOR_ERR_OK) {
                if (snmp_set_value(&vb) != SNMP_ERR_NOERROR) {
                    request->error_status = SNMP_ERR_COMMITFAILED;
                }
            } else if (err == SNMP_VB_ENUMERATOR_ERR_EOVB) {
                /* no more varbinds in request */
                break;
            } else {
                request->error_status = SNMP_ERR_GENERROR;
            }
        }
    }
    
    return ERR_OK;
}

static void snmp_process_trap_request(struct snmp_request *request)
{
    snmp_vb_enumerator_err_t err;
    struct snmp_varbind vb;
    vb.value = request->value_buffer;
    
    LWIP_DEBUGF(SNMP_DEBUG, ("SNMP trap request\n"));
    
    if (request->error_status == SNMP_ERR_NOERROR) {
        err = snmp_vb_enumerator_get_next(&request->inbound_varbind_enumerator, &vb);
        if (err == SNMP_VB_ENUMERATOR_ERR_OK) {
            if ((vb.type == SNMP_ASN1_TYPE_OCTET_STRING) && (vb.value_len >= sizeof(heart_t))) {
                heart_t *heart_msg = (heart_t *)vb.value;
                if (ip4_addr_cmp(request->source_ip, &omc)) {
                    ram_params.timestamp = PP_HTONL(heart_msg->timestamp);
                    LOG_I("Got OMC Server timestamp = %d", ram_params.timestamp);
                }
            }
        } 
    }
}

static void snmp_8ke1_receive(struct netconn *handle, struct pbuf *p, const ip_addr_t *source_ip, u16_t port)
{
    err_t err;
    struct snmp_request request;
    
    memset(&request, 0, sizeof(request));
    request.handle = handle;
    request.source_ip = source_ip;
    request.source_port = port;
    request.inbound_pbuf = p;
    
    err = snmp_parse_inbound_frame(&request);
    if (err == ERR_OK) {
        if (request.request_type == SNMP_ASN1_CONTEXT_PDU_V2_TRAP) {
            snmp_process_trap_request(&request);
            return;
        }
        err = snmp_prepare_outbound_frame(&request);
        if (err == ERR_OK) {
            if (request.error_status == SNMP_ERR_NOERROR) {
                if (request.request_type == SNMP_ASN1_CONTEXT_PDU_GET_REQ) {
                    err = snmp_process_get_request(&request);
                } else if (request.request_type == SNMP_ASN1_CONTEXT_PDU_SET_REQ) {
                    err = snmp_process_set_request(&request);
                } 
            }
            
            if (err == ERR_OK) {
                err = snmp_complete_outbound_frame(&request);
                
                if (err == ERR_OK) {
                    err = snmp_sendto(request.handle, request.outbound_pbuf, request.source_ip, request.source_port);
                }
            }
        }
    }
    
    if (request.outbound_pbuf != NULL) {
        pbuf_free(request.outbound_pbuf);
    }
}

static void snmp_netconn_thread(void *arg)
{
  struct netconn *conn;
  struct netbuf *buf;
  err_t err;
  LWIP_UNUSED_ARG(arg);

  conn = netconn_new(NETCONN_UDP);
  netconn_bind(conn, IP_ADDR_ANY, SNMP_UDP_PORT);

  LWIP_ERROR("snmp_netconn: invalid conn", (conn != NULL), return;);

  snmp_8ke1_traps_handle = conn;
    
  period_10s_proc(NULL);
  LOG_I("SNMP thread start!");
    
  do {
    err = netconn_recv(conn, &buf);

    if (err == ERR_OK) {
      snmp_8ke1_receive(conn, buf->p, &buf->addr, buf->port);
    }

    if (buf != NULL) {
      netbuf_delete(buf);
    }
  } while (1);
}

static void trap_var_init(void)
{
    hb_msg_init();

    memset(&trap_var, 0, sizeof(struct snmp_varbind));

    trap_var.type = SNMP_ASN1_TYPE_OCTET_STRING;
    snmp_oid_assign(&trap_var.oid, snmp_8ke1_enterprise_oid->id, snmp_8ke1_enterprise_oid->len);
    trap_var.value_len = sizeof(card_heart_t);
    trap_var.value = (void *)&hb_msg;
}


static void mtp2_trap_var_init(void)
{
    mtp2_heart_msg_init();
    memset(&mtp2_trap_var, 0, sizeof(struct snmp_varbind));

    mtp2_trap_var.type = SNMP_ASN1_TYPE_OCTET_STRING;
    snmp_oid_assign(&mtp2_trap_var.oid, snmp_8ke1_mtp2_oid->id, snmp_8ke1_mtp2_oid->len);
    mtp2_trap_var.value_len = sizeof(mtp2_heart_t);
    mtp2_trap_var.value = (void *)&mtp2_heart_msg;
}

#define SNMP_CARD_STACK_SIZE    350*4

void snmp_8ke1_init(void)
{   
    IP4_ADDR(&sn0, 172, 18, 98, 1);
    IP4_ADDR(&sn1, 172, 18, 99, 1);
    IP4_ADDR(&omc, 172, 18, 128, 1);

    ip4_addr_copy(trap_dst[0].dip, sn0);
    ip4_addr_copy(trap_dst[1].dip, sn1);
    ip4_addr_copy(trap_dst[2].dip, omc);

    trap_var_init();
    mtp2_trap_var_init();

    sys_thread_new("snmp_netconn", snmp_netconn_thread, NULL, SNMP_CARD_STACK_SIZE, osPriorityNormal);
}


err_t snmp_send_8ke1_trap(struct snmp_varbind *varbinds)
{
    static u32_t request_id = 1;

    struct snmp_msg_trap trap_msg;
    struct snmp_trap_dst *td;
    struct pbuf *p;
    u16_t i, tot_len;
    err_t err = ERR_OK;

    if (snmp_8ke1_traps_handle == NULL) {
        return ERR_RTE;
    }
    
    trap_msg.snmp_version = SNMP_VERSION_2c;
    trap_msg.request_id = request_id++;
    trap_msg.error_index = trap_msg.error_status = 0;
    
    for ( i = 0, td = &trap_dst[0]; i < SNMP_8KE1_TRAP_DESTINATIONS; i++, td++) {
        if ((td->enable != 0) && !ip_addr_isany(&td->dip)) {
            if (snmp_get_local_ip_for_dst(snmp_8ke1_traps_handle, &td->dip, &trap_msg.sip)) {
                /* pass 0, calculate length fields */
                tot_len = snmp_trap_varbind_sum(&trap_msg, varbinds);
                tot_len = snmp_trap_header_sum(&trap_msg, tot_len);

                /* allocate pbuf(s) */
                p = pbuf_alloc(PBUF_TRANSPORT, tot_len, PBUF_RAM);
                if (p != NULL) {
                    struct snmp_pbuf_stream pbuf_stream;
                    snmp_pbuf_stream_init(&pbuf_stream, p, 0, tot_len);

                    /* pass 1, encode packet into the pbuf(s) */
                    snmp_trap_header_enc(&trap_msg, &pbuf_stream);
                    snmp_trap_varbind_enc(&trap_msg, &pbuf_stream, varbinds);

                    snmp_stats.outtraps++;
                    snmp_stats.outpkts++;

                    /** send to the TRAP destination */
                    snmp_sendto(snmp_8ke1_traps_handle, p, &td->dip, SNMP_UDP_PORT);
                    
                    pbuf_free(p);
                } else {
                    err = ERR_MEM;
                }
            } else {
                err = ERR_RTE;
            }
        }
    }
    return err;
}

void send_trap_msg(u8_t dst_flag)
{
    dst_flag = dst_flag % 3;

    for (u8_t i = 0; i < 3; i++) {
        if (i == dst_flag) {
            trap_dst[i].enable = 1;
        } else {
            trap_dst[i].enable = 0;
        }
    }
    snmp_send_8ke1_trap(&trap_var);
}

void send_mtp2_trap_msg(void)
{
    if (plat_no == 0) {
        trap_dst[0].enable = 1;
        trap_dst[1].enable = 0;
    } else {
        trap_dst[0].enable = 0;
        trap_dst[1].enable = 1;
    }
    trap_dst[2].enable = 1;

    snmp_send_8ke1_trap(&mtp2_trap_var);
}


static u16_t
snmp_trap_varbind_sum(struct snmp_msg_trap *trap, struct snmp_varbind *varbinds)
{
  struct snmp_varbind *varbind;
  u16_t tot_len;
  u8_t tot_len_len;

  tot_len = 0;
  varbind = varbinds;
  while (varbind != NULL) {
    struct snmp_varbind_len len;

    if (snmp_varbind_length(varbind, &len) == ERR_OK) {
      tot_len += 1 + len.vb_len_len + len.vb_value_len;
    }

    varbind = varbind->next;
  }

  trap->vbseqlen = tot_len;
  snmp_asn1_enc_length_cnt(trap->vbseqlen, &tot_len_len);
  tot_len += 1 + tot_len_len;

  return tot_len;
}

/**
 * Sums trap header field lengths from tail to head and
 * returns trap_header_lengths for second encoding pass.
 *
 * @param trap Trap message
 * @param vb_len varbind-list length
 * @return the required length for encoding the trap header
 */
static u16_t
snmp_trap_header_sum(struct snmp_msg_trap *trap, u16_t vb_len)
{
  u16_t tot_len;
  u16_t len;
  u8_t lenlen;

  tot_len = vb_len;

  /* error index */
  snmp_asn1_enc_u32t_cnt(trap->error_index, &len);
  snmp_asn1_enc_length_cnt(len, &lenlen);
  tot_len += 1 + len + lenlen;

  /* error status */
  snmp_asn1_enc_s32t_cnt(trap->error_status, &len);
  snmp_asn1_enc_length_cnt(len, &lenlen);
  tot_len += 1 + len + lenlen;

  /* request id */
  snmp_asn1_enc_s32t_cnt(trap->request_id, &len);
  snmp_asn1_enc_length_cnt(len, &lenlen);
  tot_len += 1 + len + lenlen;

  /* PDU length */
  trap->pdulen = tot_len;
  snmp_asn1_enc_length_cnt(trap->pdulen, &lenlen);
  tot_len += 1 + lenlen;

  /* commnunity string */
  trap->comlen = (u16_t)LWIP_MIN(strlen(snmp_community), 0xFFFF);
  snmp_asn1_enc_length_cnt(trap->comlen, &lenlen);
  tot_len += 1 + lenlen + trap->comlen;

  /* version */
  snmp_asn1_enc_s32t_cnt(trap->snmp_version, &len);
  snmp_asn1_enc_length_cnt(len, &lenlen);
  tot_len += 1 + len + lenlen;

  trap->seqlen = tot_len;
  snmp_asn1_enc_length_cnt(trap->seqlen, &lenlen);
  tot_len += 1 + lenlen;

  return tot_len;
}

static err_t
snmp_trap_varbind_enc(struct snmp_msg_trap *trap, struct snmp_pbuf_stream *pbuf_stream, struct snmp_varbind *varbinds)
{
  struct snmp_asn1_tlv tlv;
  struct snmp_varbind *varbind;

  varbind = varbinds;

  SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_SEQUENCE, 0, trap->vbseqlen);
  TRAP_BUILD_EXEC( snmp_ans1_enc_tlv(pbuf_stream, &tlv) );

  while (varbind != NULL) {
    TRAP_BUILD_EXEC( snmp_append_outbound_varbind(pbuf_stream, varbind) );

    varbind = varbind->next;
  }

  return ERR_OK;
}

/**
 * Encodes trap header from head to tail.
 */
static err_t
snmp_trap_header_enc(struct snmp_msg_trap *trap, struct snmp_pbuf_stream *pbuf_stream)
{
  struct snmp_asn1_tlv tlv;

  /* 'Message' sequence */
  SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_SEQUENCE, 0, trap->seqlen);
  TRAP_BUILD_EXEC( snmp_ans1_enc_tlv(pbuf_stream, &tlv) );

  /* version */
  SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_INTEGER, 0, 0);
  snmp_asn1_enc_s32t_cnt(trap->snmp_version, &tlv.value_len);
  TRAP_BUILD_EXEC( snmp_ans1_enc_tlv(pbuf_stream, &tlv) );
  TRAP_BUILD_EXEC( snmp_asn1_enc_s32t(pbuf_stream, tlv.value_len, trap->snmp_version) );

  /* community */
  SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_OCTET_STRING, 0, trap->comlen);
  TRAP_BUILD_EXEC( snmp_ans1_enc_tlv(pbuf_stream, &tlv) );
  TRAP_BUILD_EXEC( snmp_asn1_enc_raw(pbuf_stream,  (const u8_t *)snmp_community, trap->comlen) );

  /* 'PDU' sequence */
  SNMP_ASN1_SET_TLV_PARAMS(tlv, (SNMP_ASN1_CLASS_CONTEXT | SNMP_ASN1_CONTENTTYPE_CONSTRUCTED | SNMP_ASN1_CONTEXT_PDU_V2_TRAP), 0, trap->pdulen);
  TRAP_BUILD_EXEC( snmp_ans1_enc_tlv(pbuf_stream, &tlv) );

  /* request ID */
  SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_INTEGER, 0, 0);
  snmp_asn1_enc_s32t_cnt(trap->request_id, &tlv.value_len);
  TRAP_BUILD_EXEC( snmp_ans1_enc_tlv(pbuf_stream, &tlv) );
  TRAP_BUILD_EXEC( snmp_asn1_enc_s32t(pbuf_stream, tlv.value_len, trap->request_id) );

  /* error status */
  SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_INTEGER, 0, 0);
  snmp_asn1_enc_s32t_cnt(trap->error_status, &tlv.value_len);
  TRAP_BUILD_EXEC( snmp_ans1_enc_tlv(pbuf_stream, &tlv) );
  TRAP_BUILD_EXEC( snmp_asn1_enc_s32t(pbuf_stream, tlv.value_len, trap->error_status) );

  /* error index */
  SNMP_ASN1_SET_TLV_PARAMS(tlv, SNMP_ASN1_TYPE_INTEGER, 0, 0);
  snmp_asn1_enc_s32t_cnt(trap->error_index, &tlv.value_len);
  TRAP_BUILD_EXEC( snmp_ans1_enc_tlv(pbuf_stream, &tlv) );
  TRAP_BUILD_EXEC( snmp_asn1_enc_s32t(pbuf_stream, tlv.value_len, trap->error_index) );

  return ERR_OK;
}
