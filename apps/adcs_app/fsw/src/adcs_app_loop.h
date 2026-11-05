#ifndef ADCS_APP_LOOP_H
#define ADCS_APP_LOOP_H
#include "cfe.h"

/* Gọi từ dispatcher khi nhận WAKEUP_ADCS (10 Hz): chạy 1 bước vòng ngoài P. */
void ADCS_APP_OnWakeup(void);

/* Nhận WHEEL_DATA của ACT_APP (chỉ để giám sát / HK, không nằm trong vòng điều khiển). */
void ADCS_APP_ReceiveWheel(const CFE_SB_Buffer_t *buffer);

/* Áp dụng mode / tham số (gọi từ lệnh). Trả về false nếu bị từ chối. */
bool ADCS_APP_SetMode(uint32 mode);

/* Dừng bánh đà (gửi STOP_WHEEL sang ACT_APP). Dùng khi tắt/thoát. */
void ADCS_APP_StopWheel(void);

/* Điền các trường điều khiển vào HK. */
void ADCS_APP_FillHk(void);

#endif
