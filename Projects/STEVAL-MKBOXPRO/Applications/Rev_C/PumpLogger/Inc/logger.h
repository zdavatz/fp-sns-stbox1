/**
  ******************************************************************************
  * @file    logger.h
  * @brief   Top-level sensor logger. Owns the Sens/Gps/Bat CSVs.
  ******************************************************************************
  */
#ifndef PL_LOGGER_H
#define PL_LOGGER_H

#include <stdint.h>

int  Logger_Init(void);            /* opens session files; returns 0 on success */
void Logger_Tick(void);            /* called every 1 ms from main loop */
int  Logger_IsActive(void);
void Logger_FlushAll(void);

#endif
