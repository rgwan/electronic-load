#ifndef MEASURE_H
#define MEASURE_H
void ADC_Polling(void);
void Measure_Calculate(void);
uint16_t getVoltage(void);
uint16_t getReg(void);
extern uint8_t ADC_CH[4];
#endif

