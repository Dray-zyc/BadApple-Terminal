#pragma once
#include<stdio.h>
#ifdef _WIN32
#include<windows.h>
#else
#include<sys/ioctl.h>
#include<unistd.h>
#endif
// query visible terminal cols/rows
inline void term_size(int&cols,int&rows){
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);
    if(!GetConsoleScreenBufferInfo(h,&csbi)){
        // stdout may be redirected, fall back to the console device
        static HANDLE hcon=(HANDLE)-2;
        if(hcon==(HANDLE)-2)
            hcon=CreateFileA("CONOUT$",GENERIC_READ|GENERIC_WRITE,
                FILE_SHARE_WRITE|FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
        if(hcon==INVALID_HANDLE_VALUE||!GetConsoleScreenBufferInfo(hcon,&csbi)){
            cols=80;rows=24;
            return;
        }
    }
    cols=csbi.srWindow.Right-csbi.srWindow.Left+1;
    rows=csbi.srWindow.Bottom-csbi.srWindow.Top+1;
#else
    struct winsize ws;
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)||ws.ws_col<1||ws.ws_row<1){
        cols=80;rows=24;
        return;
    }
    cols=ws.ws_col;rows=ws.ws_row;
#endif
    if(cols<1)cols=1;
    if(rows<1)rows=1;
}
