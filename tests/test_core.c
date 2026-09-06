#include "airmon_core.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void sensors(void) {
    const uint8_t example[]={0xbe,0xef};
    assert(am_crc8(example,2)==0x92); /* Sensirion published CRC vector */
    uint8_t sht[]={0x80,0,0xa2,0x80,0,0xa2};
    float t,rh; assert(am_decode_sht4x(sht,&t,&rh));
    assert(fabsf(t-42.5013f)<.001f && fabsf(rh-56.501f)<.001f);
    for(int bit=0;bit<48;bit++) {
        uint8_t bad[6];memcpy(bad,sht,6);bad[bit/8]^=1<<(bit%8);
        assert(!am_decode_sht4x(bad,&t,&rh));
    }
    uint16_t unchanged[2]={123,456};sht[5]^=1;
    assert(!am_sensirion_words(sht,6,unchanged));assert(unchanged[0]==123&&unchanged[1]==456);
    uint8_t pm[32]={0x42,0x4d,0,28,0,99,0,98,0,97,0,3,0,12,1,44};
    unsigned sum=0;for(int i=0;i<30;i++)sum+=pm[i];pm[30]=sum>>8;pm[31]=sum;
    uint16_t values[3];assert(am_decode_pmsa003i(pm,32,values));
    assert(values[0]==3&&values[1]==12&&values[2]==300);
    for(int bit=0;bit<256;bit++) {
        uint8_t bad[32];memcpy(bad,pm,32);bad[bit/8]^=1<<(bit%8);
        assert(!am_decode_pmsa003i(bad,32,values));
    }
    assert(!am_decode_pmsa003i(pm,31,values));
    am_reading v={.state=AM_VALID,.value=NAN,.at_ms=100};
    assert(!am_reading_current(&v,100,10));v.value=1;
    assert(!am_reading_current(&v,99,10));assert(am_reading_current(&v,110,10));assert(!am_reading_current(&v,111,10));
    for(int s=0;s<5;s++)if(s!=AM_VALID){v.state=s;assert(!am_reading_current(&v,100,10));}
}
static void history(void) {
    static am_history h;am_reading r[AM_METRICS]={0};
    r[0]=(am_reading){20,1000,AM_VALID};am_history_sample(&h,1000,1700000001,r);
    r[0]=(am_reading){22,2000,AM_VALID};am_history_sample(&h,2000,1700000002,r);
    am_history_sample(&h,60000,1700000060,r);
    const am_history_point *p=am_history_at(&h,0);
    assert(h.count==1&&p->values[0]==2100&&p->values[1]==AM_INVALID);
    assert(p->uptime_s==0&&p->utc_s==1700000000);
    am_history_sample(&h,180000,1700000180,r);
    assert(h.count==3&&am_history_at(&h,1)->values[0]==AM_INVALID);
    memset(&h,0,sizeof(h));
    for(unsigned minute=0;minute<=1500;minute++) {
        r[0]=(am_reading){(float)minute,minute*60000ULL,AM_VALID};
        am_history_sample(&h,minute*60000ULL,0,r);
    }
    assert(h.count==1440&&am_history_at(&h,0)->uptime_s==3600);
    assert(am_history_at(&h,1439)->values[0]==149900&&am_history_at(&h,1440)==NULL);
    am_history_sample(&h,10000ULL*60000,0,r);
    assert(h.count==1440&&am_history_at(&h,1439)->values[0]==AM_INVALID);
    size_t head=h.head;am_history_sample(&h,0,0,r);assert(h.head==head);
}
static void touch_and_auth(void) {
    float raw[4][2]={{3900,300},{3900,3700},{200,300},{200,3700}};
    const float screen[4][2]={{30,30},{290,30},{30,210},{290,210}};
    float a[6],x,y;assert(am_touch_calibrate(raw,screen,a));
    am_touch_map(a,2050,2000,&x,&y);assert(fabsf(x-160)<.01&&fabsf(y-120)<.01);
    raw[3][0]=2000;assert(!am_touch_calibrate(raw,screen,a));
    memcpy(raw[1],raw[0],sizeof(raw[0]));assert(!am_touch_calibrate(raw,screen,a));
    assert(am_constant_time_equal("0123456789abcdef0123456789abcdef","0123456789abcdef0123456789abcdef",33));
    assert(!am_constant_time_equal("abc","abd",33));assert(!am_constant_time_equal("abc","abcd",33));
    assert(!am_constant_time_equal("","",33));assert(!am_constant_time_equal("abc","abc",3));
    assert(!am_constant_time_equal(NULL,"abc",33));
}
static void query_numbers(void) {
    uint32_t out=37;
    const char *invalid[]={"","-1","+1"," 1","1 ","1.5","180oops","0x10","nan","4294967296","999999999999999999999999"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){assert(!am_parse_u32(invalid[i],&out));assert(out==37);}
    assert(!am_parse_u32(NULL,&out));assert(!am_parse_u32("1",NULL));
    assert(am_parse_u32("0",&out)&&out==0);
    assert(am_parse_u32("000180",&out)&&out==180);
    assert(am_parse_u32("4294967295",&out)&&out==UINT32_MAX);
}
int main(void){sensors();history();touch_and_auth();query_numbers();puts("PASS: CRC vectors, 304 corrupt frames, environmental PM, freshness, history averaging/gaps/24h rollover, touch, authentication and strict query numbers");}
