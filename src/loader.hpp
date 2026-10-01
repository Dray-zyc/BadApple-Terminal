#pragma once
#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>
// one frame
struct Frame{
    uint32_t ts;
    unsigned char*gray;
};
// binary loader
struct Loader{
    uint16_t W,H;
    uint8_t fps;
    uint32_t count;
    Frame*frames;
    // load from bin
    bool load(const char*path){
        FILE*f=fopen(path,"rb");
        if(!f)return false;
        // check magic
        char magic[8];
        fread(magic,1,8,f);
        if(memcmp(magic,"BADAPPLE",8)){fclose(f);return false;}
        // read header
        uint16_t version;
        fread(&version,2,1,f);
        fread(&W,2,1,f);
        fread(&H,2,1,f);
        fread(&count,4,1,f);
        fread(&fps,1,1,f);
        fseek(f,9,SEEK_CUR);
        // alloc frames
        frames=(Frame*)malloc(sizeof(Frame)*count);
        for(uint32_t i=0;i<count;i++){
            uint32_t len;
            fread(&frames[i].ts,4,1,f);
            fread(&len,4,1,f);
            frames[i].gray=(unsigned char*)malloc(len);
            fread(frames[i].gray,1,len,f);
        }
        fclose(f);
        return true;
    }
    // free memory
    void release(){
        for(uint32_t i=0;i<count;i++)free(frames[i].gray);
        free(frames);
    }
};