#include"loader.hpp"
#include"renderer.hpp"
#include"input.hpp"
#include"audio.hpp"
#include"config.hpp"
#include"term.hpp"
#include"subtitle.hpp"
#include<chrono>
#include<thread>
#include<stdlib.h>
#include<stdio.h>
#include<signal.h>
#ifdef _WIN32
#include<windows.h>
#include<direct.h>
#include<mmsystem.h>
// high resolution waitable timer, 0 if unavailable
static HANDLE wait_timer=0;
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
    #ifdef _WIN32
    if(wait_timer){CloseHandle(wait_timer);wait_timer=0;}
    timeEndPeriod(1);
    #endif
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
// sleep until deadline, last part busy-waits for sub-ms accuracy
static void wait_until(std::chrono::steady_clock::time_point tp){
    long long us=std::chrono::duration_cast<std::chrono::microseconds>(
        tp-std::chrono::steady_clock::now()).count();
    #ifdef _WIN32
    if(wait_timer&&us>400){
        // high resolution timer, ~0.3ms late, no cpu burn
        LARGE_INTEGER due;
        due.QuadPart=-(us-300)*10;
        if(SetWaitableTimer(wait_timer,&due,0,0,0,0))
            WaitForSingleObject(wait_timer,INFINITE);
    }else if(!wait_timer&&us>12000){
        // coarse system timer: sleep all but one tick
        std::this_thread::sleep_for(std::chrono::microseconds(us-11000));
    }
    #else
    if(us>2000)std::this_thread::sleep_for(std::chrono::microseconds(us-1000));
    #endif
    while(std::chrono::steady_clock::now()<tp)std::this_thread::yield();
}
// run fade in/out
static void fade(Renderer&R,const unsigned char*gray,int cols,int rows,int from,int to,int frames,int sleep_us){
    for(int i=0;i<frames;i++){
        R.threshold=from+(to-from)*i/frames;
        R.render_scaled(gray,cols,rows);
        R.present();
        std::this_thread::sleep_for(std::chrono::microseconds(sleep_us>0?sleep_us:16000));
    }
    R.threshold=to;
}
// run noise convergence intro
static void intro_rain(Renderer&R,const unsigned char*gray,int cols,int rows,int frames,int sleep_us){
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
        std::this_thread::sleep_for(std::chrono::microseconds(sleep_us>0?sleep_us:33000));
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
    // read all config
    int base_thr        =cfg.get_int("threshold",128);
    int mode_mask       =cfg.get_int("mode_mask",0b1101);
    int start_mode      =cfg.get_int("mode",0);
    int fps             =cfg.get_int("fps",30);
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
    int sub_lang   =cfg.get_int("subtitle",0);
    int sub_info_on=cfg.get_int("subtitle_info",0);
    // fps limits
    if(fps<1)fps=30;
    if(fps>240)fps=240;
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
    Subtitle sub_lyric,sub_info;
    sub_lyric.init();sub_info.init();
    if(sub_lang==1)sub_lyric.load("assets/subtitles/lyrics_ja.srt");
    else if(sub_lang==2)sub_lyric.load("assets/subtitles/lyrics_zh.srt");
    if(sub_info_on)sub_info.load("assets/subtitles/characters.srt");
    Input::init();
    signal(SIGINT,on_signal);
    signal(SIGTERM,on_signal);
    atexit(cleanup);
    // fine timer for high refresh rates
    #ifdef _WIN32
    timeBeginPeriod(1);
    wait_timer=CreateWaitableTimerExW(0,0,0x00000002,TIMER_ALL_ACCESS);
    if(wait_timer==INVALID_HANDLE_VALUE)wait_timer=0;
    #endif
    // enter alt screen
    fputs("\033[?1049h\033[?25l\033[2J",stdout);fflush(stdout);
    // state
    enum{PLAYING,PAUSED,QUIT}st=PLAYING;
    uint32_t frame=0;
    auto t0=std::chrono::steady_clock::now();
    auto paused_at=std::chrono::steady_clock::now();
    int frame_us=1000000/fps;
    int flash_remaining=0;
    int first_frame_rendered=0;
    int cols=0,rows=0;
    // last drawn state, used to skip untouched frames
    uint32_t last_frame=0xFFFFFFFFu;
    int last_thr=-1,last_mode=-1,last_cw=-1,last_ch=-1;
    int redraw=0;
    // initial window query
    term_size(cols,rows);
    // intro rain
    const unsigned char*g0=ld.frame(0);
    if(intro_rain_on&&g0){
        R.resize(cols,rows);
        intro_rain(R,g0,cols,rows,intro_frames,frame_us*3);
    }
    // fade in
    if(fade_in&&g0){
        R.resize(cols,rows);
        fade(R,g0,cols,rows,255,base_thr,fade_frames,frame_us*2);
    }
    // start audio
    if(audio_on){
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        au.play();
    }
    t0=std::chrono::steady_clock::now();
    auto next=t0;
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
                sub_lyric.reset();sub_info.reset();
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
                    au.seek((long)ld.idx[idx].ts);
                    if(!(was_paused&&jump_keeps_pause))au.play();
                }
                t0=std::chrono::steady_clock::now()-std::chrono::milliseconds(ld.idx[idx].ts);
                if(was_paused&&jump_keeps_pause){
                    st=PAUSED;
                    paused_at=std::chrono::steady_clock::now();
                }else{
                    st=PLAYING;
                }
                if(jump_flash)flash_remaining=jump_flash_frames;
                sub_lyric.reset();sub_info.reset();
            }
            // toggle hud
            if(k=='h'||k=='H'){
                show_hud^=1;
                redraw=1;
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
            while(frame+1<ld.count&&(long long)ld.idx[frame+1].ts<=ms)frame++;
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
        // fill subtitle
        R.subtitle_count=0;
        if(sub_info_on){
            const char*i=sub_info.update(ld.idx[frame].ts);
            if(i&&*i)strncpy(R.subtitle[R.subtitle_count++],i,127);
        }
        if(sub_lang){
            const char*l=sub_lyric.update(ld.idx[frame].ts);
            if(l&&*l)strncpy(R.subtitle[R.subtitle_count++],l,127);
        }
        // fill hud
        if(show_hud){
            R.hud_count=0;
            sprintf(R.hud[R.hud_count++],"frame %u/%u",frame,ld.count);
            sprintf(R.hud[R.hud_count++],"thr %d",R.threshold);
            sprintf(R.hud[R.hud_count++],"mode %d",R.mode);
            sprintf(R.hud[R.hud_count++],"fps %d %s",fps,audio_on?"audio on":"audio off");
        }else{
            R.hud_count=0;
        }
        // draw only when the picture actually changed
        if(redraw||frame!=last_frame||R.threshold!=last_thr||R.mode!=last_mode||cols!=last_cw||rows!=last_ch){
            const unsigned char*g=ld.frame(frame);
            if(g){
                R.render_scaled(g,cols,rows);
                R.present();
                first_frame_rendered=1;
                redraw=0;
                last_frame=frame;
                last_thr=R.threshold;
                last_mode=R.mode;
                last_cw=cols;
                last_ch=rows;
            }
        }
        // pace to target fps
        next+=std::chrono::microseconds(frame_us);
        if(next<std::chrono::steady_clock::now())
            next=std::chrono::steady_clock::now();
        wait_until(next);
    }
    // fade out
    if(fade_out&&first_frame_rendered){
        term_size(cols,rows);
        R.progress=-1;
        R.hud_count=0;
        const unsigned char*g=ld.frame(frame<ld.count?frame:ld.count-1);
        if(g)fade(R,g,cols,rows,base_thr,255,fade_frames,frame_us*2);
    }
    // cleanup
    if(audio_on){au.stop();au.close();}
    R.release();ld.release();
    return 0;
}
