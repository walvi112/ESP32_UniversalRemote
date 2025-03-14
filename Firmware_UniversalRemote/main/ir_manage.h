#ifndef IR_MANAGE_H
#define IR_MANAGE_H
#include "esp_err.h"

#define IR_NAMESPACE        "ir_storage"
#define IR_TV_NUM_REMOTE    5
#define IR_TV_1             "ir_tv_1"
#define IR_TV_2             "ir_tv_2"
#define IR_TV_3             "ir_tv_3"
#define IR_TV_4             "ir_tv_4"
#define IR_TV_5             "ir_tv_5"
#define IR_TV_NUM_CODE          44

enum {
    IR_TV_CODE_ON,
    IR_TV_CODE_SOURCE,
    IR_TV_CODE_1,
    IR_TV_CODE_2,
    IR_TV_CODE_3,
    IR_TV_CODE_4,
    IR_TV_CODE_5,
    IR_TV_CODE_6,
    IR_TV_CODE_7,
    IR_TV_CODE_8,
    IR_TV_CODE_9,
    IR_TV_CODE_DOT,
    IR_TV_CODE_0,
    IR_TV_CODE_PRE,
    IR_TV_CODE_INCREASE,
    IR_TV_CODE_MUTE,
    IR_TV_CODE_CH_UP,
    IR_TV_CODE_DECREASE,
    IR_TV_CODE_LIST,
    IR_TV_CODE_CH_DOWN,
    IR_TV_CODE_BRAND1,
    IR_TV_CODE_HOME,
    IR_TV_CODE_BRAND2,
    IR_TV_CODE_BRAND3,
    IR_TV_CODE_UP,
    IR_TV_CODE_GUIDE,
    IR_TV_CODE_LEFT,
    IR_TV_CODE_ENTER,
    IR_TV_CODE_RIGHT,
    IR_TV_CODE_RETURN,
    IR_TV_CODE_DOWN,
    IR_TV_CODE_EXIT,
    IR_TV_CODE_A,
    IR_TV_CODE_B,
    IR_TV_CODE_C,
    IR_TV_CODE_D,
    IR_TV_CODE_SETTINGS,
    IR_TV_CODE_INFO,
    IR_TV_CODE_CC,
    IR_TV_CODE_STOP,
    IR_TV_CODE_PREVIOUS,
    IR_TV_CODE_RESUME,
    IR_TV_CODE_PAUSE,
    IR_TV_CODE_NEXT,
};

typedef struct ir_code_t {
    uint16_t address;
    uint16_t command;
    
} ir_code_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ir_storage_init(void);


#ifdef __cplusplus
}
#endif 

#endif