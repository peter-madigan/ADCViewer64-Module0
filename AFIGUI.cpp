#include "AFIGUI.h"
#include "AFISettings.h"
#include "TApplication.h"
#include "TROOT.h"
#include "TGClient.h"
#include "TGLayout.h"
#include "TGProgressBar.h"
#include "TGStatusBar.h"
#include "TGMsgBox.h"
#include "TPaveText.h"
#include "TGWindow.h"
#include "TGTab.h"
#include "TG3DLine.h"
#include "TGListTree.h"
#include "TGTextEdit.h"
#include "TGComboBox.h"
#include "TGLabel.h"
#include "TGNumberEntry.h"
#include "TFolder.h"
#include "TTreeViewer.h"
#include "TKey.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TGraph.h"
#include "TMultiGraph.h"
#include "TCanvas.h"
#include "TBrowser.h"
#include "Dialogs.h"
#include "TStyle.h"
#include "TColor.h"
#include "TSystem.h"
#include <cstring>
#include <algorithm>

const Double_t defRanges[6]={
  0, DEFAULT_SAMPLE_NUMBER,
  0, NUMBER_OF_BINS,
  0, NUMBER_OF_TIMEDIFF_BINS*TIMEDIFF_BIN_FACTOR
};

//______________________________________________________________________________
AFIGUI::AFIGUI(Int_t w, Int_t h)
{
  iWidth  = w;
  iHeight = h;
  fDataFolder = 0;
  fMonitorWindow = 0;
  bMonFirstCall = true;
  fAvgEvtLimits[0] = fSpectrumEvtLimits[0] = 1;
  fAvgEvtLimits[1] = 1000;
  fSpectrumEvtLimits[1] = 10000;

  fViewerConfig.init();
  fConfig = &fViewerConfig;

  std::vector< TGHorizontalFrame *> vHFrames;
  TGTextButton * fBtnHelp;
  vGate  = NULL;
  vKGate = NULL;
  fState.isActive = false;
  fState.dev = 0;
  fState.ch  = 0;
  TGHorizontalFrame* cFrame;
  fMain   = new TGMainFrame(
      gClient->GetRoot(),
      iWidth,
      iHeight,
      kMainFrame
  );
  fOptionsWindow = new OptionsWindow(fMain);
  fOptionsWindow->Connect("SignalSaveConfig()", "AFIGUI", this, "SaveConfig()");
  fOptionsWindow->SetParent(this);
  // fOptionsWindow->fListTrigDev->Connect("Selected(int)", "AFIGUI", this, "UpdateTrigChannelsList()");
  fMain->SetIconPixmap(".AFIViewer.png");
  fMain->SetWindowName(APP_PREFIX);
  fGlobalFrame = new TGCompositeFrame(
      fMain,
      iWidth,
      iHeight,
      kHorizontalFrame
  );
  fSettingsFrame = new TGVerticalFrame(
    fGlobalFrame,
    200,
    iHeight,
    kVerticalFrame
  );
  fLayoutBtn = new TGLayoutHints(kLHintsExpandX, 0,0,5,5);
  TGLayoutHints *fLayoutCFrame = new TGLayoutHints(kLHintsExpandX, 0,0,0,5);

  fBtnOpenDataFile = new TGTextButton(fSettingsFrame, "&Set datafile");
  fBtnOpenDataFile->Connect("Clicked()", "AFIGUI", this, "SetDataFileBtn()");
  fBtnOpenDataFile->SetTextJustify(36);
  fBtnOpenDataFile->SetMargins(0,0,0,0);
  fBtnOpenDataFile->SetWrapLength(-1);
  fBtnOpenDataFile->Resize();
  fSettingsFrame->AddFrame(fBtnOpenDataFile, new TGLayoutHints(kLHintsExpandX, 0,0,5,10));

  fBtnOpenOptionsWindow = new TGTextButton(fSettingsFrame, "&Options");
  fBtnOpenOptionsWindow->Connect("Clicked()", "OptionsWindow", fOptionsWindow, "PopUp()");
  fBtnOpenOptionsWindow->SetTextJustify(36);
  fBtnOpenOptionsWindow->SetMargins(0,0,0,0);
  fBtnOpenOptionsWindow->SetWrapLength(-1);
  fBtnOpenOptionsWindow->Resize();
  fSettingsFrame->AddFrame(fBtnOpenOptionsWindow, new TGLayoutHints(kLHintsExpandX, 0, 0, 0, 5));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrame,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrame->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "Limits: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  int lims[2] = {1, 1000};
  for (int i=0; i<2; i++){
    fEntryEvtLimits[i] = new TGNumberEntryField(cFrame, 0, 0.6,
                              TGNumberFormat::kNESInteger,
                              TGNumberFormat::kNEANonNegative);
    fEntryEvtLimits[i]->SetNumber(lims[i]);
    fEntryEvtLimits[i]->SetAlignment(kTextCenterX);
    fEntryEvtLimits[i]->Resize(80, 20);
    fEntryEvtLimits[i]->SetState(kFALSE);
    // fEntryEvtLimits[i]->Connect("TextChanged(const char*)", "AFIGUI", this,"HandleEventLimEntry()");
  }
  fBtnSetMaxEvts = new TGTextButton(cFrame, "SetMax");
  fBtnSetMaxEvts->Connect("Clicked()", "AFIGUI", this, "SetMaxEvents()");
  fBtnSetMaxEvts->SetTextJustify(36);
  fBtnSetMaxEvts->SetMargins(0,0,0,0);
  fBtnSetMaxEvts->SetWrapLength(-1);
  fBtnSetMaxEvts->Resize();
  fBtnSetMaxEvts->SetEnabled(kFALSE);
  cFrame->AddFrame(fBtnSetMaxEvts, new TGLayoutHints(kLHintsRight, 0, 0, 0, 5));
  for (int i=1; i>=0; i--)
    cFrame->AddFrame(fEntryEvtLimits[i], new TGLayoutHints(kLHintsRight, 0, 0, 0, 0));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrame,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrame->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,0,5));
  cFrame->AddFrame(new TGLabel(cFrame, "XRange:"), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,2,0,0));
  lims[0] = -100;
  lims[1] = 900;
  for (int i=0; i<2; i++){
    fEntryHistDisplayRange[i] = new TGNumberEntryField(cFrame, 0, 0.6,
                              TGNumberFormat::kNESInteger,
                              TGNumberFormat::kNEAAnyNumber,
                              TGNumberFormat::kNELLimitMinMax, -100000, 100000);
    fEntryHistDisplayRange[i]->SetNumber(lims[i]);
    fEntryHistDisplayRange[i]->SetAlignment(kTextCenterX);
    fEntryHistDisplayRange[i]->Resize(40, 20);
    fEntryHistDisplayRange[i]->SetState(kFALSE);
    // cFrame->AddFrame(fEntryConvDiffRange[i], new TGLayoutHints(kLHintsNormal, 0,0,0,0));
  }
  fBtnSetHistoRange = new TGTextButton(cFrame, "&Set");
  fBtnSetHistoRange->Connect("Clicked()", "AFIGUI", this, "SetDisplayRange()");
  fBtnSetHistoRange->SetTextJustify(36);
  fBtnSetHistoRange->SetMargins(0,0,0,0);
  fBtnSetHistoRange->SetWrapLength(-1);
  fBtnSetHistoRange->SetEnabled(kFALSE);
  fBtnSetHistoRange->Resize();
  cFrame->AddFrame(fBtnSetHistoRange, new TGLayoutHints(kLHintsExpandX|kLHintsRight|kLHintsCenterY, 0,0,0,0));

  for (int i=1; i>=0; i--)
    cFrame->AddFrame(fEntryHistDisplayRange[i], new TGLayoutHints(kLHintsRight, 0, 0, 0, 0));

  fSettingsFrame->AddFrame(new TGHorizontal3DLine(fSettingsFrame), new TGLayoutHints(kLHintsExpandX, 0,0,0,5));

  fSettingsTab = new TGTab(fSettingsFrame, 500, 20);
  fSettingsFrameAvg = fSettingsTab->AddTab("Wave");
  fSettingsFrameAvg->SetLayoutManager(new TGVerticalLayout(fSettingsFrameAvg));

  fSettingsFrameAvg->AddFrame(new TGLabel(fSettingsFrameAvg, "Average Waveform"), new TGLayoutHints(kLHintsLeft, 0,0,0,5));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameAvg,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameAvg->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,5,5));
  fBtnDrawAveWave = new TGTextButton(cFrame, "&Draw Average Waveform");
  fBtnDrawAveWave->Connect("Clicked()", "AFIGUI", this, "DrawAverageWaveform(Bool_t)");
  fBtnDrawAveWave->SetTextJustify(36);
  fBtnDrawAveWave->SetMargins(0,0,0,0);
  fBtnDrawAveWave->SetWrapLength(-1);
  fBtnDrawAveWave->SetEnabled(kFALSE);
  fBtnDrawAveWave->Resize();
  cFrame->AddFrame(fBtnDrawAveWave, new TGLayoutHints(kLHintsExpandX, 0,0,0,0));

  int iHelpBtn = 0;
  fBtnHelp = new TGTextButton(cFrame, " ? ");
  fBtnHelp->Connect("Clicked()", "AFIGUI", this, Form("ShowHelp(int id=%d)", iHelpBtn++));
  cFrame->AddFrame(fBtnHelp, new TGLayoutHints(kLHintsNormal, 2,0,0,0));

  fSettingsFrameAvg->AddFrame(new TGHorizontal3DLine(fSettingsFrameAvg), new TGLayoutHints(kLHintsExpandX, 0,0,0,5));

  fSettingsFrameAvg->AddFrame(new TGLabel(fSettingsFrameAvg, "Single Waveform"), new TGLayoutHints(kLHintsLeft, 0,0,0,5));
  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameAvg,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameAvg->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,5,5));
  cFrame->AddFrame(new TGLabel(cFrame, "Event number: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  fEntrySingleEvt = new TGNumberEntryField(cFrame, 0, 0.6,
                            TGNumberFormat::kNESInteger,
                            TGNumberFormat::kNEANonNegative,
                            TGNumberFormat::kNELLimitMinMax, 1, 10000000);
  fEntrySingleEvt->SetNumber(1);
  fEntrySingleEvt->SetAlignment(kTextCenterX);
  fEntrySingleEvt->Resize(40, 20);
  fEntrySingleEvt->SetState(kFALSE);
  cFrame->AddFrame(fEntrySingleEvt, new TGLayoutHints(kLHintsRight, 0, 0, 0, 0));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameAvg,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameAvg->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,5,5));
  fBtnDrawSingleWave = new TGTextButton(cFrame, "&Draw Single Event");
  fBtnDrawSingleWave->Connect("Clicked()", "AFIGUI", this, "DrawSingleWaveform()");
  fBtnDrawSingleWave->SetTextJustify(36);
  fBtnDrawSingleWave->SetMargins(0,0,0,0);
  fBtnDrawSingleWave->SetWrapLength(-1);
  fBtnDrawSingleWave->Resize();
  fBtnDrawSingleWave->SetEnabled(kFALSE);
  cFrame->AddFrame(fBtnDrawSingleWave, new TGLayoutHints(kLHintsExpandX, 0,0,0,0));

  fBtnHelp = new TGTextButton(cFrame, " ? ");
  fBtnHelp->Connect("Clicked()", "AFIGUI", this, Form("ShowHelp(int id=%d)", iHelpBtn++));
  cFrame->AddFrame(fBtnHelp, new TGLayoutHints(kLHintsNormal, 2,0,0,0));

  fSettingsFrameAvg->AddFrame(new TGHorizontal3DLine(fSettingsFrameAvg), new TGLayoutHints(kLHintsExpandX, 0,0,0,5));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameAvg,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameAvg->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,0,5));
  cFrame->AddFrame(new TGLabel(cFrame, "Threshold ADC value"), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,2,0,0));
  fEntryThreshold = new TGNumberEntryField(cFrame, 0, 0.6,
                            TGNumberFormat::kNESReal,
                            TGNumberFormat::kNEANonNegative);
  fEntryThreshold->SetNumber(PEAK_FIND_THR);
  fEntryThreshold->SetAlignment(kTextCenterX);
  fEntryThreshold->Resize(50, 20);
  fEntryThreshold->SetState(kFALSE);
  cFrame->AddFrame(fEntryThreshold, new TGLayoutHints(kLHintsRight));

  fBtnGetWFPars = new TGTextButton(fSettingsFrameAvg, "&Get waveform parameters");
  fBtnGetWFPars->Connect("Clicked()", "AFIGUI", this, "SignalGetWFPars()");
  fBtnGetWFPars->SetTextJustify(36);
  fBtnGetWFPars->SetMargins(0,0,0,0);
  fBtnGetWFPars->SetWrapLength(-1);
  fBtnGetWFPars->Resize();
  fBtnGetWFPars->SetEnabled(kFALSE);
  fSettingsFrameAvg->AddFrame(fBtnGetWFPars, new TGLayoutHints(kLHintsExpandX, 0,0,0,0));

  fSettingsFrameAvg->AddFrame(new TGHorizontal3DLine(fSettingsFrameAvg), new TGLayoutHints(kLHintsExpandX, 0,0,5,5));

  fSettingsFrameAvg->AddFrame(new TGLabel(fSettingsFrameAvg, "Kernel creation settings"), new TGLayoutHints(kLHintsLeft, 0,0,0,5));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameAvg,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameAvg->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,0,5));
  cFrame->AddFrame(new TGLabel(cFrame, "Type: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  fListKernelType = new TGComboBox(cFrame);
  fListKernelType->Resize(80, 20);
  fListKernelType->AddEntry("Precised", 1);
  fListKernelType->AddEntry("Rough", 0);
  // fListKernelType->Connect("Selected(int)", "AFIGUI", this, "SignalUpdateKernelType(int)");
  fListKernelType->SetEnabled(kFALSE);

  cFrame->AddFrame(fListKernelType, new TGLayoutHints(kLHintsRight|kLHintsCenterY, 0,0,0,0));

  fSettingsFrameAvg->AddFrame(new TGLabel(fSettingsFrameAvg, "Splitter connected to:"), new TGLayoutHints(kLHintsLeft, 0,0,0,5));
  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameAvg,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameAvg->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,0,5));
  cFrame->AddFrame(new TGLabel(cFrame, "Device SN: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  fListPKernelDev = new TGComboBox(cFrame);
  fListPKernelDev->Resize(80, 20);
  fListPKernelDev->SetEnabled(kFALSE);
  cFrame->AddFrame(fListPKernelDev, new TGLayoutHints(kLHintsRight|kLHintsCenterY, 0,0,0,0));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameAvg,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameAvg->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,0,5));
  cFrame->AddFrame(new TGLabel(cFrame, "Device CH: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  fListPKernelCh = new TGComboBox(cFrame);
  fListPKernelCh->Resize(80, 20);
  for (int i=0; i<4; i++){
    fListPKernelCh->AddEntry(Form("%d", i*16), i*16);
  }
  fListPKernelCh->Select(0);
  cFrame->AddFrame(fListPKernelCh, new TGLayoutHints(kLHintsRight|kLHintsCenterY, 0,0,0,0));
  fListKernelType->Select(1); // signal to update kernel type combo box
  fListPKernelCh->SetEnabled(kFALSE);
  fListKernelType->Connect("Selected(int)", "AFIGUI", this, "HandleKernelTypeComboBoxAvg(int)");

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameAvg,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameAvg->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,5,5));
  fBtnCreateKernel = new TGTextButton(cFrame, "Create kernel");
  fBtnCreateKernel->Connect("Clicked()", "AFIGUI", this, "DrawAverageWaveform(Bool_t bKernelCreation=true)");
  fBtnCreateKernel->SetTextJustify(36);
  fBtnCreateKernel->SetMargins(0,0,0,0);
  fBtnCreateKernel->SetWrapLength(-1);
  fBtnCreateKernel->SetEnabled(kFALSE);
  fBtnCreateKernel->Resize();
  cFrame->AddFrame(fBtnCreateKernel, new TGLayoutHints(kLHintsExpandX, 0,0,0,0));

  fBtnHelp = new TGTextButton(cFrame, " ? ");
  fBtnHelp->Connect("Clicked()", "AFIGUI", this, Form("ShowHelp(int id=%d)", iHelpBtn++));
  cFrame->AddFrame(fBtnHelp, new TGLayoutHints(kLHintsNormal, 2,0,0,0));

  fSettingsFrameCharge = fSettingsTab->AddTab("Charge");
  fSettingsFrameCharge->SetLayoutManager(new TGVerticalLayout(fSettingsFrameCharge));
  fSettingsFrameCharge->AddFrame(new TGLabel(fSettingsFrameCharge, "Charge Spectrum"), new TGLayoutHints(kLHintsLeft, 0,0,0,5));
  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameCharge,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameCharge->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 5,5,0,5));
  fBtnDrawChargeSpectrum = new TGTextButton(cFrame, "Draw &Spectrum");
  fBtnDrawChargeSpectrum->Connect("Clicked()", "AFIGUI", this, "DrawChargeSpectrum()");
  fBtnDrawChargeSpectrum->SetTextJustify(36);
  fBtnDrawChargeSpectrum->SetMargins(0,0,0,0);
  fBtnDrawChargeSpectrum->SetWrapLength(-1);
  fBtnDrawChargeSpectrum->Resize();
  fBtnDrawChargeSpectrum->SetEnabled(kFALSE);
  cFrame->AddFrame(fBtnDrawChargeSpectrum, new TGLayoutHints(kLHintsExpandX, 0,0,0,0));

  fBtnHelp = new TGTextButton(cFrame, " ? ");
  fBtnHelp->Connect("Clicked()", "AFIGUI", this, Form("ShowHelp(int id=%d)", iHelpBtn++));
  cFrame->AddFrame(fBtnHelp, new TGLayoutHints(kLHintsNormal, 2,0,0,0));

  // fBtnFitSPE = new TGTextButton(fSettingsFrameCharge, "Fit single ph.e.");
  // fBtnFitSPE->Connect("Clicked()", "AFIGUI", this, "SignalFitSPE()");
  // fBtnFitSPE->SetTextJustify(36);
  // fBtnFitSPE->SetMargins(0,0,0,0);
  // fBtnFitSPE->SetWrapLength(-1);
  // fBtnFitSPE->Resize();
  // fBtnFitSPE->SetEnabled(kFALSE);
  // fSettingsFrameCharge->AddFrame(fBtnFitSPE, new TGLayoutHints(kLHintsExpandX, 5,5,0,5));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameCharge,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameCharge->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 5,5,0,5));

  cFrame->AddFrame(new TGLabel(cFrame, "Threshold:"), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  fEntrySGausFitThr = new TGNumberEntryField(cFrame, 0, 0.6,
                            TGNumberFormat::kNESReal,
                            TGNumberFormat::kNEANonNegative,
                            TGNumberFormat::kNELLimitMinMax, 0, 10000);
  fEntrySGausFitThr->SetNumber(50);
  fEntrySGausFitThr->SetAlignment(kTextCenterX);
  fEntrySGausFitThr->Resize(40, 20);
  fEntrySGausFitThr->SetState(kFALSE);
  cFrame->AddFrame(fEntrySGausFitThr, new TGLayoutHints(kLHintsLeft, 0, 0, 0, 0));
  fEntrySGausFitStdDev = new TGNumberEntryField(cFrame, 0, 0.6,
                            TGNumberFormat::kNESReal,
                            TGNumberFormat::kNEANonNegative,
                            TGNumberFormat::kNELLimitMinMax, 0, 10000);
  fEntrySGausFitStdDev->SetNumber(2);
  fEntrySGausFitStdDev->SetAlignment(kTextCenterX);
  fEntrySGausFitStdDev->Resize(40, 20);
  fEntrySGausFitStdDev->SetState(kFALSE);
  cFrame->AddFrame(fEntrySGausFitStdDev, new TGLayoutHints(kLHintsRight, 0, 0, 0, 0));
  cFrame->AddFrame(new TGLabel(cFrame, "N StdDev:"), new TGLayoutHints(kLHintsRight|kLHintsCenterY, 0,0,0,0));

  fBtnFitSingleGaus = new TGTextButton(fSettingsFrameCharge, "Fit Single Gauss");
  fBtnFitSingleGaus->Connect("Clicked()", "AFIGUI", this, "SignalFitSGauss()");
  fBtnFitSingleGaus->SetTextJustify(36);
  fBtnFitSingleGaus->SetMargins(0,0,0,0);
  fBtnFitSingleGaus->SetWrapLength(-1);
  fBtnFitSingleGaus->Resize();
  fBtnFitSingleGaus->SetEnabled(kFALSE);
  fSettingsFrameCharge->AddFrame(fBtnFitSingleGaus, new TGLayoutHints(kLHintsExpandX, 5,5,0,5));

  fBtnFitDoubleGaus = new TGTextButton(fSettingsFrameCharge, "Fit Double Gauss");
  fBtnFitDoubleGaus->Connect("Clicked()", "AFIGUI", this, "SignalFitDGauss()");
  fBtnFitDoubleGaus->SetTextJustify(36);
  fBtnFitDoubleGaus->SetMargins(0,0,0,0);
  fBtnFitDoubleGaus->SetWrapLength(-1);
  fBtnFitDoubleGaus->Resize();
  fBtnFitDoubleGaus->SetEnabled(kFALSE);
  fSettingsFrameCharge->AddFrame(fBtnFitDoubleGaus, new TGLayoutHints(kLHintsExpandX, 5,5,0,5));

  fSettingsFrameTime = fSettingsTab->AddTab("Time");
  fSettingsFrameTime->SetLayoutManager(new TGVerticalLayout(fSettingsFrameTime));
  fSettingsFrameTime->AddFrame(new TGLabel(fSettingsFrameTime, "Time processing"), new TGLayoutHints(kLHintsLeft, 0,0,0,5));
  fSettingsFrameTime->AddFrame(new TGLabel(fSettingsFrameTime, "Using linear fit:"), new TGLayoutHints(kLHintsLeft, 0,0,0,5));
  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameTime,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameTime->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,0,0));
  fBtnDrawTimeFitSpectrum = new TGTextButton(cFrame, "Draw Spectrum");
  fBtnDrawTimeFitSpectrum->Connect("Clicked()", "AFIGUI", this, "DrawTimeFitSpectrum()");
  fBtnDrawTimeFitSpectrum->SetTextJustify(36);
  fBtnDrawTimeFitSpectrum->SetMargins(0,0,0,0);
  fBtnDrawTimeFitSpectrum->SetWrapLength(-1);
  fBtnDrawTimeFitSpectrum->Resize();
  fBtnDrawTimeFitSpectrum->SetEnabled(kFALSE);
  cFrame->AddFrame(fBtnDrawTimeFitSpectrum, new TGLayoutHints(kLHintsExpandX, 0,0,0,0));

  fBtnHelp = new TGTextButton(cFrame, " ? ");
  fBtnHelp->Connect("Clicked()", "AFIGUI", this, Form("ShowHelp(int id=%d)", iHelpBtn++));
  cFrame->AddFrame(fBtnHelp, new TGLayoutHints(kLHintsNormal, 2,0,0,0));

  fSettingsFrameTime->AddFrame(new TGHorizontal3DLine(fSettingsFrameTime), new TGLayoutHints(kLHintsExpandX, 0,0,5,5));

  fSettingsFrameTime->AddFrame(new TGLabel(fSettingsFrameTime, "Using convolution:"), new TGLayoutHints(kLHintsLeft, 0,0,0,5));
  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameTime,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameTime->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,0,5));
  cFrame->AddFrame(new TGLabel(cFrame, "Conv range"), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));

  fBtnHelp = new TGTextButton(cFrame, " ? ");
  fBtnHelp->Connect("Clicked()", "AFIGUI", this, Form("ShowHelp(int id=%d)", iHelpBtn++));
  cFrame->AddFrame(fBtnHelp, new TGLayoutHints(kLHintsCenterY, 2,2,0,0));

  cFrame->AddFrame(new TGLabel(cFrame, ":"), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));

  for (int i=0; i<2; i++){
    fEntryConvDiffRange[i] = new TGNumberEntryField(cFrame, 0, 0.6,
                              TGNumberFormat::kNESInteger,
                              TGNumberFormat::kNEANonNegative,
                              TGNumberFormat::kNELLimitMinMax, 0, 2048);
    fEntryConvDiffRange[i]->SetNumber(10);
    fEntryConvDiffRange[i]->SetAlignment(kTextCenterX);
    fEntryConvDiffRange[i]->Resize(30, 20);
    fEntryConvDiffRange[i]->SetState(kFALSE);
  }
  for (int i=1; i>=0; i--)
    cFrame->AddFrame(fEntryConvDiffRange[i], new TGLayoutHints(kLHintsRight, 0, 0, 0, 0));

  fSettingsFrameTime->AddFrame(new TGLabel(fSettingsFrameTime, "Precised kernel settings"), new TGLayoutHints(kLHintsLeft, 0,0,0,5));

  // fBtnLoadPKernels = new TGTextButton(fSettingsFrameTime, "Load kernels");
  // fBtnLoadPKernels->Connect("Clicked()", "AFIGUI", this, "SignalLoadPrecisedKernels(Bool_t showKernel=true)");
  // fBtnLoadPKernels->SetTextJustify(36);
  // fBtnLoadPKernels->SetMargins(0,0,0,0);
  // fBtnLoadPKernels->SetWrapLength(-1);
  // fBtnLoadPKernels->Resize();
  // fBtnLoadPKernels->SetEnabled(kFALSE);
  // fSettingsFrameTime->AddFrame(fBtnLoadPKernels, fLayoutBtn);

  vHFrames.push_back(new TGHorizontalFrame(
    fSettingsFrameTime,
    200,
    20,
    kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameTime->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,0,5));

  cFrame->AddFrame(new TGLabel(cFrame, "Data ID: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  fListKernelDataID = new TGComboBox(cFrame);
  fListKernelDataID->Resize(80, 20);
  fListKernelDataID->SetEnabled(kFALSE);
  fListKernelDataID->Connect("Selected(int)", "AFIGUI", this, "UpdateKernelDataID()");
  // fListKernelDataID->Select(0);
  cFrame->AddFrame(fListKernelDataID, new TGLayoutHints(kLHintsRight|kLHintsCenterY, 0,0,0,0));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameTime,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameTime->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,5,5));
  cFrame->AddFrame(new TGLabel(cFrame, "Trigger ID: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  fListKernelTrigID = new TGComboBox(cFrame);
  fListKernelTrigID->Resize(80, 20);
  fListKernelTrigID->SetEnabled(kFALSE);
  // fListKernelTrigID->AddEntry("Precised", 0);
  // fListKernelTrigID->AddEntry("Rough", 1)
  fListKernelTrigID->Connect("Selected(int)", "AFIGUI", this, "UpdateKernelTrigID()");
  cFrame->AddFrame(fListKernelTrigID, new TGLayoutHints(kLHintsRight|kLHintsCenterY, 0,0,0,0));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrameTime,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameTime->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,0,0));
  fBtnDrawTimePConvSpectrum = new TGTextButton(cFrame, "Draw Spectrum");
  fBtnDrawTimePConvSpectrum->Connect("Clicked()", "AFIGUI", this, "DrawTimePConvSpectrum()");
  fBtnDrawTimePConvSpectrum->SetTextJustify(36);
  fBtnDrawTimePConvSpectrum->SetMargins(0,0,0,0);
  fBtnDrawTimePConvSpectrum->SetWrapLength(-1);
  fBtnDrawTimePConvSpectrum->Resize();
  fBtnDrawTimePConvSpectrum->SetEnabled(kFALSE);
  cFrame->AddFrame(fBtnDrawTimePConvSpectrum, new TGLayoutHints(kLHintsExpandX, 0,0,0,0));

  fBtnHelp = new TGTextButton(cFrame, " ? ");
  fBtnHelp->Connect("Clicked()", "AFIGUI", this, Form("ShowHelp(int id=%d)", iHelpBtn++));
  cFrame->AddFrame(fBtnHelp, new TGLayoutHints(kLHintsNormal, 2,0,0,0));

  fSettingsFrameTime->AddFrame(new TGHorizontal3DLine(fSettingsFrameTime), new TGLayoutHints(kLHintsExpandX, 0,0,5,5));
  fSettingsFrameTime->AddFrame(new TGLabel(fSettingsFrameTime, "Rough kernel settings"), new TGLayoutHints(kLHintsLeft, 0,0,0,5));
  vHFrames.push_back(new TGHorizontalFrame(
    fSettingsFrameTime,
    200,
    20,
    kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrameTime->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,0,0));
  fBtnDrawTimeRConvSpectrum = new TGTextButton(cFrame, "Draw Spectrum");
  fBtnDrawTimeRConvSpectrum->Connect("Clicked()", "AFIGUI", this, "DrawTimeRConvSpectrum()");
  fBtnDrawTimeRConvSpectrum->SetTextJustify(36);
  fBtnDrawTimeRConvSpectrum->SetMargins(0,0,0,0);
  fBtnDrawTimeRConvSpectrum->SetWrapLength(-1);
  fBtnDrawTimeRConvSpectrum->Resize();
  fBtnDrawTimeRConvSpectrum->SetEnabled(kFALSE);
  cFrame->AddFrame(fBtnDrawTimeRConvSpectrum, new TGLayoutHints(kLHintsExpandX, 0,0,0,0));

  fBtnHelp = new TGTextButton(cFrame, " ? ");
  fBtnHelp->Connect("Clicked()", "AFIGUI", this, Form("ShowHelp(int id=%d)", iHelpBtn++));
  cFrame->AddFrame(fBtnHelp, new TGLayoutHints(kLHintsNormal, 2,0,0,0));

  fSettingsFrame->AddFrame(fSettingsTab, new TGLayoutHints(kLHintsExpandX|kLHintsExpandY, 0,0,0,0));

  /////// Gates
  fSettingsFrame->AddFrame(new TGLabel(fSettingsFrame, "Charge gate settings:"), new TGLayoutHints(kLHintsLeft, 0,0,0,5));
  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrame,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrame->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "ADC: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  fListGDev = new TGComboBox(cFrame);
  fListGDev->Resize(70, 20);
  fListGDev->SetEnabled(kFALSE);
  fListGDev->Connect("Selected(int)", "AFIGUI", this, "UpdateGChannelsList()");
  cFrame->AddFrame(fListGDev, new TGLayoutHints(kLHintsCenterY, 0,2,0,0));

  fListGCh = new TGComboBox(cFrame);
  fListGCh->Resize(80, 20);
  fListGCh->SetEnabled(kFALSE);
  fListGCh->Connect("Selected(int)", "AFIGUI", this, "UpdateCGateSettings()");
  fListGCh->Connect("Selected(int)", "AFIGUI", this, "UpdateKGateSettings()");
  cFrame->AddFrame(fListGCh, new TGLayoutHints(kLHintsCenterY|kLHintsRight, 0,0,0,0));
  cFrame->AddFrame(new TGLabel(cFrame, "CH: "), new TGLayoutHints(kLHintsRight|kLHintsCenterY, 0,0,0,0));
  ///// Gates end

  ///// Charge gate pedestal
  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrame,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrame->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "Ped range: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  lims[0] = 0; lims[1] = 2048;
  for (int i=0; i<2; i++){
    fEntryCPedRange[i] = new TGNumberEntryField(cFrame, 0, 0.6,
                              TGNumberFormat::kNESInteger,
                              TGNumberFormat::kNEANonNegative,
                              TGNumberFormat::kNELLimitMinMax, 0, 2048);
    fEntryCPedRange[i]->SetNumber(lims[i]);
    fEntryCPedRange[i]->SetAlignment(kTextCenterX);
    fEntryCPedRange[i]->Resize(40, 20);
    fEntryCPedRange[i]->SetState(kFALSE);
  }
  for (int i=1; i>=0; i--)
    cFrame->AddFrame(fEntryCPedRange[i], new TGLayoutHints(kLHintsRight, 0,0,0,0));
  ///// Charge gate pedestal end

  ///// Charge gate signal
  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrame,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrame->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "Sig range: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));

  for (int i=0; i<2; i++){
    fEntryCSigRange[i] = new TGNumberEntryField(cFrame, 0, 0.6,
                              TGNumberFormat::kNESInteger,
                              TGNumberFormat::kNEANonNegative,
                              TGNumberFormat::kNELLimitMinMax, 0, 2048);
    fEntryCSigRange[i]->SetNumber(lims[i]);
    fEntryCSigRange[i]->SetAlignment(kTextCenterX);
    fEntryCSigRange[i]->Resize(40, 20);
    fEntryCSigRange[i]->SetState(kFALSE);
  }
  for (int i=1; i>=0; i--)
    cFrame->AddFrame(fEntryCSigRange[i], new TGLayoutHints(kLHintsRight, 0,0,0,0));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrame,
      200,
      20,
      kHorizontalFrame
    ));
  cFrame = vHFrames.back();
  fSettingsFrame->AddFrame(cFrame, fLayoutCFrame);

  fBtnSetCGateCh = new TGTextButton(cFrame, "Set &CH");
  fBtnSetCGateCh->Connect("Clicked()", "AFIGUI", this, "SignalSaveCGateSettings(Bool_t single=true)");
  fBtnSetCGateCh->SetTextJustify(36);
  fBtnSetCGateCh->SetMargins(0,0,0,0);
  fBtnSetCGateCh->SetWrapLength(-1);
  fBtnSetCGateCh->SetEnabled(kFALSE);
  fBtnSetCGateCh->Resize();
  cFrame->AddFrame(fBtnSetCGateCh, new TGLayoutHints(kLHintsExpandX, 0,5,0,5));

  fBtnSetCGateAll = new TGTextButton(cFrame, "Set &All");
  fBtnSetCGateAll->Connect("Clicked()", "AFIGUI", this, "SignalSaveCGateSettings(Bool_t)");
  fBtnSetCGateAll->SetTextJustify(36);
  fBtnSetCGateAll->SetMargins(0,0,0,0);
  fBtnSetCGateAll->SetWrapLength(-1);
  fBtnSetCGateAll->SetEnabled(kFALSE);
  fBtnSetCGateAll->Resize();
  cFrame->AddFrame(fBtnSetCGateAll, new TGLayoutHints(kLHintsExpandX, 5,0,0,5));
  ///// Charge gate signal end

  fSettingsFrame->AddFrame(new TGHorizontal3DLine(fSettingsFrame), new TGLayoutHints(kLHintsExpandX, 0,0,0,5));

  ///// Kernel gate
  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrame,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrame->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "Kernel gate settings: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrame,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrame->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "Kernel ID: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  fListKernelID = new TGComboBox(cFrame);
  fListKernelID->Resize(80, 20);
  fListKernelID->SetEnabled(kFALSE);
  fListKernelID->Connect("Selected(int)", "AFIGUI", this, "UpdateKGateSettings()");
  cFrame->AddFrame(fListKernelID, new TGLayoutHints(kLHintsCenterY|kLHintsRight, 0,0,0,0));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrame,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fSettingsFrame->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "Range: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  lims[0] = 0; lims[1] = 2048;
  for (int i=0; i<2; i++){
    fEntryKSigRange[i] = new TGNumberEntryField(cFrame, 0, 0.6,
                              TGNumberFormat::kNESInteger,
                              TGNumberFormat::kNEANonNegative,
                              TGNumberFormat::kNELLimitMinMax, 0, 2048);
    fEntryKSigRange[i]->SetNumber(lims[i]);
    fEntryKSigRange[i]->SetAlignment(kTextCenterX);
    fEntryKSigRange[i]->Resize(40, 20);
    fEntryKSigRange[i]->SetState(kFALSE);
  }
  for (int i=1; i>=0; i--)
    cFrame->AddFrame(fEntryKSigRange[i], new TGLayoutHints(kLHintsRight, 0,0,0,0));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrame,
      200,
      20,
      kHorizontalFrame
    ));
  cFrame = vHFrames.back();
  fSettingsFrame->AddFrame(cFrame, fLayoutCFrame);

  fBtnSetKGateCh = new TGTextButton(cFrame, "Set &CH");
  fBtnSetKGateCh->Connect("Clicked()", "AFIGUI", this, "SignalSaveKGateSettings(Bool_t single=true)");
  fBtnSetKGateCh->SetTextJustify(36);
  fBtnSetKGateCh->SetMargins(0,0,0,0);
  fBtnSetKGateCh->SetWrapLength(-1);
  fBtnSetKGateCh->SetEnabled(kFALSE);
  fBtnSetKGateCh->Resize();
  cFrame->AddFrame(fBtnSetKGateCh, new TGLayoutHints(kLHintsExpandX, 0,5,0,5));

  fBtnSetKGateAll = new TGTextButton(cFrame, "Set &All");
  fBtnSetKGateAll->Connect("Clicked()", "AFIGUI", this, "SignalSaveKGateSettings(Bool_t)");
  fBtnSetKGateAll->SetTextJustify(36);
  fBtnSetKGateAll->SetMargins(0,0,0,0);
  fBtnSetKGateAll->SetWrapLength(-1);
  fBtnSetKGateAll->SetEnabled(kFALSE);
  fBtnSetKGateAll->Resize();
  cFrame->AddFrame(fBtnSetKGateAll, new TGLayoutHints(kLHintsExpandX, 5,0,0,5));

  ///// Kernel gate end

  fSettingsFrame->AddFrame(new TGHorizontal3DLine(fSettingsFrame), new TGLayoutHints(kLHintsExpandX, 0,0,0,5));

  vHFrames.push_back(new TGHorizontalFrame(
      fSettingsFrame,
      200,
      20,
      kHorizontalFrame
    ));
  cFrame = vHFrames.back();
  fSettingsFrame->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 0,0,0,0));

  fBtnOpenDrawPanel = new TGTextButton(cFrame, "&Draw panel");
  fBtnOpenDrawPanel->Connect("Clicked()", "AFIGUI", this, "OpenPanel(Bool_t type=0)");
  fBtnOpenDrawPanel->SetTextJustify(36);
  fBtnOpenDrawPanel->SetMargins(0,0,0,0);
  fBtnOpenDrawPanel->SetWrapLength(-1);
  fBtnOpenDrawPanel->SetEnabled(kFALSE);
  fBtnOpenDrawPanel->Resize();
  cFrame->AddFrame(fBtnOpenDrawPanel, new TGLayoutHints(kLHintsExpandX, 0,5,0,0));

  fBtnOpenFitPanel = new TGTextButton(cFrame, "&Fit panel");
  fBtnOpenFitPanel->Connect("Clicked()", "AFIGUI", this, "OpenPanel(Bool_t type=1)");
  fBtnOpenFitPanel->SetTextJustify(36);
  fBtnOpenFitPanel->SetMargins(0,0,0,0);
  fBtnOpenFitPanel->SetWrapLength(-1);
  fBtnOpenFitPanel->SetEnabled(kFALSE);
  fBtnOpenFitPanel->Resize();
  cFrame->AddFrame(fBtnOpenFitPanel, new TGLayoutHints(kLHintsExpandX, 5,0,0,0));

  fBrowserFrame = new TGVerticalFrame(
      fGlobalFrame,
      200,
      iHeight,
      kVerticalFrame
  );
  TGCanvas* canvas = new TGCanvas(fBrowserFrame,
                  fBrowserFrame->GetWidth(), 100,
                  kSunkenFrame|kDoubleBorder);
  fBrowser = new TGListTree(
      canvas->GetViewPort(),
      200,
      10,
      kHorizontalFrame, TGFrame::GetWhitePixel()
  );
  fBrowser->Associate(gClient->GetRoot());
  fBrowser->Connect("DoubleClicked(TGListTreeItem*,Int_t)","AFIGUI",this,
                     "OnDoubleClick(TGListTreeItem*,Int_t)");
  canvas->SetContainer(fBrowser);
  fBrowserFrame->AddFrame(canvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 0, 0, 0, 0));

  // fBtnMonitor = new TGTextButton(fBrowserFrame, "&Monitoring");
  // fBtnMonitor->Connect("Clicked()", "AFIGUI", this, "OpenMonitorDialogBtn()");
  // fBtnMonitor->SetTextJustify(36);
  // fBtnMonitor->SetMargins(0,0,0,0);
  // fBtnMonitor->SetWrapLength(-1);
  // fBtnMonitor->Resize();
  // fBrowserFrame->AddFrame(fBtnMonitor, new TGLayoutHints(kLHintsNormal, 0, 0, 0, 0));

  fPltFrame = new TGVerticalFrame(
      fGlobalFrame,
      1024,
      800,
      kVerticalFrame
  );
  fECanvas = new TRootEmbeddedCanvas("ec", fPltFrame, 1024, 768, kSunkenFrame|kDoubleBorder,\
		TGFrame::GetDefaultFrameBackground()  );

  fPltFrame->AddFrame(fECanvas, new TGLayoutHints(kLHintsExpandX|kLHintsExpandY, 0, 0, 0, 0));

  TGHorizontalFrame *sBarFrame = new TGHorizontalFrame(fPltFrame, 1024, 40, kHorizontalFrame);
  Int_t SSBparts[] = {25, 25, 10, 40};
  fStatusBar = new TGStatusBar(sBarFrame, 40, 10, kHorizontalFrame);
  fStatusBar->SetParts(SSBparts, 4);
  fStatusBar->Draw3DCorner(kFALSE);
  sBarFrame->AddFrame(fStatusBar, new TGLayoutHints(kLHintsExpandX|kLHintsCenterY, 0,0,0,0));

  TGHorizontalFrame *pBarFrame = new TGHorizontalFrame(fPltFrame, 1024, 50, kHorizontalFrame);
  fProgressBar = new TGHProgressBar(pBarFrame, TGProgressBar::kStandard, 300);
  fProgressBar->SetBarColor("lightblue");
  fProgressBar->ShowPosition(kTRUE);
  pBarFrame->AddFrame(fProgressBar, new TGLayoutHints(kLHintsExpandX|kLHintsCenterY, 0,0,0,0));

  fBtnScreenshot = new TGTextButton(pBarFrame, "Screenshot");
  fBtnScreenshot->Connect("Clicked()", "AFIGUI", this, "HandleScreenshot()");
  fBtnScreenshot->SetTextJustify(36);
  fBtnScreenshot->SetMargins(0,0,0,0);
  fBtnScreenshot->SetWrapLength(-1);
  fBtnScreenshot->SetEnabled(kFALSE);
  fBtnScreenshot->Resize();
  pBarFrame->AddFrame(fBtnScreenshot, new TGLayoutHints(kLHintsRight, 0,0,0,0));
  // gSystem->ProcessEvents();

  fPltFrame->AddFrame(sBarFrame, new TGLayoutHints(kLHintsExpandX, 0, 0, 5, 0));
  fPltFrame->AddFrame(pBarFrame, new TGLayoutHints(kLHintsExpandX, 0, 0, 5, 0));

  fGlobalFrame->AddFrame(fSettingsFrame, new TGLayoutHints(kLHintsExpandY, 10, 5, 0, 0));
  fGlobalFrame->AddFrame(fBrowserFrame, new TGLayoutHints(kLHintsExpandY, 5, 5, 0, 0));
  fGlobalFrame->AddFrame(fPltFrame, new TGLayoutHints(kLHintsExpandX|kLHintsExpandY, 5, 5, 0, 0));

  fMain->AddFrame(fGlobalFrame, new TGLayoutHints(kLHintsExpandX|kLHintsExpandY, 0, 0, 5, 5));

  fMain->Connect("CloseWindow()", "AFIGUI", this, "Exit()");
  fMain->SetCleanup(kDeepCleanup);
  fBrowser->SetCleanup(kDeepCleanup);
  fMain->DontCallClose();
  UpdateFields();
  // Emit("SaveConfig()");
  // SaveConfig();
  fSettingsTab->Connect(fSettingsTab, "Selected(Int_t)", "AFIGUI", this, "UpdateTabInfo(Int_t)");

  ConnectMonitor();
}

//______________________________________________________________________________
void AFIGUI::SetMaxEvents()
{
  int evtNum = 0;
  for (auto itD=fMeta->begin(); itD!=fMeta->end(); itD++)
  {
    if (itD->ev>evtNum) evtNum = itD->ev;
  }
  fEntryEvtLimits[1]->SetNumber(evtNum);
}

//______________________________________________________________________________
void AFIGUI::SetConfig(VIEWERCONFIG* config)
{
  if (!config)
    fConfig = &fViewerConfig;
  else
    fConfig = config;
}

//______________________________________________________________________________
void AFIGUI::HandleScreenshot()
{
  char cFileName[1538];
  char cRemove[] = "#[]";
  auto canvas = fECanvas->GetCanvas();
  TObject *Object = GetDisplayedPlot(canvas);
  if (!Object)
  {
    printf("Achtung: No plot to store!\n");
    return;
  }
  std::string sTitle(Object->GetTitle());
  size_t len = sTitle.find(";");
  if (len >= 0 && len !=0xFFFFFFFFFFFFFFFF)
    sTitle.resize(len);
  std::replace(sTitle.begin(), sTitle.end(), ' ', '_');

  for (int i=0; i<3; i++)
    sTitle.erase(std::remove(sTitle.begin(), sTitle.end(), cRemove[i]), sTitle.end());

  gSystem->Exec(Form("mkdir -p %s/%s", cWorkDir, "screenshots"));
  gSystem->cd(Form("%s/%s", cWorkDir, "screenshots"));
  sprintf(cFileName, "%s/screenshots/%s_%s.png", cWorkDir, sTitle.c_str(),fMeta->at(0).file);
  canvas->Print(cFileName);
  gSystem->cd(cWorkDir);
}

//______________________________________________________________________________
void AFIGUI::UpdateTabInfo(Int_t id)
{
  switch (id) {
    case 0:
      for (int i=0; i<2; i++)
      {
        fEntryEvtLimits[i]->SetNumber(fAvgEvtLimits[i]);
        // fAvgEvtLimits[i] = fEntryEvtLimits[i]->GetNumber();
      }
    break;
    default:
      for (int i=0; i<2; i++)
      {
        fEntryEvtLimits[i]->SetNumber(fSpectrumEvtLimits[i]);
        // fSpectrumEvtLimits[i] = fEntryEvtLimits[i]->GetNumber();
      }
    break;
  };
}

//______________________________________________________________________________
void AFIGUI::HandleEventLimEntry(Long_t)
{
  // Handle Amplitude selection entry widgets
  TGNumberEntry *EventLimEntry = (TGNumberEntry *) gTQSender;
  Int_t id = EventLimEntry->WidgetId();
  // printf("event lim id %d\n", id);
  switch (id)
  {
    case 0:
    break;
    case 1:
    break;
  }
}

//______________________________________________________________________________
void AFIGUI::SaveConfig()
{
  if (!fConfig)
  {
    printf("Achtung: Bad config pointer.\n");
    return;
  }
  // printf("Saving gui config...ok\n");
  fConfig->peakthr = fEntryThreshold->GetNumber();
  fConfig->fitpars[0] = fEntrySGausFitThr->GetNumber();
  fConfig->fitpars[1] = fEntrySGausFitStdDev->GetNumber();
  fConfig->kernelid[0] = fListKernelDataID->GetSelected();
  fConfig->kernelid[1] = fListKernelTrigID->GetSelected();
  fConfig->kerneltype = fListKernelType->GetSelected();
  fConfig->pedshift = fOptionsWindow->fEntryPedShift->GetNumber();
  fConfig->pedsubtr = fOptionsWindow->fCheckPedSubtraction->GetState();
  fConfig->offset  = fOptionsWindow->fEntrySigOffset->GetNumber();
  for (int i=0; i<2; i++)
  {
    // fConfig->evtlims[i] = fAvgEvtLimits[i];
    fConfig->evtlims[i] = fAvgEvtLimits[i];
    fConfig->evtlims_spec[i] = fSpectrumEvtLimits[i];
    fConfig->convrange[i] = fEntryConvDiffRange[i]->GetNumber();
    fConfig->xrange[i] = fEntryHistDisplayRange[i]->GetNumber();
    fConfig->bins[i]   = fOptionsWindow->fEntryHBins[i]->GetNumber();
  }
  fConfig->bins[2]   = fOptionsWindow->fEntryHBins[2]->GetNumber();
  process_config(1, cWorkDir, fConfig);
  Emit("SaveConfig()");
}

//______________________________________________________________________________
void AFIGUI::UpdateFields()
{
  if (!fConfig)
  {
    printf("Achtung: Bad config pointer.\n");
    return;
  }
  fEntryThreshold->SetNumber(fConfig->peakthr);
  fEntrySGausFitThr->SetNumber(fConfig->fitpars[0]);
  fEntrySGausFitStdDev->SetNumber(fConfig->fitpars[1]);
  fListKernelDataID->Select(fConfig->kernelid[0]);
  fListKernelTrigID->Select(fConfig->kernelid[1]);
  fListKernelType->Select(fConfig->kerneltype);
  fOptionsWindow->fEntryPedShift->SetNumber(fConfig->pedshift);
  fOptionsWindow->fCheckPedSubtraction->SetState((fConfig->pedsubtr)?kButtonDown:kButtonUp);
  fOptionsWindow->fEntrySigOffset->SetNumber(fConfig->offset);
  Int_t tabId = fSettingsTab->GetCurrent();
  for (int i=0; i<2; i++) {
    fAvgEvtLimits[i] = fConfig->evtlims[i];
    fSpectrumEvtLimits[i] = fConfig->evtlims_spec[i];
    if (!tabId)
      fEntryEvtLimits[i]->SetNumber(fAvgEvtLimits[i]);
    else
      fEntryEvtLimits[i]->SetNumber(fSpectrumEvtLimits[i]);
    fEntryConvDiffRange[i]->SetNumber(fConfig->convrange[i]);
    fEntryHistDisplayRange[i]->SetNumber(fConfig->xrange[i]);
    fOptionsWindow->fEntryHBins[i]->SetNumber(fConfig->bins[i]);
  }
  fOptionsWindow->fEntryHBins[2]->SetNumber(fConfig->bins[2]);
}

//______________________________________________________________________________
void AFIGUI::PopUp()
{
  fMain->MapSubwindows();
  fMain->SetWMSizeHints(iWidth, iHeight, 3840, 2160, 0, 0);
  fMain->Resize(fMain->GetDefaultSize());
  fMain->MapWindow();
  fMain->Resize();
  UpdateFields();
}

//______________________________________________________________________________
void AFIGUI::DrawExample(const char* fname)
{
  printf("%s\n", fname);
  Emit("DrawExample(const char*)", fname);
}

//______________________________________________________________________________
TObject* AFIGUI::GetDisplayedPlot(TCanvas *canvas)
{
  TObject *Object = NULL;
  TIter next(canvas->GetListOfPrimitives());
  while ((Object = next()))
  {
    if (Object->InheritsFrom(TH1F::Class()) \
        || Object->InheritsFrom(TGraph::Class()) \
        || Object->InheritsFrom(TMultiGraph::Class()))
      return Object;
  }
  return NULL;
}

//______________________________________________________________________________
void AFIGUI::OpenPanel(Bool_t type)
{
  TH1F   *histo = NULL;
  TGraph *graph = NULL;
  TObject *plot = GetDisplayedPlot(fECanvas->GetCanvas());
  if (!plot) { printf("Achtung! No plot is displayed!\n"); return; }
  if (plot->InheritsFrom(TH1F::Class()))
  {
    histo = (TH1F*)plot;
    if (type)
      histo->FitPanel();
    else
      histo->DrawPanel();
  }
  if (plot->InheritsFrom(TGraph::Class()))
  {
    graph = (TGraph*)plot;
    if (type)
      graph->FitPanel();
    else
      graph->DrawPanel();
  }
}

//______________________________________________________________________________
void AFIGUI::SetDisplayRange()
{
  // auto name = fSettingsTab->GetCurrentTab()->GetName();
  UChar_t mode = fSettingsTab->GetCurrentTab()->GetId();  // 47 128 134
  auto canvas = fECanvas->GetCanvas();
  TObject *obj = GetDisplayedPlot(canvas);
  if (!obj) return;
  if (! obj->InheritsFrom(TH1F::Class())) return;
  // printf("mode %d\n", mode);
  switch (mode) {
    case 47:
      fWaveDisplayRange[0] = fEntryHistDisplayRange[0]->GetNumber();
      fWaveDisplayRange[1] = fEntryHistDisplayRange[1]->GetNumber();
    break;
    case 128:
      fChargeDisplayRange[0] = fEntryHistDisplayRange[0]->GetNumber();
      fChargeDisplayRange[1] = fEntryHistDisplayRange[1]->GetNumber();
    break;
    case 134:
      fTimeDisplayRange[0] = fEntryHistDisplayRange[0]->GetNumber();
      fTimeDisplayRange[1] = fEntryHistDisplayRange[1]->GetNumber();
    break;
  }
  canvas->cd();
  ((TH1F*)obj)->GetXaxis()->SetRangeUser(fEntryHistDisplayRange[0]->GetNumber(),
                fEntryHistDisplayRange[1]->GetNumber());
  canvas->Modified();
  gPad->Update();
}

//______________________________________________________________________________
void AFIGUI::DrawChargeSpectrum()
{
  Emit("DrawChargeSpectrum()");
}

//______________________________________________________________________________
void AFIGUI::DrawTimeFitSpectrum()
{
  Emit("DrawTimeFitSpectrum()");
}

//______________________________________________________________________________
void AFIGUI::DrawTimePConvSpectrum()
{
  Emit("DrawTimePConvSpectrum()");
}

//______________________________________________________________________________
void AFIGUI::DrawTimeRConvSpectrum()
{
  Emit("DrawTimeRConvSpectrum()");
}

//______________________________________________________________________________
void AFIGUI::DrawAverageWaveform(Bool_t bKernelCreation)
{
  Emit("DrawAverageWaveform(Bool_t)",bKernelCreation);
}

//______________________________________________________________________________
void AFIGUI::HandleKernelTypeComboBoxAvg(int id)
{
  switch (id) {
    case 0:
      fListPKernelDev->SetEnabled(kFALSE);
      fListPKernelCh->SetEnabled(kFALSE);
    break;
    case 1:
      fListPKernelDev->SetEnabled(kTRUE);
      fListPKernelCh->SetEnabled(kTRUE);
    break;
  }
}

//______________________________________________________________________________
void AFIGUI::LoadCGateSettings(std::vector<GATE> *gate)
{
  if (!gate) return;
  UInt_t devsn = (UInt_t)fListGDev->GetSelected();
  UChar_t ch   = (UChar_t)fListGCh->GetSelected();
  vGate = gate;
  auto gatePtr = std::find_if(vGate->begin(), vGate->end(),
                  [devsn, ch](const GATE &gate){
                    return gate.devsn==devsn && gate.ch==ch;
                  });
  if (gatePtr==vGate->end())
  {
    printf("Achtung: channel not found\n");
    printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
  }
  fEntryCPedRange[0]->SetNumber(gatePtr->ped_start);
  fEntryCPedRange[1]->SetNumber(gatePtr->ped_end);
  fEntryCSigRange[0]->SetNumber(gatePtr->sig_start);
  fEntryCSigRange[1]->SetNumber(gatePtr->sig_end);
}

//______________________________________________________________________________
void AFIGUI::UpdateCGateSettings()
{
  if (!vGate) return;
  if (vGate->empty()) return;
  UInt_t devsn = (UInt_t)fListGDev->GetSelected();
  UChar_t ch   = (UChar_t)fListGCh->GetSelected();
  auto gatePtr = std::find_if(vGate->begin(), vGate->end(),
                  [devsn, ch](const GATE &gate){
                    return gate.devsn==devsn && gate.ch==ch;
                  });
  if (gatePtr==vGate->end())
  {
    printf("Achtung: selected device or channel not found\n");
    printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
  }
  fEntryCPedRange[0]->SetNumber(gatePtr->ped_start);
  fEntryCPedRange[1]->SetNumber(gatePtr->ped_end);
  fEntryCSigRange[0]->SetNumber(gatePtr->sig_start);
  fEntryCSigRange[1]->SetNumber(gatePtr->sig_end);
}

//______________________________________________________________________________
void AFIGUI::LoadKGateSettings(std::vector<PKGATE> *kgate)
{
  if (!kgate) return;
  UInt_t kid = (UInt_t)fListKernelID->GetSelected();
  vKGate = kgate;
  fEntryKSigRange[0]->SetNumber(vKGate->at(0).start[kid]);
  fEntryKSigRange[1]->SetNumber(vKGate->at(0).end[kid]);
}

//______________________________________________________________________________
void AFIGUI::UpdateKGateSettings()
{
  if (!vKGate) return;
  if (vKGate->empty()) return;
  UInt_t kid = fListKernelID->GetSelected();
  fEntryKSigRange[0]->SetNumber(vKGate->at(0).start[kid]);
  fEntryKSigRange[1]->SetNumber(vKGate->at(0).end[kid]);
}

//______________________________________________________________________________
void AFIGUI::DrawSingleWaveform()
{
  Emit("DrawSingleWaveform()");
}

//______________________________________________________________________________
// *SIGNAL*
void AFIGUI::SignalGetWFPars()
{
  Emit("SignalGetWFPars()");
}

//______________________________________________________________________________
// *SIGNAL*
void AFIGUI::ConnectMonitor()
{
  if (!fMonitorWindow){
    fMonitorWindow = new GUIMon(gClient->GetRoot(), fMain);
  }
  Emit("ConnectMonitor()");
}

//______________________________________________________________________________
void AFIGUI::LookingForDevices()
{
  Emit("LookingForDevices()");
}

//______________________________________________________________________________
// *SIGNAL*
const char* AFIGUI::SetDataFile(const char *name)
{
  sprintf(fDataFilename, "%s", name);
  fMain->SetWindowName(Form("%s - %s", APP_PREFIX, fDataFilename));
  Emit("SetDataFile(const char*)", fDataFilename);
  return name;
}

//______________________________________________________________________________
// *SIGNAL*
void AFIGUI::SignalSetMetaData()
{
  Emit("SignalSetMetaData()");
}

//______________________________________________________________________________
// *SIGNAL*
void AFIGUI::SignalLoadPrecisedKernels(Bool_t showKernel)
{
  // fBtnLoadPKernels->SetEnabled(kFALSE);
  Emit("SignalLoadPrecisedKernels(Bool_t)", showKernel);
}

//______________________________________________________________________________
// *SIGNAL*
void AFIGUI::SignalLoadCGate()
{
  Emit("SignalLoadCGate()");
}

//______________________________________________________________________________
// *SIGNAL*
void AFIGUI::SignalSaveCGateSettings(Bool_t single)
{
  UInt_t devsn = (UInt_t)fListGDev->GetSelected();
  UChar_t ch   = (UChar_t)fListGCh->GetSelected();
  auto gatePtr = std::find_if(vGate->begin(), vGate->end(),
                  [devsn, ch](const GATE &gate){
                    return gate.devsn==devsn && gate.ch==ch;
                  });
  if (gatePtr==vGate->end()){
    printf("Achtung: channel not found\n");
    printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
  }
  Int_t pstart = fEntryCPedRange[0]->GetNumber();
  Int_t pend   = fEntryCPedRange[1]->GetNumber();
  Int_t sstart = fEntryCSigRange[0]->GetNumber();
  Int_t send = fEntryCSigRange[1]->GetNumber();
  if (single)
  {
    gatePtr->ped_start = pstart;
    gatePtr->ped_end   = pend;
    gatePtr->sig_start = sstart;
    gatePtr->sig_end   = send;
  }
  else
  {
    for (auto itG=vGate->begin(); itG!=vGate->end(); itG++)
    {
      itG->ped_start = pstart;
      itG->ped_end   = pend;
      itG->sig_start = sstart;
      itG->sig_end   = send;
    }
  }
  Emit("SignalSaveCGateSettings(Bool_t)", single);
}

//______________________________________________________________________________
// *SIGNAL*
void AFIGUI::SignalLoadKGate()
{
  Emit("SignalLoadKGate()");
}

//______________________________________________________________________________
// *SIGNAL*
void AFIGUI::SignalResetKernelLoadState()
{
  Emit("SignalResetKernelLoadState()");
}

//______________________________________________________________________________
// *SIGNAL*
void AFIGUI::SignalSaveKGateSettings(Bool_t single)
{
  UInt_t kid = fListKernelID->GetSelected();
  Int_t start = fEntryKSigRange[0]->GetNumber();
  Int_t end   = fEntryKSigRange[1]->GetNumber();
  if (single)
  {
    vKGate->at(0).start[kid] = start;
    vKGate->at(0).end[kid]   = end;
  }
  else
  {
    for (int k=0; k<KERNEL_MAXNUM_IN_FILE; k++)
    {
      vKGate->at(0).start[k] = start;
      vKGate->at(0).end[k]   = end;
    }
  }
  Emit("SignalSaveKGateSettings(Bool_t)", single);
  SignalResetKernelLoadState();
  SignalLoadPrecisedKernels(1);
}

//______________________________________________________________________________
// *SIGNAL*
void AFIGUI::SignalFitSPE()
{
  Emit("SignalFitSPE()");
}

//______________________________________________________________________________
// *SIGNAL*
void AFIGUI::SignalFitSGauss()
{
  Emit("SignalFitSGauss()");
}

//______________________________________________________________________________
// *SIGNAL*
void AFIGUI::SignalFitDGauss()
{
  Emit("SignalFitDGauss()");
}

//______________________________________________________________________________
bool AFIGUI::SetFolder(TFolder* f)
{
  if ( !f ) return false;
  fDataFolder = f;
  // fDataFolder->ls();
  TGListTreeItem *dataItem=0;
  TGListTreeItem *devItem=0;
  TGListTreeItem *chItem=0;
  TGListTreeItem *selItem=0;
  TKey *key, *key1;
  Bool_t isFoundActiveDevice = false;
  Bool_t isFoundActiveChannel = false;

  dataItem = fBrowser->GetFirstItem();
  if (dataItem){
    fBrowser->DeleteChildren(dataItem);
    fBrowser->DeleteItem(dataItem);
    fBrowser->ClearViewPort();
    dataItem=0;
  }
  if (!dataItem) dataItem=fBrowser->AddItem(dataItem, "DATA", (void*)fDataFolder);
  fBrowser->OpenItem(dataItem);
  TIter keyList(fDataFolder->GetListOfFolders());
  UInt_t dev = 0;
  UInt_t ch  = 0;
  while ((key=(TKey*)keyList())){
    if(!devItem) devItem = fBrowser->AddItem(dataItem, key->GetName(), (void*)key);
    if (fState.isActive && !isFoundActiveDevice){
      ExtractDevCh(devItem, &dev, &ch);
      if (dev == fState.dev) {
        fBrowser->OpenItem(devItem);
        isFoundActiveDevice = true;
      }
    }
    TIter keyList1(((TFolder*)key)->GetListOfFolders());
    while ((key1=(TKey*)keyList1())){
      chItem = fBrowser->AddItem(devItem, ((TH1F*)key1)->GetName(), \
      (void*)key1, \
      gClient->GetPicture("h1_t.xpm"),
      gClient->GetPicture("h1_t.xpm"));
      if (fState.dev == dev && fState.isActive && isFoundActiveDevice && !isFoundActiveChannel)
      {
        ExtractDevCh(chItem, &dev, &ch);
        if (fState.ch == ch)
        {
          selItem = chItem;
          isFoundActiveChannel = true;
        }
      }
    }
    devItem=0;
  }
  if (fState.isActive) {
    if (selItem)
    {
      OnDoubleClick(selItem, 0);
      selItem->SetActive(kTRUE);
    }
  }
  return true;
}

//______________________________________________________________________________
// Process fBtnOpenDataFile
void AFIGUI::SetDataFileBtn()
{
  // go to the last used data directory
  gSystem->cd(fConfig->last_used_data_dir);

  const char *name = OpenFileDialog(fMain);
  if ( name != NULL)
  {
    SetDataFile(name);
    sprintf(fConfig->last_used_data_dir, "%s", gSystem->DirName(name));
    SignalSetMetaData();
    fOptionsWindow->fListTrigDev->RemoveAll();
    fListGDev->RemoveAll();
    fListPKernelDev->RemoveAll();
    for (auto itD=fMeta->begin(); itD!=fMeta->end(); itD++)
    {
      fOptionsWindow->fListTrigDev->AddEntry(Form("%x", itD->devsn), itD->devsn);
      fListGDev->AddEntry(Form("%x", itD->devsn), itD->devsn);
      fListPKernelDev->AddEntry(Form("%x", itD->devsn), itD->devsn);
      if (itD==fMeta->begin()){
        fOptionsWindow->fListTrigDev->Select(itD->devsn);
        fListGDev->Select(itD->devsn);
        fListPKernelDev->Select(itD->devsn);
      }
    }
    SignalLoadCGate();
    SignalLoadPrecisedKernels(1);

    fEntryThreshold->SetState(kTRUE);
    fEntrySGausFitThr->SetState(kTRUE);
    fEntrySGausFitStdDev->SetState(kTRUE);
    for (int i=0; i<2; i++) {
      fEntryEvtLimits[i]->SetState(kTRUE);
      fEntryCPedRange[i]->SetState(kTRUE);
      fEntryCSigRange[i]->SetState(kTRUE);
      fEntryKSigRange[i]->SetState(kTRUE);
      fEntryConvDiffRange[i]->SetState(kTRUE);
      fEntryHistDisplayRange[i]->SetState(kTRUE);
    }

    fBtnScreenshot->SetEnabled(kTRUE);
    fBtnSetHistoRange->SetEnabled(kTRUE);
    fBtnDrawAveWave->SetEnabled(kTRUE);
    fBtnCreateKernel->SetEnabled(kTRUE);
    fBtnDrawSingleWave->SetEnabled(kTRUE);
    fBtnDrawChargeSpectrum->SetEnabled(kTRUE);
    fBtnDrawTimeFitSpectrum->SetEnabled(kTRUE);
    fBtnDrawTimePConvSpectrum->SetEnabled(kTRUE);
    fBtnDrawTimeRConvSpectrum->SetEnabled(kTRUE);
    fBtnSetMaxEvts->SetEnabled(kTRUE);
    fBtnGetWFPars->SetEnabled(kTRUE);
    // fBtnFitSPE->SetEnabled(kTRUE);
    fBtnFitSingleGaus->SetEnabled(kTRUE);
    fBtnFitDoubleGaus->SetEnabled(kTRUE);
    // fBtnLoadPKernels->SetEnabled(kTRUE);
    fBtnSetCGateCh->SetEnabled(kTRUE);
    fBtnSetCGateAll->SetEnabled(kTRUE);
    fBtnSetKGateCh->SetEnabled(kTRUE);
    fBtnSetKGateAll->SetEnabled(kTRUE);
    fBtnOpenFitPanel->SetEnabled(kTRUE);
    fBtnOpenDrawPanel->SetEnabled(kTRUE);
    fOptionsWindow->fListTrigDev->SetEnabled(kTRUE);
    fOptionsWindow->fListTrigCh->SetEnabled(kTRUE);
    fListPKernelDev->SetEnabled(kTRUE);
    fListPKernelCh->SetEnabled(kTRUE);
    fListKernelType->SetEnabled(kTRUE);
    fEntrySingleEvt->SetState(kTRUE);
    fListGDev->SetEnabled(kTRUE);
    fListGCh->SetEnabled(kTRUE);
  }
}

//______________________________________________________________________________
void AFIGUI::UpdateGChannelsList()
{
  update_channels_list(fListGDev, fListGCh, fMeta);
}

// To be called from AFIProcessor::load_pkernel()
//______________________________________________________________________________
void AFIGUI::SetPKernelIDs(Int_t kids[KERNEL_MAXNUM_IN_FILE])
{
  TGComboBox* lists[3] = {fListKernelDataID, fListKernelTrigID, fListKernelID};
  Bool_t selected;
  for (int i=0; i<3; i++){
    selected=false;
    lists[i]->RemoveAll();
    for (int kn=0; kn<KERNEL_MAXNUM_IN_FILE; kn++)
    {
      if (kids[kn]!=-1)
      {
        lists[i]->AddEntry(Form("%d", kids[kn]), kids[kn]);
        if (!selected)
        {
          lists[i]->Select(kids[kn]);
          selected=true;
        }
      }
    }
    lists[i]->SetEnabled(kTRUE);
  }
}

//______________________________________________________________________________
// Process
void AFIGUI::OpenMonitorDialogBtn()
{
  if (!fMonitorWindow) ConnectMonitor();
  LookingForDevices();
  fMonitorWindow->PopUp(bMonFirstCall);
  bMonFirstCall=false;
}

//______________________________________________________________________________
void AFIGUI::ExtractDevCh(TGListTreeItem* item, UInt_t *dev, UInt_t *ch)
{
  TObject *obj = (TObject*)item->GetUserData();
  if (!obj){ return; }
  const char *format[3] = {
    "%*[A-Za-z0-9]_%*[a-z0-9]_ch%d",
    "CH%d",
    "%*[A-Za-z0-9]_%*[A-Za-z0-9]_%*[a-z0-9]_ch%d"
  };
  if ((strcmp(obj->ClassName(), "TFolder")==0)) {
    // printf("%s ", item->GetText());
    sscanf(item->GetText(), "%*[A-Za-z0-9]_%x", dev);
    return;
  }
  char kernel_tag[20];
  sscanf(item->GetParent()->GetText(), "%[A-Za-z0-9]_%x", kernel_tag, dev);
  if (strcmp("Kernels", kernel_tag)==0)
  {
    if ((*dev)==0) { fState.sel_item_type = 1; }  // precised kernel item
    else { fState.sel_item_type = 2; }            // rough kernel item
  }
  else fState.sel_item_type = 0;                  // data item
  if ((*dev)==0) return;
  // printf("%s ", obj->GetName());
  if (!fState.sel_item_type)
  {
    for (int i=0; i<2; i++)
    {
      auto n = sscanf(obj->GetName(), format[i], ch);
      if (n>-1) {
        break;
      }
    }
  }
}

//______________________________________________________________________________
void AFIGUI::SetStatusText(const char *txt, Int_t pi)
{
   // Set text in status bar.
   fStatusBar->SetText(txt,pi);
}

//______________________________________________________________________________
void AFIGUI::EventInfo(Int_t event, Int_t px, Int_t py, TObject *selected)
{
   //  Writes the event status in the status bar parts
   const char *text0, *text1, *text3;
   char text4[64];
   char text2[50];
   float xy[2] = {0,0};
   static int siEvIS01 = 0;
   static int siEvIS02 = 0;
   static int siEvIS03 = 0;
   text0 = selected->GetTitle();
   SetStatusText(text0,0);
   text1 = selected->GetName();
   SetStatusText(text1,1);
   if (event == kKeyPress)
      sprintf(text2, "%c", (char) px);
   else
      sprintf(text2, "%d,%d", px, py);
   SetStatusText(text2,2);
   text3 = selected->GetObjectInfo(px,py);
   // sscanf(text3,"x=%f,y=%f",&xy[0],&xy[1]);
   // sprintf(text4, "%s | %d point",text3, int(xy[0]*float(SAMPLE_NUMBER)/dPaxisMinMax[1]));
   SetStatusText(text3,3);
}

//______________________________________________________________________________
void AFIGUI::OnDoubleClick(TGListTreeItem* item, Int_t btn)
{
  TObject *obj = (TObject*)item->GetUserData();
  if (!obj){ return; }
  if ((strcmp(obj->ClassName(), "TFolder")==0)) { return; }
  TCanvas *c = fECanvas->GetCanvas();
  fTCanvas = c; // store global pointer
  fTCanvas->Connect("ProcessedEvent(Int_t,Int_t,Int_t,TObject*)","AFIGUI",this, "EventInfo(Int_t,Int_t,Int_t,TObject*)");
  if (strcmp(obj->ClassName(), "TH1F")==0)
  {
    TH1F *hist = (TH1F*)obj;
    hist->Draw();
  }
  else if (strcmp(obj->ClassName(), "TGraph")==0){
    ((TGraph*)obj)->Draw("APL");
  }
  else if (strcmp(obj->ClassName(), "TMultiGraph")==0){
    ((TMultiGraph*)obj)->Draw("A");
  }
  gPad->SetGridx();
  gPad->SetGridy();
  c->Update();
  UInt_t dev=0;
  UInt_t ch=0;
  ExtractDevCh(item, &dev, &ch);
  fMain->SetWindowName(Form("%s - %s - %s - %s",
      APP_PREFIX,
      fDataFilename,
      item->GetParent()->GetText(),
      obj->GetName()
    ));
  fState.isActive = true;
  fState.dev = dev;
  fState.ch  = ch;
  if (fState.sel_item_type)
  {
    UInt_t kernelID = 0;
    switch (fState.sel_item_type) {
      case 1:
        if (sscanf(obj->GetName(), "%*[A-Za-z]_id%d", &kernelID)>-1) fListKernelID->Select(kernelID);
      break;
      case 2:
        ;
      break;
    }
    return;
  }
  else {
    fListGDev->Select(dev);
    fListGCh->Select(ch);
  }
}

//______________________________________________________________________________
void AFIGUI::Exit()
{
  SaveConfig();
  gApplication->Terminate();
}

//______________________________________________________________________________
AFIGUI::~AFIGUI()
{
    fMain->Cleanup();
    delete fMain;
}

//______________________________________________________________________________
void AFIGUI::ShowHelp(int id)
{
  int retval=0;
  const char *msg[] = {
    "Reads defined number of events \nand displays the averaged waveform.",        // 0       Average Waveform
    "Reads and displays single waveform \nfor defined event number.",              // id = 1  Single Waveform
    "Kernel creation guide. \n\n\
Rough kernel prerequisites: \n\
  1. Open a data file\n\
  2. Set event number upper limit (~10000 or more)\n\
  3. Select \"Rough\" kernel in the list of kernel types\n\
  4. Click the \"Create kernel\" button\n\
  Rough kernels will be created for each channel\n\
  of each device and saved into the ROOT file.\n\n\
Precised kernel prerequisites: \n\
  1. Connect splitter with delayed channels to one of four ADC sockets\n\
  2. Turn on first 10 ADC channels\n\
  3. Start data acquisition with the 1st channel as a trigger (~10000 events)\n\
  4. Open the data file\n\
  5. Set event number upper limit (~10000 or more)\n\
  6. Select \"Precised\" kernel in the list of kernel types (selected by default)\n\
  7. In the \"Kernel creation settings\" section select the device serial number\n\
      (\"Device SN\") and the number of the first channel (\"Device CH\")\n\
      (0, 16, 32 or 48) which the splitter is connected to\n\
  8. Click the \"Create kernel\" button\n\
  One precised kernel will be created and saved into the ROOT file.\n\n\
    About kernels: \n\n\
    Kernel is an averaged waveform being used as a pattern for signal arrival\n\
time estimation by means of an autocorrelation method. The core of the method\n\
is the correlation of a signal with predefined pattern (kernel). Convolution\n\
of the signal waveform with the kernel results in another waveform which has\n\
the maximum amplitude. Position of this maximum corresponds to the best\n\
estimation of signal arrival time. \n\n\
There are two kernel types being used: \n\
  1. Rough kernel: an averaged waveform with the same sample number as \n\
  original signal waveform (discretization frequency is 100MHz).\n\
  2. Precised kernel: an averaged waveform with the sample number multiplied\n\
  by factor of 10 of the original signal waveform (discretization frequency\n\
  is 1GHz).",                                                                   // id = 2 Create kernel

    "Integrates signal within defined\n\
range and fills charge spectrum for\n\
defined event number. \n\
Integration range is set in the\n\
\"Charge gate settings\" section\n\
below.\n\
NOTES:\n\
-- \"Set CH\" button sets the gate for\n\
the selected channel.\n\
-- \"Set All\" button sets the gate for\n\
all the channels.",                                                                        // id = 3 Draw Charge spectrum

    "Determines signal arrival time\n\
for each event using linear fit and\n\
fills the histogram. The difference\n\
between each channel and trigger\n\
channel is also computed.\n\
NOTES:\n\
Before processing the following\n\
must be set:\n\
-- the trigger device and channel\n\
-- the range where the signal is\n\
The charge gate is used to set this\n\
range.",                                                                                // id = 4 Draw time fit spectrum

    "These values define the number\n\
of samples to the left and to the \n\
right side from the maximum amplitude\n\
to search for the peak position.",                                                      // id = 5 Conv range

    "Determines signal arrival time\n\
for each event using autocorrelation\n\
method with precised kernel.\n\
The difference between each\n\
channel and trigger channel\n\
is also computed.\n\
NOTES:\n\
Before processing the following\n\
must be set:\n\
-- the trigger device and channel\n\
-- the range where the kernel is\n\
-- the trigger and the data kernel\n\
IDs (kernel to be applied on the\n\
trigger and on the data channels)\n\
Kernel range is set in the \"Kernel\n\
gate settings\" section.",                                                              // id = 6 Draw time pconv spectrum

    "Determines signal arrival time\n\
for each event using autocorrelation\n\
method with rough kernel. The difference\n\
between each channel and trigger channel\n\
is also computed.\n\
NOTES:\n\
Before processing the following\n\
must be set:\n\
-- the trigger device and channel\n\
-- the range where the kernel is\n\
Kernel range is set in the file \n\
\"gaterk.cfg\"."            //                                                        id = 7 Draw time rconv spectrum
  };
  int w=380;
  int h=40;
  int count = lines_count(msg[id], strlen(msg[id]));
  char cTitle[128];
  const int ciLineHeight = 13;
  h = ciLineHeight*(count+1);
  switch (id) {
    case 0:
      sprintf(cTitle, "Draw Average Waveform");
    break;
    case 1:
      sprintf(cTitle, "Draw Single Waveform");
    break;
    case 2:
      sprintf(cTitle, "Create kernel");
      w = 600;
    break;
    case 3:
      sprintf(cTitle, "Draw charge spectrum");
    break;
    case 4:
      sprintf(cTitle, "Draw time spectrum (fit)");
    break;
    case 5:
      sprintf(cTitle, "Convolution range");
    break;
    case 6:
      sprintf(cTitle, "Draw time spectrum (precised kernel)");
    break;
    case 7:
      sprintf(cTitle, "Draw time spectrum (rough kernel)");
    break;
  }
  new Editor(fMain, w, h, msg[id], cTitle);
}

////////////////////////////OptionsWindow///////////////////////////////////////
//______________________________________________________________________________
OptionsWindow::OptionsWindow(const TGWindow *main) : MyWindow(main, 500, 500, 1)
{
  // TGVerticalFrame *vFrame = new TGVerticalFrame(fMain, 200, 20, kVerticalFrame);
  // fMain->AddFrame(vFrame, new TGLayoutHints(kLHintsExpandX|kLHintsExpandY, 5, 5, 5 ,5));

  std::vector< TGHorizontalFrame *> vHFrames;
  TGHorizontalFrame *cFrame;
  TGLayoutHints *fLayoutCFrame = new TGLayoutHints(kLHintsExpandX, 5,5,0,5);
  vHFrames.push_back(new TGHorizontalFrame(
      fMain,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fMain->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "Pedestal shift: "),
                  new TGLayoutHints(kLHintsCenterY, 0,0,0,0));
  fEntryPedShift = new TGNumberEntryField(cFrame, 0, 0.6,
                            TGNumberFormat::kNESReal,
                            TGNumberFormat::kNEANonNegative,
                            TGNumberFormat::kNELLimitMinMax, -10000, 10000);
  fEntryPedShift->SetNumber(PEDESTAL_SHIFT);
  fEntryPedShift->SetAlignment(kTextCenterX);
  fEntryPedShift->Resize(40, 20);
  cFrame->AddFrame(fEntryPedShift, new TGLayoutHints(kLHintsRight, 0, 0, 0, 0));

  vHFrames.push_back(new TGHorizontalFrame(
      fMain,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fMain->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "Baseline offset: "),
                  new TGLayoutHints(kLHintsCenterY, 0,0,0,0));
  fEntrySigOffset = new TGNumberEntryField(cFrame, 0, 0.6,
                            TGNumberFormat::kNESInteger,
                            TGNumberFormat::kNEAAnyNumber,
                            TGNumberFormat::kNELLimitMinMax, -2e4, 2e4);
  fEntrySigOffset->SetNumber(0);
  fEntrySigOffset->SetAlignment(kTextCenterX);
  fEntrySigOffset->Resize(40, 20);
  cFrame->AddFrame(fEntrySigOffset, new TGLayoutHints(kLHintsRight, 0, 0, 0, 0));

  vHFrames.push_back(new TGHorizontalFrame(
      fMain,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fMain->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "Pedestal subtraction"),
                  new TGLayoutHints(kLHintsCenterY|kLHintsLeft, 0,0,0,0));
  fCheckPedSubtraction = new TGCheckButton(cFrame, "");
  fCheckPedSubtraction->SetState(kButtonDown);
  cFrame->AddFrame(fCheckPedSubtraction, new TGLayoutHints(kLHintsRight, 0, 0, 0, 0));

  vHFrames.push_back(new TGHorizontalFrame(
      fMain,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fMain->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "Trigger Dev: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  fListTrigDev = new TGComboBox(cFrame);
  fListTrigDev->Resize(80, 20);
  fListTrigDev->Connect("Selected(int)", "OptionsWindow", this, "UpdateTrigChannelsList()");
  fListTrigDev->SetEnabled(kFALSE);
  cFrame->AddFrame(fListTrigDev, new TGLayoutHints(kLHintsRight|kLHintsCenterY, 0,0,0,0));

  vHFrames.push_back(new TGHorizontalFrame(
      fMain,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fMain->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "Trigger Ch: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));
  fListTrigCh = new TGComboBox(cFrame);
  fListTrigCh->Resize(80, 20);
  fListTrigCh->SetEnabled(kFALSE);
  cFrame->AddFrame(fListTrigCh, new TGLayoutHints(kLHintsRight|kLHintsCenterY, 0,0,0,0));

  vHFrames.push_back(new TGHorizontalFrame(
      fMain,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fMain->AddFrame(cFrame, new TGLayoutHints(kLHintsExpandX, 5,5,0,5));
  const char *lbl[] = { "Start", "End", "Nbins" };
  // cFrame->AddFrame(new TGLabel(cFrame, "Start:"), new TGLayoutHints(kLHintsLeft, 0,0,0,0));
  int iBins[3] = {-1000, 3000, 2000};
  for (int i=0; i<3; i++){
    fEntryHBins[i] = new TGNumberEntryField(cFrame, 0, 0.6,
                              TGNumberFormat::kNESReal,
                              TGNumberFormat::kNEAAnyNumber,
                              TGNumberFormat::kNELLimitMinMax, -100000, 100000);
    fEntryHBins[i]->SetNumber(iBins[i]);
    fEntryHBins[i]->SetAlignment(kTextCenterX);
    fEntryHBins[i]->Resize(40, 20);
  }
  fEntryHBins[2]->SetLimits(TGNumberFormat::kNELLimitMinMax,0,100000);

  for (int i=2; i>=0; i--){
    cFrame->AddFrame(fEntryHBins[i], new TGLayoutHints(kLHintsRight, 0, (i==2)?5:2, 0, 0));
    cFrame->AddFrame(new TGLabel(cFrame, lbl[i]), new TGLayoutHints(kLHintsRight|kLHintsCenterY, 0,(i==0)?2:5,0,0));
  }

  vHFrames.push_back(new TGHorizontalFrame(
      fMain,
      200,
      20,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  fMain->AddFrame(cFrame, fLayoutCFrame);
  cFrame->AddFrame(new TGLabel(cFrame, "Gate file: "), new TGLayoutHints(kLHintsLeft|kLHintsCenterY, 0,0,0,0));

  TGTextButton *fBtnLoadGateFile = new TGTextButton(cFrame, "  &Load Gate...  ");
  fBtnLoadGateFile->Connect("Clicked()", "OptionsWindow", this, "SetGateFileDialog()");
  // this->Connect("SignalSaveConfig()", "TGTransientFrame", fMain, "UnmapWindow()");
  cFrame->AddFrame(fBtnLoadGateFile, new TGLayoutHints(kLHintsCenterX, 5, 5, 5, 5));

  TGTextButton *btn = new TGTextButton(fMain, "  &Save  ");
  btn->Connect("Clicked()", "OptionsWindow", this, "SignalSaveConfig()");
  this->Connect("SignalSaveConfig()", "TGTransientFrame", fMain, "UnmapWindow()");
  fMain->AddFrame(btn, new TGLayoutHints(kLHintsCenterX, 5, 5, 5, 5));

  // fMain->AddFrame(vFrame, new TGLayoutHints(kLHintsExpandX|kLHintsExpandY, 5, 5, 5 ,5));

  fMain->MapSubwindows();
  fMain->Resize();
}

//______________________________________________________________________________
void OptionsWindow::SetGateFileDialog()
{
  const char *name = OpenFileDialog(fMain,1);
  // printf("Gate filename set: %s\n", name);
  if ( name != NULL)
  {
    sprintf(fParent->fConfig->gatefile, "%s", name);
    // printf("Gate filename config: %s\n", fParent->fConfig.gatefile);
    process_config(1, fParent->cWorkDir, fParent->fConfig);
    fParent->SignalSetGateFile(name);
    fParent->SignalLoadCGate(); // reload gate file
  }
}

//______________________________________________________________________________
void AFIGUI::SignalSetGateFile(const char* name)
{
  Emit("SignalSetGateFile(const char*)", name);
}

//______________________________________________________________________________
void OptionsWindow::PopUp()
{
  // editor covers right half of parent window
  fMain->CenterOnParent(kTRUE, TGTransientFrame::kCenter);
  fMain->MapWindow();
  // gClient->WaitFor(fMain);
}

//______________________________________________________________________________
void OptionsWindow::SignalSaveConfig()
{
  Emit("SignalSaveConfig()");
}

//______________________________________________________________________________
void OptionsWindow::UpdateTrigChannelsList()
{
  update_channels_list(fListTrigDev, fListTrigCh, fMeta);
}

///////////////////////////GUIMon Class/////////////////////////////////////////
//______________________________________________________________________________
GUIMon::GUIMon(const TGWindow *p, const TGWindow *main,
               Int_t w, Int_t h, UInt_t options)
{
  iWidth  = w;
  iHeight = h;
  fDataFolder = 0;
  fContentFrame = 0;
  fLastEntryPtr = NULL;

  fMain   = new TGTransientFrame(p, main, w, h, options);
  fMain->SetIconPixmap(".AFIMonitor.png");
  fMain->SetWindowName(MON_PREFIX);
  fMain->Connect("CloseWindow()", "GUIMon", this, "DoClose()");
  fMain->DontCallClose(); // to avoid double deletions.

  fMain->SetCleanup(kDeepCleanup);

  fGlobalFrame = new TGHorizontalFrame(fMain, iWidth, iHeight);

  fContentFrame = new TGHorizontalFrame(
    fGlobalFrame,
    iWidth,
    400,
    kHorizontalFrame
  );

  auto defLayout = new TGLayoutHints(kLHintsExpandX|kLHintsExpandY, 0, 0, 0, 0);
  fGlobalFrame->AddFrame(fContentFrame, defLayout);
  fMain->AddFrame(fGlobalFrame, new TGLayoutHints(kLHintsExpandX|kLHintsExpandY, 0, 0, 5, 0));

  //fMain->Connect("CloseWindow()", "AFIGUI", this, "Exit()");
  fMain->SetCleanup(kDeepCleanup);
}

//______________________________________________________________________________
GUIMon::~GUIMon()
{
  fMain->DeleteWindow();  // deletes fMain
  delete this;
}

//______________________________________________________________________________
void GUIMon::BuildMaps()
{
  const int ciNMap = MAPS_NUMBER;
  const int ciDN = fMeta->size();
  TGVerticalFrame *mapFrame[ciNMap];
  TGHorizontalFrame *mapRowFrame[CHANNEL_NUMBER];
  TGNumberEntryField *fMapLabelEntry[CHANNEL_NUMBER];
  auto defLayout = new TGLayoutHints(kLHintsNormal, 0, 0, 0, 0);
  auto cxpLayout = new TGLayoutHints(kLHintsCenterX, 5, 2, 0, 0);
  auto cxLayout = new TGLayoutHints(kLHintsCenterX, 0, 0, 0, 0);
  Pixel_t defColor = TColor::RGB2Pixel(200, 200, 200);
  Pixel_t green = TColor::RGB2Pixel(200, 250, 200);
  Pixel_t darkGreen = TColor::RGB2Pixel(150, 200, 150);
  Pixel_t brightGreen = TColor::RGB2Pixel(50, 250, 50);
  Pixel_t yellow = TColor::RGB2Pixel(250, 250, 200);
  Pixel_t orange = TColor::RGB2Pixel(250, 200, 150);
  Pixel_t blue = TColor::RGB2Pixel(150, 200, 240);
  Pixel_t brightBlue = TColor::RGB2Pixel(50, 50, 250);
  Int_t iRowHeight = 15;

  TGVerticalFrame *mapFrameGlob = new TGVerticalFrame(
    fContentFrame,
    100,
    600,
    kVerticalFrame
  );
  fContentFrame->AddFrame(mapFrameGlob, defLayout);

  TGHorizontalFrame *mapsFrame = new TGHorizontalFrame(
    mapFrameGlob,
    100,
    600,
    kHorizontalFrame
  );
  mapFrameGlob->AddFrame(mapsFrame, defLayout);

  const char *mapLabels[MAPS_NUMBER]={"Apmlitude, ch", "Charge, pC", "Time, ns"};
  const char *devLabels[ciDN];
  for (int i=0; i<ciDN; i++) devLabels[i] = Form("%x", fMeta->at(i).devsn);
  for (int i=0; i<ciNMap; i++)
  {
    mapFrame[i] = new TGVerticalFrame(
      mapsFrame,
      100,
      600,
      kVerticalFrame
    );

    // Labels
    TGVerticalFrame *labelsFrame = new TGVerticalFrame(
      mapFrame[i],
      100,
      30,
      kVerticalFrame
    );
    mapFrame[i]->AddFrame(labelsFrame, defLayout);

    TGLabel *mLabel = new TGLabel(labelsFrame, mapLabels[i]);
    labelsFrame->AddFrame(mLabel, cxLayout);
    TGHorizontalFrame *devLabelFrame = new TGHorizontalFrame(
      labelsFrame,
      100,
      15,
      kHorizontalFrame
    );
    for (int d=0; d<ciDN; d++){
      TGLabel *devLabel = new TGLabel(devLabelFrame, devLabels[d]);
      devLabel->Resize(50, 15);
      if (d==0) devLabelFrame->AddFrame(devLabel,
        new TGLayoutHints(kLHintsCenterX, 26, 4, 0, 0));
      else devLabelFrame->AddFrame(devLabel, cxpLayout);
    }
    labelsFrame->AddFrame(devLabelFrame, defLayout);

    // Maps
    mapsFrame->AddFrame(mapFrame[i], defLayout);
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      mapRowFrame[ch] = new TGHorizontalFrame(
        mapFrame[i],
        100,
        15,
        kHorizontalFrame
      );
      mapFrame[i]->AddFrame(mapRowFrame[ch], new TGLayoutHints(kLHintsNormal, 0, 0, 0, 0));

      fMapLabelEntry[ch] = new TGNumberEntryField(mapRowFrame[ch], 0, 0.6,
                            TGNumberFormat::kNESInteger,
                            TGNumberFormat::kNEANonNegative);
      fMapLabelEntry[ch]->SetNumber(ch);
      fMapLabelEntry[ch]->SetState(kFALSE);
      fMapLabelEntry[ch]->SetAlignment(kTextRight);
      fMapLabelEntry[ch]->Resize(20, iRowHeight);
      // fMapLabelEntry[ch]->Connect("DoubleClicked()", "GUIMon", this, "OnMapClick()");
      mapRowFrame[ch]->AddFrame(fMapLabelEntry[ch], new TGLayoutHints(kLHintsNormal, 0, 0, 0, 0));

      for (auto itD=fMeta->begin(); itD!=fMeta->end(); itD++)
      {
        auto cfgPtr = std::find_if(itD->cfg.begin(), itD->cfg.end(),
                        [ch](const ADCSETTINGS &cfg){
                          return cfg.ch==ch;
                        });
        MAP_ENTRY e;
        e.dev = itD->devsn;
        e.ch  = ch;
        e.entry = new TGNumberEntryField(mapRowFrame[ch], itD->devorn*ch, 0.6,
                              TGNumberFormat::kNESRealOne,
                              TGNumberFormat::kNEAAnyNumber);
        e.entry->SetNumber(0);
        e.entry->SetEnabled(kFALSE);
        Pixel_t color;
        switch (cfgPtr->type) {
          case 'l':
            switch (cfgPtr->side) {
              case 'f':
                color = green;
              break;
              case 'r':
                color = darkGreen;
              break;
            };
          break;
          case 'a':
            switch (cfgPtr->side) {
              case 'f':
                color = yellow;
              break;
              case 'r':
                color = orange;
              break;
            };
          break;
        }
        e.entry->SetBackgroundColor(color);
        if (cfgPtr->sum=='s') e.entry->SetBackgroundColor(blue);
        if ( itD->ach.test(ch) ) {
          e.entry->Connect("ProcessedEvent(Event_t*)", "GUIMon", this, "OnMapClick(Event_t*)");
        }
        if (cfgPtr->plane=='r') e.entry->SetTextColor(brightBlue);
        if (!itD->ach.test(ch)) { e.entry->SetTextColor(defColor); }
        e.entry->SetAlignment(kTextCenterX);
        e.entry->Resize(50, iRowHeight);
        fMapEntry.push_back(e);
        mapRowFrame[ch]->AddFrame(fMapEntry.back().entry, new TGLayoutHints(kLHintsNormal, 0, 0, 0, 0));
      } // end for devices
    } // end for channels
  } // end for maps
}

//______________________________________________________________________________
void GUIMon::UpdateChannelsList()
{
  update_channels_list(fListDev, fListCh, fMeta);
}

//______________________________________________________________________________
void GUIMon::UpdateTrigChannelsList()
{
  update_channels_list(fListTrigDev, fListTrigCh, fMeta);
}

//______________________________________________________________________________
void GUIMon::ResetRanges(){
  for (int i=0; i<MAPS_NUMBER; i++){
    for (int j=0; j<2; j++){
      auto idx = i*2+j;
      eRange[idx]->SetNumber(defRanges[idx]);
    }
  }
}

//______________________________________________________________________________
void GUIMon::BuildPlots(){
  std::vector< TGHorizontalFrame *> vHFrames;
  TGHorizontalFrame *cFrame;
  TGLayoutHints *fLayoutCFrame = new TGLayoutHints(kLHintsExpandX, 0,0,0,5);

  TGVerticalFrame *plotFrame1 = new TGVerticalFrame(
    fContentFrame,
    int(iWidth*0.7),
    950,
    kVerticalFrame
  );

  vHFrames.push_back(new TGHorizontalFrame(
      plotFrame1,
      iWidth,
      50,
      kHorizontalFrame
  ));
  cFrame = vHFrames.back();
  plotFrame1->AddFrame(cFrame, fLayoutCFrame);

  TGLabel *lDev = new TGLabel(cFrame, "Dev:");
  fListDev = new TGComboBox(cFrame);
  fListDev->Resize(80, 20);
  fListDev->Connect("Selected(int)", "GUIMon", this, "UpdateChannelsList()");
  TGLabel *lCh = new TGLabel(cFrame, "CH:");
  fListCh = new TGComboBox(cFrame);
  fListCh->Resize(50, 20);
  TGLabel *lTrigDev = new TGLabel(cFrame, "  Trigger Dev:");
  fListTrigDev = new TGComboBox(cFrame);
  fListTrigDev->Resize(80, 20);
  fListTrigDev->Connect("Selected(int)", "GUIMon", this, "UpdateTrigChannelsList()");
  TGLabel *lTrigCh = new TGLabel(cFrame, "CH:");
  fListTrigCh = new TGComboBox(cFrame);
  fListTrigCh->Resize(50, 20);
  for (int d=0; d<fMeta->size(); d++) {
    fListDev->AddEntry(Form("%x", fMeta->at(d).devsn), fMeta->at(d).devsn);
    fListTrigDev->AddEntry(Form("%x", fMeta->at(d).devsn), fMeta->at(d).devsn);
  }
  fListDev->Select(fMeta->at(0).devsn);
  fListTrigDev->Select(fMeta->at(0).devsn);

  TGLabel *lEvts = new TGLabel(cFrame, "Number of events:");
  eEvts = new TGNumberEntry(cFrame, 0, 9, -1,
    TGNumberFormat::kNESInteger,
    TGNumberFormat::kNEANonNegative);
  eEvts->SetNumber(MONITOR_NEVENTS);

  fBtnStart = new TGTextButton(cFrame, "&Start");
  fBtnStart->Connect("Clicked()", "GUIMon", this, "StartMonitoring()");
  fBtnStart->SetTextJustify(36);
  fBtnStart->SetMargins(0,0,0,0);
  fBtnStart->SetWrapLength(-1);
  fBtnStart->Resize();

  fBtnStop = new TGTextButton(cFrame, "S&top");
  fBtnStop->Connect("Clicked()", "GUIMon", this, "StopMonitoring()");
  fBtnStop->SetTextJustify(36);
  fBtnStop->SetMargins(0,0,0,0);
  fBtnStop->SetWrapLength(-1);
  fBtnStop->Resize();

  TGLabel *lAutoReset = new TGLabel(cFrame, "Reset after");
  eAutoReset = new TGNumberEntry(cFrame, 0, 9, -1,
    TGNumberFormat::kNESInteger,
    TGNumberFormat::kNEANonNegative);
  eAutoReset->SetNumber(MONITOR_AUTORESET);
  TGLabel *lAutoReset1 = new TGLabel(cFrame, "cycles");

  fBtnReset = new TGTextButton(cFrame, "&Reset");
  fBtnReset->Connect("Clicked()", "GUIMon", this, "ResetMonitoring()");
  fBtnReset->SetTextJustify(36);
  fBtnReset->SetMargins(0,0,0,0);
  fBtnReset->SetWrapLength(-1);
  fBtnReset->Resize();

  cFrame->AddFrame(lDev, new TGLayoutHints(kLHintsNormal, 1, 1, 1, 1));
  cFrame->AddFrame(fListDev, new TGLayoutHints(kLHintsNormal, 1, 2, 1, 1));
  cFrame->AddFrame(lCh, new TGLayoutHints(kLHintsNormal, 1, 1, 1, 1));
  cFrame->AddFrame(fListCh, new TGLayoutHints(kLHintsNormal, 1, 2, 1, 1));
  cFrame->AddFrame(lTrigDev, new TGLayoutHints(kLHintsNormal, 1, 1, 1, 1));
  cFrame->AddFrame(fListTrigDev, new TGLayoutHints(kLHintsNormal, 1, 2, 1, 1));
  cFrame->AddFrame(lTrigCh, new TGLayoutHints(kLHintsNormal, 1, 1, 1, 1));
  cFrame->AddFrame(fListTrigCh, new TGLayoutHints(kLHintsNormal, 1, 2, 1, 1));
  cFrame->AddFrame(lEvts, new TGLayoutHints(kLHintsNormal, 1, 1, 1, 1));
  cFrame->AddFrame(eEvts, new TGLayoutHints(kLHintsNormal, 1, 2, 1, 1));
  cFrame->AddFrame(fBtnStart, new TGLayoutHints(kLHintsNormal, 2, 1, 1, 1));
  cFrame->AddFrame(fBtnStop, new TGLayoutHints(kLHintsNormal, 1, 2, 1, 1));
  cFrame->AddFrame(lAutoReset, new TGLayoutHints(kLHintsNormal, 8, 1, 1, 1));
  cFrame->AddFrame(eAutoReset, new TGLayoutHints(kLHintsNormal, 1, 1, 1, 1));
  cFrame->AddFrame(lAutoReset1, new TGLayoutHints(kLHintsNormal, 1, 1, 1, 1));
  cFrame->AddFrame(fBtnReset, new TGLayoutHints(kLHintsNormal, 1, 2, 1, 1));

  TGVerticalFrame *ContentFrame1 = new TGVerticalFrame(
    plotFrame1,
    iWidth,
    300,
    kVerticalFrame
  );

  auto defLayout = new TGLayoutHints(kLHintsNormal, 0, 0, 0, 0);
  auto loHintDef = new TGLayoutHints(kLHintsExpandX|kLHintsExpandY, 5, 5, 0, 5);
  auto centeredLayout = new TGLayoutHints(kLHintsCenterX, 0, 0, 0, 0);

  for (int i=0; i<MAPS_NUMBER; i++){
    vHFrames.push_back(new TGHorizontalFrame(
        ContentFrame1,
        iWidth,
        50,
        kHorizontalFrame
    ));
    cFrame = vHFrames.back();
    ContentFrame1->AddFrame(cFrame, loHintDef);
    fECanvas[i] = new TRootEmbeddedCanvas(Form("MonCanvas%d",i), cFrame, 700, 250);
    cFrame->AddFrame(fECanvas[i], new TGLayoutHints(kLHintsExpandX|kLHintsExpandY, 0,10,0,0));
    fECanvas[i+MAPS_NUMBER] = new TRootEmbeddedCanvas(Form("MonCanvas%d",i+MAPS_NUMBER), cFrame, 200, 250);
    cFrame->AddFrame(fECanvas[i+MAPS_NUMBER], new TGLayoutHints(kLHintsExpandX|kLHintsExpandY, 0,0,0,0));

    vHFrames.push_back(new TGHorizontalFrame(
      ContentFrame1,
      600,
      50,
      kHorizontalFrame
    ));
    cFrame = vHFrames.back();
    ContentFrame1->AddFrame(cFrame, new TGLayoutHints(kLHintsLeft, 5,0,0,5));

    TGLabel *lbl = new TGLabel(cFrame, "X-Axis Range: ");
    cFrame->AddFrame(lbl, new TGLayoutHints(kLHintsNormal, 0,0,0,0));

    for (int j=0; j<2; j++){
      auto idx = i*2+j;
      eRange[idx] = new TGNumberEntryField(cFrame, -1,
        defRanges[idx],
        TGNumberFormat::kNESReal,
        TGNumberFormat::kNEANonNegative);
      eRange[idx]->SetNumber(defRanges[idx]);
      eRange[idx]->Resize(50, 20);
      cFrame->AddFrame(eRange[idx], defLayout);
    }
  }
  fListCh->Connect("Selected(int)", "GUIMon", this, "ResetRanges()");
  fListCh->Connect("Selected(int)", "GUIMon", this, "UpdateLastEntryPointer()");

  plotFrame1->AddFrame(ContentFrame1, loHintDef);
  fContentFrame->AddFrame(plotFrame1, loHintDef);
}

//______________________________________________________________________________
void GUIMon::PopUp(bool isFirstCall)
{
  if (isFirstCall){
    BuildMaps();
    BuildPlots();
  }
  fMain->SetWMSizeHints(iWidth, iHeight, 1000, 1000, 0, 0);
  fMain->Resize(fMain->GetDefaultSize());
  sprintf(fDataFilename, "%s", fMeta->at(0).file);
  fMain->SetWindowName(Form("%s - %s - %x - CH%02d",
          MON_PREFIX,
          fDataFilename,
          fListDev->GetSelected(),
          fListCh->GetSelected()));
  fMain->MapSubwindows();

  // position relative to the parent's window
  fMain->CenterOnParent();

  fMain->MapWindow();
  //gClient->WaitFor(fMain);    // otherwise canvas contextmenu does not work
}

//______________________________________________________________________________
void GUIMon::UpdateLastEntryPointer()
{
  // auto dev = fListDev->GetSelected();
  // auto ch  = fListCh->GetSelected();
  // fLastEntryPtr = std::find_if(fMapEntry.begin(), fMapEntry.end(),
  //       [dev, ch](const MAP_ENTRY &entry){return entry.dev==dev && entry.ch==ch;
  //       });
}

//______________________________________________________________________________
void GUIMon::DrawAverage(TGraph *g)
{
  TCanvas* c = fECanvas[0]->GetCanvas();
  c->cd();
  g->Draw("APL");
  Double_t xlo = eRange[0]->GetNumber();
  Double_t xup = eRange[1]->GetNumber();
  g->GetXaxis()->SetRangeUser(xlo, xup);
  // g->GetYaxis()->SetMaxDigits(3);
  c->Update();
}

//______________________________________________________________________________
void GUIMon::DrawCharge(TH1F *h)
{
  TCanvas* c = fECanvas[1]->GetCanvas();
  c->cd();
  h->Draw();
  Double_t xlo = eRange[2]->GetNumber();
  Double_t xup = eRange[3]->GetNumber();
  h->GetXaxis()->SetRangeUser(xlo, xup);
  // h->GetYaxis()->SetMaxDigits(3);
  c->Update();
}

//______________________________________________________________________________
void GUIMon::DrawTSpectrum(TH1F *h)
{
  TCanvas* c = fECanvas[2]->GetCanvas();
  c->cd();
  h->Draw();
  Double_t xlo = eRange[4]->GetNumber();
  Double_t xup = eRange[5]->GetNumber();
  h->GetXaxis()->SetRangeUser(xlo, xup);
  // h->GetYaxis()->SetMaxDigits(3);
  c->Update();
}

//______________________________________________________________________________
void GUIMon::DrawMap(UChar_t map_index, Float_t data[][CHANNEL_NUMBER])
{
  const int ciDN = fMeta->size();
  auto entriesPerMap = CHANNEL_NUMBER*ciDN;
  for (auto itD=fMeta->begin(); itD!=fMeta->end(); itD++){
    for (int ch=0; ch<CHANNEL_NUMBER; ch++){
      bool isActive=itD->ach.test(ch);
      auto cfgPtr = std::find_if(itD->cfg.begin(), itD->cfg.end(),
                      [ch](const ADCSETTINGS &cfg){
                        return cfg.ch==ch;
                      });
      auto eIndex = entriesPerMap*map_index+ciDN*ch+itD->devorn;
      auto e = fMapEntry.at(eIndex);
      if (isActive)
      {
        e.entry->SetNumber(data[itD->devorn][ch]);
      }
    }
  }
}

//______________________________________________________________________________
void GUIMon::DrawProfile(UChar_t type, TGraph *g)
{
  TCanvas* c = fECanvas[MAPS_NUMBER+type]->GetCanvas();
  c->cd();
  g->Draw("APL");
  // g->GetYaxis()->SetMaxDigits(3);
  c->Update();
}

//______________________________________________________________________________
void GUIMon::StartMonitoring()
{
  // printf("GUIMon::StartMonitoring() call.\n");
  Emit("StartMonitoring()");
}

//______________________________________________________________________________
void GUIMon::StopMonitoring()
{
  // printf("GUIMon::StopMonitoring() call.\n");
  Emit("StopMonitoring()");
}

//______________________________________________________________________________
void GUIMon::ResetMonitoring()
{
  // printf("GUIMon::StopMonitoring() call.\n");
  Emit("ResetMonitoring()");
}

//______________________________________________________________________________
void GUIMon::OnMapClick(Event_t *event)
{
  if (event->fType == kButtonPress && event->fCode==kButton1){
    TGNumberEntryField* e = (TGNumberEntryField*)gTQSender;
    auto entryPtr = std::find_if(fMapEntry.begin(), fMapEntry.end(),
                            [e](const MAP_ENTRY &entry){return entry.entry==e;
                            });
    if (entryPtr==fMapEntry.end()){ printf("Achtung! Field not found.\n"); return; }
    auto dev = entryPtr->dev;
    auto ch  = entryPtr->ch;
    // if (entryPtr->dev == fLastEntryPtr->dev && entryPtr->ch == fLastEntryPtr->ch) return;
    fListDev->Select(dev);
    fListCh->Select(ch);
    fMain->SetWindowName(Form("%s - %s - %x - CH%02d",
      MON_PREFIX,
      fDataFilename,
      dev,
      ch
    ));
  }
}

//______________________________________________________________________________
void GUIMon::DoClose(){
  Emit("DoClose()");
  CloseWindow();
  gApplication->Terminate();
   // Close the Ged editor if it was activated.
   // if (TVirtualPadEditor::GetPadEditor(kFALSE) != 0)
   //    TVirtualPadEditor::Terminate();
}

//______________________________________________________________________________
void GUIMon::CloseWindow(){
  // printf("Monitor is closed!\n");
  fMain->UnmapWindow();
}

//______________________________________________________________________________
void update_channels_list(TGComboBox *devList, TGComboBox *chList, std::vector< ADCS > *meta)
{
  if (!chList) return;
  chList->RemoveAll();
  if (!devList) return;
  Int_t devID = devList->GetSelected();
  for (int d=0; d<meta->size(); d++)
  {
    if (meta->at(d).devsn==devID)
    {
      auto index = -1;
      for (UChar_t ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        if (meta->at(d).ach.test(ch))
        {
          chList->AddEntry(Form("%d", ch), ch);
          if (index<0) index=ch;
        }
      }
      chList->Select(index);
      break;
    }
  }
}

//______________________________________________________________________________
int lines_count(const char *buf, int size)
{
  int count=1;
  for (int i=0; i<size; i++)
  {
    if (buf[i]==0xA) count++;
  }
  return count;
}

//______________________________________________________________________________
Bool_t process_config(Bool_t mode, const char * dir, VIEWERCONFIG *config)
{
  FILE *fd;
  const int ciLineSize = 256;
  char cLine[ciLineSize];

  const char * ccPathName = Form("%s/.viewer.cfg",dir);

  switch (int(mode))
  {
    case 0:
      fd = fopen(ccPathName,"rt");
      if (fd == NULL)
      {
        printf("Open configuration file error!\nThe defalult configuration file will be generated.\n");
        return false;
      }

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%s",config->gatefile);

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%d %d",&(config->evtlims[0]), &(config->evtlims[1]));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%d %d",&(config->evtlims_spec[0]), &(config->evtlims_spec[1]));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%x %u",&(config->trigger[0]), &(config->trigger[1]));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%d %d",&(config->xrange[0]), &(config->xrange[1]));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%d %d %d",&(config->bins[0]), &(config->bins[1]), &(config->bins[2]));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%d",&(config->peakthr));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%hu",&(config->kerneltype));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%x %u",&(config->splitter[0]), &(config->splitter[1]));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%hu %hu",&(config->fitpars[0]), &(config->fitpars[1]));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%u %u",&(config->convrange[0]), &(config->convrange[1]));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%hu %hu",&(config->kernelid[0]), &(config->kernelid[1]));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%hu",&(config->pedshift));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%d",&(config->offset));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%hu",&(config->pedsubtr));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%d",&(config->digi_phosphor_coef));

      fgets(cLine,sizeof(cLine),fd);
      while ( (!strncmp(cLine,"#",1) || !strncmp(cLine,"\n",1) ) && !feof(fd))
        fgets(cLine,sizeof(cLine),fd);
      sscanf(cLine,"%s",config->last_used_data_dir);
      // if (strncmp(config->last_used_data_dir,"#",1) == 0)
      //   printf(config->last_used_data_dir, "%s", LAST_USED_DATA_DIR);

      // printf("Loading gui settings...%s\n",ccPathName);
    break;
    case 1:
      fd = fopen(ccPathName,"wt");
      if (fd == NULL)
      {
        printf("Open configuration file error!\nThe last settings have not been saved.\n");
        return false;
      }
      fprintf(fd, "# Viewer - Main Configuration File #\n\n");

      fprintf(fd, "# Gate file location\n");
      fprintf(fd, "%s\n\n",config->gatefile);

      fprintf(fd, "# Events limits (first, last) average waveform\n");
      fprintf(fd, "%d %d\n\n",config->evtlims[0],config->evtlims[1]);

      fprintf(fd, "# Events limits (first, last) spectrum\n");
      fprintf(fd, "%d %d\n\n",config->evtlims_spec[0],config->evtlims_spec[1]);

      fprintf(fd, "# Trig (dev, ch)\n");
      fprintf(fd, "0x%x %u\n\n",config->trigger[0],config->trigger[1]);

      fprintf(fd, "# Xrange (xlo, xup)\n");
      fprintf(fd, "%d %d\n\n",config->xrange[0],config->xrange[1]);

      fprintf(fd, "# Bins (start, end, nbins)\n");
      fprintf(fd, "%d %d %d\n\n",config->bins[0],config->bins[1], config->bins[2]);

      fprintf(fd, "# Threshold ADC value (for peak finder)\n");
      fprintf(fd, "%d\n\n",config->peakthr);

      fprintf(fd, "# Kernel type (0 - rough, 1 - precised)\n");
      fprintf(fd, "%d\n\n",config->kerneltype);

      fprintf(fd, "# Splitter (dev, ch) // for kernel creation\n");
      fprintf(fd, "0x%x %u\n\n",config->splitter[0],config->splitter[1]);

      fprintf(fd, "# Charge spectrum fit -- Threshold, Number of St.Dev.\n");
      fprintf(fd, "%d %d\n\n",config->fitpars[0],config->fitpars[1]);

      fprintf(fd, "# Time spectrum -- convolution range (-/+)\n");
      fprintf(fd, "%d %d\n\n",config->convrange[0],config->convrange[1]);

      fprintf(fd, "# Precised kernel settings: data ID, trigger ID\n");
      fprintf(fd, "%d %d\n\n",config->kernelid[0],config->kernelid[1]);

      fprintf(fd, "# Pedestal shift -- a.u.\n");
      fprintf(fd, "%u\n\n",config->pedshift);

      fprintf(fd, "# Baseline offset -- in ADC units\n");
      fprintf(fd, "%d\n\n",config->offset);

      fprintf(fd, "# Pedestal subtraction (0 - disabled, 1 - enabled)\n");
      fprintf(fd, "%d\n\n",config->pedsubtr);

      fprintf(fd, "# Digital phosphor representation rate (>=1,Default:10) [CAUTION: Too hight rate can cause app crash]\n");
      fprintf(fd, "%d\n\n",config->digi_phosphor_coef);

      fprintf(fd, "# Last used data directory\n");
      fprintf(fd, "%s\n\n",config->last_used_data_dir);

      fprintf(fd, "# ################################### #\n");
    break;
  }

  if (fd != NULL) fclose(fd);
  return true;
}

///////////////////////////////Window///////////////////////////////////////////
MyWindow::MyWindow(const TGWindow *main, UInt_t w, UInt_t h, Bool_t saveInst)
{
  fMain = new TGTransientFrame(gClient->GetRoot(), main, w, h, kMainFrame);
  if (saveInst) fMain->Connect("CloseWindow()", "TGTransientFrame", fMain,
                        "UnmapWindow()");
  fMain->DontCallClose(); // to avoid double deletions.

  // use hierarchical cleaning
  fMain->SetCleanup(kDeepCleanup);
}

//______________________________________________________________________________
MyWindow::~MyWindow()
{
   // Delete editor dialog.

   fMain->DeleteWindow();  // deletes fMain
}

///////////////////////////////Editor///////////////////////////////////////////
Editor::Editor(const TGWindow *main, UInt_t w, UInt_t h, const char *buffer,\
              const char *title) : MyWindow(main, w, h)
{
   // Create an editor in a dialog.

   // fMain = new TGTransientFrame(gClient->GetRoot(), main, w, h, kMainFrame);
   fMain->Connect("CloseWindow()", "Editor", this, "CloseWindow()");
   // fMain->DontCallClose(); // to avoid double deletions.

   // use hierarchical cleaning
   // fMain->SetCleanup(kDeepCleanup);

   fEdit = new TGTextEdit(fMain, w, h, 0);
   fL1 = new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 3, 3, 2, 2);
   fMain->AddFrame(fEdit, fL1);
   fEdit->Connect("Closed()", "Editor", this, "DoClose()");

   // set selected text colors
   fEdit->SetSelectBack(TGTextEdit::GetDefaultFrameBackground());
   // fEdit->SetSelectFore(TGTextEdit::GetDefaultFrameBackground());
   fEdit->SetBackground(TGTextEdit::GetDefaultFrameBackground());
   fEdit->EnableMenu(kFALSE);
   fEdit->SetReadOnly(kTRUE);
   // FontStruct_t font = 42;
   // fEdit->SetFont(font);

   fOK = new TGTextButton(fMain, "  &OK  ");
   fOK->Connect("Clicked()", "Editor", this, "DoOK()");
   fL2 = new TGLayoutHints(kLHintsCenterX, 0, 0, 0, 2);
   fMain->AddFrame(fOK, fL2);

   fMain->SetWindowName(Form("Help: %s",title));
   fMain->MapSubwindows();

   fMain->Resize();

   // editor covers right half of parent window
   fMain->CenterOnParent(kTRUE, TGTransientFrame::kCenter);

   fEdit->LoadBuffer(buffer);
   fMain->MapWindow();
   gClient->WaitFor(fMain);
}

//______________________________________________________________________________
// Editor::~Editor()
// {
//    // Delete editor dialog.
//
//    fMain->DeleteWindow();  // deletes fMain
// }

//______________________________________________________________________________
void Editor::CloseWindow()
{
   // Called when closed via window manager action.

   delete this;
}

//______________________________________________________________________________
void Editor::DoOK()
{
   // Handle ok button.

   CloseWindow();
}

//______________________________________________________________________________
void Editor::DoClose()
{
   // Handle close button.

   CloseWindow();
}
