#ifndef AFI_SETTINGS_H
#define AFI_SETTINGS_H

#include <string>
#include <bitset>
#include <stdio.h>
#include <TH1F.h>
#include "TGraph.h"

#define THREAD_NUMBER         4U   // how many threads are used

#define GUI_MODE              0    // 0 - start without GUI, 1 - start with TBrowser, 2 - start with AFIGUI

#define DEVICE_ID_ADC64       0xd9
#define DEVICE_ID_TQDC        0xd6
#if device_type==0
  #define DEVICE_TYPE           DEVICE_ID_ADC64
#elif device_type==1
  #define DEVICE_TYPE           DEVICE_ID_TQDC
#endif

#define SIGNAL_INVERSION      1         // 0 - no signal inversion, 1 - signal inversion is enabled - for each sample: [*(-1)]
#define SIGNAL_POLARITY       -1         // -1 - negative polarity, 1 - positive polarity
#define SIGNAL_SETTINGS_FILENAME "settings.cfg"
#define SIGNAL_OFFSET         0         // signal shift of baseline for each sample
#define SCALE_REPRESENTATION  0         // 0 - in samples, 1 - in mV and pC
#define VERBOSE_MODE          0
#define DEBUG_MODE            0
#define RLOGGER_STORE_RGRAH   0       // 1 - enable rgrah root logger, 0 - disable
#define RLOGGER_STORE_RATE    200     // dump data to rlog* & rgraph* ROOT file after pointed number of events per thread
#if DEVICE_TYPE==DEVICE_ID_ADC64
  #define CHANNEL_NUMBER      64
  #define TIME_DISCRETIZATION 10  // 10 ns per sample for ADC64
#elif DEVICE_TYPE==DEVICE_ID_TQDC
  #define CHANNEL_NUMBER      16
  #define TIME_DISCRETIZATION 8  // 8 ns per sample for TQDC2
#endif

#define TQDC_BITS_TO_MV        (1/4.0)*2*1000.0/16384.0 // 2^14 bits/2V - 4 counts
#define TQDC_BITS_TO_PC        TQDC_BITS_TO_MV*0.02*TIME_DISCRETIZATION //  50 Ohm
// #define TQDC_BITS_TO_PC        TQDC_BITS_TO_MV*10E-3*0.02*TIME_DISCRETIZATION*10E-9*10E+12 //

#define ADC64_BITS_TO_MV       (1/64.0)*1.6*1000.0/1024 // 2^10 bits/1.6V - 64 counts 1 voltage count 0.0244 mV
#define ADC64_BITS_TO_PC       ADC64_BITS_TO_MV*(0.02)*TIME_DISCRETIZATION // 1 count - 4.88 fC

#define DEFAULT_SAMPLE_NUMBER 1024

// for time processing by convolution
#define TRIGGER_DEVICE        0xa7b54bd   // 0xa7af865 0xa7b54bd 80c635b the device for which the obtained time will be subtracted from each channel
#define TRIGGER_CHANNEL       2           // the channel for which the obtained time will be subtracted from each channel

#define KERNEL_CREATION       0                 // 1 - create kernel / 0 - don't create
#define KERNEL_TYPE           1                 // 0 - rough kernel / 1 - precised kernel
#define KERNEL_R_FILENAME     "kernel_r"        // rough kernel file name
#define KERNEL_P_FILENAME     "kernel_p.root"   // precised kernel file name
#define KERNEL_RANGE_START    287       // first point of kernel waveform
#define KERNEL_RANGE_END      293       // last  point of kernel waveform
#define KERNEL_INVERSION      -1        // 1 - not inverted / -1 - inverted
#define KERNEL_SMOOTH         1         // 1 - smooth enabled / 0 - disabled
#define KERNEL_DEVICE_FOR_SPLITTER 0xa7af865  // adc device serial number for splitter -  precised
#define KERNEL_CHANNEL_GROUP_FOR_SPLITTER 0   // 0 - 1st 16 channels, 16 - 2nd 16 channels, 32 - 3rd 16 channels, 48 - 4th 16 channels
#if DEVICE_TYPE==DEVICE_ID_ADC64
  #define KERNEL_NUMBER         10      // subdiscretization in ns - each point will be divided into 10 points
  #define KERNEL_SMOOTH_RANGE   10        // smooth expanded data array using +/- this value in points for precised kernel
#elif DEVICE_TYPE==DEVICE_ID_TQDC
  #define KERNEL_NUMBER         8       // for TQDC
  #define KERNEL_SMOOTH_RANGE   8        // smooth expanded data array using +/- this value in points for precised kernel
#endif

#define KERNEL_MAXNUM_IN_FILE 99
#define KERNEL_ID_DATA        0         // use the kernel with this id for data channels - it's index of precised kernel array
#define KERNEL_ID_TRIGGER     2         // this kernel will be used for TRIGGER_CHANNEL - it's index of precised kernel array
#define KERNEL_ID_NUMBER      2         // while only two ids reserved - KERNEL_ID_DATA and KERNEL_ID_TRIGGER
#define KERNEL_GATE_FILENAME  "gatepk.cfg"
#define KERNEL_RGATE_FILENAME  "gaterk.cfg"
#define KERNEL_CONV_RANGE_EXTENSION_START  -100  // -the value from kernel gate - cut range to speed up convolution - only for precised kernel
#define KERNEL_CONV_RANGE_EXTENSION_END    100   // +the value from kernel gate - cut range to speed up convolution - only for precised kernel
#define KERNEL_DEFAULT_ADC_FOR_SPLITTER 0 // index of ADC that will be used for precised kernel creation with a splitter by default

#define MONITOR_NEVENTS       50
#define MONITOR_AUTORESET     10000      // All the histos will be cleared after this number of cycles
#define MONITOR_DATADIR       "."
#define MONITOR_PLOT_NUMBER   10         // Number of monitor plots types [see struct MON_PLOT for details]
#define MONITOR_FILE_CHECK    10         // Number of cycles to wait before checking the last data file

#define CONV_RANGE_START      10       // cut range to speed up convolution - only for rough kernel
#define CONV_RANGE_END        10       // cut range to speed up convolution - only for rough kernel

// for time processing by fit
#define FRONT_LEVEL_LO 0.1               // in fraction
#define FRONT_LEVEL_HI 0.9               // in fraction
#define FIT_DISCRETISATION_IMPOVEMENT 1  // 0 - disabled / 1 - enabled by factor of KERNEL_NUMBER

#if DEVICE_TYPE==DEVICE_ID_ADC64
  #if SCALE_REPRESENTATION==0
    #define NUMBER_OF_BINS      2000
    #define BIN_FIRST_CHARGE    -1000
    #define BIN_LAST_CHARGE     3000
  #elif SCALE_REPRESENTATION==1
    #define NUMBER_OF_BINS      1000
    #define BIN_FIRST_CHARGE    -100
    #define BIN_LAST_CHARGE     900
  #endif
#elif DEVICE_TYPE==DEVICE_ID_TQDC
  #if SCALE_REPRESENTATION==0
    #define NUMBER_OF_BINS      5000
    #define BIN_FIRST_CHARGE    -1000
    #define BIN_LAST_CHARGE     9000
  #elif SCALE_REPRESENTATION==1
    #define NUMBER_OF_BINS      2000
    #define BIN_FIRST_CHARGE    -1000
    #define BIN_LAST_CHARGE     3000
  #endif
#endif
#define NUMBER_OF_TIME_BINS     2048*10
#define NUMBER_OF_TIMEDIFF_BINS 500
#define TIMEDIFF_BIN_FIRST     -2500
#define TIMEDIFF_BIN_LAST       2500
#define TIME_BIN_FACTOR     1
#define TIMEFIT_BIN_FACTOR  10
#define TIMEDIFF_BIN_FACTOR 1*2.5

#define COMMENT_CHAR   "#"
#define GATE_FILENAME  "charge.gt"
#define GATE_PED_START 100
#define GATE_PED_END   110
#define GATE_SIG_START 310
#define GATE_SIG_END   320
#define GATE_SHIFT     10   // shift of the signal gate interval to compute dark spectrum [for D0 estimation]
#define PEDESTAL_SHIFT 0	  // shift of pedestal to the pointed channel
#define PEDESTAL_SUBTRACTION 1  // pedestal subtraction [0-disabled,1-enabled]
#define DIGI_PHOSPHOR_COEF 10   // how often to draw single waveforms - expl.: 10 - draw each 10th waveform

#define LOG_FILENAME          "log.txt"
#define LOG_DATA_FILENAME     "data"
#define LOG_MONITOR_FILENAME  "monitor"
#define LOG_ASCII_STATE       0
#define LAST_USED_DATA_DIR    "./"  // current directory by default

#define BASELINE_RANGE_OFFSET 5        // samples from sample @ 10% amplitude of signal
#define BASELINE_VARIATION_LIMIT  3*64 // all the points within this (+/-)value will be treated as baseline
#define PEAK_FIND_THR         BASELINE_VARIATION_LIMIT*100 // default threshold to find peaks

#define GEN_GATE_TYPE   'c'       // generate charge gate file by default [c-charge gate|k-kernel gate]

typedef struct {
  UInt_t    devsn;                                   // device SN
  UInt_t    ch;                                      // channel
  Char_t    type;                                    // module type: LCM/ArcLight l/a
  Char_t    side;                                    // front/rear
  Char_t    plane;                                   // left/right
  UInt_t    couple;                                  // coupled channel
  Int_t     inv;                                     // signal polarity value 1 - negative polarity, -1 - positive polarity
  Float_t   delay;                                   // ADC channels calibration data
  Char_t    eshape;                                  // E-shape order number -- (6 channels)
  Char_t    sum;                                     // 's': sum of 6 channels; 'c': single channel
} ADCSETTINGS;

typedef struct {
  char      gatefile[256];                            // gate filename
  UInt_t    evtlims[2];                               // first and last event (average)
  UInt_t    evtlims_spec[2];                          // first and last event (time and charge spectrum)
  uint32_t  trigger[2];                               // trigger device and channel
  Int_t     xrange[2];                                // histo range (first and last bin)
  Int_t     bins[3];                                  // histo binning (first, last, nbins)
  Int_t     peakthr;                                  // peak finder threshold
  UShort_t  kerneltype;                               // kernel type (0 - rough, 1 - precised)
  uint32_t  splitter[2];                              // splitter device and channel
  UShort_t  fitpars[2];                               // charge spectrum fit: peak threshold, n sigma
  UInt_t    convrange[2];                             // time spectrum: -/+
  UShort_t  kernelid[2];                              // precised kernel -- data ID and trigger ID
  UShort_t  pedshift;                                 // pedestal shift
  Int_t     offset;                                   // baseline offset
  UShort_t  pedsubtr;                                 // pedestal subtraction (0 - disabled, 1 - enabled)
  uint32_t  digi_phosphor_coef;                       // how offten to draw single waveforms - expl.: 10 - draw each 10th waveform
  char      last_used_data_dir[512];                            // last used data directory
  void init(){
    sprintf(gatefile, "%s", GATE_FILENAME);
    sprintf(last_used_data_dir, "%s", LAST_USED_DATA_DIR);
    evtlims[0] = 1;
    evtlims[1] = 1000;

    evtlims_spec[0] = 1;
    evtlims_spec[1] = 10000;

    trigger[0] = TRIGGER_DEVICE;
    trigger[1] = TRIGGER_CHANNEL;

    xrange[0] = -100;
    xrange[1] = 900;

    bins[0] = BIN_FIRST_CHARGE;
    bins[1] = BIN_LAST_CHARGE;
    bins[2] = NUMBER_OF_BINS;

    peakthr = PEAK_FIND_THR;
    kerneltype = KERNEL_TYPE;

    splitter[0] = KERNEL_DEVICE_FOR_SPLITTER;
    splitter[1] = KERNEL_CHANNEL_GROUP_FOR_SPLITTER;

    fitpars[0] = 50; fitpars[1] = 2;

    convrange[0] = CONV_RANGE_START;
    convrange[1] = CONV_RANGE_END;

    kernelid[0] = KERNEL_ID_DATA;
    kernelid[1] = KERNEL_ID_TRIGGER;

    pedshift = PEDESTAL_SHIFT;
    offset   = SIGNAL_OFFSET;
    pedsubtr = PEDESTAL_SUBTRACTION;
    digi_phosphor_coef = DIGI_PHOSPHOR_COEF;
  }
} VIEWERCONFIG;

typedef struct {                                  // ADC(Device) meta data
   uint32_t ev;		                               	// total event number
   // uint32_t evch[CHANNEL_NUMBER];                 // individual event for each channel
   uint16_t sn;                                   // number of samples
   uint32_t devsn;                                // serial number of device
   uint32_t devid;                                // device ID
   uint8_t  devorn;                               // device order number
   char dir[512];                                 // adc file path
	 char file[256];	             	              	// adc filename
   uint64_t rectime;                              // start time of a recording data
   std::bitset<64> ach;                           // active channels
   std::vector<ADCSETTINGS> cfg;                  // vector of ADCSETTINGS structures, 64 structures for one device
} ADCS;

typedef struct {                                  // waveform parameters
  Float_t start;                                  // signal start time
  Float_t baseline;                               // mean value of base line
  Float_t amp;                                    // signal amplitude
  Float_t amp10;                                  // 10% of the signal amplitude
  Float_t amp90;                                  // 10% of the signal amplitude
  Int_t amp_s;                                    // sample number of signal amplitude
  Int_t amp10_ls;                                  // sample number of 10% of the signal amplitude from the left side of signal
  Int_t amp90_ls;                                  // sample number of 90% of the signal amplitude from the left side of signal
  Int_t amp10_rs;                                  // sample number of 10% of the signal amplitude from the right side of signal
  Int_t amp90_rs;                                  // sample number of 90% of the signal amplitude from the right side of signal
  Float_t len;                                    // signal length 0.1 - 0.1
  Float_t rise;                                   // signal rise time 0.1 - 0.9
  Float_t fall;                                   // signal fall time 0.9 - 0.1
  Int_t   polarity;                               // signal polarity -1 - negative, +1 - positive
} WFPARS;

typedef struct {
  UInt_t n;                                       // peak order number
	Int_t maxpos;                                     // position of maximum
  Int_t imp_maxpos;                                 // position of maximum with improved discretization
  Float_t amp;                                      // amp of maximum
  Float_t imp_amp;                                  // amp of maximum with improved discretization
	Int_t first;                                      // position of input point
	Int_t last;                                       // position of output point
  WFPARS pars;
} WFPEAKS;

struct WFPEAKS_CMP {
  bool operator()(const WFPEAKS a, const WFPEAKS b) const {
      return a.amp < b.amp; };
};

struct DATA {
  uint32_t ev;		                               // event number
  uint32_t evch[CHANNEL_NUMBER];                 // individual bad event counter for each channel
  uint16_t sn;                                   // number of samples
  uint32_t devsn;                                // serial number of device
  uint32_t devid;                                // device ID
  uint8_t  devorn;                               // device order number
  uint64_t utime;                                // unix time in milliseconds from system call
  uint32_t taisec;                               // trigger time in seconds
  uint32_t tainsec;                              // trigger time in nanoseconds
  bool     chstate[CHANNEL_NUMBER];              // channel's states
  uint64_t wfts[CHANNEL_NUMBER];                 // time for waveform
  std::vector<int32_t> wf[CHANNEL_NUMBER];	   	 // waveforms data
  int8_t   pol[CHANNEL_NUMBER];                  // signal polarity
  std::bitset<CHANNEL_NUMBER> chis;              // channel presents in event [false - complite event, true - incomplite] some channels might be missed in the mstream block
  bool     valid;                                  // event is corrupted or incomplete
  DATA()  { clear(); }
  void clear()
  {
    ev      = 0;
    sn      = 0;
    devsn   = 0;
    devid   = 0;
    devorn  = 0;
    utime   = 0;
    taisec  = 0;
    tainsec = 0;
    valid   = true;
    chis.set();

    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      chstate[ch] = false;
      evch[ch]    = 0;
      wfts[ch]    = 0;
      pol[ch]     = 0;
      wf[ch].clear();
    }
  }
};

struct INTEGRAL {
  Double_t aped;      // baseline position
  Double_t asig;      // average gated amplitude
  Double_t amp;      // gated amplitude excluding baseline
  Double_t aped_sh;   // shifted baseline position                             // not implemented
  Double_t asig_sh;   // shifted average gated amplitude
  Double_t amp_sh;   // shifted gated amplitude excluding baseline

  Double_t cped;      // gated charge of pedestal
  Double_t csig;      // gated charge of signal
  Double_t cint;      // gated charge of signal excluding pedestal
  Double_t cped_sh;   // shifted gated charge of pedestal                      // not implemented
  Double_t csig_sh;   // shifted gated charge of signal
  Double_t cint_sh;   // shifted gated charge of signal excluding pedestal
  Double_t k;      // (signal gate range)/(pedestal gate range) ratio
  int    err;      // errors [1-no input data,2-no gate found for this board,3-no meta found for this board]

  INTEGRAL() { clear(); }
  void clear()
  {
    aped    = 0;
    asig    = 0;
    amp     = 0;
    aped_sh = 0;
    asig_sh = 0;
    amp_sh  = 0;

    cped    = 0;
    csig    = 0;
    cint    = 0;
    cped_sh = 0;
    csig_sh = 0;
    cint_sh = 0;
    k       = 0;
    err     = 0;
  }
};

typedef struct {
  uint32_t sn;                                   // number of samples
  uint8_t  devorn;                               // device order number
  uint32_t devsn;                                // device serial number
  bool     chstate[CHANNEL_NUMBER];              // channel's states
  std::vector<float> wf[CHANNEL_NUMBER];	   	 	 // waveforms data
} KERNEL;

typedef struct {
  uint32_t sn;                                   // number of samples
  UInt_t   nk;                                   // number of kernels in the file
  Int_t    kid[KERNEL_MAXNUM_IN_FILE];           // kernel ids "_idXX"
  std::vector<float> wf[KERNEL_MAXNUM_IN_FILE];	 // waveforms data
} KERNEL_PRECISED;

typedef struct {
  uint32_t devsn;                                 // device serial number
  uint32_t ch;                                    // channel number
  uint32_t ped_start;                             // first point of pedestal
  uint32_t ped_end;                               // last point of pedestal
  uint32_t sig_start;                             // first point of signal
  uint32_t sig_end;                               // last point of signal
} GATE;

typedef struct {
  UInt_t start[KERNEL_MAXNUM_IN_FILE];            // first point of signal
  UInt_t end[KERNEL_MAXNUM_IN_FILE];              // last point of signal
} PKGATE;

typedef struct {
  float x;
  float y;
} stFITARR;

typedef struct {
  TH1F *charge[CHANNEL_NUMBER];
  TH1F *time[CHANNEL_NUMBER];
  TH1F *timediff[CHANNEL_NUMBER];
  TH1F *ptime[CHANNEL_NUMBER];
  TH1F *ptimediff[CHANNEL_NUMBER];
  TH1F *rtime[CHANNEL_NUMBER];
  TH1F *rtimediff[CHANNEL_NUMBER];
} SPECTRA;

typedef struct {
  UInt_t devsn;
  UInt_t ch;
  TObject *plot;                  // TGraph or TH1F objects
  UChar_t type;                   // 0 - amp, 1 - charge, 2 - time --- history profiles (for monitoring)
                                  // 3 - wave, 4 - charge, 5 - diff time,
                                  // 6 - time by fit, 7 - time by convolution (precised),
                                  // 8 - time by convolution (rough)
                                  // 9 - shifted charge (for dark noise estimation) --- histograms
  std::bitset<MONITOR_PLOT_NUMBER> isExist;        // bitset with existence flags
} MON_PLOT;


class  AFISettings {
  public:
    uint32_t event;
    std::string file;         // data file name
    std::string monitor_datadir;  // data folder for looking files for monitor processing
    std::string gate_preset;  // string consists pedestal and signal gate ranges
    std::string kernel_gate_preset;
    std::string kernel_channels_preset; // which channels should be used for precised kernel
    std::string kernel_ids_in_use;
    std::string workdir;
    uint32_t uikids_in_use[KERNEL_ID_NUMBER];
    uint32_t uiktrig_device;
    uint32_t uiktrig_channel;
    UChar_t gen_gate_type;
    char mode;
    uint32_t guimode;
    uint32_t kernel_creation;
    uint32_t kernel_type;
    float peakthr;          // threshold to find peaks in the waveform
    bool write_data_log;
    bool   rwf_on;
    VIEWERCONFIG config;

    AFISettings()
    {
      guimode = GUI_MODE;
      event = 1;
      file = "data.data";
      monitor_datadir = MONITOR_DATADIR;
      gate_preset = "100:110:310:320";  // first pedestal gate range and then signal gate range
      kernel_gate_preset = "310:320";   // signal location
      kernel_channels_preset = "0xa7af865_ch0.1.2.3.4.5.6.7.8.9";
      kernel_ids_in_use = "A7B54BD_ch0_trig0:data0";
      workdir = ".";
      gen_gate_type  = GEN_GATE_TYPE;
      uikids_in_use[0] = KERNEL_ID_TRIGGER;
      uikids_in_use[1] = KERNEL_ID_DATA;
      uiktrig_device  = TRIGGER_DEVICE;
      uiktrig_channel = TRIGGER_CHANNEL;
      mode = 's';           // s - show single waveform
      kernel_creation = KERNEL_CREATION;
      kernel_type = KERNEL_TYPE;
      peakthr = -100;
      write_data_log = LOG_ASCII_STATE;
      rwf_on      = RLOGGER_STORE_RGRAH; // do not store waveforms* file by default
      config.init();
    };
};
#endif
