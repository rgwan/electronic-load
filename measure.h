#ifndef MEASURE_H
#define MEASURE_H
void ADC_Polling(void);
void Measure_Calculate(void);
uint16_t getVoltage(void);
uint16_t getReg(void);
uint16_t getCurrent(void);
void setCurrent(uint16_t i);
extern uint8_t ADC_CH[4];
extern uint16_t Measure_Current;
extern uint8_t OT_Protected;
#endif

