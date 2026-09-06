#include "airmon_core.h"
#include <math.h>
#include <string.h>

const char *const am_keys[AM_METRICS] = {"temperature", "humidity", "pm1", "pm2_5", "pm10", "co2", "voc_index", "nox_index"};
const char *const am_units[AM_METRICS] = {"°C", "%", "µg/m³", "µg/m³", "µg/m³", "ppm", "index", "index"};
const char *const am_state_names[5] = {"absent", "warming_up", "valid", "stale", "failed"};
const int am_scales[AM_METRICS] = {100,100,1,1,1,1,1,1};

uint8_t am_crc8(const uint8_t *p, size_t n) {
    uint8_t crc = 0xff;
    for (size_t i=0;i<n;i++) {
        crc ^= p[i];
        for (unsigned j=0;j<8;j++) crc = (crc & 0x80) ? (uint8_t)((crc<<1)^0x31) : (uint8_t)(crc<<1);
    }
    return crc;
}
bool am_sensirion_words(const uint8_t *p,size_t n,uint16_t *out) {
    if (!p || !out || !n || n%3) return false;
    for(size_t i=0;i<n;i+=3) if(am_crc8(p+i,2)!=p[i+2]) return false;
    for(size_t i=0;i<n;i+=3) out[i/3]=(uint16_t)((p[i]<<8)|p[i+1]);
    return true;
}
bool am_decode_sht4x(const uint8_t p[6],float *t,float *rh) {
    uint16_t w[2];
    if(!am_sensirion_words(p,6,w)) return false;
    *t=-45.0f+175.0f*w[0]/65535.0f;
    *rh=fminf(100,fmaxf(0,-6.0f+125.0f*w[1]/65535.0f));
    return true;
}
bool am_decode_pmsa003i(const uint8_t *p,size_t n,uint16_t pm[3]) {
    if(n!=32 || p[0]!=0x42 || p[1]!=0x4d || p[2]!=0 || p[3]!=28 || p[29]!=0) return false;
    uint16_t sum=0; for(size_t i=0;i<30;i++) sum+=p[i];
    if(sum!=(uint16_t)((p[30]<<8)|p[31])) return false;
    /* Atmospheric/environmental concentrations, not CF=1 factory values. */
    for(size_t i=0;i<3;i++) pm[i]=(uint16_t)((p[10+2*i]<<8)|p[11+2*i]);
    return true;
}
bool am_reading_current(const am_reading *r,uint64_t now,uint32_t age) {
    return r->state==AM_VALID && isfinite(r->value) && now>=r->at_ms && now-r->at_ms<=age;
}
static void finish_minute(am_history *h,uint32_t utc_s,uint64_t now_ms) {
    am_history_point *p=&h->points[h->head];
    p->uptime_s=(uint32_t)(h->minute*60);
    uint64_t elapsed=now_ms/1000-p->uptime_s;
    p->utc_s=utc_s>elapsed ? utc_s-(uint32_t)elapsed : 0;
    for(int i=0;i<AM_METRICS;i++) {
        p->values[i]=h->samples[i] ? (int32_t)llround(h->sum[i]/h->samples[i]*am_scales[i]) : AM_INVALID;
        h->sum[i]=0; h->samples[i]=0;
    }
    h->head=(h->head+1)%AM_HISTORY;
    if(h->count<AM_HISTORY) h->count++;
}
void am_history_sample(am_history *h,uint64_t ms,uint32_t utc,const am_reading r[AM_METRICS]) {
    uint64_t minute=ms/60000;
    if(!h->started) {h->minute=minute;h->started=true;}
    if(minute<h->minute) return;
    if(minute-h->minute>AM_HISTORY) {
        memset(h,0,sizeof(*h));h->started=true;h->minute=minute-AM_HISTORY;
    }
    while(h->minute<minute) {finish_minute(h,utc,ms);h->minute++;}
    for(int i=0;i<AM_METRICS;i++) if(am_reading_current(&r[i],ms,i==AM_CO2?15000:5000)) {
        h->sum[i]+=r[i].value;h->samples[i]++;
    }
}
const am_history_point *am_history_at(const am_history *h,size_t i) {
    return i<h->count ? &h->points[(h->head+AM_HISTORY-h->count+i)%AM_HISTORY] : NULL;
}
void am_touch_map(const float a[6],float rx,float ry,float *x,float *y) {
    *x=a[0]*rx+a[1]*ry+a[2];*y=a[3]*rx+a[4]*ry+a[5];
}
bool am_touch_calibrate(const float r[4][2],const float s[4][2],float a[6]) {
    float x0=r[0][0],y0=r[0][1],x1=r[1][0],y1=r[1][1],x2=r[2][0],y2=r[2][1];
    float d=x0*(y1-y2)+x1*(y2-y0)+x2*(y0-y1);
    if(!isfinite(d)||fabsf(d)<1000) return false;
    for(int j=0;j<2;j++) {
        float z0=s[0][j],z1=s[1][j],z2=s[2][j];
        a[j*3]=(z0*(y1-y2)+z1*(y2-y0)+z2*(y0-y1))/d;
        a[j*3+1]=(z0*(x2-x1)+z1*(x0-x2)+z2*(x1-x0))/d;
        a[j*3+2]=(z0*(x1*y2-x2*y1)+z1*(x2*y0-x0*y2)+z2*(x0*y1-x1*y0))/d;
    }
    float x,y;am_touch_map(a,r[3][0],r[3][1],&x,&y);
    return isfinite(x)&&isfinite(y)&&hypotf(x-s[3][0],y-s[3][1])<12;
}
bool am_constant_time_equal(const char *a,const char *b,size_t n) {
    if(!a || !b) return false;
    size_t la=strnlen(a,n),lb=strnlen(b,n);
    unsigned diff=(unsigned)(la^lb);
    for(size_t i=0;i<n;i++) diff|=(unsigned)((i<la?(unsigned char)a[i]:0)^(i<lb?(unsigned char)b[i]:0));
    return diff==0 && la>0 && la<n;
}
