#include "timers.h"
#include "defs.h"

void timerSetup_buttonTimer(void)
{
    TIM_TimeBaseInitTypeDef		TIM_TimeBaseStructure;
	
    // Time base configuration
    TIM_TimeBaseStructure.TIM_Prescaler         = 35999;            // prescale 72 MHz by 36 kHz = 2 kHz
    TIM_TimeBaseStructure.TIM_Period            = 0xffff;           // no real period, just go to full potential
    TIM_TimeBaseStructure.TIM_ClockDivision     = 0;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    TIM_ARRPreloadConfig(TIM1, ENABLE);

    TIM_Cmd(TIM1, ENABLE);                                          // enable timer
	TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);                      // enable int from this timer
}

void timerSetup_sendFw(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	
    // Time base configuration
    TIM_TimeBaseStructure.TIM_Prescaler         = 35999;			// prescale 72 MHz by 36 kHz = 2 kHz
    TIM_TimeBaseStructure.TIM_Period            = 2000;				// with prescaler set to 2kHz, this period will be 1 second
    TIM_TimeBaseStructure.TIM_ClockDivision     = 0;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;

    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ARRPreloadConfig(TIM2, ENABLE);

    TIM_Cmd(TIM2, ENABLE);                                          // enable timer
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);                      // enable int from this timer
}

void timerSetup_cmdTimeout(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    // Time base configuration
    TIM_TimeBaseStructure.TIM_Prescaler         = 65535;            // prescale 72 MHz by 65 kHz = 1098 Hz
    TIM_TimeBaseStructure.TIM_Period            = CMD_TIMEOUT_SHORT;
    TIM_TimeBaseStructure.TIM_ClockDivision     = 0;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter	= 0;

    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
    TIM_ARRPreloadConfig(TIM3, DISABLE);                            // disable preloading

    TIM_Cmd(TIM3, ENABLE);                                          // enable timer
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);                      // enable int from this timer
}

void timerSetup_cmdTimeoutChangeLength(uint16_t newPeriod)
{
    TIM_Cmd(TIM3, DISABLE);                     // disable timer

    TIM3->CNT = 0;                              // timer back to zero
    TIM3->SR  = 0xfffe;                         // clear UIF flag
    TIM3->ARR = newPeriod;                      // set new timeout value

    TIM_Cmd(TIM3, ENABLE);                      // enable timer
}

