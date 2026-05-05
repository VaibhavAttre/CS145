#include <stdint.h>

/* RCC */
#define RCC_AHB2ENR ((volatile uint32_t*) 0x44020C8C)
#define RCC_APB1LENR ((volatile uint32_t*) 0x44020C9C) //to enable TIM2 clock

/* GPIOA: Button */
#define GPIOA_MODER ((volatile uint32_t*) 0x42020000)
#define GPIOA_PUPDR ((volatile uint32_t*) 0x4202000C)
#define GPIOA_IDR ((volatile uint32_t*) 0x42020010)

/* GPIOC: LED */
#define GPIOC_MODER ((volatile uint32_t*) 0x42020800)
#define GPIOC_BSRR ((volatile uint32_t*) 0x42020818)

/* NVIC */
#define NVIC_ISER1 ((volatile uint32_t*) 0xE000E104)
#define TIM2_NVIC_BIT 13U //TIM2 -> BIT 45. Since in NVIC_ISER1 (32 - 63), 45 - 32 = 13th bit

/* TIMER */
#define TIM2_CR1 ((volatile uint32_t*) 0x40000000) //main control: start/stop, direction, preload
#define TIM2_DIER ((volatile uint32_t*) 0x4000000C) //enables timer interrupts
#define TIM2_SR ((volatile uint32_t*) 0x40000010) //status flagS
#define TIM2_EGR ((volatile uint32_t*) 0x40000014) //force timer events
#define TIM2_PSC ((volatile uint32_t*) 0x40000028) //prescaler
#define TIM2_ARR ((volatile uint32_t*) 0x4000002C) //auto reload value

#define BUTTON_PIN 6U

#define BLINK_LED_PIN 0U
#define BUTTON_LED_PIN 1U

/* RCC_AHB2ENR  */
#define GPIOA_EN (1U << 0)
#define GPIOC_EN (1U << 2)
#define TIM2_EN (1U << 0)


//HELPERS TO SET STUFF

#define TIM_CR1_CEN (1U << 0)   // Bit 0: counter enable
//#define TIM_CR1_UDIS (1U << 1)   // Bit 1: update disable (0 = updates allowed)
//#define TIM_CR1_DIR (1U << 4)   // Bit 4: direction (0=up, 1=down)
//#define TIM_CR1_ARPE (1U << 7)   // Bit 7: auto-reload preload enable

#define TIM_DIER_UIE (1U << 0)   // Update interrupt enable
#define TIM_SR_UIF (1U << 0)   // Bit 0: update interrupt flag (slide p.1671)
#define TIM_EGR_UG (1U << 0)   // Bit 0: update generation


#define BLINK_LED_SET (1U  << BLINK_LED_PIN)
#define BLINK_LED_RESET (1U << (BLINK_LED_PIN + 16U))

#define BUTTON_LED_SET (1U << BUTTON_LED_PIN)
#define BUTTON_LED_RESET (1U << (BUTTON_LED_PIN + 16U))

#define GPIO_MODE_INPUT 0x0U
#define GPIO_MODE_OUTPUT 0x1U

#define GPIO_PULL_NONE 0x0U
#define GPIO_PULL_UP 0x1U
#define GPIO_PULL_DOWN 0x2U

void TIM2_IRQHandler(void) {

	if((*TIM2_SR & TIM_SR_UIF) != 0){

		*TIM2_SR &= ~TIM_SR_UIF;

		//Led functions
	}
}


void timer_init(void) {

	//INIT CODE FOR TIMER
	TIM2_CR1 &= ~TIM2_CR1_CEN;

	TIM2_PSC = 31999;
	TIM2_ARR = 999;

	TIM2_DIER |= TIM_DIER_UIE;
	TIM2_EGR |= TIM_EGR_UG;

	TIM2_SR &= ~TIM_SR_UIF;

	TIM2_CR1 |= TIM2_CR1_CEN;
}

void clock_init(void) {

	*RCC_AHB2ENR |= GPIOA_EN | GPIOC_EN;
	*RCC_APB1LENR |= TIM2_EN;
}

void nvic_init(void) {

	*NVIC_ISER1 = (1U << TIM2_NVIC_BIT);
}

void led_init(void) {

    /* Clear mode bits for PC0 and PC1 */
    *GPIOC_MODER &= ~((0x3U << (BLINK_LED_PIN * 2U)) |
                      (0x3U << (BUTTON_LED_PIN * 2U)));

    /* Set PC0 and PC1 to output mode */
    *GPIOC_MODER |=  ((GPIO_MODE_OUTPUT << (BLINK_LED_PIN * 2U)) |
                      (GPIO_MODE_OUTPUT << (BUTTON_LED_PIN * 2U)));

	/* Start both LEDs off */
	*GPIOC_BSRR = BLINK_LED_RESET;
	*GPIOC_BSRR = BUTTON_LED_RESET;
}

void button_init(void) {

	//INIT CODE FOR TIMER


}

int main(void) {

	/* Clock Enables */


	/*
	 * NEED SOMETHING LIKE THIS:
	 * Tim2 counts up -> reaches ARR -> update event -> UIF Flag set -> interrupt requested
		TIM2_DIER.UIE = 1     // timer is allowed to request interrupt
		NVIC_ISER1 bit = 1   // CPU is allowed to accept TIM2 interrupt
	 */

	clock_init();

	led_init();
	button_init();
	keypad_init();

	timer2_init();
}

