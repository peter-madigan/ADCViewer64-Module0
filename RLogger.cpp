#include "RLogger.h"
#include "TFile.h"
#include "TTree.h"
#include "TObject.h"
// #include <cstring>

//______________________________________________________________________________
template <class T> RLogger<T>::RLogger(const char *filename)
{
  sprintf(cFileName, "%s", filename);
  for (int t=0; t<TN; t++)
  {
    bThreadUsed[t] = true;
    // rLog[t].Init();
    vRLog[t].clear();
  }
}

//______________________________________________________________________________
template <class T> Bool_t RLogger<T>::Fill(UInt_t threadNumber, T data)
{
  if (threadNumber>=TN)
  {
    printf("Achtung: Thread index[%d] out of range[%d]!\n",threadNumber,TN);
    return false;
  }
  UInt_t t = threadNumber;
  if (bThreadUsed[t])
  {
    vRLog[t].push_back(data);
  }
  return true;
}

void CopyResults(RLOG *log,RLOG *from)
{
  log->event = from->event;
  log->sn    = from->sn;
  log->cycle = from->cycle;
  log->utime_ms = from->utime_ms;
  log->tai_ns = from->tai_ns;
  log->taisec = from->taisec;
  log->tainsec = from->tainsec;
  // log->runtag = from->runtag;
  sprintf(log->runtag, "%s", from->runtag);
  // printf("****From: %s To: %s\n", from->runtag, log->runtag);
  for (int ch=0; ch<CHANNEL_NUMBER; ch++)
  {
    for (int j=0; j<DN; j++)
      log->data[j][ch] = from->data[j][ch];
    log->wfts[ch] = from->wfts[ch];
  }
}

void CopyResults(RWAVE *log,RWAVE *from)
{
  log->event = from->event;
  log->sn    = from->sn;
  log->ch    = from->ch;
  log->utime_ms = from->utime_ms;
  log->tai_ns = from->tai_ns;
  // log->graph_ptr = from->graph_ptr;
  log->th1s_ptr = from->th1s_ptr;
}

//______________________________________________________________________________
template <class T> Bool_t RLogger<T>::Write(Bool_t bWritingFinished)
{
  T       rLogRes;
  std::vector< T > vrLogRes;
  TFile   *LogDataFile;
  TTree   *LogDataTree;
  rLogRes.InitData();
  rLogRes.InitBranches();
  //
  LogDataFile = new TFile(Form("%s.root", cFileName), "UPDATE");
  LogDataTree = (TTree*)LogDataFile->Get(Form("%s", rLogRes.treename.c_str()));
  if (!LogDataTree)
  {
    // printf("Achtung: The tree doesn't exist!\n");
    LogDataTree = new TTree(Form("%s", rLogRes.treename.c_str()),Form("%s", rLogRes.treename.c_str()));
    for ( auto &i : rLogRes.vinfo )
    {
      if ( strcmp((const char*)i.ft,"TGraph") == 0 || strcmp((const char*)i.ft,"TH1S") == 0)
        LogDataTree->Branch((const char*)i.fn,(const char*)i.ft,i.ad);
      else
        LogDataTree->Branch((const char*)i.fn,i.ad,(const char*)i.ft);
    }
  }
  else
  {
    for ( auto &i : rLogRes.vinfo )
      LogDataTree->SetBranchAddress((const char*)i.fn,i.ad);
  }


  for (int t=0; t<TN; t++)
  {
    for (auto &itL : vRLog[t])
    {
      vrLogRes.push_back(itL);
    }
  }

  std::sort( vrLogRes.begin(), vrLogRes.end(), [&](T &a, T &b){return a<b;} );
  for (auto &itL : vrLogRes)
  {
    CopyResults((T*)&rLogRes,(T*)&itL);
    LogDataTree->Fill();
  }

  LogDataFile->Write(0, TObject::kOverwrite);
  for (int t=0; t<TN; t++)
    vRLog[t].clear();
  vrLogRes.clear();

  if (bWritingFinished)
  {
    LogDataFile->Print();
    printf("All the data have been saved to %s.root\n\n", cFileName);
  }
  else
  {
    printf("%d events were dumped to %s.root\n", TN*RLOGGER_STORE_RATE,cFileName);
  }
  LogDataFile->Close();
  delete LogDataFile;

  return true;
}

template class RLogger<RLOG>;
template class RLogger<RWAVE>;
