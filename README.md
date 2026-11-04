# Shared Memory IPC on Ubuntu

Thư viện C nhỏ giúp nhiều process trao đổi dữ liệu qua POSIX Shared Memory.
Đồng bộ (mutex + condition variable) được xử lý bên trong thư viện, người dùng
chỉ cần gọi `shm_write_vku()` / `shm_read_vku()`.

## Cấu trúc thư mục

```
.
├── include/
│   └── shm_vku.h                      # API public
├── src/
│   └── shm_vku.c                      # Hiện thực API
├── test/
│   ├── test_one_writer_one_reader.c   # Test 1: 1 writer, 1 reader
│   └── test_two_writers_one_reader.c  # Test 2: 2 writer, 1 reader
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

Tuỳ chọn, để debug:

```bash
sudo apt-get install -y strace valgrind
```

## Build và test

```bash
make            # tạo build/libshm_vku.a
make test       # build và chạy cả 2 test
make clean      # xoá thư mục build/
```

Chạy riêng từng test, thêm `-v` để xem reader đọc được gì:

```bash
./build/test_one_writer_one_reader -v
./build/test_two_writers_one_reader -v
```

## API

Tất cả hàm khai báo trong `include/shm_vku.h`. Hàm trả `int` trả về `0` khi
thành công, `-1` khi lỗi và set `errno` (dùng `perror()` để in lỗi).

### `struct shm_vku`

Handle của một segment. Người dùng tự khai báo (`struct shm_vku shm;`) rồi
truyền địa chỉ vào các hàm bên dưới. Không cần đọc/ghi các field bên trong.

### Tạo và mở

| Hàm | Mô tả |
|---|---|
| `int shm_create_vku(struct shm_vku *shm, const char *name, size_t size)` | Tạo segment mới tên `name` (phải bắt đầu bằng `/`, ví dụ `"/my_shm"`) với vùng data `size` byte. Lỗi `EEXIST` nếu tên đã tồn tại. Process tạo chịu trách nhiệm gọi `shm_destroy_vku()`. |
| `int shm_open_vku(struct shm_vku *shm, const char *name, size_t size)` | Mở segment đã được tạo. `size` phải giống lúc tạo. Lỗi `ENOENT` nếu chưa tồn tại. |

### Ghi và đọc

| Hàm | Mô tả |
|---|---|
| `int shm_write_vku(struct shm_vku *shm, const void *buf, size_t len)` | Copy `len` byte từ `buf` vào segment, rồi đánh thức tất cả reader đang chờ. Lỗi `EINVAL` nếu `len` lớn hơn `size`. |
| `ssize_t shm_read_vku(struct shm_vku *shm, void *buf, size_t len, unsigned long *version)` | Chờ tới khi có data mới hơn `*version`, copy `len` byte (tối đa `size`) vào `buf`, cập nhật `*version`. Trả về số byte đã copy. |

Cách dùng `version`:

- Khởi tạo `unsigned long version = 0;` để chờ lần ghi đầu tiên.
- Gọi lại `shm_read_vku()` với **cùng biến đó** để nhận mỗi bản cập nhật mới.
- `shm_read_vku()` luôn trả về **bản mới nhất**, không phải hàng đợi: nếu writer
  ghi nhiều lần trước khi reader kịp đọc, các bản ở giữa bị bỏ qua.

### Đóng và xoá

| Hàm | Mô tả |
|---|---|
| `int shm_close_vku(struct shm_vku *shm)` | Unmap và đóng segment trong process hiện tại. Segment vẫn còn trong `/dev/shm`. |
| `int shm_destroy_vku(const char *name)` | Xoá segment khỏi hệ thống. Process đang dùng vẫn dùng tiếp được tới khi close. |

## Ví dụ

Writer:

```c
#include "shm_vku.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
	struct shm_vku shm;
	const char *msg = "hello";

	if (shm_create_vku(&shm, "/my_shm", 4096) < 0) {
		perror("shm_create_vku");
		return 1;
	}

	shm_write_vku(&shm, msg, strlen(msg) + 1);
	getchar();	/* chờ reader đọc xong */

	shm_close_vku(&shm);
	shm_destroy_vku("/my_shm");
	return 0;
}
```

Reader:

```c
#include "shm_vku.h"

#include <stdio.h>

int main(void)
{
	struct shm_vku shm;
	unsigned long version = 0;
	char buf[64];

	if (shm_open_vku(&shm, "/my_shm", 4096) < 0) {
		perror("shm_open_vku");
		return 1;
	}

	shm_read_vku(&shm, buf, sizeof(buf), &version);
	printf("read: %s\n", buf);

	shm_close_vku(&shm);
	return 0;
}
```

Compile với thư viện:

```bash
make
gcc -Iinclude writer.c build/libshm_vku.a -o writer
gcc -Iinclude reader.c build/libshm_vku.a -o reader
```

Chạy writer trước, reader sau (2 terminal).

## Lưu ý

- **Chạy process tạo segment trước** các process mở segment.
- Writer và reader phải dùng **cùng `name` và `size`**, nên đặt chung một `#define`.
- Nhiều writer cùng ghi được: một process `create`, các process còn lại `open`.
- Nếu chương trình bị kill trước khi gọi `shm_destroy_vku()`, segment còn sót
  trong `/dev/shm` và lần chạy sau `create` sẽ lỗi `EEXIST`. Xoá tay bằng
  `rm /dev/shm/my_shm`.
