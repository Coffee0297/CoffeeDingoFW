#include "neopixels.h"

// TIM1 Update event → DMA2 Stream 5 Channel 6 (per STM32F4 DMA request mapping)
#define NEO_DMA_STREAM_ID  STM32_DMA_STREAM_ID(2, 5)
#define NEO_DMA_CHANNEL    6
#define NEO_DMA_MODE      (STM32_DMA_CR_CHSEL(NEO_DMA_CHANNEL) | \
                           STM32_DMA_CR_PL(2)                   | \
                           STM32_DMA_CR_DIR_M2P                 | \
                           STM32_DMA_CR_PSIZE_HWORD             | \
                           STM32_DMA_CR_MSIZE_HWORD             | \
                           STM32_DMA_CR_MINC                    | \
                           STM32_DMA_CR_TCIE)

NeoPixels::NeoPixels(uint8_t numPixels, PWMDriver *pwmDriver,
                     const PWMConfig *pwmCfg, PwmChannel pwmCh)
    : m_numPixels(numPixels), m_pwmDriver(pwmDriver),
      m_pwmCfg(pwmCfg), m_pwmCh(pwmCh),
      m_dmaBuffer{}, m_dmaStream(nullptr), m_busy(false), m_initialized(false)
{
}

void NeoPixels::init() {
    m_dmaStream = dmaStreamAlloc(NEO_DMA_STREAM_ID, 6, dmaCallback, this);
    osalDbgAssert(m_dmaStream != nullptr, "DMA2 Stream5 already in use");
    pwmStart(m_pwmDriver, m_pwmCfg);
    pwmEnableChannel(m_pwmDriver, (pwmchannel_t)m_pwmCh, 0);
    m_initialized = true;
}

void NeoPixels::buildBuffer() {
    uint16_t idx = 0;
    for (uint8_t p = 0; p < m_numPixels; p++) {
        // WS2812B expects GRB order, MSB first
        uint32_t grb = pixels[p].GetBytes();
        for (int8_t bit = 23; bit >= 0; bit--)
            m_dmaBuffer[idx++] = (grb & (1u << bit)) ? NEO_T1H : NEO_T0H;
    }
    // Entries from idx onward remain 0 (reset pulse — CCR=0 holds output low)
}

void NeoPixels::Update() {
    if (m_busy) return;

    if (!m_initialized)
        init();

    for (uint8_t i = 0; i < m_numPixels; i++)
        pixels[i].Update();

    buildBuffer();
    m_busy = true;

    dmaStreamSetPeripheral(m_dmaStream, &m_pwmDriver->tim->CCR[(pwmchannel_t)m_pwmCh]);
    dmaStreamSetMemory0(m_dmaStream, m_dmaBuffer);
    dmaStreamSetTransactionSize(m_dmaStream, (uint16_t)(m_numPixels * 24 + NEO_RST));
    dmaStreamSetMode(m_dmaStream, NEO_DMA_MODE);
    // Reset timer counter so DMA starts on a known boundary, preventing a
    // phantom bit from an in-progress timer period shifting the bit stream.
    m_pwmDriver->tim->CNT = 0;
    dmaStreamEnable(m_dmaStream);
}

void NeoPixels::dmaCallback(void *p, uint32_t flags) {
    (void)flags;
    NeoPixels *self = static_cast<NeoPixels*>(p);
    dmaStreamDisable(self->m_dmaStream);
    self->m_busy = false;
    // Timer keeps running at CCR=0 (output low) until next Update()
}
