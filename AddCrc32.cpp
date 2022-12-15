#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef signed char int8_t;
typedef signed short int int16_t;
typedef signed int int32_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;

#define MAX_NEW_BIN_SIZE (512 * 1024) // KB
volatile uint8_t NewBinArray[MAX_NEW_BIN_SIZE];

#define CHECKSUM_OFFSET_FROM_TAIL (16 * 15)

#define CRC_CTL_CRCMODE_Pos (30)

// clang-format off
#define CRC_CCITT           (0UL << CRC_CTL_CRCMODE_Pos) /*!<CRC Polynomial Mode - CCITT \hideinitializer */
#define CRC_8               (1UL << CRC_CTL_CRCMODE_Pos) /*!<CRC Polynomial Mode - CRC8 \hideinitializer */
#define CRC_16              (2UL << CRC_CTL_CRCMODE_Pos) /*!<CRC Polynomial Mode - CRC16 \hideinitializer */
#define CRC_32              (3UL << CRC_CTL_CRCMODE_Pos) /*!<CRC Polynomial Mode - CRC32 \hideinitializer */
// clang-format on

typedef struct {
    const char mode[10];
    const uint32_t polynom;
    uint32_t seed;
    uint32_t mask;
    uint32_t input_com;
    uint32_t input_rvs;
    uint32_t output_com;
    uint32_t output_rvs;
    uint32_t transfer_mode;
} CRC_MODE_ATTR_T;

const uint32_t ModeTable[] = {
    CRC_CCITT,
    CRC_8,
    CRC_16,
    CRC_32,
};

typedef enum {
    E_CRCCCITT = 0,
    E_CRC8,
    E_CRC16,
    E_CRC32,
} E_CRC_MODE;

const CRC_MODE_ATTR_T CRCAttr[] = {
    {"CRC-CCITT", 0x1021, 0xFFFF, 0xFFFF, 0, 0, 0, 0, 0},
    {"CRC-8    ", 0x7, 0xFF, 0xFF, 0, 0, 0, 0, 0},
    {"CRC-16   ", 0x8005, 0xFFFF, 0xFFFF, 0, 0, 0, 0, 0},
    {"CRC-32   ", 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 0, 0, 0},
};

static int8_t order;
static uint32_t polynom;
static uint32_t crcinit;
static uint32_t crcxor;
static int8_t refin;
static int8_t refout;

static uint32_t crcmask;
static uint32_t crchighbit;
static uint32_t crcinit_direct;
static uint32_t crcinit_nondirect;

volatile uint32_t u32TotalByteCnt;
volatile uint32_t u32seed;

static uint32_t reflect(uint32_t crc, int bitnum) {
    // reflects the lower 'bitnum' bits of 'crc'
    uint32_t i, j = 1, crcout = 0;

    for (i = (uint32_t)1 << (bitnum - 1); i; i >>= 1) {
        if (crc & i)
            crcout |= j;
        j <<= 1;
    }

    return (crcout);
}

static uint32_t crcbitbybit(uint8_t *p, uint32_t len, int8_t IsInput1sCOM,
                            int8_t IsCRC1sCOM) {
    /* bit by bit algorithm with augmented zero bytes.
     suited for polynom orders between 1...32. */
    uint32_t i, j, k, bit;
    uint32_t crc = crcinit_nondirect;

    for (i = 0; i < len; i++) {
        k = (uint32_t)*p++;
        if (IsInput1sCOM) {
            k = (~k) & 0xFF;
        }

        if (refin)
            k = reflect(k, 8);

        for (j = 0x80; j; j >>= 1) {
            bit = crc & crchighbit;
            crc <<= 1;

            if (k & j)
                crc |= 1;

            if (bit)
                crc ^= polynom;
        }
    }

    for (i = 0; i < order; i++) {
        bit = crc & crchighbit;
        crc <<= 1;
        if (bit)
            crc ^= polynom;
    }

    if (refout)
        crc = reflect(crc, order);

    crc ^= crcxor;
    crc &= crcmask;

    return (crc);
}

uint32_t CRC_SWResult(uint8_t *string, int8_t IsInput1sCOM, int8_t IsInputRVS,
                      int8_t IsCRC1sCOM, int8_t IsCRCRVS) {
    uint8_t i;
    uint32_t bit, crc, crc_result;
    volatile uint8_t uCRCMode;
    //	volatile uint32_t u32SWCRC32;

    /* CRC in CRC32 mode */
    order = 32;
    uCRCMode = E_CRC32;
    polynom = CRCAttr[uCRCMode].polynom;
    //    crcinit = CRCAttr[uCRCMode].seed;
    crcinit = u32seed;

    refin = IsInputRVS;
    refout = IsCRCRVS;

    /* at first, compute constant bit masks for whole CRC and CRC high bit */
    crcmask = ((((uint32_t)1 << (order - 1)) - 1) << 1) | 1;
    crchighbit = (uint32_t)1 << (order - 1);
    crcxor = (IsCRC1sCOM) ? crcmask : 0;

    /* check parameters */
    if (polynom != (polynom & crcmask))
        return (0);

    if (crcinit != (crcinit & crcmask))
        return (0);

    if (crcxor != (crcxor & crcmask))
        return (0);

    /* compute missing initial CRC value ... direct always is 1 */
    crc = crcinit;
    for (i = 0; i < order; i++) {
        bit = crc & 1;
        if (bit)
            crc ^= polynom;
        crc >>= 1;

        if (bit)
            crc |= crchighbit;
    }
    crcinit_nondirect = crc;
    crc_result = crcbitbybit((uint8_t *)string, u32TotalByteCnt, IsInput1sCOM,
                             IsCRC1sCOM);
    return crc_result;
}

int main(int argc, char *argv[]) {
    int ret;
    FILE *fptr;
    long binSize, resultSize;
    uint32_t oriCrc32, newCrc32;
    // uint32_t newBinSize;

    //  if(argc < 4)
    //  {
    //      printf("param < 3");
    //      exit(1);
    //  }

    // newBinSize = atoi(argv[3]);
    //  if(newBinSize >  MAX_NEW_BIN_SIZE)
    //  {
    //      printf("output binary size over maximum");
    //      exit(1);
    //  }

    if ((fopen_s(&fptr, argv[1], "rb")) != 0) {
        printf("Error! opening file: %s", argv[1]);
        exit(1);
    }

    // obtain file size:
    fseek(fptr, 0, SEEK_END);
    binSize = ftell(fptr);
    rewind(fptr);

    //  if(binSize > newBinSize - 4)//last four bytes are reserved for CRC32
    //  {
    //      printf("Input bin size:%d will over output binary size: %d\n",
    //      binSize, newBinSize); fclose (fptr); exit(1);
    //  }

    // copy the file into the NewBinArray:
    memset((uint8_t *)NewBinArray, 0xFF, MAX_NEW_BIN_SIZE);
    resultSize = fread((char *)NewBinArray, sizeof(char), binSize, fptr);
    if (resultSize != binSize) {
        fclose(fptr);
        printf("Reading error", stderr);
        exit(3);
    }
    fclose(fptr);

    u32TotalByteCnt = binSize - CHECKSUM_OFFSET_FROM_TAIL;
    u32seed = 0xFFFFFFFF;
    newCrc32 = CRC_SWResult((uint8_t *)NewBinArray, 0, 0, 0, 0);
    printf("add Bin CRC32: 0x%08X at address 0x%08X\n", newCrc32,
           binSize - CHECKSUM_OFFSET_FROM_TAIL);

    // Put CRC32 at the end of binary
    memcpy((char *)NewBinArray + binSize - CHECKSUM_OFFSET_FROM_TAIL,
           (uint8_t *)&newCrc32, 4);
    if ((fopen_s(&fptr, argv[1], "wb")) != 0) {
        printf("Error! opening file: %s\n", argv[1]);
        exit(1);
    }
    fwrite((char *)NewBinArray, sizeof(char), binSize, fptr);
    fclose(fptr);

    return ret;
}
