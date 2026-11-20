#pragma once
#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<string.h>
// frame index entry
struct FrameEntry{
    uint32_t ts;    // time stamp in ms
    uint32_t off;   // file offset of frame data
    uint32_t len;   // stored byte length
};
// binary loader, decodes one frame on demand
struct Loader{
    uint16_t W,H;
    uint8_t fps,version;
    uint32_t count;
    FrameEntry*idx;         // frame index
    FILE*fp;                // open asset file
    unsigned char*buf;      // decoded gray frame
    unsigned char*pk;       // packed 4bit nibbles
    unsigned char*tmp;      // raw stored bytes
    uint32_t cached;        // index in buf, NO_FRAME if empty
    bool load(const char*path);
    const unsigned char*frame(uint32_t i);
    void release();
};
