#pragma once
// audio player via winmm or aplay
struct Audio{
    int opened=0;
    int pid=-1;
    char path[256]={0};
    bool open(const char*p);
    void play();
    void pause();
    void resume();
    void stop();
    void close();
    void seek(long ms);
    long position();
};