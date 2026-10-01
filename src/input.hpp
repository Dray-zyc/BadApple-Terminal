#pragma once
// special key codes
enum{KEY_NONE=0,KEY_LEFT=1000,KEY_RIGHT,KEY_UP,KEY_DOWN};
// non-blocking input
struct Input{
    static void init();
    static void restore();
    static bool hasKey();
    static int  getKey();
};