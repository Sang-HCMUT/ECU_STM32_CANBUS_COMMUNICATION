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
volatile uint32_t pulse_count_left = 0;
volatile uint32_t pulse_count_right = 0;
volatile uint32_t current_rpm_left = 0;
volatile uint32_t current_rpm_right = 0;
volatile uint8_t flag_emergency = 0;
volatile uint8_t throttle_percent = 0;
volatile uint8_t brake_percent = 0;
volatile uint8_t abs_active = 0;
uint32_t last_log_tick = 0;

CAN_RxHeaderTypeDef rxHeader;
uint8_t rxData[8];

CAN_TxHeaderTypeDef txHeader;
uint32_t txMailbox;
uint8_t txData[8];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);

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

  HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
  HAL_CAN_Start(&hcan);
  
  // Cau hinh ban tin TX gui RPM & Status
  txHeader.StdId = 0x102;
  txHeader.ExtId = 0x01;
  txHeader.RTR = CAN_RTR_DATA;
  txHeader.IDE = CAN_ID_STD;
  txHeader.DLC = 3; // 2 byte RPM, 1 byte ABS status
  txHeader.TransmitGlobalTime = DISABLE;
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_tick = HAL_GetTick();
  while (1)
  {
    if (HAL_GetTick() - last_log_tick >= 500) {
        printf("RPM_L: %lu | RPM_R: %lu | Throttle: %u | Brake: %u | ABS: %u | Emergency: %u\r\n", 
               current_rpm_left, current_rpm_right, throttle_percent, brake_percent, abs_active, flag_emergency);
        last_log_tick = HAL_GetTick();
    }

    if (HAL_GetTick() - last_tick >= 50) {
        // Chu kỳ lấy mẫu 50ms (1/20 giây)
        current_rpm_left = pulse_count_left * 60; 
        current_rpm_right = pulse_count_right * 60;
        pulse_count_left = 0;
        pulse_count_right = 0;
        last_tick = HAL_GetTick();
        
        // Phát RPM trung bình (hoặc tuỳ chỉnh) lên CAN cho ADAS/Dashboard hiển thị
        uint32_t current_rpm_avg = (current_rpm_left + current_rpm_right) / 2;
        txData[0] = (current_rpm_avg >> 8) & 0xFF; // High byte
        txData[1] = current_rpm_avg & 0xFF;        // Low byte
        txData[2] = abs_active;
        HAL_CAN_AddTxMessage(&hcan, &txHeader, txData, &txMailbox);
    }

    if (flag_emergency == 1) {
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 0); // Động cơ Trái (A-1A) -> PB0
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0); // Động cơ Trái (A-1B) -> PB1
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0); // Động cơ Phải (B-1A) -> PA6
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0); // Động cơ Phải (B-1B) -> PA7
        abs_active = 0;
    } 
    else if (brake_percent > 30) {
        // ABS Độc lập: Nếu bánh nào tốc độ thấp (bó cứng) thì nhấp nhả bánh đó
        // ABS Độc lập: Nếu bánh nào tốc độ thấp (bó cứng) thì nhấp nhả bánh đó
        uint32_t pwm_brake_left = (30 * 65535) / 100;  // Lực hãm 30%
        uint32_t pwm_brake_right = (30 * 65535) / 100; 
        
        // Cycle 50ms nhấp nhả
        if (HAL_GetTick() % 100 < 50) {
            if (current_rpm_left < 100) pwm_brake_left = 0;   // Nhả phanh trái
            if (current_rpm_right < 100) pwm_brake_right = 0; // Nhả phanh phải
        }

        // Bánh trái (A) ở TIM3_CH3, CH4
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pwm_brake_left); 
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0);
        // Bánh phải (B) ở TIM3_CH1, CH2
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pwm_brake_right); 
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
        abs_active = 1;
    } 
    else {
        uint32_t target_duty = (throttle_percent * 65535) / 100;
        if (brake_percent > 0) target_duty = 0; // Normal braking
        
        // Thuật toán Soft-Start (Lọc thông thấp) để chống giật cục / sụt áp khi thay đổi ga đột ngột
        static float current_duty = 0;
        current_duty += ((float)target_duty - current_duty) * 0.02f; // Tăng dần 2% mỗi vòng lặp
        
        uint32_t duty = (uint32_t)current_duty;
        if (duty > 65535) duty = 65535;
        
        // Bánh trái (A)
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, duty); 
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0);
        // Bánh phải (B)
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty); 
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
        abs_active = 0;
    }
    
    HAL_Delay(10); // 10ms cycle loop to ensure Soft-Start algorithm works correctly
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
  /* With GCC/RAISONANCE, small printf (option LD Linker->Libraries->Small printf
     set to 'Yes') calls __io_putchar() */
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

int _write(int file, char *ptr, int len)
{
  int DataIdx;
  for (DataIdx = 0; DataIdx < len; DataIdx++)
  {
    __io_putchar(*ptr++);
  }
  return len;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_4) {
        pulse_count_left++;
    }
    if (GPIO_Pin == GPIO_PIN_5) {
        pulse_count_right++;
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef rxHeader;
    uint8_t rxData[8];

    // Khắc phục lỗi kẹt FIFO Overrun
    __HAL_CAN_CLEAR_FLAG(hcan, CAN_FLAG_FOV0);

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK) {
        if (rxHeader.StdId == 0x200) {
            if (rxData[0] == 1) { // Lệnh khẩn cấp từ ADAS
                if (flag_emergency == 0) {
                    flag_emergency = 1;
                    printf("[EMERGENCY] Nhan lenh tu ADAS: Khoa cung banh xe do va cham!\r\n");
                }
            } 
            else if (rxData[0] == 0) { // Lệnh phục hồi từ ADAS
                if (flag_emergency == 1) {
                    flag_emergency = 0;
                    printf("[RECOVERY] Nhan lenh tu ADAS: Mo khoa banh xe, hoat dong binh thuong!\r\n");
                }
            }
        }
        else if (rxHeader.StdId == 0x100) {
            throttle_percent = rxData[0];
            brake_percent = rxData[1];
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
