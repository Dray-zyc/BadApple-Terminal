#include"renderer.hpp"
// decode one utf8 codepoint
static int utf8_decode(const char*p,uint32_t&cp){
    unsigned char c=*p;
    if(c<0x80){cp=c;return 1;}
    if((c>>5)==0x6){cp=((c&0x1F)<<6)|(p[1]&0x3F);return 2;}
    if((c>>4)==0xE){cp=((c&0x0F)<<12)|((p[1]&0x3F)<<6)|(p[2]&0x3F);return 3;}
    if((c>>3)==0x1E){cp=((c&0x07)<<18)|((p[1]&0x3F)<<12)|((p[2]&0x3F)<<6)|(p[3]&0x3F);return 4;}
    cp=c;return 1;
}
// display width of one codepoint
static int cp_width(uint32_t cp){
    if(cp<0x80)return 1;
    // zero-width combining marks
    if(cp>=0x0300&&cp<=0x036F)return 0;
    // CJK wide (W/F)
    if(cp>=0x1100&&cp<=0x115F)return 2;
    if(cp>=0x2E80&&cp<=0xA4CF)return 2;
    if(cp>=0xAC00&&cp<=0xD7A3)return 2;
    if(cp>=0xF900&&cp<=0xFAFF)return 2;
    if(cp>=0xFE30&&cp<=0xFE6F)return 2;
    if(cp>=0xFF00&&cp<=0xFF60)return 2;
    if(cp>=0xFFE0&&cp<=0xFFE6)return 2;
    if(cp>=0x1F300&&cp<=0x1F9FF)return 2;
    if(cp>=0x20000&&cp<=0x2FFFD)return 2;
    // CJK ambiguous - treat as wide
    if(cp==0x00B7)return 2;                // middle dot
    if(cp==0x30FB)return 2;                // katakana middle dot
    if(cp==0x2013||cp==0x2014)return 2;   // en/em dash
    if(cp==0x2015)return 2;                // horizontal bar
    if(cp==0x2018||cp==0x2019)return 2;   // single quotes
    if(cp==0x201C||cp==0x201D)return 2;   // double quotes
    if(cp==0x2026)return 2;                // ellipsis
    if(cp==0x2027)return 2;                // hyphenation point
    if(cp==0x2030)return 2;                // per mille
    if(cp>=0x2100&&cp<=0x214F)return 2;   // letterlike symbols
    if(cp>=0x2150&&cp<=0x218F)return 2;   // number forms
    if(cp>=0x2460&&cp<=0x24FF)return 2;   // enclosed alphanumerics
    if(cp>=0x25A0&&cp<=0x25FF)return 2;   // geometric shapes
    if(cp>=0x2600&&cp<=0x27BF)return 2;   // misc symbols
    // block elements must stay 1 col for renderer
    if(cp>=0x2500&&cp<=0x259F)return 1;
    return 1;
}
// draw one text line centered at row y
static void draw_line(Renderer&R,int y,const char*s){
    if(y<0||y>=R.ch)return;
    int avail=R.cw-4;
    if(avail<4)return;
    // truncate by display width
    char buf[512];
    int n=0,w=0;
    while(*s&&n<500){
        uint32_t cp;
        int len=utf8_decode(s,cp);
        int cwid=cp_width(cp);
        if(w+cwid>avail)break;
        for(int k=0;k<len&&n<500;k++)buf[n++]=s[k];
        w+=cwid;
        s+=len;
    }
    buf[n]=0;
    int x=(R.cw-w)>>1;
    if(x<2)x=2;
    int col=x,i=0;
    while(i<n&&col<R.cw-2){
        uint32_t cp;
        int len=utf8_decode(buf+i,cp);
        int cwid=cp_width(cp);
        if(col+cwid>R.cw-2)break;
        Cell&c=R.back[y*R.cw+col];
        c.ch=cp;
        c.r=c.g=c.b=255;
        for(int k=1;k<cwid;k++){
            Cell&c2=R.back[y*R.cw+col+k];
            c2.ch=0;
            c2.r=c2.g=c2.b=255;
        }
        i+=len;
        col+=cwid;
    }
}
// append one utf8 codepoint
static void utf8_append(char*buf,int&pos,uint32_t cp){
    if(cp<0x80){buf[pos++]=(char)cp;return;}
    if(cp<0x800){buf[pos++]=(char)(0xC0|(cp>>6));buf[pos++]=(char)(0x80|(cp&0x3F));return;}
    if(cp<0x10000){buf[pos++]=(char)(0xE0|(cp>>12));buf[pos++]=(char)(0x80|((cp>>6)&0x3F));buf[pos++]=(char)(0x80|(cp&0x3F));return;}
    buf[pos++]=(char)(0xF0|(cp>>18));buf[pos++]=(char)(0x80|((cp>>12)&0x3F));
    buf[pos++]=(char)(0x80|((cp>>6)&0x3F));buf[pos++]=(char)(0x80|(cp&0x3F));
}
// init defaults, no buffer yet
void Renderer::init(int w,int h){
    W=w;H=h;threshold=128;mode=0;mode_mask=0b1101;hud_count=0;progress=-1;
    subtitle_count=0;
    front=back=0;pix=0;cw=ch=0;pw=ph=0;
}
// free buffers
void Renderer::release(){
    free(front);free(back);free(pix);
    front=back=0;pix=0;cw=ch=0;pw=ph=0;
}
// change mode (no realloc needed)
void Renderer::setMode(int m){
    int mask=mode_mask&0b1101;
    if(!mask)mask=0b1101;
    mode=m&7;
    if(!(mask&(1<<mode))){
        for(int i=0;i<8;i++)if(mask&(1<<i)){mode=i;break;}
    }
    invalidate();
}
// next enabled mode by mask
int Renderer::nextMode(){
    int mask=mode_mask&0b1101;
    if(!mask)mask=0b1101;
    for(int i=1;i<=8;i++){
        int m=(mode+i)&7;
        if(mask&(1<<m))return m;
    }
    return mode;
}
// realloc when terminal size changes
void Renderer::resize(int cols,int rows){
    if(cols<1)cols=1;
    if(rows<1)rows=1;
    if(cw==cols&&ch==rows)return;
    if(front)free(front);
    if(back)free(back);
    cw=cols;ch=rows;
    front=(Cell*)malloc(cw*ch*sizeof(Cell));
    back=(Cell*)malloc(cw*ch*sizeof(Cell));
    for(int i=0;i<cw*ch;i++){
        front[i].ch=0;front[i].r=front[i].g=front[i].b=0;
        back[i].ch=' ';back[i].r=back[i].g=back[i].b=255;
    }
    // clear physical screen
    fputs("\033[2J",stdout);fflush(stdout);
}
// clear back to blank
void Renderer::clear(){
    for(int i=0;i<cw*ch;i++){back[i].ch=' ';back[i].r=back[i].g=back[i].b=255;}
}
// mark front as invalid
void Renderer::invalidate(){
    for(int i=0;i<cw*ch;i++)front[i].ch=0;
}
// box filter: source W x H gray -> pw x ph subpixel grid
static void resample(const unsigned char*g,int W,int H,unsigned char*dst,int pw,int ph){
    for(int y=0;y<ph;y++){
        int y0=y*H/ph,y1=(y+1)*H/ph;
        if(y1<=y0)y1=y0+1;
        for(int x=0;x<pw;x++){
            int x0=x*W/pw,x1=(x+1)*W/pw;
            if(x1<=x0)x1=x0+1;
            int sum=0,cnt=0;
            for(int sy=y0;sy<y1;sy++){
                const unsigned char*row=g+sy*W;
                for(int sx=x0;sx<x1;sx++){sum+=row[sx];cnt++;}
            }
            dst[y*pw+x]=(unsigned char)(sum/cnt);
        }
    }
}
// mode 0: half blocks, 1x2 subpixels per cell, pure black/white
static void draw_half(Renderer&R,int tw,int th,int offx,int offy){
    unsigned char*p=R.pix;int pw=R.pw,thr=R.threshold;
    for(int cy=0;cy<th;cy++){
        int r0=(cy*2)*pw,r1=(cy*2+1)*pw;
        int by=offy+cy;
        if(by<0||by>=R.ch)continue;
        for(int cx=0;cx<tw;cx++){
            int bx=offx+cx;
            if(bx<0||bx>=R.cw)continue;
            bool up=p[r0+cx]>=thr,dn=p[r1+cx]>=thr;
            Cell&c=R.back[by*R.cw+bx];
            c.ch=up&&dn?0x2588:up?0x2580:dn?0x2584:' ';
            c.r=c.g=c.b=255;
        }
    }
}
// mode 2: density ladder, 10 gray levels per cell
static void draw_density(Renderer&R,int tw,int th,int offx,int offy){
    static const char*dens=" .:-=+*#%@";
    unsigned char*p=R.pix;int pw=R.pw;
    for(int cy=0;cy<th;cy++){
        int r0=(cy*2)*pw,r1=(cy*2+1)*pw;
        int by=offy+cy;
        if(by<0||by>=R.ch)continue;
        for(int cx=0;cx<tw;cx++){
            int bx=offx+cx;
            if(bx<0||bx>=R.cw)continue;
            int v=(p[r0+cx]+p[r1+cx])>>1;
            Cell&c=R.back[by*R.cw+bx];
            c.ch=(unsigned char)dens[v*9/255];
            c.r=c.g=c.b=255;
        }
    }
}
// mode 3: braille dots, 2x4 subpixels per cell
static void draw_braille(Renderer&R,int tw,int th,int offx,int offy){
    static const int mp[2][4]={{0,1,2,6},{3,4,5,7}};
    unsigned char*p=R.pix;int pw=R.pw,thr=R.threshold;
    for(int cy=0;cy<th;cy++){
        int r0=(cy*4)*pw;
        int by=offy+cy;
        if(by<0||by>=R.ch)continue;
        for(int cx=0;cx<tw;cx++){
            int bx=offx+cx;
            if(bx<0||bx>=R.cw)continue;
            uint32_t bits=0;
            for(int dy=0;dy<4;dy++){
                const unsigned char*row=p+(r0+dy*pw)+cx*2;
                if(row[0]>=thr)bits|=1u<<mp[0][dy];
                if(row[1]>=thr)bits|=1u<<mp[1][dy];
            }
            Cell&c=R.back[by*R.cw+bx];
            c.ch=0x2800+bits;
            c.r=c.g=c.b=255;
        }
    }
}
// scaled render to full terminal
void Renderer::render_scaled(const unsigned char*gray,int cols,int rows){
    resize(cols,rows);
    clear();
    // pixel per cell: braille packs 2x4, others 1x2
    int pcw=(mode==3)?2:1,pch=(mode==3)?4:2;
    // best fit in cw x ch keeping aspect
    long long lhs=(long long)cw*H*pcw;
    long long rhs=(long long)ch*W*pch;
    int tw,th;
    if(lhs<=rhs){tw=cw;th=(int)((long long)cw*H*pcw/(W*pch));}
    else        {th=ch;tw=(int)((long long)ch*W*pch/(H*pcw));}
    if(tw<1)tw=1;
    if(th<1)th=1;
    // centered in buffer
    int offx=(cw-tw)>>1,offy=(ch-th)>>1;
    // resample source into subpixel grid
    int nw=tw*pcw,nh=th*pch;
    if(nw!=pw||nh!=ph){
        free(pix);
        pix=(unsigned char*)malloc((size_t)nw*nh);
        if(!pix){pw=ph=0;return;}
        pw=nw;ph=nh;
    }
    resample(gray,W,H,pix,pw,ph);
    if(mode==3)draw_braille(*this,tw,th,offx,offy);
    else if(mode==2)draw_density(*this,tw,th,offx,offy);
    else draw_half(*this,tw,th,offx,offy);
    // overlay hud on left margin
    if(hud_count>0){
        int maxw=0;
        for(int i=0;i<hud_count;i++){
            int l=(int)strlen(hud[i]);
            if(l>maxw)maxw=l;
        }
        if(offx>=maxw+2){
            for(int i=0;i<hud_count&&i<ch;i++){
                int l=(int)strlen(hud[i]);
                for(int j=0;j<l;j++){
                    Cell&c=back[i*cw+j];
                    c.ch=(unsigned char)hud[i][j];
                    c.r=c.g=c.b=255;
                }
            }
        }
    }
    // draw progress bar on bottom row
    if(progress>=0&&ch>=2){
        int y=ch-1;
        int inner=cw-8;
        if(inner>120)inner=120;
        if(inner<4)inner=4;
        char bar[160];
        int pos=0;
        int fill=inner*progress/1000;
        bar[pos++]='[';
        for(int i=0;i<inner;i++)bar[pos++]=(i<fill)?'=':' ';
        bar[pos++]=']';
        bar[pos++]=' ';
        int pct=progress/10;
        if(pct<10){bar[pos++]=' ';bar[pos++]='0'+pct;}
        else if(pct<100){bar[pos++]='0'+pct/10;bar[pos++]='0'+pct%10;}
        else{bar[pos++]='0'+pct/100;bar[pos++]='0'+(pct/10)%10;bar[pos++]='0'+pct%10;}
        bar[pos++]='%';
        int x0=(cw-pos)>>1;
        if(x0<0)x0=0;
        for(int i=0;i<pos&&x0+i<cw;i++){
            Cell&c=back[y*cw+x0+i];
            c.ch=(unsigned char)bar[i];
            c.r=c.g=c.b=255;
        }
    }
    // draw subtitles above progress bar
    if(subtitle_count>0){
        int base=ch-1-(progress>=0?1:0);
        for(int i=0;i<subtitle_count;i++){
            int y=base-(subtitle_count-1-i);
            if(y>=0)draw_line(*this,y,subtitle[i]);
        }
    }
}
// diff update stdout
void Renderer::present(){
    static char buf[1<<21];
    int pos=0;
    pos+=sprintf(buf+pos,"\033[H");
    int lastcx=-1,lastcy=-2;
    int cr=-1,cg=-1,cb=-1;
    int last_w=0;                  // width of last emitted char
    for(int cy=0;cy<ch;cy++){
        for(int cx=0;cx<cw;cx++){
            int id=cy*cw+cx;
            Cell&b=back[id];
            Cell&f=front[id];
            if(b==f)continue;
            // skip placeholder of wide chars
            if(b.ch==0){
                f=b;
                continue;
            }
            // force relocation after wide char (conhost bug)
            if(cx!=lastcx+1||cy!=lastcy||last_w==2)
                pos+=sprintf(buf+pos,"\033[%d;%dH",cy+1,cx+1);
            if((int)b.r!=cr||(int)b.g!=cg||(int)b.b!=cb){
                pos+=sprintf(buf+pos,"\033[38;2;%d;%d;%dm",b.r,b.g,b.b);
                cr=b.r;cg=b.g;cb=b.b;
            }
            utf8_append(buf,pos,b.ch);
            lastcx=cx;lastcy=cy;
            last_w=cp_width(b.ch);   // record width
        }
    }
    fwrite(buf,1,pos,stdout);
    fflush(stdout);
    Cell*tmp=front;front=back;back=tmp;
}
