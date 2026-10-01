#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
// constants
const int W=120,H=90,FPS=30;
// file paths
const char*RAW_PATH="../assets/frames.raw";
const char*BIN_PATH="../assets/frames.bin";
int main(){
    // open raw file
    FILE*fp=fopen(RAW_PATH,"rb");
    if(!fp){fprintf(stderr,"cannot open %s\n",RAW_PATH);return 1;}
    // get file size
    fseek(fp,0,SEEK_END);
    long long raw_size=ftell(fp);
    fseek(fp,0,SEEK_SET);
    // frame size check
    long long frame_bytes=(long long)W*H;
    if(raw_size%frame_bytes){fprintf(stderr,"raw size not aligned\n");return 1;}
    uint32_t count=(uint32_t)(raw_size/frame_bytes);
    printf("frames = %u, size = %dx%d, fps = %d\n",count,W,H,FPS);
    // open output
    FILE*out=fopen(BIN_PATH,"wb");
    if(!out){fprintf(stderr,"cannot write %s\n",BIN_PATH);return 1;}
    // write header fields
    fwrite("BADAPPLE",1,8,out);
    uint16_t version=2,w=W,h=H;uint8_t fps=FPS;
    fwrite(&version,2,1,out);
    fwrite(&w,2,1,out);
    fwrite(&h,2,1,out);
    fwrite(&count,4,1,out);
    fwrite(&fps,1,1,out);
    char rsv[9]={0};fwrite(rsv,1,9,out);
    // alloc frame buffer
    unsigned char*buf=(unsigned char*)malloc(frame_bytes);
    // loop all frames
    for(uint32_t i=0;i<count;i++){
        fread(buf,1,frame_bytes,fp);
        uint32_t ts=(uint32_t)((uint64_t)i*1000/FPS),len=(uint32_t)frame_bytes;
        fwrite(&ts,4,1,out);
        fwrite(&len,4,1,out);
        fwrite(buf,1,frame_bytes,out);
    }
    free(buf);
    fclose(fp);fclose(out);
    printf("done: %s\n",BIN_PATH);
    return 0;
}