//
// Created by xie on 2025/9/22.
//

#ifndef RM_DSHOT_DSHOT_H
#define RM_DSHOT_DSHOT_H

//User include
#include "main.h"
#include "stdbool.h"

///For dshot600 on 168MHz F407
///The term of pwm is 1.67us,the bit1 is 1.25us high level, and the bit0 is 0.625us high level
///Todo: Adjust these values according to the actual clock frequency of the timer
#define MOTOR_BIT_0 1
#define MOTOR_BIT_1 1
#define MOTOR_BITLENGTH 1

///Normally, a DSHOT frame contains only 16 bits of data,
///but since the TIM BURST DMA method requires resetting at the end of the frame to prevent continuous signal output,
///two additional bits are added at the end with actual compare values of 0.
#define DSHOT_DMA_BUFFER_SIZE   18

uint32_t motor1_dmaBurstBuffer[DSHOT_DMA_BUFFER_SIZE];
uint32_t motor2_dmaBurstBuffer[DSHOT_DMA_BUFFER_SIZE];

#endif //RM_DSHOT_DSHOT_H