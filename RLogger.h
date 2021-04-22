#include <vector>
#include <cstdio>
#include "AFISettings.h"
#include "TGraph.h"
#ifndef RLOGGER_H
#define RLOGGER_H

// CAUTION!
// Number of threads and user data structure (RLOG) must be defined by user before
// the implementation!
#define TN  THREAD_NUMBER       // Redefinition of thread number macro for logger
#define CN  CHANNEL_NUMBER      //
#define DN  4                   // number of data arrays
#define TREENAME_RLOG  "rlog"   // Tree name
#define TREENAME_RWAVE "rwf"    // Tree name

struct RWAVE {
  UInt_t      event    = 0;        // event number
  UInt_t      sn       = 0;        // serial number
  UInt_t      ch       = 0;        // channel number
  ULong64_t   utime_ms = 0;        // unix time in ms // from event time header
  ULong64_t   tai_ns  = 0;       // time in ns // taisec*1e9+tainsec - without WR - it's time from the board has been started
  // TGraph      *graph_ptr = nullptr;          // pointer to graph object
  TH1S        *th1s_ptr = nullptr;          // pointer to histo object

  // META DATA - info about struct - introspection
  struct RLOG_INFO {
      void       *ad;     // address of struct field in memory
      const char *fn;     // name of struct field
      const char *ft;     // ROOT Branch type: '/I' - int, '/F' - float and so on
  };
  std::vector<RLOG_INFO> vinfo;
  std::string treename = TREENAME_RWAVE;         // treename of RWAVE tree
  // constructor of struct
  void InitData()
  {
    event    = 0;
    sn       = 0;
    ch       = 0;
    utime_ms = 0;
    tai_ns = 0;
  };
  void InitBranches()
  {
    RLOG_INFO info;
    info.ad = &event; info.fn = "event"; info.ft = "event/i"; vinfo.push_back(info);
    info.ad = &sn;    info.fn = "sn";    info.ft = "sn/i";    vinfo.push_back(info);
    info.ad = &ch;    info.fn = "ch";    info.ft = "ch/i";    vinfo.push_back(info);
    info.ad = &utime_ms; info.fn = "utime_ms";  info.ft = "utime_ms/l"; vinfo.push_back(info);
    info.ad = &tai_ns; info.fn = "tai_ns";  info.ft = "tai_ns/l"; vinfo.push_back(info);
   // info.ad = &graph_ptr; info.fn = "graph_ptr"; info.ft = "TGraph"; vinfo.push_back(info);
    info.ad = &th1s_ptr; info.fn = "th1s_ptr"; info.ft = "TH1S"; vinfo.push_back(info);
  };
};

struct RLOG {
  ULong64_t   utime_ms = 0;       // unix time in ms // from event time header
  ULong64_t   tai_ns  = 0;       // unix time in ns // taisec*1e9+tainsec - without WR - it's time from the board has been started
  ULong64_t   taisec = 0;        // unix time // taisec, need to *5/8 to bring to real time space
  ULong64_t   tainsec = 0;       // unix time // tainsec, need to *5/8 to bring to real time space
  char        runtag[64];        // date_time (i.e. 20191011_173429)
  UInt_t      sn     = 0;        // serial number
  UInt_t      cycle  = 0;        // cycle number (valid for monitoring only)
  UInt_t      event  = 0;        // event number
  Float_t     data[DN][CN];       // data to be written into the tree: [ 0 amp | 1 integral | 2 time ][CN]
  ULong64_t   wfts[CN];          // waveform arrival time

  // META DATA - info about struct
  struct RLOG_INFO {
      void       *ad;     // address of struct field in memory
      const char *fn;     // name of struct field
      const char *ft;     // ROOT Branch type: '/I' - int, '/F' - float and so on
  };
  std::vector<RLOG_INFO> vinfo;
  std::string treename = TREENAME_RLOG;         // treename of RLOG tree
  // constructor of struct
  void InitData()
  {
    for (int i=0; i<CN; i++)
    {
      for (int j=0; j<DN; j++)
        data[j][i] = 0;
      wfts[i] = 0;
    }
  };

  void InitBranches()
  {
    RLOG_INFO info;
    info.ad = &utime_ms; info.fn = "utime_ms"; info.ft = "utime_ms/l"; vinfo.push_back(info);
    info.ad = &tai_ns; info.fn = "tai_ns"; info.ft = "tai_ns/l"; vinfo.push_back(info);
    info.ad = &taisec; info.fn = "taisec"; info.ft = "taisec/l"; vinfo.push_back(info);
    info.ad = &tainsec; info.fn = "tainsec"; info.ft = "tainsec/l"; vinfo.push_back(info);
    info.ad = &runtag; info.fn = "runtag"; info.ft = "runtag/C"; vinfo.push_back(info);
    info.ad = &sn;    info.fn = "sn";    info.ft = "sn/i";    vinfo.push_back(info);
    info.ad = &cycle; info.fn = "cycle"; info.ft = "cycle/i"; vinfo.push_back(info);
    info.ad = &event; info.fn = "event"; info.ft = "event/i"; vinfo.push_back(info);
    char cBranchNameCH[2148] = "ch00/F";
    char cBranchNameCH_wfts[2148] = "ch00/l";
    for (int ch=1; ch<CHANNEL_NUMBER; ch++){
      sprintf(cBranchNameCH, "%s:ch%02d", cBranchNameCH, ch);
      sprintf(cBranchNameCH_wfts, "%s:ch%02d", cBranchNameCH_wfts, ch);
    }
    info.ad = data[0]; info.fn = "amplitude"; info.ft = Form("%s", cBranchNameCH); vinfo.push_back(info);
    info.ad = data[1]; info.fn = "integral";  info.ft = Form("%s", cBranchNameCH); vinfo.push_back(info);
    info.ad = data[2]; info.fn = "integral_sh";  info.ft = Form("%s", cBranchNameCH); vinfo.push_back(info);
    info.ad = data[3]; info.fn = "time";      info.ft = Form("%s", cBranchNameCH); vinfo.push_back(info);

    info.ad = wfts;    info.fn = "wfts";      info.ft = Form("%s", cBranchNameCH_wfts); vinfo.push_back(info);
  };
};

inline bool operator==(const RLOG& lhs, const RLOG& rhs){ return lhs.event==rhs.event; }
inline bool operator!=(const RLOG& lhs, const RLOG& rhs){ return !operator==(lhs, rhs); }
inline bool operator< (const RLOG& lhs, const RLOG& rhs){ return lhs.event<rhs.event; }
inline bool operator> (const RLOG& lhs, const RLOG& rhs){ return operator<(rhs, lhs); }
inline bool operator<=(const RLOG& lhs, const RLOG& rhs){ return !operator>(lhs, rhs); }
inline bool operator>=(const RLOG& lhs, const RLOG& rhs){ return !operator<(lhs, rhs); }

inline bool operator==(const RWAVE& lhs, const RWAVE& rhs){ return lhs.event==rhs.event; }
inline bool operator!=(const RWAVE& lhs, const RWAVE& rhs){ return !operator==(lhs, rhs); }
inline bool operator< (const RWAVE& lhs, const RWAVE& rhs){ return lhs.event<rhs.event; }
inline bool operator> (const RWAVE& lhs, const RWAVE& rhs){ return operator<(rhs, lhs); }
inline bool operator<=(const RWAVE& lhs, const RWAVE& rhs){ return !operator>(lhs, rhs); }
inline bool operator>=(const RWAVE& lhs, const RWAVE& rhs){ return !operator<(lhs, rhs); }

template <class T>  class RLogger
{
private:
  char                cFileName[512];
  Bool_t              bThreadUsed[TN];
  std::vector<T>      vRLog[TN];

public:
  RLogger(const char *filename="rlog");
  virtual ~RLogger() {}
  Bool_t  Fill(UInt_t threadNumber, T data);
  Bool_t  Write(Bool_t bWritingFinished=false);
  void    SetFileName(const char *filename) { sprintf(cFileName, "%s", filename); }
};
#endif
