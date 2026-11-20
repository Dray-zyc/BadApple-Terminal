#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>
// default asset spec
const int DEF_W=240,DEF_H=180,DEF_FPS=60;
// file paths
const char*RAW_PATH="../assets/frames.raw";
const char*BIN_PATH="../assets/frames.bin";
// pack one frame: two 4bit gray pixels per byte, high nibble first
static int pack_frame(const unsigned char*src,int n,unsigned char*dst){
    for(int i=0;i<n;i+=2){
        unsigned char hi=src[i]>>4;
        unsigned char lo=(i+1<n)?(src[i+1]>>4):0;
        dst[i>>1]=(unsigned char)((hi<<4)|lo);
    }
    return (n+1)>>1;
}
// rle one buffer: (count,value) pairs, count capped at 255
static int rle_pack(const unsigned char*src,int m,unsigned char*dst){
    int i=0,o=0;
    while(i<m){
        unsigned char v=src[i];
        int len=1;
        while(i+len<m&&src[i+len]==v&&len<255)len++;
        dst[o++]=(unsigned char)len;
        dst[o++]=v;
        i+=len;
    }
    return o;
}
int main(int argc,char**argv){
    // args: [raw] [bin] [W] [H] [fps]
    const char*raw_path=argc>1?argv[1]:RAW_PATH;
    const char*bin_path=argc>2?argv[2]:BIN_PATH;
    int W=argc>3?atoi(argv[3]):DEF_W;
    int H=argc>4?atoi(argv[4]):DEF_H;
    int FPS=argc>5?atoi(argv[5]):DEF_FPS;
    if(W<2||H<1||FPS<1){fprintf(stderr,"bad args\n");return 1;}
    int n=W*H;
    // open raw file
    FILE*fp=fopen(raw_path,"rb");
    if(!fp){fprintf(stderr,"cannot open %s\n",raw_path);return 1;}
    // get file size
    fseek(fp,0,SEEK_END);
    long long raw_size=ftell(fp);
    fseek(fp,0,SEEK_SET);
    // frame size check
    if(raw_size%(long long)n){fprintf(stderr,"raw size not aligned\n");fclose(fp);return 1;}
    uint32_t count=(uint32_t)(raw_size/n);
    printf("frames = %u, size = %dx%d, fps = %d\n",count,W,H,FPS);
    // open output
    FILE*out=fopen(bin_path,"wb");
    if(!out){fprintf(stderr,"cannot write %s\n",bin_path);fclose(fp);return 1;}
    // write header fields
    fwrite("BADAPPLE",1,8,out);
    uint16_t version=3,w=(uint16_t)W,h=(uint16_t)H;uint8_t fps=(uint8_t)FPS;
    fwrite(&version,2,1,out);
    fwrite(&w,2,1,out);
    fwrite(&h,2,1,out);
    fwrite(&count,4,1,out);
    fwrite(&fps,1,1,out);
    char rsv[9]={0};fwrite(rsv,1,9,out);
    // buffers
    unsigned char*buf=(unsigned char*)malloc(n);
    unsigned char*pk=(unsigned char*)malloc((n+1)>>1);
    unsigned char*rle=(unsigned char*)malloc((size_t)n+2);
    long long total=0,maxlen=0;
    // loop all frames
    for(uint32_t i=0;i<count;i++){
        if(fread(buf,1,n,fp)!=(size_t)n){fprintf(stderr,"short read at frame %u\n",i);break;}
        int m=pack_frame(buf,n,pk);
        uint32_t len=(uint32_t)rle_pack(pk,m,rle);
        uint32_t ts=(uint32_t)((uint64_t)i*1000/FPS);
        fwrite(&ts,4,1,out);
        fwrite(&len,4,1,out);
        fwrite(rle,1,len,out);
        total+=(long long)len+8;
        if(len>maxlen)maxlen=len;
    }
    free(buf);free(pk);free(rle);
    fclose(fp);fclose(out);
    printf("packed %lld bytes (x%.2f), max frame %lld\n",total,(double)raw_size/total,maxlen);
    printf("done: %s\n",bin_path);
    return 0;
}
