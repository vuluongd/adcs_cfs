#include "light_tracking.h"
#include "light_sensor.h"
#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    LIGHT_Config_t c;
    LIGHT_Filter_t f;
    LIGHT_DataPayload_t p;
    const LIGHT_PinConfig_t *pins;
    float angle,left,right,truth;
    LIGHT_DefaultConfig(&c);
    assert(LIGHT_AngleFromError(&c,0,&angle)==LIGHT_STATUS_NO_CALIBRATION);
    assert(LIGHT_AngleFromError(NULL,0,&angle)==LIGHT_STATUS_BAD_INPUT);
    assert(LIGHT_AngleFromError(&c,NAN,&angle)==LIGHT_STATUS_BAD_INPUT);
    assert(LIGHT_AngleFromError(&c,0,NULL)==LIGHT_STATUS_BAD_INPUT);
    LIGHT_UseSimulationCalibration(&c);
    c.filter_alpha=1;
    LIGHT_ResetFilter(&f);
    for (uint32_t i=0;i<160;++i)
    {
        uint8_t status=LIGHT_SimGenerate(i,LIGHT_SIM_SWEEP,&left,&right,&truth);
        LIGHT_Estimate(&c,&f,left,right,status,&p);
        assert(status==LIGHT_STATUS_OK && p.valid && p.error_valid);
        assert(fabsf(p.bearing_rad-truth)<0.00001f);
    }
    assert(LIGHT_AngleFromError(&c,0.9f,&angle)==LIGHT_STATUS_OUT_OF_VIEW);
    assert(angle==0);
    assert(LIGHT_SimGenerate(0,LIGHT_SIM_DARK,&left,&right,&truth)==LIGHT_STATUS_OK);
    LIGHT_Estimate(&c,&f,left,right,LIGHT_STATUS_OK,&p);
    assert(!p.error_valid && !p.valid && p.status==LIGHT_STATUS_DARK && isnan(truth));
    assert(LIGHT_SimGenerate(0,LIGHT_SIM_IO_ERROR,&left,&right,&truth)==LIGHT_STATUS_IO_ERROR);
    assert(LIGHT_SimGenerate(0,LIGHT_SIM_SATURATED,&left,&right,&truth)==LIGHT_STATUS_SATURATED);
    assert(LIGHT_SimGenerate(0,(LIGHT_SimMode_t)99,&left,&right,&truth)==LIGHT_STATUS_BAD_INPUT);
    assert(LIGHT_SimGenerate(0,LIGHT_SIM_SWEEP,NULL,&right,&truth)==LIGHT_STATUS_BAD_INPUT);
    pins=LIGHT_GetPinConfig(LIGHT_BOARD_RPI3);
    assert(pins && pins->sda_gpio==2 && pins->scl_gpio==3);
    assert(pins->sda_physical==3 && pins->scl_physical==5);
    assert(pins->left_address==0x23 && pins->right_address==0x5c);
    pins=LIGHT_GetPinConfig(LIGHT_BOARD_ESP32_CLASSIC);
    assert(pins && pins->sda_gpio==21 && pins->scl_gpio==22 && pins->linux_device==NULL);
    assert(LIGHT_GetPinConfig((LIGHT_Board_t)99)==NULL);
    puts("PASS: angle API; 160 virtual angle/lux samples; faults; pin profiles");
    return 0;
}
