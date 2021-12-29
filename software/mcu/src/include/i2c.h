#ifndef __I2C_H__
#define __I2C_H__

#include <em_device.h>
#include "utils.h"
#include "cmu.h"

#define I2C_NORMAL 0
#define I2C_FAST 1

#define I2C_RESTART 0
#define I2C_STOP 1

void i2c0_init(uint8_t ubMode, uint8_t ubSCLLocation, uint8_t ubSDALocation);
uint8_t i2c0_transmit(uint8_t ubAddress, uint8_t *pubSrc, uint32_t ulCount, uint8_t ubStop);
static inline uint8_t i2c0_write(uint8_t ubAddress, uint8_t *pubSrc, uint32_t ulCount, uint8_t ubStop)
{
    return i2c0_transmit((ubAddress << 1) & ~0x01, pubSrc, ulCount, ubStop);
}
static inline uint8_t i2c0_read(uint8_t ubAddress, uint8_t *pubDst, uint32_t ulCount, uint8_t ubStop)
{
    return i2c0_transmit((ubAddress << 1) | 0x01, pubDst, ulCount, ubStop);
}
static inline uint8_t i2c0_write_byte(uint8_t ubAddress, uint8_t ubData, uint8_t ubStop)
{
    return i2c0_transmit((ubAddress << 1) & ~0x01, &ubData, 1, ubStop);
}
static inline uint8_t i2c0_read_byte(uint8_t ubAddress, uint8_t ubStop)
{
    uint8_t ubData;

    i2c0_transmit((ubAddress << 1) | 0x01, &ubData, 1, ubStop);

    return ubData;
}

#endif  // __I2C_H__
