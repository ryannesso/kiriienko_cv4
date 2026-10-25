/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"
#include <stdio.h>

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);


/* Function processing DMA Rx data. Counts how many capital and small letters are in sentence.
 * Result is supposed to be stored in global variable of type "letter_count_" that is defined in "main.h"
 *
 * @param1 - received sign
 */
void proccesDmaData(uint8_t sign);


static uint8_t frame_active = 0U;
static uint8_t frame_text[DMA_USART2_BUFFER_SIZE];
static uint8_t frame_length = 0U;
static uint8_t frame_characters_after_start = 0U;
static uint8_t lowercase_count = 0U;
static volatile uint8_t frame_ready = 0U;
volatile uint32_t system_millis = 0U;


int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();

  USART2_RegisterCallback(proccesDmaData);
	char tx_message[160];
	uint32_t last_buffer_report = 0U;

  while (1)
  {
	  /* Periodic transmission of information about DMA Rx buffer state.
	   * Transmission frequency - 0.5Hz.
	   * Message format - "Buffer capacity: %d bytes, occupied memory: %d bytes, load [in %]: %f%"
	   * Example message (what I wish to see in terminal) - Buffer capacity: 1000 bytes, occupied memory: 231 bytes, load [in %]: 23.1%
	   */

	  /* Valid text string information transmission.
	   * Transmission frequency - when new valid string is received.
	   * Message format - "Valid string: %s, lower-case: %d, upper-case: %d"
	   * Example message (what I wish to see in terminal) - Valid string: Platn15uborZnakov, lower-case: 13, upper-case: 2
	   */
	  if ((frame_ready != 0U) &&
		  !LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_7))
	  {
		  int length;
		  frame_ready = 0U;
		  length = snprintf(tx_message, sizeof(tx_message),
				  "Valid string: %s, lower-case: %u\r\n",
				  (char *)frame_text, (unsigned)lowercase_count);
		  if (length > 0)
		  {
			  USART2_PutBuffer((uint8_t *)tx_message, (uint8_t)length);
		  }
	  }
	  else if (((system_millis - last_buffer_report) >= 2000U) &&
		  !LL_DMA_IsEnabledChannel(DMA1, LL_DMA_CHANNEL_7))
	  {
		  uint16_t used_bytes = USART2_GetRxBufferUsed();
		  uint16_t load_tenths = (uint16_t)((used_bytes * 1000U) /
			  DMA_USART2_BUFFER_SIZE);
		  int length = snprintf(tx_message, sizeof(tx_message),
			  "Buffer capacity: %u bytes, occupied memory: %u bytes, load [in %%]: %u.%u%%\\r\\n",
			  (unsigned)DMA_USART2_BUFFER_SIZE, (unsigned)used_bytes,
			  (unsigned)(load_tenths / 10U), (unsigned)(load_tenths % 10U));
		  if (length > 0)
		  {
			  USART2_PutBuffer((uint8_t *)tx_message, (uint8_t)length);
		  }
		  last_buffer_report = system_millis;
	  }

  }
  /* USER CODE END 3 */
}


void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);

  if(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
  {
  Error_Handler();  
  }
  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {
    
  }
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {
  
  }
  LL_Init1msTick(8000000);
  LL_SYSTICK_SetClkSource(LL_SYSTICK_CLKSOURCE_HCLK);
  LL_SetSystemCoreClock(8000000);
}

/*
 * Implementation of function processing data received via USART.
 */
void proccesDmaData(uint8_t sign)
{
	if (frame_active == 0U)
	{
		if (sign == '#')
		{
			frame_active = 1U;
			frame_length = 0U;
			frame_characters_after_start = 0U;
			lowercase_count = 0U;
		}
		return;
	}

	if (sign == '#')
	{
		/* A new start marker starts a new frame. */
		frame_length = 0U;
		frame_characters_after_start = 0U;
		lowercase_count = 0U;
		return;
	}

	frame_characters_after_start++;
	if (sign == '$')
	{
		if ((frame_length != 0U) && (frame_characters_after_start <= 35U))
		{
			frame_text[frame_length] = '\0';
			frame_ready = 1U;
		}
		frame_active = 0U;
		return;
	}

	/* '$' must arrive within the first 35 characters after '#'. */
	if (frame_characters_after_start >= 35U)
	{
		frame_active = 0U;
		return;
	}

	if (frame_length >= (DMA_USART2_BUFFER_SIZE - 1U))
	{
		frame_active = 0U;
		return;
	}

	frame_text[frame_length++] = sign;
	if ((sign >= 'a') && (sign <= 'z'))
	{
		lowercase_count++;
	}
}


void Error_Handler(void)
{

}

#ifdef  USE_FULL_ASSERT

void assert_failed(char *file, uint32_t line)
{ 

}

#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
