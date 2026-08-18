# LCPI T113 Build Image

Đây là repository **thực sự dùng để build image microSD** cho board LCPI-PC-T113:

- SoC T113-S3;
- LCD EP4303B-V1, RGB666, 480×272 @ khoảng 58 Hz;
- cảm ứng GT911;
- Buildroot `2026.05.1`, Linux `6.18.8`, U-Boot `2026.01`.

Repository tạo một raw image Linux cho microSD. Nó không phải Tina image của
hãng và không dùng PhoenixCard.

> Nếu mục tiêu của bạn là thiết kế giao diện trên Windows trước, xem repository
> [lcpi-lvgl-ui-lab](https://github.com/dangdangduong2k2/lcpi-lvgl-ui-lab).
> Simulator đó không tự build image; repository này mới là nơi build firmware.

## Bắt đầu tại đây

Đọc theo thứ tự này:

1. [Build image từ máy Windows mới hoàn toàn](docs/01-build-image-from-zero.md)
2. [Ghi microSD và boot lần đầu](docs/02-flash-microsd-and-first-boot.md)
3. [Sửa source rồi build lại image](docs/03-change-and-rebuild.md)
4. [Khắc phục lỗi build, flash và boot](docs/04-build-and-flash-troubleshooting.md)

Tài liệu kỹ thuật tham khảo:

- [Hardware notes](docs/hardware-notes.md)
- [Bring-up checklist](docs/bringup-checklist.md)
- [Nguồn tham chiếu](docs/sources.md)

## Lệnh build ngắn gọn

Sau khi đã cài WSL/Ubuntu theo tài liệu số 1, trong Ubuntu chạy:

```bash
mkdir -p ~/src
git clone https://github.com/dangdangduong2k2/lcpi-t113-build-image.git ~/src/lcpi-t113-build-image
cd ~/src/lcpi-t113-build-image

bash scripts/setup-host.sh     # chỉ lần đầu
make configure                 # chỉ lần đầu hoặc khi chủ động reset config
make build
make print-paths
```

Image cần flash:

```text
~/lcpi-t113-work/output/images/sdcard.img
```

Luôn flash file trong đường dẫn trên hoặc bản sao vừa được copy từ đó. Đừng tự
chọn `images/sdcard.img` cũ trong một thư mục Windows: nó có thể không phải
artifact của lần build mới nhất.

## Các lệnh thường dùng

| Khi nào | Lệnh |
|---|---|
| Build incremental bình thường | `make build` |
| Sửa app LVGL chạy trên board | `make rebuild-lvgl-test` |
| Sửa `package/lvgl/lv_conf.h` | `make rebuild-lvgl` |
| Sửa DTS/kernel | `make rebuild-linux` |
| Sửa U-Boot/UART | `make rebuild-uboot` |
| Sửa qua menuconfig rồi muốn lưu vào Git | `make update-defconfig` |
| Xem source/output/image path | `make print-paths` |

`make configure` nạp lại `configs/lcpi_t113_defconfig` và có thể bỏ các thay
đổi menuconfig chưa được lưu. Vì vậy không chạy nó mỗi lần sửa UI.

## Cấu trúc source quan trọng

```text
package/lcpi-lvgl-test/src/main.c      app LVGL thực chạy trên board
package/lvgl/lv_conf.h                 cấu hình LVGL target
package/lcpi-lvgl-test/S99...          tự chạy test màu rồi app khi boot
board/lctech/lcpi-t113/linux-dts/...   DTS panel/touch/pinmux
board/lctech/lcpi-t113/linux.fragment  kernel config
board/lctech/lcpi-t113/genimage.cfg    layout raw microSD image
configs/lcpi_t113_defconfig            Buildroot configuration
```

Không sửa trực tiếp bất kỳ file nào trong `~/lcpi-t113-work/output/` hoặc
`~/lcpi-t113-work/buildroot-2026.05.1`; đó là output/cache được tạo lại.

## Phạm vi phần cứng

Cấu hình này dành cho đúng tổ hợp T113-S3 + EP4303B-V1 + GT911. Backlight sáng
không tự chứng minh hệ thống boot thành công, vì hardware có thể bật backlight
độc lập. Image chạy chuỗi màu framebuffer trước launcher LVGL để hỗ trợ chẩn
đoán không cần UART.
