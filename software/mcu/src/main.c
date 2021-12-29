#include <em_device.h>
#include <stdlib.h>
#include <math.h>
#include "debug_macros.h"
#include "utils.h"
#include "nvic.h"
#include "atomic.h"
#include "systick.h"
#include "rmu.h"
#include "emu.h"
#include "cmu.h"
#include "gpio.h"
#include "dbg.h"
#include "msc.h"
#include "rtcc.h"
#include "adc.h"
#include "crc.h"
#include "usart.h"
#include "i2c.h"
#include "wdog.h"

// Structs
typedef struct __attribute__((__packed__))
{
    uint16_t usMagic;
    uint8_t ubCommand;
    uint8_t ubPayloadSize;
} usart_cmd_header_t;

typedef struct __attribute__((__packed__))
{
    uint8_t ubChannel;
    float fDutyCycle;
} usart_cmd_set_dc_t;
typedef struct __attribute__((__packed__))
{
    uint8_t ubChannel;
    float fDutyCycle;
} usart_cmd_get_dc_t;
typedef struct __attribute__((__packed__))
{
    uint8_t ubChannel;
    float fVoltage;
} usart_cmd_get_voltage_t;
typedef struct __attribute__((__packed__))
{
    uint8_t ubChannel;
    float fTemperature;
} usart_cmd_get_temp_t;
typedef struct __attribute__((__packed__))
{
    uint32_t ulUID[2];
} usart_cmd_get_uid_t;
typedef struct __attribute__((__packed__))
{
    uint16_t usVersion;
    char szDate[12];
    char szTime[9];
} usart_cmd_get_sw_info_t;

// Defines
#define USART_HEADER_MAGIC      0xFAC7

#define USART_CMD_SET_DC        0x01
#define USART_CMD_GET_DC        0x02
#define USART_CMD_GET_VOLTAGE   0x03
#define USART_CMD_GET_TEMP      0x04
#define USART_CMD_ERROR         0xE0
#define USART_CMD_GET_UID       0xF0
#define USART_CMD_GET_SW_INFO   0xF1
#define USART_CMD_RESET_BL      0xFE
#define USART_CMD_RESET_APP     0xFF

#define USART_VOLTAGE_AVDD      0
#define USART_VOLTAGE_DVDD      1
#define USART_VOLTAGE_IOVDD     2
#define USART_VOLTAGE_CORE      3
#define USART_VOLTAGE_5V0       4
#define USART_VOLTAGE_VEXT      5

#define USART_TEMP_EMU          0
#define USART_TEMP_ADC          1

// Forward declarations
static void reset() __attribute__((noreturn));
static void sleep();

static uint32_t get_free_ram();

static void get_device_name(char *pszDeviceName, uint32_t ulDeviceNameSize);
static uint16_t get_device_revision();

static void wdog_warning_isr();

// Variables

// ISRs

// Functions
void reset()
{
    SCB->AIRCR = 0x05FA0000 | _VAL2FLD(SCB_AIRCR_SYSRESETREQ, 1);

    while(1);
}
void sleep()
{
    rtcc_set_alarm(rtcc_get_time() + 5);

    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk; // Configure Deep Sleep (EM2/3)

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        IRQ_CLEAR(RTCC_IRQn);

        __DMB(); // Wait for all memory transactions to finish before memory access
        __DSB(); // Wait for all memory transactions to finish before executing instructions
        __ISB(); // Wait for all memory transactions to finish before fetching instructions
        __SEV(); // Set the event flag to ensure the next WFE will be a NOP
        __WFE(); // NOP and clear the event flag
        __WFE(); // Wait for event
        __NOP(); // Prevent debugger crashes

        cmu_init();
        cmu_update_clocks();
    }
}

uint32_t get_free_ram()
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        extern void *_sbrk(int);

        void *pCurrentHeap = _sbrk(1);

        if(!pCurrentHeap)
            return 0;

        uint32_t ulFreeRAM = (uint32_t)__get_MSP() - (uint32_t)pCurrentHeap;

        _sbrk(-1);

        return ulFreeRAM;
    }
}

void get_device_name(char *pszDeviceName, uint32_t ulDeviceNameSize)
{
    uint8_t ubFamily = (DEVINFO->PART & _DEVINFO_PART_DEVICE_FAMILY_MASK) >> _DEVINFO_PART_DEVICE_FAMILY_SHIFT;
    const char* szFamily = "?";

    switch(ubFamily)
    {
        case 0x10: szFamily = "EFR32MG1P";  break;
        case 0x11: szFamily = "EFR32MG1B";  break;
        case 0x12: szFamily = "EFR32MG1V";  break;
        case 0x13: szFamily = "EFR32BG1P";  break;
        case 0x14: szFamily = "EFR32BG1B";  break;
        case 0x15: szFamily = "EFR32BG1V";  break;
        case 0x19: szFamily = "EFR32FG1P";  break;
        case 0x1A: szFamily = "EFR32FG1B";  break;
        case 0x1B: szFamily = "EFR32FG1V";  break;
        case 0x1C: szFamily = "EFR32MG12P"; break;
        case 0x1D: szFamily = "EFR32MG12B"; break;
        case 0x1E: szFamily = "EFR32MG12V"; break;
        case 0x1F: szFamily = "EFR32BG12P"; break;
        case 0x20: szFamily = "EFR32BG12B"; break;
        case 0x21: szFamily = "EFR32BG12V"; break;
        case 0x25: szFamily = "EFR32FG12P"; break;
        case 0x26: szFamily = "EFR32FG12B"; break;
        case 0x27: szFamily = "EFR32FG12V"; break;
        case 0x28: szFamily = "EFR32MG13P"; break;
        case 0x29: szFamily = "EFR32MG13B"; break;
        case 0x2A: szFamily = "EFR32MG13V"; break;
        case 0x2B: szFamily = "EFR32BG13P"; break;
        case 0x2C: szFamily = "EFR32BG13B"; break;
        case 0x2D: szFamily = "EFR32BG13V"; break;
        case 0x2E: szFamily = "EFR32ZG13P"; break;
        case 0x31: szFamily = "EFR32FG13P"; break;
        case 0x32: szFamily = "EFR32FG13B"; break;
        case 0x33: szFamily = "EFR32FG13V"; break;
        case 0x34: szFamily = "EFR32MG14P"; break;
        case 0x35: szFamily = "EFR32MG14B"; break;
        case 0x36: szFamily = "EFR32MG14V"; break;
        case 0x37: szFamily = "EFR32BG14P"; break;
        case 0x38: szFamily = "EFR32BG14B"; break;
        case 0x39: szFamily = "EFR32BG14V"; break;
        case 0x3A: szFamily = "EFR32ZG14P"; break;
        case 0x3D: szFamily = "EFR32FG14P"; break;
        case 0x3E: szFamily = "EFR32FG14B"; break;
        case 0x3F: szFamily = "EFR32FG14V"; break;
        case 0x47: szFamily = "EFM32G";     break;
        case 0x48: szFamily = "EFM32GG";    break;
        case 0x49: szFamily = "EFM32TG";    break;
        case 0x4A: szFamily = "EFM32LG";    break;
        case 0x4B: szFamily = "EFM32WG";    break;
        case 0x4C: szFamily = "EFM32ZG";    break;
        case 0x4D: szFamily = "EFM32HG";    break;
        case 0x51: szFamily = "EFM32PG1B";  break;
        case 0x53: szFamily = "EFM32JG1B";  break;
        case 0x55: szFamily = "EFM32PG12B"; break;
        case 0x57: szFamily = "EFM32JG12B"; break;
        case 0x64: szFamily = "EFM32GG11B"; break;
        case 0x67: szFamily = "EFM32TG11B"; break;
        case 0x6A: szFamily = "EFM32GG12B"; break;
        case 0x78: szFamily = "EZR32LG";    break;
        case 0x79: szFamily = "EZR32WG";    break;
        case 0x7A: szFamily = "EZR32HG";    break;
    }

    uint8_t ubPackage = (DEVINFO->MEMINFO & _DEVINFO_MEMINFO_PKGTYPE_MASK) >> _DEVINFO_MEMINFO_PKGTYPE_SHIFT;
    char cPackage = '?';

    if(ubPackage == 74)
        cPackage = '?';
    else if(ubPackage == 76)
        cPackage = 'L';
    else if(ubPackage == 77)
        cPackage = 'M';
    else if(ubPackage == 81)
        cPackage = 'Q';

    uint8_t ubTempGrade = (DEVINFO->MEMINFO & _DEVINFO_MEMINFO_TEMPGRADE_MASK) >> _DEVINFO_MEMINFO_TEMPGRADE_SHIFT;
    char cTempGrade = '?';

    if(ubTempGrade == 0)
        cTempGrade = 'G';
    else if(ubTempGrade == 1)
        cTempGrade = 'I';
    else if(ubTempGrade == 2)
        cTempGrade = '?';
    else if(ubTempGrade == 3)
        cTempGrade = '?';

    uint16_t usPartNumber = (DEVINFO->PART & _DEVINFO_PART_DEVICE_NUMBER_MASK) >> _DEVINFO_PART_DEVICE_NUMBER_SHIFT;
    uint8_t ubPinCount = (DEVINFO->MEMINFO & _DEVINFO_MEMINFO_PINCOUNT_MASK) >> _DEVINFO_MEMINFO_PINCOUNT_SHIFT;

    snprintf(pszDeviceName, ulDeviceNameSize, "%s%huF%hu%c%c%hhu", szFamily, usPartNumber, FLASH_SIZE >> 10, cTempGrade, cPackage, ubPinCount);
}
uint16_t get_device_revision()
{
    uint16_t usRevision;

    /* CHIP MAJOR bit [3:0]. */
    usRevision = ((ROMTABLE->PID0 & _ROMTABLE_PID0_REVMAJOR_MASK) >> _ROMTABLE_PID0_REVMAJOR_SHIFT) << 8;
    /* CHIP MINOR bit [7:4]. */
    usRevision |= ((ROMTABLE->PID2 & _ROMTABLE_PID2_REVMINORMSB_MASK) >> _ROMTABLE_PID2_REVMINORMSB_SHIFT) << 4;
    /* CHIP MINOR bit [3:0]. */
    usRevision |= (ROMTABLE->PID3 & _ROMTABLE_PID3_REVMINORLSB_MASK) >> _ROMTABLE_PID3_REVMINORLSB_SHIFT;

    return usRevision;
}

void wdog_warning_isr()
{
    DBGPRINTLN_CTX("Watchdog warning!");
}

void init_timers()
{
    // Timer 0
    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_TIMER0, 1);

    TIMER0->CTRL = TIMER_CTRL_RSSCOIST | TIMER_CTRL_PRESC_DIV4 | TIMER_CTRL_CLKSEL_PRESCHFPERCLK | TIMER_CTRL_FALLA_NONE | TIMER_CTRL_RISEA_NONE | TIMER_CTRL_MODE_UP;
    TIMER0->TOP = 255;
    TIMER0->CNT = 0x0000;

    TIMER0->CC[0].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER0->CC[0].CCV = 0x0000;

    TIMER0->CC[1].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER0->CC[1].CCV = 0x0000;

    TIMER0->CC[2].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER0->CC[2].CCV = 0x0000;

    TIMER0->ROUTELOC0 = TIMER_ROUTELOC0_CC0LOC_LOC28 | TIMER_ROUTELOC0_CC1LOC_LOC26 | TIMER_ROUTELOC0_CC2LOC_LOC14;
    TIMER0->ROUTEPEN |= TIMER_ROUTEPEN_CC0PEN | TIMER_ROUTEPEN_CC1PEN | TIMER_ROUTEPEN_CC2PEN;

    // Timer 1
    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_TIMER1, 1);

    TIMER1->CTRL = TIMER_CTRL_RSSCOIST | TIMER_CTRL_PRESC_DIV4 | TIMER_CTRL_CLKSEL_PRESCHFPERCLK | TIMER_CTRL_FALLA_NONE | TIMER_CTRL_RISEA_NONE | TIMER_CTRL_MODE_UP;
    TIMER1->TOP = 255;
    TIMER1->CNT = 0x0000;

    TIMER1->CC[0].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER1->CC[0].CCV = 0x0000;

    TIMER1->CC[1].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER1->CC[1].CCV = 0x0000;

    TIMER1->CC[2].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER1->CC[2].CCV = 0x0000;

    TIMER1->CC[3].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER1->CC[3].CCV = 0x0000;

    TIMER1->ROUTELOC0 = TIMER_ROUTELOC0_CC0LOC_LOC15 | TIMER_ROUTELOC0_CC1LOC_LOC13 | TIMER_ROUTELOC0_CC2LOC_LOC11 | TIMER_ROUTELOC0_CC3LOC_LOC7;
    TIMER1->ROUTEPEN |= TIMER_ROUTEPEN_CC0PEN | TIMER_ROUTEPEN_CC1PEN | TIMER_ROUTEPEN_CC2PEN | TIMER_ROUTEPEN_CC3PEN;

    // Start both timers
    TIMER0->CMD = TIMER_CMD_START;
    TIMER1->CMD = TIMER_CMD_START;
}
void set_channel_dc(uint8_t ubChannel, float fDuty)
{
    if(ubChannel > 6)
        return;

    if(fDuty < 0 || fDuty > 1)
        return;

    if(ubChannel > 2)
        TIMER1->CC[ubChannel - 3].CCVB = (uint16_t)(fDuty * 256);
    else
        TIMER0->CC[ubChannel].CCVB = (uint16_t)(fDuty * 256);
}
float get_channel_dc(uint8_t ubChannel)
{
    if(ubChannel > 6)
        return 0.f;

    if(ubChannel > 2)
        return (float)TIMER1->CC[ubChannel - 3].CCV / 256;
    else
        return (float)TIMER0->CC[ubChannel].CCV / 256;
}

int init()
{
    rmu_init(RMU_CTRL_PINRMODE_FULL, RMU_CTRL_SYSRMODE_EXTENDED, RMU_CTRL_LOCKUPRMODE_EXTENDED, RMU_CTRL_WDOGRMODE_EXTENDED); // Init RMU and set reset modes

    emu_init(1); // Init EMU, ignore DCDC and switch digital power immediatly to DVDD

    cmu_init(); // Init Clocks

    dbg_init(); // Init Debug module
    dbg_swo_config(BIT(0) | BIT(1), 200000); // Init SWO channels 0 and 1 at 200 kHz

    msc_init(); // Init Flash, RAM and caches

    systick_init(); // Init system tick

    wdog_init((8 <<_WDOG_CTRL_PERSEL_SHIFT) | (3 << _WDOG_CTRL_WARNSEL_SHIFT)); // Init the watchdog timer, 2049 ms timeout, 75% warning
    wdog_set_warning_isr(wdog_warning_isr);

    gpio_init(); // Init GPIOs
    ldma_init(); // Init LDMA
    rtcc_init(); // Init RTCC
    crc_init(); // Init CRC calculation unit
    adc_init(); // Init ADCs

    float fAVDDHighThresh, fAVDDLowThresh;
    float fDVDDHighThresh, fDVDDLowThresh;
    float fIOVDDHighThresh, fIOVDDLowThresh;

    emu_vmon_avdd_config(1, 3.1f, &fAVDDLowThresh, 3.22f, &fAVDDHighThresh); // Enable AVDD monitor
    emu_vmon_dvdd_config(1, 3.1f, &fDVDDLowThresh); // Enable DVDD monitor
    emu_vmon_iovdd_config(1, 3.15f, &fIOVDDLowThresh); // Enable IOVDD monitor

    fDVDDHighThresh = fDVDDLowThresh + 0.026f; // Hysteresis from datasheet
    fIOVDDHighThresh = fIOVDDLowThresh + 0.026f; // Hysteresis from datasheet

    usart0_init(115200, USART_FRAME_STOPBITS_ONE | USART_FRAME_PARITY_NONE | USART_FRAME_DATABITS_EIGHT, 17, 19, -1, -1);
    i2c0_init(I2C_NORMAL, 31, 1);

    char szDeviceName[32];

    get_device_name(szDeviceName, 32);

    DBGPRINTLN_CTX("USB Fan Controller v%lu (%s %s)!", BUILD_VERSION, __DATE__, __TIME__);
    DBGPRINTLN_CTX("Device: %s", szDeviceName);
    DBGPRINTLN_CTX("Device Revision: 0x%04X", get_device_revision());
    DBGPRINTLN_CTX("Calibration temperature: %hhu C", (DEVINFO->CAL & _DEVINFO_CAL_TEMP_MASK) >> _DEVINFO_CAL_TEMP_SHIFT);
    DBGPRINTLN_CTX("Flash Size: %hu kB", FLASH_SIZE >> 10);
    DBGPRINTLN_CTX("RAM Size: %hu kB", SRAM_SIZE >> 10);
    DBGPRINTLN_CTX("Free RAM: %lu B", get_free_ram());
    DBGPRINTLN_CTX("Unique ID: %08X-%08X", DEVINFO->UNIQUEH, DEVINFO->UNIQUEL);

    DBGPRINTLN_CTX("RMU - Reset cause: %hhu", rmu_get_reset_reason());
    DBGPRINTLN_CTX("RMU - Reset state: %hhu", rmu_get_reset_state());

    rmu_clear_reset_reason();
    rmu_set_reset_state(0x00);

    DBGPRINTLN_CTX("CMU - HFXO Oscillator: %.3f MHz", (float)HFXO_OSC_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFRCO Oscillator: %.3f MHz", (float)HFRCO_OSC_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - AUXHFRCO Oscillator: %.3f MHz", (float)AUXHFRCO_OSC_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - LFXO Oscillator: %.3f kHz", (float)LFXO_OSC_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LFRCO Oscillator: %.3f kHz", (float)LFRCO_OSC_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - ULFRCO Oscillator: %.3f kHz", (float)ULFRCO_OSC_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - HFSRC Clock: %.3f MHz", (float)HFSRC_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HF Clock: %.3f MHz", (float)HF_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFCORE Clock: %.3f MHz", (float)HFCORE_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFEXP Clock: %.3f MHz", (float)HFEXP_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFPER Clock: %.3f MHz", (float)HFPER_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFLE Clock: %.3f MHz", (float)HFLE_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - ADC0 Clock: %.3f MHz", (float)ADC0_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - DBG Clock: %.3f MHz", (float)DBG_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - AUX Clock: %.3f MHz", (float)AUX_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - LFA Clock: %.3f kHz", (float)LFA_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LETIMER0 Clock: %.3f kHz", (float)LETIMER0_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LFB Clock: %.3f kHz", (float)LFB_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LEUART0 Clock: %.3f kHz", (float)LEUART0_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LFE Clock: %.3f kHz", (float)LFE_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - RTCC Clock: %.3f kHz", (float)RTCC_CLOCK_FREQ / 1000);

    DBGPRINTLN_CTX("EMU - AVDD Fall Threshold: %.2f mV!", fAVDDLowThresh * 1000);
    DBGPRINTLN_CTX("EMU - AVDD Rise Threshold: %.2f mV!", fAVDDHighThresh * 1000);
    DBGPRINTLN_CTX("EMU - AVDD Voltage: %.2f mV", adc_get_avdd());
    DBGPRINTLN_CTX("EMU - AVDD Status: %s", g_ubAVDDLow ? "LOW" : "OK");
    DBGPRINTLN_CTX("EMU - DVDD Fall Threshold: %.2f mV!", fDVDDLowThresh * 1000);
    DBGPRINTLN_CTX("EMU - DVDD Rise Threshold: %.2f mV!", fDVDDHighThresh * 1000);
    DBGPRINTLN_CTX("EMU - DVDD Voltage: %.2f mV", adc_get_dvdd());
    DBGPRINTLN_CTX("EMU - DVDD Status: %s", g_ubDVDDLow ? "LOW" : "OK");
    DBGPRINTLN_CTX("EMU - IOVDD Fall Threshold: %.2f mV!", fIOVDDLowThresh * 1000);
    DBGPRINTLN_CTX("EMU - IOVDD Rise Threshold: %.2f mV!", fIOVDDHighThresh * 1000);
    DBGPRINTLN_CTX("EMU - IOVDD Voltage: %.2f mV", adc_get_iovdd());
    DBGPRINTLN_CTX("EMU - IOVDD Status: %s", g_ubIOVDDLow ? "LOW" : "OK");
    DBGPRINTLN_CTX("EMU - Core Voltage: %.2f mV", adc_get_corevdd());
    DBGPRINTLN_CTX("EMU - 5V0 Voltage: %.2f mV", adc_get_5v0());
    DBGPRINTLN_CTX("EMU - VEXT Voltage: %.2f mV", adc_get_vext());

    DBGPRINTLN_CTX("Scanning I2C bus 0...");

    for(uint8_t a = 0x08; a < 0x78; a++)
    {
        if(i2c0_write(a, 0, 0, I2C_STOP))
            DBGPRINTLN_CTX("  Address 0x%02X ACKed!", a);
    }

    return 0;
}
int main()
{
    init_timers();

    while(1)
    {
        wdog_feed();

        static uint64_t ullLastUSARTChange = 0;
        static uint32_t ulLastUSARTAvailable = 0;

        if(usart0_available() != ulLastUSARTAvailable)
        {
            ulLastUSARTAvailable = usart0_available();
            ullLastUSARTChange = g_ullSystemTick;
        }

        if(ulLastUSARTAvailable != 0 && g_ullSystemTick - ullLastUSARTChange > 2000)
        {
            DBGPRINTLN_CTX("Clearing USART buffer...");

            usart0_flush();
        }

        if(usart0_available() >= sizeof(usart_cmd_header_t))
        {
            usart_cmd_header_t xHeader;

            DBGPRINTLN_CTX("Reading header...");
            usart0_read((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

            DBGPRINTLN_CTX("Header [M %04X] [C %02X] [S %02X]", xHeader.usMagic, xHeader.ubCommand, xHeader.ubPayloadSize);

            if(xHeader.usMagic != USART_HEADER_MAGIC)
            {
                DBGPRINTLN_CTX("Invalid magic!");

                continue;
            }

            switch(xHeader.ubCommand)
            {
                case USART_CMD_SET_DC:
                {
                    if(xHeader.ubPayloadSize != sizeof(usart_cmd_set_dc_t))
                    {
                        DBGPRINTLN_CTX("Invalid payload size!");

                        xHeader.ubCommand = USART_CMD_ERROR;
                        xHeader.ubPayloadSize = 0;
                        usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                        break;
                    }

                    if(usart0_available() < xHeader.ubPayloadSize)
                    {
                        DBGPRINTLN_CTX("Not enough data, waiting...");

                        uint64_t ullStartTick = g_ullSystemTick;

                        while(usart0_available() < xHeader.ubPayloadSize && g_ullSystemTick - ullStartTick <= 500);

                        if(usart0_available() < xHeader.ubPayloadSize)
                        {
                            DBGPRINTLN_CTX("Timed out waiting for payload!");

                            xHeader.ubCommand = USART_CMD_ERROR;
                            xHeader.ubPayloadSize = 0;
                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                            break;
                        }
                    }

                    usart_cmd_set_dc_t xPayload;

                    DBGPRINTLN_CTX("Reading payload...");
                    usart0_read((uint8_t *)&xPayload, xHeader.ubPayloadSize);

                    DBGPRINTLN_CTX("USART_CMD_SET_DC [C %hhu] [D %.6f]", xPayload.ubChannel, xPayload.fDutyCycle);

                    if(xPayload.ubChannel > 6)
                    {
                        DBGPRINTLN_CTX("Invalid channel!");

                        xHeader.ubCommand = USART_CMD_ERROR;
                        xHeader.ubPayloadSize = 0;
                        usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                        break;
                    }

                    if(xPayload.fDutyCycle < 0.0f || xPayload.fDutyCycle > 1.0f)
                    {
                        DBGPRINTLN_CTX("Invalid duty cycle!");

                        xHeader.ubCommand = USART_CMD_ERROR;
                        xHeader.ubPayloadSize = 0;
                        usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                        break;
                    }

                    set_channel_dc(xPayload.ubChannel, xPayload.fDutyCycle);

                    xHeader.ubPayloadSize = 0;
                    usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                }
                break;
                case USART_CMD_GET_DC:
                {
                    if(xHeader.ubPayloadSize != sizeof(usart_cmd_get_dc_t))
                    {
                        DBGPRINTLN_CTX("Invalid payload size!");

                        xHeader.ubCommand = USART_CMD_ERROR;
                        xHeader.ubPayloadSize = 0;
                        usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                        break;
                    }

                    if(usart0_available() < xHeader.ubPayloadSize)
                    {
                        DBGPRINTLN_CTX("Not enough data, waiting...");

                        uint64_t ullStartTick = g_ullSystemTick;

                        while(usart0_available() < xHeader.ubPayloadSize && g_ullSystemTick - ullStartTick <= 500);

                        if(usart0_available() < xHeader.ubPayloadSize)
                        {
                            DBGPRINTLN_CTX("Timed out waiting for payload!");

                            xHeader.ubCommand = USART_CMD_ERROR;
                            xHeader.ubPayloadSize = 0;
                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                            break;
                        }
                    }

                    usart_cmd_get_dc_t xPayload;

                    DBGPRINTLN_CTX("Reading payload...");
                    usart0_read((uint8_t *)&xPayload, xHeader.ubPayloadSize);

                    DBGPRINTLN_CTX("USART_CMD_GET_DC [C %hhu]", xPayload.ubChannel);

                    if(xPayload.ubChannel > 6)
                    {
                        DBGPRINTLN_CTX("Invalid channel!");

                        xHeader.ubCommand = USART_CMD_ERROR;
                        xHeader.ubPayloadSize = 0;
                        usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                        break;
                    }

                    xPayload.fDutyCycle = get_channel_dc(xPayload.ubChannel);

                    usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                    usart0_write((uint8_t *)&xPayload, sizeof(usart_cmd_get_dc_t));
                }
                break;
                case USART_CMD_GET_VOLTAGE:
                {
                    if(xHeader.ubPayloadSize != sizeof(usart_cmd_get_voltage_t))
                    {
                        DBGPRINTLN_CTX("Invalid payload size!");

                        xHeader.ubCommand = USART_CMD_ERROR;
                        xHeader.ubPayloadSize = 0;
                        usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                        break;
                    }

                    if(usart0_available() < xHeader.ubPayloadSize)
                    {
                        DBGPRINTLN_CTX("Not enough data, waiting...");

                        uint64_t ullStartTick = g_ullSystemTick;

                        while(usart0_available() < xHeader.ubPayloadSize && g_ullSystemTick - ullStartTick <= 500);

                        if(usart0_available() < xHeader.ubPayloadSize)
                        {
                            DBGPRINTLN_CTX("Timed out waiting for payload!");

                            xHeader.ubCommand = USART_CMD_ERROR;
                            xHeader.ubPayloadSize = 0;
                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                            break;
                        }
                    }

                    usart_cmd_get_voltage_t xPayload;

                    DBGPRINTLN_CTX("Reading payload...");
                    usart0_read((uint8_t *)&xPayload, xHeader.ubPayloadSize);

                    DBGPRINTLN_CTX("USART_CMD_GET_VOLTAGE [C %hhu]", xPayload.ubChannel);

                    switch(xPayload.ubChannel)
                    {
                        case USART_VOLTAGE_AVDD:
                        {
                            xPayload.fVoltage = adc_get_avdd();

                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                            usart0_write((uint8_t *)&xPayload, sizeof(usart_cmd_get_voltage_t));
                        }
                        break;
                        case USART_VOLTAGE_DVDD:
                        {
                            xPayload.fVoltage = adc_get_dvdd();

                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                            usart0_write((uint8_t *)&xPayload, sizeof(usart_cmd_get_voltage_t));
                        }
                        break;
                        case USART_VOLTAGE_IOVDD:
                        {
                            xPayload.fVoltage = adc_get_iovdd();

                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                            usart0_write((uint8_t *)&xPayload, sizeof(usart_cmd_get_voltage_t));
                        }
                        break;
                        case USART_VOLTAGE_CORE:
                        {
                            xPayload.fVoltage = adc_get_corevdd();

                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                            usart0_write((uint8_t *)&xPayload, sizeof(usart_cmd_get_voltage_t));
                        }
                        break;
                        case USART_VOLTAGE_5V0:
                        {
                            xPayload.fVoltage = adc_get_5v0();

                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                            usart0_write((uint8_t *)&xPayload, sizeof(usart_cmd_get_voltage_t));
                        }
                        break;
                        case USART_VOLTAGE_VEXT:
                        {
                            xPayload.fVoltage = adc_get_vext();

                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                            usart0_write((uint8_t *)&xPayload, sizeof(usart_cmd_get_voltage_t));
                        }
                        break;
                        default:
                        {
                            DBGPRINTLN_CTX("Invalid voltage channel!");

                            xHeader.ubCommand = USART_CMD_ERROR;
                            xHeader.ubPayloadSize = 0;
                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                        }
                        break;
                    }
                }
                break;
                case USART_CMD_GET_TEMP:
                {
                    if(xHeader.ubPayloadSize != sizeof(usart_cmd_get_temp_t))
                    {
                        DBGPRINTLN_CTX("Invalid payload size!");

                        xHeader.ubCommand = USART_CMD_ERROR;
                        xHeader.ubPayloadSize = 0;
                        usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                        break;
                    }

                    if(usart0_available() < xHeader.ubPayloadSize)
                    {
                        DBGPRINTLN_CTX("Not enough data, waiting...");

                        uint64_t ullStartTick = g_ullSystemTick;

                        while(usart0_available() < xHeader.ubPayloadSize && g_ullSystemTick - ullStartTick <= 500);

                        if(usart0_available() < xHeader.ubPayloadSize)
                        {
                            DBGPRINTLN_CTX("Timed out waiting for payload!");

                            xHeader.ubCommand = USART_CMD_ERROR;
                            xHeader.ubPayloadSize = 0;
                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                            break;
                        }
                    }

                    usart_cmd_get_temp_t xPayload;

                    DBGPRINTLN_CTX("Reading payload...");
                    usart0_read((uint8_t *)&xPayload, xHeader.ubPayloadSize);

                    DBGPRINTLN_CTX("USART_CMD_GET_TEMP [C %hhu]", xPayload.ubChannel);

                    switch(xPayload.ubChannel)
                    {
                        case USART_TEMP_EMU:
                        {
                            xPayload.fTemperature = emu_get_temperature();

                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                            usart0_write((uint8_t *)&xPayload, sizeof(usart_cmd_get_temp_t));
                        }
                        break;
                        case USART_TEMP_ADC:
                        {
                            xPayload.fTemperature = adc_get_temperature();

                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                            usart0_write((uint8_t *)&xPayload, sizeof(usart_cmd_get_temp_t));
                        }
                        break;
                        default:
                        {
                            DBGPRINTLN_CTX("Invalid temperature channel!");

                            xHeader.ubCommand = USART_CMD_ERROR;
                            xHeader.ubPayloadSize = 0;
                            usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                        }
                        break;
                    }
                }
                break;
                case USART_CMD_GET_UID:
                {
                    if(xHeader.ubPayloadSize != 0)
                    {
                        DBGPRINTLN_CTX("Invalid payload size!");

                        xHeader.ubCommand = USART_CMD_ERROR;
                        xHeader.ubPayloadSize = 0;
                        usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                        break;
                    }

                    DBGPRINTLN_CTX("USART_CMD_GET_UID");

                    usart_cmd_get_uid_t xPayload;

                    xHeader.ubPayloadSize = sizeof(usart_cmd_get_uid_t);

                    xPayload.ulUID[0] = DEVINFO->UNIQUEL;
                    xPayload.ulUID[1] = DEVINFO->UNIQUEH;

                    usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                    usart0_write((uint8_t *)&xPayload, sizeof(usart_cmd_get_uid_t));
                }
                break;
                case USART_CMD_GET_SW_INFO:
                {
                    if(xHeader.ubPayloadSize != 0)
                    {
                        DBGPRINTLN_CTX("Invalid payload size!");

                        xHeader.ubCommand = USART_CMD_ERROR;
                        xHeader.ubPayloadSize = 0;
                        usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                        break;
                    }

                    DBGPRINTLN_CTX("USART_CMD_GET_SW_INFO");

                    usart_cmd_get_sw_info_t xPayload;

                    xHeader.ubPayloadSize = sizeof(usart_cmd_get_sw_info_t);

                    xPayload.usVersion = BUILD_VERSION;
                    strcpy(xPayload.szDate, __DATE__);
                    strcpy(xPayload.szTime, __TIME__);

                    usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                    usart0_write((uint8_t *)&xPayload, sizeof(usart_cmd_get_sw_info_t));
                }
                break;
                case USART_CMD_RESET_BL:
                {
                    if(xHeader.ubPayloadSize != 0)
                    {
                        DBGPRINTLN_CTX("Invalid payload size!");

                        xHeader.ubCommand = USART_CMD_ERROR;
                        xHeader.ubPayloadSize = 0;
                        usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                        break;
                    }

                    DBGPRINTLN_CTX("USART_CMD_RESET_BL");

                    usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                    DBGPRINTLN_CTX("Resetting to bootloader...");

                    delay_ms(100);

                    rmu_set_reset_state(0x01);
                    reset();
                }
                break;
                case USART_CMD_RESET_APP:
                {
                    if(xHeader.ubPayloadSize != 0)
                    {
                        DBGPRINTLN_CTX("Invalid payload size!");

                        xHeader.ubCommand = USART_CMD_ERROR;
                        xHeader.ubPayloadSize = 0;
                        usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                        break;
                    }

                    DBGPRINTLN_CTX("USART_CMD_RESET_APP");

                    usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));

                    DBGPRINTLN_CTX("Resetting to application...");

                    delay_ms(100);

                    rmu_set_reset_state(0x00);
                    reset();
                }
                break;
                default:
                {
                    DBGPRINTLN_CTX("Invalid command!");

                    xHeader.ubCommand = USART_CMD_ERROR;
                    xHeader.ubPayloadSize = 0;
                    usart0_write((uint8_t *)&xHeader, sizeof(usart_cmd_header_t));
                }
            }
        }
    }

    return 0;
}