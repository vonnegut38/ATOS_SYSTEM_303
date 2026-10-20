/* ASND-v1 client; all exchanges go through the kernel, never raw NatFeats.
 * Existing verified U8 cache is expanded to S16LE. No resampling/mixing in
 * the application loop during playback. Protocol v1 STOP/PLAY restarts.
 */
#include "app_abi.h"
#include "gui_app.h"
#include "modplay_sound.h"
#include "atos_sound_upload.h"
static as_client client;
static uint8_t reply[AS_MAX_REPLY];
static int connected, uncertain, refused;
static uint32_t device_caps;

/* Freestanding support, kept private to the MODPLAY binary. */
#ifndef MODPLAY_HOST_TEST
void *memset(void *p,int c,size_t n){uint8_t *d=p;while(n--)*d++=(uint8_t)c;return p;}
int memcmp(const void *a,const void *b,size_t n){const uint8_t *x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;++x;++y;}return 0;}
#endif
static int exchange(void *ctx,const uint8_t *req,size_t n,uint8_t *rep,size_t cap,size_t *got)
{
    int32_t rc;(void)ctx;refused=0;
    rc=gui_call(ATOS_CALL_SOUND_XCHG,(uint32_t)n,(uint32_t)cap,(void *)req,rep);
    /* Broker BUSY is a bare positive status, NOT a packet length. */
    if(rc==AS_E_BUSY){refused=1;return AS_IO_UNAVAILABLE;}
    if(rc<(int32_t)AS_HEADER || (size_t)rc>cap)return AS_IO_UNAVAILABLE;
    *got=(size_t)rc;return 0;
}
static int checked(int rc){if(rc<0 && !refused)uncertain=1;return rc;}
static int command(uint16_t op,const void *p,size_t n)
{size_t got;if(uncertain)return AS_IO_UNAVAILABLE;return checked(as_client_command(&client,op,p,n,reply,sizeof reply,&got));}
int modsound_available(void)
{uint32_t info[4]={0};return gui_call(ATOS_CALL_SOUND_INFO,0,0,info,0)>=0 && info[0];}
int modsound_open(uint32_t *budget)
{
    int rc;
    if(uncertain)return AS_IO_UNAVAILABLE;
    if(!modsound_available())return AS_IO_UNAVAILABLE;
    as_client_init(&client,(as_transport){exchange,0});
    rc=command(AS_HELLO,0,0);
    /* No successful HELLO: do not RESET somebody else's device. */
    if(rc)return rc;
    connected=1;
    rc=command(AS_GET_CAPS,0,0);if(rc)return rc;
    if((as_get32(reply)&(AS_CAP_PCM|AS_CAP_LOOP))!=(AS_CAP_PCM|AS_CAP_LOOP))return AS_E_UNSUPPORTED;
    device_caps=as_get32(reply);*budget=as_get32(reply+16);return 0;
}
int modsound_close(void)
{
    int rc=0;
    /* On uncertain delivery only process teardown may recover. Never change
       a command under an unacknowledged sequence number. */
    if(uncertain)return AS_IO_UNAVAILABLE;
    if(connected)rc=command(AS_RESET,0,0);
    if(!rc){connected=0;}return rc;
}
static uint32_t module_id,module_offset,module_crc;
static uint8_t module_payload[AS_MAX_PAYLOAD];
int modsound_module_begin(uint32_t bytes)
{
    int rc=command(AS_GET_CAPS,0,0);if(rc)return rc;
    if(!(as_get32(reply)&AS_CAP_TRACKER))return AS_E_UNSUPPORTED;
    as_put32(module_payload,0);as_put32(module_payload+4,bytes);as_put32(module_payload+8,0);
    rc=command(AS_TRACKER_LOAD,module_payload,12);
    if(!rc){module_id=as_get32(reply);module_offset=0;module_crc=~0u;}
    return rc;
}
int modsound_module_data(const uint8_t *p,unsigned n)
{
    if(!module_id||n>AS_MAX_PAYLOAD-12u||!n)return AS_E_ARGUMENT;
    as_put32(module_payload,1);as_put32(module_payload+4,module_id);as_put32(module_payload+8,module_offset);
    for(unsigned i=0;i<n;++i)module_payload[12+i]=p[i];
    int rc=command(AS_TRACKER_LOAD,module_payload,n+12u);
    if(!rc){if(as_get32(reply)!=module_id||as_get32(reply+4)!=module_offset+n)return checked(AS_IO_PROTOCOL);
        module_offset+=n;module_crc=as_crc32_update(module_crc,p,n);}
    return rc;
}
int modsound_module_commit(void)
{
    as_put32(module_payload,2);as_put32(module_payload+4,module_id);as_put32(module_payload+8,~module_crc);
    return command(AS_TRACKER_LOAD,module_payload,12);
}
int modsound_module_control(unsigned action)
{uint8_t p[4];as_put32(p,action);return command(AS_TRACKER_CONTROL,p,4);}
int modsound_module_status(uint8_t out[208])
{int rc=command(AS_TRACKER_STATUS,0,0);if(!rc)for(unsigned i=0;i<208;++i)out[i]=reply[i];return rc;}

int modsound_module_can_seek(void){return connected && (device_caps&AS_CAP_TRACKER_SEEK)!=0;}
int modsound_module_seek(uint32_t ms)
{uint8_t p[4];if(!modsound_module_can_seek())return AS_E_UNSUPPORTED;as_put32(p,ms);return command(AS_TRACKER_SEEK,p,4);}
