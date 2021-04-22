#include <vector>
#include <numeric>
#include <bitset>
#include "TFile.h"
#include "TGraph.h"
#include "TROOT.h"
#include "TSystem.h"
#include <TSystemDirectory.h>
#include "TPad.h"
#include "TTree.h"
#include "AFISettings.h"
#include "AFIErrors.h"

#ifndef AFI_DECODER_H
#define AFI_DECODER_H

#define SYNC_WORD_TIME  0x3f60b8a8
#define SYNC_WORD       0x2A502A50

// Useful for indexation of event positions to speed up reading out
typedef struct {
   size_t ev;			// event number
   size_t po;			// event header position
   UInt_t sn;     // board serial number
} FILEINDEX;

// Event block header struct
struct ADC64_EVENT_TIME_HDR_BLOCK {
 uint32_t sword;                // sync word
 uint32_t length;               // Event time payload length: (S1+S2+...) bytes
 uint32_t time_lo;              // unix time of event from system call
 uint32_t time_hi;              // unix time of event from system call
 ADC64_EVENT_TIME_HDR_BLOCK() { clear(); }
 void clear()
 {
   sword  = 0;
   length = 0;
   time_lo  = 0;
   time_hi  = 0;
 }
};

// Event block header struct
struct ADC64_EVENT_HDR_BLOCK {
 uint32_t sword;                // sync word
 uint32_t length;                  // Event payload length: (S1+S2+...) bytes
 uint32_t evnum;                  // event number
 ADC64_EVENT_HDR_BLOCK() { clear(); }
 void clear()
 {
   sword  = 0;
   length = 0;
   evnum  = 0;
 }
};

// Device event block header struct
struct ADC64_DEVICE_EVENT_HDR_BLOCK {
  uint32_t sn;                        // device sn
  uint32_t id;                        // device id
  uint32_t length;                    // device payload length
  ADC64_DEVICE_EVENT_HDR_BLOCK() { clear(); }
  void clear()
  {
    sn     = 0;
    id     = 0;
    length = 0;
  }
};

// MStream block header struct
struct ADC64_MSTREAM_HDR_BLOCK{
  uint32_t ch;                      // Subtype-defined bits // just channel number
  uint32_t length;                    // MStream Payload Length: S 32-bit words
  uint32_t type;                      // MStream Subtype
  ADC64_MSTREAM_HDR_BLOCK() { clear(); }
  void clear()
  {
    ch     = 0;
    length = 0;
    type   = 0;
  }
};

// MStream Subtype 0 payload struct
struct ADC64_SUBTYPE_0_PAYLOAD{
  uint32_t taisec;                       // Event timestamp, TAI seconds
  uint32_t tainsec;                      // Event timestamp, TAI nanoseconds
  uint8_t  taiflags;                     // Event timestamp, TAI flags
  uint32_t  chlo;                        // readout channels 31:0 bit mask
  uint32_t  chup;                        // readout channels 63:32 bit mask
  ADC64_SUBTYPE_0_PAYLOAD() { clear(); }
  void clear()
  {
    taisec   = 0;
    tainsec  = 0;
    taiflags = 0;
    chlo     = 0;
    chup     = 0;
  }
};

struct ADC64_SUBTYPE_1_PAYLOAD{
  uint32_t wf_tsup;
  uint32_t wf_tslo;
  uint32_t tai_flags;
  ADC64_SUBTYPE_1_PAYLOAD() { clear(); }
  void clear()
  {
    wf_tsup   = 0;
    wf_tslo   = 0;
    tai_flags = 0;
  }
};

// TQDC2 Data structs //
struct TQDC2_ADCDATA
{
  uint16_t timestamp;               // ADC timestamp is number of 8 ns intervals after event timestamp.
  uint16_t length;                  // ADC data length (in bytes)
  TQDC2_ADCDATA()  { clear(); }
  void clear()
  {
    timestamp = 0;
    length    = 0;
  }
};

struct TQDC2_TDCDATA
{
  uint8_t   n;                      // word order number
  uint16_t  timestamp;              // TDC timestamp - TDC timestamp in number of 25 ns time intervals since global trigger timestamp found in event header
  uint16_t  evnum;                  // Event number
  uint8_t   id;                     // TDC ID
  uint16_t  wcount;                 // TDC word count
  uint8_t   ch;                     // channel
  uint32_t  ledge;                  // TDC data, time of leading edge - number of 100 ps time intervals since global trigger timestamp found in event header;
  uint32_t  tedge;                  // TDC data, time of trailing edge - number of 100 ps time intervals since global trigger timestamp found in event header;
  uint8_t   rcdata;                 // rcdata
  uint16_t  err;                    // TDC error flags bits:
                                      // [0] Hit lost in group 0 from read-out fifo overflow
                                      // [1] Hit lost in group 0 from L1 buffer overflow
                                      // [2] Hit error have been detected in group 0
                                      // [3] Hit lost in group 1 from read-out fifo overflow
                                      // [4] Hit lost in group 1 from L1 buffer overflow
                                      // [5] Hit error have been detected in group 1
                                      // [6] Hit lost in group 2 from read-out fifo overflow
                                      // [7] Hit lost in group 2 from L1 buffer overflow
                                      // [8] Hit error have been detected in group 2
                                      // [9] Hit lost in group 3 from read-out fifo overflow
                                      // [10] Hit lost in group 3 from L1 buffer overflow
                                      // [11] Hit error have been detected in group 3
                                      // [12] Hits rejected because of programmed event size limit
                                      // [13] Event lost (trigger FIFO ovefrlow)
                                      // [14] Internal fatal chip error has been detected
  TQDC2_TDCDATA()  { clear(); }
  void clear()
  {
    n         = 0;
    timestamp = 0;
    evnum     = 0;
    id        = 0;
    wcount    = 0;
    ch        = 0;
    ledge     = 0;
    tedge     = 0;
    err       = 0;
  }
};

// Data block
struct TQDC2_DATA_HDR_BLOCK{
  uint8_t  type;                     // ADC data type 0x1, TDC data type 0x0
  uint8_t  ch;                       // Channel number
  uint8_t  spec;                     // ADC Data Block specific
  uint16_t length;                   // Data Payload length (4*(N-1) bytes)

  TQDC2_DATA_HDR_BLOCK() { clear(); }
  void clear()
  {
    type   = 0;
    ch     = 0;
    spec   = 0;
    length = 0;
  }
};

// MStream block
struct TQDC2_MSTREAM_HDR_BLOCK
{
  uint32_t subtype;                      // MStream subtype
  uint32_t length;                       // MStream Payload Length S 32-bit words
  uint32_t ch;                           // Subtype-defined bits

  uint32_t taisec;                       // Event timestamp, TAI seconds
  uint32_t tainsec;                      // Event timestamp, TAI nanoseconds
  uint8_t  taiflags;                     // Event timestamp, TAI flags
  TQDC2_MSTREAM_HDR_BLOCK() { clear(); }
  void clear()
  {
    subtype  = 0;
    ch       = 0;
    taisec   = 0;
    tainsec  = 0;
    taiflags = 0;
  }
};

// Device block
struct TQDC2_DEV_HDR_BLOCK
{
  uint32_t sn;
  uint32_t id;
  uint32_t length;
  TQDC2_DEV_HDR_BLOCK() { clear(); }
  void clear()
  {
    sn = 0;
    id = 0;
    length = 0;
  }
};

// Event block
struct TQDC2_EVENT_HDR_BLOCK
{
  uint32_t sword;
  uint32_t length;
  uint32_t evnum;
  TQDC2_EVENT_HDR_BLOCK() { clear(); }
  void clear()
  {
    sword  = 0;
    length = 0;
    evnum  = 0;
  }
};
//////////////////////////////////
// end TQDC2 struct declaration //
//////////////////////////////////

typedef struct {
  Bool_t      used   = false;
  ULong64_t   utime  = 0;                // unix time
  UInt_t      sn;                        // serial number
  UInt_t      cycle;                     // cycle number (valid for monitoring only)
  UChar_t     mode;                      // tag of processing type [a|i|t|w]
  UInt_t      event;                     // event number
  Float_t     data[CHANNEL_NUMBER];      // data to be written into the tree
  ULong64_t   wfts[CHANNEL_NUMBER];      // waveform arrival time
} DATALOG;

typedef struct {
  UInt_t      event;                     // event number
  UInt_t      sn;                        // serial number
  Float_t     amp[CHANNEL_NUMBER];       // data to be written into the tree
  Float_t     time[CHANNEL_NUMBER];      // data to be written into the tree
  Float_t     charge[CHANNEL_NUMBER];
  std::vector<int32_t> wf[CHANNEL_NUMBER];
} ASCIIDATALOG;

class AFIDecoder
{
  private:
    uint32_t                      uiEvent;
    uint32_t                      uiTotalEvents;
    uint32_t                      uiDeviceSN;
    uint32_t                      uiDeviceID;
    uint32_t                      uiDevType;     // stores device ids
    uint32_t                      uiDevTimeDiscr;      // device time discretization
    uint32_t                      uiDevChannelNum;  // device channel number
    uint64_t                      uiRecTime;        // start time of a recording data
    UInt_t                        uiConvDiffLeft;
    UInt_t                        uiConvDiffRight;
    Int_t                         iSignalOffset;
    char                          cDataFileName[1024];
    char                          cWorkDir[1024];
    char                          cGateFileName[512];
    char                          cRunTag[64];
    bool                          bFirstEvent;
    bool                          bIsEventTimeHdr;
    bool                          bVerbose;
    uint16_t                      uiSampleNumber;
    uint8_t                       uiDeviceNumber;
    FILEINDEX                     FileIndex;
    ADCS                          Adcs;
    WFPARS                        wfPars;
    WFPEAKS                       wfPeaks;
    KERNEL                        rKernel;
    KERNEL_PRECISED               pKernel;
    GATE                          Gate;
    GATE                          rkGate;
    PKGATE                        pkGate;
    ADCSETTINGS                   setCH;
    INTEGRAL                      Integral[THREAD_NUMBER];    // all about integral

    // Event data
    DATA                          data[THREAD_NUMBER];      // data container for ADC64
    DATA                          data_tqdc[THREAD_NUMBER]; // data container for TQDC

    // ADC64
    ADC64_EVENT_TIME_HDR_BLOCK    ADC64EventTimeHdr[THREAD_NUMBER]; // presents in the new format only
    ADC64_EVENT_HDR_BLOCK         ADC64EventHdr[THREAD_NUMBER];
    ADC64_DEVICE_EVENT_HDR_BLOCK  ADC64DevHdr[THREAD_NUMBER];
    ADC64_MSTREAM_HDR_BLOCK       ADC64MStreamHdr[THREAD_NUMBER];
    ADC64_SUBTYPE_0_PAYLOAD       ADC64Subtype0[THREAD_NUMBER];
    ADC64_SUBTYPE_1_PAYLOAD       ADC64Subtype1[THREAD_NUMBER];

    // TQDC
    TQDC2_EVENT_HDR_BLOCK         TQDC2EventHdr[THREAD_NUMBER];
    TQDC2_DEV_HDR_BLOCK           TQDC2DevHdr[THREAD_NUMBER];
    TQDC2_MSTREAM_HDR_BLOCK       TQDC2MStreamHdr[THREAD_NUMBER];
    TQDC2_DATA_HDR_BLOCK          TQDC2DataBlockHdr[THREAD_NUMBER];
    TQDC2_TDCDATA                 TQDC2Tdc[THREAD_NUMBER];
    TQDC2_ADCDATA                 TQDC2Adc[THREAD_NUMBER];
    //

    std::vector<FILEINDEX>        vFInd;	// indexes of positions of begining of events in the file - allows to speed up file read out much
    std::vector<ADCS>             vADCs;    // metadata for all of ADC devices
    std::vector<WFPEAKS>          vPeaks;  // consist info about found peaks in the waveform
    std::vector<FILEINDEX>::iterator itFInd;
    std::bitset<64>               bvCHState;
    std::vector<std::bitset<64>>  vCHState;
    std::vector<KERNEL>           vRKernel;
    std::vector<KERNEL_PRECISED>  vPKernel;
    std::vector<GATE>             vGate;
    std::vector<PKGATE>           vPKGate;
    std::vector<GATE>             vRKGate;
    std::vector<ADCSETTINGS>      vSetCH;       // info about signal settings for each channel
    std::vector<unsigned int>     vUsedKernelChannels{0,1,2,3,4,5,6,7,8,9};

    // for data logger
    DATALOG                       dataLog[THREAD_NUMBER];
    ASCIIDATALOG                  aDataLog[THREAD_NUMBER];
    std::vector<DATALOG>          vDataLog[THREAD_NUMBER];
    std::vector<ASCIIDATALOG>     vaDataLog[THREAD_NUMBER];

    uint32_t        FileIndexation(FILE *fd, UInt_t devsn);
    std::bitset<64> ReadActiveChannels(uint32_t lo, uint32_t up);
    void            ReadMetaDataADC64(FILE *fd, bool bVerbose = false);
    void            ReadMetaDataTQDC(FILE *fd, bool bVerbose = false);
    void            DefineDeviceType(FILE *fd);

  public:
    AFIDecoder();
    const char* GetClassName() { return "AFIDecoder"; }
    const char* GetRunTag()    { return cRunTag; }

    void            SetFileName(const char *);
    void            SetWorkDir(const char *);     // Forward directory with Viewer binaries
    UInt_t          LookingForDevices(const char * ccFile);
    void            SetConvDiffLeft(UInt_t value)   { uiConvDiffLeft  = value; }
    void            SetConvDiffRight(UInt_t value)  { uiConvDiffRight = value; }
    void            SetSignalOffset(Int_t offset) { iSignalOffset = offset; }
    FILE *          OpenDataFile(const char *,bool bIndexation = true, UInt_t devsn = 0x00000);
    void            ClearIndexation() { vFInd.clear(); }
    void            CloseDataFile(FILE *data=NULL);
    uint32_t        GetDeviceID() { return uiDeviceID; }
    uint32_t        GetDeviceSN() { return uiDeviceSN; }
    uint16_t        GetNumberOfSamples() { return uiSampleNumber; }
    uint8_t         GetNumberOfDevices() { return uiDeviceNumber; }
    uint32_t        GetTotalEvents() { return uiTotalEvents; } // to be removed
    std::vector<ADCS> *             GetADCMetaData() { return &vADCs; }
    std::vector<std::bitset<64>>    GetActiveChannels() { return vCHState; }
    std::vector<KERNEL> *           GetRoughKernelData() { return &vRKernel; }
    std::vector<KERNEL_PRECISED> *  GetPrecisedKernelData() { return &vPKernel; }
    std::vector<GATE> *             GetGateData() { return &vGate; }
    std::vector<PKGATE> *           GetPKernelGateData() { return &vPKGate; }
    std::vector<GATE> *             GetRKernelGateData() { return &vRKGate; }
    Int_t   ReadEventADC64(FILE *fd, uint32_t event, std::vector<DATA> *vData, \
                           bool bVerbose,Int_t devorn,UInt_t devsn, uint16_t thread_id);
    Int_t   ReadEventTQDC(FILE *fd, uint32_t event, std::vector<DATA> *vData,\
                           bool bVerbose,Int_t devorn,UInt_t devsn, uint16_t thread_id);
    void    Smooth(const UInt_t sn, float *fWave, int iNPoints,float fStart,float fEnd);
    void    GetAverageWave(std::vector<DATA> *vData, std::vector<DATA> *vAveData, bool bThreadSum=false);

    INTEGRAL * GetIntegral(UInt_t devsn, UInt_t ch, std::vector<int32_t> *wave, uint16_t thread_id);
    Float_t GetIntegralADC64(UInt_t devsn, UInt_t ch, std::vector<int32_t> *uiWave, Bool_t bPedSubtr=true);
    Float_t GetIntegralTQDC(UInt_t devsn, UInt_t ch, std::vector<int32_t> *uiWave, Bool_t bPedSubtr=true);
    Float_t GetIntegral1(UInt_t devsn, UInt_t ch, std::vector<int32_t> *uiWave, GATE* gatePtr, Bool_t bPedSubtr=true);
    Float_t GetAmplitude(uint16_t uiSN, uint32_t uiDevSN, uint8_t uiCH, std::vector<int32_t> *uiWave);
    UInt_t  GetPeakPos(std::vector<int32_t> *uiWave);

    std::vector<WFPEAKS> *  GetPeaksData() { return &vPeaks; }
    Bool_t                  FindPeaks(uint32_t devsn, uint8_t ch, std::vector<int32_t> *uiWave,Float_t fThr = 0);
    Bool_t                  GetWFPars(UInt_t uiSN, Char_t polarity, Float_t baseline,\
       Float_t *fNewWave, UInt_t uiMaxPos = 0, Float_t iImpAmp = 0, Float_t iAmp = 0);
    // WFPARS *                GetWFParsMain(UInt_t uiSN, std::vector<int32_t> *uiWave, UInt_t uiMaxPos = 0) {}
    bool                    IsUnderThreshold(Double_t value,Double_t loThr,Double_t hiThr){ return ((value>=loThr)&&(value<=hiThr)); }

    // int16_t SetInvCH(int16_t invch) {return invch;};

    // Time processing
    // by convolution
    void NormalizeRoughKernel(std::vector<KERNEL> *vRKernel);
    void NormalizePrecisedKernel(std::vector<KERNEL_PRECISED> *vPKernel);
    int  GetConvMin(int devorn,int iCH,const uint16_t sn,std::vector<DATA> *vData,std::vector<KERNEL> *vRKernel);
    int  GetPrecisedConvMin(int devorn,UInt_t kind,int iCH,const uint16_t sn,\
                            std::vector<DATA> *vData,\
                            std::vector<KERNEL_PRECISED> *vPKernel,\
                            std::vector<PKGATE> *vPKGate);
    // by linear fit
    bool  GetPol1Coeffs(std::vector<stFITARR> *data, double *K, double *B);
    Double_t GetTimeFitCrossPoint(UInt_t devsn, UInt_t ch, UInt_t uiSN, std::vector<int32_t> *uiWave);
    Double_t GetTimeFitCrossPointImproved(UInt_t devsn,UInt_t ch,std::vector<int32_t> *uiWave,\
                                          Float_t *fAmplitude = NULL);

    void CopyMetaData(std::vector<DATA> *vData,std::vector<DATA> *vAveData,Int_t devorn = 0);
    void CreateRoughKernel(std::vector<DATA> *vAveData);
    void CreatePrecisedKernel(std::vector<DATA> *vAveData, const char * channels = NULL);
    bool LoadRoughKernel(const char * filename);
    bool LoadPrecisedKernel(const char * filename, std::vector<PKGATE> *vPKGate);
    bool LoadGateSettings(const char * filename,int ndev);
    bool LoadPKernelGateSettings(const char * filename);
    bool LoadRKernelGateSettings(const char * filename);
    int  LoadADCSettings(const char * filename=SIGNAL_SETTINGS_FILENAME);
    void GenerateGateConfigFile(const char * filename,const char * gate,int ndev);
    void SaveGateConfigFile();
    void SetGateConfigFileName(const char* fname) { sprintf(cGateFileName, "%s", fname); }
    void GenerateKernelGateConfigFile(const char * filename,const char * gate);
    void SaveKernelGateConfigFile();

    void PrintDataStruct(DATA *d);
    void PrintMetaData(std::vector<ADCS> *meta, Short_t index=-1, Bool_t bPrintMap=false);

    // Data logger -- root file
    void LogInit(Bool_t bIsMonintor=false);
    void LogFill(UInt_t wid, Float_t data[CHANNEL_NUMBER], UInt_t sn, \
                 UInt_t event, UChar_t mode, ULong_t time=0, UInt_t cycle=0);
    void LogWrite(const char*);

    // Data logger -- ASCII file
    void LogDataInit();
    void LogDataFill(UInt_t wid, UInt_t event, UInt_t devsn, Float_t amp[CHANNEL_NUMBER],
                     Float_t charge[CHANNEL_NUMBER], Float_t time[CHANNEL_NUMBER], std::vector<int32_t> *uiWave);
    void LogDataWrite(const char*);

    // Mapping settings
    bool IsArcLight(UInt_t devsn,UInt_t ch);
    bool IsArcLight(ADCS *devPtr,UInt_t ch);
    bool IsLCM(ADCS *devPtr,UInt_t ch);
    bool IsFrontSide(ADCS *devPtr,UInt_t ch);
    bool IsRearSide(ADCS *devPtr,UInt_t ch);
    bool IsLeftPlane(ADCS *devPtr,UInt_t ch);
    bool IsRightPlane(ADCS *devPtr,UInt_t ch);
    bool IsSum(ADCS *devPtr,UInt_t ch);                  // sum of some channels
    bool IsSingle(ADCS *devPtr,UInt_t ch);               // single channel
    UChar_t GetEShapeNumber(ADCS *devPtr,UInt_t ch);
};

#endif
