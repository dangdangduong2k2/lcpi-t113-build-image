# Khắc phục lỗi build, flash và boot

| Hiện tượng | Nguyên nhân thường gặp | Cách xử lý an toàn |
|---|---|---|
| `Invalid distribution name` khi cài WSL | tên distro không tồn tại trên máy | chạy `wsl --list --online`, copy đúng tên ở cột `NAME` |
| `make: command not found` | chưa cài tool host | vào repository rồi chạy `bash scripts/setup-host.sh` |
| `Your PATH contains spaces...` | chạy Makefile nội bộ Buildroot hoặc PATH Windows lọt vào | quay về gốc repository và chạy `make build`; Makefile này tự làm sạch PATH |
| `No space left on device` | disk WSL không còn đủ chỗ | kiểm tra `df -h ~`; giải phóng dung lượng an toàn rồi chạy lại `make build` |
| Download bị ngắt | mạng/proxy | chạy lại `make build`; tarball được cache và Makefile kiểm SHA-256 |
| Build lại nhưng UI không đổi | chỉ chạy `make build` sau khi sửa local package | chạy `make rebuild-lvgl-test` |
| Đổi font/config LVGL nhưng không đổi | liblvgl cũ còn được dùng | chạy `make rebuild-lvgl` |
| Windows đòi format thẻ sau flash | Windows không đọc ext4 | chọn **Cancel**, không format |
| PhoenixCard không nhận `sdcard.img` | format image sai công cụ | dùng Raspberry Pi Imager/Etcher; PhoenixCard chỉ dành image Tina `IMAGEWTY` |
| Backlight sáng nhưng màn hình đen | backlight không phải boot witness | ghi lại có thấy chuỗi màu không; kiểm tra FPC/image rồi lấy UART log nếu cần |

## Lấy log build dễ đọc

Trong thư mục repository:

```bash
make build 2>&1 | tee ~/lcpi-t113-build.log
```

Khi có lỗi, gửi **dòng lỗi đầu tiên** cùng khoảng 30 dòng trước/sau nó. Đừng chỉ
gửi dòng `make: *** Error 2`, vì đó chỉ là hậu quả cuối cùng.

## Không xóa lung tung

Không chạy `rm -rf` theo hướng dẫn ngẫu nhiên. Hãy thử lại `make build` trước.
Nếu cache/config thực sự cần tạo lại, dừng lại và xác định chính xác thư mục
trước khi xóa. Những thư mục giá trị cần giữ là source repository và
`~/lcpi-t113-work/downloads/` (cache tải có kiểm hash).

## Kiểm tra image trước flash

```bash
IMAGE="$HOME/lcpi-t113-work/output/images/sdcard.img"
test -f "$IMAGE" || { echo "Khong tim thay image"; exit 1; }
sha256sum "$IMAGE"
```

Nếu command trên không in hash, đừng flash thẻ. Chạy `make print-paths` để xem
Makefile đang dùng output directory nào.
