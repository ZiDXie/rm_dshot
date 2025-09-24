//
// Created by xie on 2025/9/22.
//

#include "dshot.h"

uint32_t dmaBurstBuffer[DSHOT_DMA_BUFFER_SIZE*4];

void dshotInit(void)
{}

/// @brief Start the dshot timer
void dshotTimerStart(void)
{
    HAL_TIM_PWM_Start(MOTOR1_TIM, MOTOR1_TIM_CHANNEL);
}

/// @brief To check CRC and prepare the dshot packet
/// @param value
/// The throttle value ranges from 0 to 2047, with values from 48 to 2047 representing throttle levels from 0% to 100%.
/// Values from 0 to 47 are reserved for special commands.
/// @param requestTelemetry
/// Whether to request the return of data
/// @return Dshot packet
static uint16_t prepareDshotPacket(const uint16_t value, bool requestTelemetry)
{
    // throttle is 11 bits, so shift left 1 bit and add telemetry request bit to make 12 bits
    uint16_t packet = (value << 1) | (requestTelemetry ? 1 : 0);
    // checksum is 4 bits, so we need to shift left 4 bits and add it to make 16 bits
    // compute checksum
    int csum = 0;
    int csum_data = packet;
    for (int i = 0; i < 3; i++) {
        csum ^=  csum_data;   // xor data by nibbles
        csum_data >>= 4;
    }
    // Protect lower 4 bits
    csum &= 0xf;
    // append checksum
    packet = (packet << 4) | csum;
    return packet;
}

/// @brief Convert 16 bits packet to 16 pwm signal
/// @param dmabuffer The buffer to store the pwm signal
/// @param packet Dshot packet,use prepareDshotPacket() to get it
static void loadDmaBufferDshot(uint32_t* dmabuffer,int stride,uint16_t packet)
{
    int i=0;
    for(i = 0; i < 16; i++)
    {
        dmabuffer[i*stride] = (packet & 0x8000) ? MOTOR_BIT_1 : MOTOR_BIT_0; //MSB first
        packet <<= 1;
    }

    dmabuffer[i++ * stride] = 0; //Add two 0 at the end to reset the signal
    dmabuffer[i++ * stride] = 0;
}

static HAL_StatusTypeDef dshotDmaStart()
{

}

/// @brief Update the dshot channel
/// @param index Motor index
/// @param value Control value,range from 0 to 2047,with values from 48 to 2047 representing throttle levels from 0% to 100%.
/// @param requestTelemetry Whether to request the return of data
/// @return Success or not
bool dshotUpdateChannel(uint8_t index,uint16_t value,bool requestTelemetry) {
    if (index>=4) {
        return false;
    }
    uint16_t packet = prepareDshotPacket(value, requestTelemetry);
    loadDmaBufferDshot(&dmaBurstBuffer[index],4,packet);
    return true;
}

/// @brief Write the dshot signal
/// @return
bool dshotWrite(void) {
    return true;
}