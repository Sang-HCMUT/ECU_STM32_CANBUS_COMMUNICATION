# Sơ đồ Kiến Trúc & Luồng Giao Tiếp (Flowchart) Hệ thống 3 Node ECU

Dưới đây là sơ đồ luồng và sơ đồ tuần tự chi tiết mô tả cách các ECU (ADAS, EPS, ABS) tương tác với nhau qua mạng CAN Bus và giao tiếp với màn hình IVI Dashboard trong kiến trúc mới nhất.

## 1. Kiến Trúc Tổng Thể & Giao Tiếp CAN (Flowchart)

```mermaid
%%{init: {"flowchart": {"nodeSpacing": 15, "rankSpacing": 20}}}%%
graph LR
    subgraph EPS [EPS - Trợ Lái & Giả Lập Động Cơ]
        ADC_V["Vô lăng (ADC0)"] --> |"Anti-Jitter"| Servo["PWM (Servo Lái)"]
        Racing["Giả lập Đua xe (Racing Profile)"] --> |"Tạo Ga (0-100%)"| Override
        ADC_P["Chân Phanh (ADC1)"] --> |"Tạo Phanh (0-100%)"| Override
        Crash_EPS["Nhận Cờ Crash (0x200)"] --> Override{"Safety Override?"}
        Override --> |"Bình thường"| CAN_TX1["TX: ID 0x100 (Ga, Phanh, Lái)"]
        Override --> |"Tai nạn"| C_Force["Ép Ga=0, Phanh=100"] --> CAN_TX1
    end

    subgraph ADAS [ADAS - Giám Sát & Điều Phối]
        MPU["MPU6050 (I2C)"] --> |"Pitch/Roll"| CrashLogic{"Gia tốc bất thường?"}
        CrashLogic --> |"Crash"| CAN_TX2["TX: ID 0x200 [1] (Emergency)"]
        Btn["Nút Reset"] --> |"Unlock"| CAN_TX2_OK["TX: ID 0x200 [0] (Safe)"]
        CAN_RX_ADAS["CAN RX (0x100, 0x102)"] --> Aggregator["Tổng hợp dữ liệu JSON"]
        Aggregator --> UART_TX["Xuất UART ra Dashboard"]
    end

    subgraph ABS [ABS - Phanh & Truyền Động]
        CAN_RX_ABS["CAN RX (0x100, 0x200)"]
        CAN_RX_ABS --> |"ID 0x200 [1]"| Cutoff["Cắt nguồn Động cơ (PWM=0)"]
        CAN_RX_ABS --> |"ID 0x100"| ProcessABS["Xử lý Phanh/Ga (Soft-Start)"]
        ProcessABS --> Motor["PWM L9110S (Quay Bánh)"]
        WheelSens["Cảm biến Tốc độ (EXTI)"] --> CalcRPM["Tính RPM thực tế"]
        CalcRPM --> CAN_TX3["TX: ID 0x102 (RPM, ABS_Flag)"]
    end
    
    subgraph IVI [Dashboard - Orange Pi 3B]
        UART_RX["Nhận Serial JSON"] --> UI_Override{"Có Crash?"}
        UI_Override --> |"Có"| UI_Crash["Ép UI Ga=0, Tốc độ=0, Hiện Overlay Đỏ"]
        UI_Override --> |"Không"| UI_Normal["Render 3D Car & Arc Gauge"]
    end

    CAN_TX1 --> CAN_RX_ADAS
    CAN_TX1 --> CAN_RX_ABS
    
    CAN_TX2 --> Crash_EPS
    CAN_TX2 --> CAN_RX_ABS
    CAN_TX2_OK --> Crash_EPS
    
    CAN_TX3 --> CAN_RX_ADAS
    
    UART_TX --> UART_RX
```

## 2. Sơ Đồ Tuần Tự Xử Lý Khẩn Cấp (Crash Sequence Diagram)

```mermaid
sequenceDiagram
    participant ADAS as ECU ADAS (Coordinator)
    participant EPS as ECU EPS (Simulator)
    participant ABS as ECU ABS (Powertrain)
    participant IVI as Dashboard (UI)

    Note over ADAS, IVI: Trạng thái bình thường (Đang đua xe)
    EPS->>ABS: CAN 0x100 (Ga 100%, Phanh 0%)
    ABS->>ADAS: CAN 0x102 (RPM 200)
    EPS->>ADAS: CAN 0x100 (Ga 100%, Phanh 0%)
    ADAS->>IVI: UART JSON (Ga: 100, RPM: 200, Crash: 0)
    
    Note over ADAS, IVI: Phát hiện Tai Nạn (Crash)
    ADAS->>EPS: CAN 0x200 [1] (Báo động khẩn cấp)
    ADAS->>ABS: CAN 0x200 [1] (Báo động khẩn cấp)
    ADAS->>IVI: UART JSON (Crash: 1)
    
    IVI->>IVI: Frontend Override: Ép đồng hồ = 0, Cây Ga = 0
    EPS->>EPS: Dừng Servo. Cưỡng chế Ga=0%, Phanh=100%
    ABS->>ABS: Cắt toàn bộ xung PWM ra động cơ (Dừng quay)
    
    EPS->>ABS: CAN 0x100 (Ga 0%, Phanh 100%)
    ABS->>ADAS: CAN 0x102 (RPM 0)
    
    Note over ADAS, IVI: Phục hồi (Nhấn nút Reset trên ADAS)
    ADAS->>EPS: CAN 0x200 [0] (An toàn)
    ADAS->>ABS: CAN 0x200 [0] (An toàn)
    
    EPS->>EPS: Mở khóa Servo. Reset timer vòng đua về 0s
    ABS->>ABS: Cho phép nhận Ga/Phanh lại bình thường
    ADAS->>IVI: UART JSON (Crash: 0)
    IVI->>IVI: Ẩn Overlay đỏ, Resume Animation
```
