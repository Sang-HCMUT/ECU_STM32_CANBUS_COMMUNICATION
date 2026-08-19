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
#include "can.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <math.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MPU6050_ADDR 0xD0
#define SMPLRT_DIV_REG 0x19
#define ACCEL_CONFIG_REG 0x1C
#define ACCEL_XOUT_H_REG 0x3B
#define PWR_MGMT_1_REG 0x6B
#define WHO_AM_I_REG 0x75
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
int16_t Accel_X_RAW = 0;
int16_t Accel_Y_RAW = 0;
int16_t Accel_Z_RAW = 0;
float Ax, Ay, Az;
uint8_t is_crashed = 0;

CAN_TxHeaderTypeDef TxHeader;
uint32_t TxMailbox;
uint8_t TxData[8];

// Du lieu tu cac Node khac tren mang CAN (phai co volatile vi duoc cap nhat trong ngat)
volatile uint8_t ext_throttle = 0;
volatile uint8_t ext_brake = 0;
volatile uint32_t ext_rpm = 0;
volatile uint8_t ext_steering = 127;
volatile uint8_t ext_abs_active = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void MPU6050_Init(void);
void MPU6050_Read_Accel(void);
void CAN_Filter_Config(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void MPU6050_Init(void)
{
  uint8_t check;
  uint8_t Data;

  // Check device ID
  HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, WHO_AM_I_REG, 1, &check, 1, 1000);
  
  // Wake up the sensor
  Data = 0x00;
  HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, PWR_MGMT_1_REG, 1, &Data, 1, 1000);
  
  // Set DATA RATE of 1KHz by writing SMPLRT_DIV register
  Data = 0x07;
  HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, SMPLRT_DIV_REG, 1, &Data, 1, 1000);

  // Set accelerometer configuration in ACCEL_CONFIG Register
  // FS_SEL=1 -> +/- 4g (8192 LSB/g)
  Data = 0x08; 
  HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, ACCEL_CONFIG_REG, 1, &Data, 1, 10);
}

void MPU6050_Read_Accel(void)
{
  uint8_t Rec_Data[6] = {0}; // Khởi tạo bằng 0 để tránh rác RAM nếu I2C lỗi
  
  // Read 6 BYTES of data starting from ACCEL_XOUT_H register
  // Giảm timeout từ 1000ms xuống 10ms để tránh treo toàn bộ board ADAS nếu lỏng dây I2C
  if (HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, ACCEL_XOUT_H_REG, 1, Rec_Data, 6, 10) == HAL_OK)
  {
    Accel_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    Accel_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    Accel_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);
    
    // Convert to g (using +/- 4g scale -> divide by 8192.0)
    Ax = Accel_X_RAW / 8192.0f;
    Ay = Accel_Y_RAW / 8192.0f;
    Az = Accel_Z_RAW / 8192.0f;
  }
  // Nếu lỗi (lỏng dây), giữ nguyên giá trị Ax, Ay, Az cũ, hệ thống vẫn chạy tiếp không bị treo!
}

void CAN_Filter_Config(void)
{
  CAN_FilterTypeDef canfilterconfig = {0};
  
  canfilterconfig.FilterActivation = CAN_FILTER_ENABLE;
  canfilterconfig.FilterBank = 0;
  canfilterconfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  canfilterconfig.FilterIdHigh = 0x0000;
  canfilterconfig.FilterIdLow = 0x0000;
  canfilterconfig.FilterMaskIdHigh = 0x0000;
  canfilterconfig.FilterMaskIdLow = 0x0000;
  canfilterconfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canfilterconfig.FilterScale = CAN_FILTERSCALE_32BIT;
  canfilterconfig.SlaveStartFilterBank = 14;
  
  HAL_CAN_ConfigFilter(&hcan, &canfilterconfig);
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
  MX_CAN_Init();
  MX_I2C2_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  CAN_Filter_Config();
  while (HAL_CAN_Start(&hcan) != HAL_OK)
  {
      HAL_Delay(10);
  }
  HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);

  TxHeader.StdId = 0x200;
  TxHeader.ExtId = 0x01;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 1;
  TxHeader.TransmitGlobalTime = DISABLE;
  
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  TIM2->CCR1 = 65535; // Cấm còi kêu (Tránh lỗi tràn 16-bit của số 65536)
  
  MPU6050_Init();
  
  // Phát bản tin An Toàn ngay khi khởi động để mở khóa EPS và ABS nếu chúng bị kẹt ở lần Crash trước
  uint8_t SafeData[1] = {0};
  uint32_t SafeMailbox;
  if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0) {
      HAL_CAN_AddTxMessage(&hcan, &TxHeader, SafeData, &SafeMailbox);
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    MPU6050_Read_Accel();
    
    // Calculate total acceleration vector
    float accel_vector = sqrt(Ax*Ax + Ay*Ay + Az*Az);
    
    static uint32_t last_log_time = 0;
    if (HAL_GetTick() - last_log_time >= 100)
    {
      last_log_time = HAL_GetTick();
      
      // Đọc trực tiếp thanh ghi lỗi của CAN để chẩn đoán phần cứng
      uint8_t can_tec = (hcan.Instance->ESR >> 16) & 0xFF;
      uint8_t can_rec = (hcan.Instance->ESR >> 24) & 0xFF;
      uint8_t can_state = HAL_CAN_GetState(&hcan);

      // IN RA JSON CHO DASHBOARD (Thêm TEC và REC để debug)
      printf("{\"Ax\":%.2f,\"Ay\":%.2f,\"Az\":%.2f,\"Ga\":%u,\"Phanh\":%u,\"RPM\":%lu,\"Crash\":%u,\"Steer\":%u,\"ABS\":%u,\"TEC\":%u,\"REC\":%u,\"ST\":%u}\r\n", 
             Ax, Ay, Az, ext_throttle, ext_brake, ext_rpm, is_crashed, ext_steering, ext_abs_active, can_tec, can_rec, can_state);
    }
    
    static uint32_t crash_time = 0;
    static uint8_t is_logged = 0;

    // Threshold > 5.0g for strong collision / rollover (Tăng ngưỡng lên 5.0 để chống nhiễu MPU6050)
    if (accel_vector > 5.0f && is_crashed == 0)
    {
      is_crashed = 1;
      crash_time = HAL_GetTick();
      is_logged = 0; // Reset log cờ
    }
    
    if (is_crashed == 1)
    {
      // Sau khi tai nạn, hệ thống bị khoá chết vĩnh viễn (chỉ có thể phục hồi bằng nút Reset cứng trên mạch)
      // Còi kêu trong 5 giây đầu tiên, sau đó tắt cho đỡ ồn
      if (HAL_GetTick() - crash_time > 5000)
      {
        TIM2->CCR1 = 65535; // Tắt còi
      }
      else
      {
        TIM2->CCR1 = 0; // Bật còi
        if (is_logged == 0) {
          printf("[BUZZER] TAI NAN! He thong da bi khoa cung!\r\n");
          is_logged = 1;
        }
      }
      
      // LUÔN LUÔN phát liên tục bản tin KHẨN CẤP (1) mỗi 100ms để đảm bảo các ECU khác bị khoá
      TxData[0] = 1;
      static uint32_t last_emergency_tx = 0;
      if (HAL_GetTick() - last_emergency_tx >= 100)
      {
        last_emergency_tx = HAL_GetTick();
        if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0)
        {
          HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);
        }
      }
    }
    else
    {
      // TRẠNG THÁI AN TOÀN (BÌNH THƯỜNG)
      // Luôn luôn phát bản tin An Toàn (0) mỗi 500ms để đảm bảo các ECU luôn được mở khóa.
      // Việc phát liên tục này KHÔNG làm reset EPS racing cycle vì EPS chỉ xử lý khi eps_emergency_stop == 1.
      TxData[0] = 0;
      static uint32_t last_safe_tx = 0;
      if (HAL_GetTick() - last_safe_tx >= 500)
      {
        last_safe_tx = HAL_GetTick();
        if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0)
        {
          HAL_CAN_AddTxMessage(&hcan, &TxHeader, TxData, &TxMailbox);
        }
      }
    }
    
    HAL_Delay(15); // 15ms cycle loop
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
}

/* USER CODE BEGIN 4 */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

int _write(int file, char *ptr, int len)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
  return len;
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  CAN_RxHeaderTypeDef RxHeader;
  uint8_t RxData[8];

  // Khắc phục lỗi kẹt FIFO: Nếu bị Overrun (FOVR0), phần cứng sẽ từ chối nhận thêm data mãi mãi.
  // Xóa cờ Overrun thủ công ở đây giúp hệ thống luôn tự phục hồi nếu bị tràn do nhiễu hoặc CPU bận.
  __HAL_CAN_CLEAR_FLAG(hcan, CAN_FLAG_FOV0);

  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) != HAL_OK)
  {
    return;
  }

  // Nhan du lieu tu Node EPS (ID 0x100)
  if (RxHeader.StdId == 0x100)
  {
    ext_throttle = RxData[0];
    ext_brake = RxData[1];
    if (RxHeader.DLC >= 3) {
      ext_steering = RxData[2];
    }
  }
  // Nhan du lieu tu Node ABS (ID 0x102)
  else if (RxHeader.StdId == 0x102)
  {
    ext_rpm = (RxData[0] << 8) | RxData[1];
    if (RxHeader.DLC >= 3) {
      ext_abs_active = RxData[2];
    }
  }
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
