/*
 * ST7789  76×284  Demo  —  ESP32-C6
 *
 * SCL=20  SDA=19  RST=18  DC=15  CS=14  BL=9(PROG)
 *
 * ST7789 GRAM is 240×320.  This 76×284 panel sits at an offset.
 * Adjust XOFF/YOFF if the image is blank or partially visible.
 * Start with (82,0) — if wrong try (0,0) or (0,18).
 */

#include "driver/gpio.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// ── Pins ──────────────────────────────────────────────────────────────────
#define LCD_SCK  20
#define LCD_SDA  19
#define LCD_RST  18
#define LCD_DC   15
#define LCD_CS   14
#define LCD_BL    9   // strapping pin — driven via raw GPIO after boot

// ── Panel geometry inside ST7789 GRAM ─────────────────────────────────────
#define XOFF   82   // ← adjust if display is blank
#define YOFF    0
#define D_W    76
#define D_H   284

// ── RGB565 colours ────────────────────────────────────────────────────────
#define BLACK    0x0000
#define WHITE    0xFFFF
#define RED      0xF800
#define GREEN    0x07E0
#define BLUE     0x001F
#define YELLOW   0xFFE0
#define CYAN     0x07FF
#define MAGENTA  0xF81F
#define ORANGE   0xFD20

Adafruit_ST7789 tft(LCD_CS, LCD_DC, LCD_SDA, LCD_SCK, LCD_RST);

// ── Window helpers (local coords 0,0 = top-left of visible panel) ─────────
void wFill(uint16_t c)                        { tft.fillRect(XOFF,YOFF,D_W,D_H,c); }
void wRect(int x,int y,int w,int h,uint16_t c){ tft.fillRect(XOFF+x,YOFF+y,w,h,c); }
void wLine(int x0,int y0,int x1,int y1,uint16_t c){ tft.drawLine(XOFF+x0,YOFF+y0,XOFF+x1,YOFF+y1,c); }
void wCircle(int x,int y,int r,uint16_t c)    { tft.fillCircle(XOFF+x,YOFF+y,r,c); }
void wText(int x,int y,const char*s,uint16_t c,uint8_t sz=1){
    tft.setTextColor(c); tft.setTextSize(sz);
    tft.setCursor(XOFF+x,YOFF+y); tft.print(s);
}
void wHLine(int y,uint16_t c){ tft.drawFastHLine(XOFF,YOFF+y,D_W,c); }

// ── Scene 1 — Info card ───────────────────────────────────────────────────
void sceneInfo() {
    wFill(BLACK);
    wRect(0,  0, D_W, 20, BLUE);
    wText(6,  5, "ST7789", WHITE, 1);
    wText(4, 26, "76 x 284", CYAN,   1);
    wText(4, 40, "ESP32-C6", GREEN,  1);
    wText(4, 54, "SPI 40MHz", YELLOW, 1);
    wLine(0,68,D_W-1,68,WHITE);
    wCircle(12, 88, 10, RED);
    wCircle(38, 88, 10, GREEN);
    wCircle(63, 88, 10, BLUE);
    // bar chart
    int bx[] = {4,18,32,46,60};
    int bh[] = {80,60,100,45,70};
    uint16_t bc[] = {RED,GREEN,BLUE,YELLOW,CYAN};
    for(int i=0;i<5;i++) wRect(bx[i],200-bh[i],10,bh[i],bc[i]);
    wText(4,204,"BARS",WHITE,1);
    wRect(0,D_H-18,D_W,18,MAGENTA);
    wText(8,D_H-13,"DEMO v1.0",BLACK,1);
}

// ── Scene 2 — Rainbow stripes ─────────────────────────────────────────────
void sceneRainbow() {
    uint16_t cols[]={RED,ORANGE,YELLOW,GREEN,CYAN,BLUE,MAGENTA};
    int bh = D_H/7;
    for(int i=0;i<7;i++) wRect(0,i*bh,D_W,bh,cols[i]);
    wText(4,D_H/2-4,"RAINBOW",BLACK,1);
}

// ── Scene 3 — Vertical gradient ───────────────────────────────────────────
void sceneGradient() {
    for(int y=0;y<D_H;y++){
        uint8_t t=(uint8_t)((y*255L)/(D_H-1));
        uint16_t r5,g6,b5;
        if(t<128){ r5=(127-t)>>2; g6=t>>1;       b5=0; }
        else     { r5=0;           g6=(255-t)>>1;  b5=(t-128)>>2; }
        wHLine(y,(r5<<11)|(g6<<5)|b5);
    }
    wText(6,D_H/2-4,"GRADIENT",WHITE,1);
}

// ── Scene 4 — Bouncing ball ───────────────────────────────────────────────
void sceneBounce() {
    wFill(BLACK);
    int x=D_W/2, y=14, dx=2, dy=3, r=10;
    uint16_t cols[]={RED,GREEN,BLUE,YELLOW,CYAN,MAGENTA};
    int ci=0;
    for(int f=0;f<150;f++){
        wCircle(x,y,r,BLACK);
        x+=dx; y+=dy;
        if(x-r<=0||x+r>=D_W){dx=-dx;ci=(ci+1)%6;}
        if(y-r<=0||y+r>=D_H){dy=-dy;ci=(ci+1)%6;}
        wCircle(x,y,r,cols[ci]);
        delay(18);
    }
}

// ── Backlight on GPIO9 (strapping pin — must use raw driver) ───────────────
static void blOn(){
    gpio_reset_pin(GPIO_NUM_9);
    gpio_set_direction(GPIO_NUM_9,GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(GPIO_NUM_9,GPIO_FLOATING);
    gpio_set_drive_capability(GPIO_NUM_9,GPIO_DRIVE_CAP_3);
    gpio_set_level(GPIO_NUM_9,0);
}

// ─────────────────────────────────────────────────────────────────────────
void setup(){
    Serial.begin(115200);
    delay(300);
    Serial.printf("ST7789 76x284 — XOFF=%d YOFF=%d\n",XOFF,YOFF);

    blOn();                         // backlight on (safe after boot)

    tft.init(240,320);              // full GRAM; we address window via offset
    tft.setRotation(0);
    tft.invertDisplay(true);        // most ST7789 panels need invert
    tft.fillScreen(BLACK);
    delay(100);
    Serial.println("Display ready.");
}

void loop(){
    Serial.println("Scene: Info card");    sceneInfo();    delay(4000);
    Serial.println("Scene: Rainbow");      sceneRainbow(); delay(3000);
    Serial.println("Scene: Gradient");     sceneGradient();delay(3000);
    Serial.println("Scene: Bounce");       sceneBounce();
}
