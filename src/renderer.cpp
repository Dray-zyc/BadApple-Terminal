#include"renderer.hpp"
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
    front=back=0;cw=ch=0;
}
// free buffers
void Renderer::release(){free(front);free(back);front=back=0;cw=ch=0;}
// change mode (no realloc needed)
void Renderer::setMode(int m){
    mode=m;
    invalidate();
}
// next enabled mode by mask
int Renderer::nextMode(){
    for(int i=1;i<=8;i++){
        int m=(mode+i)&7;
        if(mode_mask&(1<<m))return m;
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
// draw cells for half/full/density/color modes
static void draw_half(Renderer&R,const unsigned char*g,int tw,int th,int offx,int offy){
    static const char*dens=" .:-=+*#%@";
    for(int cy=0;cy<th;cy++){
        for(int cx=0;cx<tw;cx++){
            int bx=offx+cx,by=offy+cy;
            if(bx<0||bx>=R.cw||by<0||by>=R.ch)continue;
            int sx=cx*R.W/tw;
            int sy0=(cy*2)*R.H/(th*2);
            int sy1=(cy*2+1)*R.H/(th*2);
            if(sx>=R.W)sx=R.W-1;
            if(sy0>=R.H)sy0=R.H-1;
            if(sy1>=R.H)sy1=R.H-1;
            int v0=g[sy0*R.W+sx],v1=g[sy1*R.W+sx];
            Cell&c=R.back[by*R.cw+bx];
            c.r=c.g=c.b=255;
            if(R.mode==0){
                bool up=v0>=R.threshold,dn=v1>=R.threshold;
                c.ch=up&&dn?0x2588:up?0x2580:dn?0x2584:' ';
            }else if(R.mode==1){
                int v=(v0+v1)>>1;
                c.ch=v>=R.threshold?0x2588:' ';
            }else if(R.mode==2){
                int v=(v0+v1)>>1;
                c.ch=dens[v*9/255];
            }else if(R.mode==4){
                bool up=v0>=R.threshold,dn=v1>=R.threshold;
                c.ch=up&&dn?0x2588:up?0x2580:dn?0x2584:' ';
                if(c.ch!=' '){
                    int x=sx,y=(sy0+sy1)>>1;
                    int gx=0,gy=0;
                    if(x>0&&x<R.W-1)gx=g[y*R.W+x+1]-g[y*R.W+x-1];
                    if(y>0&&y<R.H-1)gy=g[(y+1)*R.W+x]-g[(y-1)*R.W+x];
                    int mag=(gx<0?-gx:gx)+(gy<0?-gy:gy);
                    if(mag>60){c.r=0;c.g=255;c.b=255;}
                }
            }
        }
    }
}
// draw cells for braille mode
static void draw_braille(Renderer&R,const unsigned char*g,int tw,int th,int offx,int offy){
    static const int mp[2][4]={{0,1,2,6},{3,4,5,7}};
    for(int cy=0;cy<th;cy++){
        for(int cx=0;cx<tw;cx++){
            int bx=offx+cx,by=offy+cy;
            if(bx<0||bx>=R.cw||by<0||by>=R.ch)continue;
            uint32_t bits=0;
            for(int dy=0;dy<4;dy++){
                for(int dx=0;dx<2;dx++){
                    int sx=(cx*2+dx)*R.W/(tw*2);
                    int sy=(cy*4+dy)*R.H/(th*4);
                    if(sx>=R.W)sx=R.W-1;
                    if(sy>=R.H)sy=R.H-1;
                    if(g[sy*R.W+sx]>=R.threshold)bits|=1u<<mp[dx][dy];
                }
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
    // pixel per cell
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
    if(mode==3)draw_braille(*this,gray,tw,th,offx,offy);
    else       draw_half(*this,gray,tw,th,offx,offy);
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
}
// diff update stdout
void Renderer::present(){
    static char buf[1<<21];
    int pos=0;
    pos+=sprintf(buf+pos,"\033[H");
    int lastcx=-1,lastcy=-2;
    int cr=-1,cg=-1,cb=-1;
    for(int cy=0;cy<ch;cy++){
        for(int cx=0;cx<cw;cx++){
            int id=cy*cw+cx;
            Cell&b=back[id];
            Cell&f=front[id];
            if(b==f)continue;
            if(cx!=lastcx+1||cy!=lastcy)pos+=sprintf(buf+pos,"\033[%d;%dH",cy+1,cx+1);
            if((int)b.r!=cr||(int)b.g!=cg||(int)b.b!=cb){
                pos+=sprintf(buf+pos,"\033[38;2;%d;%d;%dm",b.r,b.g,b.b);
                cr=b.r;cg=b.g;cb=b.b;
            }
            utf8_append(buf,pos,b.ch);
            lastcx=cx;lastcy=cy;
        }
    }
    fwrite(buf,1,pos,stdout);
    fflush(stdout);
    Cell*tmp=front;front=back;back=tmp;
}