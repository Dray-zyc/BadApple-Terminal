#include"loader.hpp"
// sentinel for empty decode cache
static const uint32_t NO_FRAME=0xFFFFFFFFu;
// expand one stored frame: rle -> packed nibbles -> gray
static void decode_rle(const unsigned char*src,uint32_t len,unsigned char*pk,unsigned char*dst,int n){
    uint32_t m=(uint32_t)((n+1)>>1);
    uint32_t i=0,o=0;
    while(i+1<len&&o<m){
        int c=src[i++];
        unsigned char v=src[i++];
        while(c-->0&&o<m)pk[o++]=v;
    }
    while(o<m)pk[o++]=0;
    for(int k=0;k<n;k++)
        dst[k]=(k&1)?(unsigned char)((pk[k>>1]&15)*17)
                    :(unsigned char)((pk[k>>1]>>4)*17);
}
// load header and build frame index
bool Loader::load(const char*path){
    char magic[8];
    int n;
    uint32_t i;
    fp=0;idx=0;buf=pk=tmp=0;cached=NO_FRAME;count=0;version=0;
    fp=fopen(path,"rb");
    if(!fp)return false;
    if(fread(magic,1,8,fp)!=8||memcmp(magic,"BADAPPLE",8)){fclose(fp);fp=0;return false;}
    // read header
    if(fread(&version,2,1,fp)!=1||fread(&W,2,1,fp)!=1||fread(&H,2,1,fp)!=1)goto fail;
    if(fread(&count,4,1,fp)!=1||fread(&fps,1,1,fp)!=1)goto fail;
    fseek(fp,9,SEEK_CUR);
    if(W<1||H<1||count<1||count>10000000u)goto fail;
    // build index, skip frame data
    idx=(FrameEntry*)malloc(sizeof(FrameEntry)*count);
    if(!idx)goto fail;
    n=W*H;
    for(i=0;i<count;i++){
        uint32_t ts,len;
        if(fread(&ts,4,1,fp)!=1||fread(&len,4,1,fp)!=1)goto fail;
        idx[i].ts=ts;
        idx[i].len=len;
        idx[i].off=(uint32_t)ftell(fp);
        // v2 stores raw gray, v3 stores rle of packed nibbles
        if(version<3&&len!=(uint32_t)n)goto fail;
        if(version>=3&&len>(uint32_t)n+2)goto fail;
        if(fseek(fp,len,SEEK_CUR))goto fail;
    }
    // decode buffers
    buf=(unsigned char*)malloc(n);
    pk=(unsigned char*)malloc((n+1)>>1);
    tmp=(unsigned char*)malloc(n+2);
    if(!buf||!pk||!tmp)goto fail;
    cached=NO_FRAME;
    return true;
    fail:
    free(idx);free(buf);free(pk);free(tmp);
    fclose(fp);
    fp=0;idx=0;buf=pk=tmp=0;
    return false;
}
// decode frame i, returns internal gray buffer
const unsigned char*Loader::frame(uint32_t i){
    if(!buf||count==0)return 0;
    if(i>=count)i=count-1;
    if(cached==i)return buf;
    // every frame carries its own 8 byte header, so data is not contiguous
    if(fseek(fp,(long)idx[i].off,SEEK_SET)){
        memset(buf,0,(size_t)W*H);
        cached=NO_FRAME;
        return buf;
    }
    size_t got;
    if(version>=3){
        got=fread(tmp,1,idx[i].len,fp);
        if(got<idx[i].len){memset(buf,0,(size_t)W*H);cached=NO_FRAME;return buf;}
        decode_rle(tmp,idx[i].len,pk,buf,W*H);
    }else{
        got=fread(buf,1,idx[i].len,fp);
        if(got<idx[i].len){memset(buf,0,(size_t)W*H);cached=NO_FRAME;return buf;}
    }
    cached=i;
    return buf;
}
// free everything
void Loader::release(){
    free(idx);free(buf);free(pk);free(tmp);
    idx=0;buf=pk=tmp=0;
    if(fp){fclose(fp);fp=0;}
    count=0;cached=NO_FRAME;
}
