#include"input.hpp"
#ifdef _WIN32
#include<conio.h>
#else
#include<termios.h>
#include<unistd.h>
#include<fcntl.h>
// saved old termios
static struct termios oldt;
#endif
// win32: nothing to setup
#ifdef _WIN32
void Input::init(){}
void Input::restore(){}
bool Input::hasKey(){return _kbhit()!=0;}
int Input::getKey(){
    int c=_getch();
    if(c==0||c==224){
        int c2=_getch();
        switch(c2){
            case 75:return KEY_LEFT;
            case 77:return KEY_RIGHT;
            case 72:return KEY_UP;
            case 80:return KEY_DOWN;
        }
        return KEY_NONE;
    }
    return c;
}
#else
// linux/macos: raw mode + non-block
void Input::init(){
    tcgetattr(STDIN_FILENO,&oldt);
    struct termios raw=oldt;
    raw.c_lflag&=~(ICANON|ECHO);
    raw.c_cc[VMIN]=0;raw.c_cc[VTIME]=0;
    tcsetattr(STDIN_FILENO,TCSANOW,&raw);
    fcntl(STDIN_FILENO,F_SETFL,O_NONBLOCK);
}
void Input::restore(){tcsetattr(STDIN_FILENO,TCSANOW,&oldt);}
bool Input::hasKey(){char c;return read(STDIN_FILENO,&c,1)==1;}
int Input::getKey(){
    char c;
    if(read(STDIN_FILENO,&c,1)!=1)return KEY_NONE;
    if(c==27){
        char s[2];
        if(read(STDIN_FILENO,&s[0],1)!=1)return 27;
        if(read(STDIN_FILENO,&s[1],1)!=1)return 27;
        if(s[0]=='['){
            switch(s[1]){
                case 'A':return KEY_UP;
                case 'B':return KEY_DOWN;
                case 'C':return KEY_RIGHT;
                case 'D':return KEY_LEFT;
            }
        }
        return 27;
    }
    return c;
}
#endif