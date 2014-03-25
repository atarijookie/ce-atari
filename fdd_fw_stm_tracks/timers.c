#include "timers.h"

void timerSetup_index(void)
{
    TIM_TimeBaseInitTypeDef     TIM_TimeBaseStructure;
    TIM_OCInitTypeDef           TIM_OCInitStructure;
    
    uint16_t PrescalerValue = 0;
    uint16_t period         = 2000 / 5;                                 // this is length of 1 period in clock ticks when timer is prescaled to 2000 Hz
    
    // Compute the prescaler value
    PrescalerValue = (uint16_t) (SystemCoreClock / 2000) - 1;           // prescale to 2000 Hz - 1 tick is 0.5 ms
    
    // Time base configuration
    TIM_TimeBaseStructure.TIM_Period            = period - 1;           // count of tick per period for 5 RPMs - 400 ticks (-1)
    TIM_TimeBaseStructure.TIM_Prescaler         = PrescalerValue;
    TIM_TimeBaseStructure.TIM_ClockDivision     = 0;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    // PWM1 Mode configuration: Channel2
    TIM_OCInitStructure.TIM_OCMode          = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState     = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse           = 10;                       // pulse will be 10 ticks == 5 ms long
    TIM_OCInitStructure.TIM_OCPolarity      = TIM_OCPolarity_Low;

    TIM_OC2Init(TIM2, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);
    
    TIM_ARRPreloadConfig(TIM2, ENABLE);

    TIM_Cmd(TIM2, ENABLE);                                              // enable timer
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);                          // enable int from this timer
}

void timerSetup_mfmRead(void)
{
    TIM_TimeBaseInitTypeDef     TIM_TimeBaseStructure;
    TIM_OCInitTypeDef           TIM_OCInitStructure;
    
    uint16_t PrescalerValue     = 0;
    uint16_t period             = 8;                                    // 4us - length of 1 period in clock ticks when timer is prescaled to 2 MHz
    
    // Compute the prescaler value
    PrescalerValue = (uint16_t) (SystemCoreClock / 2000000) - 1;        // prescale to 2 MHz - 1 tick is 0.5 us
    
    // Time base configuration
    TIM_TimeBaseStructure.TIM_Period            = period - 1;           // count of tick per period for MFM stream
    TIM_TimeBaseStructure.TIM_Prescaler         = PrescalerValue;
    TIM_TimeBaseStructure.TIM_ClockDivision     = 0;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);
    
    // PWM1 Mode configuration: Channel1
    TIM_OCInitStructure.TIM_OCMode          = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState     = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse           = 1;                        // pulse will be 1 tick == 0.5 us long
    TIM_OCInitStructure.TIM_OCPolarity      = TIM_OCPolarity_Low;

    TIM_OC1Init(TIM1, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
    
    TIM_ARRPreloadConfig(TIM1, DISABLE);                                // disable preloading

    TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);                          // enable int from this timer

    // set TIM1 DMA control register (TIMx_DCR) as: DBL (<<8) =0 (1 transfer), DBA (<<0) =11 (0x2C TIMx_ARR)
    TIM1->DCR = 11;

    // reference manual on page 407 says only UDE
    TIM_ITConfig(TIM1, (1 <<  8), ENABLE);                              // enable Bit  8 - UDE: Update DMA request enable

    TIM1->BDTR = 0x8000;                                                // set MOE bit (Main Output Enable)
    TIM_Cmd(TIM1, ENABLE);                                              // enable timer
}

void timerSetup_mfmWrite(void)                  
{
    // TIM3_CH1 can be used with DMA1_CH6
    TIM_TimeBaseInitTypeDef     TIM_TimeBaseStructure;
    TIM_ICInitTypeDef           TIM_CH1_ICInitStructure;

    // Time base configuration
    TIM_TimeBaseStructure.TIM_Period            = 0xff;                     // never let it go to 0xffff
    TIM_TimeBaseStructure.TIM_Prescaler         = 9;                        // prescale: 10, this means 7.2 MHz
    TIM_TimeBaseStructure.TIM_ClockDivision     = 0;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
    TIM_ARRPreloadConfig(TIM3, DISABLE);                                    // disable preloading

    // setup input capture
    TIM_CH1_ICInitStructure.TIM_Channel         = TIM_Channel_1;
    TIM_CH1_ICInitStructure.TIM_ICPolarity      = TIM_ICPolarity_Falling;
    TIM_CH1_ICInitStructure.TIM_ICSelection     = TIM_ICSelection_DirectTI;
    TIM_CH1_ICInitStructure.TIM_ICPrescaler     = TIM_ICPSC_DIV1;
    TIM_CH1_ICInitStructure.TIM_ICFilter        = 0;
    TIM_ICInit(TIM3, &TIM_CH1_ICInitStructure);
    
    // set TIM1 DMA control register (TIMx_DCR) as: DBL (<<8) =0 (1 transfer), DBA (<<0) = 13   (0x34 TIMx_CCR1)
    TIM3->DCR = 13;
    TIM_ITConfig(TIM3, (1 <<  9), ENABLE);                                  // enable Bit 9 - CC1DE: Capture/Compare 1 DMA request enable (CC1 DMA request enabled)
    
    TIM_Cmd(TIM3, ENABLE);                                                  // enable timer
}

void timerSetup_stepLimiter(void)                   
{
    // TIM4
    TIM_TimeBaseInitTypeDef     TIM_TimeBaseStructure;

    // Time base configuration
    TIM_TimeBaseStructure.TIM_Period            = 0xffff;                   
    TIM_TimeBaseStructure.TIM_Prescaler         = 35999;                    // prescale 72 MHz by 36 kHz = 2 kHz
    TIM_TimeBaseStructure.TIM_ClockDivision     = 0;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);
    TIM_ARRPreloadConfig(TIM4, DISABLE);                                    // disable preloading

    TIM_Cmd(TIM4, ENABLE);                                                  // enable timer
}

/*
void timerSetup_cmdTimeout(void)                    
{
    TIM_TimeBaseInitTypeDef     TIM_TimeBaseStructure;

  // Time base configuration
  TIM_TimeBaseStructure.TIM_Prescaler                   = 35999;            // prescale 72 MHz by 36 kHz = 2 kHz
  TIM_TimeBaseStructure.TIM_Period                      = 2000;             // with prescaler set to 2kHz, this period will be 1 second
  TIM_TimeBaseStructure.TIM_ClockDivision           = 0;
  TIM_TimeBaseStructure.TIM_CounterMode             = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter =   0;
    
  TIM_TimeBaseInit(TIM5, &TIM_TimeBaseStructure);
  TIM_ARRPreloadConfig(TIM5, DISABLE);                          // disable preloading

  TIM_Cmd(TIM5, ENABLE);                                                        // enable timer
    TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE);              // enable int from this timer
}
*/

WORD start;

void timeoutStart(void)
{
/*  
    // init the timer 4, which will serve for timeout measuring 
  TIM_Cmd(TIM5, DISABLE);                                               // disable timer
    TIM5->CNT   = 0;                                                                // set timer value to 0
    TIM5->SR    = 0xfffe;                                                       // clear UIF flag
  TIM_Cmd(TIM5, ENABLE);                                                // enable timer
*/
    start = TIM4->CNT;
}   


BYTE timeout(void)
{
    /*
    if((TIM5->SR & 0x0001) != 0) {      // overflow of TIM4 occured?
        TIM5->SR = 0xfffe;                          // clear UIF flag
        return TRUE;
    }
    */

    WORD diff, now;
    
    now = TIM4->CNT;
    diff = now - start;
    
    if(diff > 2000) {
        return TRUE;
    }   
    
    return FALSE;
}
