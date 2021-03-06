#include <xc.h>
#include "sdcard.h"
#include "XenSay.h"

static u8 flags;

static u8 SPI(const u8 send)
{
    /*u8 retry;
    u8 timeout;
    
    for (retry = 0; retry < 5; ++retry)
    {
        timeout = 0;
        */
    //IEC1bits.SPI2TXIE = 1;
    SPI2BUF = send;
    //IEC1bits.SPI2RXIE = 1;
    //while (/*!(flags & FLAGS_TX) || */!(flags & FLAGS_RX));
    //flags &= ~FLAGS_RX;
    while (/*timeout < 40 && */(!SPI2STATbits.SPITBE || !SPI2STATbits.SPIRBF));
        /*if (timeout < 40)
            return (SPI2BUF);
    }*/
    
    return (SPI2BUF);
}

static u8 sd_wait_rep()
{
    u16 i = 0;
    u8 data = 0xff;

    do
    {
        data = SPI(0xff); // 0xff == les donnees a envoyer a SPI en mode WAIT (quand on n'a pas les donnes a envoyer)
        ++i;
    } while (i <= 20 && data == 0xff); // i <= 20 == TIMEOUT
    return (data);
}

static u8 sd_send_cmd(const u8 cmd, const u8 arg1, const u8 arg2,
        const u8 arg3, const u8 arg4, const u8 crc) //crc -> octet du control
{
    SPI((cmd | 0x40) & 0x7f); // Send CMD
    SPI(arg1);
    SPI(arg2);
    SPI(arg3);
    SPI(arg4);
    SPI(crc | 0x1);

    return (sd_wait_rep());
}

static u8 sd_error(char *str)
{
    LATBbits.LATB13 = 1;
    lcd_write_line(str, 0);
    return (0);
}

void sdcard_init(void)
{
    int     i = 0;
    // Configuration des PPS
    TRISBbits.TRISB2 = 1;
    SDI2Rbits.SDI2R = 4; // Serial Data Input
    RPB5Rbits.RPB5R = 4;

    // Configuration de la pin SS
    TRISBbits.TRISB13 = 0;
    LATBbits.LATB13 = 0;

    // Initialise SPI
    SPI2CON = 0;
    SPI2BRG = 2; // SPI Baud Rate Register

    SPI2CONbits.MSTEN = 1; // Le pic est le maitre
    SPI2CONbits.CKE = 1; // CKE: SPI Clock Edge Select bit ; 1 = Serial output data changes on transition from active clock state to idle clock state (see CKP bit)
    SPI2CONbits.CKP = 1; // Idle clock = low
    SPI2CONbits.SMP = 1;
    SPI2CONbits.ON = 1;

    // Interruption config
    IEC1bits.SPI2RXIE = 0;  // Gestion de recois (interrupt enable bit)
    IEC1bits.SPI2TXIE = 0; //Gestion de transmission des donnees
    IEC1bits.SPI2EIE = 0; //Gestion d'erreurs
    IPC9bits.SPI2IP = 0; // SPI2 Interrupt Priority (0 a 7)
    IPC9bits.SPI2IS = 0; // SPI2 Interrupt Sub-Priority (0 a 3)



    for (i = 0; i < 20; ++i)//min 74 clock's -> 20 * 8 > 74 ;D
        SPI(0xff);
    asm("nop");
    sd_send_cmd(0, 0, 0, 0, 0, 0x95);
}

u8 sdcard_start(void)
{
    u8 rep;
    u16 i;
    static u8  retry = 0;
    u8  timeout;

    BEGIN_INIT:
    timeout = 20;
        // Initialisation de la carte sd
    flags = 0;

    //sd_send_cmd(0, 0, 0, 0, 0, 0x95);
//    for (i = 0; i < 64; ++i)//min 74 clock's -> 20 * 8 > 74 ;D
//        SPI(0xff);
    //sd_send_cmd(41, 0x40, 0, 0, 0, 0);
    // Passage en mode natif de la carte (512 coups de clock si l'etat precedent = unfinished read)
//    LATBbits.LATB13 = 1;
    //asm("nop");
//    for (i = 0; i < 20; ++i)//min 74 clock's -> 20 * 8 > 74 ;D
//        SPI(0xff);

    // Reset the sdcard with CMD0
    LATBbits.LATB13 = 0;

    while (sd_send_cmd(0, 0, 0, 0, 0, 0x95) == 255 && timeout--); // b255 -> quand l'octet est en pull UP (11111111) -> SD carte ne repond pas
    if (!timeout)
        return (sd_error("    NO SDCARD   "));
    LATBbits.LATB13 = 1;
    asm("nop");
    LATBbits.LATB13 = 0;
    // Check the sdcard version and voltage with CMD8
    if ((rep = sd_send_cmd(8, 0, 0, 1, 0xaa, 0x87)) == 255) // No support current voltage
        return (sd_error("NO SUPPORT 3.3V "));
    if (rep & 0x4)
        return (sd_error(" BAD SD VERSION "));
    if (rep != 0x1)
        return (sd_error("   SD ERROR 1   "));
    SPI(0xff); // Command version + reserved
    SPI(0xff); // Reserved
    SPI(0xff); // Reserved + voltage
    SPI(0xff); // Echo back

    // Finish initialization with ACMD41
    i = 0;
    do {        
        // CMD55
        sd_send_cmd(55, 0, 0, 0, 0, 0x65);

        // ACMD41
        rep = sd_send_cmd(41, 0x40, 0, 0, 0, 0x77);
 
    } while (rep & 0x1 && i++ < 10);

    if (rep != 0 && retry < 2)
    {
        retry++;
        goto BEGIN_INIT;
    }
    retry = 0;
    if (rep != 0)
        return (sd_error("   FAIL INIT    "));

    // Check capacity card support with CMD58
    if (sd_send_cmd(58, 0, 0, 0, 0, 0) != 0)
        return (sd_error("   SD ERROR 4   "));
    if (SPI(0xff) & 0x40)// Get CCS (Card Capacity Status)
        flags |= FLAGS_CCS;
    SPI(0xff);
    SPI(0xff);
    SPI(0xff);

    // Set block len to 512 bytes if sdsc
    if (flags & FLAGS_CCS)
    {
        if (sd_send_cmd(16, 0, 0, 0x2, 0, 0) != 0)
            return (sd_error(" SD ERROR SDSC  "));
    }

    LATBbits.LATB13 = 1;

    return (1);
}

u8 *sdcard_read(u32 addr)
{
    static u8 block[512];
    u16 i;

    if (!(flags & FLAGS_CCS))
        addr *= 512;

    LATBbits.LATB13 = 0;

    // Request read block
    if (sd_send_cmd(17, addr >> 24, addr >> 16, addr >> 8, addr, 0) != 0)
    {
        sd_error(" SD READ ERROR  ");
        return (0);
    }
    
    // Wait data
    if (sd_wait_rep() != 0xfe)
    {
        sd_error("SD READ TOKEN ER");
        return (0);
    }
        
    // Read data block (512 bytes)
    i = 0;
    while (i < 512)
        block[i++] = SPI(0xff);

    // Ignore CRC
    SPI(0xff);
    SPI(0xff);

    LATBbits.LATB13 = 1;
    

    return (block);
}

__ISR(_SPI2_VECTOR, IPL7AUTO) SDCardISR()
{
    if (IEC1bits.SPI2TXIE && IFS1bits.SPI2TXIF)
    {
        IEC1bits.SPI2TXIE = 0;
        flags |= FLAGS_TX;
    }
    if (IEC1bits.SPI2RXIE && IFS1bits.SPI2RXIF)
    {
        IEC1bits.SPI2RXIE = 0;
        flags |= FLAGS_RX;
    }
}