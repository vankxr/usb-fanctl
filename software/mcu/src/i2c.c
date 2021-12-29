#include "i2c.h"

void i2c0_init(uint8_t ubMode, uint8_t ubSCLLocation, uint8_t ubSDALocation)
{
    if(ubSCLLocation > AFCHANLOC_MAX)
        return;

    if(ubSDALocation > AFCHANLOC_MAX)
        return;

    CMU->HFPERCLKEN0 |= CMU_HFPERCLKEN0_I2C0;

    I2C0->CTRL = I2C_CTRL_CLHR_STANDARD | I2C_CTRL_TXBIL_EMPTY;
    I2C0->ROUTEPEN = I2C_ROUTEPEN_SCLPEN | I2C_ROUTEPEN_SDAPEN;
    I2C0->ROUTELOC0 = ((uint32_t)ubSCLLocation << _I2C_ROUTELOC0_SCLLOC_SHIFT) | ((uint32_t)ubSDALocation << _I2C_ROUTELOC0_SDALOC_SHIFT);

    if(ubMode == I2C_NORMAL)
        I2C0->CLKDIV = (((HFPER_CLOCK_FREQ / 100000) - 8) / 8) - 1;
    else if(ubMode == I2C_FAST)
        I2C0->CLKDIV = (((HFPER_CLOCK_FREQ / 400000) - 8) / 8) - 1;

    I2C0->CTRL |= I2C_CTRL_EN;
    I2C0->CMD = I2C_CMD_ABORT;

    while(I2C0->STATE & I2C_STATE_BUSY);
}
uint8_t i2c0_transmit(uint8_t ubAddress, uint8_t *pubSrc, uint32_t ulCount, uint8_t ubStop)
{
    I2C0->IFC = _I2C_IFC_MASK;

    I2C0->CMD = I2C_CMD_START;

    while(!(I2C0->IF & (I2C_IF_START | I2C_IF_RSTART | I2C_IF_ARBLOST | I2C_IF_BUSERR)));

    if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
    {
        I2C0->CMD = I2C_CMD_ABORT;

        return 0;
    }

    I2C0->TXDATA = ubAddress;

    while(!(I2C0->IF & (I2C_IF_ACK | I2C_IF_NACK | I2C_IF_ARBLOST | I2C_IF_BUSERR)));

    if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
    {
        I2C0->CMD = I2C_CMD_ABORT;

        return 0;
    }
    else if(I2C0->IF & I2C_IF_NACK)
    {
        I2C0->CMD = I2C_CMD_STOP;

        while(I2C0->IFC & (I2C_IFC_MSTOP | I2C_IFC_ARBLOST | I2C_IF_BUSERR));

        if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
            I2C0->CMD = I2C_CMD_ABORT;

        return 0;
    }

    if(ulCount)
        do
        {
            if(ubAddress & 1) // Read
            {
                while(!(I2C0->IF & (I2C_IF_RXDATAV | I2C_IF_ARBLOST | I2C_IF_BUSERR)));

                if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
                {
                    I2C0->CMD = I2C_CMD_ABORT;

                    return 0;
                }

                *pubSrc++ = I2C0->RXDATA;

                if(ulCount > 1)
                    I2C0->CMD = I2C_CMD_ACK;
                else
                    I2C0->CMD = I2C_CMD_NACK;
            }
            else // Write
            {
                I2C0->IFC = I2C_IFC_ACK;

                I2C0->TXDATA = *pubSrc++;

                while(!(I2C0->IF & (I2C_IF_ACK | I2C_IF_NACK | I2C_IF_ARBLOST | I2C_IF_BUSERR)));

                if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
                {
                    I2C0->CMD = I2C_CMD_ABORT;

                    return 0;
                }
                else if(I2C0->IF & I2C_IF_NACK)
                {
                    I2C0->CMD = I2C_CMD_STOP;

                    while(I2C0->IFC & (I2C_IFC_MSTOP | I2C_IFC_ARBLOST | I2C_IF_BUSERR));

                    if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
                        I2C0->CMD = I2C_CMD_ABORT;

                    return 0;
                }
            }
        } while(--ulCount);

    if(ubStop)
    {
        I2C0->CMD = I2C_CMD_STOP;

        while(I2C0->IFC & (I2C_IFC_MSTOP | I2C_IFC_ARBLOST | I2C_IF_BUSERR));

        if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
        {
            I2C0->CMD = I2C_CMD_ABORT;

            return 0;
        }
    }

    return 1;
}