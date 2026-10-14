#include "cfe_tbl_filedef.h"
#include "sch_lab_tbl.h"
#include "cfe_sb.h"
#include "light_app_msgids.h"

SCH_LAB_ScheduleTable_t Schedule = {
    .TickRate = 10,
    .Config = {{CFE_SB_MSGID_WRAP_VALUE(LIGHT_APP_WAKEUP_MID), 2, 0}}
};
CFE_TBL_FILEDEF(Schedule, SCH_LAB.Schedule, LIGHT 5Hz wakeup, sch_lab_table.tbl)
