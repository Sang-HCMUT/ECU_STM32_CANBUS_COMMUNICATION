/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "can.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t adc_values[3]; // Đọc giá trị ADC từ 3 kênh

CAN_TxHeaderTypeDef TxHeader; // Header cho bản tin CAN truyền
uint8_t TxData[2]; // Dữ liệu truyền (Ga, Phanh)
uint32_t TxMailbox; // Mailbox cho bản tin CAN truyền

uint32_t last_can_tx_time = 0; // Thời gian truyền CAN trước

/* Low Power / Idle Mode Variables */
uint32_t last_active_time = 0; // Thời gian hoạt động trước
uint8_t is_idle_mode = 0; // Trạng thái idle
volatile uint8_t eps_emergency_stop = 0; // Trạng thái khẩn cấp từ ADAS (0: Normal, 1: Emergency)       
volatile uint32_t racing_start_offset = 0; // Lưu mốc thời gian reset vòng đua (0: Normal, 1: Emergency)
uint16_t last_adc_values[3] = {0, 0, 0}; // Giá trị ADC trước đó (Vô lăng, Phanh)
const uint16_t ADC_NOISE_THRESHOLD = 20; // Ngưỡng nhiễu ADC (20: Ngưỡng nhiễu)
const uint32_t IDLE_TIMEOUT_MS = 3000; // Thời gian idle (3000ms: 3s)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Ánh xạ giá trị ADC sang PWM
long map(long x, long in_min, long in_max, long out_min, long out_max) 
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min; 
} 
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_CAN_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  printf("System Init OK. UART Log Ready.\r\n"); // Log thông báo khởi động thành công

  /* Khởi động TIM3_CH1 PWM trên chân PA6 (Điều khiển Servo) */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

  /* Khởi động ADC1 quét 2 kênh liên tục bằng DMA (Vô lăng, Phanh) */
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_values, 2); 

  /* Cấu hình CAN Filter (Chấp nhận tất cả các ID) */
  CAN_FilterTypeDef canfilterconfig = {0};
  canfilterconfig.FilterActivation = CAN_FILTER_ENABLE;
  canfilterconfig.FilterBank = 0;
  canfilterconfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  canfilterconfig.FilterIdHigh = 0;
  canfilterconfig.FilterIdLow = 0;
  canfilterconfig.FilterMaskIdHigh = 0;
  canfilterconfig.FilterMaskIdLow = 0;
  canfilterconfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canfilterconfig.FilterScale = CAN_FILTERSCALE_32BIT;
  canfilterconfig.SlaveStartFilterBank = 14;
  HAL_CAN_ConfigFilter(&hcan, &canfilterconfig);

  HAL_CAN_Start(&hcan);
  HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);

  TxHeader.StdId = 0x100;
  TxHeader.ExtId = 0;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 3;
  TxHeader.TransmitGlobalTime = DISABLE;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* Check for ADC changes to determine activity (Check Vô lăng và Phanh) */
    uint8_t activity_detected = 0; // Trạng thái hoạt động (0: Normal, 1: Activity)
    for (int i = 0; i < 2; i++) {
        int diff = (int)adc_values[i] - (int)last_adc_values[i]; // Tính toán sự khác biệt giữa giá trị ADC hiện tại và giá trị ADC trước đó
        if (diff > ADC_NOISE_THRESHOLD || diff < -ADC_NOISE_THRESHOLD) { // Nếu sự khác biệt lớn hơn ngưỡng nhiễu, thì hoạt động đã xảy ra
            activity_detected = 1; // Nếu sự khác biệt lớn hơn ngưỡng nhiễu, thì hoạt động đã xảy ra (1: Activity, 0: Normal)
            last_adc_values[i] = adc_values[i]; // Cập nhật giá trị ADC trước đó
        }
    }

    if (activity_detected) {
        last_active_time = HAL_GetTick(); // Cập nhật thời gian hoạt động
        if (is_idle_mode && !eps_emergency_stop) {
            is_idle_mode = 0; // Chuyển sang trạng thái hoạt động
            HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
            printf("WAKE UP - Exit Idle Mode\r\n"); // Log thông báo chuyển sang trạng thái hoạt động     
        }
    } else { 
        if (!is_idle_mode && (HAL_GetTick() - last_active_time > IDLE_TIMEOUT_MS)) { 
            is_idle_mode = 1; // Chuyển sang trạng thái idle
            HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1); // Ngắt nguồn Servo
            printf("ENTER SLEEP MODE - Servo PWM Stopped\r\n"); // Log thông báo chuyển sang trạng thái idle     
        }
    }

    /* 1. Xử lý EPS (Trợ lái cục bộ): Điều khiển Servo có làm mượt (EMA & Slew Rate) */
    static uint32_t last_eps_update = 0; // Thời gian truyền CAN trước
    if (!is_idle_mode && !eps_emergency_stop && (HAL_GetTick() - last_eps_update >= 20)) {
        last_eps_update = HAL_GetTick(); // Cập nhật thời gian truyền CAN
        
        // a. Lọc nhiễu ADC bằng bộ lọc EMA (Exponential Moving Average)
        // Trọng số 0.15 lọc sạch hoàn toàn nhiễu điện áp mà độ trễ siêu nhỏ (~100ms)
        static float smoothed_adc = 2048.0f; // Giá trị ADC trung bình
        smoothed_adc = (0.15f * adc_values[0]) + (0.85f * smoothed_adc);
        
        // b. Ánh xạ sang xung PWM (1000us -> 2000us)
        uint32_t target_duty = (uint32_t)map((uint16_t)smoothed_adc, 0, 4095, 1000, 2000); // Ánh xạ giá trị ADC sang PWM (1000us -> 2000us)
        
        // c. Thuật toán Hysteresis kết hợp Slew Rate (Chống rung/rè tuyệt đối)
        static uint32_t last_duty = 1500; // Giá trị PWM trước đó
        int32_t diff = (int32_t)target_duty - (int32_t)last_duty; // Tính toán sự khác biệt giữa giá trị PWM hiện tại và giá trị PWM trước đó (1500us -> 2000us)    
        
        // Vùng mù 5us: Các dòng Servo giá rẻ (MG996R/SG90) cực kỳ nhạy cảm với nhiễu 1-2us.
        // Chỉ cho phép Servo nhúc nhích nếu mục tiêu lệch quá 5us (~0.5 độ).
        // Đảm bảo khi sếp buông tay, Servo sẽ KHOÁ CỨNG như tượng đá!
        if (diff > 5 || diff < -5) {
            
            // Giới hạn tốc độ văng (Slew Rate Limit): Tối đa 30us mỗi 20ms.
            // Ngăn chặn hiện tượng giật cục cơ khí (bẻ răng cưa) và sụt nguồn khi sếp vặn vô lăng quá bạo lực.
            if (diff > 30) {  
                last_duty += 30;
            } else if (diff < -30) {
                last_duty -= 30;
            } else {
                last_duty = target_duty;
            }
            
            // CHỈ cập nhật thanh ghi PWM khi giá trị THỰC SỰ thay đổi
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, last_duty);
        }
    }

    /* 2. Transmit CAN Bus: Gửi dữ liệu định kỳ (50ms khi active, 500ms khi idle) */ 
    uint32_t tx_interval = is_idle_mode ? 500 : 50; // Thời gian truyền CAN (50ms khi active, 500ms khi idle)    
    if (HAL_GetTick() - last_can_tx_time >= tx_interval)
    {
      last_can_tx_time = HAL_GetTick(); // Cập nhật thời gian truyền CAN

      // -------------------------------------------------------------
      // TÍNH NĂNG ADAS: RACING SIMULATION (Giả lập đua xe thể thao)
      // Sinh tín hiệu Ga ngầu như tay đua: Tăng tốc gắt -> Chuyển số (tụt ga) -> Đạp lút ga
      // -------------------------------------------------------------
      uint32_t current_time = (HAL_GetTick() - racing_start_offset) % 30000; // Chu kỳ đua 30 giây
      uint8_t throttle_percent = 0; // Tốc độ Ga (0-100%)
      
      if (current_time < 5000) {
          // Số 1 (0-5s): Đạp lút ga từ 0 -> 100% cực gắt
          throttle_percent = (current_time * 100) / 5000; // Tốc độ Ga (0-100%)
      } else if (current_time < 5500) {
          // Côn/Sang số 2 (5-5.5s): Tụt ga
          throttle_percent = 40; // Tốc độ Ga (0-100%)
      } else if (current_time < 12000) {
          // Số 2 (5.5-12s): Kéo từ 40% -> 100%
          uint32_t val = 40 + ((current_time - 5500) * 60) / 6500;
          throttle_percent = val > 100 ? 100 : val; // Tốc độ Ga (0-100%)
      } else if (current_time < 12500) {
          // Côn/Sang số 3 (12-12.5s): Tụt ga
          throttle_percent = 60; // Tốc độ Ga (0-100%)
      } else if (current_time < 20000) {
          // Số 3 (12.5-20s): Kéo từ 60% -> 100%
          uint32_t val = 60 + ((current_time - 12500) * 40) / 7500;
          throttle_percent = val > 100 ? 100 : val; // Tốc độ Ga (0-100%)
      } else if (current_time < 20500) {
          // Côn/Sang số 4 (20-20.5s): Tụt ga nhẹ
          throttle_percent = 80; // Tốc độ Ga (0-100%)
      } else if (current_time < 26000) {
          // Số 4 (20.5-26s): Đạp lút cán giữ max tốc độ
          uint32_t val = 80 + ((current_time - 20500) * 20) / 5500;
          throttle_percent = val > 100 ? 100 : val; // Tốc độ Ga (0-100%)
      } else {
          // 4 GIÂY CUỐI CÙNG: MÔ PHỎNG QUÁN TÍNH ĐỘNG CƠ & CAM ĐỘ (LUMPY IDLE)
          // Xóa bỏ các khối ga phẳng (square-wave) gây cảm giác giả tạo.
          // Áp dụng độ dốc suy giảm (Decay) để mô phỏng động cơ mất tua tự nhiên.
          
          uint32_t t = current_time - 26000;
          if (t < 400) {
              // Nhịp 1: Nẹt rát lên đỉnh 100% rồi trôi tụt dần về 20%
              throttle_percent = 100 - (t * 80) / 400; 
          } else if (t < 800) {
              // Nhịp 2: Nẹt bồi 70% rồi trôi tụt về 15%
              throttle_percent = 70 - ((t - 400) * 55) / 400; 
          } else if (t < 1100) {
              // Nhịp 3: Nẹt nhẹ 40% rồi trôi về 10%
              throttle_percent = 40 - ((t - 800) * 30) / 300; 
          } else {
              // Trạng thái Garanti (Idle): Mô phỏng động cơ thay Cam độ (xe đua).
              // Tua máy không bao giờ đứng im mà dao động lụp bụp liên tục (8% - 14%).
              uint8_t rumble = (t % 150 < 50) ? 14 : ((t % 100 < 30) ? 8 : 11);
              throttle_percent = rumble; // Tốc độ Ga (0-100%)
          }
      }
      
      uint16_t throttle_adc = (throttle_percent * 4095) / 100; // Giả lập số ADC để in ra log (0-4095)      
      // -------------------------------------------------------------


      uint16_t brake_adc = adc_values[1];    // Kênh 1 lúc này là Phanh (PA2)
      uint8_t brake_percent = (uint8_t)map(brake_adc, 0, 4095, 0, 100);
      uint8_t steering_can = (uint8_t)map(adc_values[0], 0, 4095, 0, 255);

      // -------------------------------------------------------------
      // XỬ LÝ KHẨN CẤP (CRASH HANDLING) - SAFETY OVERRIDE
      // Nếu ADAS báo tai nạn, mạch EPS phải lập tức khóa tín hiệu: Ngắt sạch Ga, Lút Cán Phanh.
      // Điều này đảm bảo Dashboard và ABS nhận được tín hiệu chính xác tuyệt đối.
      // -------------------------------------------------------------
      if (eps_emergency_stop) {
          throttle_percent = 0;
          brake_percent = 100;
      }

      TxData[0] = throttle_percent;
      TxData[1] = brake_percent;
      TxData[2] = steering_can;

      if (!is_idle_mode) {
          printf("ADC: Volang=%u, Ga=%u, Phanh=%u | TX_CAN: Ga=%u%%, Phanh=%u%%, Volang_CAN=%u\r\n", 
                 adc_values[0], throttle_adc, brake_adc, throttle_percent, brake_percent, steering_can);
      }

      // Kiểm tra Mailbox trống và truyền bản tin
      if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0)
      {
        HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);
      }
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  CAN_RxHeaderTypeDef RxHeader;
  uint8_t RxData[8];

  // Khắc phục lỗi kẹt FIFO Overrun
  __HAL_CAN_CLEAR_FLAG(hcan, CAN_FLAG_FOV0);

  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK)
  {
    return;
  }

  // Nhận bản tin khẩn cấp từ Node ADAS (ID 0x200)
  if (RxHeader.StdId == 0x200)
  {
    if (RxData[0] == 1) // Va chạm xảy ra
    {
       if (eps_emergency_stop == 0) {
           eps_emergency_stop = 1;
           printf("[EMERGENCY] Nhan lenh tu ADAS: Ngat tro luc lai do va cham!\r\n");
           HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1); // Ngắt nguồn servo
       }
    }
    else if (RxData[0] == 0) // An toàn
    {
       if (eps_emergency_stop == 1) {
           eps_emergency_stop = 0;
           racing_start_offset = HAL_GetTick(); // Reset lại mốc thời gian vù ga từ con số 0
           printf("[RECOVERY] Nhan lenh tu ADAS: He thong hoat dong binh thuong!\r\n");
           if (!is_idle_mode) {
             HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
           }
       }
    }
  }
}

int __io_putchar(int ch)
{
  extern UART_HandleTypeDef huart1;
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
