#include "light_tracking.h"
#include "light_sensor.h"
#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <stdio.h>

/* Synthetic table for interpolation tests ONLY. Not hardware calibration. */
static const LIGHT_CalPoint_t Table[]={{-0.6f,-0.5f},{0.0f,0.0f},{0.4f,0.3f}};

int main(void)
{
    LIGHT_Config_t c;
    LIGHT_Filter_t f;
    LIGHT_DataPayload_t p;
    LIGHT_Sensor_t sensor;
    LIGHT_DefaultConfig(&c);
    LIGHT_ResetFilter(&f);
    c.filter_alpha=1.0f;
    for (int step=-10;step<=10;++step)
        for (int scale=1;scale<=5;++scale)
        {
            float d=(float)step*0.1f;
            LIGHT_Estimate(&c,&f,100.0f*scale*(1-d),100.0f*scale*(1+d),LIGHT_STATUS_OK,&p);
            assert(p.error_valid && !p.valid && p.status==LIGHT_STATUS_NO_CALIBRATION);
            assert(fabsf(p.normalized_diff-d)<0.00001f && p.bearing_rad==0.0f);
        }
    c.right_gain=0.8f;
    LIGHT_Estimate(&c,&f,700,875,LIGHT_STATUS_OK,&p);
    assert(p.error_valid && fabsf(p.normalized_diff)<0.00001f);
    c.right_gain=1;
    c.left_offset_lux=50; c.right_offset_lux=80;
    LIGHT_Estimate(&c,&f,750,780,LIGHT_STATUS_OK,&p);
    assert(p.error_valid && fabsf(p.normalized_diff)<0.00001f);
    LIGHT_DefaultConfig(&c);
    LIGHT_Estimate(&c,&f,0,0,LIGHT_STATUS_OK,&p);
    assert(!p.error_valid && !p.valid && p.status==LIGHT_STATUS_DARK && !f.initialized);
    for (uint8_t status=LIGHT_STATUS_IO_ERROR;status<=LIGHT_STATUS_NOT_READY;++status)
    {
        LIGHT_Estimate(&c,&f,700,700,status,&p);
        assert(!p.error_valid && !p.valid && p.status==status && !f.initialized);
    }
    LIGHT_Estimate(&c,&f,700,700,LIGHT_STATUS_SATURATED,&p);
    assert(!p.error_valid && p.status==LIGHT_STATUS_SATURATED);
    LIGHT_Estimate(&c,&f,NAN,700,LIGHT_STATUS_OK,&p);
    assert(!p.error_valid && p.status==LIGHT_STATUS_BAD_INPUT);
    LIGHT_Estimate(&c,&f,-1,700,LIGHT_STATUS_OK,&p);
    assert(!p.error_valid && p.status==LIGHT_STATUS_BAD_INPUT);
    c.filter_alpha=0;
    LIGHT_Estimate(&c,&f,700,700,LIGHT_STATUS_OK,&p);
    assert(!p.error_valid && p.status==LIGHT_STATUS_BAD_INPUT);
    LIGHT_DefaultConfig(&c);
    LIGHT_ResetFilter(&f);
    LIGHT_Estimate(&c,&f,700,700,LIGHT_STATUS_OK,&p);
    LIGHT_Estimate(&c,&f,500,900,LIGHT_STATUS_OK,&p);
    assert(p.error_valid && f.left_filtered==600 && f.right_filtered==800);
    LIGHT_Estimate(&c,&f,700,700,LIGHT_STATUS_IO_ERROR,&p);
    LIGHT_Estimate(&c,&f,500,900,LIGHT_STATUS_OK,&p);
    assert(p.error_valid && f.left_filtered==500 && f.right_filtered==900);
    c.filter_alpha=1;
    c.calibration=Table; c.calibration_count=3;
    LIGHT_ResetFilter(&f);
    LIGHT_Estimate(&c,&f,800,1200,LIGHT_STATUS_OK,&p); /* d=+0.2 -> rad=+0.15 */
    assert(p.valid && p.error_valid && fabsf(p.bearing_rad-0.15f)<0.00001f);
    LIGHT_Estimate(&c,&f,1300,700,LIGHT_STATUS_OK,&p); /* d=-0.3 -> rad=-0.25 */
    assert(p.valid && fabsf(p.bearing_rad+0.25f)<0.00001f);
    LIGHT_Estimate(&c,&f,600,1400,LIGHT_STATUS_OK,&p); /* upper endpoint */
    assert(p.valid && fabsf(p.bearing_rad-0.3f)<0.00001f);
    LIGHT_Estimate(&c,&f,0,1400,LIGHT_STATUS_OK,&p); /* no extrapolation */
    assert(!p.valid && p.error_valid && p.status==LIGHT_STATUS_OUT_OF_VIEW && !f.initialized);
    {
        const LIGHT_CalPoint_t bad[]={{0,0},{0,0.1f}};
        c.calibration=bad; c.calibration_count=2;
        LIGHT_Estimate(&c,&f,700,700,LIGHT_STATUS_OK,&p);
        assert(!p.valid && !p.error_valid && p.status==LIGHT_STATUS_BAD_INPUT);
    }
    c.calibration=Table; c.calibration_count=1;
    LIGHT_Estimate(&c,&f,700,700,LIGHT_STATUS_OK,&p);
    assert(!p.error_valid && p.status==LIGHT_STATUS_BAD_INPUT);
    LIGHT_DefaultConfig(&c);
    LIGHT_ResetFilter(&f);
    LIGHT_Estimate(&c,&f,0,1000,LIGHT_STATUS_OK,&p);
    c.calibration=Table; c.calibration_count=3;
    LIGHT_Estimate(&c,&f,500,500,LIGHT_STATUS_OK,&p);
    assert(!p.valid && p.error_valid && p.status==LIGHT_STATUS_OUT_OF_VIEW && !f.initialized);
    LIGHT_DefaultConfig(&c);
    LIGHT_ResetFilter(&f);
    LIGHT_SensorInit(&sensor);
    assert(!sensor.hardware);
    for (int i=0;i<100;++i)
    {
        float left,right;
        uint8_t status=LIGHT_SensorRead(&sensor,&left,&right);
        LIGHT_Estimate(&c,&f,left,right,status,&p);
        assert(p.error_valid && !p.valid && p.status==LIGHT_STATUS_NO_CALIBRATION);
    }
    LIGHT_SensorClose(&sensor);
    puts("PASS: 105 ratio/intensity cases; no invented angle; LUT/interpolation/range; faults; EMA; simulation");
    return 0;
}
