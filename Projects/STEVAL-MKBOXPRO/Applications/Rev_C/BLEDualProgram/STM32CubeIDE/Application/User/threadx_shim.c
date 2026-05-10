/*
 * threadx_shim.c — minimal stubs so the SDDataLogFileX-branch hci_tl.c
 * (which references tx_time_get / tx_thread_relinquish / ErrorLog_Write)
 * can be linked into a non-ThreadX, non-FileX baseline like BLEDualProgram
 * without modifying the middleware source itself.
 *
 * tx_api.h #define-maps the public names to underscore-prefixed internals:
 *   tx_time_get          -> _tx_time_get
 *   tx_thread_relinquish -> _txe_thread_relinquish
 * so we provide the underscore-prefixed implementations to satisfy the
 * linker references that hci_tl.c emits after macro expansion.
 *
 * Behaviour:
 *  - _tx_time_get           : returns HAL tick count divided by 10
 *                             (10ms-tick semantics the caller expects).
 *  - _txe_thread_relinquish : no-op (no scheduler to yield to).
 *  - ErrorLog_Write         : no-op (no SD-card error log in this app).
 *
 * Only used by the Makefile build of BLEDualProgram. The IDE build doesn't
 * compile this file (it's not in .project linkedResources).
 */

#include <stdint.h>
#include "stm32u5xx_hal.h"

/* ULONG/UINT/VOID — declare locally so we don't drag in tx_api.h
 * (which would also re-declare the public names as macros and fight us). */
typedef unsigned long  ULONG;
typedef unsigned int   UINT;
#define VOID void

ULONG _tx_time_get(VOID)
{
  /* HAL_GetTick() returns ms; the caller treats the result as 10ms-ticks,
   * so divide by 10 to get the same units. */
  return (ULONG)(HAL_GetTick() / 10U);
}

UINT _txe_thread_relinquish(VOID)
{
  /* No scheduler — nothing to yield to. */
  return 0U; /* TX_SUCCESS */
}

void ErrorLog_Write(const char *msg)
{
  (void)msg;
  /* No SD-card error log in BLEDualProgram. */
}
