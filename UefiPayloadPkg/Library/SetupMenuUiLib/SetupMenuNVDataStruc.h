#ifndef _SETUPMENUNVDATASTRUC_H_
#define _SETUPMENUNVDATASTRUC_H_

#pragma pack(1)
typedef struct {

  UINT8 Hyperthreading;
  UINT8 TurboMode;
  UINT8 Cx;
  UINT8 CxLimit;
  UINT8 PrimaryDisplay;
  UINT8 EnergyEfficientTurbo;
  UINT8 LlcDeadline;
  UINT8 IntelVtx;
  UINT8 IntelVtd;
  UINT8 SecureBoot;
  UINT8 PxeRetries;
  UINT8 PwrG3;
  UINT8 PcieSsc;
  UINT8 PcieSris;
  UINT8 Ibecc;
    
} SETUPMENU_CONFIGURATION;

#endif
