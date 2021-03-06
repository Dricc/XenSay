#include "XenSay.h"

void    init_timer1(void)
{
    T1CON = 0;              // Reset le registre du timer 1
    T1CONbits.TCKPS = 2;    // Prescale de la clock du timer 1 a 1:64
    IPC1bits.T1IP = 5;      // Set la priorite de l'interruption du timer 1 a 6
    IFS0bits.T1IF = 0;      // Reset du flag d'interrupt du timer 1
    TMR1 = 0;               // Reset le timer 1
    PR1 = 156;            // osc (8 MHz) / PBDIV(8) -> 1MHz ->  / timer prescale(64) -> 15625 -> 1s
    IEC0bits.T1IE = 1;      // Active l'interruption du timer 1
    T1CONbits.ON = 1;
}

// Load des switch + envoie de l'etat des LEDs
void __attribute__ ((interrupt(IPL5AUTO))) __attribute__ ((vector(4))) timer1(void)
{
    if (SPI1STATbits.SPITBE)
    {
//        g_led = g_switch;   //Debug switch <-> LED
         pulse_load();
         SPI1BUF = g_led;
    }
     TMR1 = 0;
     IFS0bits.T1IF = 0;
}