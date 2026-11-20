#include"subtitle.hpp"
// parse one timestamp HH:MM:SS,mmm
static uint32_t parse_ts(const char*p){
    int h=0,m=0,s=0,ms=0;
    sscanf(p,"%d:%d:%d,%d",&h,&m,&s,&ms);
    return (uint32_t)(((h*60+m)*60+s)*1000+ms);
}
// ensure capacity
static void grow(Subtitle&S){
    if(S.count<S.cap)return;
    S.cap=S.cap?S.cap*2:64;
    S.cues=(SubCue*)realloc(S.cues,S.cap*sizeof(SubCue));
}
// load from srt file
bool Subtitle::load(const char*path){
    FILE*f=fopen(path,"rb");
    if(!f)return false;
    char line[512];
    int state=0;
    uint32_t start=0,end=0;
    while(fgets(line,sizeof(line),f)){
        int n=(int)strlen(line);
        while(n>0&&(line[n-1]=='\n'||line[n-1]=='\r'))line[--n]=0;
        if(n==0){state=0;continue;}
        if(state==0){
            char*p=strstr(line,"-->");
            if(p){start=parse_ts(line);end=parse_ts(p+3);state=1;}
        }else if(state==1){
            grow(*this);
            cues[count].start=start;
            cues[count].end=end;
            snprintf(cues[count].text,128,"%s",line);
            cues[count].text[127]=0;
            count++;
            state=2;
        }
    }
    fclose(f);
    return true;
}
// update current cue by time
const char*Subtitle::update(uint32_t ms){
    if(cur>0&&ms<cues[cur-1].end)cur=0;
    while(cur<count&&cues[cur].end<ms)cur++;
    if(cur<count&&cues[cur].start<=ms&&ms<cues[cur].end)
        return cues[cur].text;
    return "";
}