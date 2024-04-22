/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Auto-generated by STM32CubeIDE
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2023 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */

#if !defined(__SOFT_FP__) && defined(__ARM_FP)
#warning "FPU is not initialized, but the project is compiling for an FPU. Please initialize the FPU before use."
#endif

#include "Stm32_F103x6.h"
#include "Stm32_F103C6_gpio_driver.h"
#include "Stm32_F103C6_EXTI_driver.h"
#include "Stm32_F103C6_USART_driver.h"
#include "lcd_driver.h"
#include "keypad_driver.h"
#include "Stm32_F103C6_SPI_driver.h"
#include "Stm32_F103C6_I2C_driver.h"
#include "I2C_Slave_EEPROM.h"
#include "TIM.h"
#include "core_cm3.h"

uint32_t TASKA_flag, TASKB_flag, IRQ_flag = 0;

#define TaskA_Stack_Size 100
#define TaskB_Stack_Size 100

extern int _estack ;

unsigned int _S_MSP = &_estack ;
unsigned int _E_MSP ;

//Process Stack Task A
unsigned int _S_PSP_TA ;
unsigned int _E_PSP_TA ;

//Process Stack Task B
unsigned int _S_PSP_TB ;
unsigned int _E_PSP_TB ;

#define OS_SET_PSP(address) __asm volatile ("mov r0, %0 \n\t msr PSP, r0"  : : "r" (address))
#define OS_SWITCH_SP_to_PSP __asm volatile ("mrs r0, CONTROL \n\t mov r1, #0x02 \n\t orr r0,r0,r1 \n\t msr CONTROL, r0 ")  //set bit 1 in CONTROL register to one
#define OS_SWITCH_SP_to_MSP __asm volatile ("mrs r0, CONTROL \n\t mov r1, #0x05 \n\t and r0,r0,r1 \n\t msr CONTROL, r0 ")  //set bit 1 in CONTROL register to zero
#define SWITCH_CPU_AccessLevel_privileged 	  __asm volatile("mrs  r3, CONTROL  \n\t lsr   r3,r3,#0x1   \n\t lsl   r3,r3,#0x1    \n\t msr  CONTROL, r3")
#define SWITCH_CPU_AccessLevel_unprivileged   __asm volatile("mrs  r3, CONTROL  \n\t orr   r3,r3,#0x1   \n\t msr  CONTROL, r3")

#define OS_Generate_Exception __asm volatile ("SVC #0x3")

void clock_init()
{
	//Enable port A clock
	RCC_GPIOA_CLK_EN();
	//Enable port B clock
	RCC_GPIOB_CLK_EN();
	//Enable AFIO clock
	RCC_AFIO_CLK_EN();
}

void IRQ_EXTI9_callback(void){
	if (IRQ_flag == 0)
	{
		TASKA_flag = 1;
		IRQ_flag =1;
	}else if(IRQ_flag == 1){
		TASKB_flag = 1;
		IRQ_flag = 0;
	}
}

int TASKA(int a, int b, int c)
{
	return a+b+c;
}

int TASKB(int a,int b, int c)
{
	return a+b+c;
}



void PendSV_Handler()
{
	SCB->ICSR |= SCB_ICSR_PENDSVCLR_Msk;
}



void OS_SVC_services(int *StackFramePointer)
{
	unsigned char SVC_number;
	unsigned char VAL1,VAL2;
	SVC_number = *( (unsigned char*) ( (unsigned char*)StackFramePointer[6] )-2 ) ;
	VAL1 = StackFramePointer[0];
	VAL2 = StackFramePointer[1];

	switch(SVC_number)
		{
		case 1:
			StackFramePointer[0] = VAL1 + VAL2;
			break;
		case 2:
			StackFramePointer[0] = VAL1 - VAL2;
			break;
		case 3:
			SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
			break;
		}
}
__attribute ((naked))void SVC_Handler()
{
	__asm("tst lr, #4 \n\t"
		  "ITE EQ \n\t"
		  "mrseq r0,MSP \n\t"
		  "mrsne r0,PSP \n\t"
		  "B OS_SVC_services");
}

int OS_SVC_Set(int a, int b, int SVC_ID)
{
	int result;
	switch(SVC_ID)
	{
	case 1:
		__asm("svc 0x01");
		break;
	case 2:
		__asm("svc 0x02");
		break;
	case 3:
		__asm("svc 0x03");
		break;
	}

	__asm("mov %0, r0"
		  :"=r" (result));
	return result;

}

/*
enum CPU_AccessLevel{
	privileged,
	unprivileged
};

void SWITCH_CPU_AccessLevel(enum CPU_AccessLevel Level)
{
	switch(Level)
	{
	case privileged:
		//clear bit 0 in CONTROL register
		__asm("mrs  r3, CONTROL     \n\t"
				"lsr   r3,r3,#0x1    \n\t"
				"lsl   r3,r3,#0x1    \n\t"
				"msr  CONTROL, r3");
		break;

	case unprivileged:
		__asm("mrs  r3, CONTROL    \n\t"
				"orr   r3,r3,#0x1   \n\t"
				"msr  CONTROL, r3");
		break;
	}
}
*/

void MainOs()
{
	// 1/2 KiloByte for MSP
	_E_MSP = _S_MSP - 512;

	// 1 Byte for TASKA
	_S_PSP_TA = _E_MSP - 8;
	_E_PSP_TA = _S_PSP_TA - TaskA_Stack_Size;

	// 1 Byte for TASKB
	_S_PSP_TB = _E_PSP_TA - 8;
	_E_PSP_TB = _S_PSP_TB - TaskB_Stack_Size;

	while(1)
	{
		__asm("NOP");
		if(TASKA_flag == 1){
			//set PSP register to _S_PSP_TA
			OS_SET_PSP(_S_PSP_TA);
			//set SP -> PSP
			OS_SWITCH_SP_to_PSP ;                //set SP before set access level to unprivileged (before lose access)
			//set access level to unprivileged
			SWITCH_CPU_AccessLevel_unprivileged;
			TASKA_flag = TASKA(1,2,3);
			//set access level to privileged
			OS_Generate_Exception;				//set access level to privileged before setting SP -> MSP to have access first
			//set SP -> MSP
			OS_SWITCH_SP_to_MSP;

		}else if (TASKB_flag == 1) {
			//set PSP register to _S_PSP_TB
			OS_SET_PSP(_S_PSP_TB);
			//set SP -> PSP
			//set access level to unprivileged
			SWITCH_CPU_AccessLevel_unprivileged;
			TASKB_flag = TASKB(1,2,3);
			//set access level to privileged
			OS_Generate_Exception;
			//set SP -> MSP
			OS_SWITCH_SP_to_MSP;
		}
	}

}

int main(void)
{
//	clock_init();
//	EXTI_PinConfig_t EXTI_cfg;
//	EXTI_cfg.EXTI_PIN = EXTI9PB9;
//	EXTI_cfg.Trigger_Case = EXTI_Trigger_RISING;
//	EXTI_cfg.P_IRQ_CALLBACK = IRQ_EXTI9_callback;
//	EXTI_cfg.IRQ_EN = EXTI_IRQ_Enable;
//	MCAL_EXTI_GPIO_Init(&EXTI_cfg);
//
//
//	MainOs();

	int result;
//  result = OS_SVC_Set(3,2,1);

//	result = OS_SVC_Set(3,2,2);
//	result = OS_SVC_Set(3,2,3);

	OS_SVC_Set(0,0,3);

	while(1){
		if(IRQ_flag){
			IRQ_flag = 0;
		}
	}
}
