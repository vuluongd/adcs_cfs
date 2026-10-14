#define _POSIX_C_SOURCE 200809L
#include "light_tracking.h"
#include "light_sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void PrintResult(const LIGHT_DataPayload_t *p)
{
    printf("L=%.2f R=%.2f error=%+.4f error_valid=%u angle_valid=%u status=%u sim=%u",
           (double)p->left_lux,(double)p->right_lux,(double)p->normalized_diff,
           p->error_valid,p->valid,p->status,p->simulated);
    if (p->valid) printf(" angle=%.3fdeg",(double)p->bearing_rad*57.2957795131);
    else printf(" angle=N/A");
    puts("");
}

int main(int argc, char **argv)
{
    LIGHT_Config_t cfg;
    LIGHT_Filter_t filter;
    LIGHT_DataPayload_t out;
    LIGHT_DefaultConfig(&cfg);
    LIGHT_ResetFilter(&filter);
    if (argc==3 && strcmp(argv[1],"--pins")==0)
    {
        LIGHT_Board_t board;
        const LIGHT_PinConfig_t *pins;
        if (strcmp(argv[2],"rpi3")==0) board=LIGHT_BOARD_RPI3;
        else if (strcmp(argv[2],"esp32")==0) board=LIGHT_BOARD_ESP32_CLASSIC;
        else { fprintf(stderr,"Unknown board: choose rpi3 or esp32\n"); return 2; }
        pins=LIGHT_GetPinConfig(board);
        printf("%s\nSDA=GPIO%d SCL=GPIO%d\n",pins->board_name,pins->sda_gpio,pins->scl_gpio);
        if (pins->sda_physical>=0)
            printf("Physical header: SDA=%d SCL=%d 3V3=%d GND=%d\n",
                   pins->sda_physical,pins->scl_physical,pins->power_physical,pins->ground_physical);
        else puts("Use board labels 3V3/GND; physical connector positions depend on the board.");
        printf("LEFT/1=0x%02X (ADDR low); RIGHT/2=0x%02X (ADDR high)\n",
               pins->left_address,pins->right_address);
        puts("Explicit pin profile; does not detect the actual wires or configure pin mux.");
        return 0;
    }
    if (argc==3)
    {
        char *le,*re;
        float left=strtof(argv[1],&le),right=strtof(argv[2],&re);
        if (*le || *re || le==argv[1] || re==argv[2]) return 2;
        LIGHT_Estimate(&cfg,&filter,left,right,LIGHT_STATUS_OK,&out);
        PrintResult(&out);
        return out.error_valid ? 0 : 1;
    }
    if (argc==2 && strcmp(argv[1],"--i2c")==0)
    {
        LIGHT_Sensor_t sensor;
        struct timespec delay={0,200000000};
        if (setenv("LIGHT_APP_BACKEND","i2c",1)!=0) return 2;
        LIGHT_SensorInit(&sensor);
        for (int i=0;i<60;++i)
        {
            float left,right;
            uint8_t status=LIGHT_SensorRead(&sensor,&left,&right);
            LIGHT_Estimate(&cfg,&filter,left,right,status,&out);
            PrintResult(&out);
            fflush(stdout);
            nanosleep(&delay,NULL);
        }
        LIGHT_SensorClose(&sensor);
        return 0;
    }
    if (argc!=1 && !(argc==2 && strcmp(argv[1],"--sim")==0))
    {
        fprintf(stderr,"Usage: light_demo [left_lux right_lux | --i2c | --sim | --pins rpi3|esp32]\n");
        return 2;
    }
    cfg.filter_alpha=1.0f;
    LIGHT_UseSimulationCalibration(&cfg);
    puts("SIMULATION ONLY: known virtual angles; synthetic table is NOT real BH1750 calibration");
    for (uint32_t step=0;step<=80;step+=10)
    {
        float left,right,true_angle;
        uint8_t status=LIGHT_SimGenerate(step,LIGHT_SIM_SWEEP,&left,&right,&true_angle);
        LIGHT_Estimate(&cfg,&filter,left,right,status,&out);
        out.simulated=1;
        printf("true_angle=%+.3fdeg ",(double)true_angle*57.2957795131);
        PrintResult(&out);
    }
    return 0;
}
