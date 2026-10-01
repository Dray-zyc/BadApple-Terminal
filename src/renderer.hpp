#pragma once
#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>
// one character cell
struct Cell{
    uint32_t ch;uint8_t r,g,b;
    bool operator==(const Cell&B)const{return ch==B.ch&&r==B.r&&g==B.g&&b==B.b;}
    bool operator!=(const Cell&B)const{return !(*this==B);}
};
// renderer struct
struct Renderer{
    int W,H;           // source pixel size
    int cw,ch;         // buffer size = terminal cell size
    int threshold;     // binarize threshold
    int mode;          // render mode index
    int mode_mask;     // bitmask of enabled modes
    int progress;      // -1=off, 0..1000=progress permille
    char hud[4][48];   // up to 4 hud lines
    int  hud_count;    // active hud lines
    Cell*front;        // front buffer
    Cell*back;         // back buffer
    void init(int w,int h);
    void release();
    void setMode(int m);
    int  nextMode();
    void resize(int cols,int rows);
    void clear();
    void present();
    void invalidate();
    void render_scaled(const unsigned char*gray,int cols,int rows);
};