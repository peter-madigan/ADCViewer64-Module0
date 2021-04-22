#include "AFIDecoder.h"
#include "AFIGUI.h"
#include "AFIProcessor.h"
#include "AFISettings.h"
#include "AFIErrors.h"
#include "RLogger.h"

#include "stdlib.h"
#include "string.h"
#include <math.h>
#include <cstdio>
#include <vector>
#include <bitset>
#include <unistd.h>
#include <sys/stat.h>
#include "stdint.h"		// declaration of uint32_t and etc types...
#include <algorithm>
// #include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

#include "TApplication.h"
#include "TROOT.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TMultiGraph.h"
#include "TVectorF.h"
#include "TVectorD.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TF1.h"
#include "TAxis.h"
#include "TGaxis.h"
#include "TStyle.h"
#include "TFolder.h"
#include "TFrame.h"
#include "TBrowser.h"
#include "TStopwatch.h"
#include "TPaveText.h"
#include "TGProgressBar.h"
#include "TTimeStamp.h"
#include "TMath.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <getopt.h>           // options processing

AFISettings settings;
AFIGUI *gui;
AFIDecoder *Decoder;
RLogger<RLOG> *Logger;
RLogger<RWAVE> *rLoggerWF;
std::vector<GATE> *vGate;
TStopwatch   *Watch;
  // Create the collection which will hold the threads, our "pool"
std::vector<std::thread> workers;
std::vector<ADCS> *vADCMeta;
TFolder *DataFolder;
TFolder *KernelFolder;
std::vector<PKGATE> *vPKGate;
std::vector<GATE>   *vRKGate;
std::vector<KERNEL> *vRKernel;
std::vector<KERNEL_PRECISED> *vPKernel;
std::vector<DATA> vDataPtr;
SPECTRA spectra;
std::vector<SPECTRA> vSpectra;
Int_t iError=0;
UShort_t usPedShift=PEDESTAL_SHIFT;
bool bPedSubtr=true;
bool bMonOnline=false;
bool bPKernelLoaded=false;
bool bRKernelLoaded=false;
bool bKernelTypeChanged=false;
UChar_t ucLastKernelType=1;
bool bMonInit=false;    // True if the monitor window is shown
bool bDevNumChanged=false;
MON_PLOT MonGraph;
std::vector<MON_PLOT> vMonGraph;    // stores monitoring graphs of time profiles
MON_PLOT MonHisto;
std::vector<MON_PLOT> vMonHisto;
MON_PLOT ViewHisto;
std::vector<MON_PLOT> vViewHisto;   // global veiwer histograms container
Char_t lastPlotType;

//______________________________________________________________________________
int start_processing()
{
  #if ROOT_VERSION_CODE <= ROOT_VERSION(6,10,8)
    TGaxis::SetMaxDigits(3);
  #endif

  FILE   * fdDataFile     = NULL;
  uint32_t uiNumOfEvents  = 0;
  uint8_t  uiNumOfDevices = 0;
  UInt_t uiTotEventsBuff  = 0;
  std::vector<DATA> vData;        // vector of data structures with different device id <data device id#1 ... data device id#N>
  vData.clear();
  DATA adata;
  std::vector<DATA> vAveData;			// average waveforms data
  vAveData.clear();
  std::vector<WFPEAKS> *vPeak = NULL;        // Waveform peak's info
  vMonGraph.clear();
  vMonHisto.clear();
  vViewHisto.clear();
  lastPlotType = -1;

  // Set stat options
  gStyle->SetStatY(0.9);    // Set y-position (fraction of pad size)
  gStyle->SetStatX(0.9);    // Set x-position (fraction of pad size)
  gStyle->SetStatW(0.20);   // Set width of stat-box (fraction of pad size)
  gStyle->SetStatH(0.20);   // Set height of stat-box (fraction of pad size)
  gStyle -> SetStatFormat("4.3f");
  gStyle -> SetFitFormat ("4.3f");
  gStyle -> SetOptStat(1001110);  // gStyle -> SetOptStat(1110);  // gStyle -> SetOptStat(1001111);
  gStyle -> SetOptFit(111);
  // True title aligning
  gStyle->SetTitleY(0.98);
  gStyle->SetTitleAlign(23);  // aligning of title
  // gStyle.SetTitleFontSize(0.09);
  //////////////////////////////////////

  Decoder = new AFIDecoder();
  Watch   = new TStopwatch();
  Logger  = new RLogger<RLOG>();
  rLoggerWF  = new RLogger<RWAVE>();

  printf("Data processing in:    %d threads\n",THREAD_NUMBER);

  if (Decoder->LoadADCSettings(Form("%s/%s", settings.workdir.c_str(), SIGNAL_SETTINGS_FILENAME)))
  {
    printf("Achtung! Settings file can't be loaded!\n");
  }
  Decoder->SetWorkDir(settings.workdir.c_str());
  DataFolder=gROOT->GetRootFolder()->AddFolder("DATA", "Data");
  gROOT->GetListOfBrowsables()->Add(DataFolder,"Data");

  bPedSubtr  = settings.config.pedsubtr;
  usPedShift = settings.config.pedshift;
  Decoder->SetSignalOffset(settings.config.offset);
  set_gate_file(settings.config.gatefile);


  // The first, fundamental operation to be performed in order to make ROOT
  // thread-aware.
  ROOT::EnableThreadSafety();
  /////////////////////////////////////////////////////////////////////////////
  if (settings.guimode==0 || settings.guimode==1)
  {
    const int ciDN = Decoder->LookingForDevices(settings.file.c_str());         // number of devices
    if (!ciDN) exit(-1);
    vADCMeta = Decoder->GetADCMetaData();               // retrieving ADCS structure
    viewer_histos_init();

    Decoder->PrintMetaData(vADCMeta);
    switch (settings.mode)
    {
      case 's':
        draw_single_wave();
      break;
      case 'a':
        draw_avg_wave();
      break;
      case 'i':
        draw_charge_spectrum();
      break;
      case 'g':
        printf("Gen Mode: %d\n", settings.gen_gate_type);
        switch (settings.gen_gate_type)
        {
          case 'c':
            printf("Mode:                  charge gate file generation [DevSN CH %s in points]\n",settings.gate_preset.c_str());
            Decoder->GenerateGateConfigFile(Form("%s/%s", settings.workdir.c_str(), settings.config.gatefile),settings.gate_preset.c_str(),uiNumOfDevices);
          break;
          case 'k':
            printf("Mode:                  kernel gate file generation [ID %s in points]\n",settings.kernel_gate_preset.c_str());
            Decoder->GenerateKernelGateConfigFile(Form("%s/%s", settings.workdir.c_str(), KERNEL_GATE_FILENAME),settings.kernel_gate_preset.c_str());
          break;
        }
        exit(0);
      break;
      case 't':
        switch (settings.kernel_type)
        {
          case 0:
            draw_time_spectrum_rconv();
          break;
          case 1:
            draw_time_spectrum_pconv();
          break;
          case 2:
            draw_time_spectrum_fit();
          break;
        }
      break;
      case 'x':
        processall();
      break;
    }
    // end of readout

    if (settings.kernel_creation == 1)  // load kernels after creating
    {
      switch (settings.kernel_type)
      {
        case 0:
        // Rough kernel
        load_rkernel();
        break;
        case 1:
        // Precised kernel
        load_pkernel();
        break;
      }
    }
  }
  switch (settings.guimode) {
    case 0:
      if (settings.rwf_on)
      {
        gDirectory->Close(); // cleanup th1s rwf histos
      }
      gApplication->Terminate();
    break;
    case 1:
    // gApplication->Connect("CloseWindow()", "TApplication", gApplication, "Terminate()");
      auto b = new TBrowser("Data",DataFolder,Form("DATA BROWSER - %s",settings.file.c_str()),1500,800);
      auto frame = b->GetBrowserImp()->GetMainFrame();
      frame->Connect("CloseWindow()","TApplication",gApplication,"Terminate()");
    // TQObject::Connect("Destruct()", "TApplication", gApplication, "Terminate()");
    break;
  }
  return 0;
}

//______________________________________________________________________________
void print_usage(char** argv)
{
  printf("Usage examples:\n");
  printf("%s -h                                    - this help\n",argv[0]);
  printf("\nSingle waveform processing:\n");
  printf("%s -g 0 -f file.data -m s                - show single waveform (batch mode)\n",argv[0]);
  printf("%s -g 1 -f file.data -m s -e 99          - show single waveform for the 99th event\n",argv[0]);
  printf("\nAverage waveform processing:\n");
  printf("%s -g 1 -f file.data -m a                - show average waveform overall events\n",argv[0]);
  printf("%s -g 1 -f file.data -m a -e 1000        - show average waveform within 1000 events\n",argv[0]);
  printf("%s -g 1 -f file.data -m a -e 1000 -k 1 -s 0  - show average waveform within 1000 events and generate rough kernel\n",argv[0]);
  printf("%s -g 1 -f file.data -m a -e 1000 -k 1   - show average waveform within 1000 events and generate precised kernel\n",argv[0]);
  printf("%s -g 1 -f file.data -m a -e 1000 -k 1 -c 0xa7af865_ch0.1.2.3.4.5.6.7.8.9 - show average waveform within 1000 events and generate precised kernel for the pointed channels\n",argv[0]);
  printf("\nIntegral processing:\n");
  printf("%s -g 1 -f file.data -m i                - show charge integral\n",argv[0]);
  printf("\nTime processing:\n");
  printf("%s -g 1 -f file.data -m t                - show time distribution\n",argv[0]);
  printf("%s -g 1 -f file.data -m t -p A7AF865_ch0_trig1:data1  - show time distribution using specific kernels[-s 1] for trigger[ch0] and data channels[for all other]\n",argv[0]);
  printf("%s -g 1 -f file.data -m t -s 2 -p A7AF865_ch0  - show time distribution using linear fit[-s 2] for trigger[ch0] and data channels[not used]\n",argv[0]);
  printf("\nGate files generating:\n");
  printf("%s -g 1 -f file.data -m g -r c           - generate charge gate[-r c] file with the following settings: [Dev.Num. CH 30 50 100 120 in points]\n",argv[0]);
  printf("%s -g 1 -f file.data -m g -p 10:90:100:120  - generate gate file with the defined settings: [Pedestal: 10-90 Signal: 100-120 in points]\n",argv[0]);
  printf("%s -g 1 -f file.data -m g -r k -p 300:320   - generate kernel gate[-r k] file with the defined settings: [Signal: 300-320 in points]\n",argv[0]);
  printf("\nConvertor mode:\n");
  printf("%s -g 0 -f file.data -m x                - convert the data into the ROOT file (amplitude,charge,time) ['rlog_<data_file.data>.root']\n",argv[0]);
  printf("%s -g 0 -f file.data -m x -w             - convert the data into the ROOT file AND generate ROOT file with waveforms ['rwf_<data_file.data>.root']\n",argv[0]);

  // printf("%s -g 2                                  - main user interface [this gui is default, the option can be omitted]\n",argv[0]);
  // printf("%s -g 3                                  - monitoring\n",argv[0]);
  // printf("%s -g 3 -d /path/to/data/folder          - monitoring with path initialization\n",argv[0]);
  // printf("%s -g 1 --gui=1 --file=file.data --mode=s --event=99 - alternative synapsys\n",argv[0]);
}

//______________________________________________________________________________
bool process_options(int argc, char** argv)
{
  // printf("argc %d\n", argc);
  int opt = 0;
  static struct option long_options[] =
  {
    {"gui",    required_argument, NULL, 'g' },                   // gui type 0-no gui,1-TBrowser,2-GUI,3-monitoring
    {"dir",    required_argument, NULL, 'd' },                // data directory for monitor processing
    {"file",   required_argument, NULL, 'f' },                   // filename
    {"mode",   required_argument, NULL, 'm' },                      // [s,a,g,i,t]
    {"event",  required_argument, NULL, 'e' },                  // [-m s] event or [-m a] event limit
    {"kernel", required_argument, NULL, 'k' },                 // [0,1] - kernel creation option [don't create,create]
    {"kernel_settings",    required_argument, NULL, 's' },        // [0,1] - kernel type [rough,precised]
    {"preset", required_argument, NULL, 'p' },            // gate ranges with [-m g] option or kernel gate range with [-m k] option
    {"kernel_channels_preset",    required_argument, NULL, 'c' }, // kernels channels to be using
    {"gen_gate", required_argument, NULL, 'r' },               // gate generation [0-charge|1-kernel]
    {"rwf_on", required_argument, NULL, 'w' },               // enable/disable waveform storing to the ROOT tree
    {NULL,      0                , NULL, 0   }
  };

  if (argv[1] != NULL )
  {
    if (strncmp(argv[1],"-h",2) == 0 )
    {
      print_usage(argv);
      exit(-1);
    }
  }

  // glogal while
  while ((opt = getopt_long(argc, argv,"g:", long_options, NULL )) != -1)
  {
    switch (opt)
    {
      case 'g' :
      settings.guimode = atoi(optarg);
      while ((opt = getopt_long(argc, argv,"d:f:", long_options, NULL )) != -1)
      {
        switch (opt)
        {
          case 'd' :
          settings.monitor_datadir = optarg;
          break;
          case 'f' :
          settings.file = optarg;
          while ((opt = getopt_long(argc, argv,"m:", long_options, NULL )) != -1)
          {
            switch (opt)
            {
              case 'm' :
              settings.mode = (char)(*optarg);
              switch (settings.mode)
              {
                case 's':
                while ((opt = getopt_long(argc, argv,"e:", long_options, NULL )) != -1)
                {
                  switch (opt)
                  {
                    case 'e' :
                    settings.event = atoi(optarg);
                    if (settings.event <= 0) settings.event = 1;
                    break;
                    default:
                    print_usage(argv);
                    exit(-1);
                  }
                }
                break;
                case 'a':
                while ((opt = getopt_long(argc, argv,"e:k:c:s:", long_options, NULL )) != -1)
                {
                  switch (opt)
                  {
                    case 'e' :
                    settings.event = atoi(optarg);
                    if (settings.event <= 0) settings.event = 0;  // 0 - means no limits - process all events
                    break;
                    case 'k' :
                    settings.kernel_creation = atoi(optarg);
                    if (settings.kernel_creation != 0 && settings.kernel_creation != 1) settings.kernel_creation = KERNEL_CREATION;
                    break;
                    case 's' :
                    settings.kernel_type = atoi(optarg);
                    if (settings.kernel_type != 0 && settings.kernel_type != 1) settings.kernel_type = KERNEL_TYPE;
                    break;
                    case 'c' :
                    settings.kernel_channels_preset = optarg;
                    break;
                    default:
                    print_usage(argv);
                    exit(-1);
                  }
                }
                break;
                case 'i':
                  ;
                break;
                case 'x':
                  while ((opt = getopt_long(argc, argv,"e:p:lw", long_options, NULL )) != -1)
                  {
                    switch (opt)
                    {
                      case 'e' :
                        settings.event = atoi(optarg);
                        if (settings.event <= 0) settings.event = 0;
                      break;
                      case 'p':
                        settings.kernel_ids_in_use = optarg;
                        sscanf(settings.kernel_ids_in_use.c_str(),"%x_ch%d",\
                              &settings.uiktrig_device,\
                              &settings.uiktrig_channel);
                      break;
                      case 'l' :
                        settings.write_data_log = true;
                      break;
                      case 'w' :
                        settings.rwf_on = true;   // enable waveform storing
                      break;
                      default:
                        print_usage(argv);
                        exit(-1);
                    }
                  }
                break;
                case 'g':
                  while ((opt = getopt_long(argc, argv,"r:p:", long_options, NULL )) != -1)
                  {
                    switch (opt)
                    {
                      case 'r' :
                      // printf("gen type: %c\n", optarg[0]);
                      settings.gen_gate_type = optarg[0];
                      if (settings.gen_gate_type != 'c' && settings.gen_gate_type != 'k')
                      {
                        print_usage(argv);
                        exit(-1);
                      }
                      break;
                      case 'p' :
                      settings.gate_preset = optarg;
                      settings.kernel_gate_preset = optarg;
                      break;
                      default:
                      print_usage(argv);
                      exit(-1);
                    }
                  }
                break;
                case 't':
                while ((opt = getopt_long(argc, argv,"p:s:", long_options, NULL )) != -1)
                {
                  switch (opt)
                  {
                    case 'p' :
                    settings.kernel_ids_in_use = optarg;
                    if (settings.kernel_type!=2)
                    {
                      sscanf(settings.kernel_ids_in_use.c_str(),"%x_ch%d_trig%d:data%d",\
                      &settings.uiktrig_device,\
                      &settings.uiktrig_channel,\
                      &settings.uikids_in_use[0],\
                      &settings.uikids_in_use[1]);
                    }
                    else
                    {
                      sscanf(settings.kernel_ids_in_use.c_str(),"%x_ch%d",\
                      &settings.uiktrig_device,\
                      &settings.uiktrig_channel);
                    }
                    break;
                    case 's' :
                    settings.kernel_type = atoi(optarg);
                    if (settings.kernel_type != 0 && settings.kernel_type != 1 && settings.kernel_type != 2) settings.kernel_type = KERNEL_TYPE;
                    break;
                    default:
                    print_usage(argv);
                    exit(-1);
                    break;
                  }
                }
                break;
                default:
                print_usage(argv);
                exit(-1);
              }
              break;
              default:
              print_usage(argv);
              exit(-1);
            }
          }
          break;
          default:
          print_usage(argv);
          exit(-1);
        }
      }
      break;
      default:
      print_usage(argv);
      exit(-1);
      break;
    } // end switch opt
  }
  if (settings.guimode<2 && argc<=3){
    print_usage(argv);
    exit(-1);
  }
  return true;
}

//______________________________________________________________________________
void set_meta_data()
{
  vADCMeta = Decoder->GetADCMetaData();
  Decoder->PrintMetaData(vADCMeta);
  gui->SetMetaData(vADCMeta);
  if (settings.guimode!=3) viewer_histos_init();
}

//______________________________________________________________________________
void viewer_histos_init()
{
  for (auto itH=vViewHisto.begin(); itH!=vViewHisto.end(); itH++){
    delete itH->plot;
  }
  vViewHisto.clear();

  UChar_t lineWidth = 2;
  UChar_t markerStyle = 20;
  Double_t markerSize = 0.8;
  const char *hNamePrefix[6] = {"spectrum", "dtime", "ftime", "ptime", "rtime","darkspectrum"};
  const char *hTitlePrefix[6] = {"Spectrum",
                                "Time spectrum [difference]",
                                "Time spectrum [fit]",
                                "Time spectrum [precised kernel]",
                                "Time spectrum [rough kernel]",
                                "Dark noise spectrum"
                              };
  char hXaxis[64];
  switch (SCALE_REPRESENTATION)
  {
     case 0:
        sprintf(hXaxis,"Charge, adc channels");
     break;
     case 1:
        sprintf(hXaxis,"Charge, pC");
     break;
  }
  const char *hXAxisTitle[6] = {hXaxis,
                               "Time, ns",
                               "Time, ns",
                               "Time, ns",
                               "Time, ns",
                                hXaxis
                             };


  int colors[6] = { kBlue, kBlue, kViolet, kViolet, kViolet, kBlack};
  Float_t hRanges[6][3] = {
    {NUMBER_OF_BINS,BIN_FIRST_CHARGE,BIN_LAST_CHARGE},
    {NUMBER_OF_TIMEDIFF_BINS*TIMEDIFF_BIN_FACTOR,TIMEDIFF_BIN_FIRST,TIMEDIFF_BIN_LAST},
    {NUMBER_OF_TIME_BINS*TIME_BIN_FACTOR,0,NUMBER_OF_TIME_BINS},
    {NUMBER_OF_TIME_BINS*TIME_BIN_FACTOR,0,NUMBER_OF_TIME_BINS},
    {NUMBER_OF_TIME_BINS*TIME_BIN_FACTOR,0,NUMBER_OF_TIME_BINS},
    {NUMBER_OF_BINS,BIN_FIRST_CHARGE,BIN_LAST_CHARGE}
  };

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto sn = (*itD).devsn;
    ViewHisto.devsn = sn;
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      ViewHisto.isExist.reset();

      if ((*itD).ach.test(ch))
      {
        ViewHisto.ch = ch;

        // Charge histo
        for (int pt=4; pt<=9; pt++)
        {
          auto rng = hRanges[pt-4];
          ViewHisto.type = pt;
          ViewHisto.plot = new TH1F(Form("%s_%x_ch%02d",hNamePrefix[pt-4], sn,ch),\
                                  Form("%s [%x CH%02d];%s;Number of counts, N",hTitlePrefix[pt-4], sn,ch, hXAxisTitle[pt-4]),\
                                  rng[0],rng[1],rng[2]);
          ((TH1F *)ViewHisto.plot)->SetLineColor(colors[pt-4]);
          ViewHisto.isExist.set(ViewHisto.type);
          vViewHisto.push_back(ViewHisto);
        }
      }
    }
  }
}

//______________________________________________________________________________
void viewer_histos_clear()
{
  Int_t iNBins = NUMBER_OF_BINS;
  Double_t dFirstBin = BIN_FIRST_CHARGE;
  Double_t dLastBin = BIN_LAST_CHARGE;
  if (settings.guimode==2){
    iNBins = int(gui->GetNBins());
    dFirstBin = gui->GetFirstBin();
    dLastBin = gui->GetLastBin();
  }
  for (auto itH=vViewHisto.begin(); itH!=vViewHisto.end(); itH++)
  {
    TH1F *histo = (TH1F*)itH->plot;
    histo->Reset();
    if (itH->type==4 || itH->type==9) histo->SetBins(iNBins, dFirstBin, dLastBin);
  }
}

//______________________________________________________________________________
void reset_kernel_load_state()
{
  bPKernelLoaded = false;
  bRKernelLoaded = false;
}

//______________________________________________________________________________
void draw_charge_spectrum()
{
  std::mutex m;

  iError=0;
  DataFolder->Clear();
  vADCMeta = Decoder->GetADCMetaData();               // retrieving ADCS structure
  const int ciDN = vADCMeta->size();                  // number of devices
  TFolder *DeviceFolder[ciDN];
  TFolder *DeviceFolderPed[ciDN];
  UInt_t uiTotEvents[ciDN]; for (int i=0; i<ciDN; i++) uiTotEvents[i]=0;
  UInt_t uiEvtsMax = 0x0;
  UInt_t uiFirstEvent = 1;
  Int_t iNBins       = NUMBER_OF_BINS;
  Double_t dFirstBin = BIN_FIRST_CHARGE;
  Double_t dLastBin  = BIN_LAST_CHARGE;
  if (settings.guimode==2){
    iNBins = int(gui->GetNBins());
    dFirstBin = gui->GetFirstBin();
    dLastBin = gui->GetLastBin();
    uiFirstEvent = gui->GetAvgFirstEventNumber();
  }
  // printf("NBins %d FirstBin %.2f LastBin %.2f\n", iNBins, dFirstBin, dLastBin);
  printf("\nMode:                  integral spectrum\n");

  load_charge_gate();

  viewer_histos_clear();

  TH1F *Histo[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];
  TH1F *HistoPed[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];
  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = (*itD).devorn;
    auto devsn = (*itD).devsn;

    if (settings.guimode==2) uiTotEvents[d] = gui->GetAvgLastEventNumber();
    else{
      if (settings.event==1) uiTotEvents[d] = itD->ev;
      else uiTotEvents[d] = settings.event;
    }
    if (settings.event <= 0  || uiTotEvents[d] > itD->ev)
      uiTotEvents[d] = itD->ev;
    // printf("uiTotEvents %d\n", uiTotEvents[d]);
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if ((*itD).ach.test(ch))
      {
        auto gatePtr = std::find_if(vGate->begin(), vGate->end(),
                        [devsn, ch](const GATE &gate){
                          return gate.devsn==devsn && gate.ch==ch;
                        });
        if (gatePtr==vGate->end())
        {
          printf("Achtung: Device has not been found in %s!\n", settings.config.gatefile);
          iError = ERROR_GATE_EMPTY;
          return;
        }

        for (int t=0; t<THREAD_NUMBER; t++)
        {
          Histo[d][ch][t] = new TH1F(Form("shisto_dev%02d_ch%02d_worker%02d",
                                          d,ch,t),
                                     Form("shisto_dev%02d_ch%02d_worker%02d",
                                          d,ch,t), iNBins,dFirstBin,dLastBin);
          HistoPed[d][ch][t] = new TH1F(Form("shisto_ped_dev%02d_ch%02d_worker%02d",
                                          d,ch,t),
                                     Form("shisto_ped_dev%02d_ch%02d_worker%02d",
                                          d,ch,t), iNBins,dFirstBin,dLastBin);
        }
      }
    }
  }

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    if (uiTotEvents[d]>uiEvtsMax) uiEvtsMax=uiTotEvents[d];
  }
  if (settings.guimode==2){
    gui->GetProgressBar()->Reset();
    gui->GetProgressBar()->SetMax(uiEvtsMax);
  }

  // Decoder->LogInit();

  gSystem->ProcessEvents();  // handle GUI events

  //////////////////////////////////////////////////////////////////////////////
  ////////////////Draw Charge Spectrum lambda///////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  auto workDrawSpectrum = [ciDN,uiFirstEvent,uiEvtsMax,&uiTotEvents,&Histo,&HistoPed,&m](UInt_t workerID)
  {
    std::vector<ADCS> meta(*vADCMeta);
    INTEGRAL *integral = 0;
    iError=0;
    std::vector<DATA> vD;        // vector of data structures with different device id <data device id#1 ... data device id#N>
    Float_t fChargeMap[ciDN][CHANNEL_NUMBER];
    Float_t fChargeShiftedMap[ciDN][CHANNEL_NUMBER];
    FILE *fd[ciDN];

    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d = (*itD).devorn;

      fd[d] = Decoder->OpenDataFile(Form("%s/%s",(*itD).dir,(*itD).file),0);  // second parameter disables indexation
      // printf("ThreadID: %d d: %d FD: %p Data: %p [%s]\n", workerID,d,fd[d],&vD,meta.at(d).file);
      if (!fd[d]) {
        iError=ERROR_DATAFILE_OPEN;
        return;
      }
    }

    for (auto e=uiFirstEvent;  e<=uiEvtsMax; e+=THREAD_NUMBER)
    {
      // printf("Event#%d wid: %d\n",e+workerID,workerID);
      for (auto itD=meta.begin(); itD!=meta.end(); itD++)
      {
        auto d = (*itD).devorn;
        if (e+workerID > uiTotEvents[d]) continue;
        // if (e>10000) printf("Event#%d WorkerID %02d Device: %x\n", e+workerID, workerID, (*itD).devsn);
        switch (DEVICE_TYPE)
        {
          case DEVICE_ID_ADC64:
            iError = Decoder->ReadEventADC64(fd[d],e+workerID,&vD,VERBOSE_MODE,d, (*itD).devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          break;
          case DEVICE_ID_TQDC:
            iError = Decoder->ReadEventTQDC(fd[d],e+workerID,&vD,VERBOSE_MODE,d, (*itD).devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          break;
        }
        if (iError!=0)
        {
          continue;
        }

        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          fChargeMap[d][ch]        = 0;
          fChargeShiftedMap[d][ch] = 0;
          if ((*itD).ach.test(ch))
          {
            integral = Decoder->GetIntegral( (*itD).devsn, ch, &vD.at(0).wf[ch], workerID );
            if (bPedSubtr)
            {
              fChargeMap[d][ch] = integral->cint;
              fChargeShiftedMap[d][ch] = integral->cint_sh;
              // fAmpMap[d][ch] = integral->amp;
            }
            else
            {
              fChargeMap[d][ch] = integral->csig;
              fChargeShiftedMap[d][ch] = integral->csig_sh;
              // fAmpMap[d][ch] = integral->amp_sh;
            }
            Histo[d][ch][workerID]->Fill(fChargeMap[d][ch]+usPedShift);
            HistoPed[d][ch][workerID]->Fill(fChargeShiftedMap[d][ch]+usPedShift);
          }
        }
        // Decoder->LogFill(workerID, fIntegral[d], (*itD).devsn, e, 'i',ULong_t(vD.at(0).tainsec)); // last parameter is trigger time in ns
      }
      if (settings.guimode==2 && workerID == 0)
      {
        gui->GetProgressBar()->Increment(THREAD_NUMBER);
      }
    }

    std::lock_guard<std::mutex> lock(m);

    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d  = (*itD).devorn;
      auto sn = (*itD).devsn;
      Decoder->CloseDataFile(fd[d]);
      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        if ((*itD).ach.test(ch))
        {
          auto histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
                          [sn,ch] (const MON_PLOT& plot) {
                            return plot.devsn == sn && plot.ch == ch && plot.type == 4;
                          });
          ((TH1F*)histoPtr->plot)->Add(Histo[d][ch][workerID]);

          auto histoPedPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
                          [sn,ch] (const MON_PLOT& plot) {
                            return plot.devsn == sn && plot.ch == ch && plot.type == 9;
                          });
          ((TH1F*)histoPedPtr->plot)->Add(HistoPed[d][ch][workerID]);
        }
      }
    }

  };
  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////End of lambda///////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////

  printf("Readout limits:        %d-%d\n", uiFirstEvent, uiEvtsMax);

  Watch->Reset();
  Watch->Start();
  // Fill the "pool" with workers
  workers.clear();
  for (auto id=0; id<THREAD_NUMBER; id++)
    workers.emplace_back(workDrawSpectrum, id);
  // Now join them
  for (auto &&worker : workers) worker.join();
  Watch->Stop();
  printf("The code took %f seconds\n", Watch->RealTime());

  // Decoder->LogWrite(LOG_DATA_FILENAME);

  // if (iError) {
  //   printf("Error %d\n", iError);
  //   if (settings.guimode!=2){
  //     exit(iError);
  //   }
  //   else {;}
  // }
  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d  = itD->devorn;
    auto sn = itD->devsn;
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch))
      {
        for (int t=0; t<THREAD_NUMBER; t++)
        {
          delete Histo[d][ch][t];
          delete HistoPed[d][ch][t];
        }
      }
    }
  }

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d  = itD->devorn;
    auto sn = itD->devsn;
    DeviceFolder[d] = DataFolder->AddFolder(Form("ADC%02dSignal_%x",d,itD->devsn),Form("ADC%02d_%x",d,itD->devsn));
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch)){
        auto histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
        [sn,ch] (const MON_PLOT& histo) {
          return histo.devsn == sn && histo.ch == ch && histo.type == 4;
        });
        DeviceFolder[d]->Add((TH1F*)histoPtr->plot);
      }
    }
  }
  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d  = itD->devorn;
    auto sn = itD->devsn;
    DeviceFolderPed[d] = DataFolder->AddFolder(Form("ADC%02dNoise_%x",d,itD->devsn),Form("ADC%02d_%x",d,itD->devsn));
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch)){
        auto histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
        [sn,ch] (const MON_PLOT& histo) {
          return histo.devsn == sn && histo.ch == ch && histo.type == 9;
        });
        DeviceFolderPed[d]->Add((TH1F*)histoPtr->plot);
      }
    }
  }

  if (settings.guimode==2) gui->SetFolder(DataFolder);
}

//______________________________________________________________________________
void draw_time_spectrum_pconv()
{
  std::mutex m;

  vADCMeta = Decoder->GetADCMetaData();               // retrieving ADCS structure
  const int ciDN = vADCMeta->size();                    // number of devices

  UInt_t uiTrigDevice = settings.uiktrig_device;
  UInt_t uiTrigChannel = settings.uiktrig_channel;
  UInt_t uiTrigIndex = settings.uikids_in_use[0];
  UInt_t uiDataIndex = settings.uikids_in_use[1];
  UInt_t uiTotEvents[ciDN]; for (int i=0; i<ciDN; i++) uiTotEvents[i]=0;
  UInt_t uiEvtsMin = 0xffffffff;
  UInt_t uiFirstEvent = 1;
  if (settings.guimode<2)
  {
    Decoder->SetConvDiffLeft(settings.config.convrange[0]);
    Decoder->SetConvDiffRight(settings.config.convrange[1]);
  }
  else
  {
    Decoder->SetConvDiffLeft(gui->GetConvDiffLeft());
    Decoder->SetConvDiffRight(gui->GetConvDiffRight());
    uiFirstEvent = gui->GetAvgFirstEventNumber();
    uiTrigDevice = gui->GetTriggerDeviceSN();
    uiTrigChannel = gui->GetTriggerCH();
    uiTrigIndex = gui->GetAvgKernelTrigID();
    uiDataIndex = gui->GetAvgKernelDataID();
  }

  DataFolder->Clear();
  DataFolder->Add(KernelFolder);
  TFolder *DeviceFolder[ciDN];
  TFolder *DeviceFolderDiff[ciDN];
  TH1F *Histo[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];
  TH1F *HistoDiff[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];

  load_pkernel();
  viewer_histos_clear();

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    if (settings.guimode==2) uiTotEvents[d] = gui->GetAvgLastEventNumber();
    else{
      if (settings.event==1) uiTotEvents[d] = itD->ev;
      else uiTotEvents[d] = settings.event;
    }
    if (settings.event <= 0  || uiTotEvents[d] > itD->ev) uiTotEvents[d] = itD->ev;

    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch))
      {
        for (int t=0; t<THREAD_NUMBER; t++){
          Histo[d][ch][t] = new TH1F(Form("thisto_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                        Form("thisto [DEV%02d CH%02d worker%02d]",d,ch,t),\
                                        NUMBER_OF_TIME_BINS*TIME_BIN_FACTOR,0,NUMBER_OF_TIME_BINS);
          HistoDiff[d][ch][t] = new TH1F(Form("thisto_diff_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                        Form("thisto diff [DEV%02d CH%02d worker%02d]",d,ch,t),\
                                        NUMBER_OF_TIMEDIFF_BINS*TIMEDIFF_BIN_FACTOR,TIMEDIFF_BIN_FIRST,TIMEDIFF_BIN_LAST);
        }
      }
    }
  }
  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    if (uiTotEvents[d]<uiEvtsMin) uiEvtsMin=uiTotEvents[d];
  }
  if (settings.guimode==2){
    gui->GetProgressBar()->Reset();
    gui->GetProgressBar()->SetMax(uiEvtsMin);
  }

  // Draw Time Spectrum using precised kernel - lambda ////////////////////////
  auto workDrawPrecisedTimeSpectrum = [ciDN,uiTrigDevice,uiTrigChannel,uiTrigIndex,uiDataIndex,uiFirstEvent,&uiTotEvents,
                                      &uiEvtsMin,&Histo,&HistoDiff,&m](UInt_t workerID)
  {
    std::vector<ADCS> meta(*vADCMeta);
    iError=0;
    std::vector<DATA> vD;        // vector of data structures with different device id <data device id#1 ... data device id#N>
    vD.clear();
    std::vector<DATA> vData;

    float fDiff = 0;
    float fTime = 0;
    float fTrigTime = 0;

    FILE *fd[ciDN];
    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d = (*itD).devorn;
      fd[d] = Decoder->OpenDataFile(Form("%s/%s",(*itD).dir,(*itD).file),0);  // second parameter disables indexation
      if (!fd[d]) {
        iError=ERROR_DATAFILE_OPEN;
        return;
      }
    }

    for (auto e=uiFirstEvent;  e<=uiEvtsMin; e+=THREAD_NUMBER)
    {
      vData.clear();
      fTrigTime = 0;
      if (e+workerID > uiEvtsMin) break;

      for (auto itD=meta.begin(); itD!=meta.end(); itD++)
      {
        auto d = (*itD).devorn;
        auto devsn = (*itD).devsn;
        switch (DEVICE_TYPE)
        {
          case DEVICE_ID_ADC64:
            iError = Decoder->ReadEventADC64(fd[d],e+workerID,&vD,VERBOSE_MODE,d, devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          break;
          case DEVICE_ID_TQDC:
            iError = Decoder->ReadEventTQDC(fd[d],e+workerID,&vD,VERBOSE_MODE,d, devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          break;
        }
        if (iError!=0) continue;

        vData.push_back(vD.at(0));

        if ( ((*itD).devsn==uiTrigDevice) && ((*itD).ach.test(uiTrigChannel)) )
          fTrigTime = Decoder->GetPrecisedConvMin(d,uiTrigIndex,uiTrigChannel,(*itD).sn,&vData,vPKernel,vPKGate);
      }

      if(vData.empty()) continue;

      for (auto itD=meta.begin(); itD!=meta.end(); itD++)
      {
        auto d = (*itD).devorn;
        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          if ((*itD).ach.test(ch))
          {
            fTime = 0;
            fTime     = Decoder->GetPrecisedConvMin(d,uiDataIndex,ch,(*itD).sn,&vData,vPKernel,vPKGate);
            fDiff = fTime - fTrigTime;
            // printf("diff: %f\n", fDiff);
            Histo[d][ch][workerID]->Fill(fTime);
            HistoDiff[d][ch][workerID]->Fill(fDiff + usPedShift);
          }
        }
      }

      if (settings.guimode==2 && workerID == 0)
      {
        gui->GetProgressBar()->Increment(THREAD_NUMBER);
      }
    }

    std::lock_guard<std::mutex> lock(m);

    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d = (*itD).devorn;
      auto sn = (*itD).devsn;
      Decoder->CloseDataFile(fd[d]);
      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        if ((*itD).ach.test(ch))
        {
          // time pconv histo
          auto histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
                          [sn,ch] (const MON_PLOT& plot) {
                            return plot.devsn == sn && plot.ch == ch && plot.type == 7;
                          });
          ((TH1F*)histoPtr->plot)->Add(Histo[d][ch][workerID]);
          // timediff histo
          histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
                          [sn,ch] (const MON_PLOT& plot) {
                            return plot.devsn == sn && plot.ch == ch && plot.type == 5;
                          });
          ((TH1F*)histoPtr->plot)->Add(HistoDiff[d][ch][workerID]);
        }
      }
    }
  };
  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////////END OF LAMBDA/////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  printf("Mode:                  time spectrum\n");
  printf("Processing type:       using convolution with the precised kernel\n");
  printf("Trigger kernel ID:     %d\n", uiTrigIndex);
  printf("Data    kernel ID:     %d\n", uiDataIndex);
  printf("Trigger device:        %x\n", uiTrigDevice);
  printf("Trigger channel:       %d\n", uiTrigChannel);
  printf("Readout limits:        %d-%d\n", uiFirstEvent, uiEvtsMin);

  if (vPKernel->empty())
  {
    iError = ERROR_KERNEL_EMPTY;
    printf("Achtung: Kernel panic. The kernel is empty!\nCan't analyse data.\n");
    if (settings.guimode!=2) exit(iError);
  }

  Watch->Reset();
  Watch->Start();
  workers.clear();
  for (auto id=0; id<THREAD_NUMBER; id++)
    workers.emplace_back(workDrawPrecisedTimeSpectrum, id);

  for (auto &&worker : workers) worker.join();
  Watch->Stop();
  printf("The code took          %f seconds\n", Watch->RealTime());
  // if (iError) {
  //   if (settings.guimode!=2){ exit(iError); }
  //   else {;}
  // }

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch))
      {
        for (int t=0; t<THREAD_NUMBER; t++){
          delete Histo[d][ch][t];
          delete HistoDiff[d][ch][t];
        }
      }
    }
  }

  // FILE *fdLog = fopen(LOG_FILENAME,"at");

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d  = itD->devorn;
    auto sn = itD->devsn;
    DeviceFolder[d] = DataFolder->AddFolder(Form("ADC%02d_%x_time",d,sn),Form("ADC%02d_time",d));
    DeviceFolderDiff[d] = DataFolder->AddFolder(Form("ADC%02d_%x_diff_time",d,sn),Form("ADC%02d_time_difference [chXX-ch%02d]",d,TRIGGER_CHANNEL));
    for (int ch=0; ch<CHANNEL_NUMBER; ch++){
      if (itD->ach.test(ch))
      {
        auto histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
        [sn,ch] (const MON_PLOT& histo) {
          return histo.devsn == sn && histo.ch == ch && histo.type == 7;
        });
        DeviceFolder[d]->Add((TH1F*)histoPtr->plot);

        histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
        [sn,ch] (const MON_PLOT& histo) {
          return histo.devsn == sn && histo.ch == ch && histo.type == 5;
        });
        DeviceFolderDiff[d]->Add((TH1F*)histoPtr->plot);
      }
    }
  }
  // fclose(fdLog);
  if (settings.guimode==2) gui->SetFolder(DataFolder);
}

//______________________________________________________________________________
void draw_time_spectrum_rconv()
{
  std::mutex m;

  vADCMeta = Decoder->GetADCMetaData();               // retrieving ADCS structure
  const int ciDN = vADCMeta->size();                    // number of devices

  UInt_t uiFirstEvent = 1;
  UInt_t uiTotEvents[ciDN]; for (int i=0; i<ciDN; i++) uiTotEvents[i]=0;
  UInt_t uiEvtsMin = 0xffffffff;
  UInt_t uiTrigDevice = settings.uiktrig_device;
  UInt_t uiTrigChannel = settings.uiktrig_channel;
  if (settings.guimode<2)
  {
    Decoder->SetConvDiffLeft(settings.config.convrange[0]);
    Decoder->SetConvDiffRight(settings.config.convrange[1]);
  }
  else
  {
    Decoder->SetConvDiffLeft(gui->GetConvDiffLeft());
    Decoder->SetConvDiffRight(gui->GetConvDiffRight());
    uiFirstEvent = gui->GetAvgFirstEventNumber();
    uiTrigDevice = gui->GetTriggerDeviceSN();
    uiTrigChannel = gui->GetTriggerCH();
  }

  DataFolder->Clear();
  DataFolder->Add(KernelFolder);
  TFolder *DeviceFolder[ciDN];
  TFolder *DeviceFolderDiff[ciDN];
  TH1F *SpectrumRTime[ciDN][CHANNEL_NUMBER];
  TH1F *SpectrumTimeDiff[ciDN][CHANNEL_NUMBER];
  TH1F *Histo[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];
  TH1F *HistoDiff[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];

  load_rkernel();
  if (iError){
    return;
  }
  viewer_histos_clear();

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    if (settings.guimode==2) uiTotEvents[d] = gui->GetAvgLastEventNumber();
    else{
      if (settings.event==1) uiTotEvents[d] = itD->ev;
      else uiTotEvents[d] = settings.event;
    }
    if (settings.event <= 0  || uiTotEvents[d] > itD->ev) uiTotEvents[d] = itD->ev;

    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch) && vRKernel->at(d).chstate[ch])
      {
        for (int t=0; t<THREAD_NUMBER; t++){
          Histo[d][ch][t] = new TH1F(Form("thisto_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                        Form("thisto [DEV%02d CH%02d worker%02d]",d,ch,t),\
                                        NUMBER_OF_TIME_BINS,0,NUMBER_OF_TIME_BINS);
          HistoDiff[d][ch][t] = new TH1F(Form("dtime_adc%02d_ch%02d_worker%02ds",d,ch,t),\
                                        Form("Time spectrum [ADC%02d CH%02d TriggerCH%02d worker%02d]",d,ch,uiTrigChannel,t),\
                                        NUMBER_OF_TIMEDIFF_BINS*TIMEDIFF_BIN_FACTOR,TIMEDIFF_BIN_FIRST,TIMEDIFF_BIN_LAST);
        }
      }
    }
  }

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    if (uiTotEvents[d]<uiEvtsMin) uiEvtsMin=uiTotEvents[d];
  }
  if (settings.guimode==2){
    gui->GetProgressBar()->Reset();
    gui->GetProgressBar()->SetMax(uiEvtsMin);
  }

  // Draw Time Spectrum lambda /////////////////////////////////////////////////
  auto workDrawRoughTimeSpectrum = [ciDN,uiTrigDevice,uiTrigChannel,uiFirstEvent,uiEvtsMin,\
                                    &uiTotEvents,&Histo,&HistoDiff,&m](UInt_t workerID)
  {
    std::vector<ADCS> meta(*vADCMeta);
    iError=0;
    std::vector<DATA> vD;        // vector of data structures with different device id <data device id#1 ... data device id#N>
    vD.clear();
    std::vector<DATA> vData;

    float fDiff = 0;
    float fTime = 0;
    float fTrigTime = 0;

    FILE *fd[ciDN];
    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d = (*itD).devorn;
      fd[d] = Decoder->OpenDataFile(Form("%s/%s",(*itD).dir,(*itD).file),0,(*itD).devsn);  // second parameter disables indexation
      if (!fd[d]) {
        iError=ERROR_DATAFILE_OPEN;
        return;
      }
    }

    for (auto e=uiFirstEvent;  e<=uiEvtsMin; e+=THREAD_NUMBER)
    {
      vData.clear();
      fTrigTime = 0;
      if (e+workerID > uiEvtsMin) break;

      for (auto itD=meta.begin(); itD!=meta.end(); itD++)
      {
        auto d = (*itD).devorn;
        auto devsn = (*itD).devsn;
        switch (DEVICE_TYPE)
        {
          case DEVICE_ID_ADC64:
            iError = Decoder->ReadEventADC64(fd[d],e+workerID,&vD,VERBOSE_MODE,d, devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          break;
          case DEVICE_ID_TQDC:
            iError = Decoder->ReadEventTQDC(fd[d],e+workerID,&vD,VERBOSE_MODE,d, devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          break;
        }
        if (iError!=0) continue;

        vData.push_back(vD.at(0));

        if ( ((*itD).devsn==uiTrigDevice) && ((*itD).ach.test(uiTrigChannel)) )
        {
          fTrigTime = Decoder->GetConvMin(d,uiTrigChannel,(*itD).sn,&vData,vRKernel);
        }
      }

      if(vData.empty()) continue;

      for (auto itD=meta.begin(); itD!=meta.end(); itD++)
      {
        auto d = (*itD).devorn;
        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          fTime = 0;
          if ((*itD).ach.test(ch) == true && vRKernel->at(d).chstate[ch] == true)
          {
            fTime = Decoder->GetConvMin(d,ch,(*itD).sn,&vData,vRKernel);
            fDiff = fTime - fTrigTime;
            Histo[d][ch][workerID]->Fill(fTime);
            HistoDiff[d][ch][workerID]->Fill(fDiff + usPedShift);
          }
        }
      }
      if (settings.guimode==2 && workerID == 0)
      {
        gui->GetProgressBar()->Increment(THREAD_NUMBER);
      }
    }

    std::lock_guard<std::mutex> lock(m);

    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d  = (*itD).devorn;
      auto sn = (*itD).devsn;
      Decoder->CloseDataFile(fd[d]);
      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        if ((*itD).ach.test(ch) && vRKernel->at(d).chstate[ch])
        {
          // time rconv histo
          auto histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
                          [sn,ch] (const MON_PLOT& plot) {
                            return plot.devsn == sn && plot.ch == ch && plot.type == 8;
                          });
          ((TH1F*)histoPtr->plot)->Add(Histo[d][ch][workerID]);
          // timediff histo
          histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
                          [sn,ch] (const MON_PLOT& plot) {
                            return plot.devsn == sn && plot.ch == ch && plot.type == 5;
                          });
          ((TH1F*)histoPtr->plot)->Add(HistoDiff[d][ch][workerID]);
        }
      }
    }
  };
  ////////////////////////////////////////////////////////////

  printf("Mode:                  time spectrum\n");
  printf("Processing type:       using convolution with the rough kernel\n");
  printf("Trigger device:        %x\n", uiTrigDevice);
  printf("Trigger channel:       %d\n", uiTrigChannel);

  if (vRKernel->empty())
  {
    iError = ERROR_KERNEL_EMPTY;
    printf("Achtung: Kernel panic. The kernel is empty!\nCan't analyse data.\n");
    if (settings.guimode!=2) exit(iError);
  }

  Watch->Reset();
  Watch->Start();
  workers.clear();
  for (auto id=0; id<THREAD_NUMBER; id++)
    workers.emplace_back(workDrawRoughTimeSpectrum, id);

  for (auto &&worker : workers) worker.join();
  Watch->Stop();
  printf("The code took          %f seconds\n", Watch->RealTime());
  // if (iError) {
  //   if (settings.guimode!=2){ exit(iError); }
  //   else {;}
  // }

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch) && vRKernel->at(d).chstate[ch])
      {
        for (int t=0; t<THREAD_NUMBER; t++){
          delete Histo[d][ch][t];
          delete HistoDiff[d][ch][t];
        }
      }
    }
  }

  // FILE *fdLog = fopen(LOG_FILENAME,"at");

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d  = itD->devorn;
    auto sn = itD->devsn;
    DeviceFolder[d] = DataFolder->AddFolder(Form("ADC%02d_%x_time",d,itD->devsn),Form("ADC%02d_time",d));
    DeviceFolderDiff[d] = DataFolder->AddFolder(Form("ADC%02d_%x_diff_time",d,itD->devsn),Form("ADC%02d_time_difference [chXX-ch%02d]",d,TRIGGER_CHANNEL));
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch))
      {
        auto histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
        [sn,ch] (const MON_PLOT& histo) {
          return histo.devsn == sn && histo.ch == ch && histo.type == 8;
        });
        DeviceFolder[d]->Add((TH1F*)histoPtr->plot);

        histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
        [sn,ch] (const MON_PLOT& histo) {
          return histo.devsn == sn && histo.ch == ch && histo.type == 5;
        });
        DeviceFolderDiff[d]->Add((TH1F*)histoPtr->plot);
      }
    }
  }

  // fclose(fdLog);
  if (settings.guimode==2) gui->SetFolder(DataFolder);
}

//______________________________________________________________________________
void draw_time_spectrum_fit()
{
  std::mutex m;

  vADCMeta = Decoder->GetADCMetaData();               // retrieving ADCS structure
  const int ciDN = vADCMeta->size();                    // number of devices
  UInt_t uiFirstEvent = 1;
  UInt_t uiTotEvents[ciDN]; for (int i=0; i<ciDN; i++) uiTotEvents[i]=0;
  UInt_t uiEvtsMin = 0xffffffff;
  UInt_t uiTrigDevice = settings.uiktrig_device;
  UInt_t uiTrigChannel = settings.uiktrig_channel;
  if (settings.guimode<2)
  {
    Decoder->SetConvDiffLeft(settings.config.convrange[0]);
    Decoder->SetConvDiffRight(settings.config.convrange[1]);
  }
  else
  {
    Decoder->SetConvDiffLeft(gui->GetConvDiffLeft());
    Decoder->SetConvDiffRight(gui->GetConvDiffRight());
    uiFirstEvent  = gui->GetAvgFirstEventNumber();
    uiTrigDevice  = gui->GetTriggerDeviceSN();
    uiTrigChannel = gui->GetTriggerCH();
  }

  DataFolder->Clear();
  DataFolder->Add(KernelFolder);
  TFolder *DeviceFolder[ciDN];
  TFolder *DeviceFolderDiff[ciDN];
  TH1F *Histo[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];
  TH1F *HistoDiff[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];

  load_charge_gate();
  viewer_histos_clear();

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    if (settings.guimode==2) uiTotEvents[d] = gui->GetAvgLastEventNumber();
    else{
      if (settings.event==1) uiTotEvents[d] = itD->ev;
      else uiTotEvents[d] = settings.event;
    }
    if (settings.event <= 0  || uiTotEvents[d] > itD->ev) uiTotEvents[d] = itD->ev;

    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch))
      {
        for (int t=0; t<THREAD_NUMBER; t++){
          Histo[d][ch][t] = new TH1F(Form("thisto_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                        Form("thisto [DEV%02d CH%02d worker%02d]",d,ch,t),\
                                        NUMBER_OF_TIME_BINS*TIME_BIN_FACTOR,0,NUMBER_OF_TIME_BINS);
          HistoDiff[d][ch][t] = new TH1F(Form("thisto_diff_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                        Form("thisto diff [DEV%02d CH%02d worker%02d]",d,ch,t),\
                                        NUMBER_OF_TIMEDIFF_BINS*TIMEDIFF_BIN_FACTOR,TIMEDIFF_BIN_FIRST,TIMEDIFF_BIN_LAST);
        }
      }
    }
  }

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    if (uiTotEvents[d]<uiEvtsMin) uiEvtsMin=uiTotEvents[d];
  }
  if (settings.guimode==2){
    gui->GetProgressBar()->Reset();
    gui->GetProgressBar()->SetMax(uiEvtsMin);
  }

  // Draw Time Spectrum using linear fit - lambda /////////////////////////////
  // Only for negative signals ////////////////////////////////////////////////
  auto workDrawFitTimeSpectrum = [ciDN,uiTrigDevice,uiTrigChannel,uiFirstEvent,uiEvtsMin,&uiTotEvents,\
                                  &Histo,&HistoDiff,&m](UInt_t workerID)
  {
    std::vector<ADCS> meta(*vADCMeta);
    iError=0;
    std::vector<DATA> vD;        // vector of data structures with different device id <data device id#1 ... data device id#N>
    vD.clear();
    std::vector<DATA> vData;

    float fDiff = 0;
    float fTime = 0;
    float fTrigTime = 0;
    Float_t fA = 0;   // amplitude of signal - minimum of waveform
    Float_t fAT = 0;   // amplitude of signal - minimum of waveform for Trigger

    FILE *fd[ciDN];
    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d = (*itD).devorn;
      fd[d] = Decoder->OpenDataFile(Form("%s/%s",(*itD).dir,(*itD).file),0,(*itD).devsn);  // second parameter disables indexation
      if (!fd[d]) {
        iError=ERROR_DATAFILE_OPEN;
        return;
      }
    }

    for (auto e=uiFirstEvent;  e<=uiEvtsMin; e+=THREAD_NUMBER)
    {
      vData.clear();
      fTrigTime = 0;
      if (e+workerID > uiEvtsMin) break;

      for (auto itD=meta.begin(); itD!=meta.end(); itD++)
      {
        auto d = (*itD).devorn;
        auto devsn = (*itD).devsn;
        switch (DEVICE_TYPE)
        {
          case DEVICE_ID_ADC64:
            iError = Decoder->ReadEventADC64(fd[d],e+workerID,&vD,VERBOSE_MODE,d, devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          break;
          case DEVICE_ID_TQDC:
            iError = Decoder->ReadEventTQDC(fd[d],e+workerID,&vD,VERBOSE_MODE,d, devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          break;
        }
        if (iError!=0) continue;

        vData.push_back(vD.at(0));

        if ( ((*itD).devsn==uiTrigDevice) && ((*itD).ach.test(uiTrigChannel)) )
        {
          if (!FIT_DISCRETISATION_IMPOVEMENT)
          {
            // not improved
            fTrigTime = Decoder->GetTimeFitCrossPoint((*itD).devsn, uiTrigChannel, (*itD).sn, \
                                                      &vData.at(d).wf[uiTrigChannel]);
          }
          else
          {
            fTrigTime = Decoder->GetTimeFitCrossPointImproved((*itD).devsn, uiTrigChannel,\
                                &vD.at(0).wf[uiTrigChannel], &fAT);
          }
         }
        }

        if(vData.empty()) continue;

        for (auto itD=meta.begin(); itD!=meta.end(); itD++)
        {
          auto d = (*itD).devorn;
          for (int ch=0; ch<CHANNEL_NUMBER; ch++)
          {
            if ((*itD).ach.test(ch))
            {
              fTime = 0;
              if (!FIT_DISCRETISATION_IMPOVEMENT)
              {
                // not improved
                fTime     = Decoder->GetTimeFitCrossPoint((*itD).devsn, ch, (*itD).sn, \
                                                          &vData.at(d).wf[ch]);
              }
              else
              {
                // improved
                // if (e==1) printf("Data channel: %d\n",ch);
                fTime     = Decoder->GetTimeFitCrossPointImproved((*itD).devsn, ch, \
                                                          &vData.at(d).wf[ch], &fA);
                // if (e==1) printf("Trigger channel: %d\n",uiTrigChannel);
              }
              fDiff = fTime - fTrigTime;
              //if (ch == 0) printf("Data time: %f TrigTime: %f DiffTime: %f\n", fTime,fTrigTime,fDiff);
              if (!FIT_DISCRETISATION_IMPOVEMENT)
              {
                Histo[d][ch][workerID]->Fill(fTime*KERNEL_NUMBER);
                HistoDiff[d][ch][workerID]->Fill(fDiff*KERNEL_NUMBER + usPedShift);
              }
              else
              {
                Histo[d][ch][workerID]->Fill(fTime);
                HistoDiff[d][ch][workerID]->Fill(fDiff + usPedShift);
              }
            }
          }
      }

      if (settings.guimode==2 && workerID == 0)
      {
        gui->GetProgressBar()->Increment(THREAD_NUMBER);
      }
    }

    std::lock_guard<std::mutex> lock(m);

    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d  = (*itD).devorn;
      auto sn = (*itD).devsn;
      Decoder->CloseDataFile(fd[d]);
      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        if ((*itD).ach.test(ch))
        {
          // time fit histo
          auto histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
                          [sn,ch] (const MON_PLOT& plot) {
                            return plot.devsn == sn && plot.ch == ch && plot.type == 6;
                          });
          ((TH1F*)histoPtr->plot)->Add(Histo[d][ch][workerID]);
          // timediff histo
          histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
                          [sn,ch] (const MON_PLOT& plot) {
                            return plot.devsn == sn && plot.ch == ch && plot.type == 5;
                          });
          ((TH1F*)histoPtr->plot)->Add(HistoDiff[d][ch][workerID]);
        }
      }
    }
  };
  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////////END OF LAMBDA/////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////

  printf("Mode:                  time spectrum\n");
  printf("Processing type:       using linear fit\n");
  printf("Trigger device:        %x\n", uiTrigDevice);
  printf("Trigger channel:       %d\n", uiTrigChannel);
  Watch->Reset();
  Watch->Start();
  workers.clear();
  for (auto id=0; id<THREAD_NUMBER; id++)
    workers.emplace_back(workDrawFitTimeSpectrum, id);

  for (auto &&worker : workers) worker.join();
  Watch->Stop();
  printf("The code took          %f seconds\n", Watch->RealTime());
  // if (iError) {
  //   if (settings.guimode!=2){ exit(iError); }
  //   else {;}
  // }

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch))
      {
        for (int t=0; t<THREAD_NUMBER; t++){
          delete Histo[d][ch][t];
          delete HistoDiff[d][ch][t];
        }
      }
    }
  }

  // FILE *fdLog = fopen(LOG_FILENAME,"at");

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d  = itD->devorn;
    auto sn = itD->devsn;
    DeviceFolder[d] = DataFolder->AddFolder(Form("ADC%02d_%x_time",d,itD->devsn),Form("ADC%02d_time",d));
    DeviceFolderDiff[d] = DataFolder->AddFolder(Form("ADC%02d_%x_diff_time",d,itD->devsn),Form("ADC%02d_time_difference [chXX-ch%02d]",d,TRIGGER_CHANNEL));
    for (int ch=0; ch<CHANNEL_NUMBER; ch++) {
      if (itD->ach.test(ch))
      {
        auto histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
        [sn,ch] (const MON_PLOT& histo) {
          return histo.devsn == sn && histo.ch == ch && histo.type == 6;
        });
        DeviceFolder[d]->Add((TH1F*)histoPtr->plot);

        histoPtr = std::find_if(vViewHisto.begin(), vViewHisto.end(),
        [sn,ch] (const MON_PLOT& histo) {
          return histo.devsn == sn && histo.ch == ch && histo.type == 5;
        });
        DeviceFolderDiff[d]->Add((TH1F*)histoPtr->plot);

        ///////////////////////////////////////////////////////////////
        // float fMean = 0;
        // float fStdDev = 0;
        // SpectrumTimeDiff[d][ch]->GetXaxis()->SetRangeUser(usPedShift-60,usPedShift+60);
        // fMean = SpectrumTimeDiff[d][ch]->GetMean();
        // SpectrumTimeDiff[d][ch]->GetXaxis()->SetRangeUser(fMean-18,fMean+18);
        // fMean = SpectrumTimeDiff[d][ch]->GetMean();
        // fStdDev = SpectrumTimeDiff[d][ch]->GetStdDev();
        //
        // SpectrumTimeDiff[d][ch]->Fit("gaus","Q","",fMean-6,fMean+6);
        // TF1 *g = (TF1*)SpectrumTimeDiff[d][ch]->GetListOfFunctions()->FindObject("gaus");
        // if (g != NULL)
        // {
        //   int chch = 0;
        //   sscanf(settings.file.c_str(),"/Users/fish/Downloads/ch1_ch%d.data",&chch);
        //   // if (
        //     //ch == settings.uiktrig_channel ||
        //     // ch == chch-1)
        //   {
        //     printf("TrigCH%02d CH%02d Mean: %7.3f Std.Dev.: %.3f FitMean: %7.3f [%7.3f] FitStd.Dev: %.3f\n",\
        //      settings.uiktrig_channel,ch,fMean,fStdDev,g->GetParameter(1),g->GetParameter(1)-usPedShift,g->GetParameter(2));
        //     // fprintf(fdLog,"%s CH%02d Mean: %.3f Std.Dev.: %.3f FitMean: %.3f FitStd.Dev: %.3f\n",\
        //       settings.file.c_str(), ch,fMean,fStdDev,g->GetParameter(1),g->GetParameter(2));
        //   }
        // }
        ///////////////////////////////////////////////////////////////
      }
    }
  }
  // fclose(fdLog);
  if (settings.guimode==2) gui->SetFolder(DataFolder);
}

//______________________________________________________________________________
void draw_avg_wave(Bool_t bKernelCreation)
{
  std::mutex m;
  iError=0;

  printf("\nMode:                  average waveform\n");
  vADCMeta = Decoder->GetADCMetaData();               // retrieving ADCS structure
  if (settings.guimode==2){
    settings.kernel_type = gui->GetAvgKernelType();
    if (settings.kernel_type==1)
    {
      switch (gui->GetAvgKernelCH())
      {
        case 0:
          settings.kernel_channels_preset = Form("%x_ch0.1.2.3.4.5.6.7.8.9", gui->GetAvgKernelSN());
        break;
        case 16:
          settings.kernel_channels_preset = Form("%x_ch16.17.18.19.20.21.22.23.24.25", gui->GetAvgKernelSN());
        break;
        case 32:
          settings.kernel_channels_preset = Form("%x_ch32.33.34.35.36.37.38.39.40.41", gui->GetAvgKernelSN());
        break;
        case 48:
          settings.kernel_channels_preset = Form("%x_ch48.49.50.51.52.53.54.55.56.57", gui->GetAvgKernelSN());
        break;
        default:
          settings.kernel_channels_preset = Form("%x_ch0.1.2.3.4.5.6.7.8.9", gui->GetAvgKernelSN());
        }
    }
    settings.kernel_creation = bKernelCreation;
  }

  DataFolder->Clear();

  DATA adata;
  std::vector<DATA> vAveData;			// average waveforms data
  vAveData.clear();

  const int ciDN = vADCMeta->size();                    // number of devices
  TFolder *DeviceFolder[ciDN];
  TFolder *DeviceFolderExp[ciDN];   // for signal exposition
  TMultiGraph *MGraph[ciDN][CHANNEL_NUMBER];
  UInt_t uiTotEvents[ciDN]; for (int i=0; i<ciDN; i++) uiTotEvents[i]=0;
  UInt_t uiEvtsMax = 0x0;
  UInt_t uiFirstEvent = 1;
  if (settings.guimode==2){
    uiFirstEvent = gui->GetAvgFirstEventNumber();
  }
  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;

    adata.clear();
    if (settings.guimode==2) uiTotEvents[d] = gui->GetAvgLastEventNumber();
    else{
      if (settings.event==1) uiTotEvents[d] = itD->ev;
      else uiTotEvents[d] = settings.event;
    }
    if (settings.event <= 0  || uiTotEvents[d] > itD->ev) uiTotEvents[d] = itD->ev;
    // printf("uiTotEvents %d\n", uiTotEvents[d]);
    adata.devsn = itD->devsn;
    adata.sn = itD->sn;
    vAveData.push_back(adata);
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch))
      {
        MGraph[d][ch] = new TMultiGraph();
        MGraph[d][ch]->SetNameTitle(
            Form("CH%02d",ch),\
            Form("Single waveforms CH%02d;Sample number, N;Amplitude, channels",ch));
        for (int s=0; s<itD->sn; s++)
        {
          vAveData.at(d).wf[ch].push_back(0);
        }
      }
    }
  }

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    if (uiTotEvents[d]>uiEvtsMax) uiEvtsMax=uiTotEvents[d];
  }
  if (settings.guimode==2){
    gui->GetProgressBar()->Reset();
    gui->GetProgressBar()->SetMax(uiEvtsMax);
  }
  if (settings.config.digi_phosphor_coef <= 0)
    settings.config.digi_phosphor_coef = DIGI_PHOSPHOR_COEF;

  printf("Readout limits:        %d-%d\n", uiFirstEvent, uiEvtsMax);
  printf("DigiPhosphor:          show each %dth waveform\n", settings.config.digi_phosphor_coef);
  // LAMBDA
  auto workAvgWave = [ciDN,uiFirstEvent,uiEvtsMax,&uiTotEvents,&vAveData,&MGraph,&m](UInt_t workerID)
  {
    std::vector<ADCS> meta(*vADCMeta);

    std::vector<DATA> vD, vAD;       // vector of data structures with different device id <data device id#1 ... data device id#N>
    std::vector<DATA> vData;

    FILE *fd[ciDN];
    DATA adata;

    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d = (*itD).devorn;

      adata.clear();
      adata.devsn = (*itD).devsn;
      adata.sn = (*itD).sn;
      vAD.push_back(adata);

      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        for (int s=0; s<(*itD).sn; s++) vAD.at(d).wf[ch].push_back(0);
      }
    }
    // printf("wid: %d vad: %zu\n", workerID,vAD.size());

    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d = (*itD).devorn;
      // printf("Dev#%02d uiTotEvents %d\n", d, uiTotEvents[d]);
      fd[d] = Decoder->OpenDataFile(Form("%s/%s",(*itD).dir,(*itD).file),0);  // second parameter disables indexation
      // printf("ThreadID: %d Dev: %d FD: %p Data: %p [%s]\n", workerID,d,fd[d],&vD,meta.at(d).file);
      if (!fd[d]) {
        iError=ERROR_DATAFILE_OPEN;
        return;
      }
    }

    for (auto e=uiFirstEvent; e<=uiEvtsMax; e+=THREAD_NUMBER)
    {
      vData.clear();
      for (auto itD=meta.begin(); itD!=meta.end(); itD++)
      {
        auto d = (*itD).devorn;
        if (e+workerID > uiTotEvents[d]) continue;
        // if ((e+workerID)%100 == 0) printf("Event#%d Device: %d ThreadID %d\n", e+workerID,d,workerID);
        switch (DEVICE_TYPE) {
          case DEVICE_ID_ADC64:
            iError = Decoder->ReadEventADC64(fd[d],e+workerID,&vD,VERBOSE_MODE,d, (*itD).devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          break;
          case DEVICE_ID_TQDC:
            iError = Decoder->ReadEventTQDC(fd[d],e+workerID,&vD,VERBOSE_MODE,d, (*itD).devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          break;
        }
        if (iError!=0)
        {
          for (int ch=0; ch<CHANNEL_NUMBER; ch++) vAD.at(d).evch[ch]++;
          continue;
        }

        vData.push_back(vD.at(0));

        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          if (itD->ach.test(ch))
          {
            auto graph = new TGraph();
            if (!vData.at(d).wf[ch].empty())
            {
              // if (Decoder->GetAmplitude((*itD).sn, (*itD).devsn, ch, &vData.at(d).wf[ch]) > 30000)
              if ((e+workerID)%settings.config.digi_phosphor_coef == 0)
              {
                for (int s=0; s<(*itD).sn; s++)
                 graph->SetPoint(s,s,Double_t(vData.at(d).wf[ch].at(s)));
                graph->SetNameTitle(Form("event%04d_%x_ch%02d",e+workerID,(*itD).devsn,ch),Form("event%04d",e+workerID));
                // graph->SetMarkerStyle(20);
                // graph->SetMarkerSize(0.4);
                graph->SetLineColor(workerID+1);
                MGraph[d][ch]->Add(graph,"l");
              }
            }
          }
        }

        if (CHANNEL_NUMBER-vD.at(0).chis.count() != (*itD).ach.count())
          printf("Achtung: Event#%04d is incomplete!", e+workerID);
      }
      if(!vData.empty())
        Decoder->GetAverageWave(&vData,&vAD);
      else
        continue;

      if (settings.guimode==2 && workerID == 0)
      {
        gui->GetProgressBar()->Increment(THREAD_NUMBER);
      }
    }

    std::lock_guard<std::mutex> lock(m);

    Decoder->GetAverageWave(&vAD, &vAveData,true);
    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d = (*itD).devorn;
      Decoder->CloseDataFile(fd[d]);
      // if (workerID==0)
        // Decoder->CopyMetaData(&vD, &vAveData, d); // to copy info about devices to the average wave struct
    }
  };
  // END OF LAMBDA
  Watch->Reset();
  Watch->Start();
  // Fill the "pool" with workers
  workers.clear();
  for (auto id=0; id<THREAD_NUMBER; id++)
    workers.emplace_back(workAvgWave, id);
  // Now join them
  for (auto &&worker : workers) worker.join();

  Watch->Stop();
  printf("The code took          %f seconds\n", Watch->RealTime());
  // if (iError) {
  //   if (settings.guimode!=2){ exit(iError); }
  //   else {;}
  // }

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    DeviceFolder[d] = DataFolder->AddFolder(Form("ADC%02dAveProfile_%x",d,itD->devsn),Form("ADC_%02d_%x",d,itD->devsn));
    uiTotEvents[d]-=uiFirstEvent-1;
    vAveData.at(d).ev = uiTotEvents[d]; // forward number of events for kernel creation
    // printf("Dev#%02d Total events: %d\n", d, uiTotEvents[d]);
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch))
        DeviceFolder[d]->Add(show_avg_waveform(d,ch,itD->sn,uiTotEvents[d],&vAveData));
    }
  }

  // for Digital phosphor graphs
  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    DeviceFolderExp[d] = DataFolder->AddFolder(Form("ADC%02dDigiPhosphor_%x",d,itD->devsn),Form("ADC%02d_%x",d,itD->devsn));
    uiTotEvents[d]-=uiFirstEvent-1;
    // printf("Dev#%02d Total events: %d\n", d, uiTotEvents[d]);
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch))
      {
        // #if ROOT_VERSION_CODE > ROOT_VERSION(6,10,8)
        //   MGraph[d][ch]->GetYaxis()->SetMaxDigits(3);
        // #endif
        // MGraph[d][ch]->GetXaxis()->SetRangeUser(0, itD->sn);
        DeviceFolderExp[d]->Add(MGraph[d][ch]);
      }
    }
  }

  switch (settings.kernel_type)
  {
    case 0:
      if (settings.kernel_creation == 1)
      {
        Decoder->CreateRoughKernel(&vAveData);
        bRKernelLoaded = false;
      }
      // if (bKernelTypeChanged)
        load_rkernel();
    break;
    case 1:
      if (settings.kernel_creation == 1)
      {
        Decoder->CreatePrecisedKernel(&vAveData, settings.kernel_channels_preset.c_str());
        bPKernelLoaded = false;
      }
      // if (bKernelTypeChanged)
        load_pkernel();
    break;
  }
  DataFolder->Add(KernelFolder);

  if (settings.guimode==2) gui->SetFolder(DataFolder);
}

//______________________________________________________________________________
void draw_single_wave()
{
  if (settings.guimode!=2)
    printf("Mode:                  single waveform\n");
  iError = 0;
  DataFolder->Clear();
  FILE   * fdDataFile     = NULL;
  DATA adata;
  std::vector<DATA> vData;        // vector of data structures with different device id <data device id#1 ... data device id#N>
  vData.clear();
  vDataPtr.clear();

  std::vector<ADCS> meta(*vADCMeta);

  const int ciDN = meta.size();                       // number of devices
  TFolder *DeviceFolder[ciDN];
  UInt_t uiEventNum[ciDN];
  for (int d=0; d<ciDN; d++) uiEventNum[d] = 0;
  if (settings.event <= 0) settings.event = 1;
  if (settings.guimode!=2)
    printf("Event number:          %d\n", settings.event);
  else
    printf("Event number:          %d\n", gui->GetSingleEventNumber());

  for (auto itD=meta.begin(); itD!=meta.end(); itD++)
  {
    auto d = (*itD).devorn;
    auto devsn = (*itD).devsn;
    fdDataFile    = Decoder->OpenDataFile(Form("%s/%s",(*itD).dir,(*itD).file),0);
    if (!fdDataFile) exit(-2);
    if (settings.guimode==2)
      uiEventNum[d] = gui->GetSingleEventNumber();
    else
      uiEventNum[d] = settings.event;
    if (uiEventNum[d]>(*itD).ev) uiEventNum[d] = (*itD).ev;

    switch (DEVICE_TYPE)
    {
      case DEVICE_ID_ADC64:
        iError = Decoder->ReadEventADC64(fdDataFile,uiEventNum[d],&vData,VERBOSE_MODE,d, (*itD).devsn,0);
      break;
      case DEVICE_ID_TQDC:
        iError = Decoder->ReadEventTQDC(fdDataFile,uiEventNum[d],&vData,VERBOSE_MODE,d, (*itD).devsn,0);
      break;
    }

    if (iError!=0) return;

    vDataPtr.push_back(vData.at(0));
    printf("Time for %X:\n",devsn);
    printf(" TAI:                  %llu ns [ %d sec + %d ns ] - unix time from board power cycle\n", \
            ULong64_t(vData.at(0).taisec*pow(10,9))+ULong64_t(vData.at(0).tainsec),vData.at(0).taisec,vData.at(0).tainsec);
    time_t tm = ULong64_t(vData.at(0).utime/1000);
    printf(" UTime:                %llu ms -> %s", ULong64_t(vData.at(0).utime),asctime(localtime(&tm)));
    if ((CHANNEL_NUMBER-vData.at(0).chis.count()) != ((*itD).ach.count()))
      printf("Event state:           incomplete [some channels might be absence]\n");
    // printf("Event state:           %d %d[some channels might be absence]\n", (*itD).ach.count(),vData.at(0).chis.count());

    Decoder->CloseDataFile(fdDataFile);
    if (iError) {
      printf("Error %d\n", iError);
      if (settings.guimode!=2){
        exit(iError);
      }
      else {return;}
    }

    DeviceFolder[d] = DataFolder->AddFolder(Form("ADC%02d_%x",d,vData.at(0).devsn),Form("Device_%02d",d));
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if ((*itD).ach.test(ch))
        DeviceFolder[d]->Add(show_single_waveform(d,ch,(*itD).sn,&vData));
      // if (d == 0 && ch == 0)
      // {
      //   Decoder->GetPeaks(vADCMeta->at(d).sn,&vData.at(d).wf[ch],settings.peakthr);
      //   vPeak = Decoder->GetPeaksData();
      //   sort(vPeak->begin(), vPeak->end(), WFPEAKS_CMP()); // sort by increasing of amplitude
      //   // for (int p = 0; p<vPeak->size(); p++) printf("Peak%d max: %f\n",p,float(vPeak->at(p).max) );
      // }
    }
  }
  if (settings.guimode==2) gui->SetFolder(DataFolder);
}

//______________________________________________________________________________
Int_t load_charge_gate()
{
  // printf("%s\n", gSystem->GetWorkingDirectory().c_str());
  iError=0;
  bool res;
  const int ciDN = vADCMeta->size();
  res = Decoder->LoadGateSettings(Form("%s", settings.config.gatefile),ciDN);
  if (res)
  {
    vGate = Decoder->GetGateData();
    if (vGate->empty())
    {
      printf("Achtung: The GATE vector is empty!\n");
      iError = ERROR_GATE_EMPTY;
      return iError;
    }
  }
  else
  {
    printf("Achtung: Can't load gate settings: %s [File error]!\n", Form("%s", settings.config.gatefile));
    iError = ERROR_GATE_FILE_OPEN;
    return iError;
  }

  if (vGate->size() < ciDN)
  {
    printf("Achtung: Gate file contains less devices than there were found!\n");
    iError = ERROR_GATE_MISSING_SN;
    return iError;
  }
  if (settings.guimode==2){
    gui->LoadCGateSettings(vGate);
  }
  return 0;
}

    ULong64_t tai_atPPS[16];  


//______________________________________________________________________________
void processall()
{
  std::mutex m;
  std::condition_variable cv;
  bool bDataStored = false;

  iError=0;
  DataFolder->Clear();
  const int ciDN = vADCMeta->size();  // number of devices
  TFolder *DeviceFolderCharge[ciDN];
  TFolder *DeviceFolderTime[ciDN];
  TFolder *DeviceFolderDiff[ciDN];
  TFolder *DeviceFolderAvg[ciDN];
  UInt_t uiTrigDevice = settings.uiktrig_device;
  UInt_t uiTrigChannel = settings.uiktrig_channel;
  UInt_t uiTotEvents[ciDN]; for (int i=0; i<ciDN; i++) uiTotEvents[i]=0;
  UInt_t uiEvtsMin = 0xffffffff;
  UInt_t uiFirstEvent = 1;
  UInt_t uiWorkerCounter = 0;
  UInt_t uiAwakenedWorkerCounter = 0;

  DATA adata;
  std::vector<DATA> vAveData;			// average waveforms data
  vAveData.clear();
  TH1F *Histo_ch[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];    // charge temporary histos
  TH1F *Histo_ft[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];    // time by fit
  TH1F *HistoDiff_ft[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];   // diff time by fit
  TH1F *Spectrum[ciDN][CHANNEL_NUMBER];
  TH1F *SpectrumFTime[ciDN][CHANNEL_NUMBER];
  TH1F *SpectrumTimeDiff[ciDN][CHANNEL_NUMBER];

  load_charge_gate();

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = (*itD).devorn;

    if (settings.event==1) uiTotEvents[d] = itD->ev;
    else uiTotEvents[d] = settings.event;
    if (settings.event <= 0  || uiTotEvents[d] > itD->ev) uiTotEvents[d] = itD->ev;

    adata.clear();
    adata.devsn = (*itD).devsn;
    vAveData.push_back(adata);
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      for (int s=0; s<(*itD).sn; s++) vAveData.at(d).wf[ch].push_back(0);
      // Add histos for charge spectra
      if ((*itD).ach.test(ch) && settings.guimode != 0)
      {
        Spectrum[d][ch] = new TH1F(Form("spectrum_adc%02d_ch%02d",d,ch),\
                                 Form("Spectrum [ADC%02d CH%02d]",d,ch),\
                                 NUMBER_OF_BINS,BIN_FIRST_CHARGE,BIN_LAST_CHARGE);
        SpectrumFTime[d][ch] = new TH1F(Form("ftime_adc%02d_ch%02d",d,ch),\
                                 Form("Time spectrum [ADC%02d CH%02d]",d,ch),\
                                 NUMBER_OF_TIME_BINS*TIME_BIN_FACTOR,0,NUMBER_OF_TIME_BINS);
        SpectrumTimeDiff[d][ch] = new TH1F(Form("dtime_adc%02d_ch%02d",d,ch),\
                                 Form("Time spectrum [ADC%02d CH%02d TriggerCH%02d]",d,ch,settings.uiktrig_channel),\
                                 NUMBER_OF_TIMEDIFF_BINS*TIMEDIFF_BIN_FACTOR,TIMEDIFF_BIN_FIRST,TIMEDIFF_BIN_LAST);
        SpectrumFTime[d][ch]->GetXaxis()->SetTitle("Time, ns");
        SpectrumFTime[d][ch]->GetYaxis()->SetTitle("Number of counts, N");
        SpectrumTimeDiff[d][ch]->GetXaxis()->SetTitle("Time, ns");
        SpectrumTimeDiff[d][ch]->GetYaxis()->SetTitle("Number of counts, N");
        for (int t=0; t<THREAD_NUMBER; t++)
        {
          Histo_ch[d][ch][t] = new TH1F(Form("shisto_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                  Form("shisto_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                  NUMBER_OF_BINS,BIN_FIRST_CHARGE,BIN_LAST_CHARGE);
          Histo_ft[d][ch][t] = new TH1F(Form("thisto_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                        Form("thisto [DEV%02d CH%02d worker%02d]",d,ch,t),\
                                        NUMBER_OF_TIME_BINS*TIME_BIN_FACTOR,0,NUMBER_OF_TIME_BINS);
          HistoDiff_ft[d][ch][t] = new TH1F(Form("thisto_diff_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                        Form("thisto diff [DEV%02d CH%02d worker%02d]",d,ch,t),\
                                        NUMBER_OF_TIMEDIFF_BINS*TIMEDIFF_BIN_FACTOR,TIMEDIFF_BIN_FIRST,TIMEDIFF_BIN_LAST);

        }
      } // end if
    } // end of channel loop
  }

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = itD->devorn;
    if (uiTotEvents[d]<uiEvtsMin) uiEvtsMin=uiTotEvents[d];
  }

  Logger->SetFileName(Form("rlog_%s",vADCMeta->at(0).file+9));    // ROOT logger
  if (settings.rwf_on)
    rLoggerWF->SetFileName(Form("rwf_%s",vADCMeta->at(0).file+9));    // ROOT logger
  if (settings.write_data_log)
    Decoder->LogDataInit();       // ASCII logger

  // LAMBDA ///////////////////////////////////////////////////////////////////
  auto workProcessall = [ciDN,uiTrigDevice,uiTrigChannel,uiFirstEvent,uiEvtsMin,&uiTotEvents,&vAveData,&Histo_ch,&Histo_ft,&HistoDiff_ft,&Spectrum,
                      &SpectrumFTime, &SpectrumTimeDiff,&uiWorkerCounter,&uiAwakenedWorkerCounter,&m,&cv,&bDataStored](UInt_t workerID)
  {
    std::vector<ADCS> meta(*vADCMeta);
    INTEGRAL *integral = 0;
    iError=0;
    RLOG rLog;
    RWAVE rLogWave_struct;
    rLog.InitData();
    rLogWave_struct.InitData();
    sprintf(rLog.runtag, "%s", Decoder->GetRunTag());
    Float_t fTimeMap[ciDN][CHANNEL_NUMBER];
    Float_t fChargeMap[ciDN][CHANNEL_NUMBER];
    Float_t fChargeShiftedMap[ciDN][CHANNEL_NUMBER];
    Float_t fAmpMap[ciDN][CHANNEL_NUMBER];
    uint32_t uiWorkerJobCounter = 0;
    // std::vector<TGraph*> vRTGraph;     // vector of tgraph pointers which must be deleted
    // vRTGraph.clear();
    std::vector<TH1S*> vRTH1S;     // vector of tgraph pointers which must be deleted
    vRTH1S.clear();

    std::vector<DATA> vD, vAD;       // vector of data structures with different device id <data device id#1 ... data device id#N>
    std::vector<DATA> vData;
    DATA adata;

    FILE *fd[ciDN];
    float fDiff = 0;
    float fTime = 0;
    float fTrigTime = 0;
    Float_t fA = 0;   // amplitude of signal - minimum of waveform
    Float_t fAT = 0;   // amplitude of signal - minimum of waveform for Trigger
    

    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d = (*itD).devorn;
     // tai_atPPS[d]=0;
      adata.clear();
      adata.devsn = (*itD).devsn;
      adata.sn = (*itD).sn;
      vAD.push_back(adata);

      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        for (int s=0; s<(*itD).sn; s++) vAD.at(d).wf[ch].push_back(0);
      }

      fd[d] = Decoder->OpenDataFile(Form("%s/%s",(*itD).dir,(*itD).file),0);  // second parameter disables indexation
      if (!fd[d]) {
        iError=ERROR_DATAFILE_OPEN;
        return;
      }
    }

    for (auto e=uiFirstEvent;  e<=uiEvtsMin; e+=THREAD_NUMBER)
    {
      // progress
      if (workerID==0 && (e+workerID-1)%1000==0 && (e+workerID-1)!=0 && settings.guimode!=2)
        printf("%d events processed...\n", e+workerID-1);

      vData.clear();
      fTrigTime = 0;
      if (e+workerID > uiEvtsMin) break;

      for (auto itD=meta.begin(); itD!=meta.end(); itD++)
      {
        auto d = (*itD).devorn;
        auto devsn = (*itD).devsn;
        switch (DEVICE_TYPE)
        {
          case DEVICE_ID_ADC64:
            iError = Decoder->ReadEventADC64(fd[d],e+workerID,&vD,VERBOSE_MODE,d, devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          break;
          case DEVICE_ID_TQDC:
            iError = Decoder->ReadEventTQDC(fd[d],e+workerID,&vD,VERBOSE_MODE,d, devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          break;
        }

        if (iError!=0)
        {

          for (int ch=0; ch<CHANNEL_NUMBER; ch++) vAD.at(d).evch[ch]++;
          continue;
        }

        vData.push_back(vD.at(0));


        if ( (devsn==uiTrigDevice) && ((*itD).ach.test(uiTrigChannel)) )
        {
          if (!FIT_DISCRETISATION_IMPOVEMENT)
          {
            // not improved
            fTrigTime = Decoder->GetTimeFitCrossPoint(devsn, uiTrigChannel, (*itD).sn, \
                                                      &vData.at(d).wf[uiTrigChannel]);
          }
          else
          {
            fTrigTime = Decoder->GetTimeFitCrossPointImproved(devsn, uiTrigChannel,\
                                &vD.at(0).wf[uiTrigChannel], &fAT);
          }
        }
      }

      if(vData.empty()) continue;

      // Get average waveform for each event
      if (settings.guimode != 0)
        Decoder->GetAverageWave(&vData,&vAD);

      for (auto itD=meta.begin(); itD!=meta.end(); itD++)
      {
        auto d     = (*itD).devorn;
        auto devsn = (*itD).devsn;

        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          fTimeMap[d][ch]   = 0;
          fChargeMap[d][ch] = 0;
          fAmpMap[d][ch]    = 0;
          if ((*itD).ach.test(ch))
          {
            integral = Decoder->GetIntegral(devsn, ch, &vData.at(d).wf[ch],workerID);
            if (bPedSubtr)
            {
              fChargeMap[d][ch] = integral->cint;
              fChargeShiftedMap[d][ch] = integral->cint_sh;
              fAmpMap[d][ch] = integral->amp;
            }
            else
            {
              fChargeMap[d][ch] = integral->csig;
              fChargeShiftedMap[d][ch] = integral->csig_sh;
              fAmpMap[d][ch] = integral->amp_sh;
            }

            if (settings.guimode != 0)
              Histo_ch[d][ch][workerID]->Fill(fChargeMap[d][ch]+usPedShift);

            // Get time spectra by fit
            fTime = 0;
            if (!FIT_DISCRETISATION_IMPOVEMENT)
            {
              // not improved
              fTime     = Decoder->GetTimeFitCrossPoint(devsn, ch, (*itD).sn, \
                                                        &vData.at(d).wf[ch]);
            }
            else
            {
              // improved
              fTime     = Decoder->GetTimeFitCrossPointImproved(devsn, ch, \
                                                        &vData.at(d).wf[ch], &fA);
            }
            fTimeMap[d][ch] = fTime;
            fDiff = fTime - fTrigTime;
            if (settings.guimode != 0)
            {
              if (!FIT_DISCRETISATION_IMPOVEMENT)
              {
                Histo_ft[d][ch][workerID]->Fill(fTime*KERNEL_NUMBER);
                HistoDiff_ft[d][ch][workerID]->Fill(fDiff*KERNEL_NUMBER + usPedShift);
              }
              else
              {
                Histo_ft[d][ch][workerID]->Fill(fTime);
                HistoDiff_ft[d][ch][workerID]->Fill(fDiff + usPedShift);
              }
            } // end if mode
          } // end if (meta->at(d).ach.test(ch))
        } // end of for channel
        
        if( fAmpMap[d][32] > 1500) //PPS detected ***************************************************************************PPS
              {
              tai_atPPS[d] = ULong64_t(((vData.at(d).taisec)*pow(10,9) + vData.at(d).tainsec)*5/8);
              printf("PPS detected.d=%d tai at PPS %lld\n",d,tai_atPPS[d]); 
              }


        // Fill RLOG ROOT logger
        rLog.utime_ms = ULong64_t(vData.at(d).utime);
        rLog.tai_ns    = ULong64_t(((vData.at(d).taisec)*pow(10,9) + vData.at(d).tainsec)*5/8) ;
       // printf("tai_atPPS[%d] = %lld, tai_ns=%lld; ",d,tai_atPPS[d],rLog.tai_ns);
        if(rLog.tai_ns < tai_atPPS[d]) rLog.tai_ns = rLog.tai_ns +1000000000 ;
        rLog.tai_ns    = rLog.tai_ns - tai_atPPS[d]  ;
       // printf("Corrected tai_ns=%lld\n",rLog.tai_ns);
        rLog.taisec   = ULong64_t((vData.at(d).taisec));
        rLog.tainsec  = ULong64_t((vData.at(d).tainsec));
        rLog.sn       = devsn;
        rLog.cycle    = workerID;
        rLog.event    = e+workerID;

        // printf("Event#%4d taisec: %10llu taiNsec: %10llu\n",rLog.event, rLog.taisec,rLog.tainsec);

        memcpy(rLog.data[0], fAmpMap[d],    CHANNEL_NUMBER*sizeof(Float_t));
        memcpy(rLog.data[1], fChargeMap[d], CHANNEL_NUMBER*sizeof(Float_t));
        memcpy(rLog.data[2], fChargeShiftedMap[d], CHANNEL_NUMBER*sizeof(Float_t));
        memcpy(rLog.data[3], fTimeMap[d],   CHANNEL_NUMBER*sizeof(Float_t));
        memcpy(rLog.wfts, vData.at(d).wfts, CHANNEL_NUMBER*sizeof(ULong64_t));
        Logger->Fill(workerID, rLog);

        // Fill Waveforms into the ROOT file //

        if (settings.rwf_on)
        {
          rLogWave_struct.event    = e+workerID;
          rLogWave_struct.sn       = devsn;
          rLogWave_struct.utime_ms = ULong64_t(vData.at(d).utime);

        rLogWave_struct.tai_ns    = ULong64_t(((vData.at(d).taisec)*pow(10,9) + vData.at(d).tainsec)*5/8) ;
        if(rLogWave_struct.tai_ns < tai_atPPS[d]) rLogWave_struct.tai_ns = rLogWave_struct.tai_ns +1000000000 ;
        rLogWave_struct.tai_ns    = rLogWave_struct.tai_ns - tai_atPPS[d]  ;
 

     //     rLogWave_struct.tai_ns    = ULong64_t(((vData.at(d).taisec)*pow(10,9) + vData.at(d).tainsec)*5/8 - tai_atPPS[d] ) ;
          for (int ch=0; ch<CHANNEL_NUMBER; ch++)
          {
            if ((*itD).ach.test(ch))
            {
              rLogWave_struct.ch = ch;

              // // TGraph implementation --
              // auto rgraph = new TGraph();
              // vRTGraph.push_back(rgraph);
              // rgraph->SetNameTitle(Form("wf_dev%x_ch%d_ev%d",d,ch,rLogWave_struct.event),\
              //                      Form("Single waveform [SN:%x ch%d event:%d utime: %lld];Sample number, N;Voltage, adc channels",\
              //                      d,ch,rLogWave_struct.event,rLogWave_struct.utime_ms));
              // for (int s=0; s<vData.at(d).sn; s++)
              // {
              //   rgraph->SetPoint(s,s,vData.at(d).wf[ch].at(s));
              // }
              // rLogWave_struct.graph_ptr = rgraph;
              // // ------------------------

              // TH1S implementation --
              auto rhisto1s = new TH1S(Form("wf_dev%x_ch%d_ev%d",d,ch,rLogWave_struct.event),\
                                       Form("Single waveform [SN:%x ch%d event:%d utime_ms: %lld tai_ns]",\
                                            d,ch,rLogWave_struct.event,rLogWave_struct.utime_ms, rLogWave_struct.tai_ns),vData.at(d).sn,0,vData.at(d).sn);
              vRTH1S.push_back(rhisto1s);
              for (int s=0; s<vData.at(d).sn; s++)
              {
                rhisto1s->SetBinContent(s,vData.at(d).wf[ch].at(s));
              }
              rLogWave_struct.th1s_ptr = rhisto1s;
              // ------------------------

              // filling waveforms to ROOT tree
              rLoggerWF->Fill(workerID, rLogWave_struct);
            }
          }
        }
        /////////////////////////////

        // and optionally ascii data log
        if (settings.write_data_log)
          Decoder->LogDataFill(workerID, e+workerID, devsn, fAmpMap[d], fChargeMap[d], fTimeMap[d], vData.at(d).wf);
      } // end of for devices

      // start to dump data to the disk when the limit is reached//
      if (uiWorkerJobCounter >= RLOGGER_STORE_RATE)
      {
        uiWorkerJobCounter = 0;
        std::unique_lock<std::mutex> lock(m);
        uiWorkerCounter++;

        if (uiWorkerCounter==THREAD_NUMBER)
        {
          // intermediate writting RLOG & RWAVE ROOT file
          Logger->Write();      // - dump RLOG - single call to write after the last thread job is done
          if (settings.rwf_on)
            rLoggerWF->Write(); // - dump waveforms

          // reset counters
          bDataStored = true;
          cv.notify_all();
          uiWorkerCounter = 0;
          uiAwakenedWorkerCounter = 0;
        }
        cv.wait(lock,[&]{return bDataStored;});   // wait for data dumping
        uiAwakenedWorkerCounter++;                // count how many threads are awakened
        if (uiAwakenedWorkerCounter!=THREAD_NUMBER)
        {
          bDataStored = true;                     // resend notification if not all threads are awakened
          cv.notify_all();
          // free memory
          if (settings.rwf_on && settings.guimode == 0)
            gDirectory->Close(); // cleanup th1s rwf histos
        }
        else
        {
          cv.notify_all();                        // the last call
          bDataStored = false;                    // reset flag
          uiAwakenedWorkerCounter = 0;
        }
        // printf("workerID: %d Lock flag: %d Awakened: %d threads\n", workerID, bDataStored,uiAwakenedWorkerCounter);
      }
      ///////////////////////////////////
      uiWorkerJobCounter++;
    } // end of for events

    std::lock_guard<std::mutex> lock(m);

    // Get average for each thread
    if (settings.guimode != 0)
      Decoder->GetAverageWave(&vAD, &vAveData,true);
    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d = (*itD).devorn;
      // if (workerID==0)
      //   Decoder->CopyMetaData(&vData, &vAveData, d); // to copy info about devices to the average wave struct
      // Integral summation
      if (settings.guimode != 0)
      {
        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          if ((*itD).ach.test(ch))
          {
            Spectrum[d][ch]->Add(Histo_ch[d][ch][workerID]);
            SpectrumFTime[d][ch]->Add(Histo_ft[d][ch][workerID]);
            SpectrumTimeDiff[d][ch]->Add(HistoDiff_ft[d][ch][workerID]);
          }
        }
      }
      Decoder->CloseDataFile(fd[d]);
    } // end of for d
  };
  // END OF LAMBDA ////////////////////////////////////////////////////////////

  printf("Mode:                  processall\n");
  printf("Processing type:       avg, charge, time using linear fit\n");
  printf("Trigger device:        %x\n", uiTrigDevice);
  printf("Trigger channel:       %d\n", uiTrigChannel);
  printf("Readout limits:        %d-%d\n", uiFirstEvent, uiEvtsMin);
  Watch->Reset();
  Watch->Start();

  workers.clear();
  for (auto id=0; id<THREAD_NUMBER; id++)
    workers.emplace_back(workProcessall, id);
  for (auto &&worker : workers) worker.join();
  Watch->Stop();
  printf("The code took          %f seconds\n", Watch->RealTime());

  // if (iError) {
  //   // if (settings.guimode!=2){ exit(iError); }
  //   // else {;}
  // }

  TTimeStamp tStamp;

  // Decoder->LogWrite("log");
  // writting RLOG
  Logger->Write(true);      // true - means close file
  // writting waveforms into the ROOT file
  if (settings.rwf_on)
    rLoggerWF->Write(true); // true - means close file
  if (settings.write_data_log)
    Decoder->LogDataWrite("ascii");

  if (settings.guimode != 0)
  {
    for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
    {
      auto d = itD->devorn;
      auto devsn = itD->devsn;
      DeviceFolderAvg[d] = DataFolder->AddFolder(Form("ADC%02d_%x_avg",d,devsn),Form("ADC_%02d_%x_avg",d,devsn));
      DeviceFolderCharge[d] = DataFolder->AddFolder(Form("ADC%02d_%x_charge",d,devsn),Form("ADC%02d_%x_charge",d,devsn));
      DeviceFolderTime[d] = DataFolder->AddFolder(Form("ADC%02d_%x_time",d,devsn),Form("ADC%02d_time",d));
      DeviceFolderDiff[d] = DataFolder->AddFolder(Form("ADC%02d_%x_diff_time",d,devsn),\
                                      Form("ADC%02d_time_difference [chXX-ch%02d]",d,TRIGGER_CHANNEL));
      uiTotEvents[d]-=uiFirstEvent-1;
      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        if (itD->ach.test(ch))
        {
          DeviceFolderAvg[d]->Add(show_avg_waveform(d,ch,itD->sn,uiTotEvents[d],&vAveData));
          DeviceFolderCharge[d]->Add(Spectrum[d][ch]);
          DeviceFolderTime[d]->Add(SpectrumFTime[d][ch]);
          DeviceFolderDiff[d]->Add(SpectrumTimeDiff[d][ch]);
        }
      }
    }

    for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
    {
      auto d = (*itD).devorn;
      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        if ((*itD).ach.test(ch))
        {
          for (int t=0; t<THREAD_NUMBER; t++){
            delete Histo_ch[d][ch][t];
            delete Histo_ft[d][ch][t];
            delete HistoDiff_ft[d][ch][t];
          }
        }
      }
    }
  }
}

//______________________________________________________________________________
bool load_pkernel(Bool_t showKernel)
{
  if (bPKernelLoaded) return true;
  iError = 0;

  load_pgate();  // Get vPKGate loaded

  bool res=0;
  res = Decoder->LoadPrecisedKernel(Form("%s/%s", settings.workdir.c_str(), KERNEL_P_FILENAME),vPKGate);
  if (res)
  {
    vPKernel = Decoder->GetPrecisedKernelData();
    Decoder->NormalizePrecisedKernel(vPKernel);
  }
  else
  {
    iError = ERROR_KERNEL_LOAD;
    printf("Achtung: Can't load the kernel: %s [File or Tree access error]\n", Form("%s/%s", settings.workdir.c_str(), KERNEL_P_FILENAME));
    return false;
  }

  int iNKernels = 0;
  if (res == true && vPKernel->empty() == false)
  {
    KernelFolder = DataFolder->AddFolder("Kernels","Kernels");
    for (int k=0; k<KERNEL_MAXNUM_IN_FILE; k++)
    {
      if (vPKernel->at(KERNEL_DEFAULT_ADC_FOR_SPLITTER).kid[k] != -1) // vPKernel->at(0).kid[k] - is device order number = 0 because of this option is used only for 1st adc
      {
        iNKernels++;
        KernelFolder->Add( show_pkernel(k,vADCMeta->at(KERNEL_DEFAULT_ADC_FOR_SPLITTER).sn*KERNEL_NUMBER,vPKernel,\
                                                      vPKGate->at(KERNEL_DEFAULT_ADC_FOR_SPLITTER).start[k]*KERNEL_NUMBER,\
                                                      vPKGate->at(KERNEL_DEFAULT_ADC_FOR_SPLITTER).end[k]*KERNEL_NUMBER) );
      }
    }
  }

  if (settings.uiktrig_channel < 0 || settings.uiktrig_channel > CHANNEL_NUMBER-1)
  {
    iError = ERROR_KERNEL_WRONG_TRIG_CH;
    printf("Achtung: The trigger channel [%d] is out of range [0-63]!\n",settings.uiktrig_channel);
    return false;
  }

  auto kids = vPKernel->at(0).kid;
  bool dataKIDFound = false;
  bool trigKIDFound = false;
  for (int nk=0; nk<KERNEL_ID_NUMBER; nk++)
  {
    if (!dataKIDFound && kids[nk] == settings.uikids_in_use[0]) dataKIDFound = true;
    if (!trigKIDFound && kids[nk] == settings.uikids_in_use[1]) trigKIDFound = true;
  }
  if (!dataKIDFound && !trigKIDFound)
  {
    printf("Achtung: Some pointed kernel ids don't exist in the kernel file [%s].\n \
      Please compare the preset line: [%s] and used kernel!\n",Form("%s/%s", settings.workdir.c_str(),\
      KERNEL_P_FILENAME), settings.kernel_ids_in_use.c_str());
    return false;
  }

  if (settings.kernel_creation==1) showKernel = true;
  if (showKernel)
  {
    if (settings.guimode==2)
    {
      DataFolder->Add(KernelFolder);
      gui->SetPKernelIDs(vPKernel->at(0).kid); // updates kernel id lists
      gui->LoadKGateSettings(vPKGate);
      gui->SetFolder(DataFolder);
    }
  }
  bPKernelLoaded = true;
  bRKernelLoaded = false;
  return bPKernelLoaded;
}

//______________________________________________________________________________
bool load_rkernel()
{
  // Rough kernel
  if (bRKernelLoaded) return true;
  load_rgate();
  iError = 0;
  bool res = Decoder->LoadRoughKernel(Form("%s/%s", settings.workdir.c_str(), KERNEL_R_FILENAME));
  if (res)
  {
    vRKernel = Decoder->GetRoughKernelData();
    Decoder->NormalizeRoughKernel(vRKernel);
  }
  else{
    iError = ERROR_KERNEL_LOAD;
    printf("Achtung: Can't load the kernel: %s [File or Tree access error]\n", Form("%s/%s_SN.root", settings.workdir.c_str(), KERNEL_R_FILENAME));
    return false;
  }

  if (res == true && vRKernel->empty() == false)
  {
    for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
    {
      auto d     = itD->devorn;
      auto devsn = itD->devsn;
      KernelFolder = DataFolder->AddFolder(Form("Kernels_%x", devsn),Form("Kernels_%x", devsn));
      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        if (vRKernel->at(d).chstate[ch])
        {
          auto rgatePos = std::find_if(vRKGate->begin(), vRKGate->end(),
                               [d, devsn, ch] (const GATE& rgate) {
                                  return rgate.devsn == devsn && rgate.ch == ch;
                               });
          KernelFolder->Add( show_rkernel(d,ch,(*itD).sn,vRKernel,rgatePos->sig_start,rgatePos->sig_end) );
        }
      }
    }
  }
  bRKernelLoaded = true;
  bPKernelLoaded = false;
  return bRKernelLoaded;
}

//______________________________________________________________________________
bool load_pgate()
{
  // Loading precised kernel gate configuration file //////////////////////////
  bool res=0;
  res = Decoder->LoadPKernelGateSettings(Form("%s/%s", settings.workdir.c_str(), KERNEL_GATE_FILENAME));
  if (res)
  {
    vPKGate = Decoder->GetPKernelGateData();
    if (vPKGate->empty())
    {
      printf("Achtung: The precised kernel gate vector is empty!\n");
      return false;
    }
  }
  else
    printf("Achtung: Can't load precised kernel gate settings: %s [File error]!\n", Form("%s/%s", settings.workdir.c_str(), KERNEL_GATE_FILENAME));
  return true;
}

//______________________________________________________________________________
bool load_rgate()
{
  // Loading precised kernel gate configuration file //////////////////////////
  bool res=0;
  res = Decoder->LoadRKernelGateSettings(Form("%s/%s", settings.workdir.c_str(), KERNEL_RGATE_FILENAME));
  if (res)
  {
    vRKGate = Decoder->GetRKernelGateData();
    if (vRKGate->empty())
    {
      printf("Achtung: The rough kernel gate vector is empty!\n");
      return false;
    }
  }
  else
    printf("Achtung: Can't load rough kernel gate settings: %s [File error]!\n", Form("%s/%s", settings.workdir.c_str(), KERNEL_RGATE_FILENAME));
  return true;
}

//______________________________________________________________________________
TGraph* show_single_waveform(uint8_t devorn, uint8_t ch, uint16_t sn, std::vector<DATA> *vData)
{
  TGraph * GraphSingleWF;
  if (vADCMeta->at(devorn).ach.test(ch))
  {
    Double_t wave = 0;
    Double_t time = 0;
    GraphSingleWF = new TGraph();
    switch (DEVICE_TYPE)
    {
      case DEVICE_ID_ADC64:
        GraphSingleWF->SetNameTitle(Form("CH%02d",ch),Form("Single waveform CH%02d [Event#%d];Sample number, N;Amplitude, channels",ch,vData->at(0).ev));
        for (int s=0; s<sn; s++)
        {
          if (vData->at(0).wf[ch].empty())
            wave = 0;
          else
            wave = vData->at(0).wf[ch].at(s);
          time = s;
          if (SCALE_REPRESENTATION)
          {
            wave *= ADC64_BITS_TO_MV;
            time *= TIME_DISCRETIZATION;
          }
          GraphSingleWF->SetPoint(s,time,wave);
          // printf("#%d Wave: %d\n",s,wave);
        }
      break;
      case DEVICE_ID_TQDC:
        GraphSingleWF->SetNameTitle(Form("CH%02d",ch),Form("Single waveform CH%02d [Event#%d];Sample number, N;Amplitude, channels",ch,vData->at(0).ev));
        for (int s=0; s<sn; s++)
        {
          // wave = vData->at(0).wf[ch].at(s);
          wave = vData->at(0).wf[ch].at(s)*TQDC_BITS_TO_MV; // in mV
          GraphSingleWF->SetPoint(s,s,wave);
          // printf("#%d Wave: %d\n",s,wave);
        }
      break;
    }

    if (SCALE_REPRESENTATION)
    {
      GraphSingleWF->GetYaxis()->SetTitle("Amplitude, mV");
      GraphSingleWF->GetXaxis()->SetTitle("Time, ns");
    }
    GraphSingleWF->SetMarkerSize(0.4);
    GraphSingleWF->SetMarkerStyle(20);
    GraphSingleWF->SetMarkerColor(kBlue);
    GraphSingleWF->SetLineColor(kBlue);
    if (SCALE_REPRESENTATION)
      GraphSingleWF->GetXaxis()->SetRangeUser(0, sn*TIME_DISCRETIZATION);
    else
      GraphSingleWF->GetXaxis()->SetRangeUser(0, sn);
    #if ROOT_VERSION_CODE > ROOT_VERSION(6,10,8)
      GraphSingleWF->GetYaxis()->SetMaxDigits(3);
    #endif

  }
  else
  {
    printf("The channel #%02d is disabled!\n", ch);
    return NULL;
  }
  return GraphSingleWF;
}

//______________________________________________________________________________
TGraph* show_avg_waveform(uint8_t devorn, uint8_t ch, uint16_t sn, \
                    uint32_t nevents, std::vector<DATA> *vAveData)
{
  Double_t wave = 0;
  Double_t time = 0;
  nevents -= vAveData->at(devorn).evch[ch];
  // nevents = vAveData->at(devorn).evch[ch];
  TGraph *GraphAverageWF;
  GraphAverageWF = new TGraph();
  GraphAverageWF->SetNameTitle(Form("CH%02d",ch),
    Form("Average waveform CH%02d [%d events];Sample number, N;Amplitude, channels",ch,nevents));

  switch (DEVICE_TYPE)
  {
    case DEVICE_ID_ADC64:
      // printf("ch: %02d events: %d [bad: %d]\n",ch, nevents,vAveData->at(devorn).evch[ch]);
      for (int s=0; s<sn; s++)
      {
        wave = double(vAveData->at(devorn).wf[ch].at(s))/double(nevents);
        time = s;
        if (SCALE_REPRESENTATION)
        {
            wave *= ADC64_BITS_TO_MV;
            time *= TIME_DISCRETIZATION;
        }
        GraphAverageWF->SetPoint(s,time,wave);
        // if (ch==0) printf("%d %.3f\n", s, wave);
      }
    break;
    case DEVICE_ID_TQDC:
      for (int s=0; s<sn; s++)
      {
        // wave = double(vAveData->at(devorn).wf[ch].at(s))/double(nevents);
        wave = double(vAveData->at(devorn).wf[ch].at(s))*TQDC_BITS_TO_MV/double(nevents);
        GraphAverageWF->SetPoint(s,s,wave);
        // if (ch==0) printf("%d %.3f\n", s, wave);
      }
    break;
  }

  if (SCALE_REPRESENTATION)
  {
    GraphAverageWF->GetYaxis()->SetTitle("Amplitude, mV");
    GraphAverageWF->GetXaxis()->SetTitle("Time, ns");
  }
  GraphAverageWF->SetMarkerSize(0.4);
  GraphAverageWF->SetMarkerStyle(20);
  GraphAverageWF->SetMarkerColor(kGreen);
  GraphAverageWF->SetLineColor(kGreen);
  if (SCALE_REPRESENTATION)
    GraphAverageWF->GetXaxis()->SetRangeUser(0, sn*TIME_DISCRETIZATION);
  else
    GraphAverageWF->GetXaxis()->SetRangeUser(0, sn);
  #if ROOT_VERSION_CODE > ROOT_VERSION(6,10,8)
    GraphAverageWF->GetYaxis()->SetMaxDigits(3);
  #endif

  return GraphAverageWF;
}

//______________________________________________________________________________
TGraph* show_rkernel(uint8_t devorn, uint8_t ch, uint16_t sn, \
                                 std::vector<KERNEL> *vKernel,\
                                 int start, int end)
{
  TGraph *GraphKernel;
  GraphKernel = new TGraph();
  GraphKernel->SetNameTitle(Form("kernel_%x_ch%02d",vKernel->at(devorn).devsn,ch),Form("Kernel [%x CH%02d]",vKernel->at(devorn).devsn,ch));

  for (int s=start; s<=end; s++)
  {
    GraphKernel->SetPoint(s-start,s,vKernel->at(devorn).wf[ch].at(s-start));
    // printf("s: %d %d wave: %d\n", s,s-start,vKernel->at(devorn).wf[ch].at(s-start));
  }

  GraphKernel->SetMarkerSize(0.4);
  GraphKernel->SetMarkerStyle(22);
  GraphKernel->SetMarkerColor(kMagenta+3);
  GraphKernel->SetLineColor(kMagenta+3);
  GraphKernel->GetXaxis()->SetTitle("Sample number, N");

  return GraphKernel;
}

//______________________________________________________________________________
TGraph * show_pkernel(UInt_t kind, uint16_t sn, \
                                 std::vector<KERNEL_PRECISED> *vKernel,\
                                 int start, int end)
{
  TGraph *GraphKernel;
  GraphKernel = new TGraph();
  GraphKernel->SetNameTitle(Form("Kernel_id%d",vKernel->at(0).kid[kind]),Form("Kernel ID%02d",vKernel->at(0).kid[kind]));

  for (int s=start; s<=end; s++)
  {
    GraphKernel->SetPoint(s-start,s,vKernel->at(0).wf[kind].at(s-start));
    // printf("s: %d %d wave: %d\n", s,s-start,vKernel->at(0).wf[kind].at(s-start));
  }

  GraphKernel->SetMarkerSize(0.4);
  GraphKernel->SetMarkerStyle(21);
  GraphKernel->SetLineWidth(2);
  GraphKernel->SetMarkerColor(kMagenta+4);
  GraphKernel->SetLineColor(kMagenta+4);
  GraphKernel->GetXaxis()->SetTitle("Time, ns");

  return GraphKernel;
}

////////////////////////Only for Monitoring/////////////////////////////////////
//______________________________________________________________________________
std::string exec(const char* cmd)
{
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

//______________________________________________________________________________
void clear_plots()
{
  // bMonOnline = false;
  for (auto itG=vMonGraph.begin(); itG!=vMonGraph.end(); itG++)
  {
    ((TGraph*)itG->plot)->Set(0);
  }
  for (auto itH=vMonHisto.begin(); itH!=vMonHisto.end(); itH++)
  {
    ((TH1F*)itH->plot)->Reset();
  }
  if (bMonOnline) start_monitoring();
}

//______________________________________________________________________________
Int_t get_last_datafile(char *fname)
{
  iError = 0;
  const char *path = settings.monitor_datadir.c_str();
  struct stat info;
  if ( stat(path, &info) != 0 ) {
    printf("Achtung: Directory %s does not exist!\n", path);
    iError = ERROR_DATADIR_NOT_FOUND;
    return iError;
  }
  std::string last_datafile_name = exec(
    Form("ls -1tr %s | grep .data | tail -1", path));
  int n = last_datafile_name.size();
  if ( n == 0 ) {
    printf("Achtung: Data files not found in directory %s!\n", path);
    iError = ERROR_DATAFILE_NOT_FOUND;
    return iError;
  }
  last_datafile_name.at(n-1)=0;
  // printf("DATAFILE: %s\n", last_datafile_name.c_str());
  sprintf(fname, "%s", last_datafile_name.c_str());
  return iError;
}

//______________________________________________________________________________
void looking_for_devices()
{
  char filename[512];
  get_last_datafile(filename);
  if (iError) {
    exit(iError);
  }
  if (!Decoder->LookingForDevices(Form("%s/%s", settings.monitor_datadir.c_str(), filename))) // number of devices
  {
    printf("Error %d raised. Exiting.\n", ERROR_DATADIR_OPEN);
    exit(ERROR_DATADIR_OPEN);
  }
  vADCMeta = Decoder->GetADCMetaData();               // retrieving ADCS structure
  gui->GetMonitor()->SetMetaData(vADCMeta);
  if (bMonInit) return;  // skip the following lines if the monitor window is shown
  bMonInit = true;
  Decoder->PrintMetaData(vADCMeta, -1, false);        // The third parameter toggles the map printout
  UChar_t lineWidth = 2;
  UChar_t markerStyle = 20;
  Double_t markerSize = 0.8;
  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto sn = (*itD).devsn;
    MonGraph.devsn = sn;
    MonHisto.devsn = sn;
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      MonGraph.isExist.reset();
      MonHisto.isExist.reset();

      // if ((*itD).ach.test(ch))
      {
        MonGraph.ch = ch;
        MonHisto.ch = ch;
        const char *gNamePrefix[3] = {"amp", "charge", "time"};
        const char *gTitlePrefix[3] = {"Amplitude time profile",
                                      "Charge time profile",
                                      "Time profile of signal arrival time"};
        const char *gYAxisTitle[3] = {"Amplitude, channels",
                                     "Charge, pC",
                                     "Time, ns"};
        const char *hNamePrefix[3] = {"spectrum", "dtime", "ftime"};
        const char *hTitlePrefix[3] = {"Spectrum",
                                      "Time spectrum",
                                      "Time spectrum"};
        const char *hYAxisTitle[3] = {"Charge, adc channels",
                                     "Time, ns",
                                     "Time, ns"};


        int colors[3] = { kBlue, kBlue, kViolet};
        Float_t hRanges[3][3] = {
          {NUMBER_OF_BINS,BIN_FIRST_CHARGE,BIN_LAST_CHARGE},
          {NUMBER_OF_TIMEDIFF_BINS*TIMEDIFF_BIN_FACTOR,TIMEDIFF_BIN_FIRST,TIMEDIFF_BIN_LAST},
          {NUMBER_OF_TIME_BINS*TIME_BIN_FACTOR,0,NUMBER_OF_TIME_BINS}
        };
        for (int gt=0; gt<3; gt++)
        {
          MonGraph.type = gt;
          MonGraph.plot  = new TGraph();
          ((TGraph *)MonGraph.plot)->SetNameTitle(
                                      Form("graph_%s_%x_%02d", gNamePrefix[gt], MonGraph.devsn, ch),
                                      Form("%s [%x CH%02d];Time;%s", gTitlePrefix[gt], MonGraph.devsn, ch, gYAxisTitle[gt])
                                    );
          ((TGraph *)MonGraph.plot)->SetLineColor(colors[gt]);
          ((TGraph *)MonGraph.plot)->SetLineWidth(lineWidth);
          ((TGraph *)MonGraph.plot)->SetMarkerStyle(markerStyle);
          ((TGraph *)MonGraph.plot)->SetMarkerColor(colors[gt]);
          ((TGraph *)MonGraph.plot)->SetMarkerSize(markerSize);

          MonGraph.isExist.set(gt);
          vMonGraph.push_back(MonGraph);
        }

        // Charge histo
        for (int pt=4; pt<=6; pt++)
        {
          auto rng = hRanges[pt-4];
          MonHisto.type = pt;
          MonHisto.plot = new TH1F(Form("%s_%x_ch%02d",hNamePrefix[pt-4], sn,ch),\
                                  Form("%s [%x CH%02d];%s;Number of counts, N",hTitlePrefix[pt-4], sn,ch, hYAxisTitle[pt-4]),\
                                  rng[0],rng[1],rng[2]);
          ((TH1F *)MonHisto.plot)->SetLineColor(colors[pt-4]);
          MonHisto.isExist.set(MonHisto.type);
          vMonHisto.push_back(MonHisto);
        }
      }
    }
  }
}

//______________________________________________________________________________
void dev_num_changed()
{
  printf("Achtung: Number of devices has changed! Please restart monitoring.\n");
  bDevNumChanged=true;
  exit(0);
}

//______________________________________________________________________________
void start_monitoring()
{
  bMonOnline=true;
  int count = 0;
  UInt_t devsn;
  UChar_t channel;
  UInt_t nevents;

  Decoder->LogInit(true);
  UInt_t uiIsEnoughEvts;

  UChar_t lastDevNum = vADCMeta->size();
  load_charge_gate();

  while ( bMonOnline )
  {
    if (count%MONITOR_FILE_CHECK == 0)
    {
      looking_for_devices();
    }

    if (lastDevNum!=vADCMeta->size()){
      dev_num_changed();
    }

    if (count>=gui->GetMonitor()->GetResetNumber()) clear_plots();
    devsn = gui->GetMonitor()->GetDeviceSN();
    channel = gui->GetMonitor()->GetChannel();
    nevents = gui->GetMonitor()->GetNumberOfEvents();
    uiIsEnoughEvts=0;
    Decoder->ClearIndexation();
    for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
    {
      auto fd = Decoder->OpenDataFile(Form("%s/%s", (*itD).dir, (*itD).file), 1,(*itD).devsn);
      (*itD).ev = Decoder->GetTotalEvents();
      Decoder->CloseDataFile(fd);
    }
    printf("Cycle#%02d Nevents: %d  Dev: %x Ch: %02d ", count, nevents, devsn, channel);
    for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
    {
      if ((*itD).ev+THREAD_NUMBER>nevents) uiIsEnoughEvts++;
    }
    // Start monitoring only when each file contains more than nevents events.
    Watch->Reset();
    Watch->Start();
    if (uiIsEnoughEvts==vADCMeta->size()) process_monitoring(0, count, devsn, channel, nevents);
    Watch->Stop();

    if (count%100 == 0 && count!=0)
    {
      printf("[%f s] ", Watch->RealTime());
      Decoder->LogWrite(LOG_MONITOR_FILENAME);
    }
    else printf("[%f s]\n", Watch->RealTime());

    count++;
    gSystem->ProcessEvents();

    // break;
  }
}

//______________________________________________________________________________
void stop_monitoring()
{
  // bMonInit=false;   // uncomment if the monitor is started from the main Viewer window
  if (bMonOnline){
    bMonOnline=false;
    printf("Monitoring is aborted!\n");
  }
}

//______________________________________________________________________________
void process_monitoring(UInt_t trigChannel, UInt_t cycle,
                        UInt_t devsn, UChar_t channel, UInt_t nevents)
{
  std::mutex m;
  std::condition_variable cond_var;
  bool bNotified = true;

  UInt_t uiTotEvents = nevents;
  UInt_t uiTrigChannel = gui->GetMonitor()->GetTrigChannel();
  UInt_t uiTrigDevice = gui->GetMonitor()->GetTrigDeviceSN();
  DATA adata;
  std::vector<DATA> vAD[THREAD_NUMBER], vAveData;			// average waveforms data
  vAveData.clear();
  for (int t=0; t<THREAD_NUMBER; t++) vAD[t].clear();
  const int ciDN = vADCMeta->size();                    // number of devices
  TH1F *Histo_ch[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];    // charge temporary histos
  TH1F *Histo_ft[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];    // time by fit
  TH1F *HistoDiff_ft[ciDN][CHANNEL_NUMBER][THREAD_NUMBER];   // diff time by fit
  Float_t fTimeMap[ciDN][CHANNEL_NUMBER];
  Float_t fChargeMap[ciDN][CHANNEL_NUMBER];
  Float_t fAmpMap[ciDN][CHANNEL_NUMBER];

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d = (*itD).devorn;
    auto sn =  (*itD).devsn;
    adata.devsn = sn;
    vAveData.push_back(adata);
    for (int t=0; t<THREAD_NUMBER; t++) vAD[t].push_back(adata);
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      fTimeMap[d][ch]=0;
      fChargeMap[d][ch]=0;
      fAmpMap[d][ch]=0;
      for (int s=0; s<(*itD).sn; s++) vAveData.at(d).wf[ch].push_back(0);
      for (int t=0; t<THREAD_NUMBER; t++)
      {
        for (int s=0; s<(*itD).sn; s++) vAD[t].at(d).wf[ch].push_back(0);
      }
      // Add histos for charge spectra
      if ((*itD).ach.test(ch))
      {
        for (int t=0; t<THREAD_NUMBER; t++){
          Histo_ch[d][ch][t] = new TH1F(Form("shisto_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                  Form("shisto_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                  NUMBER_OF_BINS,BIN_FIRST_CHARGE,BIN_LAST_CHARGE);
          Histo_ft[d][ch][t] = new TH1F(Form("thisto_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                        Form("thisto [DEV%02d CH%02d worker%02d]",d,ch,t),\
                                        NUMBER_OF_TIME_BINS*TIME_BIN_FACTOR,0,NUMBER_OF_TIME_BINS);
          HistoDiff_ft[d][ch][t] = new TH1F(Form("thisto_diff_dev%02d_ch%02d_worker%02d",d,ch,t),\
                                        Form("thisto diff [DEV%02d CH%02d worker%02d]",d,ch,t),\
                                        NUMBER_OF_TIMEDIFF_BINS*TIMEDIFF_BIN_FACTOR,TIMEDIFF_BIN_FIRST,TIMEDIFF_BIN_LAST);

        }
      }
    }
    if ( nevents > (*itD).ev ) uiTotEvents = (*itD).ev;
  }

  // LAMBDA
  auto workMonitor = [ciDN,uiTotEvents,uiTrigDevice,uiTrigChannel,&vAD,&vAveData,
                      &Histo_ch,&Histo_ft,&HistoDiff_ft,
                      &bNotified,&m,&cond_var](UInt_t workerID)
  {
    std::unique_lock<std::mutex> lock(m);

    std::vector<ADCS> meta(*vADCMeta);
    iError=0;
    std::vector<DATA> vD;       // vector of data structures with different device id <data device id#1 ... data device id#N>
    FILE *fd[ciDN];
    float fDiff = 0;
    float fTime = 0;
    float fTrigTime = 0;
    Float_t fA = 0;   // amplitude of signal - minimum of waveform
    Float_t fAT = 0;   // amplitude of signal - minimum of waveform for Trigger
    UInt_t uiNumOfEvents[ciDN];
    std::vector<DATA> vData;

    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      fd[(*itD).devorn] = Decoder->OpenDataFile(Form("%s/%s",(*itD).dir,(*itD).file),0);  // second parameter disables indexation
      uiNumOfEvents[(*itD).devorn] = (*itD).ev;
      if (!fd[(*itD).devorn])
      {
        iError=ERROR_DATAFILE_OPEN;
        return;
      }
    }
    // printf("Files have been opened\n");
    Int_t iInt = uiTotEvents/THREAD_NUMBER; // 5/4 = 1
    Int_t iRem = uiTotEvents%THREAD_NUMBER; // 5%4 = 1
    Bool_t bIsValidEvent;
    for (int e=1; e<=uiTotEvents; e+=THREAD_NUMBER)
    {
      bIsValidEvent = (
        (e<1+iInt*THREAD_NUMBER) ||
        !((e==1+iInt*THREAD_NUMBER) && (workerID>=iRem))
      );
      // printf("e %d\n", e);
      vData.clear();
      fTrigTime = 0;
      for (auto itD=meta.begin(); itD!=meta.end(); itD++)
      {
        auto d = (*itD).devorn;
        UInt_t evt = uiNumOfEvents[d]-uiTotEvents+e+workerID;
        // printf("wID %d valid: %d evt %d e %d\n", workerID, bIsValidEvent, evt, e);
        if (bIsValidEvent)
        {
          iError = Decoder->ReadEventADC64(fd[d],evt,&vD,VERBOSE_MODE,d, (*itD).devsn,workerID); // the last two parametes: verbose mode and the number of device to find event position in index array
          if (iError!=0) return;
          vData.push_back(vD.at(0));
          if ( ((*itD).devsn==uiTrigDevice) && ((*itD).ach.test(uiTrigChannel)) )
          {
            if (!FIT_DISCRETISATION_IMPOVEMENT)
            {
              // not improved
              fTrigTime = Decoder->GetTimeFitCrossPoint((*itD).devsn, uiTrigChannel, (*itD).sn, \
                                                        &vData.at(d).wf[uiTrigChannel]);
            }
            else
            {
              fTrigTime = Decoder->GetTimeFitCrossPointImproved((*itD).devsn, uiTrigChannel,\
                                  &vD.at(0).wf[uiTrigChannel], &fAT);
            }
          }
        }
      }

      // Get average waveform for each event
      if (bIsValidEvent)
      {
        if(!vData.empty()) Decoder->GetAverageWave(&vData,&vAD[workerID]);

        for (auto itD=meta.begin(); itD!=meta.end(); itD++)
        {
          auto d = (*itD).devorn;

          for (int ch=0; ch<CHANNEL_NUMBER; ch++)
          {
            if ((*itD).ach.test(ch))
            {
              // Get charge spectra
              switch (DEVICE_TYPE)
              {
                case DEVICE_ID_ADC64:
                  Histo_ch[d][ch][workerID]->Fill(Decoder->GetIntegralADC64((*itD).devsn, ch, &vData.at(d).wf[ch], bPedSubtr)+usPedShift);
                break;
                case DEVICE_ID_TQDC:
                  Histo_ch[d][ch][workerID]->Fill(Decoder->GetIntegralTQDC((*itD).devsn, ch, &vData.at(d).wf[ch], bPedSubtr)+usPedShift);
                break;
              }
              // Get time spectra by fit
              fTime = 0;
              if (!FIT_DISCRETISATION_IMPOVEMENT)
              {
                // not improved
                fTime     = Decoder->GetTimeFitCrossPoint((*itD).devsn, ch, (*itD).sn, \
                                                          &vData.at(d).wf[ch]);
              }
              else
              {
                // improved
                fTime     = Decoder->GetTimeFitCrossPointImproved((*itD).devsn, ch, \
                                                          &vData.at(d).wf[ch], &fA);
              }
              fDiff = fTime - fTrigTime;
              if (!FIT_DISCRETISATION_IMPOVEMENT)
              {
                Histo_ft[d][ch][workerID]->Fill(fTime*KERNEL_NUMBER);
                HistoDiff_ft[d][ch][workerID]->Fill(fDiff*KERNEL_NUMBER + usPedShift);
              }
              else
              {
                Histo_ft[d][ch][workerID]->Fill(fTime);
                HistoDiff_ft[d][ch][workerID]->Fill(fDiff + usPedShift);
              }
            } // end if (meta->at(d).ach.test(ch))
          } // end of for channel
        } // end of for d
      }
    } // end of for evts

    while(!bNotified){
      cond_var.wait(lock);
    }
    bNotified = false;

    // Get average for each thread
    Decoder->GetAverageWave(&vAD[workerID], &vAveData,true);
    for (auto itD=meta.begin(); itD!=meta.end(); itD++)
    {
      auto d   = (*itD).devorn;
      auto sn  = (*itD).devsn;
      // if (workerID==0)
      //   Decoder->CopyMetaData(&vData, &vAveData, d); // to copy info about devices to the average wave struct
      // Integral summation
      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        if ((*itD).ach.test(ch))
        {

          for (int pt=4; pt<=6; pt++)
          {
            auto histoPtr = std::find_if(vMonHisto.begin(), vMonHisto.end(),
                            [sn,ch,pt] (const MON_PLOT& plot) {
                              return plot.devsn == sn && plot.ch == ch && plot.type == pt;
                            });
            switch (pt)
            {
              case 4:
                // charge
                ((TH1F*)histoPtr->plot)->Add(Histo_ch[d][ch][workerID]);
              break;
              case 5:
                // diff time
                ((TH1F*)histoPtr->plot)->Add(HistoDiff_ft[d][ch][workerID]);
              break;
              case 6:
                // time
                ((TH1F*)histoPtr->plot)->Add(Histo_ft[d][ch][workerID]);
              break;
            }
          }
        }
      }
      Decoder->CloseDataFile(fd[d]);
    } // end of for d
    bNotified = true;
    cond_var.notify_one();
  };
  // END OF LAMBDA ////////////////////////////////////////////////////////////

  // Fill the "pool" with workers
  // printf("\n");
  workers.clear();
  for (auto id=0; id<THREAD_NUMBER; id++)
    workers.emplace_back(workMonitor, id);
  // Now join them
  for (auto &&worker : workers) worker.join();

  if (iError) {
    if (settings.guimode!=2){ exit(iError); }
    else {;}
  }

  TTimeStamp tStamp;

  for (auto itD=vADCMeta->begin(); itD!=vADCMeta->end(); itD++)
  {
    auto d  = (*itD).devorn;
    auto sn = (*itD).devsn;
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      fAmpMap[d][ch] = 0;
      fChargeMap[d][ch] = 0;
      fTimeMap[d][ch] = 0;
      if ((*itD).ach.test(ch))
      {
        fAmpMap[d][ch] = Decoder->GetAmplitude((*itD).sn, sn, ch, &vAveData.at(d).wf[ch])/uiTotEvents;

        for (int gt=0; gt<3; gt++)
        {
          auto graphPtr = std::find_if(vMonGraph.begin(), vMonGraph.end(),
          [sn,ch,gt] (const MON_PLOT& graph) {
            return graph.devsn == sn && graph.ch == ch && graph.type == gt;
          });
          auto ht = gt+3;
          auto histoPtr = std::find_if(vMonHisto.begin(), vMonHisto.end(),
          [sn,ch,ht] (const MON_PLOT& histo) {
            return histo.devsn == sn && histo.ch == ch && histo.type == ht;
          });
          switch (gt) {
            case 0:
              ((TGraph*)graphPtr->plot)->SetPoint(cycle,cycle,fAmpMap[d][ch]);
            break;
            case 1:
              fChargeMap[d][ch] = ((TH1F*)histoPtr->plot)->GetMean();       // charge map
              ((TGraph*)graphPtr->plot)->SetPoint(cycle,cycle,fChargeMap[d][ch]);
            break;
            case 2:
              fTimeMap[d][ch] = ((TH1F*)histoPtr->plot)->GetMean();    // time map
              ((TGraph*)graphPtr->plot)->SetPoint(cycle,cycle,fTimeMap[d][ch]);
            break;
          }
        }
        for (int t=0; t<THREAD_NUMBER; t++){
          delete Histo_ch[d][ch][t];
          delete Histo_ft[d][ch][t];
          delete HistoDiff_ft[d][ch][t];
        }
      }
    }
    Decoder->LogFill(0,fAmpMap[d],   (*itD).devsn,nevents,'a',tStamp.GetSec(),cycle);
    Decoder->LogFill(0,fChargeMap[d],(*itD).devsn,nevents,'i',tStamp.GetSec(),cycle);
    Decoder->LogFill(0,fTimeMap[d],  (*itD).devsn,nevents,'t',tStamp.GetSec(),cycle);
  }

  // Render maps
  gui->GetMonitor()->DrawMap(0, fAmpMap);
  gui->GetMonitor()->DrawMap(1, fChargeMap);
  gui->GetMonitor()->DrawMap(2, fTimeMap);

  // Render average waveform
  auto devIndex = std::find_if(vADCMeta->begin(), vADCMeta->end(),
                       [devsn] (const ADCS& adc) {
                          return adc.devsn == devsn;
                       });
  gui->GetMonitor()->DrawAverage(show_avg_waveform(devIndex->devorn,channel,
                                 vADCMeta->at(devIndex->devorn).sn,uiTotEvents,&vAveData));

  // Render histos
  for (int pt=4; pt<=6-1; pt++)
  {
    auto histoPtr = std::find_if(vMonHisto.begin(), vMonHisto.end(),
    [devsn,channel,pt] (const MON_PLOT& plot) {
      return plot.devsn == devsn && plot.ch == channel && plot.type == pt;
    });

    switch (pt)
    {
      case 4:
        // charge
        gui->GetMonitor()->DrawCharge((TH1F*)histoPtr->plot);
      break;
      case 5:
        // diff time
        gui->GetMonitor()->DrawTSpectrum((TH1F*)histoPtr->plot);
      break;
      case 6:
        // time
       gui->GetMonitor()->DrawTSpectrum((TH1F*)histoPtr->plot);
      break;
    }
  }

  // Render time profiles
  for (int gt=0; gt<3; gt++)
  {
    auto graphPtr = std::find_if(vMonGraph.begin(), vMonGraph.end(),
    [devsn,channel,gt] (const MON_PLOT& graph) {
      return graph.devsn == devsn && graph.ch == channel && graph.type == gt;
    });
    gui->GetMonitor()->DrawProfile(gt, (TGraph*)graphPtr->plot);
  }
}

//______________________________________________________________________________
void get_peaks()
{
  LAST_STATE state = gui->GetLastState();
  Int_t devsn = state.dev;
  Int_t ch    = state.ch;
  auto dataPtr = std::find_if(vDataPtr.begin(), vDataPtr.end(),
  [devsn] (const DATA& data) {
    return data.devsn == devsn;
  });
  // printf("DevSN %x CH %02d vData size: %zu\n", devsn, ch, vDataPtr.size());
  if (dataPtr==vDataPtr.end()) printf("Achtung: data has not been found.\n");
  else Decoder->FindPeaks(devsn, ch, &(dataPtr->wf[ch]), gui->GetThreshold());
}

//______________________________________________________________________________
Double_t ARFunc(Double_t *xx, Double_t *par)
{
  Int_t    GAUS_NUMBER = 30;  // number of gaussian functions that are used for a pmt response evaluation
  Double_t x      = xx[0];
  // parameters
  Double_t Q0     = par[0];   // mean of the pedestal
  Double_t sigma0 = par[1];   // standart deviation of the pedestal
  Double_t Q1     = par[2];   // mean of the first peak
  Double_t sigma1 = par[3];   // standart deviation of the first peak
  Double_t mu     = par[4];   // number of photoelectrons
  Double_t weight = par[5];   // contribution of exponential events in the pedestal
  Double_t alpha  = par[6];   // exponent parameter
  Double_t norm   = par[7];   // number events in the pedestal
  Double_t constant = par[8]; // some constant for error function

  Double_t dGaus     = 0;
  Double_t dExp      = 0;
  Double_t dExpCut   = 0;
  Double_t dSignal   = 0;
  Double_t dPedestal = 0;

  dGaus   = 1/(sigma0 * sqrt(2*TMath::Pi())) * exp(-pow(x-Q0,2)/(2*pow(sigma0,2)) );
  dExp    = alpha*exp(-alpha*x);
  // dExpCut = constant*(1+erf( (x-Q0)/(sigma0*sqrt(2)) ));  // The cumulative distribution function of gaussian function
  dExpCut = /* (1/2)* */(1+erf( (x-Q0)/(sigma0*sqrt(2)) ));  // The cumulative distribution function of gaussian function

  dPedestal = ( (1-weight)*dGaus + weight*dExp*dExpCut)*exp(-mu);

  for (int i=1; i<=GAUS_NUMBER; i++)
  {
     Double_t part1 = pow(mu,i)*exp(-mu) / TMath::Factorial(i);
     Double_t part2 = 1/(sigma1*sqrt(2*TMath::Pi()*i));
     Double_t part3 = exp(-pow(x-Q0-i*Q1,2)/(2*i*pow(sigma1,2)));
     dSignal += part1 * part2 * part3;
   }

  Double_t dRes = (dPedestal + dSignal)*norm;

  return dRes;
}

//______________________________________________________________________________
TH1F* get_displayed_histo()
{
  TObject* obj = gui->GetDisplayedPlot(gui->GetCanvas());
  if (!obj)
  {
    printf("Achtung! No histogram is displayed.\n");
    return NULL;
  }
  if ( obj->InheritsFrom(TH1F::Class()) ) return (TH1F*)obj;
  return NULL;
}

//______________________________________________________________________________
void save_config()
{
  Decoder->SetSignalOffset(gui->GetConfig()->offset);
  bPedSubtr = gui->GetConfig()->pedsubtr;
  usPedShift = gui->GetConfig()->pedshift;
}

//______________________________________________________________________________
void fit_arfunc()
{
  TH1F* histo = get_displayed_histo();
  TF1 *ar_func = new TF1("ar_func", ARFunc, 0, gui->GetLastBin(), 8);
  ar_func->SetParNames("Q0", "#sigma0", "Q1", "#sigma1", "#mu", "Exp.weight", "#alpha", "Event number");
  auto nEvents = histo->GetEntries();
  int iHistoRange[2] = {0, 1000};
  TCanvas *canvas = gui->GetCanvas();
  canvas->cd();
  iHistoRange[0] = gPad->GetUxmin();
  iHistoRange[1] = gPad->GetUxmax();
  ar_func->SetParameters(usPedShift, 1., usPedShift+70, 1., 1, 0.5, 1, nEvents);
  ar_func->SetParLimits(0, usPedShift-15, usPedShift+15);
  ar_func->SetParLimits(1, 0, 30);
  ar_func->SetParLimits(2, usPedShift-15+70, usPedShift+15+70);
  ar_func->SetParLimits(3, 0, 30);
  ar_func->SetParLimits(4, 0, 10);
  ar_func->SetParLimits(5, 0, 50);
  ar_func->SetParLimits(6, 0, 10);
  ar_func->SetParLimits(7, 0, nEvents);
  ar_func->FixParameter(7, nEvents);
  histo->Fit("ar_func", "", "", iHistoRange[0], iHistoRange[1]);
  canvas->Update();
}

//______________________________________________________________________________
void fit_single_gauss()
{
  TH1F* histo = get_displayed_histo();
  if (!histo)
  {
    printf("Achtung: No histogram is displayed!\n");
    return;
  }
  TH1F* darkhisto = (TH1F*)gROOT->FindObject(Form("dark%s",histo->GetName()));
  if (!darkhisto)
  {
    printf("Achtung: No dark histogram is found!\n");
    return;
  }

  double dPeakThr = 50;
  double dNtimes = 2;
  if (settings.guimode==2)
  {
    dPeakThr = gui->GetSGausFitThr();
    dNtimes  = gui->GetSGausFitStdDev();
  }
  double dQ0dark= 0;   // mean of pedestal from dark noise spectrum
  double dConst = 0;
  double dMean  = 0;
  double dMeanByMean  = 0;
  double dStd   = 0;
  double dN     = 0;
  double dS     = 0;
  int iWinMin   = 0;
  int iWinMax   = 0;
  double dBC    = 0;   // bin content
  bool  bSS     = 0;  // first condition
  bool  bSE     = 0;  // second condition
  double dBinWidth = 1; // ratio NumBins per tick
  double dMax   = 0;  // amplitude of max in the peak
  stPEAKS peak;
  peak.max      = 0;
  peak.first    = 0;
  peak.last     = 0;
  std::vector<stPEAKS> vPeaks;
  vPeaks.clear();

  double dFirstBin = BIN_FIRST_CHARGE;
  if (settings.guimode==2)
    dFirstBin = gui->GetFirstBin();

  auto canvas = gui->GetCanvas();
  canvas->cd();

  printf("Looking for object:  %s [%p] (NULL - means object's not found).\n",histo->GetName(),histo);

  // Simple Peak finder //
  iWinMin = histo->GetXaxis()->GetFirst();
  iWinMax = histo->GetXaxis()->GetLast();
  dBinWidth = histo->GetXaxis()->GetBinWidth(iWinMin);

  printf("Displayed range:     %d - %d bins [bin width: %.2f]\n", iWinMin,iWinMax,dBinWidth);

  for (int b=iWinMin; b<iWinMax; b++)
  {
    dBC = histo->GetBinContent(b);
    if (dBC > dPeakThr) { bSS = true;  if (peak.first == 0) peak.first = b; }
    if (bSS == true && dBC < dPeakThr) { bSE = true; if (peak.last == 0) peak.last = b; }

    if (bSS == true && bSE != true)
    {
      if (dBC > dMax) { dMax = dBC; peak.max = b; }
    }

    if (bSS == true && bSE == true)
    {
      if (peak.last - peak.max > peak.max - peak.first)
        peak.first = peak.max - (peak.last - peak.max);
      peak.max   = peak.max*dBinWidth+dFirstBin;
      peak.first = peak.first*dBinWidth+dFirstBin;
      peak.last  = peak.last*dBinWidth+dFirstBin;
      vPeaks.push_back(peak);
      bSS = false;
      bSE = false;
      peak.max   = 0;
      peak.first = 0;
      peak.last  = 0;
      dMax       = 0;
    }
  }
  if (vPeaks.empty())
  {
    printf("Achtung: No peaks have been found!\n");
    return;
  }
  printf("Number of peaks:     %lu\n", vPeaks.size());
  int pp=0;
  for(auto v=vPeaks.begin(); v!=vPeaks.end(); v++, pp++)
    printf("Peak#%02d maxpos: %.0f over %.0f amplitude in range of [%.0f - %.0f]\n", pp+1,v->max,dPeakThr,v->first,v->last);
  // Simple Peak finder //

  if (histo->Integral() != 0)
  {
    TF1 *SingleGausFit = new TF1("SingleGausFit","gaus",vPeaks[0].first,vPeaks[0].last);
    histo->Fit("SingleGausFit","QR");
    dConst = SingleGausFit->GetParameter(0);
    dMean  = SingleGausFit->GetParameter(1);
    dStd   = SingleGausFit->GetParameter(2);

    SingleGausFit->SetParLimits(0,0,100000);
    SingleGausFit->SetParLimits(1,dMean-(2*dNtimes*dStd),dMean+(2*dNtimes*dStd));
    SingleGausFit->SetParLimits(2,0,(2*dNtimes*dStd));

    SingleGausFit->SetParameters(dConst,dMean,dStd);
    SingleGausFit->SetParNames("Const","Mean","St.Dev.");
    histo->Fit("SingleGausFit","Q","",dMean-(dNtimes*dStd),dMean+(dNtimes*dStd));

    dConst = SingleGausFit->GetParameter(0);
    dMean  = SingleGausFit->GetParameter(1);
    dStd   = SingleGausFit->GetParameter(2);
    dN     = dConst*dStd*sqrt(TMath::TwoPi())*(1/dBinWidth);

    dQ0dark = darkhisto->GetMean();
    dMeanByMean = histo->GetMean();
    dS  = dMeanByMean - dQ0dark;

    printf("-----Single fit results:-----\n");
    printf("Const:               %.3f\n", dConst);
    printf("Mean:                %.3f\n", dMean);
    printf("St.Dev.:             %.3f\n", dStd);
    printf("Q0:                  %.3f [dark]\n", dQ0dark);
    printf("S:                   %.3f [by fit]\n",  dMean - dQ0dark);
    printf("S:                   %.3f [by mean]\n", dS);
    printf("Number of events:    %d [+/- %.2f*sigma]\n", int(dN),dNtimes);
    printf("-----------------------------\n");

    // // log started
    // uint32_t devsn = gui->GetDisplayedDev();
    // int ch         = gui->GetDisplayedCH();
    // auto gatePtr = std::find_if(vGate->begin(), vGate->end(),
    //                      [ch,devsn] (const GATE& g) {
    //                         return g.ch == ch && g.devsn == devsn;
    //                      });
    //
    // if (gatePtr==vGate->end())
    // {
    //   printf("Achtung: Device or channel not found.\n");
    // }
    // // calibration at november - CalibartionRun_10V.data amp
    // double dGains[16] =
    // { 1,1,
    //   56.46, 59.56, 51.9,  59.47, 55.61, 59.82,
    //   1,
    //   56.61, 57.38, 43.81, 42,    45.31, 58.44,
    //   1
    // };
    //
    // FILE *fdLog = fopen(Form("%s/%s", settings.workdir.c_str(),"calib.txt"),"at");
    // // if (ch<16)
    // // {
    // //   dGains[ch] *= 1.4;
    // //   fprintf(fdLog,"File: %s CH %02d Gate %d Q0 %.2f Mean %.2f S %.2f G_1.4 %.2f Mu %.2f\n",\
    // //           vADCMeta->at(0).file,ch, (gatePtr->sig_end-gatePtr->sig_start)*10,\
    // //           dQ0dark,dMeanByMean,dS,dGains[ch],dS/dGains[ch] );
    // // }
    // fclose(fdLog);
    // // log end

  }
  else printf("Not enough data for fit. Tune threshold carefully!\n");

  canvas->Update();
}

//______________________________________________________________________________
void fit_double_gauss()
{
  TH1F* histo = get_displayed_histo();
  if (!histo)
  {
    printf("Achtung: No histogram is displayed!\n");
    return;
  }
  printf("Looking for object:  %s [%p] (NULL - means object's not found).\n",histo->GetName(),histo);

  TH1F* darkhisto = (TH1F*)gROOT->FindObject(Form("dark%s",histo->GetName()));
  if (!darkhisto)
  {
    printf("Achtung: No dark histogram is found!\n");
    return;
  }

  double dPeakThr = 50;
  double dNtimes = 2;
  if (settings.guimode==2)
  {
    dPeakThr = gui->GetSGausFitThr();
    dNtimes  = gui->GetSGausFitStdDev();
  }
  double dQ0    = 0;   // mean of pedestal
  double dQ0dark= 0;   // mean of pedestal from dark noise spectrum
  double dP1    = 0;   // mean of 1st peak
  double dGpix  = 0;   // gain of pixel
  double dGdet  = 0;   // gain of detector
  double dMean  = 0;   // mean of spectrum
  double dCT    = 0;   // crosstalk
  double dMu    = 0;   // number of photoelectrons
  double dMu_thr    = 0;   // number of photoelectrons taking into account light and dark spectra up to threshold
  double dN0    = 0;   // number of pedestal events @ light
  double dD0    = 0;   // number of pedestal events @ dark
  double dQ0Std = 0;   // standart deviation
  double dP1Std = 0;   // standart deviation
  double dQ0Const = 0;
  double dP1Const = 0;
  double dN     = 0;   // number events in spectrum
  double dMuErr = 0;
  double dMuErrNoise = 0;
  double dS     = 0;
  int iWinMin   = 0;
  int iWinMax   = 0;
  double dBC=0;   // bin content
  bool  bSS = 0;  // first condition
  bool  bSE = 0;  // second condition
  double dMax = 0;  // amplitude of max in the peak
  double dBinWidth = 1; // ratio NumBins per tick
  stPEAKS peak;
  peak.max   = 0;
  peak.first = 0;
  peak.last  = 0;
  std::vector<stPEAKS> vPeaks;
  vPeaks.clear();

  double dFirstBin = BIN_FIRST_CHARGE;
  if (settings.guimode==2)
    dFirstBin = gui->GetFirstBin();

  TCanvas *canvas = gui->GetCanvas();
  canvas->cd();
  TPaveText *PaveFitInfo = new TPaveText();
  TList *ListFitInfoLines;

  // Simple Peak finder //
  iWinMin = histo->GetXaxis()->GetFirst();
  iWinMax = histo->GetXaxis()->GetLast();
  dBinWidth = histo->GetXaxis()->GetBinWidth(iWinMin);

  printf("Displayed range:     %d - %d bins [bin width: %.2f]\n", iWinMin,iWinMax,dBinWidth);

  for (int b=iWinMin; b<iWinMax; b++)
  {
    dBC = histo->GetBinContent(b);
    if (dBC > dPeakThr) { bSS = true;  if (peak.first == 0) peak.first = b; }
    if (bSS == true && dBC < dPeakThr) { bSE = true; if (peak.last == 0) peak.last = b; }

    if (bSS == true && bSE != true)
    {
      if (dBC > dMax) { dMax = dBC; peak.max = b; }
    }

    if (bSS == true && bSE == true)
    {
      if (peak.last - peak.max > peak.max - peak.first) peak.first = peak.max - (peak.last - peak.max);
      peak.max   = peak.max*dBinWidth+dFirstBin;
      peak.first = peak.first*dBinWidth+dFirstBin;
      peak.last  = peak.last*dBinWidth+dFirstBin;
      vPeaks.push_back(peak);
      bSS = false;
      bSE = false;
      peak.max   = 0;
      peak.first = 0;
      peak.last  = 0;
      dMax       = 0;
    }
  }

  printf("Number of peaks:     %lu\n", vPeaks.size());
  int pp=0;
  for(auto v=vPeaks.begin(); v!=vPeaks.end(); v++, pp++) printf("Peak#%02d max: %.0f over %.0f amplitude in range of [%.0f - %.0f]\n", pp+1,v->max,dPeakThr,v->first,v->last);
  // Simple Peak finder //

  if (vPeaks.size()>=2)
  // if (vPeaks.size()>=1)
  {
    TF1 *SingleGausFit = new TF1("SingleGausFit","gaus",vPeaks[0].first,vPeaks[0].last);
    histo->Fit("SingleGausFit","QR");
    dQ0Const = SingleGausFit->GetParameter(0);
    dQ0      = SingleGausFit->GetParameter(1);
    dQ0Std   = SingleGausFit->GetParameter(2);

    histo->Fit("SingleGausFit","Q","",vPeaks[1].first,vPeaks[1].last);
    dP1Const = SingleGausFit->GetParameter(0);
    dP1      = SingleGausFit->GetParameter(1);
    dP1Std   = SingleGausFit->GetParameter(2);

    TF1 *DoubleGausFit = new TF1("DoubleGausFit","gaus(0)+gaus(3)",dQ0-(3*dQ0Std),dP1+(dNtimes*dP1Std));
    // TF1 *DoubleGausFit = new TF1("DoubleGausFit","gaus(0)+gaus(3)",-50,dPeakThr);
    DoubleGausFit->SetParLimits(0,0,100000);
    DoubleGausFit->SetParLimits(1,dQ0-(5*dQ0Std),dQ0+(5*dQ0Std));
    DoubleGausFit->SetParLimits(2,0,(5*dQ0Std));
    DoubleGausFit->SetParLimits(3,0,100000);
    DoubleGausFit->SetParLimits(4,dP1-(5*dP1Std),dP1+(5*dP1Std));
    DoubleGausFit->SetParLimits(5,0,(5*dP1Std));
    DoubleGausFit->SetParameters(dQ0Const,dQ0,dQ0Std,dP1Const,dP1,dP1Std);
    DoubleGausFit->SetParNames("C0","Q0","#sigma0","C1","P1","#sigma1");
    histo->Fit("DoubleGausFit","QR");
    dQ0    = DoubleGausFit->GetParameter(1);
    dQ0Std = DoubleGausFit->GetParameter(2);
    dP1    = DoubleGausFit->GetParameter(4);
    dP1Std = DoubleGausFit->GetParameter(5);

    histo->GetXaxis()->UnZoom();
    dMean = histo->GetMean();
    dQ0dark = darkhisto->GetMean();

    // if (iSpectrumType == 0)
      histo->GetXaxis()->SetRangeUser((iWinMin-1)*dBinWidth+dFirstBin,iWinMax*dBinWidth+dFirstBin);
    // else if (iSpectrumType == 1)
    //   histo->GetXaxis()->SetRangeUser((iWinMin-1)*dBinWidth,(iWinMax-1)*dBinWidth);

    dN = histo->GetEntries();
    dN0 = dQ0Const*dQ0Std*sqrt(TMath::TwoPi())*(1/dBinWidth);

    dGpix = dP1 - dQ0;
    dMu   = (-1)*log(dN0/dN);
    if (dMu != 0) dGdet = (dMean - dQ0)/dMu;
    dCT = (dGdet/dGpix - 1)*100;
    dS  = dMean - dQ0;



    // dMuErr = DrsDecoder->GetMuError(dN,dMu);
    // dMuErrNoise = DrsDecoder->GetMuErrorNoise(dN,dMu,dDCR[iSpectrumChannelID-1]*1000,\
    //                               fGateEnd[iSpectrumChannelID-1]-fGateStart[iSpectrumChannelID-1]);

    printf("-----Double fit results:-----\n");
    printf("Histo mean:          %.3f\n", dMean);
    printf("Q0:                  %.3f [dark: %.3f]\n", dQ0,dQ0dark);
    printf("Q0 St.Dev.:          %.3f\n", dQ0Std);
    printf("P1:                  %.3f\n", dP1);
    printf("P1 St.Dev.:          %.3f\n", dP1Std);
    printf("S:                   %.3f\n", dS);
    printf("N:                   %.0f events at all\n", dN);
    printf("N0:                  %.0f  events in the pedestal\n", dN0);
    printf("Mu:                  %.3f ph.e. [by non efficiency]\n", dMu);
    printf("Mu:                  %.3f ph.e. [by signal]\n", dS/dGpix);
    // printf("Mu error:            %f%% (no noise)\n", dMuErr*100);
    // printf("Mu error:            %f%% (@ %.3f kC/s noise level)\n",dMuErrNoise*100,dDCR[iSpectrumChannelID-1]);
    printf("Gpix:                %.3f\n", dGpix);
    printf("Gdet:                %.3f\n", dGdet);
    printf("Crosstallk:          %.2f%%\n", dCT);
    printf("-----------------------------\n");

    if ((ListFitInfoLines = PaveFitInfo->GetListOfLines())!=0) ListFitInfoLines->Delete();
    PaveFitInfo->SetTextSize(0.02);
    PaveFitInfo->SetTextAlign(11);  // default bottom aligment
    PaveFitInfo->SetTextColor(kGray+3);
    PaveFitInfo->SetTextFont(42);
    PaveFitInfo->SetFillColorAlpha(0,0);
    PaveFitInfo->AddText(Form("Mu:            %.2f#pm%.2f (%.2f)%%\n", dMu,dMuErr*100,dMuErrNoise*100));
    PaveFitInfo->AddText(Form("Gpix:         %.2f\n", dGpix));
    PaveFitInfo->AddText(Form("Gdet:         %.2f\n", dGdet));
    PaveFitInfo->AddText(Form("Crosstalk: %.2f%%\n", dCT));

    histo->Draw();
    // PaveFitInfo->Draw();

    // // log started
    // uint32_t devsn = gui->GetDisplayedDev();
    // int ch         = gui->GetDisplayedCH();
    // auto gatePtr = std::find_if(vGate->begin(), vGate->end(),
    //                      [ch,devsn] (const GATE& g) {
    //                         return g.ch == ch && g.devsn == devsn;
    //                      });
    //
    // if (gatePtr==vGate->end())
    // {
    //   printf("Achtung: Device or channel not found.\n");
    // }
    // FILE *fdLog = fopen(Form("%s/%s", settings.workdir.c_str(),"calib.txt"),"at");
    // // fprintf(fdLog,"CH %02d Gate %d Q0 %.2f P1 %.2f Mean %.2f S %.2f G %.2f Mu %.2f  Q0 %.2f Mean %.2f S %.2f\n",\
    // //         ch, (gatePtr->sig_end-gatePtr->sig_start)*10,dQ0, dP1, dMean, dS, dGpix,dS/dGpix, dQ0dark,dMean,dMean-dQ0dark);
    // // fprintf(fdLog,"File: %s CH %02d Gate %d Q0 %.2f Q0Dark %.2f P1 %.2f Mean %.2f S %.2f Gpix %.2f Mu_mean %.2f Mu_non_eff %.2f\n",\
    // //         vADCMeta->at(0).file,ch, (gatePtr->sig_end-gatePtr->sig_start)*10,dQ0,dQ0dark, dP1, dMean, dS, dGpix,dS/dGpix, dMu);
    // fprintf(fdLog,"%s %02d %d %d %.2f %.2f %.2f %.2f %.2f %.2f %.2f\n",\
    //           vADCMeta->at(0).file,ch, gatePtr->sig_start*10, gatePtr->sig_end*10,\
    //           dQ0dark,dQ0, dP1, dMean, dS, dGpix, dMu);
    // // fprintf(fdLog,"File: %s CH %02d Gate %d Q0 %.2f Mean %.2f S %.2f\n",\
    //         vADCMeta->at(0).file,ch, (gatePtr->sig_end-gatePtr->sig_start)*10, dQ0dark,dMean,dMean-dQ0dark);
    // fclose(fdLog);
    // // log end

  }
  else printf("Not enough data for fit.\nTune the threshold and binning carefully!\nNumber of peaks should be more than two ones.\n");

  // // store selected window ranges
  // iWinMin = histo->GetXaxis()->GetFirst();
  // iWinMax = histo->GetXaxis()->GetLast();
  // dBinWidth = histo->GetXaxis()->GetBinWidth(iWinMin);
  //
  // histo->GetXaxis()->SetRangeUser(dFirstBin,dPeakThr);
  // dN0 = histo->Integral();
  // darkhisto->GetXaxis()->SetRangeUser(dFirstBin,dPeakThr);
  // dD0 = darkhisto->Integral();
  // dMu_thr = (-1)*log(dN0/dD0);
  // // histo->GetXaxis()->UnZoom();
  // histo->GetXaxis()->SetRangeUser((iWinMin-1)*dBinWidth+dFirstBin,iWinMax*dBinWidth+dFirstBin);
  // darkhisto->GetXaxis()->UnZoom();
  // printf("Range: %.0f : %.0f N0: %.0f D0: %.0f Mu: %.3f by thr\n", dFirstBin,dPeakThr,dN0,dD0,dMu_thr);

  canvas->Update();
}

//______________________________________________________________________________
void set_gate_file(const char* fname)
{
    // printf("%s\n", fname);
  Decoder->SetGateConfigFileName(fname);
  // Decoder->SaveGateConfigFile();
}

/////////////////////////////Connections////////////////////////////////////////
//______________________________________________________________________________
bool make_connections()
{
  gui->Connect("SetDataFile(const char*)", "AFIDecoder", Decoder, "LookingForDevices(const char*)"); // AFIDecoder::LookingForDevices invokation
  gui->Connect("SignalSaveCGateSettings(Bool_t)", "AFIDecoder", Decoder, "SaveGateConfigFile()"); // AFIDecoder::SaveGateConfigFile invokation
  gui->Connect("SignalSaveKGateSettings(Bool_t)", "AFIDecoder", Decoder, "SaveKernelGateConfigFile()"); // AFIDecoder::SaveKernelGateConfigFile invokation
  gui->Connect("SignalResetKernelLoadState()", 0, 0, "reset_kernel_load_state()"); // AFIDecoder::SaveKernelGateConfigFile invokation
  gui->Connect("DrawChargeSpectrum()", 0, 0, "draw_charge_spectrum()");       // AFIProcessor::draw_charge_spectrum() invokation
  gui->Connect("DrawAverageWaveform(Bool_t)", 0, 0, "draw_avg_wave(Bool_t)");             // AFIProcessor::draw_ave_wave() invokation
  gui->Connect("DrawSingleWaveform()", 0, 0, "draw_single_wave()");           // AFIProcessor::draw_ave_wave() invokation
  gui->Connect("DrawTimeFitSpectrum()", 0, 0, "draw_time_spectrum_fit()");    // AFIProcessor::draw_time_spectrum_fit() invokation
  gui->Connect("DrawTimePConvSpectrum()", 0, 0, "draw_time_spectrum_pconv()");    // AFIProcessor::draw_time_spectrum_pconv() invokation
  gui->Connect("DrawTimeRConvSpectrum()", 0, 0, "draw_time_spectrum_rconv()");    // AFIProcessor::draw_time_spectrum_rconv() invokation
  gui->Connect("LookingForDevices()", 0, 0, "looking_for_devices()");
  gui->Connect("SignalSetGateFile(const char*)", 0, 0, "set_gate_file(const char*)");
  gui->Connect("SaveConfig()", 0, 0, "save_config()");
  gui->Connect("ConnectMonitor()", 0, 0, "connect_monitor()");
  gui->Connect("SignalSetMetaData()", 0, 0, "set_meta_data()");
  gui->Connect("SignalLoadPrecisedKernels(Bool_t)", 0, 0, "load_pkernel(Bool_t)");
  gui->Connect("SignalLoadCGate()", 0, 0, "load_charge_gate()");
  gui->Connect("SignalLoadKGate()", 0, 0, "load_pgate()");
  gui->Connect("SignalGetWFPars()", 0, 0, "get_peaks()");
  gui->Connect("SignalFitSPE()", 0, 0, "fit_arfunc()");
  gui->Connect("SignalFitSGauss()", 0, 0, "fit_single_gauss()");
  gui->Connect("SignalFitDGauss()", 0, 0, "fit_double_gauss()");
  // gui->Connect("SignalUpdateKernelType(int)", 0, 0, "update_kernel_type()");
  connect_monitor();
  return true;
}

//______________________________________________________________________________
bool connect_monitor()
{
  gui->GetMonitor()->Connect("StartMonitoring()", 0, 0, "start_monitoring()");
  gui->GetMonitor()->Connect("StopMonitoring()", 0, 0, "stop_monitoring()");
  gui->GetMonitor()->Connect("ResetMonitoring()", 0, 0, "clear_plots()");
  gui->GetMonitor()->Connect("DoClose()", 0, 0, "stop_monitoring()");
  return true;
}
