#ifndef AFI_GUI_H
#define AFI_GUI_H
#include "RQ_OBJECT.h"
#include "TGFrame.h"
#include "TGNumberEntry.h"
#include "TGComboBox.h"
#include "TGButton.h"
#include "AFISettings.h"
#include "TRootEmbeddedCanvas.h"
#define MAPS_NUMBER 3
#define THRESHOLD_VALUE 1000
#if DEVICE_TYPE==DEVICE_ID_ADC64
  #define APP_PREFIX "ADC64Viewer"
  #define MON_PREFIX "ADC64Monitor"
#elif DEVICE_TYPE==DEVICE_ID_TQDC
  #define APP_PREFIX "TQDCViewer"
  #define MON_PREFIX "TQDCMonitor"
#endif


class TGMainFrame;
class TGCompositeFrame;
class TGVerticalFrame;
class TGHorizontalFrame;
class TGTransientFrame;
class TGNumberEntry;
class TGNumberEntryField;
class TGListTree;
class TGListTreeItem;
class TGTextEdit;
class TGHProgressBar;
class TGStatusBar;
class TGTab;
class TGCanvas;
class TCanvas;
class TFolder;
class TGLayoutHints;
class TGraph;
class TGWindow;
class TH1F;
class TH2F;
class TObject;
class AFIGUI;

extern "C" {
  void   update_channels_list(TGComboBox *devList, TGComboBox *chList,\
                            std::vector< ADCS > *meta);
  int    lines_count(const char *buf, int size);
  Bool_t process_config(Bool_t mode, const char * dir, VIEWERCONFIG *config);
}

typedef struct {
  UInt_t dev;
  UChar_t ch;
  TGNumberEntryField* entry;
} MAP_ENTRY;

typedef struct {
  Bool_t  isActive;
  UInt_t  dev;
  UChar_t ch;
  UChar_t sel_item_type;    // 0 -- data, 1 -- precised kernel, 2 -- rough kernel
} LAST_STATE;

//______________________________________________________________________________
class MyWindow {
// Dummy window class
protected:
  TGTransientFrame  *fMain;   // main frame of this widget

public:
  MyWindow(const TGWindow *main, UInt_t w, UInt_t h, Bool_t singleUse=false);
  virtual ~MyWindow();
};

//______________________________________________________________________________
class OptionsWindow : public MyWindow {

friend class AFIGUI;
  RQ_OBJECT("OptionsWindow")
private:
  AFIGUI                *fParent;
  TGNumberEntryField    *fEntryPedShift;
  TGNumberEntryField    *fEntrySigOffset;
  TGNumberEntryField    *fEntryHBins[3];            // Bins layout -- first bin, last bin, num of bins
  TGCheckButton         *fCheckPedSubtraction;
  TGComboBox            *fListTrigDev;
  TGComboBox            *fListTrigCh;
  TGTextButton          *fBtnOk;
  TGTextButton          *fBtnLoadGateFile;

  std::vector< ADCS >   *fMeta;

public:
  OptionsWindow(const TGWindow *main);  // ctor
  void SetParent(AFIGUI *gui) { fParent = gui; }
  void PopUp();
  void UpdateTrigChannelsList();        // slot
  void SetGateFileDialog();             // slot

  void SignalSaveConfig();              // *SIGNAL*
  //
  Double_t GetPedestalShift()      { return fEntryPedShift->GetNumber(); }
  Double_t GetSignalOffset()       { return fEntrySigOffset->GetNumber(); }
  Bool_t   GetPedSubtractionFlag() { return fCheckPedSubtraction->IsOn(); }
  UInt_t   GetTriggerDeviceSN()    { return fListTrigDev->GetSelected(); }
  UInt_t   GetTriggerCH()          { return fListTrigCh->GetSelected(); }
};

//______________________________________________________________________________
class Editor: public MyWindow {

private:
  // TGTransientFrame *fMain;   // main frame of this widget
  TGTextEdit       *fEdit;   // text edit widget
  TGTextButton     *fOK;     // OK button
  TGLayoutHints    *fL1;     // layout of TGTextEdit
  TGLayoutHints    *fL2;     // layout of OK button

public:
  Editor(const TGWindow *main, UInt_t w, UInt_t h, const char *buffer,\
          const char *title);
  // virtual ~Editor();

  // slots
  void   CloseWindow();
  void   DoOK();
  void   DoClose();
};

//______________________________________________________________________________
class GUIMon {
  RQ_OBJECT("GUIMon")

  private:
    TGTransientFrame    *fMain;
    TGHorizontalFrame   *fGlobalFrame;
    TGHorizontalFrame   *fLblFrame, *fPltFrame;
    TGHorizontalFrame   *fContentFrame;
    TRootEmbeddedCanvas *fCanvas;
    TGTextButton        *fBtnStart, *fBtnStop, *fBtnReset, *fBtnOpenDataFile;
    TGComboBox          *fListDev, *fListCh;
    TGComboBox          *fListTrigDev, *fListTrigCh;
    TFolder             *fDataFolder;
    TRootEmbeddedCanvas *fECanvas[MAPS_NUMBER*2];
    Int_t               iWidth;
    Int_t               iHeight;
    TGNumberEntry       *eEvts, *eAutoReset;
    TGNumberEntryField  *eRange[MAPS_NUMBER*2];
    std::vector< MAP_ENTRY > fMapEntry;
    std::vector< ADCS > *fMeta;
    MAP_ENTRY           *fLastEntryPtr;   // selected channel map entry
    char                fDataFilename[512];

  public:
    GUIMon(const TGWindow *p, const TGWindow *main,
      Int_t w=1400, Int_t h=1000, UInt_t options=kVerticalFrame);
    ~GUIMon();
    const char* GetClassName() { return "GUIMon"; }
    void OnOpenFileBtnClick();
    void DoClose();
    void PopUp(bool isFirstCall=false);

    // Signals
    void CloseWindow();                       // *SIGNAL*
    void StartMonitoring();                   // *SIGNAL*
    void StopMonitoring();                    // *SIGNAL*
    void ResetMonitoring();                   // *SIGNAL*

    // Slots
    void UpdateChannelsList();
    void UpdateTrigChannelsList();
    void UpdateLastEntryPointer();
    void OnMapClick(Event_t *event);// { printf("Clicked!\n"); }
    void ResetRanges();

    UInt_t GetDeviceSN()        { return fListDev->GetSelected(); }
    UChar_t GetChannel()        { return fListCh->GetSelected(); }
    UInt_t GetTrigDeviceSN()    { return fListTrigDev->GetSelected(); }
    UChar_t GetTrigChannel()    { return fListTrigCh->GetSelected(); }
    UInt_t GetNumberOfEvents()  { return eEvts->GetNumber(); }
    UInt_t GetResetNumber()     { return eAutoReset->GetNumber(); }
    void SetMetaData(std::vector<ADCS> *meta) { fMeta=meta; }

    void DrawAverage(TGraph *g);
    void DrawCharge(TH1F* h);
    void DrawTSpectrum(TH1F* h);
    void DrawMap(UChar_t map_index, Float_t [][CHANNEL_NUMBER]);
    void DrawProfile(UChar_t type, TGraph *g);
    void BuildMaps();
    void BuildPlots();
};

//______________________________________________________________________________
class AFIGUI {
friend class OptionsWindow;
  RQ_OBJECT("AFIGUI")

  private:
    TGMainFrame         *fMain;
    TGCompositeFrame    *fGlobalFrame;
    TGVerticalFrame     *fSettingsFrame;        // settings frame for processing
    TGVerticalFrame     *fBrowserFrame;         // crazy midnight display channels
    TGListTree          *fBrowser;
    TGVerticalFrame     *fPltFrame;             // displays canvas
    // TGTransientFrame    *fOptionsFrame;         // options frame
    OptionsWindow       *fOptionsWindow;
    TRootEmbeddedCanvas *fECanvas;
    TCanvas             *fTCanvas;
    TGTab               *fSettingsTab;
    TGCompositeFrame    *fSettingsFrameAvg;     // Average waveform settings
    TGCompositeFrame    *fSettingsFrameCharge;
    TGCompositeFrame    *fSettingsFrameTime;
    TGComboBox          *fListKernelType, *fListKernelDataID, *fListKernelTrigID,\
                        *fListPKernelCh, *fListGDev, *fListGCh;
    TGComboBox          *fListKernelID;         // Kernel gate settings
    TGComboBox          *fListPKernelDev;       // Kernel device
    TGNumberEntryField  *fEntryEvtLimits[2], *fEntryCPedRange[2],\
                        *fEntryCSigRange[2], *fEntryKSigRange[2],\
                        *fEntrySingleEvt;
    TGNumberEntryField  *fEntryTrigDev;
    TGNumberEntryField  *fEntryHistDisplayRange[2];
    TGNumberEntryField  *fEntryConvDiffRange[2];    // Convolution number of samples to the left and to the right side from the maximum amplitude
    TGNumberEntryField  *fEntryThreshold;           // Threshold for peaks determination
    TGNumberEntryField  *fEntrySGausFitThr;
    TGNumberEntryField  *fEntrySGausFitStdDev;
    // TGNumberEntryField  *fEntryOptsPedShift;        // Pedestal shift
    // TGNumberEntryField  *fEntryOptsSigOffset;       // Signal offset

    TFolder             *fDataFolder;
    Int_t               iWidth;
    Int_t               iHeight;
    TGTextButton        *fBtnDrawChargeSpectrum;
    TGTextButton        *fBtnDrawTimeFitSpectrum;
    TGTextButton        *fBtnDrawTimePConvSpectrum;
    TGTextButton        *fBtnDrawTimeRConvSpectrum;
    TGTextButton        *fBtnDrawAveWave;
    TGTextButton        *fBtnDrawSingleWave;
    TGTextButton        *fBtnOpenDataFile;
    TGTextButton        *fBtnSetMaxEvts;
    TGTextButton        *fBtnOpenOptionsWindow;
    TGTextButton        *fBtnCreateKernel;
    TGTextButton        *fBtnLoadPKernels;
    TGTextButton        *fBtnGetWFPars;
    TGTextButton        *fBtnSetCGateCh, *fBtnSetCGateAll, *fBtnSetKGateCh, *fBtnSetKGateAll;
    TGTextButton        *fBtnOpenFitPanel;
    TGTextButton        *fBtnOpenDrawPanel;
    TGTextButton        *fBtnWaveSetDisplayRange, *fBtnChargeSetDisplayRange, *fBtnTimeSetDisplayRange;
    TGTextButton        *fBtnSetHistoRange;
    TGTextButton        *fBtnFitSPE;
    TGTextButton        *fBtnFitSingleGaus;
    TGTextButton        *fBtnFitDoubleGaus;
    TGTextButton        *fBtnScreenshot;

    Double_t            fWaveDisplayRange[2], fChargeDisplayRange[2],\
                        fTimeDisplayRange[2];
    Int_t               fAvgEvtLimits[2], fSpectrumEvtLimits[2];
    // TGTextButton        *fBtnMonitor;
    GUIMon              *fMonitorWindow;
    TGHProgressBar      *fProgressBar;
    TGStatusBar         *fStatusBar;
    TGLayoutHints       *fLayoutBtn;
    bool                bMonFirstCall;
    std::vector< ADCS > *fMeta;
    LAST_STATE          fState;
    VIEWERCONFIG        *fConfig;
    VIEWERCONFIG        fViewerConfig;

    // TGLayoutHints       *fLayout;
    std::vector<GATE>   *vGate;
    std::vector<PKGATE> *vKGate;

    char                fDataFilename[512];   // data
    char                cWorkDir[1024];
    void UpdateFields();

  public:
    AFIGUI(Int_t w=1000, Int_t h=700);
    ~AFIGUI();
    const char* GetClassName() { return "AFIGUI"; }
    void OnDoubleClick(TGListTreeItem* item, Int_t btn);
    void ExtractDevCh(TGListTreeItem* item, UInt_t *dev, UInt_t *ch);
    void Exit();
    void PopUp();
    UInt_t GetSingleEventNumber()   { return fEntrySingleEvt->GetNumber(); }
    UInt_t GetAvgFirstEventNumber() { return fEntryEvtLimits[0]->GetNumber(); }
    UInt_t GetAvgLastEventNumber()  { return fEntryEvtLimits[1]->GetNumber(); }
    UInt_t GetConvDiffLeft()        { return fEntryConvDiffRange[0]->GetNumber();}
    UInt_t GetConvDiffRight()       { return fEntryConvDiffRange[1]->GetNumber();}
    Double_t GetNBins()             { return fOptionsWindow->fEntryHBins[2]->GetNumber(); }
    Double_t GetFirstBin()          { return fOptionsWindow->fEntryHBins[0]->GetNumber(); }
    Double_t GetLastBin()           { return fOptionsWindow->fEntryHBins[1]->GetNumber(); }
    Double_t GetThreshold()         { return fEntryThreshold->GetNumber(); }
    Double_t GetSGausFitThr()       { return fEntrySGausFitThr->GetNumber(); }
    Double_t GetSGausFitStdDev()    { return fEntrySGausFitStdDev->GetNumber(); }

    Double_t GetPedShift()          { return fOptionsWindow->GetPedestalShift(); }
    Double_t GetSignalOffset()      { return fOptionsWindow->GetSignalOffset(); }
    Bool_t   GetPedSubtractionFlag(){ return fOptionsWindow->GetPedSubtractionFlag(); }

    UInt_t GetDisplayedDev()        { return fListGDev->GetSelected(); }
    Int_t  GetDisplayedCH()         { return fListGCh->GetSelected(); }

    Int_t  GetAvgKernelType()       { return fListKernelType->GetSelected(); }
    Int_t  GetAvgKernelSN()         { return fListPKernelDev->GetSelected(); }
    Int_t  GetAvgKernelCH()         { return fListPKernelCh->GetSelected(); }
    Int_t  GetAvgKernelTrigID()     { return fListKernelTrigID->GetSelected(); }
    Int_t  GetAvgKernelDataID()     { return fListKernelDataID->GetSelected(); }
    UInt_t GetTriggerDeviceSN()     { return fOptionsWindow->GetTriggerDeviceSN(); }
    UInt_t GetTriggerCH()           { return fOptionsWindow->GetTriggerCH(); }
    Double_t* GetWaveDisplayRange() { return fWaveDisplayRange; }
    Double_t* GetChargeDisplayRange(){ return fChargeDisplayRange; }
    Double_t* GetTimeDisplayRange() { return fTimeDisplayRange; }
    LAST_STATE GetLastState()       { return fState; }
    TCanvas*  GetCanvas()           { return fECanvas->GetCanvas(); }
    VIEWERCONFIG* GetConfig()       { return fConfig; }
    void UpdateTrigChannelsList();
    void HandleKernelTypeComboBoxAvg(int);
    void UpdateKernelDataID() {}
    void UpdateKernelTrigID() {}
    void UpdateTrigDevice() {}
    void UpdateTrigChannel() {}
    void UpdateGChannelsList();
    void LoadCGateSettings(std::vector<GATE> *gate=NULL);
    void UpdateCGateSettings();
    void LoadKGateSettings(std::vector<PKGATE> *kgate=NULL);
    void UpdateKGateSettings();

    void UpdateTabInfo(Int_t id);
    void HandleEventLimEntry(Long_t);
    void HandleScreenshot();

    //
    void DrawChargeSpectrum();        // *SIGNAL*
    void DrawTimeFitSpectrum();       // *SIGNAL*
    void DrawTimePConvSpectrum();     // *SIGNAL*
    void DrawTimeRConvSpectrum();     // *SIGNAL*
    void DrawAverageWaveform(Bool_t bKernelCreation = false);       // *SIGNAL*
    void DrawSingleWaveform();        // *SIGNAL*
    void ConnectMonitor();            // *SIGNAL*
    void DrawExample(const char *fname="../adc01_20191011_173428.data"); // *SIGNAL*
    const char* SetDataFile(const char* name);  // *SIGNAL*
    void LookingForDevices();         // *SIGNAL*
    void SignalSetMetaData();         // *SIGNAL*
    void SignalLoadPrecisedKernels(Bool_t showKernel=false); // *SIGNAL*
    void SignalLoadCGate();           // *SIGNAL*
    void SignalSaveCGateSettings(Bool_t single=false);     // *SIGNAL*
    void SignalLoadKGate();           // *SIGNAL*
    void SignalSaveKGateSettings(Bool_t single=false);     // *SIGNAL*
    void SignalResetKernelLoadState();        // *SIGNAL*
    void SignalGetWFPars();             // *SIGNAL*
    void SignalFitSPE();                // *SIGNAL*
    void SignalFitSGauss();             // *SIGNAL*
    void SignalFitDGauss();             // *SIGNAL*
    void SignalSetGateFile(const char*);// *SIGNAL*

    void SetStatusText(const char *txt, Int_t pi);	// Shows text in the status bar frame
    void EventInfo(Int_t event, Int_t px, Int_t py, TObject *selected); // Handles pointer moving in the canvas
    void SetDataFileBtn();
    void OpenMonitorDialogBtn();
    void OpenPanel(Bool_t type);
    bool SetFolder(TFolder* f);
    void SetDisplayRange();
    void SetMaxEvents();
    void SaveConfig();
    void SetMetaData(std::vector<ADCS> *meta) { fMeta=meta; fOptionsWindow->fMeta=meta; }
    void SetWorkDir(const char *s) { snprintf(cWorkDir,1024, "%s", s); }
    void SetConfig(VIEWERCONFIG* config);
    void SetPKernelIDs(Int_t kids[KERNEL_MAXNUM_IN_FILE]);
    TObject* GetDisplayedPlot(TCanvas *canvas);
    void ShowHelp(int id);

    TGHProgressBar *GetProgressBar() { return fProgressBar; }
    GUIMon *GetMonitor() { return fMonitorWindow; }
};
#endif
