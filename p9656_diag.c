/* Jungo Connectivity Confidential. Copyright (c) 2019 Jungo Connectivity Ltd.  https://www.jungo.com */

/************************************************************************
*  File: p9656_diag.c
*
*  Sample user-mode diagnostics application for accessing PLX 9656
*  devices using WinDriver's API and the plx_lib and plx_diag_lib
*  libraries.
*
*  Note: This code sample is provided AS-IS and as a guiding sample only.
*************************************************************************/

#include "diag_lib.h"
#include "wdc_diag_lib.h"
#include "../diag_lib/plx_diag_lib.h"

/*************************************************************
  General definitions
 *************************************************************/
/* Default vendor and device IDs */
#define P9656_DEFAULT_VENDOR_ID 0x10b5 /* Vendor ID */
#ifdef P9656_LITE_BOARD
    #define P9656_DEFAULT_DEVICE_ID 0x9601 /* Device ID */
#else
    #define P9656_DEFAULT_DEVICE_ID 0x9656 /* Device ID */
#endif

/* PLX 9656 is a master device */
#define IS_MASTER TRUE

extern void PLX_DIAG_MenuDmaTransactionInit(DIAG_MENU_OPTION *pParentMenu,
    WDC_DEVICE_HANDLE *phDev, PLX_DIAG_DMA *pDma,
    PLX_INT_HANDLER DiagDmaTransactionIntHandler);

/*************************************************************
  Static functions prototypes
 *************************************************************/
/* -----------------------------------------------
    Main diagnostics menu
   ----------------------------------------------- */
static DIAG_MENU_OPTION *MenuMainInit(WDC_DEVICE_HANDLE *phDev,
    PLX_DIAG_DMA *pDma);

/* -----------------------------------------------
    Interrupt handling
   ----------------------------------------------- */
static void DiagDmaIntHandler(WDC_DEVICE_HANDLE hDev,
    PLX_INT_RESULT *pIntResult);

static void DiagDmaTransactionIntHandler(WDC_DEVICE_HANDLE hDev,
    PLX_INT_RESULT *pIntResult);


/* ----------------------------------------------------
    Plug-and-play and power management events handling
   ---------------------------------------------------- */
static void DiagEventHandler(WDC_DEVICE_HANDLE hDev, DWORD dwAction);

/*************************************************************
  Functions implementation
 *************************************************************/
static DWORD PLX_Init(WDC_DEVICE_HANDLE *phDev)
{
    /* Initialize the PLX library */
    DWORD dwStatus = PLX_LibInit();
    if (WD_STATUS_SUCCESS != dwStatus)
    {
        PLX_DIAG_ERR("p9656_diag: Failed to initialize the PLX library: %s",
            PLX_GetLastErr());
        return dwStatus;
    }

    /* Find and open a PLX 9656 device (by default ID) */
    if (P9656_DEFAULT_VENDOR_ID)
    {
        *phDev = PLX_DeviceOpen(P9656_DEFAULT_VENDOR_ID,
            P9656_DEFAULT_DEVICE_ID, IS_MASTER);
        if (!*phDev)
        {
            PLX_DIAG_ERR("p9656_diag: Failed locating and opening a handle to "
                "device (VID 0x%x DID 0x%x)\n", P9656_DEFAULT_VENDOR_ID,
                P9656_DEFAULT_DEVICE_ID);
        }
    }

    return WD_STATUS_SUCCESS;
}

int main(void)
{
    DWORD dwStatus;
    WDC_DEVICE_HANDLE hDev = NULL;
    PLX_DIAG_DMA dma;
    DIAG_MENU_OPTION *pMenuRoot;

    BZERO(dma);

    printf("\n");
    printf("PLX 9656 diagnostic utility.\n");
    printf("Application accesses hardware using " WD_PROD_NAME ".\n");

    /* Initialize the PLX library */
    dwStatus = PLX_Init(&hDev);
    if (WD_STATUS_SUCCESS != dwStatus)
        return dwStatus;

    pMenuRoot = MenuMainInit(&hDev, &dma);

    /* Busy loop that runs the menu tree created above and communicates
        with the user */
    return DIAG_MenuRun(pMenuRoot);
}
/* -----------------------------------------------
    Main diagnostics menu
   ----------------------------------------------- */

static DWORD MenuMainExitCb(PVOID pCbCtx)
{
    DWORD dwStatus = WD_STATUS_SUCCESS;
    PLX_MENU_CTX *pPlxMenuCtx = (PLX_MENU_CTX *)pCbCtx;

    /* Perform necessary cleanup before exiting the program */
    if (pPlxMenuCtx->pDma->hDma)
        PLX_DIAG_DMAClose(*(pPlxMenuCtx->phDev), pPlxMenuCtx->pDma);

    if (*(pPlxMenuCtx->phDev) && !PLX_DeviceClose(*(pPlxMenuCtx->phDev)))
        PLX_DIAG_ERR("p9656_diag: Failed closing PLX device: %s",
            PLX_GetLastErr());

    dwStatus = PLX_LibUninit();
    if (WD_STATUS_SUCCESS != dwStatus)
    {
        PLX_DIAG_ERR("p9656_diag: Failed to uninit the PLX library: %s",
            PLX_GetLastErr());
    }

    return WD_STATUS_SUCCESS;
}

static DIAG_MENU_OPTION *MenuMainInit(WDC_DEVICE_HANDLE *phDev,
    PLX_DIAG_DMA *pDma)
{
    BOOL fIsDACSupported;
    static DIAG_MENU_OPTION menuRoot = { 0 };
    static PLX_MENU_CTX plxMenuCtx = { 0 };

    strcpy(menuRoot.cTitleName, "PLX 9656 main menu");
    menuRoot.cbExit = MenuMainExitCb;

    plxMenuCtx.phDev = phDev;
    plxMenuCtx.pDma = pDma;
    plxMenuCtx.fIsMaster = IS_MASTER;

#ifdef P9656_LITE_BOARD
    fIsDACSupported = FALSE;
#else
    fIsDACSupported = TRUE;
#endif

    menuRoot.pCbCtx = &plxMenuCtx;

    MenuCommonScanBusInit(&menuRoot);
    PLX_DIAG_MenuDeviceOpenInit(&menuRoot, &plxMenuCtx);
    PLX_DIAG_MenuReadWriteAddrInit(&menuRoot, phDev);
    PLX_DIAG_MenuReadWriteCfgSpaceInit(&menuRoot, phDev);
    PLX_DIAG_MenuReadWriteRegsInit(&menuRoot, phDev);
    PLX_DIAG_MenuDmaInit(&menuRoot, phDev, pDma, DiagDmaIntHandler,
        fIsDACSupported);
    PLX_DIAG_MenuDmaTransactionInit(&menuRoot, phDev, pDma, DiagDmaTransactionIntHandler);
    PLX_DIAG_MenuEventsInit(&menuRoot, phDev, DiagEventHandler);
    PLX_DIAG_MenuEEPROMInit(&menuRoot, phDev, BIT7);
    PLX_DIAG_MenuResetBoardInit(&menuRoot, phDev);

    return &menuRoot;
}

/* -----------------------------------------------
    Interrupt handling
   ----------------------------------------------- */
/* Diagnostics interrupt handler routine */
static void DiagDmaIntHandler(WDC_DEVICE_HANDLE hDev,
    PLX_INT_RESULT *pIntResult)
{
    /* TODO: add your own code here */

    PPLX_DEV_CTX pDevCtx = (PPLX_DEV_CTX)WDC_GetDevContext(hDev);
    PLX_DMA_STRUCT *pPLXDma = (PLX_DMA_STRUCT *)pDevCtx->pPLXDma;
    WD_DMA *pDma = (WD_DMA *)pPLXDma->pDma;

    printf("\nGot DMA interrupt, counter %d, INTCSR 0x%X, DMACSR 0x%X\n",
        pIntResult->dwCounter, pIntResult->u32INTCSR,
        (UINT32)pIntResult->bDMACSR);

    DIAG_PrintPerformance(pDma->dwBytes, &pPLXDma->dmaStartTime);
}

static void DiagDmaTransactionIntHandler(WDC_DEVICE_HANDLE hDev,
    PLX_INT_RESULT *pIntResult)
{
    DWORD dwStatus;
    PPLX_DEV_CTX pDevCtx = (PPLX_DEV_CTX)WDC_GetDevContext(hDev);
    PLX_DMA_STRUCT *pPLXDma = (PLX_DMA_STRUCT *)pDevCtx->pPLXDma;
    WD_DMA *pDma = (WD_DMA *)pPLXDma->pDma;

    printf("Got DMA interrupt (DMA transfer has been finished [%u pages]), "
        "counter %d\n", pDma->dwPages, pIntResult->dwCounter);

    dwStatus = WDC_DMATransferCompletedAndCheck(pDma, TRUE);
    if (dwStatus == WD_STATUS_SUCCESS)
    {
        printf("DMA transaction has been completed\n");
        DIAG_PrintPerformance(pDma->dwBytes, &pPLXDma->dmaStartTime);
    }
    else if (dwStatus != (DWORD)WD_MORE_PROCESSING_REQUIRED)
    {
        printf("DMA transfer failed\n");
    }
}

/* ----------------------------------------------------
    Plug-and-play and power management events handling
   ---------------------------------------------------- */
/* Plug-and-play and power management events handler routine */
static void DiagEventHandler(WDC_DEVICE_HANDLE hDev, DWORD dwAction)
{
    /* TODO: You can modify this function in order to implement your own
             diagnostics events handler routine */

    printf("\nReceived event notification (device handle 0x%p): ", hDev);
    switch (dwAction)
    {
    case WD_INSERT:
        printf("WD_INSERT\n");
        break;
    case WD_REMOVE:
        printf("WD_REMOVE\n");
        break;
    case WD_POWER_CHANGED_D0:
        printf("WD_POWER_CHANGED_D0\n");
        break;
    case WD_POWER_CHANGED_D1:
        printf("WD_POWER_CHANGED_D1\n");
        break;
    case WD_POWER_CHANGED_D2:
        printf("WD_POWER_CHANGED_D2\n");
        break;
    case WD_POWER_CHANGED_D3:
        printf("WD_POWER_CHANGED_D3\n");
        break;
    case WD_POWER_SYSTEM_WORKING:
        printf("WD_POWER_SYSTEM_WORKING\n");
        break;
    case WD_POWER_SYSTEM_SLEEPING1:
        printf("WD_POWER_SYSTEM_SLEEPING1\n");
        break;
    case WD_POWER_SYSTEM_SLEEPING2:
        printf("WD_POWER_SYSTEM_SLEEPING2\n");
        break;
    case WD_POWER_SYSTEM_SLEEPING3:
        printf("WD_POWER_SYSTEM_SLEEPING3\n");
        break;
    case WD_POWER_SYSTEM_HIBERNATE:
        printf("WD_POWER_SYSTEM_HIBERNATE\n");
        break;
    case WD_POWER_SYSTEM_SHUTDOWN:
        printf("WD_POWER_SYSTEM_SHUTDOWN\n");
        break;
    default:
        printf("0x%x\n", dwAction);
        break;
    }
}


