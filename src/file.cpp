


#include "th9x.h"
#include "stdio.h"
#include "inttypes.h"
#include "string.h"


#ifdef TEST
#include "assert.h"
#  define EESIZE   150
#  define BS       4
#  define RESV     64  //reserv for eeprom header with directory (eeFs)
#else
//
// bs=16  128 blocks    verlust link:128  16files:16*8  128     sum 256
// bs=32   64 blocks    verlust link: 64  16files:16*16 256     sum 320
//
#  define EESIZE   2048
#  define BS       16
#  define RESV     64  //reserv for eeprom header with directory (eeFs)
#endif
#define FIRSTBLK (RESV/BS)
#define BLOCKS   (EESIZE/BS)

#define EEFS_VERS 4
struct DirEnt{
  uint8_t  startBlk;
  uint16_t size:12;
  uint16_t typ:4;
}__attribute__((packed));
#define MAXFILES (1+MAX_MODELS+3)
struct EeFs{
  uint8_t  version;
  uint8_t  mySize;
  uint8_t  freeList;
  uint8_t  bs;
  DirEnt   files[MAXFILES];
}__attribute__((packed)) eeFs;


static uint8_t EeFsRead(uint8_t blk,uint8_t ofs){
  uint8_t ret;
  eeprom_read_block(&ret,(const void*)(blk*BS+ofs),1);
  return ret;
}
static void EeFsWrite(uint8_t blk,uint8_t ofs,uint8_t val){
  eeWriteBlockCmp(&val, (void*)(blk*BS+ofs), 1);
}

static uint8_t EeFsGetLink(uint8_t blk){
  return EeFsRead( blk,0);
}
static void EeFsSetLink(uint8_t blk,uint8_t val){
  EeFsWrite( blk,0,val);
}
static uint8_t EeFsGetDat(uint8_t blk,uint8_t ofs){
  return EeFsRead( blk,ofs+1);
}
static void EeFsSetDat(uint8_t blk,uint8_t ofs,uint8_t*buf,uint8_t len){
  //EeFsWrite( blk,ofs+1,val);
  eeWriteBlockCmp(buf, (void*)(blk*BS+ofs+1), len);
}
  
uint16_t EeFsGetFree()
{
  uint16_t  ret = 0;
  uint8_t i = eeFs.freeList;
  while( i ){
    ret += BS-1;
    i = EeFsGetLink(i);
  }
  return ret;
}
static void EeFsFree1(uint8_t blk){///free one or more blocks
  EeFsSetLink(blk, eeFs.freeList);
  eeFs.freeList = blk; //chain in front
}
static void EeFsFree(uint8_t blk){///free one or more blocks
  uint8_t i = blk;
  while( EeFsGetLink(i)) i = EeFsGetLink(i);
  EeFsSetLink(i,eeFs.freeList);
  eeFs.freeList = blk; //chain in front
}
static uint8_t EeFsAlloc(){ ///alloc one block from frelist
  uint8_t ret=eeFs.freeList;
  if(ret){
    eeFs.freeList = EeFsGetLink(ret);
    EeFsSetLink(ret,0);
  }
  return ret;
}
static void EeFsFlush()
{
  eeWriteBlockCmp(&eeFs, 0,sizeof(eeFs));
}

void EeFsFormat()
{
  if(sizeof(eeFs) != RESV){
    extern void eeprom_RESV_mismatch();
    eeprom_RESV_mismatch();
  }
  memset(&eeFs,0, sizeof(eeFs));
  eeFs.version  = EEFS_VERS;
  eeFs.mySize   = sizeof(eeFs);
  eeFs.freeList = 0;
  eeFs.bs       = BS;
  for(uint8_t i=FIRSTBLK; i < BLOCKS; i++)  EeFsFree1(i);
  EeFsFlush();
}
bool EeFsOpen()
{
  eeprom_read_block(&eeFs,0,sizeof(eeFs));
#ifdef SIM
  if(eeFs.version != EEFS_VERS)    printf("bad eeFs.version\n");
  if(eeFs.mySize  != sizeof(eeFs)) printf("bad eeFs.mySize\n");
#endif  
  return eeFs.version == EEFS_VERS && eeFs.mySize  == sizeof(eeFs);
}


void EFile::swap(uint8_t i_fileId1,uint8_t i_fileId2)
{
  DirEnt            tmp = eeFs.files[i_fileId1];
  eeFs.files[i_fileId1] = eeFs.files[i_fileId2];
  eeFs.files[i_fileId2] = tmp;;
}

void EFile::rm(uint8_t i_fileId){
  if(eeFs.files[i_fileId].startBlk)
    EeFsFree(eeFs.files[i_fileId].startBlk);
  memset(&eeFs.files[i_fileId],0,sizeof(eeFs.files[i_fileId]));
}

uint16_t EFile::size(){
  return eeFs.files[m_fileId].size;
}
uint8_t EFile::open(uint8_t i_fileId){
  m_fileId = i_fileId;
  ofs      = 0;
  pos      = 0;
  bRlc     = 0;
  currBlk  = eeFs.files[m_fileId].startBlk; 
  return  eeFs.files[m_fileId].typ; 
}
uint8_t EFile::read(uint8_t*buf,uint8_t i_len){
  uint8_t len = eeFs.files[m_fileId].size - pos; 
  if(len < i_len) i_len = len;
  len = i_len;
  while(len)
  {
    if(!currBlk) break;
    *buf++ = EeFsGetDat(currBlk, ofs++);
    if(ofs>=(BS-1)){
      ofs=0;
      currBlk=EeFsGetLink(currBlk);
    }
    len--;
  }
  pos += i_len - len;
  return i_len - len;
}
uint8_t EFile::readRlc(uint8_t*buf,uint8_t i_len){
  uint8_t i;
  for( i=0; i<i_len; ){
    if((bRlc&0x7f) == 0) {
      if(read(&bRlc,1)!=1) break;
    }
    assert(bRlc & 0x7f);
    uint8_t l=min(bRlc&0x7f,i_len-i);
    if(bRlc&0x80){
      memset(&buf[i],0,l);
    }else{
      uint8_t lr = read(&buf[i],l);
      if(lr!=l) return i+lr;
    }
    i    += l;
    bRlc -= l;
  }
  return i;
}
uint8_t EFile::write(uint8_t*buf,uint8_t i_len){
  uint8_t len=i_len;
  if(!currBlk && pos==0)
  {
    eeFs.files[m_fileId].startBlk = currBlk = EeFsAlloc();
      
  }
  while(len)
  {
    if(!currBlk) break;
    if(ofs>=(BS-1)){
      ofs=0;
      if( ! EeFsGetLink(currBlk) ){
        EeFsSetLink(currBlk, EeFsAlloc());
      }
      currBlk = EeFsGetLink(currBlk);
    }
    if(!currBlk) break;
    uint8_t l = BS-1-ofs; if(l>len) l=len;
    EeFsSetDat(currBlk, ofs, buf, l);
    buf+=l;
    ofs+=l;
    len-=l;
  }
  pos += i_len - len;
  return i_len - len;
}
uint8_t EFile::writeRlc(uint8_t i_fileId, uint8_t typ,uint8_t*buf,uint8_t i_len){
  open(i_fileId);
  eeFs.files[i_fileId].typ      = typ; 
  eeFs.files[m_fileId].size     = 0; 

  bool    state0 = true;
  uint8_t cnt    = 0;
  uint8_t i;
  for( i=0; i<=i_len; i++)
  {
    bool nst0 = buf[i] == 0;
    if( nst0 && !state0 && buf[i+1]!=0) nst0 = false ;
    if(nst0 != state0 || cnt>=0x7f || i==i_len){
      if(state0){
        if(cnt>0){
          cnt|=0x80;
          write(&cnt,1);
          cnt=0;
        }
      }else{
        write(&cnt,1);
        write(&buf[i-cnt],cnt);
        cnt=0;
      }
      state0 = nst0;
    }
    cnt++;
  }

  eeFs.files[m_fileId].size     = pos; 
  if(currBlk && EeFsGetLink(currBlk)){
    EeFsFree(EeFsGetLink(currBlk));
    EeFsSetLink(currBlk, 0);
  }
  EeFsFlush();
  return i_len;
}

#ifdef TEST
uint8_t eeprom[EESIZE];
static void EeFsDump(){
  for(int i=0; i<EESIZE; i++)
  {
    printf("%02x ",eeprom[i]);
    if(i%16 == 15) puts("");
  }
  puts("");
}
void eeprom_read_block (void *pointer_ram,
                   const void *pointer_eeprom,
                        size_t size)
{
  memcpy(pointer_ram,&eeprom[(int)pointer_eeprom],size);
}
void eeWriteBlockCmp(const void *i_pointer_ram, void *i_pointer_eeprom, size_t size)
{
  memcpy(&eeprom[(int)i_pointer_eeprom],i_pointer_ram,size);
}

#define FILES 5
int main();
void showfiles()
{
  printf("------ free: %d\n",EeFsGetFree());
  EeFsDump();
  EFile f;
  for(int i=0; i<MAXFILES; i++){
    if(f.open(i)==0) continue;
    printf("file%d %4d ",i,f.size());
    for(int j=0; j<100; j++){ 
      uint8_t buf[2];
      if(f.readRlc(buf,1)==0) break;
      printf("%c",buf[0]);
    }
    printf("\n");
  }
  
}
int main()
{
  if(!EeFsOpen()) EeFsFormat();
  showfiles();
  EFile    f[FILES];
  for(int i=0; i<FILES; i++){ f[i].create(i,5); }

  
  for(int i=0; i<FILES; i++){
    uint8_t buf[20];
    for(int j=0; j<10; j++){ 
      buf[j*2]=i+'0';
      buf[j*2+1]=j+'a';
    }
    f[i].writeRlc(buf,20);
  }
  for(int i=0; i<FILES; i++){ f[i].trunc(); }
  
 
  showfiles();
  EFile::rm(3);
  EFile::rm(1);
  EFile::rm(5);
  EFile::rm(6);
  showfiles();
  
  f[0].create(6,6);
  for(int i=0; i<255; i++){ f[0].writeRlc((uint8_t*)"66666",5);   }
  f[0].trunc(); 

  showfiles();

  f[0].open(6);
  for(int i=0; i<9; i++){ uint8_t b; f[0].readRlc(&b,1);   }
  f[0].trunc(); 

  showfiles();
}


#endif




