
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MEMORYFLASHWRITEREAD_H
#define __MEMORYFLASHWRITEREAD_H

/* Includes ------------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
typedef enum
{
	JumpToApp = 0,
	JumpToIap,
}JumpPartition;

#pragma pack(1)

typedef struct _MEMINF_
{
	uint32_t	AppStartAddr;
	uint32_t	AppEndAddr;
	uint32_t	PageSize;
	uint8_t		PagesPer_Packet;
	uint32_t	ProvidSaveStartAddr;
	uint32_t	ProvidSaveStopAddr;
}_MEMINF, *_pMEMINF;

#pragma pack()

/* Exported macro ------------------------------------------------------------*/
/* Base address of the Flash sectors */
#define ADDR_FLASH_SECTOR_0     ((uint32_t)0x08000000) /* Base @ of Sector 0, 16 Kbytes */
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000) /* Base @ of Sector 1, 16 Kbytes */
#define ADDR_FLASH_SECTOR_2     ((uint32_t)0x08008000) /* Base @ of Sector 2, 16 Kbytes */
#define ADDR_FLASH_SECTOR_3     ((uint32_t)0x0800C000) /* Base @ of Sector 3, 16 Kbytes */
#define ADDR_FLASH_SECTOR_4     ((uint32_t)0x08010000) /* Base @ of Sector 4, 64 Kbytes */
#define ADDR_FLASH_SECTOR_5     ((uint32_t)0x08020000) /* Base @ of Sector 5, 128 Kbytes */
#define ADDR_FLASH_SECTOR_6     ((uint32_t)0x08040000) /* Base @ of Sector 6, 128 Kbytes */
#define ADDR_FLASH_SECTOR_7     ((uint32_t)0x08060000) /* Base @ of Sector 7, 128 Kbytes */
#define ADDR_FLASH_SECTOR_8     ((uint32_t)0x08080000) /* Base @ of Sector 8, 128 Kbytes */
#define ADDR_FLASH_SECTOR_9     ((uint32_t)0x080A0000) /* Base @ of Sector 9, 128 Kbytes */
#define ADDR_FLASH_SECTOR_10    ((uint32_t)0x080C0000) /* Base @ of Sector 10, 128 Kbytes */
#define ADDR_FLASH_SECTOR_11    ((uint32_t)0x080E0000) /* Base @ of Sector 11, 128 Kbytes */


/* Flash partition memory map of the Flash sectors */
//Iap code memory space.
#define IAP_FLASH_PARTITION							ADDR_FLASH_SECTOR_0
#define IAP_BACK_UP_FLASH_PARTITION			ADDR_FLASH_SECTOR_1
//Info data memory space.
#define INFO_FLASH_PARTITION						ADDR_FLASH_SECTOR_2
#define INFO_BACK_UP_FLASH_PARTITION		ADDR_FLASH_SECTOR_3
//C code Lib function space.
#define C_LIB_STORE_PARTITION						ADDR_FLASH_SECTOR_4
//App code memory space.
#define APP_FLASH_PARTITION							ADDR_FLASH_SECTOR_5
#define APP_BACK_UP_FLASH_PARTITION			ADDR_FLASH_SECTOR_6
#define APP_TEMP_FLASH_PARTITION				ADDR_FLASH_SECTOR_7

#define SRAM1_ADDR_PARTITION						0x20000000
#define SRAM2_ADDR_PARTITION						0x10000000


/* Exported variables --------------------------------------------------------*/
//extern _pMEMINF pMemInf;
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
uint32_t GetSector(uint32_t Address);
void MemoryReadWriteTest(uint32_t SourSector, uint32_t DestSector);
void MemoryFuncJump(JumpPartition JumpSpace);

#endif /* __MEMORYFLASHWRITEREAD_H */
