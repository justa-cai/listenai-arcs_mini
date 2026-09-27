#!/usr/bin/env python3
"""Compile the actual ADB service against a small host transport harness."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HEADERS = {
    'FreeRTOS.h': '#include <stdint.h>\ntypedef uint32_t StackType_t;\n#define taskENTER_CRITICAL() ((void)0)\n#define taskEXIT_CRITICAL() ((void)0)\n',
    'task.h': 'static unsigned uxTaskGetStackHighWaterMark(void *p) { (void)p; return 1024; }\n',
    'lisa_mem.h': '#include <stdlib.h>\n#define lisa_mem_alloc malloc\n#define lisa_mem_free free\n#define lisa_mem_calloc calloc\n#define lisa_mem_realloc realloc\n',
    'lisa_thread.h': 'typedef struct { const char *name; unsigned stack_size, priority; } lisa_thread_attr_t;\nstatic void *lisa_thread_create(const lisa_thread_attr_t *a, void (*fn)(void *), void *p) { (void)a;(void)fn;(void)p;return 0; }\n',
    'lisa_log.h': '#define LISA_LOGI(...) ((void)0)\n',
    'mbedtls/md5.h': '#include <stdint.h>\nstatic void mbedtls_md5(const uint8_t *p, unsigned n, uint8_t *d) { (void)p;(void)n;for(unsigned i=0;i<16;i++)d[i]=0; }\n',
    'sys_init.h': '#define SYS_INIT(...)\n',
}
HARNESS = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "apps/arcs-mini/miniapp/miniapp_adb.c"
struct miniapp_source { unsigned refs; size_t size; char data[65536]; };
static struct miniapp_source original, replacement;
static struct miniapp_source *active;
static unsigned sends, closes;
static uint8_t output[70000];
static size_t output_size;
static uint32_t last_id, last_size;
static uint32_t stat_mode;
miniapp_source_t *miniapp_source_acquire(void) { if(active)active->refs++;return active; }
void miniapp_source_release(miniapp_source_t *s) { if(s){assert(s->refs);s->refs--;} }
const char *miniapp_source_data(const miniapp_source_t *s,size_t *size){*size=s->size;return s->data;}
bool miniapp_install_begin(void){return true;}
void miniapp_install_end(void){}
int miniapp_install(const miniapp_package_t *p,const char *s,char *e,size_t n){(void)p;(void)s;(void)e;(void)n;return 0;}
void adb_write(uint32_t local,uint32_t remote,uint8_t *data,uint32_t size){
    assert(local==1&&remote==2&&size>=8);sends++;
    last_id=read_u32(data);last_size=read_u32(data+4);
    if(last_id==SYNC_STAT){assert(size==16);stat_mode=read_u32(data+4);}
    if(last_id==SYNC_DATA){assert(size==last_size+8);assert(output_size+last_size<=sizeof(output));memcpy(output+output_size,data+8,last_size);output_size+=last_size;}
}
void adb_close(uint32_t l,uint32_t r){assert(l==1&&r==2);closes++;}
void adb_packet_free(adb_packet_t *p){free(p);}
int adb_service_hd_register(const struct adb_service_handle *h){(void)h;return 0;}
static void feed(struct adb_service *s,const void *data,size_t n){
    adb_packet_t *p=calloc(1,sizeof(*p)+n);assert(p);p->msg.data_length=n;memcpy(p->data,data,n);assert(local_sync_write(s,p)==0);
}
static void request(struct adb_service *s,uint32_t command,const char *path){
    uint8_t header[8];write_u32(header,command);write_u32(header+4,strlen(path));
    for(unsigned i=0;i<8;i++)feed(s,header+i,1);
    for(size_t i=0;i<strlen(path);i++)feed(s,path+i,1);
}
static struct adb_service open_service(void){
    struct adb_service s={.local_id=1,.remote_id=2};assert(local_sync_open(&s,NULL)==0);return s;
}
int main(void){
    original.refs=1;original.size=65536;
    for(size_t i=0;i<original.size;i++)original.data[i]=(char)(i%251);
    active=&original;struct adb_service s=open_service();
    request(&s,SYNC_STAT,"/miniapp/miniapp.lua");assert(stat_mode==0100444&&original.refs==2);
    local_sync_ready(&s);assert(sends==1); // STAT acknowledgement is not a DATA request.
    request(&s,SYNC_RECV,"/miniapp/miniapp.lua");assert(sends==2&&last_id==SYNC_DATA);
    // No further data until ACK, even while the active instance is replaced.
    replacement.refs=1;replacement.size=3;memcpy(replacement.data,"new",3);active=&replacement;
    while(last_id==SYNC_DATA)local_sync_ready(&s);
    assert(last_id==SYNC_DONE&&last_size==0&&output_size==65536);
    assert(!memcmp(output,original.data,original.size));
    unsigned before=sends;local_sync_ready(&s);assert(sends==before);
    local_sync_close(&s);assert(original.refs==1);
    // Direct RECV also pins a snapshot; disconnect releases it immediately.
    s=open_service();request(&s,SYNC_RECV,"/miniapp/miniapp.lua");assert(replacement.refs==2);
    local_sync_close(&s);assert(replacement.refs==1);
    // No running instance, disallowed path, and embedded NUL all fail safely.
    active=NULL;s=open_service();request(&s,SYNC_RECV,"/miniapp/miniapp.lua");assert(last_id==SYNC_FAIL);local_sync_close(&s);
    active=&original;s=open_service();request(&s,SYNC_RECV,"/miniapp/other.lua");assert(last_id==SYNC_FAIL);local_sync_close(&s);
    s=open_service();uint8_t invalid[11];write_u32(invalid,SYNC_RECV);write_u32(invalid+4,3);memcpy(invalid+8,"a\0b",3);feed(&s,invalid,sizeof(invalid));assert(last_id==SYNC_FAIL);local_sync_close(&s);
    puts("ADB pull: fragmentation, ACK pacing, 64 KiB, replacement, disconnect and error cases passed");
}
'''
with tempfile.TemporaryDirectory(prefix='miniapp-adb-test-') as directory:
    temp = Path(directory)
    for name, source in HEADERS.items():
        path = temp / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(source)
    source = temp / 'test.c'
    source.write_text(HARNESS)
    binary = temp / 'test'
    defines = ['CONFIG_MINIAPP=1', 'CONFIG_MINIAPP_ADB_DEBUG=1',
               'CONFIG_MINIAPP_SOURCE_MAX_BYTES=65536',
               'CONFIG_MINIAPP_SCREEN_WIDTH=240', 'CONFIG_MINIAPP_SCREEN_HEIGHT=240',
               'CONFIG_MINIAPP_HEAP_MAX_BYTES=393216']
    subprocess.run(['cc', '-std=c11', '-fsanitize=address,undefined', '-g',
                    *['-D' + d for d in defines], '-I' + str(temp), '-I' + str(ROOT),
                    '-I' + str(ROOT / 'arcs-sdk/components/cherryusb-appclass/adb'),
                    str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
