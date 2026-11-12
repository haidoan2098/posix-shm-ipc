# Shared Memory IPC on Ubuntu

Thư viện C nhỏ giúp nhiều process trao đổi dữ liệu qua POSIX Shared Memory
(`shm_open`, `ftruncate`, `mmap`, `munmap`, `shm_unlink`).

Đồng bộ tối thiểu bằng một **ready-flag**: writer ghi xong thì bật flag, reader
chờ flag rồi mới đọc.

## Cấu trúc thư mục

```
.
├── include/shm_vku.h      # API public
├── src/shm_vku.c          # Hiện thực API
├── demo/
│   ├── demo1/             # 1 writer, 1 reader
│   ├── demo2/             # 1 writer, 2 reader
│   └── demo3/             # benchmark: pipe vs shared memory
│       (demo1, demo2 gồm writer.c và reader.c)
├── docs/
├── Makefile
└── README.md
```

## Cài đặt

Yêu cầu: Ubuntu 22.04 trở lên (glibc >= 2.34, không cần `-lrt` hay `-lpthread`).

```bash
sudo apt-get update
sudo apt-get install -y build-essential   # gcc, make, glibc headers
```

## Build

```bash
make            # tạo build/libshm_vku.a và build/demo1..3
make clean      # xoá thư mục build/
```

## Chạy demo

Mở nhiều terminal, chạy từ thư mục gốc của project. Nhấn `Ctrl+C` để dừng.

### Demo 1: 1 writer, 1 reader

```bash
./build/demo1/writer    # terminal 1: nhấn Enter để bắt đầu ghi
./build/demo1/reader    # terminal 2: chạy trước khi nhấn Enter
```

**Thể hiện:** reader đứng chờ ready-flag, writer ghi xong thì reader nhận đúng
dữ liệu. Reader luôn nhận bản mới nhất đang có.

### Demo 2: 1 writer, 2 reader

```bash
./build/demo2/writer    # terminal 1: nhấn Enter để bắt đầu ghi
./build/demo2/reader    # terminal 2: tự sinh 2 process reader
```

**Thể hiện:** hai process khác nhau (PID khác nhau) cùng đọc một vùng shared
memory và thấy cùng dữ liệu.

### Demo 3: benchmark pipe vs shared memory

```bash
./build/demo3/benchmark
```

**Thể hiện:** so sánh thời gian truyền 10MB giữa `pipe()` và shared memory.

## API

Tất cả hàm khai báo trong `include/shm_vku.h`. Hàm trả `int` trả về `0` khi
thành công, `-1` khi lỗi và set `errno` (dùng `perror()` để in lỗi).

### `struct shm_info`

Chỗ mỗi process lưu thông tin về shared memory mà nó đã tạo hoặc mở (`fd`,
địa chỉ `mmap`, kích thước). Người dùng tự khai báo (`struct shm_info info;`)
rồi truyền địa chỉ vào các hàm bên dưới. Struct này nằm trong process, không
nằm trong shared memory, nên mỗi process có một bản riêng. Không cần đọc/ghi
các field bên trong.

### Tạo và mở

| Hàm | Mô tả |
|---|---|
| `int shm_create_vku(struct shm_info *info, const char *name, size_t size)` | Tạo shared memory mới tên `name` (phải bắt đầu bằng `/`, ví dụ `"/my_shm"`) với vùng data `size` byte. Lỗi `EEXIST` nếu tên đã tồn tại. Process tạo chịu trách nhiệm gọi `shm_destroy_vku()`. |
| `int shm_open_vku(struct shm_info *info, const char *name, size_t size)` | Mở shared memory đã được tạo. `size` phải giống lúc tạo. Lỗi `ENOENT` nếu chưa tồn tại. |

### Ghi và đọc

| Hàm | Mô tả |
|---|---|
| `int shm_write_vku(struct shm_info *info, const void *buf, size_t len)` | Copy `len` byte từ `buf` vào shared memory rồi bật ready-flag. Lỗi `EINVAL` nếu `len` lớn hơn `size`. |
| `int shm_read_vku(struct shm_info *info, void *buf, size_t len)` | Chờ tới khi ready-flag được bật, rồi copy `len` byte (tối đa `size`) vào `buf`. Chờ vô hạn nếu không ai ghi. |

### Đóng và xoá

| Hàm | Mô tả |
|---|---|
| `int shm_close_vku(struct shm_info *info)` | Unmap và đóng shared memory trong process hiện tại. Object vẫn còn trong `/dev/shm`. |
| `int shm_destroy_vku(const char *name)` | Xoá object khỏi hệ thống. Process đang dùng vẫn dùng tiếp được tới khi close. |

## Dùng thư viện trong chương trình khác

```c
#include "shm_vku.h"
```

```bash
make
gcc -Iinclude my_app.c build/libshm_vku.a -o my_app
```

Xem `demo/demo1/writer.c` và `demo/demo1/reader.c` làm ví dụ.

## Lưu ý

- **Chạy process tạo shared memory trước** các process mở nó. Ở demo 1 và 2 đó
  là writer.
- Writer và reader phải dùng **cùng `name` và `size`**, nên đặt chung một
  `#define`. `size` lớn hơn size thật của object sẽ làm chương trình bị `SIGBUS`
  khi chạm vào phần vượt.
- `shm_read_vku()` luôn trả về **bản mới nhất** đang có, không phải hàng đợi: nếu
  writer ghi nhiều lần trước khi reader đọc, các bản ở giữa bị bỏ qua. Reader
  cũng không biết bản đọc được là mới hay đã đọc rồi.
- **Giới hạn của ready-flag:** chỉ an toàn với **1 writer ghi một lần**, nhiều
  reader đọc thoải mái. Không có lock, nên nếu ghi lại trong lúc reader đang đọc,
  hoặc có nhiều writer cùng ghi, reader có thể nhận dữ liệu dở dang.
  Muốn dùng nhiều writer thì cần thêm khoá (ví dụ mutex dùng chung giữa các
  process).
- Cờ `ready` là `volatile bool`: `volatile` chỉ bắt compiler luôn đọc lại cờ từ
  bộ nhớ, không ngăn CPU đổi thứ tự thao tác. Chạy đúng trên x86, chưa đảm bảo
  trên kiến trúc khác như ARM.
- Nếu chương trình bị kill trước khi gọi `shm_destroy_vku()`, object còn sót
  trong `/dev/shm`. Các chương trình trong `demo/` tự xoá bản cũ khi khởi động;
  với chương trình của bạn thì xoá tay bằng `rm /dev/shm/<tên>` (bỏ dấu `/` đầu
  tên).
