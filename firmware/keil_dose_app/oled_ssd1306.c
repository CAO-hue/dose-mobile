/**
 * oled_ssd1306.c - SSD1306 128x64 OLED driver (I2C, software bit-bang)
 * SCL = PB6, SDA = PB7, slave addr 0x3C.
 * Uses a 1024-byte frame buffer; text drawn in 6x8 cells (5x7 glyph + 1 space col).
 * If the display appears upside down, swap segment/COM direction in oled_init()
 *   (0xA0<->0xA1  or  0xC0<->0xC8), or invert the font bit order.
 */
#include "app.h"
#include "oled_ssd1306.h"

#define OLED_ADDR   0x3C
#define OLED_W      128u
#define OLED_H      64u
#define OLED_PAGES  8u

/* ---------- software I2C on PB6(SCL)/PB7(SDA) ---------- */
#define SCL_PIN  GPIO_Pin_6
#define SDA_PIN  GPIO_Pin_7

#define SCL_H()  GPIO_SetBits(GPIOB, SCL_PIN)
#define SCL_L()  GPIO_ResetBits(GPIOB, SCL_PIN)
#define SDA_H()  GPIO_SetBits(GPIOB, SDA_PIN)
#define SDA_L()  GPIO_ResetBits(GPIOB, SDA_PIN)
#define SDA_IN()  GPIO_ReadInputDataBit(GPIOB, SDA_PIN)

static void i2c_delay(void)
{
    volatile uint32_t i;
    for (i = 0; i < 20u; i++) { __NOP(); }
}

static void i2c_init_pins(void)
{
    GPIO_InitTypeDef gpio;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin   = SCL_PIN | SDA_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_OD;   /* open-drain, pull-ups external/onboard */
    GPIO_Init(GPIOB, &gpio);
    SCL_H();
    SDA_H();
}

static void i2c_start(void)
{
    SDA_H(); SCL_H(); i2c_delay();
    SDA_L(); i2c_delay();
    SCL_L(); i2c_delay();
}

static void i2c_stop(void)
{
    SDA_L(); i2c_delay();
    SCL_H(); i2c_delay();
    SDA_H(); i2c_delay();
}

static void i2c_write_byte(uint8_t dat)
{
    uint8_t i;
    for (i = 0; i < 8u; i++)
    {
        if (dat & 0x80u) SDA_H(); else SDA_L();
        dat <<= 1;
        i2c_delay();
        SCL_H(); i2c_delay();
        SCL_L(); i2c_delay();
    }
    SDA_H();            /* release for ACK */
    i2c_delay();
    SCL_H(); i2c_delay();
    SCL_L(); i2c_delay();
}

static void oled_write_byte(uint8_t cmd_data, uint8_t value)
{
    i2c_start();
    i2c_write_byte((uint8_t)(OLED_ADDR << 1));   /* write */
    i2c_write_byte(cmd_data);                    /* 0x00=command, 0x40=data */
    i2c_write_byte(value);
    i2c_stop();
}

static void oled_cmd(uint8_t c)  { oled_write_byte(0x00u, c); }
static void oled_data(uint8_t d) { oled_write_byte(0x40u, d); }

/* ---------- font (5x7 glyphs -> 6 columns in page) ---------- */
static const struct { char c; uint8_t b[5]; } FONT[] = {
    { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { '!', { 0x00, 0x00, 0x5F, 0x00, 0x00 } },
    { '-', { 0x08, 0x08, 0x08, 0x08, 0x08 } },
    { '.', { 0x00, 0x00, 0x60, 0x00, 0x00 } },
    { '/', { 0x10, 0x08, 0x04, 0x02, 0x01 } },
    { '0', { 0x3E, 0x51, 0x49, 0x45, 0x3E } },
    { '1', { 0x00, 0x42, 0x7F, 0x40, 0x00 } },
    { '2', { 0x62, 0x51, 0x49, 0x49, 0x46 } },
    { '3', { 0x22, 0x41, 0x49, 0x49, 0x36 } },
    { '4', { 0x18, 0x14, 0x12, 0x7F, 0x10 } },
    { '5', { 0x27, 0x45, 0x45, 0x45, 0x39 } },
    { '6', { 0x3C, 0x4A, 0x49, 0x49, 0x30 } },
    { '7', { 0x01, 0x71, 0x09, 0x05, 0x03 } },
    { '8', { 0x36, 0x49, 0x49, 0x49, 0x36 } },
    { '9', { 0x06, 0x49, 0x49, 0x29, 0x1E } },
    { ':', { 0x00, 0x00, 0x22, 0x00, 0x00 } },
    { 'A', { 0x7E, 0x09, 0x09, 0x09, 0x7E } },
    { 'C', { 0x3E, 0x41, 0x41, 0x41, 0x22 } },
    { 'E', { 0x7F, 0x49, 0x49, 0x49, 0x41 } },
    { 'H', { 0x7F, 0x08, 0x08, 0x08, 0x7F } },
    { 'I', { 0x00, 0x41, 0x7F, 0x41, 0x00 } },
    { 'K', { 0x7F, 0x08, 0x14, 0x22, 0x41 } },
    { 'L', { 0x7F, 0x40, 0x40, 0x40, 0x40 } },
    { 'M', { 0x7F, 0x02, 0x0C, 0x02, 0x7F } },
    { 'N', { 0x7F, 0x02, 0x04, 0x08, 0x7F } },
    { 'O', { 0x3E, 0x41, 0x41, 0x41, 0x3E } },
    { 'P', { 0x7F, 0x09, 0x09, 0x09, 0x06 } },
    { 'R', { 0x7F, 0x09, 0x19, 0x29, 0x46 } },
    { 'S', { 0x46, 0x49, 0x49, 0x49, 0x31 } },
    { 'T', { 0x01, 0x01, 0x7F, 0x01, 0x01 } },
    { 'h', { 0x7F, 0x08, 0x08, 0x08, 0x70 } },
    { 'u', { 0x7C, 0x40, 0x40, 0x40, 0x7C } },
    { 'v', { 0x0C, 0x10, 0x20, 0x10, 0x0C } },
};
static uint8_t framebuf[OLED_W * OLED_PAGES];

/* ---------- public API ---------- */

void oled_init(void)
{
    i2c_init_pins();
    i2c_delay();

    oled_cmd(0xAE);            /* display off */
    oled_cmd(0xD5); oled_cmd(0x80);
    oled_cmd(0xA8); oled_cmd(0x3F);
    oled_cmd(0xD3); oled_cmd(0x00);
    oled_cmd(0x40);            /* start line 0 */
    oled_cmd(0x8D); oled_cmd(0x14);   /* charge pump on */
    oled_cmd(0x20); oled_cmd(0x00);   /* horizontal addressing */
    oled_cmd(0xA1);            /* segment remap (mirror if upside down) */
    oled_cmd(0xC8);            /* COM scan direction (mirror if upside down) */
    oled_cmd(0xDA); oled_cmd(0x12);
    oled_cmd(0x81); oled_cmd(0xCF);
    oled_cmd(0xD9); oled_cmd(0xF1);
    oled_cmd(0xDB); oled_cmd(0x40);
    oled_cmd(0xA4);
    oled_cmd(0xA6);            /* normal display */
    oled_cmd(0xAF);            /* display on */

    oled_clear_buf();
    oled_update();
}

void oled_clear_buf(void)
{
    uint16_t i;
    for (i = 0; i < sizeof(framebuf); i++) framebuf[i] = 0x00u;
}

void oled_text(uint8_t page, uint8_t x, const char *s)
{
    while (*s)
    {
        const uint8_t *g = 0;
        uint8_t i;
        for (i = 0; i < (sizeof(FONT) / sizeof(FONT[0])); i++)
        {
            if (FONT[i].c == *s) { g = FONT[i].b; break; }
        }
        if (g)
        {
            uint8_t c;
            for (c = 0; c < 5u; c++)
            {
                if ((page < OLED_PAGES) && ((uint16_t)x + c < OLED_W))
                {
                    framebuf[(uint16_t)page * OLED_W + x + c] = g[c];
                }
            }
        }
        x += 6u;   /* advance one 6x8 cell */
        s++;
    }
}

void oled_update(void)
{
    uint16_t i;

    oled_cmd(0x21); oled_cmd(0x00); oled_cmd(0x7F);   /* col 0..127 */
    oled_cmd(0x22); oled_cmd(0x00); oled_cmd(0x07);   /* page 0..7  */

    i2c_start();
    i2c_write_byte((uint8_t)(OLED_ADDR << 1));
    i2c_write_byte(0x40u);          /* data mode */
    for (i = 0; i < sizeof(framebuf); i++)
    {
        i2c_write_byte(framebuf[i]);
    }
    i2c_stop();
}
