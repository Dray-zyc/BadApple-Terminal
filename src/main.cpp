#include"loader.hpp"
#include"renderer.hpp"
#include"input.hpp"
#include"audio.hpp"
#include"config.hpp"
#include"term.hpp"
#include<chrono>
#include<thread>
#include<stdlib.h>
#include<stdio.h>
#include<signal.h>
#ifdef _WIN32
#include<windows.h>
#include<direct.h>
// chdir to exe dir
static void chdir_to_exe(const char*argv0){
    char buf[1024];
    DWORD n=GetModuleFileNameA(0,buf,sizeof(buf));
    if(n==0||n>=sizeof(buf))return;
    char*p=strrchr(buf,'\\');
    if(!p)return;
    *p=0;
    _chdir(buf);
}
// enable ansi escape on conhost
static void enable_vt(){
    HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode=0;
    if(GetConsoleMode(h,&mode))
        SetConsoleMode(h,mode|0x0004);
}
#endif
// signal handler
static void on_signal(int){exit(0);}
// restore terminal on exit
static void cleanup(){
    Input::restore();
    fputs("\033[?25h\033[0m\033[?1049l",stdout);
    fflush(stdout);
}
// set window title based on hud state
static void set_title(int window_title,int show_hud){
    if(!window_title)return;
    #ifdef _WIN32
    SetConsoleTitleA(show_hud?"Bad Apple - Debug":"Bad Apple");
    #else
    printf("\033]0;%s\007",show_hud?"Bad Apple - Debug":"Bad Apple");
    fflush(stdout);
    #endif
}
// run fade in/out
static void fade(Renderer&R,const unsigned char*gray,int cols,int rows,int from,int to,int frames,int poll_ms){
    for(int i=0;i<frames;i++){
        R.threshold=from+(to-from)*i/frames;
        R.render_scaled(gray,cols,rows);
        R.present();
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms>0?poll_ms:16));
    }
    R.threshold=to;
}
// run noise convergence intro
static void intro_rain(Renderer&R,const unsigned char*gray,int cols,int rows,int frames,int poll_ms){
    static const char glyphs[]="!@#$%^&*()_+-=[]{}|;:,.<>?/";
    int ng=sizeof(glyphs)-1;
    for(int t=0;t<frames;t++){
        R.render_scaled(gray,cols,rows);
        // noise from 900 down to 0, last frame is 0
        int noise=(frames>1)?(900*(frames-1-t)/(frames-1)):0;
        int n=R.cw*R.ch;
        for(int i=0;i<n;i++){
            if((rand()&1023)<noise){
                R.back[i].ch=(unsigned char)glyphs[rand()%ng];
                R.back[i].r=R.back[i].g=R.back[i].b=255;
            }
        }
        R.present();
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms>0?poll_ms:33));
    }
    // force full redraw of clean frame
    R.invalidate();
    R.render_scaled(gray,cols,rows);
    R.present();
}
int main(int argc,char**argv){
    // windows: utf8 + vt + chdir to exe
    #ifdef _WIN32
    SetConsoleOutputCP(65001);
    enable_vt();
    chdir_to_exe(argv[0]);
    #endif
    // load config
    Config cfg;
    if(!cfg.load("config.json"))
        fprintf(stderr,"no config.json, using defaults\n");
    // load frames
    Loader ld;
    if(!ld.load("assets/frames.bin")){
        fprintf(stderr,"error: cannot load assets/frames.bin\n");
        fprintf(stderr,"make sure you run from the project root\n");
        return 1;
    }
    // printf("frames = %u, size = %ux%u, fps = %u\n",ld.count,ld.W,ld.H,ld.fps);
    // read all config
    int base_thr        =cfg.get_int("threshold",128);
    int mode_mask       =cfg.get_int("mode_mask",0b11111);
    int start_mode      =cfg.get_int("mode",0);
    int fps_cap         =cfg.get_int("fps_cap",60);
    int show_hud        =cfg.get_int("show_hud",0);
    int audio_on        =cfg.get_int("audio",1);
    int jump_keeps_pause=cfg.get_int("jump_keeps_pause",0);
    int window_title    =cfg.get_int("window_title",1);
    int progress_bar    =cfg.get_int("progress_bar",0);
    int fade_in         =cfg.get_int("fade_in",0);
    int fade_out        =cfg.get_int("fade_out",0);
    int fade_frames     =cfg.get_int("fade_frames",30);
    int loop            =cfg.get_int("loop",1);
    int jump_flash      =cfg.get_int("jump_flash",0);
    int jump_flash_frames=cfg.get_int("jump_flash_frames",8);
    int intro_rain_on   =cfg.get_int("intro_rain",1);
    int intro_frames    =cfg.get_int("intro_frames",30);
    // init renderer
    Renderer R;R.init(ld.W,ld.H);
    R.threshold=base_thr;
    R.mode_mask=mode_mask;
    R.setMode(start_mode);
    set_title(window_title,show_hud);
    // init audio
    Audio au;
    if(audio_on&&au.open("assets/audio.wav")){
        printf("audio opened\n");
    }else{
        audio_on=0;
        fprintf(stderr,"audio disabled or unavailable\n");
    }
    Input::init();
    signal(SIGINT,on_signal);
    signal(SIGTERM,on_signal);
    atexit(cleanup);
    // enter alt screen
    fputs("\033[?1049h\033[?25l\033[2J",stdout);fflush(stdout);
    // state
    enum{PLAYING,PAUSED,QUIT}st=PLAYING;
    uint32_t frame=0;
    auto t0=std::chrono::steady_clock::now();
    auto paused_at=std::chrono::steady_clock::now();
    int poll_ms=fps_cap>0?1000/fps_cap:8;
    int flash_remaining=0;
    int first_frame_rendered=0;
    int cols=0,rows=0;
    // initial window query
    term_size(cols,rows);
    // intro rain
    if(intro_rain_on){
        R.resize(cols,rows);
        intro_rain(R,ld.frames[0].gray,cols,rows,intro_frames,poll_ms*3);
    }
    // fade in
    if(fade_in){
        R.resize(cols,rows);
        fade(R,ld.frames[0].gray,cols,rows,255,base_thr,fade_frames,poll_ms*2);
    }
    // start audio
    if(audio_on){
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        au.play();
    }
    t0=std::chrono::steady_clock::now();
    while(st!=QUIT){
        // handle keys
        while(Input::hasKey()){
            int k=Input::getKey();
            if(k=='q'||k=='Q'||k==27){st=QUIT;break;}
            if(k==' '){
                if(st==PLAYING){st=PAUSED;if(audio_on)au.pause();paused_at=std::chrono::steady_clock::now();}
                else if(st==PAUSED){st=PLAYING;if(audio_on)au.resume();t0+=(std::chrono::steady_clock::now()-paused_at);}
            }
            if(k==KEY_LEFT&&frame>0){frame--;st=PAUSED;if(audio_on)au.pause();paused_at=std::chrono::steady_clock::now();}
            if(k==KEY_RIGHT&&frame+1<ld.count){frame++;st=PAUSED;if(audio_on)au.pause();paused_at=std::chrono::steady_clock::now();}
            // restart
            if(k=='r'||k=='R'){
                frame=0;
                if(audio_on){au.stop();au.seek(0);au.play();}
                t0=std::chrono::steady_clock::now();
                st=PLAYING;
                if(jump_flash)flash_remaining=jump_flash_frames;
            }
            // jump 20..100%
            if(k>='1'&&k<='5'){
                int was_paused=(st==PAUSED);
                int pct=(k-'0')*20;
                uint32_t idx=(uint32_t)((uint64_t)ld.count*pct/100);
                if(idx>=ld.count)idx=ld.count-1;
                frame=idx;
                if(audio_on){
                    au.stop();
                    au.seek((long)ld.frames[idx].ts);
                    if(!(was_paused&&jump_keeps_pause))au.play();
                }
                t0=std::chrono::steady_clock::now()-std::chrono::milliseconds(ld.frames[idx].ts);
                if(was_paused&&jump_keeps_pause){
                    st=PAUSED;
                    paused_at=std::chrono::steady_clock::now();
                }else{
                    st=PLAYING;
                }
                if(jump_flash)flash_remaining=jump_flash_frames;
            }
            // toggle hud
            if(k=='h'||k=='H'){
                show_hud^=1;
                set_title(window_title,show_hud);
            }
            // cycle mode
            if(k=='m'||k=='M'){
                R.setMode(R.nextMode());
            }
        }
        if(st==QUIT)break;
        // query size (renderer handles realloc)
        term_size(cols,rows);
        // advance frame
        if(st==PLAYING){
            long long ms=-1;
            if(audio_on)ms=au.position();
            if(ms<0){
                ms=std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now()-t0).count();
            }
            while(frame+1<ld.count&&(long long)ld.frames[frame+1].ts<=ms)frame++;
            if(frame+1>=ld.count){
                // end of playback
                if(loop){
                    frame=0;
                    if(audio_on){au.stop();au.seek(0);au.play();}
                    t0=std::chrono::steady_clock::now();
                }else{
                    st=QUIT;
                    break;
                }
            }
        }
        // flash threshold
        if(flash_remaining>0){
            R.threshold=base_thr+80;
            if(R.threshold>255)R.threshold=255;
            flash_remaining--;
        }else{
            R.threshold=base_thr;
        }
        // progress
        if(progress_bar){
            R.progress=(int)((uint64_t)frame*1000/ld.count);
        }else{
            R.progress=-1;
        }
        // fill hud
        if(show_hud){
            R.hud_count=0;
            sprintf(R.hud[R.hud_count++],"frame %u/%u",frame,ld.count);
            sprintf(R.hud[R.hud_count++],"thr %d",R.threshold);
            sprintf(R.hud[R.hud_count++],"mode %d",R.mode);
            sprintf(R.hud[R.hud_count++],"audio %s",audio_on?"on":"off");
        }else{
            R.hud_count=0;
        }
        // render
        R.render_scaled(ld.frames[frame].gray,cols,rows);
        R.present();
        first_frame_rendered=1;
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
    }
    // fade out
    if(fade_out&&first_frame_rendered){
        term_size(cols,rows);
        R.progress=-1;
        R.hud_count=0;
        fade(R,ld.frames[frame<ld.count?frame:ld.count-1].gray,cols,rows,base_thr,255,fade_frames,poll_ms*2);
    }
    // cleanup
    if(audio_on){au.stop();au.close();}
    R.release();ld.release();
    return 0;
}