#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* --- 1. SYSTEM DEFINITIONS --- */
#define SCREEN_W 80
#define SCREEN_H 25
#define VGA_ADDR 0xB8000
#define SHUTDOWN_PORT 0x604
#define SHUTDOWN_CMD  0x2000

/* VGA Colors */
enum vga_color {
    BLACK = 0, BLUE = 1, GREEN = 2, CYAN = 3, RED = 4, MAGENTA = 5, BROWN = 6, LIGHT_GREY = 7,
    DARK_GREY = 8, LIGHT_BLUE = 9, LIGHT_GREEN = 10, LIGHT_CYAN = 11, LIGHT_RED = 12, LIGHT_MAGENTA = 13, YELLOW = 14, WHITE = 15,
};

enum os_state { STATE_BOOT, STATE_LOGIN, STATE_DESKTOP };
enum os_state current_state = STATE_BOOT;

/* GLOBAL SETTINGS */
int MOUSE_SPEED = 1; 
uint8_t DESKTOP_COLOR = CYAN;
char USERNAME[20] = "Leo";
char PASSWORD[20] = "123";
long system_ticks = 0; // Uptime counter

/* FILE SYSTEM (RAM DISK) */
typedef struct {
    char name[16];
    char content[2048]; // 2KB per file (enough for Paint images)
    bool used;
    bool is_image;
} File;

File ram_disk[10]; 
char clipboard[512] = ""; 

/* GUI STATE */
uint16_t back_buffer[SCREEN_W * SCREEN_H]; 
uint8_t paint_canvas[60 * 20]; 
int settings_edit_mode = 0; 

/* APP STATE: NOTEPAD */
char np_buffer[512] = ""; 
char np_filename[16] = "Untitled.txt";
int np_cursor = 0; 
int np_menu_open = 0; 
bool np_bold = false;

/* DIALOGS */
bool show_save_dialog = false; 
bool show_load_dialog = false;
char dialog_input_buf[16] = ""; 
int dialog_mode = 0; // 1=Notepad Save, 2=Paint Save, 3=Paint Load

/* APP STATE: PAINT */
int paint_menu = 0; 
uint8_t paint_color = BLACK; 
char paint_filename[16] = "drawing.png";

/* APP STATE: CALCULATOR */
int calc_acc = 0, calc_curr = 0; 
char calc_op = 0; 
bool calc_new_entry = true;

/* APP STATE: SNAKE GAME */
int snake_x[100], snake_y[100]; 
int snake_len = 3; 
int snake_dir = 1; 
int food_x = 10, food_y = 10;
bool game_over = false; 
int game_tick = 0;

/* LOGIN STATE */
char login_user[20] = ""; 
char login_pass[20] = "";
bool login_focus_pass = false; 
bool shift_pressed = false;

/* --- 2. LOW LEVEL I/O --- */
static inline uint8_t inb(uint16_t port) { uint8_t ret; asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) ); return ret; }
static inline void outb(uint16_t port, uint8_t val) { asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) ); }
static inline void outw(uint16_t port, uint16_t val) { asm volatile ( "outw %0, %1" : : "a"(val), "Nd"(port) ); }
void sys_shutdown() { outw(SHUTDOWN_PORT, SHUTDOWN_CMD); asm volatile("hlt"); }
void sys_reboot() { uint8_t good = 0x02; while (good & 0x02) good = inb(0x64); outb(0x64, 0xFE); asm volatile("hlt"); }

/* --- 3. UTILS --- */
int strlen(const char* str) { int len = 0; while (str[len]) len++; return len; }
bool streq(const char* s1, const char* s2) { int i = 0; while(s1[i] && s2[i]) { if(s1[i] != s2[i]) return false; i++; } return s1[i] == s2[i]; }
void strcpy_safe(char* dest, const char* src, int max) { int i=0; while(src[i] && i<max-1) { dest[i]=src[i]; i++; } dest[i]=0; }
void memset(void *dest, int val, size_t len) { unsigned char *ptr = dest; while (len-- > 0) *ptr++ = val; }
int rand_pseudo() { static int seed = 12345; seed = seed * 1103515245 + 12345; return (unsigned int)(seed/65536) % 32768; }

/* --- 4. GRAPHICS ENGINE --- */
uint16_t vga_entry(unsigned char uc, uint8_t color) { return (uint16_t) uc | (uint16_t) color << 8; }
void draw_rect(int x, int y, int w, int h, uint8_t bg, uint8_t fg, char fill) {
    for (int r = 0; r < h; r++) { for (int c = 0; c < w; c++) {
            int cx = x + c, cy = y + r;
            if (cx >= 0 && cx < SCREEN_W && cy >= 0 && cy < SCREEN_H) back_buffer[cy * SCREEN_W + cx] = vga_entry(fill, bg << 4 | fg);
    }}
}
void draw_text(int x, int y, const char* text, uint8_t bg, uint8_t fg) {
    int i = 0; while (text[i] != 0) {
        if (x+i >= 0 && x+i < SCREEN_W && y >= 0 && y < SCREEN_H) back_buffer[y * SCREEN_W + (x+i)] = vga_entry(text[i], bg << 4 | fg);
        i++;
    }
}
void draw_number(int x, int y, int num, uint8_t bg, uint8_t fg) {
    char buf[16]; int i = 0; if (num == 0) { buf[0] = '0'; buf[1] = 0; }
    else { int n = num; if (n < 0) n = -n; while (n > 0) { buf[i++] = (n % 10) + '0'; n /= 10; } if (num < 0) buf[i++] = '-'; buf[i] = 0; for(int j=0; j<i/2; j++) { char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t; } }
    draw_text(x, y, buf, bg, fg);
}
void buffer_swap() { uint16_t* vga = (uint16_t*) VGA_ADDR; for (int i = 0; i < SCREEN_W * SCREEN_H; i++) vga[i] = back_buffer[i]; }

/* --- 5. WINDOW SYSTEM --- */
int mouse_x = 40, mouse_y = 12; uint8_t mouse_cycle = 0; int8_t mouse_byte[3]; bool mouse_left = false;
char kbd_map[128] = { 0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0 };
char kbd_map_shift[128] = { 0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0 };

typedef struct { int id; int x, y, w, h; char title[20]; bool visible; int active_tab; } Window;
Window win_notepad  = {1, 5, 3, 40, 18, "Gemini Notepad", false, 0};
Window win_calc     = {2, 50, 5, 22, 14, "Calculator", false, 0};
Window win_settings = {3, 15, 4, 34, 16, "Settings", false, 0}; 
Window win_paint    = {4, 10, 2, 40, 20, "Paint", false, 0};
Window win_files    = {5, 20, 5, 30, 15, "File Explorer", false, 0};
Window win_snake    = {6, 25, 5, 30, 15, "Snake Game", false, 0};
Window* windows[] = {&win_notepad, &win_calc, &win_settings, &win_paint, &win_files, &win_snake};
Window* drag_win = NULL; Window* resize_win = NULL; int drag_off_x = 0, drag_off_y = 0; bool start_open = false;

/* --- 6. FILE SYSTEM --- */
void fs_save(const char* name, const char* content, bool is_img) {
    int len = is_img ? 1200 : 512;
    for(int i=0; i<10; i++) { if(ram_disk[i].used && streq(ram_disk[i].name, name)) { for(int j=0; j<len; j++) ram_disk[i].content[j] = content[j]; ram_disk[i].is_image = is_img; return; } }
    for(int i=0; i<10; i++) { if(!ram_disk[i].used) { strcpy_safe(ram_disk[i].name, name, 16); for(int j=0; j<len; j++) ram_disk[i].content[j] = content[j]; ram_disk[i].is_image = is_img; ram_disk[i].used = true; return; } }
}
void fs_load_paint(const char* name) {
    for(int i=0; i<10; i++) { if(ram_disk[i].used && streq(ram_disk[i].name, name)) { for(int j=0; j<1200; j++) paint_canvas[j] = (uint8_t)ram_disk[i].content[j]; strcpy_safe(paint_filename, ram_disk[i].name, 16); win_paint.visible=true; return; } }
}
void fs_load_txt(const char* name) {
    for(int i=0; i<10; i++) { if(ram_disk[i].used && streq(ram_disk[i].name, name)) { strcpy_safe(np_buffer, ram_disk[i].content, 512); strcpy_safe(np_filename, ram_disk[i].name, 16); np_cursor = strlen(np_buffer); win_notepad.visible = true; return; } }
}

/* --- 7. LOGIC: SNAKE --- */
void reset_snake() { snake_len = 3; snake_x[0]=15; snake_y[0]=8; snake_dir=1; game_over=false; }
void update_snake() {
    if(!win_snake.visible || game_over) return;
    game_tick++; if(game_tick < 3000) return; game_tick = 0;
    for(int i=snake_len; i>0; i--) { snake_x[i] = snake_x[i-1]; snake_y[i] = snake_y[i-1]; }
    if(snake_dir==0) snake_y[0]--; else if(snake_dir==1) snake_x[0]++; else if(snake_dir==2) snake_y[0]++; else if(snake_dir==3) snake_x[0]--;
    if(snake_x[0] < 0 || snake_x[0] >= 28 || snake_y[0] < 0 || snake_y[0] >= 11) game_over = true;
    for(int i=1; i<snake_len; i++) if(snake_x[0]==snake_x[i] && snake_y[0]==snake_y[i]) game_over = true;
    if(snake_x[0] == food_x && snake_y[0] == food_y) { snake_len++; food_x = rand_pseudo() % 28; food_y = rand_pseudo() % 11; }
}

/* --- 8. RENDERERS --- */
void render_snake_win(Window* w) {
    int gx = w->x+1, gy = w->y+2; draw_rect(gx, gy, w->w-2, w->h-4, BLACK, GREEN, ' ');
    if(game_over) { draw_text(gx+8, gy+5, "GAME OVER", BLACK, RED); return; }
    for(int i=0; i<snake_len; i++) draw_text(gx + snake_x[i], gy + snake_y[i], "O", BLACK, GREEN);
    draw_text(gx + food_x, gy + food_y, "*", BLACK, RED);
    draw_text(w->x+2, w->y+w->h-2, "Score:", LIGHT_GREY, BLACK); draw_number(w->x+9, w->y+w->h-2, (snake_len-3)*10, LIGHT_GREY, BLACK);
}

void render_paint(Window* w) {
    draw_rect(w->x+1, w->y+2, w->w-2, 1, LIGHT_GREY, BLACK, ' ');
    draw_text(w->x+2, w->y+2, "File", (paint_menu==1)?BLUE:LIGHT_GREY, (paint_menu==1)?WHITE:BLACK);
    draw_text(w->x+8, w->y+2, "Edit", (paint_menu==2)?BLUE:LIGHT_GREY, (paint_menu==2)?WHITE:BLACK);
    uint8_t pals[] = { BLACK, RED, GREEN, BLUE, CYAN, BROWN, YELLOW, MAGENTA, LIGHT_RED };
    for(int i=0; i<9; i++) { draw_rect(w->x + 14 + (i*2), w->y + 2, 2, 1, pals[i], pals[i], ' '); if (paint_color == pals[i]) draw_text(w->x + 14 + (i*2), w->y + 2, "^", pals[i], WHITE); }
    int cx = w->x+1, cy = w->y+3, cw = w->w-2, ch = w->h-4; draw_rect(cx, cy, cw, ch, WHITE, WHITE, ' ');
    for(int r=0; r<20; r++) for(int c=0; c<60; c++) if(r<ch && c<cw && paint_canvas[r*60+c]!=0xFF) draw_rect(cx+c, cy+r, 1, 1, paint_canvas[r*60+c], paint_canvas[r*60+c], 219);
    
    // SOLID MENUS TO FIX GLITCHES
    if(paint_menu == 1) { 
        draw_rect(w->x+2, w->y+3, 10, 4, WHITE, BLACK, ' '); 
        draw_rect(w->x+3, w->y+4, 10, 4, DARK_GREY, BLACK, 0); // Shadow
        draw_text(w->x+3, w->y+3, "New", WHITE, BLACK); draw_text(w->x+3, w->y+4, "Open", WHITE, BLACK); draw_text(w->x+3, w->y+5, "Save As", WHITE, BLACK);
    }
    if(paint_menu == 2) { 
        draw_rect(w->x+8, w->y+3, 16, 5, WHITE, BLACK, ' '); 
        draw_text(w->x+9, w->y+3, "480p (4:3)", WHITE, BLACK); draw_text(w->x+9, w->y+4, "720p (16:9)", WHITE, BLACK); draw_text(w->x+9, w->y+5, "Tiny", WHITE, BLACK); draw_text(w->x+9, w->y+6, "Full", WHITE, BLACK); 
    }
}

void render_notepad(Window* w) {
    draw_rect(w->x+1, w->y+2, w->w-2, 1, LIGHT_GREY, BLACK, ' ');
    draw_text(w->x+2, w->y+2, "File", (np_menu_open==1)?BLUE:LIGHT_GREY, (np_menu_open==1)?WHITE:BLACK);
    draw_text(w->x+8, w->y+2, "Edit", (np_menu_open==2)?BLUE:LIGHT_GREY, (np_menu_open==2)?WHITE:BLACK);
    draw_text(w->x+w->w-10, w->y+2, np_filename, LIGHT_GREY, DARK_GREY);
    int tx=w->x+1, ty=w->y+3, tw=w->w-2, th=w->h-4;
    draw_rect(tx, ty, tw, th, WHITE, np_bold?WHITE:LIGHT_GREY, ' ');
    int cx=0, cy=0; for(int i=0; i<strlen(np_buffer); i++) {
        if(cy >= th) break; if(np_buffer[i]=='\n') { cx=0; cy++; continue; }
        draw_text(tx+cx, ty+cy, (char[]){np_buffer[i],0}, WHITE, np_bold?WHITE:BLACK); cx++; if(cx>=tw) { cx=0; cy++; }
    }
    if(cy<th) draw_text(tx+cx, ty+cy, "_", WHITE, BLACK);
    
    // SOLID MENUS
    if(np_menu_open == 1) { 
        draw_rect(w->x+2, w->y+3, 10, 4, WHITE, BLACK, ' ');
        draw_rect(w->x+3, w->y+4, 10, 4, DARK_GREY, BLACK, 0); // Shadow
        draw_text(w->x+3, w->y+3, "Save", WHITE, BLACK); draw_text(w->x+3, w->y+4, "Save As", WHITE, BLACK); draw_text(w->x+3, w->y+5, "Load", WHITE, BLACK); 
    }
    if(np_menu_open == 2) { 
        draw_rect(w->x+8, w->y+3, 10, 3, WHITE, BLACK, ' '); 
        draw_text(w->x+9, w->y+3, "Bold", WHITE, BLACK); draw_text(w->x+9, w->y+4, "Copy", WHITE, BLACK); draw_text(w->x+9, w->y+5, "Paste", WHITE, BLACK); 
    }
}

void render_settings(Window* w) { 
    draw_rect(w->x+1, w->y+2, w->w-2, 1, DARK_GREY, BLACK, ' '); 
    draw_text(w->x+2, w->y+2, "Disp", (w->active_tab==0)?LIGHT_GREY:DARK_GREY, (w->active_tab==0)?BLACK:LIGHT_GREY); 
    draw_text(w->x+8, w->y+2, "Input", (w->active_tab==1)?LIGHT_GREY:DARK_GREY, (w->active_tab==1)?BLACK:LIGHT_GREY); 
    draw_text(w->x+15, w->y+2, "User", (w->active_tab==2)?LIGHT_GREY:DARK_GREY, (w->active_tab==2)?BLACK:LIGHT_GREY); 
    draw_text(w->x+21, w->y+2, "Info", (w->active_tab==3)?LIGHT_GREY:DARK_GREY, (w->active_tab==3)?BLACK:LIGHT_GREY); 
    
    if (w->active_tab==0) { 
        draw_text(w->x+3, w->y+5, "Wallpaper:", LIGHT_GREY, BLACK);
        draw_text(w->x+3, w->y+7, (DESKTOP_COLOR==CYAN)?"(o) Cyan":"( ) Cyan", LIGHT_GREY, BLACK);
        draw_text(w->x+15, w->y+7, (DESKTOP_COLOR==BLUE)?"(o) Blue":"( ) Blue", LIGHT_GREY, BLACK);
        draw_text(w->x+3, w->y+9, (DESKTOP_COLOR==DARK_GREY)?"(o) Grey":"( ) Grey", LIGHT_GREY, BLACK);
        draw_text(w->x+15, w->y+9, (DESKTOP_COLOR==RED)?"(o) Red ":"( ) Red ", LIGHT_GREY, BLACK);
    } 
    else if (w->active_tab==1) { 
        draw_text(w->x+3, w->y+5, "Mouse Speed:", LIGHT_GREY, BLACK);
        draw_text(w->x+3, w->y+7, (MOUSE_SPEED==0)?"(o) Slow":"( ) Slow", LIGHT_GREY, BLACK); 
        draw_text(w->x+3, w->y+8, (MOUSE_SPEED==1)?"(o) Norm":"( ) Norm", LIGHT_GREY, BLACK); 
        draw_text(w->x+3, w->y+9, (MOUSE_SPEED==2)?"(o) Fast":"( ) Fast", LIGHT_GREY, BLACK);
    } 
    else if (w->active_tab==2) { 
        draw_text(w->x+3, w->y+5, "User:", LIGHT_GREY, BLACK); draw_rect(w->x+8, w->y+5, 12, 1, (settings_edit_mode==1)?WHITE:DARK_GREY, (settings_edit_mode==1)?WHITE:DARK_GREY, ' '); draw_text(w->x+8, w->y+5, USERNAME, (settings_edit_mode==1)?WHITE:DARK_GREY, BLACK); 
        draw_text(w->x+3, w->y+7, "Pass:", LIGHT_GREY, BLACK); draw_rect(w->x+8, w->y+7, 12, 1, (settings_edit_mode==2)?WHITE:DARK_GREY, (settings_edit_mode==2)?WHITE:DARK_GREY, ' '); draw_text(w->x+8, w->y+7, PASSWORD, (settings_edit_mode==2)?WHITE:DARK_GREY, BLACK); 
        draw_rect(w->x+8, w->y+9, 6, 1, GREEN, BLACK, ' '); draw_text(w->x+9, w->y+9, " OK ", GREEN, BLACK); 
    } 
    else { 
        draw_text(w->x+3, w->y+5, "Gemini OS Pro", BLUE, LIGHT_GREY); draw_text(w->x+3, w->y+7, "RAM: 4096MB", LIGHT_GREY, BLACK); 
        draw_text(w->x+3, w->y+8, "Ver: 2.0 Stable", LIGHT_GREY, BLACK);
        draw_text(w->x+3, w->y+9, "Uptime:", LIGHT_GREY, BLACK); draw_number(w->x+11, w->y+9, system_ticks/10, LIGHT_GREY, BLACK);
    }
}
void render_explorer(Window* w) { draw_rect(w->x+1, w->y+2, w->w-2, w->h-3, WHITE, WHITE, ' '); int fy=w->y+2; for(int i=0; i<10; i++) if(ram_disk[i].used) { draw_text(w->x+2, fy++, ram_disk[i].name, ram_disk[i].is_image?BLUE:BLACK, ram_disk[i].is_image?WHITE:WHITE); } }
void render_calc(Window* w) { draw_rect(w->x+2, w->y+2, w->w-4, 2, WHITE, BLACK, ' '); draw_number(w->x+3, w->y+3, calc_new_entry?calc_curr:calc_acc, WHITE, BLACK); draw_text(w->x+2, w->y+5, "[7][8][9][+]", LIGHT_GREY, BLACK); draw_text(w->x+2, w->y+7, "[4][5][6][-]", LIGHT_GREY, BLACK); draw_text(w->x+2, w->y+9, "[1][2][3][*]", LIGHT_GREY, BLACK); draw_text(w->x+2, w->y+11,"[C][0][=][/]", LIGHT_GREY, BLACK); }

void draw_window(Window* w) {
    if (!w->visible) return;
    draw_rect(w->x+1, w->y+1, w->w, w->h, BLACK, DARK_GREY, 176); draw_rect(w->x, w->y, w->w, w->h, LIGHT_GREY, BLACK, ' '); draw_rect(w->x, w->y, w->w, 1, BLUE, WHITE, ' ');
    draw_text(w->x+1, w->y, w->title, BLUE, WHITE); draw_text(w->x+w->w-3, w->y, "[X]", RED, WHITE); draw_text(w->x+w->w-1, w->y+w->h-1, "/", LIGHT_GREY, DARK_GREY);
    if (w->id == 1) render_notepad(w); else if (w->id == 2) render_calc(w); else if (w->id == 3) render_settings(w); else if (w->id == 4) render_paint(w); else if (w->id == 5) render_explorer(w); else if (w->id == 6) render_snake_win(w);
}

/* --- 9. INPUT LOGIC --- */
void handle_click(int mx, int my) {
    if(show_save_dialog || show_load_dialog) {
        int dx=25, dy=10;
        if(my==dy+5 && mx>=dx+2 && mx<=dx+8) { 
            if(dialog_mode==1) { strcpy_safe(np_filename, dialog_input_buf, 16); fs_save(np_filename, np_buffer, false); }
            if(dialog_mode==2) { strcpy_safe(paint_filename, dialog_input_buf, 16); fs_save(paint_filename, (char*)paint_canvas, true); }
            if(dialog_mode==3) { fs_load_paint(dialog_input_buf); }
            show_save_dialog=false; show_load_dialog=false;
        }
        if(my==dy+5 && mx>=dx+10 && mx<=dx+18) { show_save_dialog=false; show_load_dialog=false; }
        return;
    }
    if (start_open) {
        if (mx < 20 && my > SCREEN_H - 14) { 
            int item = (my - (SCREEN_H - 14));
            if (item == 3) win_notepad.visible = true; else if (item == 4) win_calc.visible = true; else if (item == 5) win_paint.visible = true;
            else if (item == 6) win_settings.visible = true; else if (item == 7) win_files.visible = true; else if (item == 8) { win_snake.visible=true; reset_snake(); }
            else if (item == 9) sys_reboot(); else if (item == 10) sys_shutdown();
            start_open = false; return;
        } else start_open = false; 
    }
    if (my == SCREEN_H - 1 && mx < 8) { start_open = !start_open; return; }

    for(int i=5; i>=0; i--) { // Reverse order for Z-index
        Window* w = windows[i]; if (!w->visible) continue;
        if (mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h) {
            settings_edit_mode = 0;
            if (mx == w->x + w->w - 1 && my == w->y + w->h - 1) { resize_win = w; return; }
            if (my == w->y) { if (mx >= w->x + w->w - 3) { w->visible = false; return; } drag_win = w; drag_off_x = mx - w->x; drag_off_y = my - w->y; return; }
            
            if(w->id==4) { // Paint
                if(my==w->y+2 && mx<w->x+6) paint_menu = (paint_menu==1)?0:1;
                else if(my==w->y+2 && mx<w->x+12) paint_menu = (paint_menu==2)?0:2;
                else if(my==w->y+2) { int idx=(mx-(w->x+14))/2; if(idx>=0 && idx<9) { uint8_t p[]={0,4,2,1,3,6,14,5,12}; paint_color=p[idx]; } }
                if(paint_menu==1) { 
                    if(my==w->y+3) { memset(paint_canvas, 0xFF, 1200); paint_menu=0; } 
                    if(my==w->y+4) { show_load_dialog=true; dialog_mode=3; strcpy_safe(dialog_input_buf, paint_filename, 16); paint_menu=0; }
                    if(my==w->y+5) { show_save_dialog=true; dialog_mode=2; strcpy_safe(dialog_input_buf, paint_filename, 16); paint_menu=0; }
                }
                if(paint_menu==2) { if(my==w->y+3) { w->w=32; w->h=14; paint_menu=0; } if(my==w->y+4) { w->w=45; w->h=16; paint_menu=0; } if(my==w->y+5) { w->w=20; w->h=10; paint_menu=0; } if(my==w->y+6) { w->w=60; w->h=20; paint_menu=0; } }
                return;
            }
            if(w->id==1) { // Notepad
                if(my==w->y+2 && mx<w->x+6) np_menu_open=(np_menu_open==1)?0:1;
                else if(my==w->y+2 && mx<w->x+12) np_menu_open=(np_menu_open==2)?0:2;
                if(np_menu_open==1) {
                    if(my==w->y+3) { fs_save(np_filename, np_buffer, false); np_menu_open=0; }
                    if(my==w->y+4) { show_save_dialog=true; dialog_mode=1; strcpy_safe(dialog_input_buf, np_filename, 16); np_menu_open=0; }
                    if(my==w->y+5) { win_files.visible=true; np_menu_open=0; }
                }
                if(np_menu_open==2) { if(my==w->y+3) np_bold=!np_bold; np_menu_open=0; }
                return;
            }
            if(w->id==3) { // Settings (FIXED UI)
                if(my==w->y+2) { if(mx<w->x+7) w->active_tab=0; else if(mx<w->x+14) w->active_tab=1; else if(mx<w->x+20) w->active_tab=2; else w->active_tab=3; }
                if(w->active_tab==0) { if(my==w->y+7 && mx<w->x+12) DESKTOP_COLOR=CYAN; if(my==w->y+7 && mx>w->x+12) DESKTOP_COLOR=BLUE; if(my==w->y+9 && mx<w->x+12) DESKTOP_COLOR=DARK_GREY; if(my==w->y+9 && mx>w->x+12) DESKTOP_COLOR=RED; }
                if(w->active_tab==1 && my>=w->y+7 && my<=w->y+9) MOUSE_SPEED = my - (w->y+7);
                if(w->active_tab==2) { if(my==w->y+5) settings_edit_mode=1; if(my==w->y+7) settings_edit_mode=2; if(my==w->y+9) settings_edit_mode=0; }
            }
            if(w->id==5 && my>w->y+1) { 
                int idx=my-(w->y+2); 
                if(idx>=0 && idx<10 && ram_disk[idx].used) {
                    if(ram_disk[idx].is_image) fs_load_paint(ram_disk[idx].name);
                    else fs_load_txt(ram_disk[idx].name);
                }
            }
            if(w->id==2 && my>w->y+4) { // Calc Fixed
                int row = (my - (w->y+5)) / 2; int col = (mx - (w->x+2)) / 4; 
                if(row==0) { if(col==0) calc_curr=calc_curr*10+7; if(col==1) calc_curr=calc_curr*10+8; if(col==2) calc_curr=calc_curr*10+9; if(col==3) { calc_op='+'; calc_acc=calc_curr; calc_curr=0; } }
                if(row==1) { if(col==0) calc_curr=calc_curr*10+4; if(col==1) calc_curr=calc_curr*10+5; if(col==2) calc_curr=calc_curr*10+6; if(col==3) { calc_op='-'; calc_acc=calc_curr; calc_curr=0; } }
                if(row==2) { if(col==0) calc_curr=calc_curr*10+1; if(col==1) calc_curr=calc_curr*10+2; if(col==2) calc_curr=calc_curr*10+3; if(col==3) { calc_op='*'; calc_acc=calc_curr; calc_curr=0; } }
                if(row==3) { 
                    if(col==0) { calc_curr=0; calc_acc=0; } if(col==1) calc_curr=calc_curr*10+0; 
                    if(col==2) { if(calc_op=='+') calc_curr+=calc_acc; if(calc_op=='-') calc_curr=calc_acc-calc_curr; if(calc_op=='*') calc_curr*=calc_acc; if(calc_op=='/') if(calc_curr) calc_curr=calc_acc/calc_curr; calc_new_entry=true; }
                    if(col==3) { calc_op='/'; calc_acc=calc_curr; calc_curr=0; }
                }
            }
            return;
        }
    }
}

void handle_key(char c, uint8_t code) {
    if (current_state == STATE_LOGIN) {
        if (code == 0x1C) { if (streq(login_user, USERNAME) && streq(login_pass, PASSWORD)) current_state = STATE_DESKTOP; else login_user[0]=0; return; }
        if (code == 0x0F) { login_focus_pass = !login_focus_pass; return; }
        char* t = login_focus_pass ? login_pass : login_user; int l = strlen(t); if (c=='\b') { if(l>0) t[l-1]=0; } else if(l<18) { t[l]=c; t[l+1]=0; }
    } else if (current_state == STATE_DESKTOP) {
        if(win_snake.visible && !game_over) { if(code==0x48) snake_dir=0; if(code==0x4D) snake_dir=1; if(code==0x50) snake_dir=2; if(code==0x4B) snake_dir=3; }
        if(win_snake.visible && game_over && (c=='r' || c=='R')) reset_snake();
        if(settings_edit_mode > 0 && c) { char* t = (settings_edit_mode == 1) ? USERNAME : PASSWORD; int l = strlen(t); if (c=='\b') { if(l>0) t[l-1]=0; } else if (l<18) { t[l]=c; t[l+1]=0; } return; }
        if((show_save_dialog||show_load_dialog) && c) { int l = strlen(dialog_input_buf); if(c=='\b') { if(l>0) dialog_input_buf[l-1]=0; } else if(l<15) { dialog_input_buf[l]=c; dialog_input_buf[l+1]=0; } return; }
        if(win_notepad.visible && c && !show_save_dialog) { int l = strlen(np_buffer); if(c=='\b') { if(l>0) np_buffer[l-1]=0; } else if(l<511 && c >= 32) { np_buffer[l]=c; np_buffer[l+1]=0; } else if(c=='\n') { np_buffer[l]='\n'; np_buffer[l+1]=0; } }
    }
}

void update_paint_tool() {
    if (win_paint.visible && mouse_left) {
        int cx = win_paint.x+1, cy = win_paint.y+3, cw = win_paint.w-2, ch = win_paint.h-4;
        if(mouse_x>=cx && mouse_x<cx+cw && mouse_y>=cy && mouse_y<cy+ch) { int lx=mouse_x-cx, ly=mouse_y-cy; if(ly<20 && lx<60) paint_canvas[ly*60+lx]=paint_color; }
    }
}

/* --- 10. MAIN --- */
void render_boot(int frame) {
    draw_rect(0, 0, SCREEN_W, SCREEN_H, BLACK, BLACK, ' ');
    draw_text(39, 10, "\x1E", CYAN, BLACK); draw_text(35, 12, "Gemini OS", WHITE, BLACK);
    char s[] = {'|', '/', '-', '\\'}; draw_text(39, 14, (char[]){s[(frame/5)%4], 0}, DARK_GREY, BLACK);
    buffer_swap();
}

void render() {
    if(show_save_dialog || show_load_dialog) {
        int dx=25, dy=10; draw_rect(dx, dy, 30, 8, LIGHT_GREY, BLACK, ' '); draw_rect(dx, dy, 30, 1, BLUE, WHITE, ' ');
        draw_text(dx+1, dy, show_load_dialog?"Open File...":"Save As...", BLUE, WHITE); draw_text(dx+2, dy+2, "Name:", LIGHT_GREY, BLACK);
        draw_rect(dx+2, dy+3, 20, 1, WHITE, BLACK, ' '); draw_text(dx+2, dy+3, dialog_input_buf, WHITE, BLACK);
        draw_rect(dx+2, dy+5, 6, 1, GREEN, BLACK, ' '); draw_text(dx+3, dy+5, " OK ", GREEN, BLACK);
    }
    else if (current_state == STATE_LOGIN) {
        draw_rect(0, 0, SCREEN_W, SCREEN_H, BLUE, BLUE, 177);
        draw_rect(25, 8, 30, 10, LIGHT_GREY, BLACK, ' '); draw_rect(25, 8, 30, 1, DARK_GREY, WHITE, ' ');
        draw_text(35, 8, "LOGIN", DARK_GREY, WHITE);
        draw_text(27, 11, "User:", LIGHT_GREY, BLACK); 
        draw_rect(33, 11, 20, 1, login_focus_pass?LIGHT_GREY:WHITE, login_focus_pass?LIGHT_GREY:WHITE, ' '); draw_text(33, 11, login_user, login_focus_pass?LIGHT_GREY:WHITE, BLACK);
        draw_text(27, 13, "Pass:", LIGHT_GREY, BLACK); 
        draw_rect(33, 13, 20, 1, login_focus_pass?WHITE:LIGHT_GREY, login_focus_pass?WHITE:LIGHT_GREY, ' '); 
        for(int i=0; i<strlen(login_pass); i++) draw_text(33+i, 13, "*", login_focus_pass?WHITE:LIGHT_GREY, BLACK);
        draw_text(27, 16, "Press [TAB] / [ENTER]", LIGHT_GREY, DARK_GREY);
    } else if (current_state == STATE_DESKTOP) {
        draw_rect(0, 0, SCREEN_W, SCREEN_H, DESKTOP_COLOR, DESKTOP_COLOR, 177);
        draw_rect(0, SCREEN_H-1, SCREEN_W, 1, LIGHT_GREY, BLACK, ' ');
        draw_rect(0, SCREEN_H-1, 8, 1, GREEN, BLACK, ' '); draw_text(1, SCREEN_H-1, " START ", GREEN, BLACK);
        for(int i=0; i<6; i++) draw_window(windows[i]);
        if (start_open) {
            int mx = 0, my = SCREEN_H - 14; 
            draw_rect(mx, my, 20, 13, LIGHT_GREY, BLACK, ' ');
            draw_rect(mx+1, my+1, 18, 1, DARK_GREY, WHITE, ' ');
            draw_text(mx+2, my+1, "Gemini OS", DARK_GREY, WHITE);
            draw_text(mx+2, my+3, "Notepad", LIGHT_GREY, BLACK); draw_text(mx+2, my+4, "Calculator", LIGHT_GREY, BLACK);
            draw_text(mx+2, my+5, "Paint", LIGHT_GREY, BLACK); draw_text(mx+2, my+6, "Settings", LIGHT_GREY, BLACK);
            draw_text(mx+2, my+7, "Explorer", LIGHT_GREY, BLACK); draw_text(mx+2, my+8, "Snake Game", LIGHT_GREY, BLACK);
            draw_text(mx+2, my+9, "Restart", LIGHT_GREY, RED); draw_text(mx+2, my+10, "Shutdown", LIGHT_GREY, RED);
        }
    }
    if (mouse_x >= 0 && mouse_x < SCREEN_W && mouse_y >= 0 && mouse_y < SCREEN_H) back_buffer[mouse_y*SCREEN_W+mouse_x] = vga_entry(0x1E, WHITE);
    buffer_swap();
}

void update_drivers() {
    system_ticks++;
    uint8_t status = inb(0x64);
    if ((status & 0x01) && (status & 0x20)) {
        uint8_t b = inb(0x60); mouse_byte[mouse_cycle++] = b;
        if (mouse_cycle == 3) { mouse_cycle = 0; if ((mouse_byte[0] & 0xC0) == 0) {
            if (MOUSE_SPEED == 0) { mouse_x += mouse_byte[1]/2; mouse_y -= mouse_byte[2]/2; } else { mouse_x += mouse_byte[1]; mouse_y -= mouse_byte[2]; }
            mouse_left = mouse_byte[0] & 1;
            if (mouse_x < 0) mouse_x = 0; if (mouse_x >= SCREEN_W) mouse_x = SCREEN_W - 1;
            if (mouse_y < 0) mouse_y = 0; if (mouse_y >= SCREEN_H) mouse_y = SCREEN_H - 1;
            if (mouse_left && !drag_win && !resize_win) handle_click(mouse_x, mouse_y);
            if (!mouse_left) { drag_win = NULL; resize_win = NULL; }
            if (drag_win) { drag_win->x = mouse_x - drag_off_x; drag_win->y = mouse_y - drag_off_y; }
            if (resize_win) { int nw = mouse_x - resize_win->x + 1; int nh = mouse_y - resize_win->y + 1; if(nw > 10) resize_win->w = nw; if(nh > 5) resize_win->h = nh; }
            update_paint_tool();
        }}
    } else if (status & 0x01) {
        uint8_t scancode = inb(0x60);
        if (scancode == 0x2A || scancode == 0x36) shift_pressed = true;
        else if (scancode == 0xAA || scancode == 0xB6) shift_pressed = false;
        else if (!(scancode & 0x80)) { char c = shift_pressed ? kbd_map_shift[scancode] : kbd_map[scancode]; handle_key(c, scancode); }
    }
}

void kernel_main(void) {
    for(int i=0; i<1200; i++) paint_canvas[i] = 0xFF; 
    for(int i=0; i<10; i++) ram_disk[i].used = false; 
    
    mouse_cycle = 0; uint32_t wait = 10000; while(wait--) asm volatile("nop");
    outb(0x64, 0xA8); outb(0x64, 0x20); uint8_t status = inb(0x60) | 2; outb(0x64, 0x60); outb(0x60, status); outb(0x64, 0xD4); outb(0x60, 0xF4); inb(0x60);
    
    for(int i=0; i<100; i++) { render_boot(i); for(int d=0; d<12000000; d++) asm volatile("nop"); }
    current_state = STATE_LOGIN;
    while(1) { update_drivers(); update_snake(); render(); }
}