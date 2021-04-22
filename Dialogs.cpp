#include "Dialogs.h"
#include "TVirtualX.h"

InputDialog::InputDialog(const char *prompt, const char *defval, char *retstr)
{
   // Create simple input dialog.

   fWidgets = new TList;

   const TGWindow *main = gClient->GetRoot();
   fDialog = new TGTransientFrame(main, main, 10, 10);

   // command to be executed by buttons and text entry widget
   char cmd[128];
   sprintf(cmd, "{long r__ptr=0x%lx; ((InputDialog*)r__ptr)->ProcessMessage($MSG,$PARM1,$PARM2);}", (Long_t)this);

   // create prompt label and textentry widget
   TGLabel *label = new TGLabel(fDialog, prompt);
   fWidgets->Add(label);

   TGTextBuffer *tbuf = new TGTextBuffer(256);  //will be deleted by TGtextEntry
   tbuf->AddText(0, defval);

   fTE = new TGTextEntry(fDialog, tbuf);
   fTE->Resize(260, fTE->GetDefaultHeight());
   fTE->SetCommand(cmd);

   TGLayoutHints *l1 = new TGLayoutHints(kLHintsTop | kLHintsLeft, 5, 5, 5, 0);
   TGLayoutHints *l2 = new TGLayoutHints(kLHintsTop | kLHintsLeft, 5, 5, 5, 5);
   fWidgets->Add(l1);
   fWidgets->Add(l2);

   fDialog->AddFrame(label, l1);
   fDialog->AddFrame(fTE, l2);

   // create frame and layout hints for Ok and Cancel buttons
   TGHorizontalFrame *hf = new TGHorizontalFrame(fDialog, 60, 20, kFixedWidth);
   TGLayoutHints     *l3 = new TGLayoutHints(kLHintsCenterY | kLHintsExpandX, 5, 5, 0, 0);

   // put hf as last in list to be deleted
   fWidgets->Add(l3);

   // create OK and Cancel buttons in their own frame (hf)
   UInt_t  nb = 0, width = 0, height = 0;
   TGTextButton *b;

   b = new TGTextButton(hf, "&Ok", cmd, 1);
   fWidgets->Add(b);
   b->Associate(fDialog);
   hf->AddFrame(b, l3);
   height = b->GetDefaultHeight();
   width  = TMath::Max(width, b->GetDefaultWidth()); ++nb;

   b = new TGTextButton(hf, "&Cancel", cmd, 2);
   fWidgets->Add(b);
   b->Associate(fDialog);
   hf->AddFrame(b, l3);
   height = b->GetDefaultHeight();
   width  = TMath::Max(width, b->GetDefaultWidth()); ++nb;

   // place button frame (hf) at the bottom
   TGLayoutHints *l4 = new TGLayoutHints(kLHintsBottom | kLHintsCenterX, 0, 0, 5, 5);
   fWidgets->Add(l4);
   fWidgets->Add(hf);

   fDialog->AddFrame(hf, l4);

   // keep buttons centered and with the same width
   hf->Resize((width + 20) * nb, height);

   // set dialog title
   fDialog->SetWindowName("Get Input");

   // map all widgets and calculate size of dialog
   fDialog->MapSubwindows();

   width  = fDialog->GetDefaultWidth();
   height = fDialog->GetDefaultHeight();

   fDialog->Resize(width, height);

   // position relative to the parent window (which is the root window)
   Window_t wdum;
   int      ax, ay;

   gVirtualX->TranslateCoordinates(main->GetId(), main->GetId(),
                          (((TGFrame *) main)->GetWidth() - width) >> 1,
                          (((TGFrame *) main)->GetHeight() - height) >> 1,
                          ax, ay, wdum);
   fDialog->Move(ax, ay);
   fDialog->SetWMPosition(ax, ay);

   // make the message box non-resizable
   fDialog->SetWMSize(width, height);
   fDialog->SetWMSizeHints(width, height, width, height, 0, 0);

   fDialog->SetMWMHints(kMWMDecorAll | kMWMDecorResizeH  | kMWMDecorMaximize |
                                       kMWMDecorMinimize | kMWMDecorMenu,
                        kMWMFuncAll  | kMWMFuncResize    | kMWMFuncMaximize |
                                       kMWMFuncMinimize,
                        kMWMInputModeless);

   // popup dialog and wait till user replies
   fDialog->MapWindow();

   fRetStr = retstr;

   gClient->WaitFor(fDialog);
}

InputDialog::~InputDialog()
{
   // Cleanup dialog.

   fWidgets->Delete();
   delete fWidgets;

   delete fTE;
   delete fDialog;
}

void InputDialog::ProcessMessage(Long_t msg, Long_t parm1, Long_t parm2)
{
   // Handle button and text enter events

   switch (GET_MSG(msg)) {
      case kC_COMMAND:
         switch (GET_SUBMSG(msg)) {
             case kCM_BUTTON:
                switch (parm1) {
                   case 1:
                      // here copy the string from text buffer to return variable
                      strcpy(fRetStr, fTE->GetBuffer()->GetString());
                      delete this;
                      break;

                   case 2:
                      fRetStr[0] = 0;
                      delete this;
                      break;
                 }
              default:
                 break;
          }
          break;

       case kC_TEXTENTRY:
         switch (GET_SUBMSG(msg)) {
             case kTE_ENTER:
                // here copy the string from text buffer to return variable
                strcpy(fRetStr, fTE->GetBuffer()->GetString());
                delete this;
                break;
             default:
                break;
          }
          break;

       default:
          break;
   }

}


//--- Utility Functions --------------------------------------------------------

const char *OpenFileDialog(TGMainFrame *MainWindow,int filetype)
{
   // Prompt for file to be opened. Depending on navigation in
   // dialog the current working directory can be changed.
   // The returned file name is always with respect to the
   // current directory.

   const char *gOpenAsTypesADC64[] =
   {
     "ADC data files",    "*.data",
     "All files",     "*",
     0,               0
   };
   const char *gOpenAsTypesTQDC2[] =
   {
     "ADC data files",    "*.dat",
     "All files",     "*",
     0,               0
   };

   const char *gOpenAsTypes1[] =
   {
     "ADC data files",    "*.gt", // gate file type
     // "All files",     "*",
     0,               0
   };

   static TGFileInfo fi;
   switch (filetype) {
     case 0:
       switch (DEVICE_TYPE)
       {
          case DEVICE_ID_ADC64:
            fi.fFileTypes = gOpenAsTypesADC64;
          break;
          case DEVICE_ID_TQDC:
            fi.fFileTypes = gOpenAsTypesTQDC2;
          break;
       }
     break;
     case 1:
      fi.fFileTypes = gOpenAsTypes1;  // gate file type
     break;
   }

   fi.SetMultipleSelection(0);
   TGFileDialog *fd = new TGFileDialog(gClient->GetRoot(), MainWindow, kFDOpen, &fi);

   return fi.fFilename;
}

const char *SaveFileDialog(int *FILETYPE,TGMainFrame *MainWindow,std::string FilePrefix)
{
   // Prompt for file to be saved. Depending on navigation in
   // dialog the current working directory can be changed.
   // The returned file name is always with respect to the
   // current directory.

   const char *gSaveAsTypes[] = {
      "Macro files",  "*.C",                    // id: 0
      "ROOT files",   "*.root",         // id: 2
      "Encapsulated PostScript", "*.eps",       // id: 4
      "Jpg files",    "*.jpg",                  // id: 6
      "Png files",    "*.png",                  // id: 8
      // "All files",    "*",
       0,              0
   };

   static TGFileInfo fi;
   fi.fFileTypes = gSaveAsTypes;

   new TGFileDialog(gClient->GetRoot(), MainWindow, kFDSave, &fi);

   (*FILETYPE) = fi.fFileTypeIdx;

   char *ext = NULL;
   switch (fi.fFileTypeIdx) {
     case 0:
        ext = strstr(fi.fFilename,".C");
     break;
     case 2:
        ext = strstr(fi.fFilename,".root");
     break;
     case 4:
        ext = strstr(fi.fFilename,".eps");
     break;
     case 6:
        ext = strstr(fi.fFilename,".jpg");
     break;
     case 8:
        ext = strstr(fi.fFilename,".png");
     break;
   }
   if (ext != NULL) *ext = 0x00;

   return fi.fFilename;
}

const char *GetStringDialog(const char *prompt, const char *defval)
{
   // Prompt for string. The typed in string is returned.

   static char answer[128];

   new InputDialog(prompt, defval, answer);

   return answer;
}

Int_t GetIntegerDialog(const char *prompt, Int_t defval)
{
   // Prompt for integer. The typed in integer is returned.

   static char answer[32];

   char defv[32];
   sprintf(defv, "%d", defval);

   new InputDialog(prompt, defv, answer);

   return atoi(answer);
}

Float_t GetFloatDialog(const char *prompt, Float_t defval)
{
   // Prompt for float. The typed in float is returned.

   static char answer[32];

   char defv[32];
   sprintf(defv, "%f", defval);

   new InputDialog(prompt, defv, answer);

   return atof(answer);
}
