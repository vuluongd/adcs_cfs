#ifndef ADCS_APP_LIGHT_H
#define ADCS_APP_LIGHT_H
#include "cfe.h"
void ADCS_APP_ReceiveLight(const CFE_SB_Buffer_t *buffer);
/* Call at EVERY control tick. false => do not use old light bearing in PID. */
bool ADCS_APP_LightUsable(bool allow_simulated);
/* Dimensionless light-error signal; separate from calibrated angle validity. */
bool ADCS_APP_LightErrorUsable(bool allow_simulated);
#endif
