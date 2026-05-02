#ifndef __EEPROM_G0_H
#define __EEPROM_G0_H

#include "stm32g0xx_hal.h"
#include <stdint.h>

/* Kody statusu zgodne z HAL */
#define EE_OK      (uint32_t)HAL_OK
#define EE_ERROR   (uint32_t)HAL_ERROR
#define EE_BUSY    (uint32_t)HAL_BUSY
#define EE_TIMEOUT (uint32_t)HAL_TIMEOUT

/* Rozmiar strony bierze się z HAL (STM32G0: 2KB) */
#ifndef PAGE_SIZE
#define PAGE_SIZE               ((uint32_t)FLASH_PAGE_SIZE)
#endif

/* Liczba zmiennych wirtualnych (dopasuj do swoich potrzeb) */
#ifndef NB_OF_VAR
#define NB_OF_VAR              ((uint8_t)0x03)
#endif

/* Ustal start emulacji. Dla STM32G030F6 (32KB Flash):
 * ostatnie 2 strony: 0x08007000..0x080077FF i 0x08007800..0x08007FFF
 */
#ifndef EEPROM_START_ADDRESS
#define EEPROM_START_ADDRESS   ((uint32_t)0x08007000U)
#endif

/* Adresy bazowe stron */
#define PAGE0_BASE_ADDRESS     ((uint32_t)(EEPROM_START_ADDRESS + 0x0000))
#define PAGE0_END_ADDRESS      ((uint32_t)(EEPROM_START_ADDRESS + (PAGE_SIZE - 1U)))

#define PAGE1_BASE_ADDRESS     ((uint32_t)(EEPROM_START_ADDRESS + PAGE_SIZE))
#define PAGE1_END_ADDRESS      ((uint32_t)(EEPROM_START_ADDRESS + (2U * PAGE_SIZE - 1U)))

/* Identyfikatory stron liczone w numerach stron Flash (G0 kasuje po stronach) */
#define FLASH_PAGE_NUMBER(addr)   (((addr) - FLASH_BASE) / PAGE_SIZE)
#define PAGE0_ID                  ((uint32_t)FLASH_PAGE_NUMBER(PAGE0_BASE_ADDRESS))
#define PAGE1_ID                  ((uint32_t)FLASH_PAGE_NUMBER(PAGE1_BASE_ADDRESS))

/* Użyte strony */
#define PAGE0                    ((uint16_t)0x0000)
#define PAGE1                    ((uint16_t)0x0001)

/* Brak ważnej strony */
#define NO_VALID_PAGE            ((uint16_t)0x00AB)

/* Statusy stron (nagłówek 16-bit jak w oryginale) */
#define ERASED                   ((uint16_t)0xFFFF)
#define RECEIVE_DATA             ((uint16_t)0xEEEE)
#define VALID_PAGE               ((uint16_t)0x0000)

/* Kierunki operacji */
#define READ_FROM_VALID_PAGE     ((uint8_t)0x00)
#define WRITE_IN_VALID_PAGE      ((uint8_t)0x01)

/* Strona pełna */
#define PAGE_FULL                ((uint8_t)0x80)

/* Interfejs */
uint16_t EE_Init(void);
uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data);
uint16_t EE_WriteVariable(uint16_t VirtAddress, uint16_t Data);

/* Tablica adresów wirtualnych definiowana przez użytkownika */
extern uint16_t VirtAddVarTab[NB_OF_VAR];

#endif /* __EEPROM_G0_H */
