# Hệ thống ECU trên xe ô tô (STM32) & Dashboard

Repository này lưu trữ toàn bộ mã nguồn của hệ thống mô phỏng các ECU trên xe ô tô sử dụng vi điều khiển STM32, giao tiếp qua mạng CAN và một giao diện Dashboard để giám sát/điều khiển.

## Cấu trúc thư mục

- `ABS/`: Mã nguồn Firmware cho hệ thống Chống bó cứng phanh (Anti-lock Braking System).
- `ADAS/`: Mã nguồn Firmware cho hệ thống Hỗ trợ lái xe nâng cao (Advanced Driver Assistance Systems).
- `EPS/`: Mã nguồn Firmware cho hệ thống Lái trợ lực điện (Electronic Power Steering).
- `dashboard/`: Ứng dụng Dashboard dùng để hiển thị thông số và điều khiển hệ thống.
- `common/`: Các thư viện và mã nguồn dùng chung cho các ECU.
- Các file `.md`: Chứa tài liệu báo cáo tổng hợp và thiết kế hệ thống.

## Hướng dẫn sử dụng (How to Use)

### 1. Nạp Firmware cho các ECU (STM32)
- **Công cụ yêu cầu**: STM32CubeIDE (hoặc Keil C/VS Code tùy vào cấu hình project) và mạch nạp ST-Link/J-Link.
- **Các bước thực hiện**:
  1. Mở phần mềm STM32CubeIDE.
  2. Chọn `File > Open Projects from File System...` và trỏ đường dẫn đến thư mục chứa ECU tương ứng (ví dụ: `ABS`, `ADAS` hoặc `EPS`).
  3. Build project (nhấn biểu tượng cái búa hoặc `Project > Build All`).
  4. Kết nối mạch STM32 với máy tính qua mạch nạp ST-Link.
  5. Nhấn nút Run/Debug để nạp chương trình xuống vi điều khiển.
  6. Lặp lại quá trình cho các node ECU còn lại.

### 2. Khởi chạy Dashboard
1. Mở terminal (Command Prompt / PowerShell) và di chuyển vào thư mục `dashboard/`.
2. Tùy thuộc vào công nghệ sử dụng của Dashboard:
   - Nếu là dự án **Node.js/React/Vue**: chạy `npm install` để cài đặt thư viện, sau đó chạy `npm start` hoặc `npm run dev`.
   - Nếu là dự án **Python**: cài đặt môi trường và thư viện `pip install -r requirements.txt`, sau đó chạy file thực thi chính (ví dụ `python main.py`).
3. Đảm bảo phần cứng chuyển đổi tín hiệu (CAN to USB hoặc UART to USB) đã được kết nối với máy tính và chọn đúng cổng COM trên giao diện để Dashboard có thể nhận và gửi dữ liệu từ mạng CAN.

## Tài liệu tham khảo
Đọc các file tài liệu Markdown ở thư mục gốc (đặc biệt là `BAO_CAO_TONG_HOP_HOAN_CHINH.md` và `DATN_TAI_LIEU_TONG_HOP.md`) để hiểu rõ hơn về:
- Sơ đồ khối hệ thống
- Kiến trúc phần cứng / phần mềm
- Bản đồ định danh (ID) và luồng dữ liệu thông điệp CAN (CAN Matrix).
