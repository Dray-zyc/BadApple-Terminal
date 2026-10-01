#include"audio.hpp"
#include<string.h>
#ifdef _WIN32
#include<windows.h>
#include<mmsystem.h>
#include<stdio.h>
#include<stdlib.h>
// win32 MCI backend
bool Audio::open(const char*p){
    strncpy(path,p,sizeof(path)-1);
    char cmd[512];
    sprintf(cmd,"open \"%s\" type waveaudio alias ba",p);
    if(mciSendStringA(cmd,0,0,0))return false;
    // use ms for position and seek
    mciSendStringA("set ba time format milliseconds",0,0,0);
    opened=1;
    return true;
}
void Audio::play(){if(opened)mciSendStringA("play ba",0,0,0);}
void Audio::pause(){if(opened)mciSendStringA("pause ba",0,0,0);}
void Audio::resume(){if(opened)mciSendStringA("resume ba",0,0,0);}
void Audio::stop(){if(opened)mciSendStringA("stop ba",0,0,0);}
void Audio::close(){if(opened){mciSendStringA("close ba",0,0,0);opened=0;}}
void Audio::seek(long ms){
    if(!opened)return;
    char cmd[64];
    sprintf(cmd,"seek ba to %ld",ms);
    mciSendStringA(cmd,0,0,0);
}
long Audio::position(){
    if(!opened)return -1;
    char buf[32]={0};
    mciSendStringA("status ba position",buf,sizeof(buf),0);
    return atol(buf);
}
#else
#include<unistd.h>
#include<signal.h>
#include<stdlib.h>
// posix backend: forked aplay
bool Audio::open(const char*p){
    strncpy(path,p,sizeof(path)-1);
    opened=1;
    return true;
}
void Audio::play(){
    if(!opened||pid>0)return;
    pid=fork();
    if(pid==0){
        #ifdef __APPLE__
        execlp("afplay","afplay",path,(char*)0);
        #else
        execlp("aplay","aplay","-q",path,(char*)0);
        #endif
        _exit(1);
    }
}
void Audio::pause(){if(pid>0)kill(pid,SIGSTOP);}
void Audio::resume(){if(pid>0)kill(pid,SIGCONT);}
void Audio::stop(){if(pid>0){kill(pid,SIGTERM);pid=-1;}}
void Audio::close(){stop();opened=0;}
// posix: no seek, silently ignored
void Audio::seek(long ms){(void)ms;}
long Audio::position(){return -1;}
#endif