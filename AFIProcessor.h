#ifndef AFI_PROCESSOR_H
#define AFI_PROCESSOR_H

class AFIDecoder;
class AFIGUI;
class AFISettings;

typedef struct{
  double max;
  double first;
  double last;
} stPEAKS;

extern "C" {
    int start_processing();
    void print_usage(char** argv);
    bool process_options(int argc, char** argv);

    void looking_for_devices();
    void start_monitoring();
    void stop_monitoring();
    void clear_plots();
    void process_monitoring(UInt_t trigChannel, UInt_t cycle,
      UInt_t devsn=0xa7b54bd, UChar_t channel=0, UInt_t nevents=MONITOR_NEVENTS);
    Int_t get_last_datafile(char* filename);
    void get_peaks();
    void save_config();
    void fit_arfunc();
    void fit_single_gauss();
    void fit_double_gauss();

    bool make_connections();
    bool connect_monitor();

    void set_meta_data();
    void set_gate_file(const char*);

    // Loading gates and kernels
    Int_t load_charge_gate();
    bool load_pgate();
    bool load_rgate();
    bool load_pkernel(Bool_t showKernel=false);
    bool load_rkernel();
    void reset_kernel_load_state();

    void viewer_histos_init();
    // Draw functions
    void draw_charge_spectrum();        // Draw charge spectrum
    void draw_time_spectrum_fit();      // Draw tspectrum based on linear fit
    void draw_time_spectrum_rconv();    // Draw tspectrum based on convolution with rough kernel
    void draw_time_spectrum_pconv();    // Draw tspectrum based on convolution with precised kernel
    void draw_avg_wave(Bool_t bKernelCreation = false);          // Draw average waveform
    void draw_single_wave();            // Draw single waveform
    void processall();                  // build average, charge, time by fit

    // Render
    TGraph * show_single_waveform(uint8_t devorn, uint8_t ch, uint16_t sn, std::vector<DATA> *vData=NULL);
    TGraph * show_avg_waveform(uint8_t devorn, uint8_t ch, uint16_t sn, uint32_t nevents, std::vector<DATA> *vAveData);
    TGraph * show_rkernel(uint8_t devorn, uint8_t ch, uint16_t sn, std::vector<KERNEL> *vKernel=NULL, int start=0, int end=DEFAULT_SAMPLE_NUMBER);
    TGraph * show_pkernel(UInt_t kind, uint16_t sn, std::vector<KERNEL_PRECISED> *vKernel=NULL, int start=0, int end=DEFAULT_SAMPLE_NUMBER);

    extern AFISettings settings;
    extern AFIGUI *gui;
};

#endif
