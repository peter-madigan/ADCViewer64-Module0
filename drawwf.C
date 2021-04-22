#define rwf_cxx
#include <TH2.h>
#include <TStyle.h>
#include <TCanvas.h>
#include <stdio.h>
#include <libgen.h>
#include <time.h>

//////////////////////////////////////////////////////////
// This class has been automatically generated on
// Wed Apr 14 09:12:04 2021 by ROOT version 6.22/06
// from TTree rwf/rwf
// found on file: rwf_0a7b54bd_20210404_093828.data.root
//////////////////////////////////////////////////////////

#ifndef rwf_h
#define rwf_h

#include <TROOT.h>
#include <TChain.h>
#include <TFile.h>

// Header file for the classes stored in the TTree if any.
#include "TH1.h"

class rwf {
public :
   TTree          *fChain;   //!pointer to the analyzed TTree or TChain
   Int_t           fCurrent; //!current Tree number in a TChain

// Fixed size dimensions of array or collections stored in the TTree if any.

   // Declaration of leaf types
   UInt_t          event;
   UInt_t          sn;
   UInt_t          ch;
   ULong64_t       utime_ms;
   ULong64_t       tai_ns;
   TH1S            *th1s_ptr;

   // List of branches
   TBranch        *b_event;   //!
   TBranch        *b_sn;   //!
   TBranch        *b_ch;   //!
   TBranch        *b_utime_ms;   //!
   TBranch        *b_tai_ns;   //!
   TBranch        *b_th1s_ptr;   //!

   rwf(const char *fname);
   Long64_t * rindex; //! transient Tree index array, where entries are sorted by time

   virtual ~rwf();
   virtual Int_t    Cut(Long64_t entry);
   virtual Int_t    GetEntry(Long64_t entry);
   virtual Long64_t LoadTree(Long64_t entry);
   virtual void     Init(TTree *tree);
   virtual void     Loop(Long64_t utime_ms, Long64_t time_from_PPS_us, Int_t window_us);
   virtual Bool_t   Notify();
   virtual void     Show(Long64_t entry = -1);
   virtual void     BuildIndex(const char *major,const char *minor);

};

#endif

#ifdef rwf_cxx
TTree * tree=0;
rwf::rwf(const char *fname) : fChain(0) 
{
// if parameter tree is not specified (or zero), connect the file
// used to generate this class and read the Tree.
   if (tree == 0) {
      TFile *f = (TFile*)gROOT->GetListOfFiles()->FindObject(fname);
      if (!f || !f->IsOpen()) {
         f = new TFile(fname);
      }
      f->GetObject("rwf",tree);

   }
   Init(tree);
}

rwf::~rwf()
{
   if (!fChain) return;
   delete fChain->GetCurrentFile();
}

Int_t rwf::GetEntry(Long64_t entry)
{
// Read contents of entry.
   if (!fChain) return 0;
   return fChain->GetEntry(entry);
}
Long64_t rwf::LoadTree(Long64_t entry)
{
// Set the environment to read one entry
   if (!fChain) return -5;
   Long64_t centry = fChain->LoadTree(entry);
   if (centry < 0) return centry;
   if (fChain->GetTreeNumber() != fCurrent) {
      fCurrent = fChain->GetTreeNumber();
      Notify();
   }
   return centry;
}

void rwf::Init(TTree *tree)
{
   // The Init() function is called when the selector needs to initialize
   // a new tree or chain. Typically here the branch addresses and branch
   // pointers of the tree will be set.
   // It is normally not necessary to make changes to the generated
   // code, but the routine can be extended by the user if needed.
   // Init() will be called many times when running on PROOF
   // (once per file to be processed).

   // Set object pointer
   th1s_ptr = 0;
   // Set branch addresses and branch pointers
   if (!tree) return;
   fChain = tree;
   fCurrent = -1;
   fChain->SetMakeClass(1);

   fChain->SetBranchAddress("event", &event, &b_event);
   fChain->SetBranchAddress("sn", &sn, &b_sn);
   fChain->SetBranchAddress("ch", &ch, &b_ch);
   fChain->SetBranchAddress("utime_ms", &utime_ms, &b_utime_ms);
   fChain->SetBranchAddress("tai_ns", &tai_ns, &b_tai_ns);
   fChain->SetBranchAddress("th1s_ptr", &th1s_ptr, &b_th1s_ptr);
   Notify();
   BuildIndex("utime_ms","tai_ns");
}

Bool_t rwf::Notify()
{
   // The Notify() function is called when a new file is opened. This
   // can be either for a new TTree in a TChain or when when a new TTree
   // is started when using PROOF. It is normally not necessary to make changes
   // to the generated code, but the routine can be extended by the
   // user if needed. The return value is currently not used.

   return kTRUE;
}

void rwf::Show(Long64_t entry)
{
// Print contents of entry.
// If entry is not specified, print current entry
   if (!fChain) return;
   fChain->Show(entry);
}
Int_t rwf::Cut(Long64_t entry)
{
// This function may be called from Loop.
// returns  1 if entry is accepted.
// returns -1 otherwise.
   return 1;
}
#endif // #ifdef rwf_cxx

rwf * r;
TTree * tr;
TEventList *elist00;
TEventList *elist;
  Int_t npassed;
  TCanvas *c;
  TCanvas *ct;
  ULong64_t t0_175854781, t0_175780172;


  Long64_t trel;
  TH1S * ch;
  Int_t bins; 
  char st[256];
  char st2[256];
  EColor color=kBlack;

 TH1 *hm =0;
 TH1 *hp =0;

  
  void FilterSinus(TH1S * h)
{
 int n=h->GetXaxis()->GetNbins();
 TVirtualFFT::SetTransform(0);
 hm = h->FFT(hm, "MAG");
 hp = h->FFT(hp, "PH");
 TVirtualFFT *fft = TVirtualFFT::GetCurrentTransform();
 Double_t *re_full = new Double_t[n];
 Double_t *im_full = new Double_t[n];
 fft->GetPointsComplex(re_full,im_full);
int i1,i2;
if(n==2048) { i1=198; i2=212;}
if(n==1024) { i1=99; i2=106;}
if(n==256) { i1=24; i2=27;}

//for(int i=99; i<=106; i++) 
for(int i=i1; i<=i2; i++) 
  { 
/*    im_full[i]=im_full[98];
    im_full[n-i]=im_full[98];
    re_full[i]=re_full[98];
    re_full[n-i]=re_full[98];
  */  
    im_full[i]=0;
    im_full[n-i]=0;
    re_full[i]=0;
    re_full[n-i]=0;
  }
if(n==1024) 
    { 
    int i=3;
    im_full[i]=0;
    im_full[n-i]=0;
    re_full[i]=0;
    re_full[n-i]=0;
    }
 
TVirtualFFT *fft_back = TVirtualFFT::FFT(1, &n, "C2R M K");
fft_back->SetPointsComplex(re_full,im_full);
fft_back->Transform();
TH1 *hb = 0;
hb = TH1::TransformHisto(fft_back,hb,"Re");
//hb->Scale(1./n);
//hb->Copy(h);
h->Reset();
h->Add(hb, 1./n);
}
  
void CdProperCanvas(Int_t chan, Int_t sn)
{
//Blue - TPC1 (NORTH), Red- TPC2 (SOUTH)

   if(sn==175780172)
   {
   color=kMagenta; //default color for chan 0 - Busy signal
   if(chan==31) {c->cd(1);color=kBlue;}
   if(chan==63) {c->cd(2);color=kRed;}
   if(chan==24) {c->cd(3);color=kBlue;}
   if(chan==56) {c->cd(4);color=kRed;}
   if(chan==15) {c->cd(5);color=kBlue;}
   if(chan==47) {c->cd(6);color=kRed;}
   if(chan==8)  {c->cd(7);color=kBlue;}
   if(chan==40) {c->cd(8);color=kRed;}
   }
   
   if(sn==175854781)
   {
   color=kGreen; //default color for chan 0 - Busy signal
   if(chan==31) {c->cd(1);color=kRed;}
   if(chan==63) {c->cd(2);color=kBlue;}
   if(chan==24) {c->cd(3);color=kRed;}
   if(chan==56) {c->cd(4);color=kBlue;}
   if(chan==15) {c->cd(5);color=kRed;}
   if(chan==47) {c->cd(6);color=kBlue;}
   if(chan==8)  {c->cd(7);color=kRed;}
   if(chan==40) {c->cd(8);color=kBlue;}
   }  

   if(chan==0) ct->cd();  
}  

//void drawwf(const char * fname, ULong64_t utime_sec, Int_t offset_usec)
void drawwf(const char * fname, const char * datime, Int_t offset_usec, Int_t window_us=300)
{


    struct tm t;
    time_t t_of_day;
    
    if(sscanf(datime, "%d-%d-%d %d:%d:%d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec)!=6)
    {
        printf("Cannot parse Date-time given.\n"); return;
    }
    t.tm_year = t.tm_year-1900;  // Year - 1900
    t.tm_mon = t.tm_mon -1; 
    //t_of_day = mktime(&t);
    t_of_day = timegm(&t);

    printf("seconds since the Epoch: %ld\n", (long) t_of_day);
  //  return;

  ULong64_t utime_sec=t_of_day;

  
  sprintf(st,"LDS events at %lld s, offset %d us",utime_sec, offset_usec);
  c=new TCanvas("c",st,500,900);
  c->Divide(2,4);
  
  ct=new TCanvas("ct",st,300,300);

  ch=new TH1S("ch","",1000,-300000,300000);
  ch->GetYaxis()->SetRangeUser(-32800,5000);
  ch->GetXaxis()->SetTitle("ns");
  ch->SetStats(0);

  for(int i=0;i<8;i++)
  {
   c->cd(i+1); 
   gPad->SetGridx(1); 
   gPad->SetGridy(1); 
  ch->Draw();
  }
  ct->cd();
   gPad->SetGridx(1); 
   gPad->SetGridy(1); 
  ch->Draw();
  


  r=new rwf(fname);
  tr=r->fChain;

  sprintf(st,"abs(utime_ms-%lld)<1000 && abs(tai_ns/1e3-%d)<%d &&  ch==00",utime_sec*1000, offset_usec,window_us);
  tr->Draw(">>elist00",st,"*");
  elist00 = (TEventList*)gDirectory->Get("elist00");

  sprintf(st,"abs(utime_ms-%lld)<1000 && abs(tai_ns/1e3-%d)<%d && (ch==31 || ch==15 || ch==63 || ch==47 \
                      ||ch==8 || ch==40 || ch==24 || ch==56 || ch==00)",utime_sec*1000, offset_usec,window_us);
  tr->Draw(">>elist",st,"*");
  elist = (TEventList*)gDirectory->Get("elist");
  
  tr->SetEventList(elist00);
  printf("Detecting relative t0 for both boards...\n");

  for(int i=0; i<elist00->GetN(); i++) {  tr->GetEntry(elist00->GetEntry(i)); if(r->sn==175780172) {t0_175780172=r->tai_ns; break;}}
  for(int i=0; i<elist00->GetN(); i++) {  tr->GetEntry(elist00->GetEntry(i)); if(r->sn==175854781) {t0_175854781=r->tai_ns; break;}}

  if(t0_175780172==0) t0_175780172=t0_175854781;
  if(t0_175854781==0) t0_175854781=t0_175780172;
  printf("ADC_175854781 t0=%lld \n",t0_175854781);
  printf("ADC_175780172 t0=%lld \n",t0_175780172);
  printf("\nADC_175780172 t0 mismatch w.r.t. given offset: %lld ns\n",t0_175780172-offset_usec*1000);
  
  //Correction based on busy WF
  //find first waveform on both adcs
  int b;
  int b_175780172;
  int b_175854781;
  

  for(int i=0; i<elist00->GetN(); i++) {  tr->GetEntry(elist00->GetEntry(i)); if(r->sn==175780172) break;}
  for(b=50; b < 250; b++) if(r->th1s_ptr->GetBinContent(b)<-10000) break;
  printf("Busy front detected for ADC %d at %d ns\n",r->sn,b*10);
  b_175780172=b*10;

  for(int i=0; i<elist00->GetN(); i++) {  tr->GetEntry(elist00->GetEntry(i)); if(r->sn==175854781) break;}
  for(b=50; b < 250; b++) if(r->th1s_ptr->GetBinContent(b)<-10000) break;
  printf("Busy front detected for ADC %d at %d ns\n",r->sn,b*10);
  b_175854781=b*10;
  
  tr->SetEventList(elist);
  npassed = elist->GetN();  // number of events to pass cuts

  // Margins for time line
  Int_t MinT=1e9, MaxT=-1e9;

  for(int i=0; i<npassed; i++)
  {
    tr->GetEntry(elist->GetEntry(i)); 
    trel=0;
   // trel=Int_t(r->tai_ns-t0_175780172);
      
 
  if(r->sn==175854781) trel=Int_t(r->tai_ns-t0_175854781); 
  else if(r->sn==175780172) trel=Int_t(r->tai_ns-t0_175780172); 
  else printf("Unknown SN=%d in the stream!\n",r->sn); 

     if(r->sn==175854781) trel=trel-b_175854781;
      if(r->sn==175780172) trel=trel-b_175780172;
      trel=trel+325; //Busy delay, must be more or less equal for both adcs
    
//    printf("tai_ns %lld, t0 %lld, trel %lld \n",r->tai_ns,t0,trel);
    bins=r->th1s_ptr->GetXaxis()->GetNbins();
    
    if(r->ch==8 || r->ch==40 || r->ch==24 || r->ch==56 ) FilterSinus(r->th1s_ptr);
    
    r->th1s_ptr->GetXaxis()->Set(bins,trel,trel+bins*10);
    r->th1s_ptr->GetXaxis()->SetTitle("ns");
    printf("%d : SN %d ch %d; tai_ns=%lld Rel.time,ns: %lld;  utime_ms %lld\n",i,r->sn,r->ch,r->tai_ns,trel,r->utime_ms);
    r->th1s_ptr->GetYaxis()->SetRangeUser(-32800,5000); 
    CdProperCanvas(r->ch, r->sn);
//    r->th1s_ptr->SetLineColor(1+r->ch/13);
    r->th1s_ptr->SetLineColor(color);
    r->th1s_ptr->DrawClone("ACsame");
    if(MinT>trel) MinT=trel;
    if(MaxT<trel+bins*10) MaxT=trel+bins*10;
  }
  printf("Time margins: %d ns  to %d ns.\n",MinT, MaxT);
  ch->GetXaxis()->SetRangeUser(MinT, MaxT); 

  printf("\nADC_175780172 t0 mismatch w.r.t. given offset: %lld ns\n",t0_175780172-offset_usec*1000);

  sprintf(st,"%s",fname); 
  sprintf(st2,"LDS_%s",basename(st)+4);
  st2[19]=0; 
  sprintf(st,"%s_%lld_%d.png",st2,utime_sec, offset_usec); 
  c->SaveAs(st);
  
}

void rwf::Loop(Long64_t utime_ms, Long64_t time_from_PPS_us, Int_t window_us=10)
{
//   In a ROOT session, you can do:
//      root> .L rwf.C
//      root> rwf t
//      root> t.GetEntry(12); // Fill t data members with entry number 12
//      root> t.Show();       // Show values of entry 12
//      root> t.Show(16);     // Read and show values of entry 16
//      root> t.Loop();       // Loop on all entries
//

//     This is the loop skeleton where:
//    jentry is the global entry number in the chain
//    ientry is the entry number in the current Tree
//  Note that the argument to GetEntry must be:
//    jentry for TChain::GetEntry
//    ientry for TTree::GetEntry and TBranch::GetEntry
//
//       To read only selected branches, Insert statements like:
// METHOD1:
//    fChain->SetBranchStatus("*",0);  // disable all branches
//    fChain->SetBranchStatus("branchname",1);  // activate branchname
// METHOD2: replace line
//    fChain->GetEntry(jentry);       //read all branches
//by  b_branchname->GetEntry(ientry); //read only this branch
   if (fChain == 0) return;

   Long64_t nentries = fChain->GetEntriesFast();

   Long64_t nbytes = 0, nb = 0;
   for (Long64_t jentry=0; jentry<nentries;jentry++) {
      Long64_t ientry = LoadTree(jentry);
      if (ientry < 0) break;
      nb = fChain->GetEntry(jentry);   nbytes += nb;
      // if (Cut(ientry) < 0) continue;
   }
}


void rwf::BuildIndex(const char *major,const char *minor)
{
  fChain->BuildIndex(major, minor);
  if(fChain->GetTreeIndex()) rindex=((TTreeIndex*)fChain->GetTreeIndex())->GetIndex();
  else rindex=0;
} 
