# Sửa source rồi build lại image

Luôn sửa source trong repository. Không sửa file sinh trong
`~/lcpi-t113-work/output/` hoặc source upstream ở
`~/lcpi-t113-work/buildroot-2026.05.1`; các file đó có thể bị Buildroot ghi đè.

## Bảng đúng file và đúng lệnh rebuild

| Bạn muốn đổi | Sửa file/thư mục | Chạy sau đó |
|---|---|---|
| Launcher LVGL, test display/touch, logic target | `package/lcpi-lvgl-test/src/main.c` | `make rebuild-lvgl-test` |
| Autostart, chuỗi test màu, governor | `package/lcpi-lvgl-test/S99lcpi-lvgl-test`, `S98lcpi-performance` | `make rebuild-lvgl-test` |
| Font, widget/driver LVGL | `package/lvgl/lv_conf.h` | `make rebuild-lvgl` |
| LCD timing, GT911, pinmux, DTS | `board/lctech/lcpi-t113/linux-dts/...dts` | `make rebuild-linux` |
| Kernel config fragment | `board/lctech/lcpi-t113/linux.fragment` | `make rebuild-linux` |
| UART/U-Boot | `board/lctech/lcpi-t113/uboot/` | `make rebuild-uboot` |
| Layout partition/image | `board/lctech/lcpi-t113/genimage.cfg` | `make build` |
| Cấu hình Buildroot | `configs/lcpi_t113_defconfig` | `make configure && make build` |

`make rebuild-lvgl-test` là bắt buộc sau khi sửa target app. Package dùng source
local, vì vậy `make build` thông thường có thể không nhận biết file C vừa đổi.
Target helper này đồng bộ source local, build lại app và tái tạo `sdcard.img`.

## Quy trình sửa UI trên board

Ví dụ đổi text/launcher thật chạy trên T113:

```bash
cd ~/src/lcpi-t113-build-image
nano package/lcpi-lvgl-test/src/main.c
make rebuild-lvgl-test

IMAGE="$HOME/lcpi-t113-work/output/images/sdcard.img"
sha256sum "$IMAGE"
```

Sau đó flash lại đúng `sdcard.img`; không cần chạy `make configure` cho một thay
đổi UI thông thường.

## Simulator UI Windows là project riêng

Repository [lcpi-lvgl-ui-lab](https://github.com/dangdangduong2k2/lcpi-lvgl-ui-lab)
giúp thử layout bằng SDL trên PC. Nó **không** tự build image và `app/ui.c` chưa
tự động được đưa vào target.

Đừng copy các file này sang image T113:

```text
app/main_simulator.c
CMakeLists.txt
lv_conf.h                # bản dành cho SDL/Windows
build/                   # output CMake
```

Target hiện chạy từ `package/lcpi-lvgl-test/src/main.c`. Simulator tạo mouse
trước khi tạo UI, còn target dò GT911 bất đồng bộ sau khi UI đã chạy. Copy thẳng
`app/ui.c` sẽ làm touch pointer bị rỗng. Trước mắt, sửa `main.c` target là cách
an toàn. Khi muốn dùng chung UI, hãy refactor có chủ đích thành `ui.c/ui.h` và
thêm API cập nhật pointer sau khi GT911 attach.

## Khi dùng menuconfig

Chỉ mở menuconfig khi bạn biết mình muốn đổi cấu hình Buildroot:

```bash
make menuconfig
make update-defconfig
git diff -- configs/lcpi_t113_defconfig
```

`make configure` nạp lại defconfig của repository và bỏ các chỉnh sửa
menuconfig chưa được lưu bằng `make update-defconfig`.
