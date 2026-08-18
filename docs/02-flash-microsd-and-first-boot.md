# Ghi microSD và boot lần đầu

## 1. Chọn đúng file

Chỉ dùng file Buildroot vừa tạo:

```text
~/lcpi-t113-work/output/images/sdcard.img
```

Không dùng PhoenixCard cho file này. Image Tina của hãng cũng có đuôi `.img`
nhưng là format `IMAGEWTY`; nó dành cho PhoenixCard và khác hoàn toàn raw image
Buildroot.

## 2. Ghi thẻ trên Windows bằng Raspberry Pi Imager hoặc balenaEtcher

Với người mới, dùng giao diện đồ họa an toàn hơn lệnh raw-write trên PowerShell.
Cài một trong hai công cụ:

- [Raspberry Pi Imager](https://www.raspberrypi.com/software/)
- [balenaEtcher](https://etcher.balena.io/)

Ví dụ với Raspberry Pi Imager:

1. Cắm microSD qua đầu đọc thẻ.
2. Mở Raspberry Pi Imager.
3. Chọn **Choose OS** → **Use custom**.
4. Chọn `sdcard.img` vừa build.
5. Chọn đúng thẻ microSD theo dung lượng và tên thiết bị. Với thẻ 8 GB, kiểm
   tra kỹ để không chọn nhầm ổ USB/SSD khác.
6. Nhấn **Write**, xác nhận cảnh báo xóa dữ liệu và chờ verify hoàn tất.
7. Eject/rút thẻ an toàn.

Không format thẻ trước khi ghi. Sau khi ghi, Windows có thể hiện hộp thoại đòi
format một partition Linux ext4; chọn **Cancel**. Format sau khi flash sẽ phá
image vừa ghi.

## 3. Boot board không cần UART

1. Tắt nguồn board.
2. Cắm microSD vào board.
3. Cắm đúng LCD EP4303B và FPC GT911.
4. Cấp nguồn.
5. Chờ tối đa khoảng một phút cho lần boot đầu.

Khi framebuffer xuất hiện, script boot chạy một chuỗi màu toàn màn hình ngắn
trước khi mở launcher LVGL. Sau đó bạn sẽ thấy giao diện home, vuốt sang trang
Display và Touch để kiểm tra màn hình/cảm ứng.

**Chỉ backlight sáng không chứng minh Linux đã boot.** Backlight của board có
thể sáng ngay cả khi pixel clock hoặc app chưa hoạt động. Hãy ghi lại xem có
thấy chuỗi màu hay không:

| Quan sát | Ý nghĩa ban đầu |
|---|---|
| Có chuỗi màu, UI không hiện | pipeline LCD/framebuffer lên; khoanh vùng app LVGL |
| Backlight nhưng không có màu | kiểm tra image, FPC, DTS/panel hoặc boot log |
| Không backlight và không màu | kiểm tra nguồn, panel, FPC và đúng revision board |

## 4. UART chỉ dùng khi cần cứu hộ

Image tự chạy test, nên bạn không cần UART cho workflow bình thường. Khi cần
log, dùng USB-UART logic **3.3 V**:

```text
P2-16 / PB6 / UART3 TX  -> RX của USB-UART
P2-15 / PB7 / UART3 RX  <- TX của USB-UART
GND                     -> GND
```

- Không nối chân VCC từ USB-UART vào board.
- Mở terminal `115200 8N1`, no parity, no flow control.
- Dùng đúng UART3 ở PB6/PB7 cho Buildroot; image Tina của hãng dùng UART0 khác.

Nếu màn hình không lên, lưu toàn bộ UART log từ lúc cấp nguồn thay vì chỉ chụp
một ảnh cuối log. Xem thêm [bringup-checklist.md](bringup-checklist.md).
