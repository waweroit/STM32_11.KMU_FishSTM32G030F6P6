/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define FLASH_MAGIC_16   ((uint16_t)0x7A4D)      // dowolna != 0x0000/0xFFFF 0x7A3D

// Wirtualne adresy "zmiennych" (16-bit payload każda)
#define VA_MAGIC         		((uint16_t)0x1001)
#define VIRTADDR_KEYCOUNT       ((uint16_t)0x1002)
#define VIRTADDR_LOWLEVEL       ((uint16_t)0x1003)

#undef  NB_OF_VAR
#define NB_OF_VAR 3
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern volatile bool isTransmissionComplete;


volatile uint16_t FlashInitialization = EE_ERROR;


uint16_t VirtAddVarTab[NB_OF_VAR] = {
    VA_MAGIC,
	VIRTADDR_KEYCOUNT,
	VIRTADDR_LOWLEVEL
};


typedef struct {
    uint16_t      magic;              // 16-bit
    int           KeyPressedCount;    // przechowywany jako 16-bit w EEPROM (LOW16)
    GPIO_PinState LowLevelWasReached; // 0/1
} DataFlash;

volatile DataFlash dane;


volatile GPIO_PinState LowLevelWasReachedPrev = GPIO_PIN_RESET; // stan niski (0)
volatile bool DisableACRelay = false;


volatile bool keySwitchPressed = false;
volatile bool waterSensorClosed = false;

bool GoToSleepMode = true;

uint32_t TimeNow = 0;
uint32_t BlinkLed_FromTime;
uint32_t BlinkLedTime = 1000; //1000ms

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

void DF_Init(void);
static void DF_Load(DataFlash *out);
static HAL_StatusTypeDef DF_Save(const DataFlash *in);
static inline uint16_t clamp_u16_from_int(int v);

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
  MX_USART1_UART_Init();
  MX_TIM14_Init();
  /* USER CODE BEGIN 2 */

  DelayInit();

  Send("Welcome waweroIT\r\n");
  HAL_Delay(1000);

  FlashInitialization = EE_Init();
  if(FlashInitialization != EE_OK)
  {
	  Send("Inicjalizacja Flash nie udana...\r\n");
  }
  else
  {
	  DF_Init();
  }


	if(dane.LowLevelWasReached == GPIO_PIN_SET)
	{
	  // Disable DC PUMP
		HAL_GPIO_WritePin(LED_01_GPIO_Port, LED_01_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(ACRelay_GPIO_Port, ACRelay_Pin, GPIO_PIN_SET);

	  Send("AC Relay: Disable \r\n");
	  DisableACRelay = true;
	  GoToSleepMode = false;
	}
	else
	{
		HAL_GPIO_WritePin(LED_01_GPIO_Port, LED_01_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(ACRelay_GPIO_Port, ACRelay_Pin, GPIO_PIN_RESET);

		Send("AC Relay: Enable \r\n");
		DisableACRelay = false;
	}

	Sendf("Odczytane dane.KeyPressedCount: %d\r\n", dane.KeyPressedCount);
	Sendf("Odczytany dane.LowLevelWasReached: %d\r\n", (int)dane.LowLevelWasReached);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */



  while (1)
  {
		if (GoToSleepMode) {
			Send("Going to sleep mode!\r\n");
			__SEV();
			__WFE();  // ustawia/zeruje event
			__WFE();  // dopiero to jest "czyste" WFE do spania
			HAL_SuspendTick();
			//HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
			HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFE);
			HAL_ResumeTick();
		}

		if (HAL_GPIO_ReadPin(UserSwitch_GPIO_Port, UserSwitch_Pin)
				== GPIO_PIN_SET) {
			keySwitchPressed = true;
		}

		if (HAL_GPIO_ReadPin(FloatSensor_GPIO_Port, FloatSensor_Pin)
				== GPIO_PIN_SET) {
			waterSensorClosed = true;
		}
		else
			waterSensorClosed = false;

		TimeNow = HAL_GetTick();

		if(keySwitchPressed || waterSensorClosed)
		{
//		HAL_NVIC_DisableIRQ(EXTI0_1_IRQn);
//
//        __HAL_GPIO_EXTI_CLEAR_IT(UserSwitch_Pin);
//        __HAL_GPIO_EXTI_CLEAR_IT(FloatSensor_Pin);

			if(keySwitchPressed == true && waterSensorClosed == false)
			{
				//HAL_NVIC_DisableIRQ(EXTI0_1_IRQn);

				dane.KeyPressedCount++;
				dane.LowLevelWasReached = GPIO_PIN_RESET; // false
				LowLevelWasReachedPrev = dane.LowLevelWasReached; // false
				Send("Key pressed\r\n");
				DisableACRelay = false;
				Send("FloatSensor Reset !\r\n");
				HAL_GPIO_WritePin(LED_01_GPIO_Port, LED_01_Pin, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(ACRelay_GPIO_Port, ACRelay_Pin, GPIO_PIN_RESET);

				if(EE_WriteVariable(VIRTADDR_KEYCOUNT, (uint16_t)dane.KeyPressedCount) != HAL_OK)
				{
					Send("Blad zapisu KeyPressedCount!\r\n");
				}

				if(EE_WriteVariable(VIRTADDR_LOWLEVEL, (uint16_t)dane.LowLevelWasReached) != HAL_OK)
				{
					Send("Blad zapisu LowLevelWasReached!\r\n");
				}

				keySwitchPressed = false;
				waterSensorClosed = false;
				GoToSleepMode = true;

				HAL_Delay(100);
			}
			else if(keySwitchPressed == false && waterSensorClosed == true)
			{
				//HAL_NVIC_DisableIRQ(EXTI0_1_IRQn);

				Send("FloatSensor Low Level !\r\n");

				if(LowLevelWasReachedPrev != true)
				{
					LowLevelWasReachedPrev = true;
					dane.LowLevelWasReached = true;

					if(LowLevelWasReachedPrev == GPIO_PIN_SET && DisableACRelay == false) // set czyli 1 - niski stan wody
					{
						HAL_GPIO_WritePin(LED_01_GPIO_Port, LED_01_Pin, GPIO_PIN_SET);
						HAL_GPIO_WritePin(ACRelay_GPIO_Port, ACRelay_Pin, GPIO_PIN_SET);
						Send("Stan wody: NISKI \r\n");
						Send("AC Relay: Disable \r\n");
						DisableACRelay = true;

						if(EE_WriteVariable(VIRTADDR_LOWLEVEL, (uint16_t)dane.LowLevelWasReached) != HAL_OK)
						{
							Send("Blad zapisu LowLevelWasReached!\r\n");
						}
					}

					BlinkLed_FromTime = TimeNow;
					GoToSleepMode = false;
				}
			}

//			Sendf("Odczytane dane.KeyPressedCount: %d\r\n", dane.KeyPressedCount);
//			Sendf("Odczytany dane.LowLevelWasReached: %d\r\n", (int)dane.LowLevelWasReached);


	//        __HAL_GPIO_EXTI_CLEAR_IT(UserSwitch_Pin);
	//        __HAL_GPIO_EXTI_CLEAR_IT(FloatSensor_Pin);
	//		HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
		}

		if(((TimeNow - BlinkLed_FromTime) >= BlinkLedTime) && LowLevelWasReachedPrev == true)
		{
			BlinkLed_FromTime = TimeNow;
			HAL_GPIO_TogglePin(LED_01_GPIO_Port, LED_01_Pin);
		}

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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {

    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) // Sprawdź, czy to Twój UART
    {
        isTransmissionComplete = true;
    }
}

//void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
//{
//
//	if(GPIO_Pin == UserSwitch_Pin)
//	{
//		keySwitchPressed = true;
//	}
//	else if(GPIO_Pin == FloatSensor_Pin)
//	{
//		waterSensorClosed = true;
//	}
//}


static inline uint16_t clamp_u16_from_int(int v) {
    if (v < 0) return 0;
    if (v > 0xFFFF) return 0xFFFF;
    return (uint16_t)v;
}

// Zapis aktualnego stanu struktury do EEPROM (3 zmienne 16 bit)
static HAL_StatusTypeDef DF_Save(const DataFlash *in)
{
    HAL_StatusTypeDef st = HAL_OK;
    // 1) MAGIC
    if (EE_WriteVariable(VA_MAGIC, (uint16_t)in->magic) != EE_OK) st = HAL_ERROR; // != 0 to samo co != HAL_OK
    // 2) LICZNIK (LOW16)
//    if (EE_WriteVariable(VIRTADDR_KEYCOUNT, clamp_u16_from_int(in->KeyPressedCount)) != EE_OK) st = HAL_ERROR;
    if (EE_WriteVariable(VIRTADDR_KEYCOUNT, (uint16_t)in->KeyPressedCount) != EE_OK) st = HAL_ERROR;
    // 3) FLAGA
    if (EE_WriteVariable(VIRTADDR_LOWLEVEL, (uint16_t)in->LowLevelWasReached) != EE_OK) st = HAL_ERROR;
    return st;
}

// Odczyt zmiennych (jeśli brak wartości — pozostawia pola bez zmian)
static void DF_Load(DataFlash *out)
{
    uint16_t v;
    if (EE_ReadVariable(VA_MAGIC, &v) == EE_OK) out->magic = v;

    if (EE_ReadVariable(VIRTADDR_KEYCOUNT, &v) == EE_OK)
        out->KeyPressedCount = (int)v; // pamiętaj: tylko LOW16

    if (EE_ReadVariable(VIRTADDR_LOWLEVEL, &v) == EE_OK)
        out->LowLevelWasReached = (GPIO_PinState)v;
}

// ======= Interfejs jak w Twoim przykładzie =======
// Wywołaj na starcie programu
void DF_Init(void)
{
    Send("Odczyt z pamieci flash na starcie...\r\n");

    // Spróbuj odczytać bieżące wartości
    memset((void*)&dane, 0, sizeof(dane));
    DF_Load((DataFlash*)&dane);

    // Sprawdź magic
    if (dane.magic != FLASH_MAGIC_16)
    {
        Send("Blad odczytu na starcie lub brak MAGIC – inicjalizacja domyslna.\r\n");

        // Pierwsze uruchomienie / dane niepoprawne → ustaw domyślne
        dane.magic             = FLASH_MAGIC_16;
        dane.KeyPressedCount   = 0;
        dane.LowLevelWasReached= GPIO_PIN_RESET;

        // Zapis domyślnych wartości
        if (DF_Save((const DataFlash*)&dane) != HAL_OK)
        {
            Send("Blad zapisu!\r\n");
        }
        else
        {
            Send("Zapis domyslnych wartosci do flash ok.\r\n");
        }
    }
    else
    {
        Send("Odczyt na starcie ok.\r\n");
        Send("Magic data ok.\r\n");
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
