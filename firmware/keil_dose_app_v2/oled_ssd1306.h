#ifndef __OLED_SSD1306_H
#define __OLED_SSD1306_H

/* SSD1306 128x64, I2C software (SCL=PA1, SDA=PA2, addr 0x3C) */
void oled_init(void);
void oled_clear_buf(void);
void oled_text(uint8_t page, uint8_t x, const char *s);  /* 6x8 cell text */
void oled_update(void);

#endif /* __OLED_SSD1306_H */
