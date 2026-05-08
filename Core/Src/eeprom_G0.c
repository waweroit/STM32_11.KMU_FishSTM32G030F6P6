#include "eeprom_G0.h"


/* --- Prototypy prywatne --- */
static HAL_StatusTypeDef EE_Format(void);
static uint16_t EE_FindValidPage(uint8_t Operation);
static uint16_t EE_VerifyPageFullWriteVariable(uint16_t VirtAddress, uint16_t Data);
static uint16_t EE_PageTransfer(uint16_t VirtAddress, uint16_t Data);
static uint16_t EE_VerifyPageFullyErased(uint32_t Address);

/* Zapis rekordu jako 64 bity:
 * [31:16] = VirtAddress, [15:0] = Data; reszta wypełniona 0xFFFF (kasowane = 0xFFFF..)
 */
static inline uint64_t pack_record(uint16_t virt, uint16_t data)
{
    uint64_t v = 0xFFFFFFFFFFFFFFFFULL;
    /* młodsze 32 bity: data (16) + virt (16) */
    uint32_t low = ((uint32_t)virt << 16) | (uint32_t)data;
    v = (v & 0xFFFFFFFF00000000ULL) | (uint64_t)low;
    return v;
}

static inline uint16_t rec_get_virt(uint64_t rec)
{
    return (uint16_t)((rec >> 16) & 0xFFFF);
}

static inline uint16_t rec_get_data(uint64_t rec)
{
    return (uint16_t)(rec & 0xFFFF);
}

uint16_t EE_Init(void)
{
    uint16_t PageStatus0 = *(__IO uint16_t*)PAGE0_BASE_ADDRESS;
    uint16_t PageStatus1 = *(__IO uint16_t*)PAGE1_BASE_ADDRESS;

    /* Naprawa stanów nagłówków po ewentualnej utracie zasilania */
    switch (PageStatus0)
    {
        case ERASED:
            if (PageStatus1 == VALID_PAGE) {
                /* Upewnij się, że Page0 wymazana */
                if (!EE_VerifyPageFullyErased(PAGE0_BASE_ADDRESS)) {
                    if (EE_Format() != HAL_OK) return HAL_ERROR;
                }
            } else if (PageStatus1 == RECEIVE_DATA) {
                /* Wymaż Page0 i zakończ transakcję przeniesienia na Page1 */
                if (!EE_VerifyPageFullyErased(PAGE0_BASE_ADDRESS)) {
                    FLASH_EraseInitTypeDef e = {0};
                    uint32_t se;
                    e.TypeErase = FLASH_TYPEERASE_PAGES;
                    e.Page      = PAGE0_ID;
                    e.NbPages   = 1;
                    HAL_FLASH_Unlock();
                    if (HAL_FLASHEx_Erase(&e, &se) != HAL_OK) { HAL_FLASH_Lock(); return HAL_ERROR; }
                    HAL_FLASH_Lock();
                }
                HAL_FLASH_Unlock();
                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, PAGE1_BASE_ADDRESS, (uint64_t)VALID_PAGE) != HAL_OK)
                { HAL_FLASH_Lock(); return HAL_ERROR; }
                HAL_FLASH_Lock();
            } else {
                if (EE_Format() != HAL_OK) return HAL_ERROR;
            }
            break;

        case RECEIVE_DATA:
            if (PageStatus1 == VALID_PAGE) {
                /* Dokończ transfer z Page1 -> Page0 */
                for (uint16_t i = 0; i < NB_OF_VAR; i++) {
                    uint16_t read;
                    if (EE_ReadVariable(VirtAddVarTab[i], &read) == 0x0) {
                        if (EE_VerifyPageFullWriteVariable(VirtAddVarTab[i], read) != HAL_OK) return HAL_ERROR;
                    }
                }
                HAL_FLASH_Unlock();
                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, PAGE0_BASE_ADDRESS, (uint64_t)VALID_PAGE) != HAL_OK)
                { HAL_FLASH_Lock(); return HAL_ERROR; }
                HAL_FLASH_Lock();

                FLASH_EraseInitTypeDef e = {0};
                uint32_t se;
                e.TypeErase = FLASH_TYPEERASE_PAGES;
                e.Page      = PAGE1_ID;
                e.NbPages   = 1;
                HAL_FLASH_Unlock();
                if (HAL_FLASHEx_Erase(&e, &se) != HAL_OK) { HAL_FLASH_Lock(); return HAL_ERROR; }
                HAL_FLASH_Lock();
            }
            else if (PageStatus1 == ERASED) {
                /* Dokończ – Page0 -> VALID */
                HAL_FLASH_Unlock();
                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, PAGE0_BASE_ADDRESS, (uint64_t)VALID_PAGE) != HAL_OK)
                { HAL_FLASH_Lock(); return HAL_ERROR; }
                HAL_FLASH_Lock();
            }
            else {
                if (EE_Format() != HAL_OK) return HAL_ERROR;
            }
            break;

        case VALID_PAGE:
            if (PageStatus1 == VALID_PAGE) {
                if (EE_Format() != HAL_OK) return HAL_ERROR;
            } else if (PageStatus1 == ERASED) {
                /* OK – nic nie robimy */
            } else { /* Page1 == RECEIVE_DATA: dokończ transfer */
                for (uint16_t i = 0; i < NB_OF_VAR; i++) {
                    uint16_t read;
                    if (EE_ReadVariable(VirtAddVarTab[i], &read) == 0x0) {
                        if (EE_VerifyPageFullWriteVariable(VirtAddVarTab[i], read) != HAL_OK) return HAL_ERROR;
                    }
                }
                HAL_FLASH_Unlock();
                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, PAGE1_BASE_ADDRESS, (uint64_t)VALID_PAGE) != HAL_OK)
                { HAL_FLASH_Lock(); return HAL_ERROR; }
                HAL_FLASH_Lock();

                FLASH_EraseInitTypeDef e = {0};
                uint32_t se;
                e.TypeErase = FLASH_TYPEERASE_PAGES;
                e.Page      = PAGE0_ID;
                e.NbPages   = 1;
                HAL_FLASH_Unlock();
                if (HAL_FLASHEx_Erase(&e, &se) != HAL_OK) { HAL_FLASH_Lock(); return HAL_ERROR; }
                HAL_FLASH_Lock();
            }
            break;

        default:
            if (EE_Format() != HAL_OK) return HAL_ERROR;
            break;
    }

    return HAL_OK;
}

static uint16_t EE_VerifyPageFullyErased(uint32_t Address)
{
    uint32_t EndAddress = Address + (PAGE_SIZE - 8U); /* skanuj DW = 8 bajtów */
    while (Address <= EndAddress) {
        uint64_t v = *(__IO uint64_t*)Address;
        if (v != 0xFFFFFFFFFFFFFFFFULL) {
            return 0; /* nie wymazana */
        }
        Address += 8U;
    }
    return 1; /* wymazana */
}

uint16_t EE_ReadVariable(uint16_t VirtAddress, uint16_t* Data)
{
    uint16_t ValidPage = EE_FindValidPage(READ_FROM_VALID_PAGE);
    if (ValidPage == NO_VALID_PAGE) return NO_VALID_PAGE;

    uint32_t page_start = (ValidPage == PAGE0) ? PAGE0_BASE_ADDRESS : PAGE1_BASE_ADDRESS;
    uint32_t addr = page_start + PAGE_SIZE - 8U; /* od końca, krok 8 */

    while (addr > page_start) {
        uint64_t rec = *(__IO uint64_t*)addr;
        uint16_t virt = rec_get_virt(rec);
        if (virt == VirtAddress) {
            *Data = rec_get_data(rec);
            return 0; /* znaleziono */
        }
        addr -= 8U;
    }
    return 1; /* nie znaleziono */
}

uint16_t EE_WriteVariable(uint16_t VirtAddress, uint16_t Data)
{
    uint16_t st = EE_VerifyPageFullWriteVariable(VirtAddress, Data);
    if (st == PAGE_FULL) {
        st = EE_PageTransfer(VirtAddress, Data);
    }
    return st;
}

static HAL_StatusTypeDef EE_Format(void)
{
    FLASH_EraseInitTypeDef e = {0};
    uint32_t se;

    e.TypeErase = FLASH_TYPEERASE_PAGES;
    e.Page      = PAGE0_ID;
    e.NbPages   = 2; /* obie strony */

    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&e, &se) != HAL_OK) { HAL_FLASH_Lock(); return HAL_ERROR; }

    /* Page0 = VALID_PAGE, Page1 pozostaje ERASED */
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, PAGE0_BASE_ADDRESS, (uint64_t)VALID_PAGE) != HAL_OK)
    { HAL_FLASH_Lock(); return HAL_ERROR; }

    HAL_FLASH_Lock();
    return HAL_OK;
}

static uint16_t EE_FindValidPage(uint8_t Operation)
{
    uint16_t s0 = *(__IO uint16_t*)PAGE0_BASE_ADDRESS;
    uint16_t s1 = *(__IO uint16_t*)PAGE1_BASE_ADDRESS;

    if (Operation == WRITE_IN_VALID_PAGE) {
        if (s1 == VALID_PAGE) {
            return (s0 == RECEIVE_DATA) ? PAGE0 : PAGE1;
        } else if (s0 == VALID_PAGE) {
            return (s1 == RECEIVE_DATA) ? PAGE1 : PAGE0;
        } else {
            return NO_VALID_PAGE;
        }
    } else { /* READ_FROM_VALID_PAGE */
        if (s0 == VALID_PAGE) return PAGE0;
        if (s1 == VALID_PAGE) return PAGE1;
        return NO_VALID_PAGE;
    }
}

static uint16_t EE_VerifyPageFullWriteVariable(uint16_t VirtAddress, uint16_t Data)
{
    uint16_t ValidPage = EE_FindValidPage(WRITE_IN_VALID_PAGE);
    if (ValidPage == NO_VALID_PAGE) return NO_VALID_PAGE;

    uint32_t addr = (ValidPage == PAGE0) ? PAGE0_BASE_ADDRESS : PAGE1_BASE_ADDRESS;
    uint32_t end  = addr + PAGE_SIZE;

    /* Pierwszy wolny slot 8-bajtowy: szukamy 0xFFFF.. */
    while (addr < end) {
        uint64_t v = *(__IO uint64_t*)addr;
        if (v == 0xFFFFFFFFFFFFFFFFULL) {
            uint64_t rec = pack_record(VirtAddress, Data);
            HAL_FLASH_Unlock();
            HAL_StatusTypeDef st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, rec);
            HAL_FLASH_Lock();
            return st;
        }
        addr += 8U;
    }
    return PAGE_FULL;
}

static uint16_t EE_PageTransfer(uint16_t VirtAddress, uint16_t Data)
{
    uint16_t valid = EE_FindValidPage(READ_FROM_VALID_PAGE);
    if (valid == NO_VALID_PAGE) return NO_VALID_PAGE;

    uint32_t new_base = (valid == PAGE1) ? PAGE0_BASE_ADDRESS : PAGE1_BASE_ADDRESS;
    uint32_t old_id   = (valid == PAGE1) ? PAGE1_ID : PAGE0_ID;

    /* Oznacz nową stronę jako RECEIVE_DATA */
    HAL_FLASH_Unlock();
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, new_base, (uint64_t)RECEIVE_DATA) != HAL_OK)
    { HAL_FLASH_Lock(); return HAL_ERROR; }
    HAL_FLASH_Lock();

    /* Zapisz zmienną wywołującą transfer */
    if (EE_VerifyPageFullWriteVariable(VirtAddress, Data) != HAL_OK) return HAL_ERROR;

    /* Przenieś resztę zmiennych: ostatnie wartości */
    for (uint16_t i = 0; i < NB_OF_VAR; i++) {
        uint16_t va = VirtAddVarTab[i];
        if (va == VirtAddress) continue;
        uint16_t v;
        if (EE_ReadVariable(va, &v) == 0x0) {
            if (EE_VerifyPageFullWriteVariable(va, v) != HAL_OK) return HAL_ERROR;
        }
    }

    /* Skasuj starą stronę */
    FLASH_EraseInitTypeDef e = {0};
    uint32_t se;
    e.TypeErase = FLASH_TYPEERASE_PAGES;
    e.Page      = old_id;
    e.NbPages   = 1;
    HAL_FLASH_Unlock();
    if (HAL_FLASHEx_Erase(&e, &se) != HAL_OK) { HAL_FLASH_Lock(); return HAL_ERROR; }

    /* Zatwierdź nową stronę jako VALID_PAGE */
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, new_base, (uint64_t)VALID_PAGE) != HAL_OK)
    { HAL_FLASH_Lock(); return HAL_ERROR; }
    HAL_FLASH_Lock();

    return HAL_OK;
}
