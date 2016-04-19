#include "msp430g2553.h"

/*综合实例      串口输入命令做不同的事
    '0'(0x30):关机，进入LPM4睡眠模式；内部定时器（TA和看门狗）不工作，可以发送UART，只有通过外部中断（P2.2，下降沿中断）才能恢复正常
    '1':睡眠，进入LPM2，与上面一样，待修改
    '2':测温度，第10通道
    '3':测光线，需要外部感光电阻，第0通道p1.0
    '4':测其它，需要传感器（ADC10），第3通道p1.3
    'A'(0x41):是否闪灯，p2.0和p2.1
    'B':切换蜂鸣器，看门狗中断P2.5输出
    'C'：切换继电器p2.4
*/

#define MAX_NUM 3          //  says长度
char TX_num = 0;
char says[] = {'x','x','\n'};
char op = 0,op_state = 0;//最低0位为设定小时或分钟数标志；1位为beep是否打开

void key_init()
{
    P2DIR &= ~0x04;         //  p2.2设置为输入；用于外部中断退出LMP4，当做开机
    P2REN |= 0x04;          //  要上下拉电阻
    P2SEL &= ~0x04;         //  普通IO功能
    P2SEL2 &= ~0x04;
    P2OUT |= 0x04;          //  上拉电阻
    P2IE |= 0x04;           //  打开中断
    P2IES |= 0x04;          //  下降沿触发
    
    P1IFG &= ~0X04;         //  清除中断标志
}

#define LED0_SW (P2OUT ^= 0x01)
#define LED1_SW (P2OUT ^= 0x02)

void led_init()
{
    P2DIR |= 0x03;          //  p2.0和p2.1设置为输出
    P2REN &= ~0x03;         //  不要上下拉电阻
    P2SEL &= ~0x03;         //  普通IO功能
    P2SEL2 &= ~0x03;
    P2IE &= ~0x03;          //  关闭IO的中断
    P2OUT |= 0x03;          //  p2.0、2.1输出1
}

void uart_init()
{
    //  uart配置
    P1DIR &= ~0x02;         //  p1.1为RXD    //  一定要有方向
    P1DIR |= 0x04;          //  p1.2为TXD
    P1SEL |= 0x06;           //  设置p1.1为RXD；p1.2为TXD
    P1SEL2 |= 0x06;          //  设置p1.1为RXD；p1.2为TXD
    
    BCSCTL1 = 0x0007u;//CAL_BC1_1MHZ;  //  调整DCO到1MHz
    DCOCTL = 0x0006u;//CAL_DCO_1MHZ;
    
    UCA0CTL1 |= UCSWRST;    //  不允许UART后开始配置
    UCA0CTL0 &= ~UC7BIT;    //  数据位为8位
    UCA0CTL0 |= UCPEN + UCPAR;//  打开校验 + 偶校验
    UCA0CTL1 |= UCSSEL_2 + UCRXEIE;//  选择uart时钟为SMCLK + 错误中断使能
    UCA0BR0 = 97;           //  设置波特率为9600:1MHz/9600 = 104;但是发现不成功，故调整为97就成功了
    UCA0BR1 = 0;            //  波特率调整查看MSP430G2系列单片机原理与实践教程完整版p116
    UCA0MCTL = UCBRS_1;     //  微调波特率
    UCA0CTL1 &= ~UCSWRST;   //  允许UART，UART进入工作状态
    IE2 |= UCA0RXIE;        //  使能接收中断
}

void adc10_init()
{
    //  adc配置
    ADC10CTL1 |= CONSEQ_0;              //  单通道单采样模式，默认，可以不设置
    ADC10CTL1 |= ADC10SSEL_3 + SHS_0;   //  时钟选择为SMCLK + 触发源为ADC10SC
    ADC10CTL1 |= INCH_10;                //  输入通道为A10，内部温度传感器
    ADC10CTL0 |= ADC10SHT_3 + ADC10ON + ADC10IE;    //  采样保持时间为64个时钟 + 开启ADC10 + 使能中断
    // ADC10AE0 |= 0x04;       //  A2通道模拟输入使能，不要也可以？？
}

void timer_init()
{
    CCTL0 = CCIE;
    CCR0 = 65312;        //  默认时钟为1.045MHz再加上下面的8分频，65312 = (1045000/8)/2，所以最终定时0.5s
    TACTL = TASSEL_2 + ID_3 + MC_1;//  配置时钟源为（SMCLK） + 8分频 + up模式,然后定时器开始工作
}

#define BEEP_SW (P2OUT ^= 0x20)

//  用看门狗定时器输出beep
void beep_init()
{
    P2DIR |= 0x20;          //  p2.5设置为输出
    P2REN &= ~0x20;         //  不要上下拉电阻
    P2SEL &= ~0x20;         //  普通IO功能
    P2SEL2 &= ~0x20;
    P2IE &= ~0x20;          //  关闭IO的中断
    P2OUT |= 0x20;          //  p2.5输出1
}

void beep_on()
{
    op_state |= 0x02;
    WDTCTL = WDTPW+WDTTMSEL+WDTCNTCL + WDTIS1;//  即设置为[密码，普通定时器模式，看门狗计数器清0] + 512周期
    IE1 |= WDTIE;                             //  打开看门狗定时器中断，即打开beep
}

void beep_off()
{
    op_state &= ~0x02;
    WDTCTL = WDTPW+WDTHOLD; //  关闭看门狗
    IE1 &= ~WDTIE;          //  关闭看门狗定时器中断
}

#define RELAY_ON (P2OUT |= 0x10)
#define RELAY_OFF (P2OUT &= ~0x10)
void relay_init() //  初始化继电器管脚
{
    P2DIR |= 0x10;          //  p2.4设置为输出
    P2REN &= ~0x10;         //  不要上下拉电阻
    P2SEL &= ~0x10;         //  普通IO功能
    P2SEL2 &= ~0x10;
    P2IE &= ~0x10;          //  关闭IO的中断
    P2OUT &= ~0x10;          //  p2.4输出0
}
void relay_on()
{
    P2OUT |= 0x10;
    op_state |= 0x04;//  第2位表示继电器是否打开
}
void relay_off()
{
    P2OUT &= ~0x10;
    op_state &= ~0x04;//  第2位表示继电器是否打开
}

void main(void) {
    WDTCTL = WDTPW+WDTHOLD; //  关闭看门狗
    
    key_init();
    led_init();
    uart_init();
    adc10_init();
    timer_init();
    beep_init();
    relay_init();
    
    while(1)
    {
        _BIS_SR(LPM1_bits + GIE);   //  进入低功耗模式1（可以为0,1；当进入2、3、4时温度无法测），并开启全局中断使能
        switch(op)
        {
            case '0':    //  进入休眠状态只有外部中断才能唤醒
                says[1] = 1;
                TX_num = 1;
                UCA0TXBUF = says[TX_num];
                IE2 |= UCA0TXIE;            //  使能发送中断
                while(TX_num != 0);         //  等待消息发送完
                _BIS_SR(LPM4_bits + GIE);
                break;
            case '1':    //  睡眠，只有时钟工作，关闭ADC
                says[1] = 1;
                TX_num = 1;
                UCA0TXBUF = says[TX_num];
                IE2 |= UCA0TXIE;            //  使能发送中断
                while(TX_num != 0);         //  等待消息发送完
                _BIS_SR(LPM2_bits + GIE);
                break;
            case '2':
                if(!(ADC10CTL1 & INCH_10))  //  如果不是通道10就切换为通道10
                {
                    ADC10CTL0 &= ~ENC;      //  ENC为0时才能修改
                    ADC10CTL1 = INCH_10;    //  只选取通道10
                }
                ADC10CTL0 |= ENC + ADC10SC; //  转换使能 + 触发启动ADC10采样转换
                break;
            case '3':
                if(!(ADC10CTL1 & INCH_0))  //  如果不是通道0就切换为通道0
                {
                    ADC10CTL0 &= ~ENC;      //  ENC为0时才能修改
                    ADC10CTL1 = INCH_0;    //  只选取通道0
                }
                ADC10CTL0 |= ENC + ADC10SC; //  转换使能 + 触发启动ADC10采样转换
                break;
            case '4':
                if(!(ADC10CTL1 & INCH_3))  //  如果不是通道3就切换为通道3
                {
                    ADC10CTL0 &= ~ENC;      //  ENC为0时才能修改
                    ADC10CTL1 = INCH_3;    //  只选取通道3
                }
                ADC10CTL0 |= ENC + ADC10SC; //  转换使能 + 触发启动ADC10采样转换
                break;
            case 'A':
                if(!(op_state & 0x01))//第0位表示是否闪led
                {
                    op_state |= 0x01;
                    says[1] = 1;
                }
                else// if(op_state & 0x01)
                {
                    op_state &= ~0x01;
                    says[1] = 0;
                }
                TX_num = 1;
                UCA0TXBUF = says[TX_num];
                IE2 |= UCA0TXIE;            //  使能发送中断
                break;
            case 'B':
                if(!(op_state & 0x02))//第1位表示是否beep响
                {
                    beep_on();
                    says[1] = 1;
                }
                else// if(op_state & 0x02)
                {
                    beep_off();
                    says[1] = 0;
                }
                TX_num = 1;
                UCA0TXBUF = says[TX_num];
                IE2 |= UCA0TXIE;            //  使能发送中断
                break;
            case 'C':
                if(!(op_state & 0x04))//第2位表示是否打开继电器
                {
                    relay_on();
                    says[1] = 1;
                }
                else// if(op_state & 0x04)
                {
                    relay_off();
                    says[1] = 0;
                }
                TX_num = 1;
                UCA0TXBUF = says[TX_num];
                IE2 |= UCA0TXIE;            //  使能发送中断
                break;
            default:
                says[1] = 1;
                TX_num = 1;
                UCA0TXBUF = says[TX_num];
                IE2 |= UCA0TXIE;            //  使能发送中断
                break;
        }
        
    }
}

#pragma vector = ADC10_VECTOR
__interrupt void ADC10()        //  ADC10中断
{
    short temp;
    // temp = (ADC10MEM - 746)/(0.000355 * 678) + 286;
    // temp = (ADC10MEM - 317)*4 + 130;
    temp = ADC10MEM;
    TX_num = 0;
    says[0] = temp;
    says[1] = temp>>8;
    UCA0TXBUF = says[TX_num];
    IE2 |= UCA0TXIE;            //  使能发送中断
}


#pragma vector = USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR()  //  接收中断
{
    op = UCA0RXBUF;         //  读取清0中断标志，否则会一直触发中断
    __bic_SR_register_on_exit(LPM1_bits);
}

#pragma vector = USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR()  //  发送中断
{
    if(TX_num < MAX_NUM - 1)
    {
        UCA0TXBUF = says[++TX_num];
    }
    else
    {
        TX_num = 0;
        IE2 &= ~UCA0TXIE;            //  关闭发送中断
    }
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void T0_A0(void)
{
    if(op_state & 0x01)
    {
        LED0_SW;      //  LED0输出取反
    }
    
}

#pragma vector = PORT2_VECTOR
__interrupt void P2(void)
{
    if((P2IFG & 0x04) == 0x04)//  判断为P2.2的IO中断才操作
    {
        P2IFG &= ~0X04;     //  清除中断标志
        __bic_SR_register_on_exit(LPM4_bits);
    }
}

//  看门狗作为普通定时器的中断
#pragma vector = WDT_VECTOR
__interrupt void wdt_timer()
{
    BEEP_SW;
}
