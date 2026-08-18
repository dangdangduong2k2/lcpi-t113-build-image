# Checklist bring-up

## A. Chốt baseline của hãng

1. Chụp hai mặt bo và panel trước khi cắm nguồn.
2. Ghi image Tina T113 bằng PhoenixCard ở chế độ **Boot Card / 启动卡**. Thao tác này xóa thẻ.
3. Image Tina hãng dùng UART0: nối PE2/TX của bo → RX adapter, GND → GND để lấy baseline hãng.
4. Boot và lưu toàn bộ log từ reset tới login.
5. Xác nhận LCD/backlight của bộ hãng có hiển thị.

Baseline này phân biệt lỗi phần cứng/panel với lỗi port mainline.

## B. Build image riêng

Trong WSL Ubuntu:

```bash
cd /mnt/d/5.ct/build_image
bash scripts/setup-host.sh
make configure
make build
make print-paths
```

Giữ nguyên log cuối của lệnh build nếu có lỗi. Không chạy build bằng PowerShell/MSYS; Buildroot yêu cầu host Linux.

## C. Ghi và boot Buildroot

1. Ghi `sdcard.img` raw lên microSD.
2. Tháo/cắm lại thẻ để writer flush hoàn toàn.
3. Cắm thẻ và cấp nguồn. Không cần UART: LVGL tự khởi động menu/test trên LCD.
4. Chỉ khi LCD không lên, nối UART3 Buildroot: P2-16/PB6/TX → adapter RX, P2-15/PB7/RX ← adapter TX, GND chung; đặt adapter ở 3,3 V và mở terminal `115200 8N1` trước khi cấp nguồn.
5. Nếu dùng UART để chẩn đoán, lưu log thành file, không chỉ chụp ảnh màn hình terminal.

Các mốc mong đợi:

```text
U-Boot SPL ...
U-Boot 2026.01 ...
Loading ... zImage
Starting kernel ...
Linux version 6.18.8 ...
lcpi-t113 login:
```

## D. Kiểm tra LCD/touch tự động

Khi boot bình thường, màn hình phải hiện Home menu với icon. Vuốt ngang qua trang Display để xem RGB/gray/motion bar; vuốt tiếp sang Touch & Performance và chạm đủ năm dấu `+` để nhận `TOUCH PASS`. Nút benchmark chạy test FPS/CPU/render/flush của LVGL trực tiếp trên LCD.

Góc trên phải hiển thị `DRM PAGE-FLIP` khi DRM/KMS đã chạy double-buffer. Nếu hiện `FBDEV FALLBACK`, màn hình vẫn dùng được nhưng độ mượt/tear-free thấp hơn.

Nếu cần log chi tiết qua UART:

```sh
dmesg | grep -Ei 'sun4i|drm|panel|tcon|framebuffer'
cat /proc/cmdline
ls -l /dev/fb0 /sys/class/drm
fbset
fb-test
```

Ghi lại:

- backlight có sáng không;
- màn hình đen/trắng hay có hình;
- màu đúng hay đảo;
- hình có lệch, rung, sọc hoặc nhấp nháy;
- output của các lệnh trên.

## E. Khoanh vùng lỗi

| Hiện tượng | Kiểm tra trước |
|---|---|
| Không có log nào | nguồn, GND, P2-16/PB6→adapter RX, baud, image ghi đúng |
| Có SPL rồi mất log | U-Boot `CONS_INDEX`, `stdout-path`, pinmux UART3 |
| Có U-Boot nhưng không vào Linux | extlinux path, DTB, zImage, partition rootfs |
| Linux boot nhưng không có login | `console=ttyS3`, getty `ttyS3` |
| Không có `/dev/fb0` | graph panel↔TCON, `dmesg` DRM, kernel fragment |
| Backlight sáng nhưng màn hình đen | PD18 clock, PD19 DE, HSYNC/VSYNC, panel power |
| Có hình nhưng lệch/rung | porch, sync polarity, pixel-clock edge |
| Màu sai | mapping RGB666, cable orientation, panel bit order |

Không đổi nhiều timing/polarity cùng lúc. Mỗi lần chỉ đổi một biến và lưu lại log/kết quả.
