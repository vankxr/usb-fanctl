#include "cmu.h"

uint32_t HFXO_OSC_FREQ;
uint32_t HFRCO_OSC_FREQ = 19000000UL;
uint32_t AUXHFRCO_OSC_FREQ = 19000000UL;
uint32_t LFXO_OSC_FREQ;
uint32_t LFRCO_OSC_FREQ = 32768UL;
uint32_t ULFRCO_OSC_FREQ = 1000UL;

uint32_t HFSRC_CLOCK_FREQ;
uint32_t HF_CLOCK_FREQ;
uint32_t HFCORE_CLOCK_FREQ;
uint32_t HFEXP_CLOCK_FREQ;
uint32_t HFPER_CLOCK_FREQ;
uint32_t HFLE_CLOCK_FREQ;
uint32_t ADC0_CLOCK_FREQ;
uint32_t DBG_CLOCK_FREQ;
uint32_t AUX_CLOCK_FREQ;
uint32_t LFA_CLOCK_FREQ;
uint32_t LETIMER0_CLOCK_FREQ;
uint32_t LFB_CLOCK_FREQ;
uint32_t LEUART0_CLOCK_FREQ;
uint32_t LFE_CLOCK_FREQ;
uint32_t RTCC_CLOCK_FREQ;

void cmu_init()
{
    // Configure peripherals for the new frequency
    cmu_config_waitstates(16000000);
    msc_config_waitstates(32000000);

    // Configure HF clock tree for the new frequency
    cmu_hf_clock_config(CMU_HFCLKSEL_HF_HFRCO, 1); // HFCLK = HFRCO / 1
    cmu_hfle_clock_config(CMU_HFPRESC_HFCLKLEPRESC_DIV2); // HFCLKLE = HFCLK / 2
    cmu_hfcore_clock_config(1); // HFCORECLK = HFCLK / 1
    cmu_hfper_clock_config(1, 1); // Peripheral clocks enabled, HFPERCLK = HFCLK / 1
    cmu_hfexp_clock_config(1); // HFEXPCLK = HFCLK / 1

    // Calibrate HFRCO for 32 MHz
    cmu_hfrco_config(1, HFRCO_CALIB_32M, 32000000); // HFRCO enabled, coarse tuned for 32 MHz

    // Calibrate AUXHFRCO for 8 MHz
    cmu_auxhfrco_config(1, AUXHFRCO_CALIB_8M, 8000000);

    // Configure LFE clock
    cmu_lfe_clock_config(CMU_LFECLKSEL_LFE_ULFRCO);

    // Disable unused oscillators
    cmu_hfxo_config(0, 0, 0);
    cmu_lfxo_config(0, 0, 0, 0);

    // Update Clocks
    cmu_update_clocks();
}
void cmu_update_clocks()
{
    AUX_CLOCK_FREQ = AUXHFRCO_OSC_FREQ;

    switch(CMU->HFCLKSTATUS & _CMU_HFCLKSTATUS_SELECTED_MASK)
    {
        case CMU_HFCLKSTATUS_SELECTED_HFRCO:
            HFSRC_CLOCK_FREQ = HFRCO_OSC_FREQ;
        break;
        case CMU_HFCLKSTATUS_SELECTED_HFXO:
            HFSRC_CLOCK_FREQ = HFXO_OSC_FREQ;
        break;
        case CMU_HFCLKSTATUS_SELECTED_LFRCO:
            HFSRC_CLOCK_FREQ = LFRCO_OSC_FREQ;
        break;
        case CMU_HFCLKSTATUS_SELECTED_LFXO:
            HFSRC_CLOCK_FREQ = LFXO_OSC_FREQ;
        break;
    }

    HF_CLOCK_FREQ = HFSRC_CLOCK_FREQ / (((CMU->HFPRESC & _CMU_HFPRESC_PRESC_MASK) >> _CMU_HFPRESC_PRESC_SHIFT) + 1);

    switch(CMU->HFPRESC & _CMU_HFPRESC_HFCLKLEPRESC_MASK)
    {
        case CMU_HFPRESC_HFCLKLEPRESC_DIV2:
            HFLE_CLOCK_FREQ = HF_CLOCK_FREQ >> 1;
        break;
        case CMU_HFPRESC_HFCLKLEPRESC_DIV4:
            HFLE_CLOCK_FREQ = HF_CLOCK_FREQ >> 2;
        break;
    }

    HFCORE_CLOCK_FREQ = HF_CLOCK_FREQ / (((CMU->HFCOREPRESC & _CMU_HFCOREPRESC_PRESC_MASK) >> _CMU_HFCOREPRESC_PRESC_SHIFT) + 1);
    HFEXP_CLOCK_FREQ = HF_CLOCK_FREQ / (((CMU->HFEXPPRESC & _CMU_HFEXPPRESC_PRESC_MASK) >> _CMU_HFEXPPRESC_PRESC_SHIFT) + 1);
    HFPER_CLOCK_FREQ = HF_CLOCK_FREQ / (((CMU->HFPERPRESC & _CMU_HFPERPRESC_PRESC_MASK) >> _CMU_HFPERPRESC_PRESC_SHIFT) + 1);

    switch(CMU->DBGCLKSEL & _CMU_DBGCLKSEL_DBG_MASK)
    {
        case CMU_DBGCLKSEL_DBG_AUXHFRCO:
            DBG_CLOCK_FREQ = AUX_CLOCK_FREQ;
        break;
        case CMU_DBGCLKSEL_DBG_HFCLK:
            DBG_CLOCK_FREQ = HF_CLOCK_FREQ;
        break;
    }

    switch(CMU->ADCCTRL & _CMU_ADCCTRL_ADC0CLKSEL_MASK)
    {
        case CMU_ADCCTRL_ADC0CLKSEL_DISABLED:
            ADC0_CLOCK_FREQ = 0;
        break;
        case CMU_ADCCTRL_ADC0CLKSEL_AUXHFRCO:
            ADC0_CLOCK_FREQ = AUX_CLOCK_FREQ;
        break;
        case CMU_ADCCTRL_ADC0CLKSEL_HFXO:
            ADC0_CLOCK_FREQ = HFXO_OSC_FREQ;
        break;
        case CMU_ADCCTRL_ADC0CLKSEL_HFSRCCLK:
            ADC0_CLOCK_FREQ = HFSRC_CLOCK_FREQ;
        break;
    }

    switch(CMU->LFACLKSEL & _CMU_LFACLKSEL_LFA_MASK)
    {
        case CMU_LFACLKSEL_LFA_DISABLED:
            LFA_CLOCK_FREQ = 0;
        break;
        case CMU_LFACLKSEL_LFA_LFRCO:
            LFA_CLOCK_FREQ = LFRCO_OSC_FREQ;
        break;
        case CMU_LFACLKSEL_LFA_LFXO:
            LFA_CLOCK_FREQ = LFXO_OSC_FREQ;
        break;
        case CMU_LFACLKSEL_LFA_ULFRCO:
            LFA_CLOCK_FREQ = ULFRCO_OSC_FREQ;
        break;
    }

    LETIMER0_CLOCK_FREQ = LFA_CLOCK_FREQ << ((CMU->LFAPRESC0 & _CMU_LFAPRESC0_LETIMER0_MASK) >> _CMU_LFAPRESC0_LETIMER0_SHIFT);

    switch(CMU->LFBCLKSEL & _CMU_LFBCLKSEL_LFB_MASK)
    {
        case CMU_LFBCLKSEL_LFB_DISABLED:
            LFB_CLOCK_FREQ = 0;
        break;
        case CMU_LFBCLKSEL_LFB_LFRCO:
            LFB_CLOCK_FREQ = LFRCO_OSC_FREQ;
        break;
        case CMU_LFBCLKSEL_LFB_LFXO:
            LFB_CLOCK_FREQ = LFXO_OSC_FREQ;
        break;
        case CMU_LFBCLKSEL_LFB_HFCLKLE:
            LFB_CLOCK_FREQ = HFLE_CLOCK_FREQ;
        break;
        case CMU_LFBCLKSEL_LFB_ULFRCO:
            LFB_CLOCK_FREQ = ULFRCO_OSC_FREQ;
        break;
    }

    LEUART0_CLOCK_FREQ = LFB_CLOCK_FREQ << ((CMU->LFBPRESC0 & _CMU_LFBPRESC0_LEUART0_MASK) >> _CMU_LFBPRESC0_LEUART0_SHIFT);

    switch(CMU->LFECLKSEL & _CMU_LFECLKSEL_LFE_MASK)
    {
        case CMU_LFECLKSEL_LFE_DISABLED:
            LFE_CLOCK_FREQ = 0;
        break;
        case CMU_LFECLKSEL_LFE_LFRCO:
            LFE_CLOCK_FREQ = LFRCO_OSC_FREQ;
        break;
        case CMU_LFECLKSEL_LFE_LFXO:
            LFE_CLOCK_FREQ = LFXO_OSC_FREQ;
        break;
        case CMU_LFECLKSEL_LFE_ULFRCO:
            LFE_CLOCK_FREQ = ULFRCO_OSC_FREQ;
        break;
    }

    RTCC_CLOCK_FREQ = LFE_CLOCK_FREQ << ((CMU->LFEPRESC0 & _CMU_LFEPRESC0_RTCC_MASK) >> _CMU_LFEPRESC0_RTCC_SHIFT);
}
void cmu_config_waitstates(uint32_t ulFrequency)
{
    if(ulFrequency <= 32000000)
        CMU->CTRL &= ~CMU_CTRL_WSHFLE;
    else
        CMU->CTRL |= CMU_CTRL_WSHFLE;
}

void cmu_clkout0_config(uint32_t ulSource, int8_t bLocation)
{
    if(bLocation > AFCHANLOC_MAX)
        return;

    if(bLocation < 0) // Disable
    {
        CMU->ROUTEPEN &= ~CMU_ROUTEPEN_CLKOUT0PEN;

        CMU->CTRL = (CMU->CTRL & ~_CMU_CTRL_CLKOUTSEL0_MASK) | CMU_CTRL_CLKOUTSEL0_DISABLED;

        return;
    }

    CMU->CTRL = (CMU->CTRL & ~_CMU_CTRL_CLKOUTSEL0_MASK) | (ulSource & _CMU_CTRL_CLKOUTSEL0_MASK);

    CMU->ROUTELOC0 = (CMU->ROUTELOC0 & ~_CMU_ROUTELOC0_CLKOUT0LOC_MASK) | ((uint32_t)bLocation << _CMU_ROUTELOC0_CLKOUT0LOC_SHIFT);
    CMU->ROUTEPEN |= CMU_ROUTEPEN_CLKOUT0PEN;
}
void cmu_clkout1_config(uint32_t ulSource, int8_t bLocation)
{
    if(bLocation > AFCHANLOC_MAX)
        return;

    if(bLocation < 0) // Disable
    {
        CMU->ROUTEPEN &= ~CMU_ROUTEPEN_CLKOUT1PEN;

        CMU->CTRL = (CMU->CTRL & ~_CMU_CTRL_CLKOUTSEL1_MASK) | CMU_CTRL_CLKOUTSEL1_DISABLED;

        return;
    }

    CMU->CTRL = (CMU->CTRL & ~_CMU_CTRL_CLKOUTSEL1_MASK) | (ulSource & _CMU_CTRL_CLKOUTSEL1_MASK);

    CMU->ROUTELOC0 = (CMU->ROUTELOC0 & ~_CMU_ROUTELOC0_CLKOUT1LOC_MASK) | ((uint32_t)bLocation << _CMU_ROUTELOC0_CLKOUT1LOC_SHIFT);
    CMU->ROUTEPEN |= CMU_ROUTEPEN_CLKOUT1PEN;
}

void cmu_hfrco_config(uint8_t ubEnable, uint32_t ulCalibration, uint32_t ulFrequency)
{
    if(!ubEnable && (CMU->HFCLKSTATUS & _CMU_HFCLKSTATUS_SELECTED_MASK) != CMU_HFCLKSTATUS_SELECTED_HFRCO)
    {
        CMU->OSCENCMD = CMU_OSCENCMD_HFRCODIS;
        while(CMU->STATUS & CMU_STATUS_HFRCOENS);

        return;
    }

    if(!ulFrequency)
        return;

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_HFRCOBSY);

    CMU->HFRCOCTRL = ulCalibration & _CMU_HFRCOCTRL_MASK;

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_HFRCOBSY);

    if(ubEnable && !(CMU->STATUS & CMU_STATUS_HFRCOENS))
    {
        CMU->OSCENCMD = CMU_OSCENCMD_HFRCOEN;

        while(!(CMU->STATUS & CMU_STATUS_HFRCORDY));
    }

    HFRCO_OSC_FREQ = ulFrequency;
}

void cmu_auxhfrco_config(uint8_t ubEnable, uint32_t ulCalibration, uint32_t ulFrequency)
{
    if(!ubEnable)
    {
        CMU->OSCENCMD = CMU_OSCENCMD_AUXHFRCODIS;
        while(CMU->STATUS & CMU_STATUS_AUXHFRCOENS);

        return;
    }

    if(!ulFrequency)
        return;

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_AUXHFRCOBSY);

    CMU->AUXHFRCOCTRL = ulCalibration & _CMU_AUXHFRCOCTRL_MASK;

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_AUXHFRCOBSY);

    if(ubEnable && !(CMU->STATUS & CMU_STATUS_AUXHFRCOENS))
    {
        CMU->OSCENCMD = CMU_OSCENCMD_AUXHFRCOEN;

        while(!(CMU->STATUS & CMU_STATUS_AUXHFRCORDY));
    }

    AUXHFRCO_OSC_FREQ = ulFrequency;
}

void cmu_hfxo_config(uint8_t ubEnable, uint32_t ulConfig, uint32_t ulFrequency)
{
    if(!ubEnable && (CMU->HFCLKSTATUS & _CMU_HFCLKSTATUS_SELECTED_MASK) != CMU_HFCLKSTATUS_SELECTED_HFXO)
    {
        CMU->OSCENCMD = CMU_OSCENCMD_HFXODIS;
        while(CMU->STATUS & CMU_STATUS_HFXOENS);

        return;
    }

    if(!ulFrequency)
        return;

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_HFXOBSY);

    CMU->HFXOCTRL = ulConfig & _CMU_HFXOCTRL_MASK;

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_HFXOBSY);

    if(ubEnable && !(CMU->STATUS & CMU_STATUS_HFXOENS))
    {
        CMU->OSCENCMD = CMU_OSCENCMD_HFXOEN;

        while(!(CMU->STATUS & CMU_STATUS_HFXORDY));
    }

    HFXO_OSC_FREQ = ulFrequency;
}
void cmu_hfxo_timeout_config(uint32_t ulPeakDetTimeout, uint32_t ulSteadyTimeout, uint32_t ulStartupTimeout)
{
    while(CMU->SYNCBUSY & CMU_SYNCBUSY_HFXOBSY);

    CMU->HFXOTIMEOUTCTRL = (ulPeakDetTimeout & _CMU_HFXOTIMEOUTCTRL_PEAKDETTIMEOUT_MASK) | (ulSteadyTimeout & _CMU_HFXOTIMEOUTCTRL_STEADYTIMEOUT_MASK) | (ulStartupTimeout & _CMU_HFXOTIMEOUTCTRL_STARTUPTIMEOUT_MASK);

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_HFXOBSY);
}
void cmu_hfxo_startup_config(float fCurrent, float fCapacitance)
{
    if(fCurrent < HFXO_MIN_UA || fCurrent > HFXO_MAX_UA)
        return;

    if(fCapacitance < HFXO_MIN_PF || fCapacitance > HFXO_MAX_PF)
        return;

    uint32_t ulCTune = HFXO_PF_TO_CTUNE(fCapacitance);
    uint32_t ulIBTrim = HFXO_UA_TO_IBTRIM(fCurrent);

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_HFXOBSY);

    CMU->HFXOSTARTUPCTRL = (CMU->HFXOSTARTUPCTRL & ~(_CMU_HFXOSTARTUPCTRL_CTUNE_MASK | _CMU_HFXOSTARTUPCTRL_IBTRIMXOCORE_MASK)) | ((ulCTune << _CMU_HFXOSTARTUPCTRL_CTUNE_SHIFT) & _CMU_HFXOSTARTUPCTRL_CTUNE_MASK) | ((ulIBTrim << _CMU_HFXOSTARTUPCTRL_IBTRIMXOCORE_SHIFT) & _CMU_HFXOSTARTUPCTRL_IBTRIMXOCORE_MASK);
}
float cmu_hfxo_get_startup_current()
{
    return HFXO_IBTRIM_TO_UA((CMU->HFXOSTARTUPCTRL & _CMU_HFXOSTARTUPCTRL_IBTRIMXOCORE_MASK) >> _CMU_HFXOSTARTUPCTRL_IBTRIMXOCORE_SHIFT);
}
float cmu_hfxo_get_startup_cap()
{
    return HFXO_CTUNE_TO_PF((CMU->HFXOSTARTUPCTRL & _CMU_HFXOSTARTUPCTRL_CTUNE_MASK) >> _CMU_HFXOSTARTUPCTRL_CTUNE_SHIFT);
}
void cmu_hfxo_steady_config(float fCurrent, float fCapacitance)
{
    if(fCurrent < HFXO_MIN_UA || fCurrent > HFXO_MAX_UA)
        return;

    if(fCapacitance < HFXO_MIN_PF || fCapacitance > HFXO_MAX_PF)
        return;

    uint32_t ulCTune = HFXO_PF_TO_CTUNE(fCapacitance);
    uint32_t ulIBTrim = HFXO_UA_TO_IBTRIM(fCurrent);

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_HFXOBSY);

    CMU->HFXOSTEADYSTATECTRL = (CMU->HFXOSTEADYSTATECTRL & ~(_CMU_HFXOSTEADYSTATECTRL_CTUNE_MASK | _CMU_HFXOSTEADYSTATECTRL_IBTRIMXOCORE_MASK)) | ((ulCTune << _CMU_HFXOSTEADYSTATECTRL_CTUNE_SHIFT) & _CMU_HFXOSTEADYSTATECTRL_CTUNE_MASK) | ((ulIBTrim << _CMU_HFXOSTEADYSTATECTRL_IBTRIMXOCORE_SHIFT) & _CMU_HFXOSTEADYSTATECTRL_IBTRIMXOCORE_MASK);
}
float cmu_hfxo_get_steady_current()
{
    return HFXO_IBTRIM_TO_UA((CMU->HFXOSTEADYSTATECTRL & _CMU_HFXOSTEADYSTATECTRL_IBTRIMXOCORE_MASK) >> _CMU_HFXOSTEADYSTATECTRL_IBTRIMXOCORE_SHIFT);
}
float cmu_hfxo_get_steady_cap()
{
    return HFXO_CTUNE_TO_PF((CMU->HFXOSTEADYSTATECTRL & _CMU_HFXOSTEADYSTATECTRL_CTUNE_MASK) >> _CMU_HFXOSTEADYSTATECTRL_CTUNE_SHIFT);
}
float cmu_hfxo_get_pda_current(uint8_t ubTrigger)
{
    if(!(CMU->STATUS & CMU_STATUS_HFXOENS))
        return 0.f;

    if(ubTrigger)
    {
        CMU->CMD = CMU_CMD_HFXOPEAKDETSTART;
        while(!(CMU->STATUS & CMU_STATUS_HFXOPEAKDETRDY));
    }

    uint32_t ulIBTrim = (CMU->HFXOTRIMSTATUS & _CMU_HFXOTRIMSTATUS_IBTRIMXOCORE_MASK) >> _CMU_HFXOTRIMSTATUS_IBTRIMXOCORE_SHIFT;

    return HFXO_IBTRIM_TO_UA(ulIBTrim);
}

void cmu_hf_clock_config(uint32_t ulSource, uint8_t ubPrescaler)
{
    if(ubPrescaler)
        CMU->HFPRESC = (CMU->HFPRESC & ~_CMU_HFPRESC_HFCLKLEPRESC_MASK) | (((ubPrescaler - 1) << _CMU_HFPRESC_HFCLKLEPRESC_SHIFT) & _CMU_HFPRESC_HFCLKLEPRESC_MASK);

    CMU->HFCLKSEL = (ulSource & _CMU_HFCLKSEL_HF_MASK);

    while((CMU->HFCLKSTATUS & _CMU_HFCLKSTATUS_SELECTED_MASK) != (ulSource & _CMU_HFCLKSTATUS_MASK));
}
void cmu_hfle_clock_config(uint32_t ulPrescaler)
{
    CMU->HFPRESC = (CMU->HFPRESC & ~_CMU_HFPRESC_HFCLKLEPRESC_MASK) | (ulPrescaler & _CMU_HFPRESC_HFCLKLEPRESC_MASK);
}
void cmu_hfcore_clock_config(uint8_t ubPrescaler)
{
    if(ubPrescaler)
        CMU->HFCOREPRESC = ((ubPrescaler - 1) << _CMU_HFCOREPRESC_PRESC_SHIFT) & _CMU_HFCOREPRESC_PRESC_MASK;
}
void cmu_hfper_clock_config(uint8_t ubEnable, uint8_t ubPrescaler)
{
    if(!ubEnable)
    {
        CMU->CTRL &= ~CMU_CTRL_HFPERCLKEN;

        return;
    }

    if(ubPrescaler)
        CMU->HFPERPRESC = ((ubPrescaler - 1) << _CMU_HFPERPRESC_PRESC_SHIFT) & _CMU_HFPERPRESC_PRESC_MASK;

    CMU->CTRL |= CMU_CTRL_HFPERCLKEN;
}
void cmu_hfexp_clock_config(uint8_t ubPrescaler)
{
    CMU->HFEXPPRESC = ((ubPrescaler - 1) << _CMU_HFEXPPRESC_PRESC_SHIFT) & _CMU_HFEXPPRESC_PRESC_MASK;
}
void cmu_dbg_clock_config(uint32_t ulSource)
{
    CMU->DBGCLKSEL = ulSource & _CMU_DBGCLKSEL_DBG_MASK;
}
void cmu_adc0_clock_config(uint32_t ulSource, uint8_t ubInvert)
{
    CMU->ADCCTRL = (CMU->ADCCTRL & ~(_CMU_ADCCTRL_ADC0CLKSEL_MASK | _CMU_ADCCTRL_ADC0CLKINV_MASK)) | (ulSource & _CMU_ADCCTRL_ADC0CLKSEL_MASK) | (ubInvert ? CMU_ADCCTRL_ADC0CLKINV : 0);
}

void cmu_hfbus_clock_gate(uint32_t ulPeripheral, uint8_t ubEnable)
{
    if(ubEnable)
        CMU->HFBUSCLKEN0 |= (ulPeripheral & _CMU_HFBUSCLKEN0_MASK);
    else
        CMU->HFBUSCLKEN0 &= ~(ulPeripheral & _CMU_HFBUSCLKEN0_MASK);
}
void cmu_hfper0_clock_gate(uint32_t ulPeripheral, uint8_t ubEnable)
{
    if(ubEnable)
        CMU->HFPERCLKEN0 |= (ulPeripheral & _CMU_HFPERCLKEN0_MASK);
    else
        CMU->HFPERCLKEN0 &= ~(ulPeripheral & _CMU_HFPERCLKEN0_MASK);
}

void cmu_lfrco_config(uint8_t ubEnable, uint32_t ulConfig)
{
    if(!ubEnable)
    {
        CMU->OSCENCMD = CMU_OSCENCMD_LFRCODIS;
        while(CMU->STATUS & CMU_STATUS_LFRCOENS);

        return;
    }

    ulConfig &= _CMU_LFRCOCTRL_TIMEOUT_MASK | _CMU_LFRCOCTRL_ENDEM_MASK | _CMU_LFRCOCTRL_ENCHOP_MASK | _CMU_LFRCOCTRL_ENVREF_MASK;

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_LFRCOBSY);

    CMU->LFRCOCTRL = (CMU->LFRCOCTRL & ~(_CMU_LFRCOCTRL_TIMEOUT_MASK | _CMU_LFRCOCTRL_ENDEM_MASK | _CMU_LFRCOCTRL_ENCHOP_MASK | _CMU_LFRCOCTRL_ENVREF_MASK)) | ulConfig;

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_LFRCOBSY);

    CMU->OSCENCMD = CMU_OSCENCMD_LFRCOEN;
    while(!(CMU->STATUS & CMU_STATUS_LFRCORDY));
}

void cmu_lfxo_config(uint8_t ubEnable, uint32_t ulConfig, float fCapacitance, uint32_t ulFrequency)
{
    if(!ubEnable)
    {
        CMU->OSCENCMD = CMU_OSCENCMD_LFXODIS;
        while(CMU->STATUS & CMU_STATUS_LFXOENS);

        return;
    }

    if(!ulFrequency)
        return;

    if(fCapacitance < LFXO_MIN_PF || fCapacitance > LFXO_MAX_PF)
        return;

    if(CMU->STATUS & CMU_STATUS_LFXOENS)
    {
        CMU->OSCENCMD = CMU_OSCENCMD_LFXODIS;
        while(CMU->STATUS & CMU_STATUS_LFXOENS);
    }

    float fCLoad = fCapacitance / 2.f;
    uint8_t ubCTune = LFXO_PF_TO_CTUNE(fCapacitance);
    uint8_t ubGain = 0;

    if(fCLoad <= 6.f)
        ubGain = 0;
    else if(fCLoad <= 8.f)
        ubGain = 1;
    else if(fCLoad <= 12.5f)
        ubGain = 2;
    else
        ubGain = 3;

    ulConfig &= _CMU_LFXOCTRL_MODE_MASK | _CMU_LFXOCTRL_HIGHAMPL_MASK | _CMU_LFXOCTRL_AGC_MASK | _CMU_LFXOCTRL_TIMEOUT_MASK;

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_LFXOBSY);

    CMU->LFXOCTRL = (CMU->LFXOCTRL & ~(_CMU_LFXOCTRL_GAIN_MASK | _CMU_LFXOCTRL_TUNING_MASK | _CMU_LFXOCTRL_MODE_MASK | _CMU_LFXOCTRL_HIGHAMPL_MASK | _CMU_LFXOCTRL_AGC_MASK | _CMU_LFXOCTRL_TIMEOUT_MASK)) | ulConfig | ((uint32_t)ubGain << _CMU_LFXOCTRL_GAIN_SHIFT) | (((uint32_t)ubCTune << _CMU_LFXOCTRL_TUNING_SHIFT) & _CMU_LFXOCTRL_TUNING_MASK);

    while(CMU->SYNCBUSY & CMU_SYNCBUSY_LFXOBSY);

    CMU->OSCENCMD = CMU_OSCENCMD_LFXOEN;
    while(!(CMU->STATUS & CMU_STATUS_LFXORDY));

    LFXO_OSC_FREQ = ulFrequency;
}
float cmu_lfxo_get_cap()
{
    return LFXO_CTUNE_TO_PF((CMU->LFXOCTRL & _CMU_LFXOCTRL_TUNING_MASK) >> _CMU_LFXOCTRL_TUNING_SHIFT);
}

void cmu_lfa_clock_config(uint32_t ulSource)
{
    CMU->LFACLKSEL = ulSource & _CMU_LFACLKSEL_MASK;
}
void cmu_lfa_letimer0_clock_config(uint8_t ubEnable, uint32_t ulPrescaler)
{
    if(!ubEnable)
    {
        CMU->LFACLKEN0 &= ~CMU_LFACLKEN0_LETIMER0;

        return;
    }

    CMU->LFAPRESC0 = (CMU->LFAPRESC0 & ~_CMU_LFAPRESC0_LETIMER0_MASK) | (ulPrescaler & _CMU_LFAPRESC0_LETIMER0_MASK);
    CMU->LFACLKEN0 |= CMU_LFACLKEN0_LETIMER0;
}
void cmu_lfb_clock_config(uint32_t ulSource)
{
    CMU->LFBCLKSEL = ulSource & _CMU_LFBCLKSEL_MASK;
}
void cmu_lfb_leuart0_clock_config(uint8_t ubEnable, uint32_t ulPrescaler)
{
    if(!ubEnable)
    {
        CMU->LFBCLKEN0 &= ~CMU_LFBCLKEN0_LEUART0;

        return;
    }

    CMU->LFBPRESC0 = (CMU->LFBPRESC0 & ~_CMU_LFBPRESC0_LEUART0_MASK) | (ulPrescaler & _CMU_LFBPRESC0_LEUART0_MASK);
    CMU->LFBCLKEN0 |= CMU_LFBCLKEN0_LEUART0;
}
void cmu_lfe_clock_config(uint32_t ulSource)
{
    CMU->LFECLKSEL = ulSource & _CMU_LFECLKSEL_MASK;
}
void cmu_lfe_rtcc_clock_config(uint8_t ubEnable, uint32_t ulPrescaler)
{
    if(!ubEnable)
    {
        CMU->LFECLKEN0 &= ~CMU_LFECLKEN0_RTCC;

        return;
    }

    CMU->LFEPRESC0 = (CMU->LFEPRESC0 & ~_CMU_LFEPRESC0_RTCC_MASK) | (ulPrescaler & _CMU_LFEPRESC0_RTCC_MASK);
    CMU->LFECLKEN0 |= CMU_LFECLKEN0_RTCC;
}
