#include "keyboard.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ volatile("inb %1,%0" : "=a"(r) : "Nd"(port));
    return r;
}

static const char SCANCODE_MAP[128] = {
    0,   27,  '1','2','3','4','5','6','7','8','9','0','-','=', 8,
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,   'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0,  ' ', 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,  0,  0,  0,  0,  0,  '7','8','9','-','4','5','6',
    '+', '1','2','3','0','.', 0,  0,  0,  0,  0
};

static const char SCANCODE_SHIFT[128] = {
    0,   27,  '!','@','#','$','%','^','&','*','(',')','_','+', 8,
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,   'A','S','D','F','G','H','J','K','L',':','"','~',
    0,   '|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0,  ' ', 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,  0,  0,  0,  0,  0,  '7','8','9','-','4','5','6',
    '+', '1','2','3','0','.', 0,  0,  0,  0,  0
};

static const int BUF_SIZE = 256;
static char     buf[BUF_SIZE];
static int      buf_head = 0;
static int      buf_tail = 0;
static bool     shift    = false;
static bool     caps     = false;
static void   (*user_cb)(char) = nullptr;

static void buf_push(char c) {
    int next = (buf_head + 1) % BUF_SIZE;
    if (next != buf_tail) {
        buf[buf_head] = c;
        buf_head = next;
    }
}

void keyboard_init() {
    buf_head = buf_tail = 0;
    shift = caps = false;
    user_cb = nullptr;
}

void keyboard_set_callback(void (*cb)(char)) {
    user_cb = cb;
}

bool keyboard_haschar() {
    return buf_head != buf_tail;
}

char keyboard_getchar() {
    if (!keyboard_haschar()) return 0;
    char c = buf[buf_tail];
    buf_tail = (buf_tail + 1) % BUF_SIZE;
    return c;
}

void keyboard_handler() {
    uint8_t sc = inb(0x60);

    if (sc & 0x80) {
        uint8_t rel = sc & 0x7F;
        if (rel == 0x2A || rel == 0x36) shift = false;
        return;
    }

    if (sc == 0x2A || sc == 0x36) { shift = true; return; }

    if (sc == 0x3A) { caps = !caps; return; }

    if (sc >= 128) return;

    char c = shift ? SCANCODE_SHIFT[sc] : SCANCODE_MAP[sc];
    if (!c) return;

    if (caps && c >= 'a' && c <= 'z') c -= 32;
    if (caps && c >= 'A' && c <= 'Z' && shift) c += 32;

    buf_push(c);
    if (user_cb) user_cb(c);
}