/*
 * adc_driver.c
 *
 *  Created on: May 25, 2026
 *      Author: Aaron
 */

#include "adc_driver.h"
#include <stdint.h>
#include <stdio.h>

#define RCC_AHB2ENR *((volatile uint32_t *)(0x44020C8CUL))

#define GPIOA_MODER *((volatile uint32_t *)(0x42020000UL))
#define GPIOA_PUPDR *((volatile uint32_t *)(0x4202000CUL))

#define ADC1_BASE 0x42028000UL

#define ADC1_ISR (*((volatile uint32_t *)(ADC1_BASE)))
#define ADC1_CR  (*((volatile uint32_t *)(ADC1_BASE + 0x08)))
#define ADC1_CFGR  (*((volatile uint32_t *)(ADC1_BASE + 0x0C))) //not really needed
#define ADC1_SMPR2 (*((volatile uint32_t *)(ADC1_BASE + 0x18)))
#define ADC1_SQR1 (*((volatile uint32_t *)(ADC1_BASE + 0x30)))
#define ADC1_DR (*((volatile uint32_t *)(ADC1_BASE + 0x40)))

#define ADC_ISR_ADRDY    (1U << 0)
#define ADC_ISR_EOC      (1U << 2)
#define ADC_ISR_LDORDY   (1U << 12)

#define ADC_CR_ADEN      (1U << 0)
#define ADC_CR_ADSTART   (1U << 2)
#define ADC_CR_ADVREGEN  (1U << 28)
#define ADC_CR_DEEPPWD   (1U << 29)
#define ADC_CR_ADCALDIF  (1U << 16)
#define ADC_CR_ADCAL     (1U << 31)

//xx
/* RCC register addresses (RCC_BASE = 0x44020C00) */
#define RCC_CCIPR5  (*((volatile uint32_t *)(0x44020C00UL + 0xE8)))
//xx

static void short_delay(volatile uint32_t n)
{
	while (n--) {__asm("nop"); }
}

void ADC_Init(void)
{

	/* 2) Route HSI to the ADC/DAC kernel clock (ADCDACSEL in CCIPR5).
	 *    Field is bits [2:0]. Value for HSI = 0b011 = 3. VERIFY in your
	 *    reference manual RM0481, RCC_CCIPR5 ADCDACSEL field. */
	RCC_CCIPR5 &= ~(0x7U << 0);
	RCC_CCIPR5 |=  (0x4U << 0);
	//xx

	RCC_AHB2ENR |= (1U << 0) | (1U << 10); //GPIOA EN & ADC12EN

	GPIOA_MODER &= ~(3U << (3 * 2));
	GPIOA_MODER |= (3U << (3 * 2));
	GPIOA_PUPDR &= ~(3U << (3*2));

	if (ADC1_CR & ADC_CR_DEEPPWD) {
		ADC1_CR &= ~ADC_CR_DEEPPWD;
	}

	ADC1_CR |= ADC_CR_ADVREGEN;
	short_delay(1000);
	//while (!(ADC1_ISR & ADC_ISR_LDORDY)) {}
	ADC1_CR &= ~ADC_CR_ADEN;
	ADC1_CR &= ~ADC_CR_ADCALDIF;
	ADC1_CR |= ADC_CR_ADCAL;
	while (ADC1_CR & ADC_CR_ADCAL) {}
	ADC1_ISR |= ADC_ISR_ADRDY;
	ADC1_CR |= ADC_CR_ADEN;
	while (!(ADC1_ISR & ADC_ISR_ADRDY)) {}
	ADC1_SMPR2 |= (7U << (3 * (15 -10)));

	ADC1_SQR1 &= ~(0xFU << 0);
	ADC1_SQR1 &= ~(0x1FU << 6);
	ADC1_SQR1 |= (15U << 6);
	//OR ADC1_SQR1 |= (3U << 6);

}

uint32_t ADC_Read(void)
{

	ADC1_ISR |= ADC_ISR_EOC;

	ADC1_CR |= ADC_CR_ADSTART;

	while(!(ADC1_ISR & ADC_ISR_EOC)) {}

	return ADC1_DR;
}
