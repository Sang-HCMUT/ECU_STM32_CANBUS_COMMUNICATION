# Tổng hợp các Lệnh Debug & Build cho Đồ án ECU (STM32)

Dưới đây là danh sách các lệnh cực kỳ hữu ích để bạn thao tác trực tiếp trên Terminal / PowerShell trong quá trình làm đồ án mà không cần phải mở giao diện (GUI) rườm rà.

## 1. Nạp Code (Flashing) bằng OpenOCD

Lệnh này dùng để nạp file `.elf` (đã được build) xuống vi điều khiển STM32F103 thông qua mạch nạp **CMSIS-DAP**. 
*(Đảm bảo bạn đang mở Terminal ở thư mục gốc của project EPS)*

```powershell
openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg -c "program build/Debug/EPS.elf verify reset exit"
```

> **Lưu ý:**
> - Nếu bạn đổi sang dùng mạch nạp **ST-Link V2**, hãy đổi `interface/cmsis-dap.cfg` thành `interface/stlink-v2.cfg` hoặc `interface/stlink.cfg`.
> - Lỗi `Error: open failed` -> Kiểm tra lại cáp kết nối USB hoặc chắc chắn rằng không có phần mềm nào khác (như STM32CubeIDE) đang chiếm dụng (lock) mạch nạp.
> - Lỗi `** Programming Failed **` -> 99% do nguồn cấp (chân VCC, GND) bị lỏng hoặc sụt áp trong quá trình nạp. Rút các tải nặng (như Servo) ra và nạp lại.

---

## 2. Giao tiếp UART (Serial Monitor) bằng PowerShell

Thay vì mở các tool ngoài như PuTTY hay Serial Monitor của VS Code, bạn có thể đọc log trực tiếp bằng PowerShell cực ngầu.

### 🔍 Tìm nhanh các cổng COM đang cắm
Khi không nhớ mạch đang cắm ở cổng COM mấy, gõ dòng này vào PowerShell, nó sẽ liệt kê tất cả các cổng (VD: `COM3`, `COM4`...):

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()
```

### 📺 Đọc Log UART liên tục
Chạy script sau để mở cổng COM và đọc từng dòng log mạch gửi lên. 
*(Nhớ đổi chữ `COM4` thành cổng COM của bạn).*

```powershell
$port = New-Object System.IO.Ports.SerialPort COM4, 115200, None, 8, One
$port.Open()
while ($true) { Write-Host $port.ReadLine() }
```

> **Tuyệt chiêu thao tác:**
> - **Để "đóng băng" (tạm dừng) log:** Click chuột trái vào giữa Terminal và giữ kéo rê nhẹ một xíu (Windows QuickEdit Mode). Màn hình sẽ đứng im để bạn bôi đen Copy dễ dàng.
> - **Để ngắt kết nối (Stop):** Ấn tổ hợp phím **`Ctrl + C`**. Bắt buộc phải tắt nếu bạn muốn nạp code mới, nếu không cổng COM sẽ báo lỗi Access Denied.

---

## 3. Build Project qua Command Line (Dành cho CMake/Ninja)

Nếu bạn thiết lập môi trường C/C++ (cài CMake và Ninja vào biến môi trường PATH), bạn có thể gõ phím để build code cực nhanh thay vì phải bấm nút trên giao diện.

```powershell
# Cấu hình project (Chỉ cần chạy 1 lần khi mới clone về)
cmake -B build/Debug -G Ninja

# Biên dịch ra file .elf (Chạy mỗi khi sửa file .c / .h)
ninja -C build/Debug
```
