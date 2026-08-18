# Ghi chú phần cứng LCPI-PC-T113

## Mức độ chắc chắn

| Hạng mục | Giá trị hiện dùng | Trạng thái |
|---|---|---|
| SoC | T113-S3, dual Cortex-A7, 128 MiB DDR3 SiP | Cần ảnh marking của bo người dùng |
| microSD | MMC0, PF0…PF5, card detect PF6 active-low, 4-bit/3,3 V | Khớp schematic và DTB hãng |
| UART hãng | UART0, TX=PE2, RX=PE3, bank PE 2,8 V | Khớp manual và DTB hãng |
| UART Buildroot | UART3, TX=PB6, RX=PB7, 115200, bank PB 3,3 V | Chân do người dùng xác nhận đang trống |
| Nguồn bank PE | AVDD2V8 qua R47=0 Ω; R49 sang 3,3 V là NC | Đọc trực tiếp schematic |
| LCD data | RGB666, PD0…PD21 | Khớp schematic và DTB hãng |
| Panel | EP4303B-V1/ST7282, 480×272 RGB-TTL, RGB666 wiring | Theo datasheet panel; cần xác nhận bằng boot thật |
| Backlight | PT4103, `LCD_PWM`; R51 từ PD22 được ghi NC | Cần ảnh/revision thật |
| Touch | GT911 qua P4: PB2 reset, PB3 IRQ, I2C2 PE12/PE13 | DTS/driver mainline đã bật; cần xác nhận boot thật |
| Wi-Fi | họ RTL8189F/TL8189FQB2 | Để vòng sau |

## PCB/revision

Drive của hãng dùng tên `LCPI-T113_D1s_F133(303F133D1S3).pdf`, nhưng title block bên trong lại ghi `LctechPi-PC-F133-V1.3 (303F133D1S4)`. `S3/S4` trong hai mã này là mã PCB, không phải hậu tố `T113-S3`. Vì vậy phải xác nhận riêng marking SoC và silkscreen PCB.

PCB dùng chung footprint eLQFP128 cho T113, F133 hoặc D1s. Không dùng image/config F133 hay D1s làm mặc định cho bản T113-S3.

## UART console Buildroot

Project chuyển console riêng sang UART3 để dùng trực tiếp USB-UART 3,3 V:

- P2-16/PB6: UART3 TX → RX của adapter;
- P2-15/PB7: UART3 RX ← TX của adapter;
- GND chung, không nối VCC của adapter;
- SPL/U-Boot dùng `CONFIG_CONS_INDEX=4`; Linux dùng `serial3`/`ttyS3`.

PB6/PB7 nối thẳng từ SoC ra P2, không có tải khác. Chúng có alternate function LCD nhưng không xung đột với connector RGB của bo, vì LCD hiện dùng nhóm PD0…PD21. Image Tina hãng vẫn xuất UART0 trên PE2/PE3; chỉ image Buildroot của project dùng UART3.

## LCD RGB

J3 là connector RGB 40 pin. Phần dữ liệu là RGB666:

- blue: LCD_D2…D7;
- green: LCD_D10…D15;
- red: LCD_D18…D23;
- PD18: pixel clock;
- PD19: DE;
- PD20: HSYNC;
- PD21: VSYNC.

Hai bit thấp của mỗi màu được nối GND trên schematic. DTS dùng pin group upstream `lcd_rgb666_pins`, drive strength 30 mA và no-pull, đúng với DTB Tina.

## Timing EP4303B

Datasheet EP4303B/ST7282 dùng timing điển hình:

```text
pixel clock = 9 MHz
active      = 480 x 272
hsync/hback/hfront = 4 / 39 / 8
vsync/vback/vfront = 4 / 8 / 8
total       = 531 x 292
refresh     = 9,000,000 / (531 x 292) = 58.05 Hz
```

HSYNC/VSYNC active-low, DE active-high và `pixelclk-active=1`. Datasheet vẽ panel lấy mẫu ở cạnh xuống DCLK; cấu hình mainline dùng cạnh lên để TCON drive dữ liệu trước cạnh lấy mẫu đó.

## Backlight

PT4103 tạo nguồn LED backlight. Chân EN nối net `LCD_PWM`, được R25 5,1 kΩ kéo lên 3,3 V. PD22/PWM7 chỉ nối tới net này qua R51, trong schematic ghi `NC`.

Baseline không khai báo `gpio-backlight`:

- nếu R51 không lắp, R25 giữ EN high và backlight sáng full;
- nếu R51 có lắp, backlight vẫn mặc định high nhưng có thể điều khiển on/off bằng PD22 active-high;
- brightness PWM được hoãn vì Linux 6.18.8 chưa có driver PWM D1/T113 hoàn chỉnh trong mainline.

Không hàn jumper hoặc thay R51 trước khi soi đúng revision bo và đo continuity PD22 ↔ `LCD_PWM`.

## Những phần cố ý chưa bật

- Wi-Fi/MMC1;
- TPADC điện trở tại J3;
- audio, camera, SPI flash;
- brightness PWM;
- GPU/UI framework.

Vòng đầu chỉ cần SD, UART, USB cơ bản và LCD. Giảm số thiết bị giúp log lỗi rõ hơn.

## Baseline vendor

Image tham chiếu:

```text
tina_lcpi-t113_uart0-2023-05-01.img
size   22,590,464 bytes
SHA256 a77a25ff94f9180048cf13a9c36784fb5f0bf47be9c0413d6b6584d3f72a5d9d
```

Image dùng Linux 5.4.61 và U-Boot 2018.05 trong hệ Tina. Nó được dùng để lấy pinmux/timing, không được nhúng vào image Buildroot.
