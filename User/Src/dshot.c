//
// Created by xie on 2025/9/22.
//

#include "dshot.h"

uint32_t dmaBurstBuffer[DSHOT_DMA_BUFFER_SIZE * 4];
uint16_t count = 0;

/// @brief Get dshot frequency
/// Unused
/// @param pwmProtocolType Dshot protocol type
/// @return dshot frequency
uint32_t getDshotHz(motorProtocolTypes_e pwmProtocolType) {
    switch (pwmProtocolType) {
        case (MOTOR_PROTOCOL_DSHOT600):
            return MOTOR_DSHOT600_HZ;
        case (MOTOR_PROTOCOL_DSHOT300):
            return MOTOR_DSHOT300_HZ;
        case (MOTOR_PROTOCOL_DSHOT150):
            return MOTOR_DSHOT150_HZ;
        default:
            return MOTOR_DSHOT600_HZ;
    }
}

/// @brief Configure the dshot timer
/// Unused
void dshotTimerConfig() {
    // uint16_t period = MOTOR_BITLENGTH - 1;
    // uint16_t prescaler = lrintf((float) timer_freq / count_freq + 0.01f) - 1;
}

/// @brief Start the dshot timer
void dshotTimerStart(void) {
    HAL_TIM_PWM_Start(&MOTOR1_TIM, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&MOTOR1_TIM, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&MOTOR1_TIM, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&MOTOR1_TIM, TIM_CHANNEL_4);
}

/// @brief To check CRC and prepare the dshot packet
/// @param value
/// The throttle value ranges from 0 to 2047, with values from 48 to 2047 representing throttle levels from 0% to 100%.
/// Values from 0 to 47 are reserved for special commands.
/// @param requestTelemetry
/// Whether to request the return of data
/// @return Dshot packet
static uint16_t prepareDshotPacket(const uint16_t value, bool requestTelemetry) {
    // throttle is 11 bits, so shift left 1 bit and add telemetry request bit to make 12 bits
    uint16_t packet = (value << 1) | (requestTelemetry ? 1 : 0);
    // checksum is 4 bits, so we need to shift left 4 bits and add it to make 16 bits
    // compute checksum
    int csum = 0;
    int csum_data = packet;
    for (int i = 0; i < 3; i++) {
        csum ^= csum_data;  // xor data by nibbles
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
static void loadDmaBufferDshot(uint32_t *dmabuffer, int stride, uint16_t packet) {
    int i = 0;
    for (i = 0; i < 16; i++) {
        dmabuffer[i * stride] = (packet & 0x8000) ? MOTOR_BIT_1 : MOTOR_BIT_0;  // MSB first
        packet <<= 1;
    }

    dmabuffer[i++ * stride] = 0;  // Add two 0 at the end to reset the signal
    dmabuffer[i++ * stride] = 0;
}

/// @brief DMA complete callback
/// @param hdma DMA handle
static void dshotDmaCpltCallback(DMA_HandleTypeDef *hdma) {
    TIM_HandleTypeDef *htim = (TIM_HandleTypeDef *) ((DMA_HandleTypeDef *) hdma)->Parent;  // Get the timer handle
    if (htim->hdma[TIM_DMA_ID_UPDATE]->Init.Mode == DMA_NORMAL) {
        htim->State = HAL_TIM_STATE_READY;
    }
    HAL_TIM_PeriodElapsedCallback(htim);
}

/// @brief Start the dshot dma
/// @param BurstBaseAddress TIM BASE+TIM_CCRx
/// @param BurstRequestSrc TIM_DMA_UPDATE
/// @param BurstBuffer The buffer to store the pwm signal
/// @param BurstLength TIM_DMABURSTLENGTH_4TRANSFERS
/// @param DataLength DSHOT_DMA_BUFFER_SIZE
/// @return Success or not or busy
static HAL_StatusTypeDef dshotDmaStart(TIM_HandleTypeDef *htim, uint32_t BurstBaseAddress, uint32_t BurstRequestSrc,
                                       const uint32_t *BurstBuffer, uint32_t BurstLength, uint32_t DataLength) {
    if (htim->DMABurstState == HAL_DMA_BURST_STATE_BUSY) {
        // DMA is running
        return HAL_BUSY;
    } else if (htim->DMABurstState == HAL_DMA_BURST_STATE_READY) {
        if (BurstBuffer == NULL && BurstLength > 0u) {
            // Request dma but no buffer
            return HAL_ERROR;
        } else {
            // Set dma busy
            htim->DMABurstState = HAL_DMA_BURST_STATE_BUSY;
        }
    }
    htim->hdma[TIM_DMA_ID_UPDATE]->XferCpltCallback = dshotDmaCpltCallback;  // Set the callback function
    // Start the DMA with interrupt
    if (HAL_DMA_Start_IT(htim->hdma[TIM_DMA_ID_UPDATE], (uint32_t) BurstBuffer, (uint32_t) &htim->Instance->DMAR,
                         DataLength) != HAL_OK) {
        return HAL_ERROR;
    }
    htim->Instance->DMAR = (BurstBaseAddress | BurstLength);  // config the DMA burst mode
    __HAL_TIM_ENABLE_DMA(htim, BurstRequestSrc);
    return HAL_OK;
}

/// @brief Update the dshot channel
/// @param index Motor index
/// @param value Control value,range from 0 to 2047,with values from 48 to 2047 representing throttle levels from 0% to
/// 100%.
/// @param requestTelemetry Whether to request the return of data
/// @return Success or not
bool dshotUpdateChannel(uint8_t index, uint16_t value, bool requestTelemetry) {
    if (index >= 4) {
        return false;
    }
    uint16_t packet = prepareDshotPacket(value, requestTelemetry);
    loadDmaBufferDshot(&(dmaBurstBuffer[index]), 4, packet);
    return true;
}

/// @brief Write the dshot signal
/// @return
bool dshotWrite(void) {
    HAL_StatusTypeDef status = dshotDmaStart(&MOTOR1_TIM, TIM_DMABase_CCR1, TIM_DMA_UPDATE, (uint32_t *) dmaBurstBuffer,
                                             TIM_DMABURSTLENGTH_4TRANSFERS, DSHOT_DMA_BUFFER_SIZE * 4);
    if (status != HAL_OK) {
        return false;
    } else {
        return true;
    }
}

/// @brief Timer period elapsed callback
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == MOTOR1_TIM.Instance) {
        HAL_TIM_DMABurst_WriteStop(&MOTOR1_TIM, TIM_DMA_UPDATE);  // Stop the dma burst
    }
    if (htim->Instance == DSHOT_UPDATE_TIM.Instance) {  // Update dshot signal periodically
        dshotWrite();  // Write the dshot signal
    }
}

/// @brief dshot init
void dshotInit(void) {
    printf("Dshot init start\r\n");
    dshotTimerStart();  // Start the dshot timer
    HAL_TIM_Base_Start_IT(&DSHOT_UPDATE_TIM);  // Start the dshot update timer
    dshotUpdateChannel(0, 0, false);
    dshotUpdateChannel(1, 0, false);
    dshotUpdateChannel(2, 0, false);
    dshotUpdateChannel(3, 0, false);
    HAL_Delay(100);
    printf("Init complete\r\n");
    printf("Enter dshot loop\r\n");
}

/// @brief dshot loop
void dshotLoop(void) {
    uint16_t value = 500;
    dshotUpdateChannel(0, value, false);
    dshotUpdateChannel(1, value, false);
    dshotUpdateChannel(2, value, false);
    dshotUpdateChannel(3, value, false);
}
