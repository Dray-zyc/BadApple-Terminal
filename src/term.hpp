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
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&csbi);
    cols=csbi.srWindow.Right-csbi.srWindow.Left+1;
    rows=csbi.srWindow.Bottom-csbi.srWindow.Top+1;
#else
    struct winsize ws;
    ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws);
    cols=ws.ws_col;rows=ws.ws_row;
#endif
}