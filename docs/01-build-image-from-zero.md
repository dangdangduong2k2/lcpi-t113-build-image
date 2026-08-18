# Build image T113 từ máy Windows mới hoàn toàn

Tài liệu này tạo image cho đúng cấu hình đã có trong repository:

- board **LCPI-PC-T113**, SoC T113-S3;
- LCD **EP4303B-V1**, RGB666, 480×272;
- cảm ứng **GT911**;
- microSD tối thiểu 8 GB.

Đây không phải firmware chung cho mọi board F133/D1s/T113 hoặc mọi LCD RGB.
Đọc [hardware-notes.md](hardware-notes.md) trước khi đổi DTS hay timing panel.

## Bạn cần gì

- Windows 10/11 64-bit và Internet ổn định.
- Ít nhất **30 GB trống** trong filesystem Linux/WSL. Lần build đầu tải và giải
  nén Buildroot, Linux, U-Boot và toolchain.
- MicroSD 8 GB trở lên để ghi image sau cùng.
- Không cần Visual Studio, CMake, Arduino hay toolchain ARM trên Windows để
  **build image**. Những thứ đó chỉ dành cho simulator UI trên PC.

Build phải chạy trong Ubuntu/WSL, không chạy trực tiếp trong PowerShell, Git
Bash hoặc MSYS2.

## 1. Cài WSL2 và Ubuntu

Mở **PowerShell bằng Run as administrator**, rồi chạy:

```powershell
wsl --status
wsl --list --online
```

Nhìn cột `NAME`, rồi dùng đúng một tên được liệt kê. Ví dụ nếu có
`Ubuntu-24.04`:

```powershell
wsl --install -d Ubuntu-24.04
```

Nếu máy chỉ liệt kê `Ubuntu`, dùng:

```powershell
wsl --install -d Ubuntu
```

Đừng tự đoán tên distro. Nếu gặp lỗi `Invalid distribution name`, chạy lại
`wsl --list --online` rồi copy đúng tên tại cột `NAME`. Khởi động lại Windows
nếu WSL yêu cầu.

Sau đó mở Ubuntu từ Start Menu. Lần đầu nó yêu cầu tạo **Linux username** và
password. Khi gõ password, màn hình không hiện dấu `*`; đây là bình thường.

Quay lại PowerShell để kiểm tra Ubuntu đang là WSL 2:

```powershell
wsl --list --verbose
```

Ở dòng Ubuntu, cột `VERSION` phải là `2`.

## 2. Clone repository vào filesystem Linux

Trong cửa sổ Ubuntu vừa mở, chạy lần lượt:

```bash
sudo apt-get update
sudo apt-get install -y git

mkdir -p ~/src
cd ~/src
git clone https://github.com/dangdangduong2k2/lcpi-t113-build-image.git
cd lcpi-t113-build-image
git status
```

Repository nên nằm ở `~/src/...`, **không** ở `/mnt/c`, `/mnt/d`, Desktop hay
OneDrive. Buildroot tạo rất nhiều file nhỏ; filesystem Linux nhanh và ổn định
hơn NTFS cho việc này.

Repository hiện là private. Nếu `git clone` hỏi xác thực, đăng nhập bằng tài
khoản GitHub đã được cấp quyền đọc repository. GitHub không chấp nhận password
tài khoản cho HTTPS Git; dùng token truy cập cá nhân khi Git yêu cầu password,
và không dán token vào source code, README hay command history.

## 3. Cài dependency Buildroot

Vẫn trong thư mục repository:

```bash
bash scripts/setup-host.sh
```

Script sẽ hỏi password Linux qua `sudo`, rồi cài compiler, thư viện ncurses,
Python, wget và các tool host cần cho Buildroot. Kết thúc đúng sẽ in:

```text
Host dependencies are ready. Next: make configure && make build
```

## 4. Cấu hình và build lần đầu

```bash
make configure
make build
```

Ý nghĩa từng lệnh:

| Lệnh | Việc nó làm |
|---|---|
| `make configure` | tải Buildroot `2026.05.1`, kiểm SHA-256, nạp `configs/lcpi_t113_defconfig` |
| `make build` | build U-Boot, Linux `6.18.8`, rootfs, LVGL và tạo raw microSD image |

Không đóng Ubuntu/WSL trong khi `make build` đang chạy. Lần đầu có thể mất khá
lâu vì phải tải và biên dịch toàn bộ toolchain. Nếu bị ngắt do mạng hoặc tắt
máy, quay lại đúng repository rồi chạy lại:

```bash
make build
```

Máy ít RAM hoặc hay hết bộ nhớ có thể giảm số luồng:

```bash
LCPI_JOBS=2 make build
```

Sau khi hoàn tất, in các đường dẫn thật mà Makefile đang dùng:

```bash
make print-paths
```

## 5. Kiểm tra file image

Image đầu ra luôn là:

```text
~/lcpi-t113-work/output/images/sdcard.img
```

Kiểm tra file và ghi lại hash trước khi flash:

```bash
IMAGE="$HOME/lcpi-t113-work/output/images/sdcard.img"

test -f "$IMAGE" && stat -c '%n: %s bytes' "$IMAGE"
sha256sum "$IMAGE"
```

Cấu hình mặc định hiện tạo image khoảng 257 MiB; đây là bình thường dù thẻ là
8 GB. Nó là **raw disk image**, không phải archive ZIP và không phải Tina
`IMAGEWTY`/PhoenixCard image.

Để mở thư mục image bằng Explorer Windows từ Ubuntu:

```bash
explorer.exe "$(wslpath -w "$HOME/lcpi-t113-work/output/images")"
```

Tiếp tục với [02-flash-microsd-and-first-boot.md](02-flash-microsd-and-first-boot.md).
