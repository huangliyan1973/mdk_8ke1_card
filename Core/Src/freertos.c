/** All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ds26518.h"
#include "zl50020.h"
#include "sram.h"
#include "eeprom.h"
#include "sched.h"
#include "mtp.h"
#include "server_interface.h"

#define LOG_TAG              "thread"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

#define BERT_TEST

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
void print_task(void)
{
    char buf[512] = {0};
    
    LOG_I("freeHeapsize = %d, miniHeapsize=%d",xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());

    vTaskList(buf);
    printf("Name\t\tState\tPri\tStack\tNum\n");
    printf("\n%s\n",buf);
}

extern void ds26518_monitor_test(int e1_no, int slot);
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityBelowNormal6,
  .stack_size = 256 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN StartDefaultTask */
  snmp_8ke1_init();
  server_interface_init();
  //shell_init();
  sched_timeout_init();

  mtp_init();

#ifdef BERT_TEST
  //enable_prbs_function(5);
  //ds26518_enable_bert(2,1);
  //HAL_Delay(500);
  //ds26518_bert_report(2);
  //send_msg(0,1,0x75);
  //zl50020_clkout_test();
  //set_ds26518_loopback(1, FRAME_LOCAL_LP);
  
  //ds26518_monitor_test(0, 2);
  //connect_slot(1,0,1,0);
  //connect_slot(2,0,2,0);
  //connect_slot(1,8, 1, 0);
  //print_zl50020();
  //test_prbs();
#endif
  for (;;)
  {
    //LOG_W("LIU 0 status = %x",check_liu_status(0));
    //print_zl50020(0,0);
    //print_task();
#ifdef BERT_TEST
    //print_prbs_value(5, 0);
    //ds26518_bert_report(2);
#endif
    osDelay(10000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
