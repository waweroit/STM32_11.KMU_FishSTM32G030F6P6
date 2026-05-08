#include "app_fish_tank.h"

#include "main.h"

#include <string.h>

#define FLASH_MAGIC_16          ((uint16_t)0x7A4D)

#define VA_MAGIC                ((uint16_t)0x1001)
#define VIRTADDR_KEYCOUNT       ((uint16_t)0x1002)
#define VIRTADDR_LOWLEVEL       ((uint16_t)0x1003)

#define APP_BLINK_PERIOD_MS     1000U
#define APP_DEBOUNCE_MS         50U
#define APP_UART_STARTUP_MS     1000U

#undef NB_OF_VAR
#define NB_OF_VAR               3U

typedef struct {
    uint16_t magic;
    uint16_t keyPressedCount;
    bool lowLevelWasReached;
} DataFlash;

typedef struct {
    DataFlash flashData;
    bool lowLevelWasReachedPrev;
    bool disableACRelay;
    bool goToSleepMode;
    bool previousKeyState;
    uint32_t lastKeyEventTick;
    uint32_t blinkLedFromTick;
} AppState;

uint16_t VirtAddVarTab[NB_OF_VAR] = {
    VA_MAGIC,
    VIRTADDR_KEYCOUNT,
    VIRTADDR_LOWLEVEL
};

static AppState app;

static void App_LoadFlashData(DataFlash *out);
static HAL_StatusTypeDef App_SaveFlashData(const DataFlash *in);
static void App_InitFlashData(void);
static void App_ApplyStoredRelayState(void);
static void App_ReadInputs(void);
static void App_HandleKeyPress(void);
static void App_HandleLowWaterDetected(void);
static void App_UpdateLedBlink(void);
static void App_EnterSleepIfAllowed(void);
static void App_SetRelayEnabled(bool enabled);
static bool App_ReadKeyPressed(void);
static bool App_ReadFloatSensorClosed(void);

void App_Init(void)
{
    memset(&app, 0, sizeof(app));
    app.goToSleepMode = true;

    DelayInit();

    Send("Welcome waweroIT\r\n");
    HAL_Delay(APP_UART_STARTUP_MS);

    const uint16_t flashStatus = EE_Init();
    if (flashStatus != EE_OK) {
        Send("Inicjalizacja Flash nie udana...\r\n");
        return;
    }

    App_InitFlashData();
    App_ApplyStoredRelayState();

    Sendf("Odczytane dane.KeyPressedCount: %u\r\n", app.flashData.keyPressedCount);
    Sendf("Odczytany dane.LowLevelWasReached: %u\r\n", app.flashData.lowLevelWasReached ? 1U : 0U);
}

void App_Process(void)
{
    App_EnterSleepIfAllowed();
    App_ReadInputs();
    App_UpdateLedBlink();
}

static void App_ReadInputs(void)
{
    const bool keyPressed = App_ReadKeyPressed();
    const bool waterSensorClosed = App_ReadFloatSensorClosed();
    const uint32_t now = HAL_GetTick();

    if (keyPressed && !app.previousKeyState && ((now - app.lastKeyEventTick) >= APP_DEBOUNCE_MS)) {
        app.lastKeyEventTick = now;
        App_HandleKeyPress();
    }

    app.previousKeyState = keyPressed;

    if (waterSensorClosed) {
        App_HandleLowWaterDetected();
    }
}

static void App_HandleKeyPress(void)
{
    if (app.flashData.keyPressedCount < UINT16_MAX) {
        app.flashData.keyPressedCount++;
    }

    app.flashData.lowLevelWasReached = false;
    app.lowLevelWasReachedPrev = false;
    app.disableACRelay = false;

    Send("Key pressed\r\n");
    Send("FloatSensor Reset !\r\n");

    App_SetRelayEnabled(true);

    if (EE_WriteVariable(VIRTADDR_KEYCOUNT, app.flashData.keyPressedCount) != EE_OK) {
        Send("Blad zapisu KeyPressedCount!\r\n");
    }

    if (EE_WriteVariable(VIRTADDR_LOWLEVEL, 0U) != EE_OK) {
        Send("Blad zapisu LowLevelWasReached!\r\n");
    }

    app.goToSleepMode = true;
    HAL_Delay(100U);
}

static void App_HandleLowWaterDetected(void)
{
    if (app.lowLevelWasReachedPrev) {
        return;
    }

    Send("FloatSensor Low Level !\r\n");

    app.lowLevelWasReachedPrev = true;
    app.flashData.lowLevelWasReached = true;

    if (!app.disableACRelay) {
        App_SetRelayEnabled(false);
        Send("Stan wody: NISKI \r\n");
        Send("AC Relay: Disable \r\n");
        app.disableACRelay = true;

        if (EE_WriteVariable(VIRTADDR_LOWLEVEL, 1U) != EE_OK) {
            Send("Blad zapisu LowLevelWasReached!\r\n");
        }
    }

    app.blinkLedFromTick = HAL_GetTick();
    app.goToSleepMode = false;
}

static void App_UpdateLedBlink(void)
{
    const uint32_t now = HAL_GetTick();

    if (app.lowLevelWasReachedPrev && ((now - app.blinkLedFromTick) >= APP_BLINK_PERIOD_MS)) {
        app.blinkLedFromTick = now;
        HAL_GPIO_TogglePin(LED_01_GPIO_Port, LED_01_Pin);
    }
}

static void App_EnterSleepIfAllowed(void)
{
    if (!app.goToSleepMode) {
        return;
    }

    Send("Going to sleep mode!\r\n");

    __SEV();
    __WFE();
    __WFE();

    HAL_SuspendTick();
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFE);
    HAL_ResumeTick();
}

static void App_ApplyStoredRelayState(void)
{
    if (app.flashData.lowLevelWasReached) {
        App_SetRelayEnabled(false);
        Send("AC Relay: Disable \r\n");
        app.disableACRelay = true;
        app.goToSleepMode = false;
        app.lowLevelWasReachedPrev = true;
        app.blinkLedFromTick = HAL_GetTick();
    } else {
        App_SetRelayEnabled(true);
        Send("AC Relay: Enable \r\n");
        app.disableACRelay = false;
    }
}

static void App_SetRelayEnabled(bool enabled)
{
    const GPIO_PinState pinState = enabled ? GPIO_PIN_RESET : GPIO_PIN_SET;

    HAL_GPIO_WritePin(LED_01_GPIO_Port, LED_01_Pin, pinState);
    HAL_GPIO_WritePin(ACRelay_GPIO_Port, ACRelay_Pin, pinState);
}

static bool App_ReadKeyPressed(void)
{
    return HAL_GPIO_ReadPin(UserSwitch_GPIO_Port, UserSwitch_Pin) == GPIO_PIN_SET;
}

static bool App_ReadFloatSensorClosed(void)
{
    return HAL_GPIO_ReadPin(FloatSensor_GPIO_Port, FloatSensor_Pin) == GPIO_PIN_SET;
}

static HAL_StatusTypeDef App_SaveFlashData(const DataFlash *in)
{
    HAL_StatusTypeDef status = HAL_OK;

    if (EE_WriteVariable(VA_MAGIC, in->magic) != EE_OK) {
        status = HAL_ERROR;
    }

    if (EE_WriteVariable(VIRTADDR_KEYCOUNT, in->keyPressedCount) != EE_OK) {
        status = HAL_ERROR;
    }

    if (EE_WriteVariable(VIRTADDR_LOWLEVEL, in->lowLevelWasReached ? 1U : 0U) != EE_OK) {
        status = HAL_ERROR;
    }

    return status;
}

static void App_LoadFlashData(DataFlash *out)
{
    uint16_t value;

    if (EE_ReadVariable(VA_MAGIC, &value) == EE_OK) {
        out->magic = value;
    }

    if (EE_ReadVariable(VIRTADDR_KEYCOUNT, &value) == EE_OK) {
        out->keyPressedCount = value;
    }

    if (EE_ReadVariable(VIRTADDR_LOWLEVEL, &value) == EE_OK) {
        out->lowLevelWasReached = (value != 0U);
    }
}

static void App_InitFlashData(void)
{
    Send("Odczyt z pamieci flash na starcie...\r\n");

    memset(&app.flashData, 0, sizeof(app.flashData));
    App_LoadFlashData(&app.flashData);

    if (app.flashData.magic != FLASH_MAGIC_16) {
        Send("Blad odczytu na starcie lub brak MAGIC - inicjalizacja domyslna.\r\n");

        app.flashData.magic = FLASH_MAGIC_16;
        app.flashData.keyPressedCount = 0U;
        app.flashData.lowLevelWasReached = false;

        if (App_SaveFlashData(&app.flashData) != HAL_OK) {
            Send("Blad zapisu!\r\n");
        } else {
            Send("Zapis domyslnych wartosci do flash ok.\r\n");
        }
    } else {
        Send("Odczyt na starcie ok.\r\n");
        Send("Magic data ok.\r\n");
    }
}
