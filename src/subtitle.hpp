#pragma once
#include<stdint.h>
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
// one cue
struct SubCue{
    uint32_t start,end;
    char text[128];
};
// one subtitle track
struct Subtitle{
    SubCue*cues;
    int count,cap,cur;
    void init(){cues=0;count=0;cap=0;cur=0;}
    void release(){free(cues);cues=0;count=0;cap=0;cur=0;}
    void reset(){cur=0;}
    bool load(const char*path);
    const char*update(uint32_t ms);
};