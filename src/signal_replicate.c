#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_clkpwr.h"

#define NUM_SINE_SAMPLE 	((uint32_t)60)
#define SINE_FREQ_IN_HZ 	((uint32_t)50)
#define ADC_FREQ 			((uint32_t)200000)
#define MATCH_VALUE_EN_US 	((uint32_t)166)
#define DMA_SIZE 			NUM_SINE_SAMPLE

static uint16_t 			bufferA[NUM_SINE_SAMPLE];
static uint16_t 			bufferB[NUM_SINE_SAMPLE];

static GPDMA_LLI_T 			adc2ALLI = {0};
static GPDMA_LLI_T 			adc2BLLI = {0};
static GPDMA_LLI_T 			A2dacLLI = {0};
static GPDMA_LLI_T 			B2dacLLI = {0};
volatile uint32_t 			controlBufferB = 0; // cuando llegue a 60 se habilita el canal de DMA->DAC

void cfgADC(void);
void cfgTimer(void);
void cfgDMA(void);
void cfgDAC(void);
void TIMER0_IRQHandler(void);

int main(void) {

	adc2ALLI.srcAddr 		= (uint32_t)&LPC_ADC->ADGDR;
	adc2ALLI.dstAddr 		= (uint32_t)&bufferA;
	adc2ALLI.nextLLI 		= (uint32_t)&adc2BLLI;
	adc2ALLI.control 		= 	DMA_SIZE
							| 	1 << 18
							|	1 << 21
							| 	1 << 27;

	adc2BLLI.srcAddr 		= (uint32_t)&LPC_ADC->ADGDR;
	adc2BLLI.dstAddr 		= (uint32_t)&bufferB;
	adc2BLLI.nextLLI 		= (uint32_t)&adc2ALLI;
	adc2BLLI.control 		= 	DMA_SIZE
							| 	1 << 18
							|	1 << 21
							| 	1 << 27;

	A2dacLLI.srcAddr 		= (uint32_t)&bufferA;
	A2dacLLI.dstAddr 		= (uint32_t)&LPC_DAC->DACR;
	A2dacLLI.nextLLI 		= (uint32_t)&B2dacLLI;
	A2dacLLI.control 		= 	DMA_SIZE
							|	1 << 18
							| 	1 << 21
							| 	1 << 26;

	B2dacLLI.srcAddr 		= (uint32_t)&bufferB;
	B2dacLLI.dstAddr 		= (uint32_t)&LPC_DAC->DACR;
	B2dacLLI.nextLLI 		= (uint32_t)&A2dacLLI;
	B2dacLLI.control 		= 	DMA_SIZE
							| 	1 << 18
							| 	1 << 21
							|   1 << 26;

	cfgDMA();
	cfgTimer();
	cfgADC();
	cfgDAC();
	GPDMA_ChannelStart(GPDMA_CH_0);

    while(1) {
    	__WFI();
    }
    return 0 ;
}

void TIMER0_IRQHandler(void) {
	if (controlBufferB == NUM_SINE_SAMPLE*2) {
		GPDMA_ChannelStart(GPDMA_CH_1);
		NVIC_DisableIRQ(TIMER0_IRQn);
	} else {
		controlBufferB++;
	}

	TIM_ClearIntPending(LPC_TIM0, TIM_MR1_INT);
}

void cfgDAC(void) {
	DAC_Init();
	DAC_CONVERTER_CFG_T dacCfg 				= {ENABLE,ENABLE,ENABLE};
	DAC_ConfigDAConverterControl(&dacCfg);
	uint32_t dac_clk 						= CLKPWR_GetPCLK(CLKPWR_PCLKSEL_DAC);
	uint32_t timeOut 						= dac_clk/(NUM_SINE_SAMPLE*SINE_FREQ_IN_HZ);
	DAC_SetDMATimeOut((uint16_t)timeOut);
}

void cfgDMA(void) {
	GPDMA_Init();
	GPDMA_Channel_CFG_T dmaCfgADC = {0}, dmaCfgDAC = {0};

	dmaCfgADC.channelNum 		= GPDMA_CH_0;
	dmaCfgADC.transferSize 		= DMA_SIZE;
	dmaCfgADC.type 				= GPDMA_P2M;
	dmaCfgADC.srcMemAddr 		= (uint32_t)&LPC_ADC->ADCR;
	dmaCfgADC.dstMemAddr 		= (uint32_t)&bufferA;
	dmaCfgADC.srcConn 			= GPDMA_ADC;
	dmaCfgADC.dstConn 			= 0;
	dmaCfgADC.src.burst 		= GPDMA_BSIZE_1;
	dmaCfgADC.src.increment 	= DISABLE;
	dmaCfgADC.src.width 		= GPDMA_HALFWORD;
	dmaCfgADC.dst.burst 		= GPDMA_BSIZE_1;
	dmaCfgADC.dst.increment 	= ENABLE;
	dmaCfgADC.dst.width 		= GPDMA_HALFWORD;
	dmaCfgADC.linkedList 		= (uint32_t)&adc2BLLI;

	dmaCfgDAC.channelNum 		= GPDMA_CH_1;
	dmaCfgDAC.transferSize 		= DMA_SIZE;
	dmaCfgDAC.type 				= GPDMA_M2P;
	dmaCfgDAC.srcMemAddr 		= (uint32_t)&bufferA;
	dmaCfgDAC.dstMemAddr 		= (uint32_t)&LPC_DAC->DACR;
	dmaCfgDAC.srcConn 			= 0;
	dmaCfgDAC.dstConn 			= GPDMA_DAC;
	dmaCfgDAC.src.burst 		= GPDMA_BSIZE_1;
	dmaCfgDAC.src.increment 	= ENABLE;
	dmaCfgDAC.src.width 		= GPDMA_HALFWORD;
	dmaCfgDAC.dst.burst 		= GPDMA_BSIZE_1;
	dmaCfgDAC.dst.increment 	= DISABLE;
	dmaCfgDAC.dst.width 		= GPDMA_HALFWORD;
	dmaCfgDAC.linkedList 		= (uint32_t)&B2dacLLI;

	GPDMA_SetupChannel(&dmaCfgADC);
	GPDMA_SetupChannel(&dmaCfgDAC);
}

void cfgTimer(void) {
	TIM_TIMERCFG_T timerCfg 			= {TIM_US,1};
	TIM_InitTimer(LPC_TIM0, &timerCfg);
	TIM_MATCHCFG_T matchCfg 			= {0};
	matchCfg.channel 					= TIM_MATCH_1;
	matchCfg.intEn 						= ENABLE;
	matchCfg.stopEn 					= DISABLE;
	matchCfg.resetEn 					= ENABLE;
	matchCfg.extOpt 					= TIM_TOGGLE;
	matchCfg.matchValue 				= MATCH_VALUE_EN_US;
	TIM_ConfigMatch(LPC_TIM0, &matchCfg);
	TIM_Enable(LPC_TIM0);

	NVIC_EnableIRQ(TIMER0_IRQn);
}

void cfgADC(void) {
	ADC_Init(ADC_FREQ);
	ADC_EdgeStartConfig(ADC_START_ON_RISING);
	ADC_PinConfig(ADC_CHANNEL_0);
	ADC_ChannelEnable(ADC_CHANNEL_0);
	ADC_StartCmd(ADC_START_ON_MAT01);
}

