#include"config.hpp"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
// skip whitespace
static const char*skip_ws(const char*p){
    while(*p&&isspace((unsigned char)*p))p++;
    return p;
}
// parse int or bool literal
static const char*parse_value(const char*p,int&out){
    p=skip_ws(p);
    if(!strncmp(p,"true",4)){out=1;return p+4;}
    if(!strncmp(p,"false",5)){out=0;return p+5;}
    char*end;out=(int)strtol(p,&end,10);
    return end;
}
// parse quoted key
static const char*parse_key(const char*p,char*buf,int n){
    p=skip_ws(p);
    if(*p!='"')return 0;
    p++;
    int i=0;
    while(*p&&*p!='"'&&i<n-1)buf[i++]=*p++;
    buf[i]=0;
    if(*p=='"')p++;
    return p;
}
// find entry index, -1 if missing
static int find_entry(const Config&C,const char*key){
    for(int i=0;i<C.count;i++)if(!strcmp(C.entries[i].key,key))return i;
    return -1;
}
// get int or default
int Config::get_int(const char*key,int def)const{
    int i=find_entry(*this,key);
    return i<0?def:entries[i].val;
}
// get bool or default
bool Config::get_bool(const char*key,bool def)const{
    int i=find_entry(*this,key);
    return i<0?def:(entries[i].val!=0);
}
// set or insert
void Config::set_int(const char*key,int v){
    int i=find_entry(*this,key);
    if(i>=0){entries[i].val=v;return;}
    if(count>=128)return;
    strncpy(entries[count].key,key,31);
    entries[count].key[31]=0;
    entries[count].val=v;
    count++;
}
// load flat json object
bool Config::load(const char*path){
    FILE*f=fopen(path,"r");
    if(!f)return false;
    fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);
    char*buf=(char*)malloc(sz+1);
    fread(buf,1,sz,f);buf[sz]=0;fclose(f);
    const char*p=buf;
    count=0;
    p=skip_ws(p);
    if(*p!='{'){free(buf);return false;}
    p++;
    while(*p){
        p=skip_ws(p);
        if(*p=='}')break;
        if(*p==','){p++;continue;}
        char key[32];
        p=parse_key(p,key,sizeof(key));
        if(!p){free(buf);return false;}
        p=skip_ws(p);
        if(*p!=':'){free(buf);return false;}
        p++;
        int v;
        p=parse_value(p,v);
        set_int(key,v);
    }
    free(buf);
    return true;
}