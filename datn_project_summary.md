# BÁO CÁO TỔNG KẾT DỰ ÁN: MÔ PHỎNG HỆ THỐNG MẠNG CAN TRÊN Ô TÔ (EPS, ABS, ADAS, IVI)

Tài liệu này tổng hợp toàn bộ thông tin kỹ thuật, kiến trúc hệ thống, và các giải pháp tối ưu đã được áp dụng trong dự án. Tài liệu được thiết kế làm bộ khung chuẩn mực để viết Báo cáo Đồ Án Tốt Nghiệp (ĐATN).

---

## 1. TỔNG QUAN KIẾN TRÚC HỆ THỐNG (SYSTEM ARCHITECTURE)

Dự án mô phỏng mạng lưới giao tiếp chuẩn trên ô tô thực tế, bao gồm **3 Node ECU (Electronic Control Unit)** giao tiếp với nhau qua mạng **CAN Bus** và **1 Node IVI (In-Vehicle Infotainment)** đóng vai trò Dashboard hiển thị.

### 1.1 Sơ đồ giao tiếp Mạng
*   **Giao thức lõi:** CAN Bus 2.0A (Standard ID 11-bit), Tốc độ 500Kbps.
*   **Các Node tham gia:**
    1.  **Node EPS** (Electronic Power Steering & Racing Sim) - Phát `0x100`, Nhận `0x200`
    2.  **Node ABS** (Anti-lock Braking System) - Phát `0x102`, Nhận `0x100`, `0x200`
    3.  **Node ADAS** (Advanced Driver Assistance System & Gateway) - Phát `0x200`, Nhận `0x100`, `0x102`
*   **Giao tiếp HMI (Human-Machine Interface):** Node ADAS tổng hợp dữ liệu từ CAN Bus, đóng gói thành chuỗi JSON và truyền qua giao thức **UART** (Web Serial API) lên Dashboard.

---

## 2. CHI TIẾT TỪNG NODE ECU & GIẢI PHÁP TỐI ƯU

### 2.1 Node EPS (Trợ lực lái điện & Giả lập vòng đua)
Đóng vai trò là buồng lái giả lập, tạo ra các tín hiệu đầu vào cho toàn bộ hệ thống.

*   **Chức năng chính:**
    *   Đọc góc quay vô lăng vật lý (Biến trở) thông qua ADC kênh 0.
    *   Tự động sinh tín hiệu Chân Ga (Throttle) và Chân Phanh (Brake) theo chu trình Đua xe (Racing Simulation 30 giây) sử dụng `HAL_GetTick()`.
    *   Điều khiển động cơ Servo (PWM TIM3) mô phỏng góc đánh lái của bánh xe dựa trên góc vô lăng.
*   **Giao tiếp CAN:**
    *   **TX (Transmit):** ID `0x100` (DLC = 3 byte). `[Ga, Phanh, Góc Lái]`.
    *   **RX (Receive):** Lắng nghe ID `0x200` từ ADAS. Nếu nhận được cờ `Crash = 1`, ngay lập tức dừng động cơ Servo và ngừng sinh Ga/Phanh.
*   **Điểm Tối ưu (Optimization):**
    *   Sử dụng **DMA (Direct Memory Access)** cho bộ ADC đọc vô lăng. CPU hoàn toàn không tốn tài nguyên chờ lấy mẫu.
    *   Thuật toán giả lập dùng hàm modulo thời gian non-blocking, giải phóng vòng lặp `while(1)` chạy ở tốc độ tối đa.

### 2.2 Node ABS (Hệ thống Chống bó cứng phanh)
Đóng vai trò điều khiển truyền động và mô phỏng bánh xe vật lý.

*   **Chức năng chính:**
    *   Thu thập dữ liệu Ga/Phanh từ EPS qua CAN Bus để xuất xung PWM điều khiển Động cơ DC kép (mô phỏng 2 bánh xe).
    *   Đọc tốc độ thực tế (RPM) của bánh xe bằng Optical Encoder (Mắt đọc hồng ngoại đĩa code) qua Ngắt ngoài (External Interrupt - EXTI).
    *   **Thuật toán ABS:** Khi người dùng đạp phanh gấp (Brake > 30%), liên tục giám sát tốc độ bánh. Nếu bánh xe tụt vòng tua quá nhanh (bó cứng), hệ thống ngắt/nhả PWM theo chu kỳ 50ms độc lập từng bánh để duy trì quán tính.
*   **Giao tiếp CAN:**
    *   **TX (Transmit):** ID `0x102` (DLC = 3 byte). `[RPM_High, RPM_Low, Trạng thái ABS]`.
    *   **RX (Receive):** Lắng nghe ID `0x100` (Ga/Phanh từ EPS) và ID `0x200` (Khoá hệ thống nếu ADAS báo tai nạn).
*   **Điểm Tối ưu (Optimization):**
    *   **Thuật toán Soft-Start:** Tốc độ động cơ được điều áp mượt mà qua bộ lọc Low-pass filter (thay đổi 2% mỗi 10ms), chống giật cục và sụt nguồn.
    *   Toàn bộ luồng đo đếm Encoder và PID hoàn toàn chạy trên ngắt và hàm định thời non-blocking.

### 2.3 Node ADAS & Gateway (Hỗ trợ lái & Nút giao tiếp trung tâm)
Đóng vai trò "Bộ não" an toàn và là cầu nối (Gateway) giữa mạng lưới CAN của xe và màn hình hiển thị.

*   **Chức năng chính:**
    *   Đọc cảm biến IMU (MPU6050) qua chuẩn I2C để lấy gia tốc 3 chiều (Ax, Ay, Az).
    *   **Crash Detection:** Tính toán Vector gia tốc tổng V = sqrt(Ax^2 + Ay^2 + Az^2). Nếu V > 2.5g (Va chạm mạnh), kích hoạt Cờ Tai Nạn (Crash Flag) và chốt khoá hệ thống vĩnh viễn (Phải Reset bằng tay). Đồng thời Hú còi Buzzer cảnh báo (PWM).
    *   **CAN Gateway:** Dùng Hardware CAN Filter ở chế độ IDMASK `0x0000` (Sniffer) để gom toàn bộ dữ liệu từ các Node khác trên Bus.
    *   Đóng gói toàn bộ thành định dạng **JSON** và đẩy qua cổng USART1 lên Dashboard với tần số 60Hz.
*   **Giao tiếp CAN:**
    *   **TX (Transmit):** ID `0x200` (DLC = 1 byte). Gửi tín hiệu Khẩn cấp `[1]` khi có tai nạn, hoặc lệnh Reset `[0]` khi board được khởi động lại.
*   **Điểm Tối ưu (Optimization):**
    *   **I2C Non-blocking Fallback:** Ép thời gian chờ (Timeout) của hàm `HAL_I2C_Mem_Read` xuống vỏn vẹn `10ms`. Tuyệt đối ngăn chặn hiện tượng ADAS bị "Treo" (Hanging) nếu dây cảm biến bị nhiễu hoặc đứt.
    *   Tránh xung đột Bus (Arbitration Drop): Không Spam liên tục bản tin an toàn, chỉ gửi tín hiệu `0x200` khi có sự kiện (Event-driven).

### 2.4 Màn hình IVI (In-Vehicle Infotainment) - Dashboard
Giao diện điều khiển trung tâm viết bằng công nghệ Web.

*   **Công nghệ:** HTML5, CSS3, JavaScript (Three.js cho render 3D).
*   **Chức năng:**
    *   Giao tiếp trực tiếp với vi điều khiển không cần cài Driver qua **Web Serial API** siêu nhanh.
    *   **Auto-Calibration:** Tự động hiệu chỉnh cân bằng góc nghiêng của IMU trong 30 frame đầu tiên để bù trừ sai số đặt mạch.
    *   Hiển thị Real-time: Vô lăng xoay, Bàn đạp Ga/Phanh lên xuống, Đồng hồ Tốc độ (KM/H) và Vòng tua (RPM).
    *   Mô hình xe 3D nghiêng theo gia tốc thực tế của mạch ADAS (Roll & Pitch).
    *   Hiệu ứng "Màn hình máu" (Crash Overlay) đỏ lừ khi nhận cờ tai nạn.

---

## 4. ĐỊNH HƯỚNG NÂNG CẤP (PHASE 3)

Để đồ án bám sát nhất với kiến trúc xe điện hiện đại (ví dụ Tesla, VinFast):
1.  **Dịch chuyển Gateway sang SBC (Single Board Computer):**
    *   Sử dụng board **Orange Pi 3B** chạy hệ điều hành Linux làm Node IVI trung tâm thay cho Laptop cá nhân.
    *   Kết nối module **MCP2515 + MCP2551 (SPI to CAN)** trực tiếp lên chân GPIO của Orange Pi. Bản thân Orange Pi sẽ trở thành 1 Node CAN độc lập, loại bỏ hoàn toàn cơ chế giao tiếp "rẻ nhánh" qua cáp USB-UART.
2.  **Embedded UI (Kiosk Mode):**
    *   Tự động boot hệ điều hành Linux vào chế độ toàn màn hình ẩn trình duyệt, kết nối với màn hình cảm ứng DSI để tạo ra cụm đồng hồ Taplo xe hơi thật 100%.
