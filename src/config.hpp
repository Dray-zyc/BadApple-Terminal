#pragma once
// generic key-value config
struct Config{
    struct Entry{char key[32];int val;};
    Entry entries[128];
    int count=0;
    // load flat json
    bool load(const char*path);
    // typed getters with default
    int  get_int (const char*key,int def)const;
    bool get_bool(const char*key,bool def)const;
    // insert or update
    void set_int(const char*key,int v);
};