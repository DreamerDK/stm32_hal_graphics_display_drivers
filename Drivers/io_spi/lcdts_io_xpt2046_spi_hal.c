/*
 * SPI HAL LCD and TS driver for all stm32 family in shar
 * author: Roberto Benjami
 * v.2022.12
*/

//-----------------------------------------------------------------------------
#include <stdio.h>

#include "main.h"
#include "lcdts_io_xpt2046_spi_hal.h"
#include "lcd.h"
#include "lcd_io.h"
#include "ts.h"

//-----------------------------------------------------------------------------
#define  DMA_MINSIZE          0x0010
#define  DMA_MAXSIZE          0xFFFE
/* note:
   - DMA_MINSIZE: if the transacion Size < DMA_MINSIZE -> not use the DMA for transaction
   - DMA_MAXSIZE: if the transacion Size > DMA_MAXSIZE -> multiple DMA transactions (because DMA transaction size register is 16bit) */

#define  LCDTS_SPI_TIMEOUT    HAL_MAX_DELAY
/* note:
   - LCDTS_SPI_TIMEOUT: HAL_SPI_Transmit and HAL_SPI_Receive timeout value */

/* SPI clock pin default state */
#define  LCDTS_SPI_DEFSTATE   0

//-----------------------------------------------------------------------------
/* Bitdepth convert macros */
#if LCD_RGB24_ORDER == 0
#define  RGB565TO888(c16)     ((c16 & 0xF800) << 8) | ((c16 & 0x07E0) << 5) | ((c16 & 0x001F) << 3)
#define  RGB888TO565(c24)     ((c24 & 0XF80000) >> 8 | (c24 & 0xFC00) >> 5 | (c24 & 0xF8 ) >> 3)
#elif LCD_RGB24_ORDER == 1
#define  RGB565TO888(c16)     ((c16 & 0xF800) >> 8) | ((c16 & 0x07E0) << 5) | ((c16 & 0x001F) << 19)
#define  RGB888TO565(c24)     ((c24 & 0XF80000) >> 19 | (c24 & 0xFC00) >> 5 | (c24 & 0xF8 ) << 8)
#endif

/* processor family dependent things */
#if defined(STM32C0)
#include "stm32c0xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_8BIT)
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_16BIT)
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32F0)
#include "stm32f0xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_8BIT)
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_16BIT)
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32F1)
#include "stm32f1xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        hlcdspi.Instance->CR1 &= ~SPI_CR1_DFF
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       hlcdspi.Instance->CR1 |= SPI_CR1_DFF
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32F2)
#include "stm32f2xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        hlcdspi.Instance->CR1 &= ~SPI_CR1_DFF
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       hlcdspi.Instance->CR1 |= SPI_CR1_DFF
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32F3)
#include "stm32f3xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_8BIT)
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_16BIT)
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32F4)
#include "stm32f4xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        hlcdspi.Instance->CR1 &= ~SPI_CR1_DFF
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       hlcdspi.Instance->CR1 |= SPI_CR1_DFF
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32F7)
#include "stm32f7xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_8BIT)
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_16BIT)
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32H7)
#include "stm32h7xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        MODIFY_REG(hlcdspi.Instance->CFG1, SPI_CFG1_DSIZE, SPI_DATASIZE_8BIT)
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       MODIFY_REG(hlcdspi.Instance->CFG1, SPI_CFG1_DSIZE, SPI_DATASIZE_16BIT)
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CFG1, SPI_CFG1_MBR, br << SPI_CFG1_MBR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXP) dummy = hlcdspi.Instance->RXDR
#elif defined(STM32G0)
#include "stm32g0xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_8BIT)
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_16BIT)
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32G4)
#include "stm32g4xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_8BIT)
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_16BIT)
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32L0)
#include "stm32l0xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        hlcdspi.Instance->CR1 &= ~SPI_CR1_DFF
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       hlcdspi.Instance->CR1 |= SPI_CR1_DFF
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32L1)
#include "stm32l1xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        hlcdspi.Instance->CR1 &= ~SPI_CR1_DFF
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       hlcdspi.Instance->CR1 |= SPI_CR1_DFF
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32L4)
#include "stm32l4xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_8BIT)
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_16BIT)
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32L5)
#include "stm32l5xx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_8BIT)
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_16BIT)
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32WB)
#include "stm32wbxx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_8BIT)
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_16BIT)
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#elif defined(STM32WL)
#include "stm32wlxx_ll_gpio.h"
#define  LCD_SPI_SETDATASIZE_8BIT(hlcdspi)        MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_8BIT)
#define  LCD_SPI_SETDATASIZE_16BIT(hlcdspi)       MODIFY_REG(hlcdspi.Instance->CR2, SPI_CR2_DS, SPI_DATASIZE_16BIT)
#define  LCD_SPI_SETBAUDRATE(hlcdspi, br)         MODIFY_REG(hlcdspi.Instance->CR1, SPI_CR1_BR, br << SPI_CR1_BR_Pos)
#define  LCD_SPI_RXFIFOCLEAR(hlcdspi, dummy)      while(hlcdspi.Instance->SR & SPI_SR_RXNE) dummy = hlcdspi.Instance->DR
#else
#error unknown processor family
#endif

//=============================================================================
extern  SPI_HandleTypeDef   LCDTS_SPI_HANDLE;

#if LCD_RGB24_BUFFSIZE < DMA_MINSIZE
#undef  LCD_RGB24_BUFFSIZE
#define LCD_RGB24_BUFFSIZE    0
#else
uint8_t lcd_rgb24_buffer[LCD_RGB24_BUFFSIZE * 3 + 1];
#endif  /* #else LCD_RGB24_DMA_BUFFERSIZE < DMA_MINSIZE */

//-----------------------------------------------------------------------------
#if LCD_SPI_MODE == 0
/* Transmit only mode */

#define  LcdDirRead()
#define  LcdDirWrite()

#elif (LCD_SPI_MODE == 1) || (LCD_SPI_MODE == 2)
/* Half duplex and full duplex mode */

//-----------------------------------------------------------------------------
/* Switch from SPI write mode to SPI read mode, read the dummy bits, modify the SPI speed */
void LcdDirRead(uint32_t DummySize)
{
  uint32_t RxDummy __attribute__((unused));
  __HAL_SPI_DISABLE(&LCDTS_SPI_HANDLE);   /* stop SPI */
  #if LCD_SPI_MODE == 1
  SPI_1LINE_RX(&LCDTS_SPI_HANDLE);        /* if half duplex -> change MOSI data direction */
  #endif
  LL_GPIO_SetPinMode(LCDTS_SCK_GPIO_Port, LCDTS_SCK_Pin, LL_GPIO_MODE_OUTPUT); /* GPIO mode = output */
  while(DummySize--)
  { /* Dummy pulses */
    HAL_GPIO_WritePin(LCDTS_SCK_GPIO_Port, LCDTS_SCK_Pin, 1 - LCDTS_SPI_DEFSTATE);
    HAL_GPIO_WritePin(LCDTS_SCK_GPIO_Port, LCDTS_SCK_Pin, LCDTS_SPI_DEFSTATE);
  }
  LL_GPIO_SetPinMode(LCDTS_SCK_GPIO_Port, LCDTS_SCK_Pin, LL_GPIO_MODE_ALTERNATE); /* GPIO mode = alternative */
  #if defined(LCD_SPI_SPD_WRITE) && defined(LCD_SPI_SPD_READ) && (LCD_SPI_SPD_WRITE != LCD_SPI_SPD_READ)
  LCD_SPI_SETBAUDRATE(LCDTS_SPI_HANDLE, LCD_SPI_SPD_READ);        /* speed change */
  #endif
  LCD_SPI_RXFIFOCLEAR(LCDTS_SPI_HANDLE, RxDummy);                 /* RX fifo clear */
}

//-----------------------------------------------------------------------------
/* Switch from SPI read mode to SPI write mode, modify the SPI speed */
void LcdDirWrite(void)
{
  __HAL_SPI_DISABLE(&LCDTS_SPI_HANDLE);                           /* stop SPI */
  #if defined(LCD_SPI_SPD_WRITE) && defined(LCD_SPI_SPD_READ) && (LCD_SPI_SPD_WRITE != LCD_SPI_SPD_READ)
  LCD_SPI_SETBAUDRATE(LCDTS_SPI_HANDLE, LCD_SPI_SPD_WRITE);       /* speed change */
  #endif
}

#endif /* LCD_SPI_MODE */

//-----------------------------------------------------------------------------
/* Set SPI 8bit mode without HAL_SPI_Init */
static inline void LcdSpiMode8(void)
{
  LCD_SPI_SETDATASIZE_8BIT(LCDTS_SPI_HANDLE);
  LCDTS_SPI_HANDLE.Init.DataSize = SPI_DATASIZE_8BIT;
}

/* Set SPI 16bit mode without HAL_SPI_Init */
static inline void LcdSpiMode16(void)
{
  LCD_SPI_SETDATASIZE_16BIT(LCDTS_SPI_HANDLE);
  LCDTS_SPI_HANDLE.Init.DataSize = SPI_DATASIZE_16BIT;
}

//-----------------------------------------------------------------------------
#if LCD_DMA_TX == 0 && LCD_DMA_RX == 0
/* DMA off mode */

#define LcdTransInit()
#define LcdTransStart()
#define LcdTransEnd()

uint32_t LCD_IO_DmaBusy(void)
{
  return 0;
}

#else /* #if LCD_DMA_TX == 0 && LCD_DMA_RX == 0 */
/* DMA on mode */

#define DMA_STATUS_FREE       0
#define DMA_STATUS_FILL       (1 << 0)
#define DMA_STATUS_MULTIDATA  (1 << 1)
#define DMA_STATUS_8BIT       (1 << 2)
#define DMA_STATUS_16BIT      (1 << 3)
#define DMA_STATUS_24BIT      (1 << 4)

struct
{
  volatile uint32_t status;   /* DMA status (0=free, other: see the DMA_STATUS... macros)  */
  uint32_t size;              /* all transactions data size */
  uint32_t trsize;            /* actual DMA transaction data size */
  uint32_t maxtrsize;         /* max size / one DMA transaction */
  uint32_t ptr;               /* data pointer for DMA */
  uint16_t data;              /* fill operation data for DMA */
}volatile dmastatus;

//-----------------------------------------------------------------------------
#ifndef  osCMSIS
/* DMA mode on, Freertos off mode */

#define LcdTransInit()

#if LCD_DMA_ENDWAIT == 0
#define LcdTransStart()       {while(dmastatus.status);}
#define LcdDmaWaitEnd(d)
#elif LCD_DMA_ENDWAIT == 1
#define LcdTransStart()       {while(dmastatus.status);}
#define LcdDmaWaitEnd(d)      {if(d) while(dmastatus.status);}
#elif LCD_DMA_ENDWAIT == 2
#define LcdTransStart()
#define LcdDmaWaitEnd(d)      {while(dmastatus.status);}
#endif  /* #elif LCD_DMA_ENDWAIT == 2 */

#define LcdTransEnd()

#define LcdDmaTransEnd()      {dmastatus.status = 0;}

#else    /* #ifndef osCMSIS */
/* Freertos mode */

//-----------------------------------------------------------------------------
#if osCMSIS < 0x20000
/* DMA on, Freertos 1 mode */

osSemaphoreId LcdSemIdHandle;
osSemaphoreDef(LcdSemId);
#define LcdSemNew0            LcdSemIdHandle = osSemaphoreCreate(osSemaphore(LcdSemId), 1); osSemaphoreWait(LcdSemIdHandle, 0)
#define LcdSemNew1            LcdSemIdHandle = osSemaphoreCreate(osSemaphore(LcdSemId), 1)
#define LcdSemWait            osSemaphoreWait(LcdSemIdHandle, osWaitForever)
#define LcdSemSet             osSemaphoreRelease(LcdSemIdHandle)

//-----------------------------------------------------------------------------
#else /* #if osCMSIS < 0x20000 */
/* DMA on, Freertos 2 mode */

osSemaphoreId_t LcdSemId;
#define LcdSemNew0            LcdSemId = osSemaphoreNew(1, 0, 0)
#define LcdSemNew1            LcdSemId = osSemaphoreNew(1, 1, 0)
#define LcdSemWait            osSemaphoreAcquire(LcdSemId, osWaitForever)
#define LcdSemSet             osSemaphoreRelease(LcdSemId)

#endif /* #else osCMSIS < 0x20000 */

//-----------------------------------------------------------------------------
/* DMA on, Freertos 1 and 2 mode */

#if LCD_DMA_ENDWAIT == 0
#define LcdTransInit()        {LcdSemNew1;}
#define LcdTransStart()       {LcdSemWait;}
#define LcdTransEnd()         {LcdSemSet;}
#define LcdDmaWaitEnd(d)
#elif LCD_DMA_ENDWAIT == 1
#define LcdTransInit()        {LcdSemNew1;}
#define LcdTransStart()       {LcdSemWait;}
#define LcdTransEnd()         {LcdSemSet;}
#define LcdDmaWaitEnd(d)      {if(d) {LcdSemWait; LcdSemSet;}}
#elif LCD_DMA_ENDWAIT == 2
#define LcdTransInit()        {LcdSemNew0;}
#define LcdTransStart()
#define LcdTransEnd()
#define LcdDmaWaitEnd(d)      {LcdSemWait;}
#endif  /* elif LCD_DMA_ENDWAIT == 2 */

#define LcdDmaTransEnd()      {dmastatus.status = 0; LcdSemSet;}

#endif /* #else osCMSIS */

//-----------------------------------------------------------------------------
/* Get the DMA operation status (0=DMA is free, 1=DMA is busy) */
uint32_t LCD_IO_DmaBusy(void)
{
  uint32_t ret = 0;
  if(dmastatus.status != DMA_STATUS_FREE)
    ret = 1;
  return ret;
}

#endif /* #else LCD_DMA_TX == 0 && LCD_DMA_RX == 0 */

//=============================================================================
/* Fill 24bit bitmap from 16bit color
   - color : 16 bit (RGB565) color
   - tg    : 24 bit (RGB888) color target bitmap pointer
   - Size  : number of pixel */
void FillConvert16to24(uint16_t color, uint8_t * tg, uint32_t Size)
{
  uint32_t c24;
  c24 = RGB565TO888(color);
  while(Size--)
  {
    *(uint32_t *)tg = c24;
    tg += 3;
  }
}

//-----------------------------------------------------------------------------
/* Convert from 16bit bitmnap to 24bit bitmap
   - src   : 16 bit (RGB565) color source bitmap pointer
   - tg    : 24 bit (RGB888) color target bitmap pointer
   - Size  : number of pixel */
void BitmapConvert16to24(uint16_t * src, uint8_t * tg, uint32_t Size)
{
  while(Size--)
  {
    *(uint32_t *)tg = RGB565TO888(*src);
    src++;
    tg += 3;
  }
}

//-----------------------------------------------------------------------------
/* Convert from 16bit bitmnap to 24bit bitmap
   - src   : 24 bit (RGB888) color source bitmap pointer
   - tg    : 16 bit (RGB565) color target bitmap pointer
   - Size  : number of pixel */
void BitmapConvert24to16(uint8_t * src, uint16_t * tg, uint32_t Size)
{
  uint32_t c24;
  while(Size--)
  {
    c24 = *(uint32_t *)src;
    *tg = RGB888TO565(c24);
    src += 3;
    tg++;
  }
}

//=============================================================================
/* TX DMA */
#if LCD_DMA_TX == 1

//-----------------------------------------------------------------------------
/* DMA operation end callback function prototype */
__weak void LCD_IO_DmaTxCpltCallback(SPI_HandleTypeDef *hspi)
{
  UNUSED(hspi);
}

//-----------------------------------------------------------------------------
/* SPI DMA operation interrupt */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if(hspi == &LCDTS_SPI_HANDLE)
  {
    if(dmastatus.size > dmastatus.trsize)
    { /* dma operation is still required */

      if(dmastatus.status == (DMA_STATUS_MULTIDATA | DMA_STATUS_8BIT))
        dmastatus.ptr += dmastatus.trsize;        /* 8bit multidata */
      else if(dmastatus.status == (DMA_STATUS_MULTIDATA | DMA_STATUS_16BIT))
        dmastatus.ptr += dmastatus.trsize << 1; /* 16bit multidata */
      else if(dmastatus.status == (DMA_STATUS_MULTIDATA | DMA_STATUS_24BIT))
        dmastatus.ptr += dmastatus.trsize << 1; /* 24bit multidata */

      dmastatus.size -= dmastatus.trsize;
      if(dmastatus.size <= dmastatus.maxtrsize)
        dmastatus.trsize = dmastatus.size;

      #if LCD_RGB24_BUFFSIZE == 0
      HAL_SPI_Transmit_DMA(&LCDTS_SPI_HANDLE, (uint8_t *)dmastatus.ptr, dmastatus.trsize);
      #else
      if(dmastatus.status == (DMA_STATUS_MULTIDATA | DMA_STATUS_24BIT))
      {
        BitmapConvert16to24((uint16_t *)dmastatus.ptr, lcd_rgb24_buffer, dmastatus.trsize);
        HAL_SPI_Transmit_DMA(&LCDTS_SPI_HANDLE, (uint8_t *)lcd_rgb24_buffer, dmastatus.trsize * 3);
      }
      else if(dmastatus.status == (DMA_STATUS_FILL | DMA_STATUS_24BIT))
        HAL_SPI_Transmit_DMA(&LCDTS_SPI_HANDLE, (uint8_t *)lcd_rgb24_buffer, dmastatus.trsize * 3);
      else
        HAL_SPI_Transmit_DMA(&LCDTS_SPI_HANDLE, (uint8_t *)dmastatus.ptr, dmastatus.trsize);
      #endif
    }
    else
    { /* dma operations have ended */
      HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
      LcdDmaTransEnd();
      LCD_IO_DmaTxCpltCallback(hspi);
    }
  }
}
#endif /* #if LCD_DMA_TX == 1 */

//-----------------------------------------------------------------------------
/* Wrtite fill and multi data to Lcd (8 and 16 bit mode)
   - pData: 8 or 16 bits data pointer
   - Size: data number
   - Mode: 8 or 16 or 24 bit mode, write or read, fill or multidata (see the LCD_IO_... defines in lcd_io.h file) */
void LCDWriteFillMultiData8and16(uint8_t * pData, uint32_t Size, uint32_t Mode)
{
  if(Mode & LCD_IO_DATA8)
    LcdSpiMode8();
  else
    LcdSpiMode16();

  #if LCD_DMA_TX == 1
  if((Size > DMA_MINSIZE) && (!LCD_DMA_UNABLE((uint32_t)pData)))
  { /* DMA mode */
    if(Mode & LCD_IO_DATA8)
    { /* 8bit DMA */
      LCDTS_SPI_HANDLE.hdmatx->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
      LCDTS_SPI_HANDLE.hdmatx->Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
      dmastatus.status = DMA_STATUS_8BIT;
    }
    else
    { /* 16bit DMA */
      LCDTS_SPI_HANDLE.hdmatx->Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
      LCDTS_SPI_HANDLE.hdmatx->Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
      dmastatus.status = DMA_STATUS_16BIT;
    }

    if(Mode & LCD_IO_FILL)
    { /* fill */
      LCDTS_SPI_HANDLE.hdmatx->Init.MemInc = DMA_MINC_DISABLE;
      dmastatus.status |= DMA_STATUS_FILL;
      dmastatus.data = *(uint16_t *)pData;
      dmastatus.ptr = (uint32_t)&dmastatus.data;
    }
    else
    { /* multidata */
      LCDTS_SPI_HANDLE.hdmatx->Init.MemInc = DMA_MINC_ENABLE;
      dmastatus.status |= DMA_STATUS_MULTIDATA;
      dmastatus.ptr = (uint32_t)pData;
    }

    dmastatus.size = Size;
    dmastatus.maxtrsize = DMA_MAXSIZE;

    if(Size > DMA_MAXSIZE)
      dmastatus.trsize = DMA_MAXSIZE;
    else /* the transaction can be performed with one DMA operation */
      dmastatus.trsize = Size;

    HAL_DMA_Init(LCDTS_SPI_HANDLE.hdmatx);
    HAL_SPI_Transmit_DMA(&LCDTS_SPI_HANDLE, (uint8_t *)dmastatus.ptr, dmastatus.trsize);
    LcdDmaWaitEnd(Mode & LCD_IO_MULTIDATA);
  }
  else
  #endif
  { /* not DMA mode */
    if(Mode & LCD_IO_FILL)
    { /* fill */
      while(Size--) /* fill 8 and 16bit */
        HAL_SPI_Transmit(&LCDTS_SPI_HANDLE, (uint8_t *)pData, 1, LCDTS_SPI_TIMEOUT);
    }
    else
    { /* multidata */
      uint32_t trsize;
      while(Size)
      {
        if(Size > DMA_MAXSIZE)
        {
          trsize = DMA_MAXSIZE;
          Size -= DMA_MAXSIZE;
        }
        else
        {
          trsize = Size;
          Size = 0;
        }
        HAL_SPI_Transmit(&LCDTS_SPI_HANDLE, pData, trsize, LCDTS_SPI_TIMEOUT);
        if(Mode & LCD_IO_DATA8)
          pData += trsize;
        else
          pData += (trsize << 1);
      }
    }
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    LcdTransEnd();
  }
}

//-----------------------------------------------------------------------------
/* Wrtite fill and multi data to Lcd (convert RGB16 bit (5-6-5) to RGB24 bit (8-8-8) mode, no dma capability)
   - pData: RGB 16 bits data pointer
   - Size: data number
   - Mode: 8 or 16 or 24 bit mode, write or read, fill or multidata (see the LCD_IO_... defines in lcd_io.h file) */
void LCDWriteFillMultiData16to24(uint8_t * pData, uint32_t Size, uint32_t Mode)
{
  LcdSpiMode8();

  #if LCD_DMA_TX == 1 && LCD_RGB24_BUFFSIZE > 0
  if(Size > DMA_MINSIZE)
  { /* DMA mode */
    LCDTS_SPI_HANDLE.hdmatx->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    LCDTS_SPI_HANDLE.hdmatx->Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    LCDTS_SPI_HANDLE.hdmatx->Init.MemInc = DMA_MINC_ENABLE;
    HAL_DMA_Init(LCDTS_SPI_HANDLE.hdmatx);

    dmastatus.maxtrsize = LCD_RGB24_BUFFSIZE;
    dmastatus.size = Size;

    if(Size > LCD_RGB24_BUFFSIZE)
      dmastatus.trsize = LCD_RGB24_BUFFSIZE;
    else
      dmastatus.trsize = Size;

    if(Mode & LCD_IO_FILL)
    { /* fill 16bit to 24bit */
      dmastatus.status = DMA_STATUS_FILL | DMA_STATUS_24BIT;
      FillConvert16to24(*(uint16_t *)pData, lcd_rgb24_buffer, dmastatus.trsize);
    }
    else
    { /* multidata 16bit to 24bit */
      dmastatus.status = DMA_STATUS_MULTIDATA | DMA_STATUS_24BIT;
      dmastatus.ptr = (uint32_t)pData;
      BitmapConvert16to24((uint16_t *)pData, lcd_rgb24_buffer, dmastatus.trsize);
    }

    HAL_SPI_Transmit_DMA(&LCDTS_SPI_HANDLE, lcd_rgb24_buffer, dmastatus.trsize * 3);
    LcdDmaWaitEnd(Mode & LCD_IO_MULTIDATA);
  }
  else
  #endif
  { /* not DMA mode */
    #if LCD_RGB24_BUFFSIZE == 0
    uint32_t rgb888;
    if(Mode & LCD_IO_FILL)
    { /* fill 16bit to 24bit */
      rgb888 = RGB565TO888(*pData);
      while(Size--)
        HAL_SPI_Transmit(&LCDTS_SPI_HANDLE, (uint8_t *)&rgb888, 3, LCDTS_SPI_TIMEOUT);
    }
    else
    { /* multidata 16bit to 24bit */
      while(Size--)
      {
        rgb888 = RGB565TO888(*pData);
        HAL_SPI_Transmit(&LCDTS_SPI_HANDLE, (uint8_t *)&rgb888, 3, LCDTS_SPI_TIMEOUT);
        pData++;
      }
    }
    #elif LCD_RGB24_BUFFSIZE > 0
    uint32_t trsize;
    if(Mode & LCD_IO_FILL)
    { /* fill */
      if(Size > LCD_RGB24_BUFFSIZE)
        trsize = LCD_RGB24_BUFFSIZE;
      else
        trsize = Size;
      FillConvert16to24(*(uint16_t *)pData, lcd_rgb24_buffer, trsize);
      while(Size)
      {
        if(Size > LCD_RGB24_BUFFSIZE)
        {
          trsize = LCD_RGB24_BUFFSIZE;
          Size -= LCD_RGB24_BUFFSIZE;
        }
        else
        {
          trsize = Size;
          Size = 0;
        }
        HAL_SPI_Transmit(&LCDTS_SPI_HANDLE, lcd_rgb24_buffer, trsize * 3, LCDTS_SPI_TIMEOUT);
      }
    }
    else
    { /* bitmap */
      while(Size)
      {
        if(Size > LCD_RGB24_BUFFSIZE)
        {
          trsize = LCD_RGB24_BUFFSIZE;
          Size -= LCD_RGB24_BUFFSIZE;
        }
        else
        {
          trsize = Size;
          Size = 0;
        }
        BitmapConvert16to24((uint16_t *)pData, lcd_rgb24_buffer, trsize);
        HAL_SPI_Transmit(&LCDTS_SPI_HANDLE, lcd_rgb24_buffer, trsize * 3, LCDTS_SPI_TIMEOUT);
        pData += trsize;
      }
    }
    #endif /* #elif LCD_RGB24_BUFFSIZE > 0 */
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    LcdTransEnd();
  }
}

//=============================================================================
/* Read */

#if LCD_SPI_MODE != 0

/* RX DMA */
#if LCD_DMA_RX == 1

//-----------------------------------------------------------------------------
/* DMA operation end callback function prototype */
__weak void LCD_IO_DmaRxCpltCallback(SPI_HandleTypeDef *hspi)
{
  UNUSED(hspi);
}

//-----------------------------------------------------------------------------
/* SPI DMA operation interrupt */
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
  uint32_t dma_status = dmastatus.status;
  if(hspi == &LCDTS_SPI_HANDLE)
  {
    #if LCD_RGB24_BUFFSIZE > 0
    if(dma_status == (DMA_STATUS_MULTIDATA | DMA_STATUS_24BIT))
    {
      BitmapConvert24to16(lcd_rgb24_buffer, (uint16_t *)dmastatus.ptr, dmastatus.trsize);
      dmastatus.ptr += dmastatus.trsize << 1; /* 24bit multidata */
    }
    else
    #endif
    {
      if(dma_status == (DMA_STATUS_MULTIDATA | DMA_STATUS_8BIT))
        dmastatus.ptr += dmastatus.trsize;        /* 8bit multidata */
      else if(dma_status == (DMA_STATUS_MULTIDATA | DMA_STATUS_16BIT))
        dmastatus.ptr += dmastatus.trsize << 1; /* 16bit multidata */
    }

    if(dmastatus.size > dmastatus.trsize)
    { /* dma operation is still required */
      dmastatus.size -= dmastatus.trsize;
      if(dmastatus.size <= dmastatus.maxtrsize)
        dmastatus.trsize = dmastatus.size;

      #if LCD_RGB24_BUFFSIZE > 0
      if(dma_status == (DMA_STATUS_MULTIDATA | DMA_STATUS_24BIT))
        HAL_SPI_Receive_DMA(&LCDTS_SPI_HANDLE, lcd_rgb24_buffer, dmastatus.trsize * 3);
      else
      #endif
        HAL_SPI_Receive_DMA(&LCDTS_SPI_HANDLE, (uint8_t *)dmastatus.ptr, dmastatus.trsize);
    }
    else
    { /* dma operations have ended */
      HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
      LcdDirWrite();
      LcdDmaTransEnd();
      LCD_IO_DmaRxCpltCallback(hspi);
    }
  }
}
#endif  /* #if LCD_DMA_RX == 1 */

//-----------------------------------------------------------------------------
/* Read data from Lcd
   - pData: 8 or 16 bits data pointer
   - Size: data number
   - Mode: 8 or 16 or 24 bit mode, write or read, fill or multidata (see the LCD_IO_... defines in lcd_io.h file) */
void LCDReadMultiData8and16(uint8_t * pData, uint32_t Size, uint32_t Mode)
{
  if(Mode & LCD_IO_DATA8)
    LcdSpiMode8();
  else
    LcdSpiMode16();

  #if LCD_DMA_RX == 1
  if((Size > DMA_MINSIZE) && (!LCD_DMA_UNABLE((uint32_t)pData)))
  { /* DMA mode */
    /* SPI RX DMA setting (8bit, multidata) */
    if(Mode & LCD_IO_DATA8)
    {
      LCDTS_SPI_HANDLE.hdmarx->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
      LCDTS_SPI_HANDLE.hdmarx->Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
      dmastatus.status = DMA_STATUS_MULTIDATA | DMA_STATUS_8BIT;
    }
    else
    {
      LCDTS_SPI_HANDLE.hdmarx->Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
      LCDTS_SPI_HANDLE.hdmarx->Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
      dmastatus.status = DMA_STATUS_MULTIDATA | DMA_STATUS_16BIT;
    }
    LCDTS_SPI_HANDLE.hdmarx->Init.MemInc = DMA_MINC_ENABLE;
    HAL_DMA_Init(LCDTS_SPI_HANDLE.hdmarx);

    dmastatus.maxtrsize = DMA_MAXSIZE;
    dmastatus.size = Size;

    if(Size > DMA_MAXSIZE)
      dmastatus.trsize = DMA_MAXSIZE;
    else
      dmastatus.trsize = Size;

    dmastatus.ptr = (uint32_t)pData;

    HAL_SPI_Receive_DMA(&LCDTS_SPI_HANDLE, pData, dmastatus.trsize);
    LcdDmaWaitEnd(1);
  }
  else
  #endif
  { /* not DMA mode */
    uint32_t trsize;
    while(Size)
    {
      if(Size > DMA_MAXSIZE)
      {
        trsize = DMA_MAXSIZE;
        Size -= DMA_MAXSIZE;
      }
      else
      {
        trsize = Size;
        Size = 0;
      }
      HAL_SPI_Receive(&LCDTS_SPI_HANDLE, pData, trsize, LCDTS_SPI_TIMEOUT);
      if(Mode & LCD_IO_DATA8)
        pData += trsize;
      else
        pData += (trsize << 1);
    }
    LcdDirWrite();
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    LcdTransEnd();
  }
}

//-----------------------------------------------------------------------------
/* Read 24bit (8-8-8) RGB data from LCD, and convert to 16bit (5-6-5) RGB data
   - pData: 16 bits RGB data pointer
   - Size: pixel number
   - Mode: 8 or 16 or 24 bit mode, write or read, fill or multidata (see the LCD_IO_... defines in lcd_io.h file) */
void LCDReadMultiData24to16(uint8_t * pData, uint32_t Size, uint32_t Mode)
{
  LcdSpiMode8();
  #if LCD_DMA_RX == 1 && LCD_RGB24_BUFFSIZE > 0
  if(Size > DMA_MINSIZE)
  { /* DMA mode */
    /* SPI RX DMA setting (8bit, multidata) */
    LCDTS_SPI_HANDLE.hdmarx->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    LCDTS_SPI_HANDLE.hdmarx->Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    LCDTS_SPI_HANDLE.hdmarx->Init.MemInc = DMA_MINC_ENABLE;
    HAL_DMA_Init(LCDTS_SPI_HANDLE.hdmarx);

    dmastatus.maxtrsize = LCD_RGB24_BUFFSIZE;
    dmastatus.size = Size;

    if(Size > LCD_RGB24_BUFFSIZE)
      dmastatus.trsize = LCD_RGB24_BUFFSIZE;
    else
      dmastatus.trsize = Size;

    dmastatus.status = DMA_STATUS_MULTIDATA | DMA_STATUS_24BIT;
    dmastatus.ptr = (uint32_t)pData;

    HAL_SPI_Receive_DMA(&LCDTS_SPI_HANDLE, lcd_rgb24_buffer, dmastatus.trsize * 3);
    LcdDmaWaitEnd(1);
  }
  else
  #endif
  { /* not DMA mode */
    #if LCD_RGB24_BUFFSIZE == 0
    uint32_t rgb888;
    while(Size--)
    {
      HAL_SPI_Receive(&LCDTS_SPI_HANDLE, (uint8_t *)&rgb888, 3, LCDTS_SPI_TIMEOUT);
      *(uint16_t *)pData = RGB888TO565(rgb888);
      pData += 2;
    }
    #elif LCD_RGB24_BUFFSIZE > 0
    uint32_t trsize;
    while(Size)
    {
      if(Size > LCD_RGB24_BUFFSIZE)
      {
        trsize = LCD_RGB24_BUFFSIZE;
        Size -= LCD_RGB24_BUFFSIZE;
      }
      else
      {
        trsize = Size;
        Size = 0;
      }
      HAL_SPI_Receive(&LCDTS_SPI_HANDLE, lcd_rgb24_buffer, trsize * 3, LCDTS_SPI_TIMEOUT);
      BitmapConvert24to16(lcd_rgb24_buffer, (uint16_t *)pData, trsize);
      pData += (trsize << 1);
    }
    #endif
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    LcdDirWrite();
    LcdTransEnd();
  }
}

#endif /* #if LCD_SPI_MODE != 0 */

//=============================================================================
/* Public functions */

/* n millisec delay */
void LCD_Delay(uint32_t Delay)
{
  #ifndef  osCMSIS
  HAL_Delay(Delay);
  #else
  osDelay(Delay);
  #endif
}

/* Backlight on-off (Bl=0 -> off, Bl=1 -> on) */
//-----------------------------------------------------------------------------
void LCD_IO_Bl_OnOff(uint8_t Bl)
{
  #if defined(LCD_BL_GPIO_Port) && defined (LCD_BL_Pin)
  if(Bl)
    #if LCD_BLON == 0
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET);
    #elif LCD_BLON == 1
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
    #endif
  else
    #if LCD_BLON == 0
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
    #elif LCD_BLON == 1
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET);
    #endif
  #endif
}

//-----------------------------------------------------------------------------
/* Lcd IO init, reset, spi speed init, get the freertos task id */
void LCD_IO_Init(void)
{
  #if defined(LCD_RST_GPIO_Port) && defined (LCD_RST_Pin)
  LCD_Delay(10);
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
  LCD_Delay(10);
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
  #endif
  LCD_Delay(10);
  #if defined(LCD_SPI_SPD_WRITE)
  LCD_SPI_SETBAUDRATE(LCDTS_SPI_HANDLE, LCD_SPI_SPD_WRITE);
  #endif
  LcdTransInit();
}

//-----------------------------------------------------------------------------
/* Lcd IO transaction
   - Cmd: 8 or 16 bits command
   - pData: 8 or 16 bits data pointer
   - Size: data number
   - DummySize: dummy byte number at read
   - Mode: 8 or 16 or 24 bit mode, write or read, fill or multidata (see the LCD_IO_... defines in lcd_io.h file) */
void LCD_IO_Transaction(uint16_t Cmd, uint8_t *pData, uint32_t Size, uint32_t DummySize, uint32_t Mode)
{
  #if LCD_SPI_MODE == 0  /* only TX mode */
  if(Mode & LCD_IO_READ)
    return;
  #endif

  LcdTransStart();
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);

  /* Command write */
  if(Mode & LCD_IO_CMD8)
    LcdSpiMode8();
  else if(Mode & LCD_IO_CMD16)
    LcdSpiMode16();
  HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&LCDTS_SPI_HANDLE, (uint8_t *)&Cmd, 1, LCDTS_SPI_TIMEOUT); /* CMD write */
  HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_SET);

  if(Size == 0)
  { /* only command byte or word */
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    LcdTransEnd();
    return;
  }

  /* Datas write or read */
  if(Mode & LCD_IO_WRITE)
  { /* Write Lcd */
    if(Mode & LCD_IO_DATA16TO24)
      LCDWriteFillMultiData16to24(pData, Size, Mode);
    else
      LCDWriteFillMultiData8and16(pData, Size, Mode);
  }
  #if LCD_SPI_MODE != 0
  else if(Mode & LCD_IO_READ)
  { /* Read LCD */
    LcdDirRead((DummySize << 3) + LCD_SCK_EXTRACLK);
    if(Mode & LCD_IO_DATA24TO16)
      LCDReadMultiData24to16(pData, Size, Mode);
    else
      LCDReadMultiData8and16(pData, Size, Mode);
  }
  #endif /* #if LCD_SPI_MODE != 0 */
}

//=============================================================================

/* if not used the TS_IRQ pin -> Z1-Z2 touch sensitivy */
#define TS_ZSENS              128

#define TOUCH_FILTER          16
#define TOUCH_MAXREPEAT       8

#define XPT2046_MODE          0
#define XPT2046_SER           0
#define XPT2046_PD            0
#define XPT2046_CMD_GETTEMP   ((1 << 7) | (0 << 4) | (XPT2046_MODE << 3) | (XPT2046_SER << 2) | XPT2046_PD)
#define XPT2046_CMD_GETTVBAT  ((1 << 7) | (2 << 4) | (XPT2046_MODE << 3) | (XPT2046_SER << 2) | XPT2046_PD)
#define XPT2046_CMD_GETX      ((1 << 7) | (5 << 4) | (XPT2046_MODE << 3) | (XPT2046_SER << 2) | XPT2046_PD)
#define XPT2046_CMD_GETY      ((1 << 7) | (1 << 4) | (XPT2046_MODE << 3) | (XPT2046_SER << 2) | XPT2046_PD)
#define XPT2046_CMD_GETZ1     ((1 << 7) | (3 << 4) | (XPT2046_MODE << 3) | (XPT2046_SER << 2) | XPT2046_PD)
#define XPT2046_CMD_GETZ2     ((1 << 7) | (4 << 4) | (XPT2046_MODE << 3) | (XPT2046_SER << 2) | XPT2046_PD)

#define ABS(N)                (((N)<0) ? (-(N)) : (N))

//=============================================================================

static  uint16_t  tx, ty;

extern  SPI_HandleTypeDef     TS_SPI_HANDLE;

//=============================================================================
/* TS chip select pin set */
void    xpt2046_ts_Init(uint16_t DeviceAddr);
uint8_t xpt2046_ts_DetectTouch(uint16_t DeviceAddr);
void    xpt2046_ts_GetXY(uint16_t DeviceAddr, uint16_t *X, uint16_t *Y);

//=============================================================================
#ifdef  __GNUC__
#pragma GCC push_options
#pragma GCC optimize("O0")
#elif   defined(__CC_ARM)
#pragma push
#pragma O0
#endif
void TS_IO_Delay(uint32_t c)
{
  while(c--);
}
#ifdef  __GNUC__
#pragma GCC pop_options
#elif   defined(__CC_ARM)
#pragma pop
#endif
//-----------------------------------------------------------------------------
void TS_IO_Init(void)
{
}

//-----------------------------------------------------------------------------
uint16_t TS_IO_Transaction(uint8_t cmd)
{
  const uint16_t d = 0;
  uint16_t ret;
  LcdTransStart();
  LCD_SPI_SETBAUDRATE(LCDTS_SPI_HANDLE, TS_SPI_SPD);           /* speed change */
  LcdSpiMode8();
  HAL_GPIO_WritePin(TS_CS_GPIO_Port, TS_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&LCDTS_SPI_HANDLE, (uint8_t *)&cmd, 1, LCDTS_SPI_TIMEOUT);
  #if XPT2046_READDELAY > 0
  TS_IO_Delay(XPT2046_READDELAY);
  #endif
  HAL_SPI_TransmitReceive(&LCDTS_SPI_HANDLE, (uint8_t *)&d, (uint8_t *)&ret, 2, LCDTS_SPI_TIMEOUT);
  HAL_GPIO_WritePin(TS_CS_GPIO_Port, TS_CS_Pin, GPIO_PIN_SET);
  ret = __REVSH(ret);
  LCD_SPI_SETBAUDRATE(LCDTS_SPI_HANDLE, LCD_SPI_SPD_WRITE);       /* speed change */
  LcdTransEnd();
  return ((ret & 0x7FFF) >> 3);
}

//-----------------------------------------------------------------------------
/* return:
   - 0 : touchscreen is not pressed
   - 1 : touchscreen is pressed */
uint8_t TS_IO_DetectToch(void)
{
  uint8_t  ret;
  static uint8_t ts_inited = 0;
  if(!ts_inited)
  {
    TS_IO_Init();
    ts_inited = 1;
  }
  #if defined(TS_IRQ_GPIO_Port) && defined (TS_IRQ_Pin)
  if(HAL_GPIO_ReadPin(TS_IRQ_GPIO_Port, TS_IRQ_Pin))
    ret = 0;
  else
    ret = 1;
  #else
  if((TS_IO_Transaction(XPT2046_CMD_GETZ1) > TS_ZSENS) || (TS_IO_Transaction(XPT2046_CMD_GETZ2) < (4095 - TS_ZSENS)))
    ret = 1;
  else
    ret = 0;
  #endif
  return ret;
}

TS_DrvTypeDef   xpt2046_ts_drv =
{
  xpt2046_ts_Init,
  0,
  0,
  0,
  xpt2046_ts_DetectTouch,
  xpt2046_ts_GetXY,
  0,
  0,
  0,
  0
};

TS_DrvTypeDef  *ts_drv = &xpt2046_ts_drv;


//-----------------------------------------------------------------------------
void xpt2046_ts_Init(uint16_t DeviceAddr)
{
  const uint8_t c = XPT2046_CMD_GETY;
  #if defined(TS_IRQ_GPIO_Port) && defined (TS_IRQ_Pin)
  HAL_GPIO_WritePin(TS_CS_GPIO_Port, TS_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&LCDTS_SPI_HANDLE, (uint8_t *)&c, 1, LCDTS_SPI_TIMEOUT);
  HAL_GPIO_WritePin(TS_CS_GPIO_Port, TS_CS_Pin, GPIO_PIN_SET);
  #endif
}

//-----------------------------------------------------------------------------
uint8_t xpt2046_ts_DetectTouch(uint16_t DeviceAddr)
{
  uint8_t ret = 0;
  int32_t x1, x2, y1, y2, i;

  if(TS_IO_DetectToch())
  {
    x1 = TS_IO_Transaction(XPT2046_CMD_GETX); /* Get X */
    y1 = TS_IO_Transaction(XPT2046_CMD_GETY); /* Get Y */
    i = TOUCH_MAXREPEAT;
    while(i--)
    {
      x2 = TS_IO_Transaction(XPT2046_CMD_GETX); /* Get X */
      y2 = TS_IO_Transaction(XPT2046_CMD_GETY); /* Get Y */
      if((ABS(x1 - x2) < TOUCH_FILTER) && (ABS(y1 - y2) < TOUCH_FILTER))
      {
        x1 = (x1 + x2) >> 1;
        y1 = (y1 + y2) >> 1;
        i = 0;
        if(TS_IO_DetectToch())
        {
          tx = x1;
          ty = y1;
          ret = 1;
        }
      }
      else
      {
        x1 = x2;
        y1 = y2;
      }
    }
    #if defined(TS_IRQ_GPIO_Port) && defined (TS_IRQ_Pin)
    HAL_GPIO_WritePin(TS_CS_GPIO_Port, TS_CS_Pin, GPIO_PIN_RESET);
    i = XPT2046_CMD_GETY;
    HAL_SPI_Transmit(&LCDTS_SPI_HANDLE, (uint8_t *)&i, 1, LCDTS_SPI_TIMEOUT);
    HAL_GPIO_WritePin(TS_CS_GPIO_Port, TS_CS_Pin, GPIO_PIN_SET);
    #endif
  }
  return ret;
}

//-----------------------------------------------------------------------------
void xpt2046_ts_GetXY(uint16_t DeviceAddr, uint16_t *X, uint16_t *Y)
{
  *X = tx,
  *Y = ty;
}
