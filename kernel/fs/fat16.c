#include "fat16.h"
#include "../drivers/storage/ata.h"
#include "../core/console.h"
#include "vfs.h"
#include <stdint.h>
#include <stddef.h>

#define SECTOR_SIZE 512u
#define MAX_OPEN 32u
#define ENTRY_SIZE 32u
#define ATTR_DIR 0x10u
#define ATTR_VOL 0x08u
#define ATTR_LFN 0x0Fu

struct vfs_file{
    uint32_t first_cluster,size,pos,dir_lba;
    uint16_t dir_offset;
    uint32_t flags;
    uint8_t used;
};

typedef struct{
    uint16_t bps;
    uint8_t spc;
    uint16_t reserved;
    uint8_t fats;
    uint16_t root_entries;
    uint32_t total;
    uint16_t spf;
    uint32_t fat_lba,root_lba,root_sectors,data_lba,clusters,dev;
} fat16_fs_t;

static fat16_fs_t fs;
static struct vfs_file handles[MAX_OPEN];
static uint8_t buf[SECTOR_SIZE], buf2[SECTOR_SIZE];

static uint16_t r16(const uint8_t*p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t r32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void w16(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void w32(uint8_t*p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}

static int rd(uint32_t lba,void*b){return ata_read28(fs.dev,lba,b);}
static int wr(uint32_t lba,const void*b){return ata_write28(fs.dev,lba,b);}
static uint32_t clba(uint32_t c){return fs.data_lba+(c-2u)*fs.spc;}
static int validc(uint32_t c){return c>=2u&&c<fs.clusters+2u;}

static int fatget(uint32_t c,uint16_t*v){
    uint32_t off=c*2u,lba=fs.fat_lba+off/512u,pos=off%512u;
    if(pos!=511){if(rd(lba,buf))return -1;*v=r16(&buf[pos]);return 0;}
    if(rd(lba,buf))return -1;
    uint8_t lo=buf[511];
    if(rd(lba+1,buf2))return -1;
    *v=(uint16_t)lo|((uint16_t)buf2[0]<<8);
    return 0;
}

static int fatset1(uint32_t base,uint32_t c,uint16_t v){
    uint32_t off=c*2u,lba=base+off/512u,pos=off%512u;
    if(pos!=511){if(rd(lba,buf))return -1;w16(&buf[pos],v);return wr(lba,buf);}
    if(rd(lba,buf))return -1;
    buf[511]=(uint8_t)v;
    if(wr(lba,buf))return -1;
    if(rd(lba+1,buf2))return -1;
    buf2[0]=(uint8_t)(v>>8);
    return wr(lba+1,buf2);
}

static int fatset(uint32_t c,uint16_t v){for(uint8_t i=0;i<fs.fats;i++)if(fatset1(fs.fat_lba+(uint32_t)i*fs.spf,c,v))return -1;return 0;}

static int allocc(uint32_t*out){
    for(uint32_t c=2;c<=fs.clusters+1;c++){
        uint16_t v;
        if(fatget(c,&v))return -1;
        if(v==0){
            if(fatset(c,0xFFFF))return -1;
            for(uint8_t s=0;s<fs.spc;s++){
                uint8_t z[512]={0};
                if(wr(clba(c)+s,z))return -1;
            }
            *out=c;
            return 0;
        }
    }
    return -1;
}

static int chain_at(uint32_t first,uint32_t idx,uint32_t*out){
    if(first<2)return -1;
    uint32_t c=first;
    for(uint32_t i=0;i<idx;i++){
        uint16_t n;
        if(fatget(c,&n))return -1;
        if(n>=FAT16_EOC||n<2||n==FAT16_BAD||!validc(n))return -1;
        c=n;
    }
    *out=c;
    return 0;
}

static int ensure_at(struct vfs_file*f,uint32_t idx,uint32_t*out){
    if(f->first_cluster<2){
        if(allocc(&f->first_cluster))return -1;
    }
    uint32_t c=f->first_cluster;
    for(uint32_t i=0;i<idx;i++){
        uint16_t n;
        if(fatget(c,&n))return -1;
        if(n>=FAT16_EOC){
            uint32_t nc;
            if(allocc(&nc))return -1;
            if(fatset(c,(uint16_t)nc))return -1;
            c=nc;
        }else{
            if(n<2||n==FAT16_BAD||!validc(n))return -1;
            c=n;
        }
    }
    *out=c;
    return 0;
}

static void name83(const char*p,char out[11]){
    for(int i=0;i<11;i++)out[i]=' ';
    while(*p=='/')p++;
    size_t n=0;
    while(p[n]&&p[n]!='/')n++;
    size_t dot=n;
    for(size_t i=0;i<n;i++)if(p[i]=='.'){dot=i;break;}
    size_t bl=dot>8?8:dot;
    for(size_t i=0;i<bl;i++){
        char c=p[i];
        if(c>='a'&&c<='z')c=(char)(c-'a'+'A');
        out[i]=c;
    }
    if(dot<n){
        size_t el=n-dot-1;
        if(el>3)el=3;
        for(size_t i=0;i<el;i++){
            char c=p[dot+1+i];
            if(c>='a'&&c<='z')c=(char)(c-'a'+'A');
            out[8+i]=c;
        }
    }
}

static int find(const char*p,uint8_t*e,uint32_t*l,uint16_t*o){
    char n[11];
    name83(p,n);
    if(p[0]=='/'&&p[1]=='\0')return 0;
    for(uint32_t s=0;s<fs.root_sectors;s++){
        uint32_t lba=fs.root_lba+s;
        if(rd(lba,buf))return -1;
        for(uint32_t off=0;off<512;off+=32){
            uint8_t first=buf[off],a=buf[off+11];
            if(first==0)return 0;
            if(first==0xE5||a==ATTR_LFN||(a&ATTR_VOL))continue;
            int same=1;
            for(int i=0;i<11;i++)if(buf[off+i]!=(uint8_t)n[i]){same=0;break;}
            if(same){
                for(int i=0;i<32;i++)e[i]=buf[off+i];
                *l=lba;
                *o=(uint16_t)off;
                return 1;
            }
        }
    }
    return 0;
}

static int freeentry(uint32_t*l,uint16_t*o){
    for(uint32_t s=0;s<fs.root_sectors;s++){
        uint32_t lba=fs.root_lba+s;
        if(rd(lba,buf))return -1;
        for(uint32_t off=0;off<512;off+=32)
            if(buf[off]==0||buf[off]==0xE5){
                *l=lba;
                *o=(uint16_t)off;
                return 0;
            }
    }
    return -1;
}

static int saveentry(struct vfs_file*f){
    if(rd(f->dir_lba,buf2))return -1;
    uint8_t*e=&buf2[f->dir_offset];
    w16(&e[26],(uint16_t)f->first_cluster);
    w32(&e[28],f->size);
    return wr(f->dir_lba,buf2);
}

int fat16_mount(uint32_t dev){
    if(dev>=ata_device_count())return -1;
    if(rd(0,buf))return -1;
    uint16_t bps=r16(&buf[11]),reserved=r16(&buf[14]),root=r16(&buf[17]),spf=r16(&buf[22]);
    uint8_t spc=buf[13],fats=buf[16];
    uint32_t total=r16(&buf[19]);
    if(!total)total=r32(&buf[32]);
    if(bps!=512||!spc||!reserved||!fats||!root||!spf||!total)return -1;
    uint32_t rootsec=((uint32_t)root*32+511)/512,rootlba=reserved+(uint32_t)fats*spf,datalba=rootlba+rootsec;
    if(datalba>=total)return -1;
    uint32_t clusters=(total-datalba)/spc;
    if(clusters<4085||clusters>=65525)return -1;
    fs=(fat16_fs_t){bps,spc,reserved,fats,root,total,spf,reserved,rootlba,rootsec,datalba,clusters,dev};
    for(uint32_t i=0;i<MAX_OPEN;i++)handles[i].used=0;
    serial_print("[fat16] mounted: sectors=");
    serial_print_dec(total);
    serial_print(" clusters=");
    serial_print_dec(clusters);
    serial_print("\n");
    return 0;
}

static int free_chain(uint32_t first) {
    if (first < 2) return 0;
    uint32_t c = first;
    uint32_t guard = 0;
    while (validc(c) && guard++ <= fs.clusters) {
        uint16_t next;
        if (fatget(c, &next)) return -1;
        if (fatset(c, 0)) return -1;
        if (next >= FAT16_EOC) return 0;
        if (next < 2 || next == FAT16_BAD || !validc(next)) return -1;
        c = next;
    }
    return -1;
}

static struct vfs_file *fat16_get_handle(void *handle)
{
    return (struct vfs_file *)handle;
}

int fat16_open(const char *p, uint32_t flags, void **out)
{
    if(!p||!out||p[0]!='/')return -1;
    uint8_t e[32];uint32_t l;uint16_t o;
    int f=find(p,e,&l,&o);
    if(f<0)return -1;
    if(!f){
        if(!(flags&VFS_O_CREAT)||p[1]=='\0'||freeentry(&l,&o))return -1;
        for(int i=0;i<32;i++)e[i]=0;
        name83(p,(char*)e);
        e[11]=0x20;
        w16(&e[26],0);
        w32(&e[28],0);
        if(rd(l,buf2))return -1;
        for(int i=0;i<32;i++)buf2[o+i]=e[i];
        if(wr(l,buf2))return -1;
    }else if(e[11]&ATTR_DIR)return -1;
    for(uint32_t i=0;i<MAX_OPEN;i++)if(!handles[i].used){
        struct vfs_file*x=&handles[i];
        x->used=1;
        x->first_cluster=r16(&e[26]);
        x->size=r32(&e[28]);
        x->pos=(flags&VFS_O_APPEND)?x->size:0;
        x->dir_lba=l;
        x->dir_offset=o;
        x->flags=flags;
        if((flags&VFS_O_TRUNC)&&(flags&VFS_O_WRITE)){
            uint32_t old_first=x->first_cluster;
            x->first_cluster=0; x->size=0; x->pos=0;
            if(old_first>=2 && free_chain(old_first)!=0){x->used=0;return -1;}
            if(saveentry(x)!=0){x->used=0;return -1;}
        }
        *out = (void *)x;
        return 0;
    }
    return -1;
}

long fat16_size(void *handle)
{
    struct vfs_file *f = fat16_get_handle(handle);
    return (f && f->used) ? (long)f->size : -1;
}

long fat16_read(void *handle, void *dstv, size_t count)
{
    struct vfs_file *f = fat16_get_handle(handle);
    if (!f || !f->used || !dstv || (f->flags & VFS_O_READ) == 0)
        return -1;
    if (f->pos >= f->size || count == 0)
        return 0;

    size_t rem = f->size - f->pos;
    if (count > rem) count = rem;

    uint8_t *dst = dstv;
    size_t done = 0;
    uint32_t cb = (uint32_t)fs.spc * 512;

    /* Trova il cluster iniziale per la posizione corrente. */
    uint32_t c;
    if (chain_at(f->first_cluster, f->pos / cb, &c))
        return -1;

    uint32_t inc = f->pos % cb;   /* offset dentro il cluster */

    while (done < count) {
        uint32_t si = inc / 512;
        uint32_t so = inc % 512;

        if (rd(clba(c) + si, buf))
            return done ? (long)done : -1;

        size_t n = 512 - so;
        if (n > count - done) n = count - done;
        for (size_t i = 0; i < n; i++)
            dst[done + i] = buf[so + i];

        done += n;
        inc += n;
        f->pos += n;


        if (inc >= cb) {
            uint16_t next;
            if (fatget(c, &next)) return done;
            if (next >= FAT16_EOC || next < 2 || next == FAT16_BAD || !validc(next))
                return done;
            c = next;
            inc = 0;
        }
    }

    return (long)done;
}

long fat16_write(void *handle, const void *srcv, size_t count)
{
    struct vfs_file *f = fat16_get_handle(handle);
    if(!f||!f->used||!srcv||(f->flags&VFS_O_WRITE)==0)return -1;
    if(count==0)return 0;
    if((uint64_t)f->pos+count>0xFFFFFFFFULL)return -1;
    const uint8_t*src=srcv;
    size_t done=0;
    uint32_t cb=(uint32_t)fs.spc*512;
    while(done<count){
        uint32_t idx=f->pos/cb,inc=f->pos%cb,c;
        if(ensure_at(f,idx,&c))return done?(long)done:-1;
        uint32_t si=inc/512,so=inc%512;
        if(rd(clba(c)+si,buf))return done?(long)done:-1;
        size_t n=512-so;
        if(n>count-done)n=count-done;
        for(size_t i=0;i<n;i++)buf[so+i]=src[done+i];
        if(wr(clba(c)+si,buf))return done?(long)done:-1;
        done+=n;
        f->pos+=n;
        if(f->pos>f->size)f->size=f->pos;
    }
    if(saveentry(f))return done?(long)done:-1;
    return(long)done;
}

int fat16_close(void *handle)
{
    struct vfs_file *f = fat16_get_handle(handle);
    if(!f||!f->used)return -1;
    int r=saveentry(f);
    f->used=0;
    return r;
}

/* VFS operations for FAT16 */
struct vfs_filesystem_ops fat16_ops = {
    .open = fat16_open,
    .read = fat16_read,
    .write = fat16_write,
    .close = fat16_close,
    .size = fat16_size,
    .mkdir = NULL
};