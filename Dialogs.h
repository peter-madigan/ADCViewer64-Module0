#include <TGLabel.h>
#include "TGButton.h"
#include "TGFrame.h"
#include "TGTextEntry.h"
#include "TGFileDialog.h"
#include "AFISettings.h"

#ifndef DIALOGS_H
#define DIALOGS_H

class InputDialog
{
private:
   TGTransientFrame *fDialog;  // transient frame, main dialog window
   TGTextEntry      *fTE;      // text entry widget containing
   TList            *fWidgets; // keep track of widgets to be deleted in dtor
   char             *fRetStr;  // address to store return string

public:
   InputDialog(const char *prompt, const char *defval, char *retstr);
   ~InputDialog();
   void ProcessMessage(Long_t msg, Long_t parm1, Long_t parm2);
};

const char *OpenFileDialog(TGMainFrame *,int filetype=0);
const char *SaveFileDialog(int*,TGMainFrame *,std::string);
const char *GetStringDialog(const char *prompt, const char *defval);
Int_t GetIntegerDialog(const char *prompt, Int_t defval);
Float_t GetFloatDialog(const char *prompt, Float_t defval);

#endif
