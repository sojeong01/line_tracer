/*
 * linetracer_1.c
 *
 * Created: 2022-05-20 오후 8:09:07
 * Author : sojeonglee
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>

int Get_ADC(unsigned char ADC_num);
void Uart_Init();
void Uart_Trans(unsigned char data);
void Num_Trans(int numdata);
int Normal_AD(int AD, int AD_Max, int AD_Min);

volatile unsigned int cnt = 0;
volatile int mode = 2;
volatile int AD[8] = {0,};
volatile int AD_Max[8] = {0,};
volatile int AD_Min[8] = {1023,};
volatile int AD_Weight[8] = {-8,-4,-2,-1,1,2,4,8}; //가중치
volatile int AD_BW[8]; //검정&하양
volatile int line = 0;
volatile int post = 0;
volatile int reverse = 0;



ISR(INT0_vect) {
	mode = 0;
} //MODE0 = MAX, MIN

ISR(INT1_vect) {
	mode = 1;
} //MODE1 = RIDE

ISR(TIMER0_OVF_vect) {
	cnt++; //제어주기마다 cnt 증가
	TCNT0 = 99; //10ms 제어주기
	
	for(int i=0; i<8; i++){
		AD[i] = Get_ADC(i);
	} //ADC값 받기
	
	if(mode == 0){
		for(int i=0; i<8; i++){
			if(AD[i] > AD_Max[i])
			AD_Max[i] = AD[i];
		}
		
		for(int i=0; i<8; i++){
			if(AD[i] < AD_Min[i])
			AD_Min[i] = AD[i];
		}
	} //최대 최소 받아오기
	
	if(mode == 1){
		line = 0;
		for(int i=0; i<8; i++){
			AD[i] = Normal_AD(AD[i], AD_Max[i], AD_Min[i]);
		} //정규화
		
		if (reverse==1)
		{
			for (int i=0; i<8; i++)
			{
				if (AD_BW[i]==1) AD_BW[i]=0;
				else AD_BW[i]=1;
			} //검은 배경에 하얀 선
		}
		else
		{
			for(int i=0; i<8; i++){
				if(AD[i] < 55) {PORTA&=~(1<<i); AD_BW[i]=1; line++;} //검정 1
				else {PORTA|=(1<<i); AD_BW[i] = 0;} //하양 0
			} //흰 배경에 검은 선
			
			if(AD_BW[0]==1 && AD_BW[7]==1 && (AD_BW[2]==0||AD_BW[3]==0||AD_BW[4]==0||AD_BW[5]==0)) 
				cnt++;
			else cnt = 0;

			if(cnt >= 50) reverse = 1;
		}
		
		int Weight=0; //가중치
		for(int i=0; i<8; i++){
			Weight += AD_Weight[i]*AD_BW[i]*10;
		}
		
		if(line>0){
			PORTE = 0b00001010;
			post = Weight;
			OCR1A = 590 - Weight;
			OCR1B = 590 + Weight;
		}
		else if(post<0){
			PORTE = 0b00000110;
			//OCR1A = 590 - post;
			OCR1A = 500;
			OCR1B = 600;
		}
		else{
			PORTE = 0b00001001;
			OCR1A = 600;
			//OCR1B = 590 + post;
			OCR1B = 500;
		}
		//오버플로우 방지
	} //정규화, 주행
	cnt = 0;
}

int main()
{
	DDRA = 0xff;
	PORTA = 0xff;
	DDRF = 0x00;
	DDRD = 0b00001000;
	ADMUX = 0b01000000; //AVCC, ADC0
	ADCSRA = 0b10000111; //ADC 활성화, 128분주
	
	Uart_Init();

	DDRB = 0xff;
	DDRE = 0x0f;
	PORTE = 0x0A; // 0b 0000 1010
	TCCR1A = (1<<COM1A1)|(0<<COM1A0)|(1<<COM1B1)|(0<<COM1B0)|(1<<WGM11);
	//A,B Channel Clear(Compare Match) & Set(Overflow), 14Mode : Fast PWN
	TCCR1B = (1<<WGM13)|(1<<WGM12)|(0<<CS02)|(0<<CS01)|(1<<CS00);
	//14 Mode : Fast PWN, 분주비 1
	ICR1 = 799;
	OCR1A = 0; //오른쪽
	OCR1B = 0; //왼쪽
	
	TCCR0 = (0<<WGM01)|(0<<WGM00)|(0<<COM01)|(0<<COM00)|(1<<CS02)|(1<<CS01)|(1<<CS00);
	TIMSK = (1<<TOIE0); //0번 overflow Interrupt Enable
	TCNT0 = 99;
	
	EICRA = (1<<ISC01)|(1<<ISC11);
	EIMSK = (1<<INT1)|(1<<INT0);
	
	sei(); //전역 Interrupt Enable
	
	while (1);
}

int Get_ADC(unsigned char ADC_num)
{
	ADMUX =(ADC_num|0b01000000);
	ADCSRA |= (1<<ADSC);
	while(!(ADCSRA&(1<<ADIF)));
	return ADC;
}

void Uart_Init()
{
	UCSR1A = 0x00;
	UCSR1B = (1<<RXEN)|(1<<TXEN);
	UCSR1C = (1<<UCSZ11)|(1<<UCSZ10);
	UBRR1H = 0;
	UBRR1L = 103;
}

void Uart_Trans(unsigned char data)
{
	while (!(UCSR1A &(1<<UDRE1)));
	UDR1 = data;
}

void Num_Trans(int numdata)
{
	int data;
	
	if(numdata < 0)
	{
		Uart_Trans('-');
		numdata = -1*numdata;
	}
	data=numdata/1000;
	Uart_Trans(data+48);
	data=(numdata%1000)/100;
	Uart_Trans(data+48);
	data=(numdata%100)/10;
	Uart_Trans(data+48);
	data=numdata%10;
	Uart_Trans(data+48);
}

int Normal_AD(int AD, int AD_Max, int AD_min)
{
	double res;
	res = ((double)(AD-AD_min)/(AD_Max-AD_min))*100;
	res = (int)res;
	return res;
}
