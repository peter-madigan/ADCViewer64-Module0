#define VERSION 20210412.0

#include <TApplication.h>
#include <TROOT.h>
#include "AFIDecoder.h"
#include "AFIGUI.h"
#include "AFIProcessor.h"
#include "AFISettings.h"
#include "RLogger.h"

int main(int argc, char *argv[])
{
  printf("%s Data Format Viewer [revision %.1f]\n",APP_PREFIX,VERSION);

  if(!process_options(argc, argv)) return -1;

  if (settings.guimode == 1 && settings.rwf_on == true)
  {
    printf("Achtung: ['-w'] option is not available with the ROOT graphical user interface ['-g 1']!\n");
    exit(0);
  }

  /////////////////////////////////////////////////////////////////////////////
  TApplication theApp("App",&argc,argv);
  if(gROOT->IsBatch())
  {
       fprintf(stderr, "%s: cannot run in batch mode\n", argv[1]);
       return 1;
  }
  if (gROOT->IsBatch()) return 1;
  /////////////////////////////////////////////////////////////////////////////

  settings.workdir = gSystem->GetWorkingDirectory();

  // Load VIEWERCONFIG cofig file /////////////////////////////////////////////
  if (!process_config( 0, settings.workdir.c_str(), &(settings.config) ))
  {
    printf("Achtung: Read viewer config error! Default viewer config file is created.\n");
    settings.config.init();
    sprintf(settings.config.gatefile, "%s/%s",settings.workdir.c_str(), GATE_FILENAME);
    process_config(1,settings.workdir.c_str(), &(settings.config));
  }
  /////////////////////////////////////////////////////////////////////////////

  start_processing();

  if (settings.guimode==2)
  {
    gui         = new AFIGUI();
    gui->SetWorkDir(settings.workdir.c_str());
    gui->SetConfig(&(settings.config));
    make_connections();
    gui->PopUp();
  }

  if (settings.guimode==3){
    gui         = new AFIGUI();
    make_connections();
    gui->OpenMonitorDialogBtn();
  }

  theApp.Run();
  return 0;
}
