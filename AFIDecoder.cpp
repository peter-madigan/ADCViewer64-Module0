#include "AFIDecoder.h"
#include "TChain.h"
#include "TKey.h"

// Constructor
AFIDecoder::AFIDecoder()
{
  uiEvent        = 0;
  uiTotalEvents  = 0;
  uiDeviceSN     = 0;
  uiDeviceID     = 0;
  uiRecTime      = 0;
  uiDevType      = DEVICE_TYPE;
  uiSampleNumber = DEFAULT_SAMPLE_NUMBER;
  uiDeviceNumber = 0;
  uiConvDiffLeft = CONV_RANGE_START;
  uiConvDiffRight = CONV_RANGE_END;
  iSignalOffset   = SIGNAL_OFFSET;
  bFirstEvent    = true;
  bIsEventTimeHdr = false;
  bVerbose       = VERBOSE_MODE;
  sprintf(cGateFileName, "%s", GATE_FILENAME);
  vFInd.clear();
  vADCs.clear();
  vRKernel.clear();
  vGate.clear();
  vPKGate.clear();
  vCHState.clear();
}

std::bitset<64> AFIDecoder::ReadActiveChannels(uint32_t lo, uint32_t up)
{
  uint64_t container = (uint64_t)lo;
  container         |= ((uint64_t)up) << 32;
  std::bitset<64> bvActiveChannels(container);
  return bvActiveChannels;
}

// looking for devices - getting device number and their filenames
UInt_t AFIDecoder::LookingForDevices(const char * ccFile)
{
  if (auto fd = fopen(ccFile, "r")){
    fclose(fd);
  }
  else {
    printf("Achtung: File doesn't exist!\n");
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return 0;
  }
  // printf("%s\n", ccFile);

  vADCs.clear();
  vCHState.clear();
  bvCHState.reset();
  ClearIndexation();

  uiDeviceNumber = 0;
  UInt_t uiData = 0;
  UInt_t uiTime = 0;
  char cTimeTag[32];
  sscanf(gSystem->BaseName(ccFile),"%x_%d_%d.data",&uiDeviceSN,&uiData,&uiTime);
  sprintf(cTimeTag, "%d_%d.data", uiData,uiTime);
  if ( !uiData && !uiTime )
  {
    Adcs.ev = 0;		                               // event number
    Adcs.sn = 0;                                   // number of samples
    Adcs.devsn = 0;                                // serial number of device
    Adcs.devid = 0;                                // device ID
    Adcs.devorn = 0;                               // device order number
    sprintf(Adcs.file,"%s",gSystem->BaseName(ccFile));
    realpath(Form("%s",gSystem->DirName(ccFile)), Adcs.dir);
    vADCs.push_back(Adcs);
    uiDeviceNumber++;
  }
  else
  {
    const char *dirname=gSystem->DirName(ccFile);
    const char *ext=".data";
    TSystemDirectory dir(dirname, dirname);
    TList *files = dir.GetListOfFiles();
    if (files)
    {
      TSystemFile *file;
      TString fname;
      TIter next(files);
      while ((file=(TSystemFile*)next()))
      {
        uiData = 0;
        uiTime = 0;
        // uiDev  = 0;

        Adcs.ev = 0;		                               // event number
        Adcs.sn = 0;                                   // number of samples
        Adcs.devsn = 0;                                // serial number of device
        Adcs.devid = 0;                                // device ID
        Adcs.devorn = 0;                               // device order number
        sprintf(Adcs.file,"empty");

        fname = file->GetName();

        if (!file->IsDirectory() && fname.EndsWith(ext))
        {
          sscanf(fname.Data(),"%x_%d_%d.data",&uiDeviceSN,&uiData,&uiTime);
          if (strcmp(Form("%d_%d.data",uiData,uiTime),cTimeTag)==0)
          {
            // printf("- %s\n", fname.Data());
            // fill order number of file and name of file for each device
            if (uiData != 0 && uiTime != 0)
            {
              // printf("Tag: %x Data: %d Time: %d\n", uiDeviceSN,uiData,uiTime);
              Adcs.devorn = uiDeviceNumber;
              Adcs.devsn = uiDeviceSN;
              sprintf(Adcs.file,"%s",fname.Data());
              realpath(Form("%s",gSystem->DirName(ccFile)), Adcs.dir);
              vADCs.push_back(Adcs);
              uiDeviceNumber++;
            }
          }
        }
      }
    }
  }

  if (vADCs.empty()) {
    printf("Achtung: No devices found!\n");
    printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return 0;
  }

  // start of readout, indexation and getting settings: active channels, number samples
  for (auto itD=vADCs.begin(); itD!=vADCs.end(); itD++)
  {
    vCHState.clear();
    auto fd = OpenDataFile(Form("%s/%s",gSystem->DirName(ccFile),(*itD).file),1,(*itD).devsn);
    if (itD==vADCs.begin())
    {
      sscanf(gSystem->BaseName(ccFile),"%*x_%d_%d.data",&uiData,&uiTime);
      sprintf(cRunTag, "%d_%d", uiData, uiTime);
    }
    // filling ADCS structure - just meta data about devices
    (*itD).ev = uiTotalEvents;
    (*itD).sn = uiSampleNumber;
    (*itD).devid = uiDeviceID;
    (*itD).devsn = uiDeviceSN;
    (*itD).ach   = bvCHState;
    (*itD).rectime = uiRecTime;

    CloseDataFile(fd);

    // add adc settings to the meta struct
    if (!vSetCH.empty())
    {
      for (auto itS=vSetCH.begin(); itS!=vSetCH.end(); itS++)
      {
        if ((*itS).devsn == (*itD).devsn)
        {
          (*itD).cfg.push_back( (*itS) );
          // printf("SN: %x Type: %c\n", (*itD).cfg.back().devsn,(*itD).cfg.back().type);
        }
      } // for loop by vSetCH
      // printf("ADCMeta: cfg settings vector size %zu\n", (*itD).cfg.size());
    }
    else printf("Achtung: ADC configuration settings have not been initialized yet!\n");

  }
  uiDeviceNumber = vADCs.size();
  return uiDeviceNumber;
}

// Open data file
FILE *AFIDecoder::OpenDataFile(const char * ccFile,bool bIndexation,UInt_t devsn)
{
  // Warning input devsn - got from file name
  FILE *fd;
  sprintf(cDataFileName,"%s",ccFile);
  if ((fd=fopen(cDataFileName, "rb")) == NULL)
  {
    printf("Achtung: Open file error or file not found!\n");
    return fd;
  }

  // DefineDeviceType(fd);

  if (bIndexation)
  {
    // Extract settings: number of active channels ...
    switch (DEVICE_TYPE)
    {
      case DEVICE_ID_ADC64:
        ReadMetaDataADC64(fd,VERBOSE_MODE);
      break;
      case DEVICE_ID_TQDC:
        ReadMetaDataTQDC(fd,VERBOSE_MODE);
      break;
    }

    // indexation of events over all file [table of events with their positions]
    uiTotalEvents = FileIndexation(fd,uiDeviceSN);
  }
  return fd;
}

// Close data file
void AFIDecoder::CloseDataFile(FILE *fd)
{
  if (fd != NULL) fclose(fd); else printf("Achtung: Close file error!\n");
}

// Set file name
void AFIDecoder::SetFileName(const char * FILENAME)
{
  sprintf(cDataFileName,"%s",FILENAME);
}

// Set work dir
void AFIDecoder::SetWorkDir(const char * workdir)
{
  sprintf(cWorkDir,"%s",workdir);
}

// !function is not implemented
void AFIDecoder::DefineDeviceType(FILE *fd)
{
  uint16_t uiTN = 0;  // thread number [0] - always read with one thread - no memory sharing
  TQDC2EventHdr[uiTN].clear();
  TQDC2DevHdr[uiTN].clear();

  // Start to read all files for meta from begining
  fseek(fd, 0, SEEK_SET);

  // Readout data
  fread(&TQDC2EventHdr[uiTN], sizeof(TQDC2EventHdr[uiTN]),1,fd);

  if (bVerbose)
  {
    printf("\n=== Event Header: ===\n");
    printf("Sync word:               %x\n",   TQDC2EventHdr[uiTN].sword);
    printf("Event length:            %d\n",   TQDC2EventHdr[uiTN].length);
    printf("Event number:            %d\n\n", TQDC2EventHdr[uiTN].evnum);
  }
  if (TQDC2EventHdr[uiTN].sword!=SYNC_WORD) {
    printf("Achtung: Wrong data file format!\n");
    return;
  }

  const uint32_t BS = (TQDC2EventHdr[uiTN].length/4); // size of event block excluding event header
  uint32_t       Buffer[BS];

  for (int p=0; p<BS; p++) Buffer[p] = 0;

  fread(&Buffer, sizeof(Buffer),1,fd);

  uint32_t offset = 0;   // in elements of event buffer
  int32_t  end    = BS;  // remain number of elements (32-bit words)

  TQDC2DevHdr[uiTN].sn     = Buffer[offset];
  offset++;
  TQDC2DevHdr[uiTN].id     = (Buffer[offset] & 0xFF000000)>>24;
  TQDC2DevHdr[uiTN].length = (Buffer[offset] & 0x00FFFFFF);
  offset++;  // skip device header

  uiDevType = TQDC2DevHdr[uiTN].id;
  switch (DEVICE_TYPE)
  {
    case DEVICE_ID_ADC64:
      uiDevTimeDiscr  = 10;
      uiDevChannelNum = 64;
    break;
    case DEVICE_ID_TQDC:
      uiDevTimeDiscr  = 8;
      uiDevChannelNum = 16;
    break;
  }
  printf("Device type:                  %x\n", uiDevType);
  printf("Device time discretization:   %d\n", uiDevTimeDiscr);
  printf("Device channel number:        %d\n", uiDevChannelNum);
}

uint32_t AFIDecoder::FileIndexation(FILE *fd,UInt_t devsn)
{
  uint32_t uiBuffer = 0;
  uint32_t uiEVENT_HEADER[3] = {0,0,0};
  // uint32_t uiEVENT_HEADER[4] = {0,0,0,0};
  uiTotalEvents = 0;

  FileIndex.ev = 0;
  FileIndex.po = 0;
  FileIndex.sn = 0;

  fseek(fd,0,SEEK_END);
	size_t sSizeOfFile = ftell(fd);
	fseek(fd,0,SEEK_SET);

  uint32_t sync_word = SYNC_WORD;
  if (bIsEventTimeHdr)
    sync_word = SYNC_WORD_TIME;

  while (!feof(fd))
  {
    fread(&uiBuffer,sizeof(uiBuffer),1,fd);
    if (uiBuffer == sync_word)
    {
      if (bIsEventTimeHdr)
      {
        fseek(fd,4*sizeof(uiBuffer)-sizeof(uint32_t),SEEK_CUR);
        fread(uiEVENT_HEADER,sizeof(uiEVENT_HEADER),1,fd);
      }
      else
      {
        fseek(fd,-sizeof(uint32_t),SEEK_CUR);
        fread(uiEVENT_HEADER,sizeof(uiEVENT_HEADER),1,fd);
      }

      uiTotalEvents++;
      switch (DEVICE_TYPE)
      {
        case DEVICE_ID_ADC64:
          FileIndex.ev = uiEVENT_HEADER[2];
        break;
        case DEVICE_ID_TQDC:
          FileIndex.ev = uiEVENT_HEADER[2]+1;
        break;
      }
      FileIndex.po = ftell(fd)-3*sizeof(uint32_t);
      FileIndex.sn = devsn;
      // if (FileIndex.ev < 10)
        // printf("Event#%zu pos: %zu sn: %x [ev.sync:%x]\n",FileIndex.ev,FileIndex.po,FileIndex.sn,uiEVENT_HEADER[0]);
      vFInd.push_back(FileIndex);

      fseek(fd,FileIndex.po+uiEVENT_HEADER[1],SEEK_SET);		// from zero position - jump over all event length
      if (FileIndex.po+uiEVENT_HEADER[1] > sSizeOfFile) break;
    }
  }
  // printf("\nData filename:         %s\n", vADCs.at(devorn).file);
  // printf("Device order number:   %d\n", devorn);
  // printf("Size of file:          %f MB\n", float(sSizeOfFile/1000000.));

  return uiTotalEvents;
}

// Reading meta data of TQDC
void AFIDecoder::ReadMetaDataTQDC(FILE *fd, bool bVerbose)
{
  uint16_t uiTN = 0;  // thread number [0] - always read with one thread - no memory sharing
  TQDC2EventHdr[uiTN].clear();
  TQDC2DevHdr[uiTN].clear();
  TQDC2MStreamHdr[uiTN].clear();
  TQDC2DataBlockHdr[uiTN].clear();
  TQDC2Tdc[uiTN].clear();
  TQDC2Adc[uiTN].clear();
  bvCHState.reset();

  // Start to read all files for meta from begining
  fseek(fd, 0, SEEK_SET);

  // Readout data
  fread(&TQDC2EventHdr[uiTN],sizeof(TQDC2EventHdr[uiTN]),1,fd);

  if (bVerbose)
  {
    printf("\n=== Event Header: ===\n");
    printf("Sync word:               %x\n",   TQDC2EventHdr[uiTN].sword);
    printf("Event length:            %d\n",   TQDC2EventHdr[uiTN].length);
    printf("Event number:            %d\n\n", TQDC2EventHdr[uiTN].evnum);
  }
  if (TQDC2EventHdr[uiTN].sword!=SYNC_WORD) {
    printf("Achtung: Wrong data file format!\n");
    return;
  }

  const uint32_t BS = (TQDC2EventHdr[uiTN].length/4); // size of event block excluding event header
  uint32_t       Buffer[BS];
  for (int p=0; p<BS; p++) Buffer[p] = 0;

  fread(&Buffer, sizeof(Buffer),1,fd);

  uint32_t offset = 0;   // in elements of event buffer
  int32_t  end    = BS;  // remain number of elements (32-bit words)

  // Read out devices
  while (end != 0)
  {
    TQDC2DevHdr[uiTN].sn     = Buffer[offset];
    offset++;
    TQDC2DevHdr[uiTN].id     = (Buffer[offset] & 0xFF000000)>>24;
    TQDC2DevHdr[uiTN].length = (Buffer[offset] & 0x00FFFFFF);
    offset++;  // skip device header

    uiDeviceSN = TQDC2DevHdr[uiTN].sn;
    uiDeviceID = TQDC2DevHdr[uiTN].id;

    if (bVerbose)
    {
      printf("=== Device Event Block === till end: %d ===\n",end);
      printf("Device SN:               %x\n",   TQDC2DevHdr[uiTN].sn);
      printf("Device ID:               %x\n",   TQDC2DevHdr[uiTN].id);
      printf("Device payload length:   %d\n\n", TQDC2DevHdr[uiTN].length);
    }

    // Read out MStreams
    while (end != 0)
    {
      TQDC2MStreamHdr[uiTN].subtype = (Buffer[offset] & 0x3);
      TQDC2MStreamHdr[uiTN].ch      = (Buffer[offset] & 0xFF000000) >> 24;
      TQDC2MStreamHdr[uiTN].length  = (Buffer[offset] & 0x00FFFFFC) >> 2;
      offset++; // skip mstream header
      TQDC2MStreamHdr[uiTN].taisec   =  Buffer[offset];
      offset++;
      TQDC2MStreamHdr[uiTN].tainsec  = (Buffer[offset] & 0xFFFFFFFC) >> 2;
      TQDC2MStreamHdr[uiTN].taiflags = (Buffer[offset] & 0x3);
      offset++;

      if (bVerbose)
      {
        printf("=== MStream Block === LeftB: %d Buffer: %x\n",end,Buffer[offset]);
        printf("mstream subtype:         %x\n", TQDC2MStreamHdr[uiTN].subtype);
        printf("mstream length:          %d\n", TQDC2MStreamHdr[uiTN].length);
        printf("mstream bits:            %d\n", TQDC2MStreamHdr[uiTN].ch);
        printf("taisec:                  %d\n", TQDC2MStreamHdr[uiTN].taisec);
        printf("tainsecs:                %d\n", TQDC2MStreamHdr[uiTN].tainsec);
        printf("flags:                   %d\n", TQDC2MStreamHdr[uiTN].taiflags);
      }

      int iDataLength = 0;

      switch (TQDC2MStreamHdr[uiTN].subtype)
      {
        case 0:
          // MStream subtype 0
          while (end != 0)
          {
            TQDC2DataBlockHdr[uiTN].type   =  (Buffer[offset] & 0xF0000000) >> 28;
            TQDC2DataBlockHdr[uiTN].ch     =  (Buffer[offset] & 0xF000000)  >> 24;
            TQDC2DataBlockHdr[uiTN].spec   =  (Buffer[offset] & 0x70000)    >> 16;
            TQDC2DataBlockHdr[uiTN].length   =  (Buffer[offset] & 0xFFFF);
            offset++;

            if (bVerbose)
            {
              printf("=== Data Block === LeftB: %d Buffer: %x\n",end,Buffer[offset]);
              printf("data type:               %d\n", TQDC2DataBlockHdr[uiTN].type);
              printf("channel number:          %d\n", TQDC2DataBlockHdr[uiTN].ch);
              printf("ADC Data Block specific: %d\n", TQDC2DataBlockHdr[uiTN].spec);
              printf("length:                  %d\n", TQDC2DataBlockHdr[uiTN].length);
            }

            // type of data [TDC or ADC]
            switch (TQDC2DataBlockHdr[uiTN].type)
            {
              case 0:
                // TDC Data type
                iDataLength = TQDC2DataBlockHdr[uiTN].length/4;

                while (iDataLength != 0)
                {
                  TQDC2Tdc[uiTN].n         = (Buffer[offset] & 0xF0000000) >> 28;

                  switch (TQDC2Tdc[uiTN].n)
                  {
                    case 2:
                      // Decode TDC event header
                      TQDC2Tdc[uiTN].timestamp = (Buffer[offset] & 0xFFF);
                      TQDC2Tdc[uiTN].evnum     = (Buffer[offset] & 0xFFF000) >> 12;
                      TQDC2Tdc[uiTN].id        = (Buffer[offset] & 0xF000000) >> 24;
                      TQDC2Tdc[uiTN].n         = (Buffer[offset] & 0xF0000000) >> 28;
                      offset++;
                      if (bVerbose)
                      {
                        printf("=== TDC Data === LeftB: %d Offset: %d\n",end,offset-1);
                        printf("word #:                 %d\n", TQDC2Tdc[uiTN].n);
                        printf("TDC ID:                 %d\n", TQDC2Tdc[uiTN].id);
                        printf("event number:           %d\n", TQDC2Tdc[uiTN].evnum);
                        printf("timestamp:              %d\n", TQDC2Tdc[uiTN].timestamp);
                        printf("-----------------\n");
                      }
                    break;
                    case 3:
                      // Decode TDC event trailer
                      TQDC2Tdc[uiTN].wcount    = (Buffer[offset] & 0xFFF);
                      TQDC2Tdc[uiTN].evnum     = (Buffer[offset] & 0xFFF000) >> 12;
                      TQDC2Tdc[uiTN].id        = (Buffer[offset] & 0xF000000) >> 24;
                      TQDC2Tdc[uiTN].n         = (Buffer[offset] & 0xF0000000) >> 28;
                      offset++;
                      if (bVerbose)
                      {
                        printf("=== TDC Data === LeftB: %d Offset: %d\n",end,offset-1);
                        printf("word #:                 %d\n", TQDC2Tdc[uiTN].n);
                        printf("TDC ID:                 %d\n", TQDC2Tdc[uiTN].id);
                        printf("event number:           %d\n", TQDC2Tdc[uiTN].evnum);
                        printf("wcount:                 %d\n", TQDC2Tdc[uiTN].wcount);
                        printf("-----------------\n");
                      }
                    break;
                    case 4:
                      // Decode TDC data leading edge
                      TQDC2Tdc[uiTN].rcdata     = (Buffer[offset] & 0x3);
                      TQDC2Tdc[uiTN].ledge     = (Buffer[offset] & 0x1FFFFC) >> 2;
                      TQDC2Tdc[uiTN].ch        = (Buffer[offset] & 0x1E00000) >> 21;
                      TQDC2Tdc[uiTN].n         = (Buffer[offset] & 0xF0000000) >> 28;
                      offset++;
                      if (bVerbose)
                      {
                        printf("=== TDC Data === LeftB: %d Offset: %d\n",end,offset-1);
                        printf("word #:                 %d\n", TQDC2Tdc[uiTN].n);
                        printf("channel number:         %d\n", TQDC2Tdc[uiTN].ch);
                        printf("leading edge:           %d\n", TQDC2Tdc[uiTN].ledge);
                        printf("rcdata:                 %d\n", TQDC2Tdc[uiTN].rcdata);
                        printf("-----------------\n");
                      }
                    break;
                    case 5:
                      // Decode TDC data trailing edge
                      TQDC2Tdc[uiTN].rcdata    = (Buffer[offset] & 0x3);
                      TQDC2Tdc[uiTN].tedge     = (Buffer[offset] & 0x1FFFFC)  >> 2;
                      TQDC2Tdc[uiTN].ch        = (Buffer[offset] & 0x1E00000) >> 21;
                      TQDC2Tdc[uiTN].n         = (Buffer[offset] & 0xF0000000) >> 28;
                      offset++;
                      if (bVerbose)
                      {
                        printf("=== TDC Data === LeftB: %d Offset: %d\n",end,offset-1);
                        printf("word #:                 %d\n", TQDC2Tdc[uiTN].n);
                        printf("channel number:         %d\n", TQDC2Tdc[uiTN].ch);
                        printf("trailing edge:          %d\n", TQDC2Tdc[uiTN].tedge);
                        printf("rcdata:                 %d\n", TQDC2Tdc[uiTN].rcdata);
                        printf("-----------------\n");
                      }
                    break;
                    case 6:
                      // Decode TDC error
                      TQDC2Tdc[uiTN].err       = (Buffer[offset] & 0x7FFF);
                      TQDC2Tdc[uiTN].id        = (Buffer[offset] & 0xF000000)  >> 24;
                      TQDC2Tdc[uiTN].n         = (Buffer[offset] & 0xF0000000) >> 28;
                      offset++;
                      if (bVerbose)
                      {
                        printf("=== TDC Data === LeftB: %d Offset: %d\n",end,offset-1);
                        printf("word #:                 %d\n", TQDC2Tdc[uiTN].n);
                        printf("id:                     %d\n", TQDC2Tdc[uiTN].id);
                        printf("TDC error:              %d\n", TQDC2Tdc[uiTN].err);
                        printf("-----------------\n");
                      }
                    break;
                    default:
                      offset++;
                    break;
                  }

                  iDataLength--;
                }
              break;
              case 1:
                // ADC Data type
                // Decode ADC data header
                TQDC2Adc[uiTN].timestamp = (Buffer[offset] & 0xFFFF);
                TQDC2Adc[uiTN].length    = (Buffer[offset] & 0xFFFF0000) >> 16;
                offset++;
                if (bVerbose)
                {
                  printf("=== ADC Data === LeftB: %d Offset: %d\n",end,offset-1);
                  printf("timestamp #:                %d\n", TQDC2Adc[uiTN].timestamp);
                  printf("length:                     %d\n", TQDC2Adc[uiTN].length);
                }

                bvCHState.set(TQDC2DataBlockHdr[uiTN].ch);
                const uint16_t SN = (TQDC2Adc[uiTN].length/4)*2;       // Number of samples
                uiSampleNumber = SN;
                offset += (SN/2);
              break;
            } // data type switch end

            end = BS - offset;

            if (end < 0) break;
          }
          if (end < 0) break;

          vCHState.push_back(bvCHState);

        break;
        case 1:
          // MStream subtype 1
          // no
        break;
      } // subtype switch end

      if (end < 0) break;
    } // mstream loop end
  } // device loop end
}

// Read event of TQDC
Int_t AFIDecoder::ReadEventTQDC(FILE *fd, uint32_t event,\
                              std::vector<DATA> *vData,\
                              bool bVerbose,Int_t devorn, \
                              UInt_t devsn,uint16_t thread_id)
{
  uint16_t uiTN = thread_id;
  uint8_t  uiDC = 0; // device counter
  uiEvent = event;

  if (vData != NULL) vData->clear();
  data_tqdc[uiTN].clear();

  TQDC2EventHdr[uiTN].clear();
  TQDC2DevHdr[uiTN].clear();
  TQDC2MStreamHdr[uiTN].clear();
  TQDC2DataBlockHdr[uiTN].clear();
  TQDC2Tdc[uiTN].clear();
  TQDC2Adc[uiTN].clear();

  // Getting position of event
  auto posptr = std::find_if(vFInd.begin(), vFInd.end(),
                       [event,devsn] (const FILEINDEX& fi) {
                          return fi.ev == event && fi.sn == devsn;
                       });
  if (posptr==vFInd.end())
  {
    printf("Achtung: Event#%04d not found!\n",event);
    return ERROR_EVENT_READ;
  }

  // Getting device poiter
  auto devPtr = std::find_if(vADCs.begin(), vADCs.end(),
                       [devsn] (const ADCS& adc) {
                          return adc.devsn == devsn;
                       });
  if (devPtr==vADCs.end())
  {
    printf("Achtung: Device %x not found!\n",devsn);
    return ERROR_EVENT_READ;
  }

  if (bVerbose)
    printf("Event number:          %zu [offset: %zu byte] device: %x\n", \
            posptr->ev,posptr->po,posptr->sn);

  fseek(fd, posptr->po, SEEK_SET);
  // Readout data
  fread(&TQDC2EventHdr[uiTN], sizeof(TQDC2EventHdr[uiTN]),1,fd);

  if (bVerbose)
  {
    printf("\n=== Event Header: ===\n");
    printf("Sync word:               %x\n",   TQDC2EventHdr[uiTN].sword);
    printf("Event length:            %d\n",   TQDC2EventHdr[uiTN].length);
    printf("Event number:            %d\n\n", TQDC2EventHdr[uiTN].evnum);
  }

  const uint32_t BS = (TQDC2EventHdr[uiTN].length/4); // size of event block excluding event header
  uint32_t       Buffer[BS];
  for (int p=0; p<BS; p++) Buffer[p] = 0;

  fread(&Buffer, sizeof(Buffer),1,fd);

  // for (int p=0; p<BS; p++)
  //   printf("#%d -> buffer: %10d [%10x] [%7d,%7d]\n",p, Buffer[p],Buffer[p],
  //           (int16_t)(Buffer[p]&0xFFFF),(int16_t)((Buffer[p]&0xFFFF0000)>>16));

  uint32_t offset = 0;   // in elements of event buffer
  int32_t  end    = BS;  // remain number of elements (32-bit words)

  // Read out devices
  while (end != 0)
  {
    TQDC2DevHdr[uiTN].sn     = Buffer[offset];
    offset++;
    TQDC2DevHdr[uiTN].id     = (Buffer[offset] & 0xFF000000)>>24;
    TQDC2DevHdr[uiTN].length = (Buffer[offset] & 0x00FFFFFF);
    offset++;  // skip device header

    data_tqdc[uiTN].ev = event;
    data_tqdc[uiTN].devsn = TQDC2DevHdr[uiTN].sn;
    data_tqdc[uiTN].devid = TQDC2DevHdr[uiTN].id;
    data_tqdc[uiTN].devorn = devorn;

    if (bVerbose)
    {
      printf("=== Device Event Block === till end: %d ===\n",end);
      printf("Device SN:               %x\n",   TQDC2DevHdr[uiTN].sn);
      printf("Device ID:               %x\n",   TQDC2DevHdr[uiTN].id);
      printf("Device payload length:   %d\n\n", TQDC2DevHdr[uiTN].length);
    }

    // Read out MStreams
    while (end != 0)
    {
      TQDC2MStreamHdr[uiTN].subtype = (Buffer[offset] & 0x3);
      TQDC2MStreamHdr[uiTN].ch      = (Buffer[offset] & 0xFF000000) >> 24;
      TQDC2MStreamHdr[uiTN].length  = (Buffer[offset] & 0x00FFFFFC) >> 2;
      offset++; // skip mstream header
      TQDC2MStreamHdr[uiTN].taisec   =  Buffer[offset];
      offset++;
      TQDC2MStreamHdr[uiTN].tainsec  = (Buffer[offset] & 0xFFFFFFFC) >> 2;
      TQDC2MStreamHdr[uiTN].taiflags = (Buffer[offset] & 0x3);
      offset++;

      data_tqdc[uiTN].taisec  = TQDC2MStreamHdr[uiTN].taisec;
      data_tqdc[uiTN].tainsec = TQDC2MStreamHdr[uiTN].tainsec;

      if (bVerbose)
      {
        printf("=== MStream Block === LeftB: %d Buffer: %x\n",end,Buffer[offset]);
        printf("mstream subtype:         %x\n", TQDC2MStreamHdr[uiTN].subtype);
        printf("mstream length:          %d\n", TQDC2MStreamHdr[uiTN].length);
        printf("mstream bits:            %d\n", TQDC2MStreamHdr[uiTN].ch);
        printf("taisec:                  %d\n", TQDC2MStreamHdr[uiTN].taisec);
        printf("tainsecs:                %d\n", TQDC2MStreamHdr[uiTN].tainsec);
        printf("flags:                   %d\n", TQDC2MStreamHdr[uiTN].taiflags);
      }

      if (!TQDC2MStreamHdr[uiTN].length)
      {
        printf("Achtung: Event#%04d is corrupted. MStream block has zero length!\n",event);
        data_tqdc[uiTN].valid = false;
        return ERROR_BAD_EVENT;
      }

      int iDataLength = 0;

      switch (TQDC2MStreamHdr[uiTN].subtype)
      {
        case 0:
          // MStream subtype 0
          while (end != 0)
          {
            TQDC2DataBlockHdr[uiTN].type   =  (Buffer[offset] & 0xF0000000) >> 28;
            TQDC2DataBlockHdr[uiTN].ch     =  (Buffer[offset] & 0xF000000)  >> 24;
            TQDC2DataBlockHdr[uiTN].spec   =  (Buffer[offset] & 0x70000)    >> 16;
            TQDC2DataBlockHdr[uiTN].length   =  (Buffer[offset] & 0xFFFF);
            offset++;

            if (bVerbose)
            {
              printf("=== Data Block === LeftB: %d Buffer: %x\n",end,Buffer[offset]);
              printf("data type:               %d\n", TQDC2DataBlockHdr[uiTN].type);
              printf("channel number:          %d\n", TQDC2DataBlockHdr[uiTN].ch);
              printf("ADC Data Block specific: %d\n", TQDC2DataBlockHdr[uiTN].spec);
              printf("length:                  %d\n", TQDC2DataBlockHdr[uiTN].length);
            }

            // type of data [TDC or ADC]
            switch (TQDC2DataBlockHdr[uiTN].type)
            {
              case 0:
                // TDC Data type
                iDataLength = TQDC2DataBlockHdr[uiTN].length/4;

                while (iDataLength != 0)
                {
                  TQDC2Tdc[uiTN].n         = (Buffer[offset] & 0xF0000000) >> 28;
                  // printf("Buffer: %x | TQDC2Tdc[uiTN].n: %d iDataLength: %d end: %d\n", Buffer[offset],TQDC2Tdc[uiTN].n,iDataLength,end);

                  switch (TQDC2Tdc[uiTN].n)
                  {
                    case 2:
                      // Decode TDC event header
                      TQDC2Tdc[uiTN].timestamp = (Buffer[offset] & 0xFFF);
                      TQDC2Tdc[uiTN].evnum     = (Buffer[offset] & 0xFFF000) >> 12;
                      TQDC2Tdc[uiTN].id        = (Buffer[offset] & 0xF000000) >> 24;
                      TQDC2Tdc[uiTN].n         = (Buffer[offset] & 0xF0000000) >> 28;
                      offset++;
                      if (bVerbose)
                      {
                        printf("=== TDC Data === LeftB: %d Offset: %d\n",end,offset-1);
                        printf("word #:                 %d\n", TQDC2Tdc[uiTN].n);
                        printf("TDC ID:                 %d\n", TQDC2Tdc[uiTN].id);
                        printf("event number:           %d\n", TQDC2Tdc[uiTN].evnum);
                        printf("timestamp:              %d\n", TQDC2Tdc[uiTN].timestamp);
                        printf("-----------------\n");
                      }
                    break;
                    case 3:
                      // Decode TDC event trailer
                      TQDC2Tdc[uiTN].wcount    = (Buffer[offset] & 0xFFF);
                      TQDC2Tdc[uiTN].evnum     = (Buffer[offset] & 0xFFF000) >> 12;
                      TQDC2Tdc[uiTN].id        = (Buffer[offset] & 0xF000000) >> 24;
                      TQDC2Tdc[uiTN].n         = (Buffer[offset] & 0xF0000000) >> 28;
                      offset++;
                      if (bVerbose)
                      {
                        printf("=== TDC Data === LeftB: %d Offset: %d\n",end,offset-1);
                        printf("word #:                 %d\n", TQDC2Tdc[uiTN].n);
                        printf("TDC ID:                 %d\n", TQDC2Tdc[uiTN].id);
                        printf("event number:           %d\n", TQDC2Tdc[uiTN].evnum);
                        printf("wcount:                 %d\n", TQDC2Tdc[uiTN].wcount);
                        printf("-----------------\n");
                      }
                    break;
                    case 4:
                      // Decode TDC data leading edge
                      TQDC2Tdc[uiTN].rcdata     = (Buffer[offset] & 0x3);
                      TQDC2Tdc[uiTN].ledge     = (Buffer[offset] & 0x1FFFFC) >> 2;
                      TQDC2Tdc[uiTN].ch        = (Buffer[offset] & 0x1E00000) >> 21;
                      TQDC2Tdc[uiTN].n         = (Buffer[offset] & 0xF0000000) >> 28;
                      offset++;
                      if (bVerbose)
                      {
                        printf("=== TDC Data === LeftB: %d Offset: %d\n",end,offset-1);
                        printf("word #:                 %d\n", TQDC2Tdc[uiTN].n);
                        printf("channel number:         %d\n", TQDC2Tdc[uiTN].ch);
                        printf("leading edge:           %d\n", TQDC2Tdc[uiTN].ledge);
                        printf("rcdata:                 %d\n", TQDC2Tdc[uiTN].rcdata);
                        printf("-----------------\n");
                      }
                    break;
                    case 5:
                      // Decode TDC data trailing edge
                      TQDC2Tdc[uiTN].rcdata    = (Buffer[offset] & 0x3);
                      TQDC2Tdc[uiTN].tedge     = (Buffer[offset] & 0x1FFFFC)  >> 2;
                      TQDC2Tdc[uiTN].ch        = (Buffer[offset] & 0x1E00000) >> 21;
                      TQDC2Tdc[uiTN].n         = (Buffer[offset] & 0xF0000000) >> 28;
                      offset++;
                      if (bVerbose)
                      {
                        printf("=== TDC Data === LeftB: %d Offset: %d\n",end,offset-1);
                        printf("word #:                 %d\n", TQDC2Tdc[uiTN].n);
                        printf("channel number:         %d\n", TQDC2Tdc[uiTN].ch);
                        printf("trailing edge:          %d\n", TQDC2Tdc[uiTN].tedge);
                        printf("rcdata:                 %d\n", TQDC2Tdc[uiTN].rcdata);
                        printf("-----------------\n");
                      }
                    break;
                    case 6:
                      // Decode TDC error
                      TQDC2Tdc[uiTN].err       = (Buffer[offset] & 0x7FFF);
                      TQDC2Tdc[uiTN].id        = (Buffer[offset] & 0xF000000)  >> 24;
                      TQDC2Tdc[uiTN].n         = (Buffer[offset] & 0xF0000000) >> 28;
                      offset++;
                      if (bVerbose)
                      {
                        printf("=== TDC Data === LeftB: %d Offset: %d\n",end,offset-1);
                        printf("word #:                 %d\n", TQDC2Tdc[uiTN].n);
                        printf("id:                     %d\n", TQDC2Tdc[uiTN].id);
                        printf("TDC error:              %d\n", TQDC2Tdc[uiTN].err);
                        printf("-----------------\n");
                      }
                    break;
                    default:
                      offset++;
                    break;
                  }

                  iDataLength--;
                }
              break;
              case 1:
                // ADC Data type
                // Decode ADC data header
                TQDC2Adc[uiTN].timestamp = (Buffer[offset] & 0xFFFF);
                TQDC2Adc[uiTN].length    = (Buffer[offset] & 0xFFFF0000) >> 16;
                offset++;
                if (bVerbose)
                {
                  printf("=== ADC Data === LeftB: %d Offset: %d\n",end,offset-1);
                  printf("timestamp #:                %d\n", TQDC2Adc[uiTN].timestamp);
                  printf("length:                     %d\n", TQDC2Adc[uiTN].length);
                }

                const uint16_t SN = (TQDC2Adc[uiTN].length/4)*2;  // Number of samples
                uint32_t ch = TQDC2DataBlockHdr[uiTN].ch;       // current channel number

                data_tqdc[uiTN].sn = SN;
                data_tqdc[uiTN].chstate[ch] = true;
                data_tqdc[uiTN].chis.reset(ch);

                auto setPtr = std::find_if(vSetCH.begin(), vSetCH.end(),
                                     [ch,devsn] (const ADCSETTINGS& in) {
                                        return in.ch == ch  && in.devsn == devsn;
                                     });
                if (setPtr==vSetCH.end())
                {
                  printf("Achtung: ReadEvent: Device is not found in settings file\n"); //return ERROR_EVENT_READ;
                  setPtr->inv = 1;  // set negative by default
                }

                int16_t wave = 0;
                // for (int s=0; s < SN/2; s++)
                for (int s=(SN/2); s-- > 0;)
                {
                  auto ind = offset+SN/2-s-1; // reverse cycle
                  // auto ind = offset+s;     // dirrect cycle

                  wave = (Buffer[ind] & 0xFFFF) * setPtr->inv + iSignalOffset;
                  data_tqdc[uiTN].wf[ch].push_back(wave);
                  wave = ((Buffer[ind] & 0xFFFF0000) >> 16) * setPtr->inv + iSignalOffset;
                  data_tqdc[uiTN].wf[ch].push_back(wave);

                  if ( (s<4 || s>(SN/2)-5) && bVerbose )
                    printf("#%04d [%d] word: %x\n",s,ind,Buffer[ind]);
                }
                offset += (SN/2);
              break;
            } // data type switch end

            end = BS - offset;

            if (end < 0) break;
          }
          if (end < 0) break;
        break;
        case 1:
          // MStream subtype 1
          // empty
        break;
      } // subtype switch end

      if (end < 0) break;
    } // mstream loop end

    // event validation
    if (CHANNEL_NUMBER-data_tqdc[uiTN].chis.count() != devPtr->ach.count())
    {
      printf("Achtung: Event#%04d is corrupted. Some channels are absence!\n",event);
      data_tqdc[uiTN].valid = false; // incomplite event mark
      return ERROR_BAD_EVENT;
    }

    if (vData != NULL)
    {
      // PrintDataStruct(&data_tqdc[uiTN]);
      vData->push_back(data_tqdc[uiTN]);
    }
    uiDC++;
  } // device loop end

  return 0;
}

// Read meta data of ADC64
void AFIDecoder::ReadMetaDataADC64(FILE *fd, bool bVerbose)
{
  uint16_t uiTN = 0;  // thread number [0] - always read with one thread - no memory sharing
  bIsEventTimeHdr = false;
  ADC64EventTimeHdr[uiTN].clear();
  ADC64EventHdr[uiTN].clear();
  ADC64DevHdr[uiTN].clear();
  ADC64MStreamHdr[uiTN].clear();
  ADC64Subtype0[uiTN].clear();

  // Start to read all files for meta from the begining
  fseek(fd, 0, SEEK_SET);

  // Readout event time header data
  fread(&ADC64EventTimeHdr[uiTN], sizeof(ADC64EventTimeHdr[uiTN]),1,fd);

  if (ADC64EventTimeHdr[uiTN].sword!=SYNC_WORD_TIME)
  {
    printf("\nAchtung: No time event header is present in the data. The data format is an old version!\n");
    fseek(fd, 0, SEEK_SET);
  }
  else
  {
    bIsEventTimeHdr = true;
    uiRecTime  = (uint64_t)ADC64EventTimeHdr[uiTN].time_lo;
    uiRecTime |= ((uint64_t)ADC64EventTimeHdr[uiTN].time_hi) << 32;
    if (bVerbose)
    {
      printf("\n=== Event Time Header: ===\n");
      printf("Sync word:               %x\n", ADC64EventTimeHdr[uiTN].sword);
      printf("Event length:            %d\n", ADC64EventTimeHdr[uiTN].length);
      printf("Time lo:                 %x\n", ADC64EventTimeHdr[uiTN].time_lo);
      printf("Time hi:                 %x\n", ADC64EventTimeHdr[uiTN].time_hi);
      printf("Time:                    %lld ms [%d]\n", uiRecTime,bIsEventTimeHdr);
    }
  }

  // Readout event header data
  fread(&ADC64EventHdr[uiTN], sizeof(ADC64EventHdr[uiTN]),1,fd);

  if (bVerbose)
  {
    printf("\n=== Event Data Header: ===\n");
    printf("Sync word:               %x\n", ADC64EventHdr[uiTN].sword);
    printf("Event length:            %d\n", ADC64EventHdr[uiTN].length);
    printf("Event number:            %d\n\n", ADC64EventHdr[uiTN].evnum);
  }
  if (ADC64EventHdr[uiTN].sword!=SYNC_WORD) {
    printf("Achtung: Wrong data file format!\n");
    return;
  }

  const uint32_t BS = (ADC64EventHdr[uiTN].length/4); // size of event block excluding event header
  uint32_t       uiBuffer[BS];
  for (int p=0; p<BS; p++) uiBuffer[p] = 0;

  fread(&uiBuffer, sizeof(uiBuffer),1,fd);

  uint32_t offset = 0;   // in elements of event buffer
  int32_t  end    = BS;  // remain number of elements (32-bit words)

  // Read out devices
  while (end != 0)
  {
    ADC64DevHdr[uiTN].sn     = uiBuffer[offset];          // be carefull there is a possible error [fixed index]
    offset++;
    ADC64DevHdr[uiTN].id     = (uiBuffer[offset] & 0xFF000000)>>24;
    ADC64DevHdr[uiTN].length = (uiBuffer[offset] & 0x00FFFFFF);
    offset++;

    uiDeviceSN = ADC64DevHdr[uiTN].sn;
    uiDeviceID = ADC64DevHdr[uiTN].id;

    if (bVerbose)
    {
      printf("=== Device Event Block === till end: %d ===\n",end);
      printf("Device SN:               %x\n", ADC64DevHdr[uiTN].sn);
      printf("Device ID:               %x\n", ADC64DevHdr[uiTN].id);
      printf("Device payload length:   %d\n\n", ADC64DevHdr[uiTN].length);
    }

    // Read out MStreams
    while (end != 0)
    {
      ADC64MStreamHdr[uiTN].ch    = (uiBuffer[offset] & 0xFF000000) >> 24;
      ADC64MStreamHdr[uiTN].length  = (uiBuffer[offset] & 0x00FFFFFC)  >> 2;
      ADC64MStreamHdr[uiTN].type    = (uiBuffer[offset] & 0x3);
      offset++;

      if (bVerbose)
      {
        printf("=== MStream Block ===\n");
        printf("Channel number:          %d\n", ADC64MStreamHdr[uiTN].ch);
        printf("mstream length:          %d\n", ADC64MStreamHdr[uiTN].length);
        printf("mstream subtype:         %x\n", ADC64MStreamHdr[uiTN].type);
      }

      switch (ADC64MStreamHdr[uiTN].type)
      {
        case 0:
          ADC64Subtype0[uiTN].taisec   = uiBuffer[offset];
          offset++;
          ADC64Subtype0[uiTN].tainsec  = (uiBuffer[offset] & 0xFFFFFFFC) >> 2;
          ADC64Subtype0[uiTN].taiflags = (uiBuffer[offset] & 0x3);
          offset++;
          ADC64Subtype0[uiTN].chlo     = uiBuffer[offset];
          offset++;
          ADC64Subtype0[uiTN].chup     = uiBuffer[offset];
          offset++;

          if (bVerbose)
          {
            printf("taisec:                  %d\n", ADC64Subtype0[uiTN].taisec);
            printf("tainsecs:                %d\n", ADC64Subtype0[uiTN].tainsec);
            printf("flags:                   %d\n", ADC64Subtype0[uiTN].taiflags);
            printf("ch lower:                %d\n", ADC64Subtype0[uiTN].chlo);
            printf("ch upper:                %d\n", ADC64Subtype0[uiTN].chup);
          }

          bvCHState.reset();
          bvCHState = ReadActiveChannels(ADC64Subtype0[uiTN].chlo,ADC64Subtype0[uiTN].chup);
          vCHState.push_back(bvCHState);

        break;
        case 1:
          uiSampleNumber = (ADC64MStreamHdr[uiTN].length-2)*2;  // Number of samples
          end = 0;  // to leave the loop
        break;
      }
    }
  }

}

// Read event of ADC64
Int_t AFIDecoder::ReadEventADC64(FILE *fd, uint32_t event,\
  std::vector<DATA> *vData,bool bVerbose,Int_t devorn, UInt_t devsn, uint16_t thread_id)
{
  uint16_t uiTN = thread_id;
  bool bTimeHdrON = bIsEventTimeHdr;
  uiEvent = event;
  if (vData != NULL) vData->clear();
  data[uiTN].clear();
  ADC64EventTimeHdr[uiTN].clear();
  ADC64EventHdr[uiTN].clear();
  ADC64DevHdr[uiTN].clear();
  ADC64MStreamHdr[uiTN].clear();
  ADC64Subtype0[uiTN].clear();
  ADC64Subtype1[uiTN].clear();

  // Getting position of event
  auto posptr = std::find_if(vFInd.begin(), vFInd.end(),
                       [event,devsn] (const FILEINDEX& fi) {
                          return fi.ev == event && fi.sn == devsn;
                       });
  if (posptr==vFInd.end())
  {
    printf("Achtung: Event#%04d not found!\n",event);
    return ERROR_EVENT_READ;
  }

  // Getting device poiter
  auto devPtr = std::find_if(vADCs.begin(), vADCs.end(),
                       [devsn] (const ADCS& adc) {
                          return adc.devsn == devsn;
                       });
  if (devPtr==vADCs.end())
  {
    printf("Achtung: Device %x not found!\n",devsn);
    return ERROR_EVENT_READ;
  }

  if (bVerbose)
    printf("Event number:          %zu [offset: %zu byte] device: %x\n", posptr->ev,posptr->po,posptr->sn);

  // Readout event TIME header
  if (bTimeHdrON)
  {
    fseek(fd, posptr->po - sizeof(ADC64EventTimeHdr[uiTN]), SEEK_SET);
    fread(&ADC64EventTimeHdr[uiTN], sizeof(ADC64EventTimeHdr[uiTN]),1,fd);
    if (ADC64EventTimeHdr[uiTN].sword==SYNC_WORD_TIME)
    {
      data[uiTN].utime  = (uint64_t)ADC64EventTimeHdr[uiTN].time_lo;
      data[uiTN].utime |= ((uint64_t)ADC64EventTimeHdr[uiTN].time_hi) << 32;
      if (bVerbose)
      {
        printf("\n=== Event Time Header: ===\n");
        printf("Sync word:               %x\n", ADC64EventTimeHdr[uiTN].sword);
        printf("Event length:            %d\n", ADC64EventTimeHdr[uiTN].length);
        printf("Time lo:                 %d\n", ADC64EventTimeHdr[uiTN].time_lo);
        printf("Time hi:                 %d\n", ADC64EventTimeHdr[uiTN].time_hi);
        printf("Time:                    %lld ms\n", data[uiTN].utime);
      }
    }
  }

  // Readout event DATA header
  fseek(fd, posptr->po, SEEK_SET);
  fread(&ADC64EventHdr[uiTN], sizeof(ADC64EventHdr[uiTN]),1,fd);
  if (bVerbose)
  {
    printf("\n=== Event Header: ===\n");
    printf("Sync word:               %x\n", ADC64EventHdr[uiTN].sword);
    printf("Event length:            %d\n", ADC64EventHdr[uiTN].length);
    printf("Event number:            %d\n\n", ADC64EventHdr[uiTN].evnum);
  }
  data[uiTN].ev     = ADC64EventHdr[uiTN].evnum;
  data[uiTN].devorn = devorn;

  const uint32_t BS = (ADC64EventHdr[uiTN].length/4); // size of event block excluding event header
  uint32_t uiBuffer[BS];
  for (int p=0; p<BS; p++) uiBuffer[p] = 0;

  fread(&uiBuffer, sizeof(uiBuffer),1,fd);
  // for (int p=0; p<BS; p++) printf("#%04d payload: %x\n", p, uiBuffer[p]);

  uint32_t offset = 0;   // in elements of event buffer
  int32_t  end    = BS;  // remain number of elements (32-bit words)

  // Read out devices
  while (end != 0)
  {
    ADC64DevHdr[uiTN].sn     = uiBuffer[offset];
    offset++;
    // For MStream v2.1
    ADC64DevHdr[uiTN].id     = (uiBuffer[offset] & 0xFF000000)>>24;
    ADC64DevHdr[uiTN].length = (uiBuffer[offset] & 0x00FFFFFF);
    offset++;

    data[uiTN].devsn = ADC64DevHdr[uiTN].sn;
    data[uiTN].devid = ADC64DevHdr[uiTN].id;

    if (bVerbose)
    {
      printf("=== Device Event Block === till the end: %d ===\n",end);
      printf("Device SN:               %x\n", ADC64DevHdr[uiTN].sn);
      printf("Device ID:               %x\n", ADC64DevHdr[uiTN].id);
      printf("Device payload length:   %d\n\n", ADC64DevHdr[uiTN].length);
    }

    // Read out MStreams
    while (end != 0)
    {
      ADC64MStreamHdr[uiTN].ch     = (uiBuffer[offset] & 0xFF000000) >> 24;
      ADC64MStreamHdr[uiTN].length = (uiBuffer[offset] & 0x00FFFFFC)  >> 2;
      ADC64MStreamHdr[uiTN].type   = (uiBuffer[offset] & 0x3);
      offset++; // skip mstream header

      if (bVerbose)
      {
        printf("=== MStream Block ===\n");
        printf("Channel number:          %d\n", ADC64MStreamHdr[uiTN].ch);
        printf("mstream length:          %d\n", ADC64MStreamHdr[uiTN].length);
        printf("mstream subtype:         %x\n", ADC64MStreamHdr[uiTN].type);
      }

      if (!ADC64MStreamHdr[uiTN].length)
      {
        printf("Achtung: Event#%04d is corrupted. MStream block has zero length!\n",event);
        data[uiTN].valid = false;
        return ERROR_BAD_EVENT;
      }

      switch (ADC64MStreamHdr[uiTN].type)
      {
        case 0:
          ADC64Subtype0[uiTN].taisec   = uiBuffer[offset];
          offset++;
          ADC64Subtype0[uiTN].tainsec  = (uiBuffer[offset] & 0xFFFFFFFC) >> 2;
          ADC64Subtype0[uiTN].taiflags = (uiBuffer[offset] & 0x3);
          offset++;
          ADC64Subtype0[uiTN].chlo     = uiBuffer[offset];
          offset++;
          ADC64Subtype0[uiTN].chup     = uiBuffer[offset];
          offset++;

          if (bVerbose)
          {
            printf("taisec:                  %d\n", ADC64Subtype0[uiTN].taisec);
            printf("tainsecs:                %d\n", ADC64Subtype0[uiTN].tainsec);
            printf("flags:                   %d\n", ADC64Subtype0[uiTN].taiflags);
            printf("ch lower:                %d\n", ADC64Subtype0[uiTN].chlo);
            printf("ch upper:                %d\n", ADC64Subtype0[uiTN].chup);
          }

          data[uiTN].taisec  = ADC64Subtype0[uiTN].taisec;
          data[uiTN].tainsec = ADC64Subtype0[uiTN].tainsec;
        break;
        case 1:
          uint16_t ch = ADC64MStreamHdr[uiTN].ch;
          const uint16_t SN = (ADC64MStreamHdr[uiTN].length-2)*2;  // Number of samples
          data[uiTN].sn = SN;

          if (bVerbose)
            printf("\nch%d Buffer size: %d offset: %d end: %d\n",ch, BS, offset, end);

          ADC64Subtype1[uiTN].wf_tslo  = uiBuffer[offset];
          offset++;
          ADC64Subtype1[uiTN].wf_tsup  = uiBuffer[offset];
          offset++;


          data[uiTN].chstate[ch] = true;
          data[uiTN].chis.reset(ch);
          data[uiTN].wfts[ch]    = (uint64_t)ADC64Subtype1[uiTN].wf_tslo;
          data[uiTN].wfts[ch]   |= ((uint64_t)ADC64Subtype1[uiTN].wf_tsup) << 32;

          if (bVerbose)
          {
            printf("=== CH%d ===\n", ch);
            printf("wf ts low:            %d [%x]\n", ADC64Subtype1[uiTN].wf_tslo, ADC64Subtype1[uiTN].wf_tslo);
            printf("wf ts hi:             %d [%x]\n", ADC64Subtype1[uiTN].wf_tsup, ADC64Subtype1[uiTN].wf_tsup);
          }

          if (bVerbose)
            printf("\n=>ch%d Buffer size: %d offset: %d end: %d\n",ch, BS, offset, end);

          auto settings = std::find_if(vSetCH.begin(), vSetCH.end(),
                               [ch,devsn] (const ADCSETTINGS& in) {
                                  return in.ch == ch && in.devsn == devsn;
                               });
          if (settings==vSetCH.end())
          {
            printf("Achtung: ReadEvent: Device is not found in settings file\n"); //return ERROR_EVENT_READ;
            settings->inv = 1;
          }

          int16_t wave = 0;
          for (int s=0; s<(SN/2); s++)
          {
            auto ind = offset+s;     // dirrect cycle
            wave = ((uiBuffer[ind] & 0xFFFF0000) >> 16) * settings->inv + iSignalOffset;
            data[uiTN].wf[ch].push_back(wave);
            wave = (uiBuffer[ind] & 0xFFFF) * settings->inv + iSignalOffset;
            data[uiTN].wf[ch].push_back(wave);

            if ( (s<4 || s>(SN/2)-5) && bVerbose  )
              printf("#%04d [%d] word: %x\n", s,ind,uiBuffer[ind]);
          }
          offset += (SN/2);
          // uiCH++;
        break;
      } // subtype switch end

      end = BS - offset;
      if (end < 0) break;
    } // mstream loop end

    // event validation
    if (CHANNEL_NUMBER-data[uiTN].chis.count() != devPtr->ach.count())
    {
      printf("Achtung: Event#%04d is corrupted. Some channels are absence!\n",event);
      data[uiTN].valid = false; // incomplite event mark
      return ERROR_BAD_EVENT;
    }
    if (vData != NULL)
    {
      // PrintDataStruct(&data[uiTN]);
      vData->push_back(data[uiTN]);
    }

  } // device loop end

  return 0;
}

void AFIDecoder::GetAverageWave(std::vector<DATA> *vData, std::vector<DATA> *vAveData, bool bThreadSum)
{
  if (vData->size()!=vAveData->size())
  {
    printf("Achtung! Data[%zu] and AveData[%zu] vectors don't match!\n",vData->size(),vAveData->size());
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return;
  }
  for (auto itD=vADCs.begin(); itD!=vADCs.end(); itD++)
  {
    auto devsn = itD->devsn;
    auto ach = itD->ach;
    // printf("DevSN %x %x\n", vData->at(itD->devorn).devsn, vAveData->at(itD->devorn).devsn);
    auto dataPtr = std::find_if(vData->begin(), vData->end(),
      [devsn] (const DATA& data) {
        return data.devsn == devsn;
    });
    if (dataPtr==vData->end())
    {
      printf("Achtung! No matching data have been found!\n");
      if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
      return;
    }
    auto aveDataPtr = std::find_if(vAveData->begin(), vAveData->end(),
      [devsn] (const DATA& data) {
        return data.devsn == devsn;
    });
    if (aveDataPtr==vAveData->end())
    {
      printf("Achtung! No matching aveData have been found!\n");
      if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
      return;
    }
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (ach.test(ch))
      {
        // count events for each channel
        if (bThreadSum)
          aveDataPtr->evch[ch] += dataPtr->evch[ch];
        else
        {
          if (dataPtr->chis.test(ch))
          {
              aveDataPtr->evch[ch]++;//=dataPtr->chis.test(ch);     // true - event is ok
              // printf("inc_ch: %s\n", dataPtr->chis.to_string().c_str());
          }
        }
        // printf("ch: %d ev: %d [%d] | chis: %d\n", ch,aveDataPtr->evch[ch],bThreadSum,dataPtr->chis.test(ch));

        if (dataPtr->wf[ch].empty() || aveDataPtr->wf[ch].empty()){
          // printf("Achtung! Event#%4d CH%02d Data vector is empty!\n",uiEvent,ch);
          printf(" CH%02d Data vector is empty!\n",ch);
          if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
          return;
        }
        if (dataPtr->wf[ch].size() != aveDataPtr->wf[ch].size()){
          printf("Achtung! Data and aveData vectors mismatch!\n");
          if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
          return;
        }
        // printf("ch %02d sn %zu %zu\n", ch, dataPtr->wf[ch].size(), aveDataPtr->wf[ch].size());
        for(int s=0; s<itD->sn; s++)
        {
          aveDataPtr->wf[ch].at(s) += dataPtr->wf[ch].at(s);
        }
      }
    }
  } // end for itD
}

//
Float_t AFIDecoder::GetIntegralADC64(UInt_t devsn,UInt_t ch,std::vector<int32_t> *iWave, Bool_t bPedSubtr)
{
  if (iWave->empty()){
    printf("Achtung! Event#%4d CH%02d Data vector is empty!\n",uiEvent,ch);
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return Float_t(-1000);
  }
  Double_t fPedIntegral = 0;
  Double_t fSigIntegral = 0;
  Double_t fIntegral    = 0;
  Double_t fK           = 0;   // to compensate the length difference between pedestal and signal intervals

  auto gatePtr = std::find_if(vGate.begin(), vGate.end(),
                  [devsn, ch](const GATE &gate){
                    return gate.devsn==devsn && gate.ch==ch;
                  });
  if (gatePtr==vGate.end())
  {
    printf("Achtung: Device has not been found in %s!\n", GATE_FILENAME);
    return Float_t(-1000);
  }

  Int_t iPedStart = gatePtr->ped_start;
  Int_t iPedEnd = gatePtr->ped_end;
  Int_t iSigStart = gatePtr->sig_start;
  Int_t iSigEnd = gatePtr->sig_end;

  // integral of wave in range [] - all points are included
  //fPedIntegral = std::accumulate(vData->at(devorn).wf[iCH].begin() + iPedStart,\
                                 vData->at(devorn).wf[iCH].begin() + iPedEnd + 1, 0);
  //fSigIntegral = std::accumulate(vData->at(devorn).wf[iCH].begin() + iSigStart,\
                                 vData->at(devorn).wf[iCH].begin() + iSigEnd + 1,0);

   // to use inversion of signal
   //bool inverseison = false;
   // auto plus = [&](Float_t a, Float_t b){return a+(inversion ? -1 : 1)*(1.0/64.0)*b; };
   Double_t factor = 1.0/64.0;
   // Double_t factor = 1.0;
   if (SCALE_REPRESENTATION)
    factor = ADC64_BITS_TO_PC;
   auto plus = [&](Double_t a, Double_t b){return a+factor*b; };
   fPedIntegral = std::accumulate(iWave->begin() + iPedStart,\
                                  iWave->begin() + iPedEnd + 1, 0, plus);
   fSigIntegral = std::accumulate(iWave->begin() + iSigStart,\
                                  iWave->begin() + iSigEnd + 1, 0, plus);

  fK = Float_t(iSigEnd-iSigStart)/Float_t(iPedEnd-iPedStart);

  auto settings = std::find_if(vSetCH.begin(), vSetCH.end(),
                       [ch,devsn] (const ADCSETTINGS& in) {
                          return in.ch == ch && in.devsn == devsn;
                       });

  if (settings==vSetCH.end())
  {
   printf("Achtung! No matching polarity settings have been found!\n");
   return Float_t(-1000);
  }

  int polarity = 1;
  if (settings->inv) polarity = -1; // invertion of signal if it is negative

  if (bPedSubtr)
    fIntegral = (fSigIntegral - fPedIntegral*fK)*polarity;
  else
    fIntegral = fSigIntegral * polarity;

  // if (ch==2) printf("GET: Event%4d devsn: %d ch: %2d Ped: %f Sig: %f Int: %f K: %f SFactor: %f\n\n", \
          uiEvent,devsn,ch,fPedIntegral,fSigIntegral,fIntegral,fK,factor);

  return fIntegral;
}

// Integral computation
INTEGRAL * AFIDecoder::GetIntegral(UInt_t devsn,UInt_t ch,std::vector<int32_t> *wave,uint16_t thread_id)
{
  Integral[thread_id].clear();

  if (wave->empty()){
    printf("Achtung! Event#%4d CH%02d Data vector is empty!\n",uiEvent,ch);
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    Integral[thread_id].err = 1;
    return &Integral[thread_id];
  }

  auto gatePtr = std::find_if(vGate.begin(), vGate.end(),
                  [devsn, ch](const GATE &gate){
                    return gate.devsn==devsn && gate.ch==ch;
                  });
  if (gatePtr==vGate.end())
  {
    printf("Achtung: Device has not been found in %s!\n", GATE_FILENAME);
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    Integral[thread_id].err = 2;
    return &Integral[thread_id];
  }

  auto setPtr = std::find_if(vSetCH.begin(), vSetCH.end(),
                       [ch,devsn] (const ADCSETTINGS& in) {
                          return in.ch == ch && in.devsn == devsn;
                       });

  if (setPtr==vSetCH.end())
  {
   printf("Achtung! No matching polarity settings have been found!\n");
   if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
   Integral[thread_id].err = 3;
   return &Integral[thread_id];
  }

  // Factor for pedestal normalizing
  int sig_len = gatePtr->sig_end - gatePtr->sig_start; // length of signal gate
  Integral[thread_id].k  = Float_t(sig_len) / Float_t(gatePtr->ped_end - gatePtr->ped_start);

  Double_t afactor = 1.0;      // amplitude factor
  Double_t cfactor = 1.0;      // charge factor
  int polarity = 1;
  if (setPtr->inv)
    polarity = -1; // invertion of signal if it is negative  [inv: 1 - negative, -1 - positive]

  int sig_end_sh   = gatePtr->sig_start - GATE_SHIFT;       // step back from the start of signal gate and treat it as end of shifted interval
  int sig_start_sh = sig_end_sh - sig_len;                  // take the full length of gate from the end point

  // set device depended factors
  switch (DEVICE_TYPE)
  {
    case DEVICE_ID_ADC64:
      afactor = 1.0;
      cfactor = 1.0/64.0; // charge factor - 64 counts value is the least step along voltage axis
    break;
    case DEVICE_ID_TQDC:
      afactor = 1.0;
      cfactor = 1.0/4.0;
    break;
  }

  if (SCALE_REPRESENTATION)
  {
    switch (DEVICE_TYPE)
    {
      case DEVICE_ID_ADC64:
        afactor = ADC64_BITS_TO_MV;
        cfactor = ADC64_BITS_TO_PC;
      break;
      case DEVICE_ID_TQDC:
        afactor = TQDC_BITS_TO_MV;
        cfactor = TQDC_BITS_TO_PC;
      break;
    }
  }
  //

  for (int s=0; s<=gatePtr->sig_end; s++)
  {
    // Integral of Pedestal
    if (s>=gatePtr->ped_start && s<=gatePtr->ped_end)
    {
      Integral[thread_id].aped += Double_t(wave->at(s))*afactor;
      Integral[thread_id].cped += Double_t(wave->at(s))*cfactor;
    }
    // Integral of Shifted Signal with the same length of signal gate
    if (s>=sig_start_sh && s<=sig_end_sh)
    {
      Integral[thread_id].asig_sh += Double_t(wave->at(s))*afactor;
      Integral[thread_id].csig_sh += Double_t(wave->at(s))*cfactor;
    }
    // Integral of pure Signal
    if (s>=gatePtr->sig_start && s<=gatePtr->sig_end)
    {
      Integral[thread_id].asig += Double_t(wave->at(s))*afactor;
      Integral[thread_id].csig += Double_t(wave->at(s))*cfactor;
    }
  }
  Integral[thread_id].aped    /= float(gatePtr->ped_end - gatePtr->ped_start+1);
  Integral[thread_id].asig    /= float(sig_len+1);
  Integral[thread_id].asig_sh /= float(sig_len+1);
  Integral[thread_id].amp     = (Integral[thread_id].asig    - Integral[thread_id].aped)*polarity;
  Integral[thread_id].amp_sh  = (Integral[thread_id].asig_sh - Integral[thread_id].aped)*polarity;

  // Extract Integral[thread_id] = Signal - Pedestal * K
  Integral[thread_id].cint     = (Integral[thread_id].csig    - Integral[thread_id].cped * Integral[thread_id].k)*polarity;
  Integral[thread_id].cint_sh  = (Integral[thread_id].csig_sh - Integral[thread_id].cped * Integral[thread_id].k)*polarity;
  Integral[thread_id].cped    *= polarity;
  Integral[thread_id].csig    *= polarity;
  Integral[thread_id].csig_sh *= polarity;


  // if (ch==2 && Integral[thread_id].cint<150) printf("INT: Event%4d devsn: %d ch: %2d Ped: %f Sig: %f Int: %f K: %f SFactor: %f GATE: %d-%d %d-%d\n", \
          uiEvent,devsn,ch,Integral[thread_id].cped,Integral[thread_id].csig,Integral[thread_id].cint,Integral[thread_id].k,cfactor,\
        gatePtr->ped_start,gatePtr->ped_end,gatePtr->sig_start,gatePtr->sig_end);

  return &Integral[thread_id];
}


//
Float_t AFIDecoder::GetIntegral1(UInt_t devsn,UInt_t ch,std::vector<int32_t> *uiWave, GATE* gatePtr, Bool_t bPedSubtr)
{
  if (uiWave->empty()){
    printf("Achtung! GetIntegral(): uiWave is empty!\n");
    printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return Float_t(0xffffffff);
  }
  Float_t fPedIntegral = 0;
  Float_t fSigIntegral = 0;
  Float_t fIntegral    = 0;
  Float_t fK           = 0;   // to compensate the length difference between pedestal and signal intervals

  // auto gatePtr = std::find_if(vGate.begin(), vGate.end(),
  //                 [devsn, ch](const GATE &gate){
  //                   return gate.devsn==devsn && gate.ch==ch;
  //                 });
  // if (gatePtr==NULL)
  // {
  //   printf("Achtung: Device has not been found in %s!\n", GATE_FILENAME);
  //   return Float_t(0xffffffff);
  // }
  Int_t iPedStart = 0;
  Int_t iPedEnd   = 0;
  Int_t iSigStart = 0;
  Int_t iSigEnd   = 0;

  //
  iPedStart = gatePtr->ped_start;
  iPedEnd   = gatePtr->ped_end;
  iSigStart = gatePtr->sig_start;
  iSigEnd   = gatePtr->sig_end;

  // printf("dev 0x%x ch%02d sig_start %d\n", gatePtr->devsn, gatePtr->ch, gatePtr->sig_start);

  // integral of wave in range [] - all points are included
  //fPedIntegral = std::accumulate(vData->at(devorn).wf[iCH].begin() + iPedStart,\
                                 vData->at(devorn).wf[iCH].begin() + iPedEnd + 1, 0);
  //fSigIntegral = std::accumulate(vData->at(devorn).wf[iCH].begin() + iSigStart,\
                                 vData->at(devorn).wf[iCH].begin() + iSigEnd + 1,0);

   // to use inversion of signal
   //bool inverseison = false;
   // auto plus = [&](Float_t a, Float_t b){return a+(inversion ? -1 : 1)*(1.0/64.0)*b; };
   auto plus = [&](Float_t a, Float_t b){return a+(1.0/64.0)*b; };
   fPedIntegral = std::accumulate(uiWave->begin() + iPedStart,\
                                  uiWave->begin() + iPedEnd + 1, 0, plus);
   fSigIntegral = std::accumulate(uiWave->begin() + iSigStart,\
                                  uiWave->begin() + iSigEnd + 1, 0, plus);

  fK = Float_t(iSigEnd-iSigStart)/Float_t(iPedEnd-iPedStart);

  // auto inv = std::find_if(vSetCH.begin(), vSetCH.end(),
  //                      [ch,devsn] (const ADCSETTINGS& in) {
  //                         return in.ch == ch && in.devsn == devsn;
  //                      });
  //
  // if (inv==vSetCH.end())
  // {
  //  printf("Achtung! No matching polarity settings have been found!\n");
  //  return Float_t(0xfffffffe);
  // }

  if (bPedSubtr)
    fIntegral = fSigIntegral - fPedIntegral*fK;
  else
    fIntegral = fSigIntegral;

  // printf("Dev: %d ch: %d Ped: %.0f Sig: %.0f Int: %.0f K: %f\n", devorn,iCH,fPedIntegral,fSigIntegral,fIntegral,fK);

  return -fIntegral;
}

// Get integral function for TQDC
Float_t AFIDecoder::GetIntegralTQDC(UInt_t devsn,UInt_t ch,std::vector<int32_t> *iWave, Bool_t bPedSubtr)
{
  if (iWave->empty()){
    printf("Achtung! GetIntegralTQDC(): iWave is empty!\n");
    printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return Float_t(0xffffffff);
  }
  Float_t fPedIntegral = 0;
  Float_t fSigIntegral = 0;
  Float_t fIntegral    = 0;
  Float_t fK           = 0;   // to compensate the length difference between pedestal and signal intervals

  auto gatePtr = std::find_if(vGate.begin(), vGate.end(),
                  [devsn, ch](const GATE &gate){
                    return gate.devsn==devsn && gate.ch==ch;
                  });
  if (gatePtr==vGate.end())
  {
    printf("Achtung: Device has not been found in %s!\n", GATE_FILENAME);
    return Float_t(0xffffffff);
  }

  Int_t iPedStart = gatePtr->ped_start;
  Int_t iPedEnd = gatePtr->ped_end;
  Int_t iSigStart = gatePtr->sig_start;
  Int_t iSigEnd = gatePtr->sig_end;

  // integral of wave in range [] - all points are included
  //fPedIntegral = std::accumulate(vData->at(devorn).wf[iCH].begin() + iPedStart,\
                                 vData->at(devorn).wf[iCH].begin() + iPedEnd + 1, 0);
  //fSigIntegral = std::accumulate(vData->at(devorn).wf[iCH].begin() + iSigStart,\
                                 vData->at(devorn).wf[iCH].begin() + iSigEnd + 1,0);

   // to use inversion of signal
   //bool inverseison = false;
   // auto plus = [&](Float_t a, Float_t b){return a+(inversion ? -1 : 1)*(1.0/64.0)*b; };
   auto plus = [&](Float_t a, Float_t b){return a+TQDC_BITS_TO_PC*b; };
   fPedIntegral = std::accumulate(iWave->begin() + iPedStart,\
                                  iWave->begin() + iPedEnd + 1, 0, plus);
   fSigIntegral = std::accumulate(iWave->begin() + iSigStart,\
                                  iWave->begin() + iSigEnd + 1, 0, plus);

  fK = Float_t(iSigEnd-iSigStart)/Float_t(iPedEnd-iPedStart);

  auto inv = std::find_if(vSetCH.begin(), vSetCH.end(),
                       [ch,devsn] (const ADCSETTINGS& in) {
                          return in.ch == ch && in.devsn == devsn;
                       });

  if (inv==vSetCH.end())
  {
   printf("Achtung! No matching polarity settings have been found!\n");
   return Float_t(0xfffffffe);
  }
  int polarity = 0;
  if (inv->inv == 1)
    polarity = -1;
  else
    polarity = 1;

  if (bPedSubtr)
    fIntegral = (fSigIntegral - fPedIntegral*fK)*polarity;
  else
    fIntegral = fSigIntegral*polarity;

  // if (ch==0) printf("Dev: %x ch: %d Ped: %10f Sig: %10f Int: %10f K: %f ConvFactor: %f\n", \
          devsn,ch,fPedIntegral,fSigIntegral,fIntegral,fK,TQDC_BITS_TO_PC);

  return fIntegral;
}

Float_t AFIDecoder::GetAmplitude(uint16_t uiSN, uint32_t uiDevSN, uint8_t uiCH, std::vector<int32_t> *iWave)
{
  if (!iWave || !iWave->size()) {
    printf("Achtung: Data vector is empty\n");
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return -100000.0;
  }

  Int_t fAmp=0;
  auto inv = std::find_if(vSetCH.begin(), vSetCH.end(),
                       [uiCH,uiDevSN] (const ADCSETTINGS& in) {
                          return in.ch == uiCH && in.devsn == uiDevSN;
                       });

  if (inv==vSetCH.end())
  {
   printf("Achtung! No matching polarity settings have been found!\n");
   return -100000.0;
  }
  if (!uiSN)
  {
   printf("Achtung! Wrong sample number[%d]!\n",uiSN);
   return -100000.0;
  }

  for (int s=0; s<uiSN; s++)
  {
    auto data = iWave->at(s);
    if ( abs(data) > fAmp ) fAmp=abs(data);
  }
  if (inv->inv==1) fAmp *= -1;
  return float(fAmp);
}

void AFIDecoder::CopyMetaData(std::vector<DATA> *vData,std::vector<DATA> *vAveData,Int_t devorn)
{
  for(std::vector<DATA>::iterator d=vData->begin(); d!=vData->end(); d++)
  {
    vAveData->at(devorn).sn = d->sn;                // number of samples
    vAveData->at(devorn).devsn = d->devsn;          // serial number of device
    vAveData->at(devorn).devid = d->devid;          // device id
    vAveData->at(devorn).devorn = d->devorn;        // device order number

    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      vAveData->at(devorn).chstate[ch] = d->chstate[ch];  // on/off channel states
    }
  }
}

void AFIDecoder::CreateRoughKernel(std::vector<DATA> *vAveData)
{
  TFile *KernelFile;
  TTree *tree;

  Float_t fWave[CHANNEL_NUMBER];
  UInt_t uiCurSN = 0;
  UInt_t uiSN    = 0;
  UInt_t uiEventNum = 0;

  int devnum = 0;
  UInt_t uiDevSN = 0;

  for (auto itD=vADCs.begin(); itD!=vADCs.end(); itD++)
  {
    auto d     = itD->devorn;
    uiDevSN    = itD->devsn;
    uiSN       = itD->sn;
    uiEventNum = vAveData->at(d).ev;
    KernelFile = new TFile(Form("%s_%X.root",KERNEL_R_FILENAME,uiDevSN),"RECREATE");
    tree = new TTree(Form("%s_%X.root",KERNEL_R_FILENAME,uiDevSN),Form("%s_%X.root",KERNEL_R_FILENAME,uiDevSN));
    tree->Branch("sample_number",&uiCurSN,"sample_number/I");

    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itD->ach.test(ch))
      {
        fWave[ch] = 0;
        tree->Branch(Form("kernel_%x_ch%02d",uiDevSN,ch),&fWave[ch],Form("kernel_%x_ch%02d/F",uiDevSN,ch));
      }
    }
    for(int s=0; s<uiSN; s++)
    {
      uiCurSN = s;
      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        if (itD->ach.test(ch))
        {
          if (uiEventNum != 0)
            fWave[ch] = float(vAveData->at(d).wf[ch].at(s))/uiEventNum;
        }
      }
      tree->Fill();
    }

    KernelFile->Write();
    KernelFile->Close();
    printf("The rough kernel [%s_%X.root] has been stored successfully!\n", KERNEL_R_FILENAME,uiDevSN);

  }
  //
  // kernel format hints
  // kernels stores in the propriet ROOT format file [.root]
  // it has a tree with branches: [<device_sn>_chXX],
  // where device_sn and XX are written in the followint format for instance: [0xA7A314C_ch40]
  // The tree [dev00_kernel_r.root] structure:
  // sample N1    - <dev00_ch01 dev00_ch02 ... dev00_ch63>
  // sample N2    - <dev00_ch01 dev00_ch02 ... dev00_ch63>
  // .....................................................
  // sample N1023 - <dev00_ch01 dev00_ch02 ... dev00_ch63>

  // The tree [dev01_kernel_r.root] structure:
  // sample N1    - <dev01_ch01 dev01_ch02 ... dev01_ch63>
  // sample N2    - <dev01_ch01 dev01_ch02 ... dev01_ch63>
  // .....................................................
  // sample N1023 - <dev00_ch01 dev00_ch02 ... dev00_ch63>
  // ...
  //
}

void AFIDecoder::CreatePrecisedKernel(std::vector<DATA> *vAveData, const char * channels)
{
  TFile *KernelFile;
  TTree *tree;
  TBranch *fBranch;
  TKey *key;
  Float_t fWave = 0;
  UInt_t uiSN = 0;    // sample number
  UInt_t uiDevSN = 0; // device SN
  UInt_t uiDev = 0;   // device ID
  UInt_t uiID = 0;
  UInt_t uiNBranches = 0;
  bool   bIsSampleBranch = false;
  bool   bIsTree = true;
  bool   bIsDevice = false;

  if (channels != NULL)
  {
    sscanf(channels,"%x_ch%d.%d.%d.%d.%d.%d.%d.%d.%d.%d",&uiDevSN,&vUsedKernelChannels.at(0),\
                                                &vUsedKernelChannels.at(1),\
                                                &vUsedKernelChannels.at(2),\
                                                &vUsedKernelChannels.at(3),\
                                                &vUsedKernelChannels.at(4),\
                                                &vUsedKernelChannels.at(5),\
                                                &vUsedKernelChannels.at(6),\
                                                &vUsedKernelChannels.at(7),\
                                                &vUsedKernelChannels.at(8),\
                                                &vUsedKernelChannels.at(9));
    printf("The following channels [%s] will be used for the precised kernel.\n", channels);
  }
  for(auto k=vUsedKernelChannels.begin(); k!=vUsedKernelChannels.end(); k++)
  {
    if ((*k) < 0 || (*k) > CHANNEL_NUMBER-1)
    {
      printf("Achtung: Some kernel channels may be out of the [0-63] range: [%s]!\n",channels);
      return;
    }
  }

  printf("Checking precised kernel...\n");
  KernelFile = new TFile(Form("%s",KERNEL_P_FILENAME),"UPDATE");
  if (KernelFile==NULL)
  {
    printf("Achtung: Can't load the kernel file - %s!\n", KERNEL_P_FILENAME);
    return;
  }
  printf("Kernel file [%s]... OK\n", KERNEL_P_FILENAME);

  tree = (TTree *)KernelFile->Get(Form("%s",KERNEL_P_FILENAME));
  // tree->Print();
  if (tree==NULL)
  {
    printf("Achtung: Can't get access to the kernel tree - %s! Creating the new one...\n", KERNEL_P_FILENAME);
    tree = new TTree(Form("%s",KERNEL_P_FILENAME),Form("%s",KERNEL_P_FILENAME));
    bIsTree = false;
  }
  printf("Kernel tree [%s]... OK\n", KERNEL_P_FILENAME);
  printf("Looking for branches:\n");

  uiNBranches = tree->GetNbranches();
  uiID = uiNBranches;
  if (bIsTree) uiID--;
  printf("Number of branches: %d of 99\n", uiNBranches);
  if (uiNBranches >= 99)
  {
    printf("Achtung: The max number of branches is reached! Please relocate the kernel file [%s] from this folder\n", KERNEL_P_FILENAME);
    return;
  }

  TIter keyList(tree->GetListOfBranches());
  while ((key = (TKey*)keyList()))
  {
    printf("Branch: %s found\n", key->GetName());
    if (strcmp(key->GetName(),channels) == 0)
    {
      printf("Achtung: The branch [%s] already exists. Please change ID!\n", channels);
      return;
    }
    if (strcmp(key->GetName(),"sample_number") == 0) bIsSampleBranch = true;
  }

  if (!bIsSampleBranch) tree->Branch("sample_number",&uiSN,"sample_number/I");
  fBranch = tree->Branch(Form("%s_id%d",channels,uiID),&fWave,Form("%s_id%d/F",channels,uiID));

  for (auto itD=vADCs.begin(); itD!=vADCs.end(); itD++)
  {
    if ( itD->devsn == uiDevSN )
    {
      auto d = itD->devorn;
      auto sn = itD->sn;
      auto nevents = vAveData->at(d).ev;
      for (int k=0; k<KERNEL_NUMBER; k++)
      {
        if (!itD->ach.test(vUsedKernelChannels.at(k)))
        {
          printf("Achtung: The channel %d is not found among active channels! Please check the input line: [%s]!\n",vUsedKernelChannels.at(k),channels);
          return;
        }
      }
      bIsDevice = true;

      for (int s=0; s<sn; s++)
      {
        fWave = 0;
        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          if (itD->ach.test(CHANNEL_NUMBER-1-ch))
          {
            for (int k=0; k<KERNEL_NUMBER; k++)
            {
              if (CHANNEL_NUMBER-1-ch == vUsedKernelChannels.at(k))
              {
                uiSN = s*KERNEL_NUMBER+(KERNEL_NUMBER-1-k);

                if (nevents != 0) fWave = float(vAveData->at(d).wf[CHANNEL_NUMBER-1-ch].at(s))/float(nevents);
                // if (uiSN<5) printf("%d[%u] CH%d - Wave: %.3f\n", s,uiSN,CHANNEL_NUMBER-1-ch,fWave);
                if (!bIsTree) tree->Fill();
                else
                {
                  tree->GetEntry(uiSN);
                  fBranch->Fill();
                }
                break;
              }
            }
            // if (s<5) printf("S%d CH%d - Wave: %.3f\n", s,ch,fWave);
          }
        }
      }
      KernelFile->Write(0,TObject::kOverwrite);
      KernelFile->Print();
      KernelFile->ls();
    }
  }
  if (!bIsDevice)
  {
    printf("Achtung: ADC[%X] is not found in the data file!\nPlease check the input line: [%s]!\n",uiDevSN,channels);
  }
  else
    printf("The new branch with id%d has been stored to the kernel [%s] successfully!\n",uiID,KERNEL_P_FILENAME);
  KernelFile->Close();
  delete KernelFile;
}

bool AFIDecoder::LoadRoughKernel(const char *filename)
{
  TFile *tFile;
  TTree *tree;
  TBranch *bCH;
  TKey *key;
  int start = 0;
  int end   = 0;
  vRKernel.clear();

  for (auto itD=vADCs.begin(); itD!=vADCs.end(); itD++)
  {
    // flush kernel struct
    rKernel.sn    = 0;                                   // number of samples
    rKernel.devsn = 0;		                               	// device serial number
    rKernel.devorn = 0;		                              // device order number
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      rKernel.chstate[ch] = false;   // state of the kernel channel
      rKernel.wf[ch].clear();        // waveforms data
    }

    tFile = TFile::Open(Form("%s_%X.root", filename,itD->devsn));
    if (tFile==NULL) {
      printf("Achtung: Rough kernel file can't be opened.\n");
      if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
      return false;
    }
    // tFile->ls();
    // printf("%s\n", Form("%s_%X.root",gSystem->BaseName(filename),itD->devsn));
    tree = (TTree *)tFile->Get(Form("%s_%X.root",gSystem->BaseName(filename),itD->devsn));
    if (tree==NULL)
    {
      printf("Achtung: Rough kernel tree can't be accessed.\n");
      if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
      return false;
    }

    int iSampNum = tree->GetEntries();
    rKernel.sn = iSampNum;

    TIter keyList(tree->GetListOfBranches());

    int iDev = 0;
    uint32_t uiSN = 0;
    int iCH  = 0;
    Float_t value = 0;
    while ((key = (TKey*)keyList()))
    {
      if (strcmp(key->GetName(),"sample_number") == 0) continue;
      sscanf(key->GetName(),"kernel_%x_ch%d",&uiSN,&iCH);
      rKernel.chstate[iCH] = true;
      rKernel.devsn = uiSN;
      rKernel.devorn = iDev;
      bCH = tree->GetBranch(key->GetName());
      bCH->SetAddress(&value);

      auto rgatePos = std::find_if(vRKGate.begin(), vRKGate.end(),
                           [uiSN, iCH] (const GATE& rgate) {
                              return rgate.ch == iCH && rgate.devsn == uiSN;
                           });
      if (rgatePos==vRKGate.end()) {
        printf("Achtung: rough kernel gate can't be found.\n");
      }

      for (int s=rgatePos->sig_start; s<=rgatePos->sig_end; s++)
      {
         bCH->GetEntry(s);
         rKernel.wf[iCH].push_back(value);
      }
    }
    vRKernel.push_back(rKernel);
    printf("Loading kernel...      %s_%X.root [%d samples]\n", filename,itD->devsn,iSampNum);
  }
  tFile->Close();

  return true;
}

bool AFIDecoder::LoadPrecisedKernel(const char *filename, std::vector<PKGATE> *vPKGate)
{
  if (vPKGate == NULL)
  {
    printf("Achtung: Precised kernel gate vector is empty!\n");
    return false;
  }
  TFile *tFile;
  TTree *tree;
  TBranch *bCH;
  TKey *key;
  vPKernel.clear();

  // flash kernel struct
  pKernel.sn = 0;                                  // number of samples
  pKernel.nk = 0;		                             	// number of kernels in the file

  for (int k=0; k<KERNEL_MAXNUM_IN_FILE; k++)
  {
    pKernel.kid[k] = -1;
    pKernel.wf[k].clear();                             // waveforms data
  }

  tFile = TFile::Open(Form("%s",filename));
  if (tFile==NULL)
  {
    printf("Achtung: Precised kernel file can't be opened.\n");
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return false;
  }
  tree = (TTree *)tFile->Get(Form("%s",gSystem->BaseName(filename)));
  if (tree==NULL)
  {
    printf("Achtung: Precised kernel tree can't be accessed.\n");
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return false;
  }

  int iSampNum = tree->GetEntries();
  UInt_t uiBranchNum = tree->GetNbranches() - 1;
  pKernel.sn = iSampNum;
  pKernel.nk = uiBranchNum;

  TIter keyList(tree->GetListOfBranches());

  Float_t value = 0;
  UInt_t uiKC = 0; // kernel counter
  UInt_t uiStart = 0;
  UInt_t uiEnd   = 0;
  while ((key = (TKey*)keyList()))
  {
    if (strcmp(key->GetName(),"sample_number") == 0) continue;
    sscanf(key->GetName(),"%*x_ch%*d.%*d.%*d.%*d.%*d.%*d.%*d.%*d.%*d.%*d_id%d",&pKernel.kid[uiKC]);
    // printf("%d id: %d %s\n",uiKC,pKernel.kid[uiKC],key->GetName());
    bCH = tree->GetBranch(key->GetName());
    bCH->SetAddress(&value);
    uiStart = vPKGate->at(0).start[uiKC]*KERNEL_NUMBER;
    uiEnd   = vPKGate->at(0).end[uiKC]*KERNEL_NUMBER;
    for (int s=uiStart; s<=uiEnd; s++)
    {
       bCH->GetEntry(s);
       pKernel.wf[uiKC].push_back(value);
    }
    uiKC++;
  }
  vPKernel.push_back(pKernel);
  printf("Loading kernel...      %s [%d samples]\n",filename,iSampNum);

  tFile->Close();

  return true;
}

bool AFIDecoder::LoadGateSettings(const char * filename,int ndev)
{
  vGate.clear();
  const int ciLineSize = 128;
  char cLine[ciLineSize];

  FILE *fd;
  printf("Loading gate...        %s\n",filename);

  fd = fopen(filename,"r");
  if (!fd) { printf("Achtung: Gate file open error!\n"); return false; }

  while(!feof(fd))
  {
    Gate.devsn    = 0;
    Gate.ch       = 0;
    Gate.ped_start = 0;
    Gate.ped_end   = 0;
    Gate.sig_start = 0;
    Gate.sig_end   = 0;
    if (!fgets(cLine,ciLineSize,fd)) break;
    if (!strncmp(cLine,COMMENT_CHAR,1) || !strncmp(cLine,"\n",1)) continue;
    // printf("Line: %s",cLine);
    sscanf(cLine,"%x %d %d %d %d %d",&Gate.devsn,&Gate.ch,&Gate.ped_start,&Gate.ped_end,&Gate.sig_start,&Gate.sig_end);
    // printf("SN: %X CH%02d Pedestal: [%d - %d] Signal: [%d - %d] [points]\n",\
            Gate.devsn,Gate.ch,Gate.ped_start,Gate.ped_end,Gate.sig_start,Gate.sig_end);
    vGate.push_back(Gate);
  }
  fclose(fd);

  return true;
}

bool AFIDecoder::LoadPKernelGateSettings(const char * filename)
{
  vPKGate.clear();
  const int ciLineSize = 128;
  char cLine[ciLineSize];
  int start = 0;
  int end   = 0;
  int id    = 0;

  FILE *fd;
  printf("Loading kernel gate... %s\n",filename);

  fd = fopen(filename,"r");
  if (!fd) { printf("Achtung: Precised kernel gate file open error!\n"); return false; }

  while(!feof(fd))
  {
    start = 0;
    end   = 0;
    id    = 0;
    if (!fgets(cLine,ciLineSize,fd)) break;
    if (!strncmp(cLine,COMMENT_CHAR,1) || !strncmp(cLine,"\n",1)) continue;
    // printf("Line: %s",cLine);
    sscanf(cLine,"%d %d %d",&id,&start,&end);
    pkGate.start[id] = start;
    pkGate.end[id]   = end;
    // printf("ID#%02d Kernel signal location: [%d - %d] [points]\n",\
            id,pkGate.start[id],pkGate.end[id]);

  }
  fclose(fd);
  vPKGate.push_back(pkGate);

  return true;
}

bool AFIDecoder::LoadRKernelGateSettings(const char * filename)
{
  vRKGate.clear();
  const int ciLineSize = 128;
  char cLine[ciLineSize];

  FILE *fd;
  printf("Loading rough kernel gate... %s\n",filename);

  fd = fopen(filename,"r");
  if (!fd) { printf("Achtung: Rough kernel gate file open error!\n"); return false; }

  while(!feof(fd))
  {
    rkGate.ped_start = 0;
    rkGate.ped_end   = 0;
    if (!fgets(cLine,ciLineSize,fd)) break;
    if (!strncmp(cLine,COMMENT_CHAR,1) || !strncmp(cLine,"\n",1)) continue;
    // printf("Line: %s",cLine);
    sscanf(cLine,"%x %d %d %d",&rkGate.devsn,&rkGate.ch,&rkGate.sig_start,&rkGate.sig_end);
    vRKGate.push_back(rkGate);
  }
  fclose(fd);

  return true;
}

int AFIDecoder::LoadADCSettings(const char * filename)
{
  vSetCH.clear();
  const int ciLineSize = 128;
  char cLine[ciLineSize];

  FILE *fd;
  printf("Loading settings...    %s\n",filename);

  fd = fopen(filename,"r");
  if (!fd) { printf("Achtung: Settings file open error!\n"); return ERROR_CONFIGFILE_OPEN; }

  while(!feof(fd))
  {
    setCH.devsn    = 0;
    setCH.ch       = 0;
    setCH.type     = 0;
    setCH.side     = 0;
    setCH.plane    = 0;
    setCH.couple   = 0;
    setCH.eshape   = 0;
    setCH.sum      = 0;
    setCH.inv      = 1;
    setCH.delay    = 0;
    if (!fgets(cLine,ciLineSize,fd)) break;
    if (!strncmp(cLine,COMMENT_CHAR,1) || !strncmp(cLine,"\n",1)) continue;
    // printf("Line: %s",cLine);
    sscanf(cLine,"%x %d %c %c %c %c %c %d %d %f",&setCH.devsn,&setCH.ch,&setCH.type,\
                                            &setCH.side,&setCH.plane,&setCH.eshape, \
                                            &setCH.sum, &setCH.couple,\
                                            &setCH.inv,&setCH.delay);

    // printf("Dev#%x CH%02d type %d side %d plane %d couple %02d inv %d delay %f\n", devsn, ch, type, side, plane, couple, inv, delay);
    vSetCH.push_back(setCH);
  }
  fclose(fd);

  return 0;
}

void AFIDecoder::GenerateGateConfigFile(const char * filename,const char * gate,int ndev)
{
  int iGate[4] = {0,0,0,0};
  sscanf(gate,"%d:%d:%d:%d",&iGate[0],&iGate[1],&iGate[2],&iGate[3]);

  if (iGate[0] >= iGate[1] || iGate[2] >= iGate[3] ||\
      iGate[0] > iGate[2] || iGate[1] > iGate[2] ||\
      iGate[0] < 0 || iGate[1] <= 0 || iGate[2] <= 0 || iGate[3] <= 0)
  {
    printf("Achtung: Bad gate preset!\n");
    printf("Gate preset: %s -> [Pedestal: %d-%d Signal: %d-%d]\n", gate,iGate[0],iGate[1],iGate[2],iGate[3]);
    exit(-6);
  }

  FILE *fd = fopen(filename,"wt");
  fprintf(fd, "# Gate configuration file #\n");
  fprintf(fd, "# Dev# CH# PedStart PedEnd SigStart SigEnd # in points #\n");
  fprintf(fd, "# This file can be generated using: [./ADC64Viewer -f anyfile.data -m k -p 100:200::310:320] #\n");
  for (auto itD=vADCs.begin(); itD!=vADCs.end(); itD++)
  {
    fprintf(fd, "##### Device #%X #####\n", itD->devsn);
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      fprintf(fd,"%X %d %d %d %d %d\n", itD->devsn,ch,\
              iGate[0],iGate[1],\
              iGate[2],iGate[3]);
    }
  }
  fclose(fd);
  printf("\nGate file [%s] has been generatated successfully!\n", filename);
  printf("The following default values for each channel of devices are used:\n");
  printf("Pedestal:           %d - %d points\nSignal:             %d - %d points\n",\
              iGate[0],iGate[1],\
              iGate[2],iGate[3]);
}

void AFIDecoder::SaveGateConfigFile()
{
  // printf("%s\n", Form("%s",cGateFileName));
  FILE *fd = fopen(Form("%s", cGateFileName),"wt");
  fprintf(fd, "# Gate configuration file #\n");
  fprintf(fd, "# Dev# CH# PedStart PedEnd SigStart SigEnd # in points #\n");
  fprintf(fd, "# This file can be generated using: [./ADC64Viewer -f anyfile.data -m k -p 100:200::310:320] #\n");
  for (auto itG=vGate.begin(); itG!=vGate.end(); itG++)
  {
    fprintf(fd,"%X %d %d %d %d %d\n", itG->devsn,itG->ch,\
            itG->ped_start,itG->ped_end,\
            itG->sig_start,itG->sig_end);

            // printf("%X %d %d %d %d %d\n", itG->devsn,itG->ch,\
            //         itG->ped_start,itG->ped_end,\
            //         itG->sig_start,itG->sig_end);
  }
  fclose(fd);
}

void AFIDecoder::GenerateKernelGateConfigFile(const char * filename,const char * gate)
{
  int iGate[2] = {0,0};
  sscanf(gate,"%d:%d",&iGate[0],&iGate[1]);

  if (iGate[0] >= iGate[1] || iGate[0] < 0 || iGate[1] <= 0)
  {
    printf("Achtung: Bad kernel gate preset!\n");
    printf("Gate preset: %s -> [%d-%d]\n", gate,iGate[0],iGate[1]);
    exit(-6);
  }

  FILE *fd = fopen(filename,"wt");
  fprintf(fd, "# Precised kernel gate configuration file #\n");
  fprintf(fd, "# This file can be generated using: [./ADC64Viewer -f anyfile.data -m k -p 300:350] [300*10 - 350*10 ns] #\n");
  fprintf(fd, "# ID Start End # in points #\n");
  for (int k=0; k<KERNEL_MAXNUM_IN_FILE; k++)
  {
    fprintf(fd,"%d %d %d\n", k,iGate[0],iGate[1]);
  }
  fclose(fd);
  printf("\nPrecised kernel gate file [%s] has been generatated successfully!\n", filename);
  printf("The following default values for each kernel are used:\n");
  printf("Signal location:       %d - %d points\n",iGate[0],iGate[1]);
}

void AFIDecoder::SaveKernelGateConfigFile()
{
  FILE *fd = fopen(Form("%s/%s", cWorkDir, KERNEL_GATE_FILENAME),"wt");
  fprintf(fd, "# Precised kernel gate configuration file #\n");
  fprintf(fd, "# This file can be generated using: [./ADC64Viewer -f anyfile.data -m k -p 300:350] [300*10 - 350*10 ns] #\n");
  fprintf(fd, "# ID Start End # in points #\n");
  for (int k=0; k<KERNEL_MAXNUM_IN_FILE; k++)
  {
    fprintf(fd,"%d %d %d\n", k, vPKGate.at(0).start[k], vPKGate.at(0).end[k]);
  }
  fclose(fd);
}

UInt_t AFIDecoder::GetPeakPos(std::vector<int32_t> *uiWave)
{
  Int_t  fAmp  = 0;
  UInt_t uiPos = 0;

  for (int s=0; s<uiWave->size(); s++)
  {
    auto data = uiWave->at(s);
    if ( abs(data) > fAmp ) {
      fAmp  = abs(data);
      uiPos = s;
    }
  }
  return uiPos;
}

// Convolution function ///////////////////////////////////////////////////////
int AFIDecoder::GetConvMin(int devorn,int iCH,const uint16_t sn,std::vector<DATA> *vData,std::vector<KERNEL> *vRKernel)
{
  float fRes[sn];
  float fMin    = 100000;
  int   iMinPos = 0;
  int   iStart  = 0;
  int   iEnd    = 2048;

  if (vRKernel->at(devorn).chstate[iCH])
  {
    int I = sn - vRKernel->at(devorn).wf[iCH].size();
    auto peakPos = GetPeakPos(&(vData->at(devorn).wf[iCH]));
    iStart = peakPos-uiConvDiffLeft;
    iEnd   = peakPos+uiConvDiffRight;
    if (iStart<0 || iStart>=sn) iStart = 0;
    if (iEnd<0 || iEnd>=sn) iEnd = sn;
    // if (iCH==0) printf("iStart %d iEnd %d\n", iStart, iEnd);
    for (int i=iStart; i<iEnd; i++)
    {
      if (i<I)
      {
        fRes[i] = 0;
        for (int j=0; j<vRKernel->at(devorn).wf[iCH].size(); j++)
        {
          fRes[i] += vRKernel->at(devorn).wf[iCH].at(j) * vData->at(devorn).wf[iCH].at(j+i);
        }
      }
      else { fRes[i] = 0; fRes[i] = vData->at(devorn).wf[iCH].at(i); }
      if (fRes[i] < fMin) { fMin = fRes[i]; iMinPos = i; }
    }
  }

  return iMinPos;
}

int AFIDecoder::GetPrecisedConvMin(int devorn,UInt_t kind,int iCH,\
                                    const uint16_t sn,std::vector<DATA> *vData,\
                                    std::vector<KERNEL_PRECISED> *vPKernel,\
                                    std::vector<PKGATE> *vPKGate)
{
  if (!vData) {
    printf("Achtung: Event#%04d CH%02d Data vector is NULL\n",uiEvent,iCH);
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return -1;
  }
  if (!vPKernel) {
    printf("Achtung: Event#%04d CH%02d Kernel vector is NULL\n",uiEvent,iCH);
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return -1;
  }
  if (!vPKGate) {
    printf("Achtung: Event#%04d CH%02d Gate vector is NULL\n",uiEvent,iCH);
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return -1;
  }

  if (vData->empty()) {
    printf("Achtung: Event#%04d CH%02d Data vector has zero size\n",uiEvent,iCH);
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return -1;
  }
  if (vPKernel->empty()) {
    printf("Achtung: Event#%04d CH%02d Kernel vector has zero size\n",uiEvent,iCH);
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return -1;
  }
  if (vPKGate->empty()) {
    printf("Achtung: Event#%04d CH%02d Gate vector has zero size\n",uiEvent,iCH);
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return -1;
  }

  if (vData->at(devorn).wf[iCH].empty()) {
    printf("Achtung: Event#%04d CH%02d Waveform data vector is empty\n",uiEvent,iCH);
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return -1;
  }
  if (vPKernel->at(0).wf[kind].empty()) {
    printf("Achtung: Event#%04d CH%02d Kernel data vector is empty\n",uiEvent,iCH);
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return -1;
  }

  float fRes[sn*KERNEL_NUMBER];
  float fNewWave[sn*KERNEL_NUMBER];
  float fMin    = 100000;
  int   iMinPos = 0;
  int   ksn = 0;
  UInt_t uiStart = 0;
  UInt_t uiEnd   = 0;
  ksn = vPKernel->at(0).wf[kind].size();
  // uiStart = vPKGate->at(0).start[kind]*KERNEL_NUMBER + KERNEL_CONV_RANGE_EXTENSION_START;
  // uiEnd   = vPKGate->at(0).end[kind]*KERNEL_NUMBER   + KERNEL_CONV_RANGE_EXTENSION_END;
  auto peakPos = GetPeakPos(&(vData->at(devorn).wf[iCH]));
  uiStart = (peakPos-uiConvDiffLeft)*KERNEL_NUMBER;
  uiEnd   = (peakPos+uiConvDiffRight)*KERNEL_NUMBER;
  if (uiStart<0 || uiStart>=sn*KERNEL_NUMBER) uiStart = 0;
  if (uiEnd<0 || uiEnd>=sn*KERNEL_NUMBER) uiEnd = sn*KERNEL_NUMBER;
  // if (iCH==0) printf("peakPos %d iStart %d iEnd %d\n", peakPos, uiStart, uiEnd);
  for (int s=0; s<sn; s++)
  {
    for (int k=0; k<KERNEL_NUMBER; k++)
    {
      fRes[KERNEL_NUMBER*s+k]     = vData->at(devorn).wf[iCH].at(s);
      fNewWave[KERNEL_NUMBER*s+k] = vData->at(devorn).wf[iCH].at(s);
    }
  }

  // smooth of the expanded data array
  if (KERNEL_SMOOTH) Smooth(sn*KERNEL_NUMBER,fNewWave,\
                            KERNEL_SMOOTH_RANGE,uiStart,uiEnd);

  int I = sn*KERNEL_NUMBER - ksn;
  for (int i=uiStart; i<uiEnd; i++)
  {
    if (i<I)
    {
      for (int j=0; j<ksn; j++)
      {
        fRes[i] += (vPKernel->at(0).wf[kind].at(j) * KERNEL_INVERSION) * fNewWave[j+i];
      }
    }
    // else
    // {
    //   fRes[i] = vData->at(devorn).wf[iCH].at(i);
    // }
    if (fRes[i] < fMin) { fMin = fRes[i]; iMinPos = i; }
  }

  // printf("Event#%04d CH%02d %d %d %d\n",uiEvent,iCH, iMinPos,uiStart,uiEnd);

  // Uncomment this snippet to make convolution perfomance
  // to see results use [root -l drawconv.cpp] macro
  if (false)
  {
    if (uiEvent == 1 && iCH == 32)
    {
      FILE *fd = fopen("conv.txt","wt");
      for (int s=0; s<sn*KERNEL_NUMBER; s++)
      {
        // printf("%d %f [wave: %f]\n", s,fRes[s],fNewWave[s]);
        printf("%d %f\n", s,fRes[s]);
        // fprintf(fd,"%d %f\n", s,fRes[s]);
        fprintf(fd,"%d %f\n", s,fNewWave[s]);
      }
      fclose(fd);
      printf("%d [%f] %d %d\n", iMinPos, fMin, uiStart,uiEnd);
      exit(-1);
    }
  }

  return iMinPos;
}

void AFIDecoder::Smooth(const UInt_t sn, float *fWave,int iNPoints,float fStart,float fEnd)
{
  // printf("start %.2f end %.2f\n", fStart, fEnd);
  int iNP = 0; // number of points in the range

  if (iNPoints != 0)
  {
    for (int s=fStart; s<fEnd; s++)
    {
      // if (s<(fStart+100)) printf("%.2f ", fWave[s]);
      if (s!=0 || s!=sn-1)
      {
        for (int i=1; i<=iNPoints; i++)
        {
          if ((s-i)>=0) fWave[s] += fWave[s-i];
          if ((s+i)<sn) fWave[s] += fWave[s+i];
          iNP++;
          if (s-i == 0 || s+i == sn-1) break;
        }
        fWave[s] /= double(2*iNP+1);
        iNP = 0;
      } // end range if
    } // end sample for
    // printf("\n");
  } // if enabled
}

// Normalization function for the rough kernel ////////////////////////////////
void AFIDecoder::NormalizeRoughKernel(std::vector<KERNEL> *vRKernel)
{
  float fKernelNorm[CHANNEL_NUMBER];
  for(auto itK=vRKernel->begin(); itK!=vRKernel->end(); itK++)
  {
    auto devsn = itK->devsn;
    // sum
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      fKernelNorm[ch] = 0;
      if (itK->chstate[ch])
      {
        for(auto s=itK->wf[ch].begin(); s!=itK->wf[ch].end(); s++)
          fKernelNorm[ch] += (*s);
      }
    }
    // normalize
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (itK->chstate[ch])
      {
        auto setPtr = std::find_if(vSetCH.begin(), vSetCH.end(),
        [ch,devsn] (const ADCSETTINGS& in) {
          return in.ch == ch && in.devsn == devsn;
        });
        for(auto s=itK->wf[ch].begin(); s!=itK->wf[ch].end(); s++)
          (*s) /= fKernelNorm[ch] * (-setPtr->inv);
      }
    }
  }
}

// Normalization function for the precised kernel//////////////////////////////
void AFIDecoder::NormalizePrecisedKernel(std::vector<KERNEL_PRECISED> *vPKernel)
{
  int devnum = 0;
  float fKernelNorm[KERNEL_MAXNUM_IN_FILE];
  for(std::vector<KERNEL_PRECISED>::iterator d=vPKernel->begin(); d!=vPKernel->end(); d++,devnum++)
  {
    // sum
    for (int k=0; k<KERNEL_MAXNUM_IN_FILE; k++)
    {
      fKernelNorm[k] = 0;
      if (d->kid[k] != -1)
      {
        for(std::vector<float>::iterator s=d->wf[k].begin(); s!=d->wf[k].end(); s++)
          fKernelNorm[k] += (*s);
      }
    }
    // normalize
    for (int k=0; k<KERNEL_MAXNUM_IN_FILE; k++)
    {
      if (d->kid[k] != -1)
      {
        for(std::vector<float>::iterator s=d->wf[k].begin(); s!=d->wf[k].end(); s++)
          (*s) /= fKernelNorm[k] * KERNEL_INVERSION;
      }
    }
  }
}

bool AFIDecoder::GetPol1Coeffs(std::vector<stFITARR> *data, double *K, double *B)
{
  if (data->empty()) return false;

  (*K) = 0;
  (*B) = 0;

  std::vector<stFITARR>::iterator p;
  const int N = data->size();
  double dDelta = 0;      // Delta = N * SUM(x^2) - (SUM(x))^2
  double dSumOfX2 = 0;    // SUM(x^2)
  double dXSum = 0;       // SUM(x)
  double dYSum = 0;       // SUM(y)
  double dXYSum = 0;      // SUM(xy)

  for(p=data->begin(); p!=data->end(); p++)
  {
      dXSum += p->x;
      dYSum += p->y;
      dXYSum += p->x * p->y;
      dSumOfX2 += pow(p->x,2);
  }

  dDelta = N * dSumOfX2 - pow(dXSum,2);
  (*B) = (dSumOfX2*dYSum - dXSum*dXYSum)/dDelta;
  (*K) = (N*dXYSum - dXSum*dYSum)/dDelta;

  return true;
}

Double_t AFIDecoder::GetTimeFitCrossPoint(UInt_t devsn, UInt_t ch, UInt_t uiSN, std::vector<int32_t> *uiWave)
{
  // TODO: Error if does not exist
  auto gatePtr = std::find_if(vGate.begin(), vGate.end(),
                  [devsn, ch](const GATE &gate){
                    return gate.devsn==devsn && gate.ch==ch;
                  });

  int iSt = gatePtr->ped_start;
  int iEn = gatePtr->ped_end;

  Float_t fBLLevel  = 0;
  Float_t fMin    = 100000;
  Int_t   iMinPos = 0;
  Int_t   iLoPos  = 0;
  Int_t   iHiPos  = 0;
  Float_t fAmp    = 0;
  Float_t fAmpLo  = 0;
  Float_t fAmpHi  = 0;

  stFITARR fitarr;
  fitarr.x = 0;
  fitarr.y = 0;
  std::vector<stFITARR> vFitArr;
  vFitArr.clear();

  // Gettting the base line level as an average in range
  for (int s=iSt; s<=iEn; s++)
  {
    fBLLevel += uiWave->at(s)/float(iEn-iSt+1);
  }

  for (int s=iEn+1; s<uiSN; s++)
  {
    if (uiWave->at(s) < fMin) { fMin = uiWave->at(s); iMinPos = s; }
  }

  fAmp   = fBLLevel - fMin;
  fAmpLo = fBLLevel - fAmp * FRONT_LEVEL_LO;   // 0.1
  fAmpHi = fBLLevel - fAmp * FRONT_LEVEL_HI;   // 0.9

  for (int s=0; s<=iMinPos; s++)
  {
    if (uiWave->at(iMinPos-s) <= fAmpHi) iHiPos = iMinPos-s;
    if (uiWave->at(iMinPos-s) <= fAmpLo) iLoPos = iMinPos-s;
    if (iMinPos-s < iEn) break;
  }

  for (int s=iLoPos-1; s<iHiPos; s++)
  {
    fitarr.x = float(s);
    fitarr.y = uiWave->at(s);
    vFitArr.push_back(fitarr);
    // printf("X: %f  Y: %f\n", fitarr.x,fitarr.y);
  }

  // printf("0.1 - %f[%d] 0.9 - %f[%d]\n", fAmpLo,iLoPos,fAmpHi,iHiPos);
  // printf("BL: %.2f Min: %.2f[%d] Amp.: %.2f AmpLo.: %.2f AmpHi.: %.2f\n",fBLLevel,fMin,iMinPos,fAmp,fAmpLo,fAmpHi);

  // parameters
  double dk0 = 0;
  double db0 = fBLLevel;
  double dk1 = 0;
  double db1 = 0;
  double dXCP = 0;
  double dYCP = 0;

  if (GetPol1Coeffs(&vFitArr,&dk1,&db1) == false) printf("Achtung: Bad function parameters! Data array may be empty.\n");

  dXCP = (db1 - db0) / (dk0 - dk1);
  dYCP = dk1 * dXCP + db1;

  // printf("Time:       %.2f ns  |  Cross point[X,Y]: [%.2f,%.2f]\n",dXCP,dXCP,dYCP);

  return dXCP;
}

Double_t AFIDecoder::GetTimeFitCrossPointImproved(UInt_t devsn,UInt_t ch,std::vector<int32_t> *uiWave, Float_t *fAmplitude)
{
  if (!uiWave) {
    printf("Achtung: Data vector is empty\n");
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return -1;
  }
  UInt_t uiSN = uiWave->size();
  if (!uiSN) {
    printf("Achtung: Data vector has 0 size\n");
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return -1;
  }

  // TODO: Error if does not exist
  auto gatePtr = std::find_if(vGate.begin(), vGate.end(),
                  [devsn, ch](const GATE &gate){
                    return gate.devsn==devsn && gate.ch==ch;
                  });
  if (gatePtr==vGate.end())
  {
    printf("Achtung: Device has not been found in %s!\n", GATE_FILENAME);
    if (DEBUG_MODE) printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    return -1;
  }

  int iSt = gatePtr->ped_start;
  int iEn = gatePtr->ped_end;
  int iSigStart = gatePtr->sig_start;
  int iSigEnd = gatePtr->sig_end;

  if (iSt>uiSampleNumber) iSt = 0;
  if (iEn>uiSampleNumber) iEn = iSt+1;
  if (iSigStart>uiSampleNumber) iSigStart = iEn + 1;
  if (iSigEnd>uiSampleNumber) iSigEnd = uiSampleNumber;

  Float_t fBLLevel  = 0;
  Float_t fMin    = 100000;
  Int_t   iMinPos = 0;
  Int_t   iLoPos  = 0;
  Int_t   iHiPos  = 0;
  Float_t fAmp    = 0;
  Float_t fAmpLo  = 0;
  Float_t fAmpHi  = 0;
  Float_t fNewWave[uiSN*KERNEL_NUMBER];
  iSt *= KERNEL_NUMBER;   // pedestal first point
  iEn *= KERNEL_NUMBER;   // pedestal last point
  iSigStart *= KERNEL_NUMBER;   // signal first point
  iSigEnd *= KERNEL_NUMBER;   // signal last point

  for (int s=0; s<uiSN; s++)
  {
    for (int k=0; k<KERNEL_NUMBER; k++)
    {
      fNewWave[KERNEL_NUMBER*s+k] = Float_t(uiWave->at(s));
    }
  }
  // smooth of the expanded data array
  Smooth(uiSN*KERNEL_NUMBER,fNewWave,KERNEL_SMOOTH_RANGE,iSt,iSigEnd);

  // Uncomment this snippet to see the improved wave perfomance
  // to see results use [root -l drawconv.cpp] macro
  if (false)
  {
    if (uiEvent == 1)
    {
      FILE *fd = fopen("conv.txt","wt");
      for (int s=0; s<uiSN*KERNEL_NUMBER; s++)
      {
        fprintf(fd,"%d %f\n", s,fNewWave[s]);
      }
      fclose(fd);
      // printf("%d [%f] %d %d\n", iMinPos, fMin, uiStart,uiEnd);
      exit(-1);
    }
  }

  stFITARR fitarr;
  fitarr.x = 0;
  fitarr.y = 0;
  std::vector<stFITARR> vFitArr;
  vFitArr.clear();

  // Gettting the base line level as an average in range
  for (int s=iSt; s<=iEn; s++)
  {
    fBLLevel += fNewWave[s];
    // if (uiEvent == 1) printf("Event#%d %d %f\n", uiEvent,s,fNewWave[s]);
  }
  fBLLevel /= float(iEn-iSt+1);
  // if (uiEvent < 10) printf("Event#%d Level: %f\n", uiEvent,fBLLevel);

  // searching a min value of the waveform
  for (int s=iEn+1; s<uiSN*KERNEL_NUMBER; s++)
  {
    if (fNewWave[s] < fMin) { fMin = fNewWave[s]; iMinPos = s; }
  }

  // getting amplitude parameters
  fAmp   = fBLLevel - fMin;
  fAmpLo = fBLLevel - fAmp * FRONT_LEVEL_LO;   // 0.1 of amplitude
  fAmpHi = fBLLevel - fAmp * FRONT_LEVEL_HI;   // 0.9 of amplitude

  // determination of front position
  for (int s=0; s<=iMinPos; s++)
  {
    if (fNewWave[iMinPos-s] <= fAmpHi) iHiPos = iMinPos-s;
    if (fNewWave[iMinPos-s] <= fAmpLo) iLoPos = iMinPos-s;
    if (iMinPos-s < iEn) break;
  }

  // extracting front fit data array
  for (int s=iLoPos-1; s<iHiPos; s++)
  {
    fitarr.x = float(s);
    fitarr.y = fNewWave[s];
    vFitArr.push_back(fitarr);
    // if (uiEvent == 1) printf("Event#%d X: %f  Y: %f\n", uiEvent,fitarr.x,fitarr.y);
  }

  // printf("0.1 - %f[%d] 0.9 - %f[%d]\n", fAmpLo,iLoPos,fAmpHi,iHiPos);
  // printf("BL: %.2f Min: %.2f[%d] Amp.: %.2f AmpLo.: %.2f AmpHi.: %.2f\n",fBLLevel,fMin,iMinPos,fAmp,fAmpLo,fAmpHi);

  // parameters
  Double_t dk0 = 0;         //
  Double_t db0 = fBLLevel;  // mean of baseline
  Double_t dk1 = 0;
  Double_t db1 = 0;
  Double_t dXCP = 0;
  Double_t dYCP = 0;

  if (GetPol1Coeffs(&vFitArr,&dk1,&db1) == false) printf("Achtung: Bad function parameters! Data array may be empty.\n");

  // dXCP = (db1 - db0) / (dk0 - dk1);
  dXCP = (db0 - db1) / dk1;
  dYCP = dk1 * dXCP + db1;

  // if (uiEvent == 1) printf("Time:       %.2f ns  |  Cross point[X,Y]: [%.2f,%.2f]\n",dXCP,dXCP,dYCP);

  (*fAmplitude) = fMin;
  //printf("Event#%d A: %f T: %f\n",uiEvent, (*fAmplitude), dXCP);
  if (dXCP <= 0) dXCP = 0;

  return dXCP;
}

void AFIDecoder::PrintDataStruct(DATA *d)
{
  printf("====== Event#%05d ======\n", d->ev);
  printf("Device SN:          %x\n", d->devsn);
  printf("Device ID:          %x\n", d->devid);
  printf("Device order number:%d\n", d->devorn);
  printf("Number of samples:  %d\n", d->sn);
  printf("tai sec:            %d\n", d->taisec);
  printf("tai nsec:           %d\n", d->tainsec);

  for (int ch=0; ch<CHANNEL_NUMBER; ch++)
  {
    if (d->chstate[ch])
    {
      #ifdef __linux__
        printf("CH%02d [%d] wfts: %lu\n",ch+1,d->chstate[ch],d->wfts[ch]);  // for linux
      #else
        printf("CH%02d [%d] wfts: %llu\n",ch+1,d->chstate[ch],d->wfts[ch]);     // for mac
      #endif
      for(std::vector<int32_t>::iterator p=d->wf[ch].begin(); p!=d->wf[ch].end(); p++)
        printf("Wave: %d\n", (*p));
    }
  }
}

void AFIDecoder::PrintMetaData(std::vector<ADCS> *meta, Short_t index, Bool_t bPrintMap)
{
  if (!meta){
    printf("The metadata vector is empty!\n");
    return;
  }
  printf("====== Meta data ======\n");
  for (int i=0; i<meta->size(); i++){
    if (((index>-1) && (i==index)) || (index<0)) {
      printf("Total events:          %d\n", meta->at(i).ev);
      printf("Device SN:             %x\n", meta->at(i).devsn);
      printf("Device ID:             %x\n", meta->at(i).devid);
      printf("Device order number:   %d\n", meta->at(i).devorn);
      printf("Number of samples:     %d\n", meta->at(i).sn);
      printf("Unix time:             %lld ms\n", meta->at(i).rectime);
      printf("File:                  %s/%s\n", meta->at(i).dir, meta->at(i).file);
      printf("Run tag:               %s\n", cRunTag);

      printf("Active channels:       ");
      for (int ch=0; ch<CHANNEL_NUMBER; ch++) printf("%d", meta->at(i).ach.test(ch));
      printf("\n");

      if (bPrintMap)
      {
        printf("Channel number:        ");
        for (int ch=0; ch<CHANNEL_NUMBER; ch++) printf("%d", int(ch/10));
        printf("\n");

        printf("                       ");
        for (int ch=0; ch<CHANNEL_NUMBER; ch++) printf("%d", ch%10);
        printf("\n");

        printf("Channel types:         ");
        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          auto cfgPtr = std::find_if(meta->at(i).cfg.begin(), meta->at(i).cfg.end(),
                          [ch](const ADCSETTINGS &cfg){
                            return cfg.ch==ch;
                          });
          printf("%c", cfgPtr->type);
        }
        printf("\n");

        printf("Channel side:          ");
        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          auto cfgPtr = std::find_if(meta->at(i).cfg.begin(), meta->at(i).cfg.end(),
                          [ch](const ADCSETTINGS &cfg){
                            return cfg.ch==ch;
                          });
          printf("%c", cfgPtr->side);
        }
        printf("\n");

        printf("Channel plane:         ");
        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          auto cfgPtr = std::find_if(meta->at(i).cfg.begin(), meta->at(i).cfg.end(),
                          [ch](const ADCSETTINGS &cfg){
                            return cfg.ch==ch;
                          });
          printf("%c", cfgPtr->plane);
        }
        printf("\n");

        printf("Channel sum:           ");
        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          auto cfgPtr = std::find_if(meta->at(i).cfg.begin(), meta->at(i).cfg.end(),
                          [ch](const ADCSETTINGS &cfg){
                            return cfg.ch==ch;
                          });
          printf("%c", cfgPtr->sum);
        }
        printf("\n");

        printf("E-shape number:        ");
        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          auto cfgPtr = std::find_if(meta->at(i).cfg.begin(), meta->at(i).cfg.end(),
                          [ch](const ADCSETTINGS &cfg){
                            return cfg.ch==ch;
                          });
          printf("%c", cfgPtr->eshape);
        }
        printf("\n");
      }
    }
  }
  printf("=======================\n");
}

Bool_t AFIDecoder::FindPeaks(uint32_t devsn, uint8_t ch, std::vector<int32_t> *uiWave, Float_t fThr)
{
  if (!uiWave) {
    printf("Achtung: Wave vector does not exist!\n");
    return false;
  }
  UInt_t uiSN = uiWave->size();
  if (!uiSN) {
    printf("Achtung: Wave vector is empty!\n");
    return false;
  }
  Float_t fBaselineSumAve = 0;
  Int_t iBaselineCount = 0;
  vPeaks.clear();
  wfPeaks.n     = 0;
  wfPeaks.maxpos = 0;
  wfPeaks.imp_maxpos = 0;
  wfPeaks.amp   = 0;
  wfPeaks.imp_amp   = 0;
  wfPeaks.first = 0;
  wfPeaks.last  = 0;
  Bool_t  bSS     = 0;  // first condition
  Bool_t  bSE     = 0;  // second condition
  Float_t fMin    = 0;  // amplitude of min in the peak
  Float_t fMax    = 0;  // amplitude of max in the peak
  Bool_t  bIsUnderThr = false;

  auto ptrSet = std::find_if(vSetCH.begin(), vSetCH.end(),
                       [ch,devsn] (const ADCSETTINGS& in) {
                          return in.ch == ch && in.devsn == devsn;
                       });

  if (ptrSet==vSetCH.end())
  {
   printf("Achtung! No matching polarity settings have been found!\n");
   return false;
  }
  // Looking for any amplitude that is under the threshold
  for (int s=0; s<uiSN; s++)
  {
    if (uiWave->at(s)>=-BASELINE_VARIATION_LIMIT && uiWave->at(s)<=BASELINE_VARIATION_LIMIT)
    {
      fBaselineSumAve += uiWave->at(s);
      iBaselineCount++;
    }
    switch (ptrSet->inv)
    {
      case 1:
        if (uiWave->at(s) < fMin) { fMin = uiWave->at(s); }
      break;
      case -1:
        if (uiWave->at(s) > fMax) { fMax = uiWave->at(s); }
      break;
    }
  }
  fThr *=-ptrSet->inv;

  // check if there is a signal
  if (!(abs(fMin) > abs(fThr) || abs(fMax) > abs(fThr))) return false;

  // improve discretization
  const unsigned int sn = uiSN*TIME_DISCRETIZATION;
  float fNewWave[sn];
  for (int s=0; s<uiSN; s++)
  {
    for (int k=0; k<TIME_DISCRETIZATION; k++)
    {
      fNewWave[TIME_DISCRETIZATION*s+k]=float(uiWave->at(s));
    }
  }
  Smooth(sn, fNewWave, KERNEL_SMOOTH_RANGE, 0, sn);
  fBaselineSumAve /= iBaselineCount;
  fMin = 0;
  fMax = 0;
  // Extract peaks location and parameters
  for (int s=0; s<sn; s++)
  {
    int k = s/TIME_DISCRETIZATION;
    switch (ptrSet->inv)
    {
      case 1:
        if (fNewWave[s] < fThr ) { bSS = true;  if (wfPeaks.first == 0) wfPeaks.first = s; }
        if (bSS == true && fNewWave[s] > fThr) { bSE = true; if (wfPeaks.last == 0) wfPeaks.last = s; }
        if (bSS == true && s == sn-1) { bSE = true; if (wfPeaks.last == 0) wfPeaks.last = s; } // if end of gate reached before threshold leaving
        if (bSS == true && bSE != true)
        {
          if (fNewWave[s] < wfPeaks.imp_amp) { wfPeaks.imp_amp = fNewWave[s]; wfPeaks.imp_maxpos = s; }

          if (!(s%TIME_DISCRETIZATION))
          {
            if (uiWave->at(k) < wfPeaks.amp) { wfPeaks.amp = uiWave->at(k);}
          }
          // printf("#%d - W: %.1f Min: %.1f SS: %d[%d] SE: %d[%d]\n", s, fNewWave[s], wfPeaks.imp_amp, bSS,wfPeaks.first,bSE,wfPeaks.last);
        }
      break;
      case -1:
        if (fNewWave[s] > fThr ) { bSS = true;  if (wfPeaks.first == 0) wfPeaks.first = s; }
        if (bSS == true && fNewWave[s] < fThr ) { bSE = true; if (wfPeaks.last == 0) wfPeaks.last = s; }
        if (bSS == true && s == sn-1) { bSE = true; if (wfPeaks.last == 0) wfPeaks.last = s; } // if end of gate reached before threshold leaving
        if (bSS == true && bSE != true)
        {
          if (fNewWave[s] > wfPeaks.imp_amp) { wfPeaks.imp_amp = fNewWave[s]; wfPeaks.imp_maxpos = s; }
          if (!(s%TIME_DISCRETIZATION))
          {
            if (uiWave->at(k) > wfPeaks.amp) { wfPeaks.amp = uiWave->at(k);  }
          }
        }
      break;
    }

    if (bSS == true && bSE == true)
    {
      wfPeaks.n++;
      wfPeaks.amp-=fBaselineSumAve;

      if ( wfPeaks.last - wfPeaks.first > 2)  // to refuse signals shorter than 2 points
      {
        if (true) printf("\nPeak%02d @%4d - Ampitude: %.2f Baseline: %.2f Location: %4d - %d samples  Threshold: %.2f\n",\
         wfPeaks.n, wfPeaks.maxpos,wfPeaks.amp,fBaselineSumAve,wfPeaks.first,wfPeaks.last,fThr);
        bool res = GetWFPars(sn, ptrSet->inv, fBaselineSumAve, fNewWave, wfPeaks.imp_maxpos, wfPeaks.imp_amp, wfPeaks.amp);
        if (res) vPeaks.push_back(wfPeaks);
      }
      bSS = false;
      bSE = false;
      wfPeaks.maxpos      = 0;
      wfPeaks.imp_maxpos  = 0;
      wfPeaks.amp         = 0;
      wfPeaks.imp_amp     = 0;
      wfPeaks.first       = 0;
      wfPeaks.last        = 0;
    }
  }

  return true;
}

Bool_t AFIDecoder::GetWFPars(UInt_t uiSN, Char_t polarity, Float_t baseline,\
   Float_t *fNewWave, UInt_t uiMaxPos, Float_t iImpAmp, Float_t iAmp)
{
  wfPars.start = 0;                                   // signal start time
  wfPars.len   = 0;                                   // signal length 0.1 - 0.1
  wfPars.amp   = iImpAmp;                                   // signal amplitude
  wfPars.amp_s     = uiMaxPos;                                   // sample number of signal amplitude
  wfPars.baseline  = baseline;                                // mean value of baseline
  wfPars.amp10     = wfPars.amp*0.1;                                   // 10% of the signal amplitude
  wfPars.amp90     = wfPars.amp*0.9;                                   // 10% of the signal amplitude
  wfPars.amp10_ls  = 0;                                   // left sample number of 10% of the signal amplitude
  wfPars.amp90_ls  = 0;                                   // left sample number of 90% of the signal amplitude
  wfPars.amp10_rs  = 0;                                   // right sample number of 10% of the signal amplitude
  wfPars.amp90_rs  = 0;                                   // right sample number of 90% of the signal amplitude
  wfPars.rise      = 0;                                   // signal rise time 0.1 - 0.9
  wfPars.fall      = 0;                                   // signal fall time 0.9 - 0.1
  wfPars.polarity  = polarity;

  Int_t  iPedRange = 0; // number of samples in the baseline range

  for (int s=wfPars.amp_s; s>0; s--)
  {
    if (abs(fNewWave[s]) > abs(wfPars.amp90)) wfPars.amp90_ls = s;
    if (abs(fNewWave[s]) > abs(wfPars.amp10))
     wfPars.amp10_ls = s;
    else break;
  }

  wfPars.amp   = iAmp;          // store the true value from the original waveform
  // printf("amp90_ls %d  amp10_ls %d\n", wfPars.amp90_ls, wfPars.amp10_ls);
  wfPars.rise = wfPars.amp90_ls - wfPars.amp10_ls;

  for (int s=wfPars.amp_s; s<uiSN; s++)
  {
    if (abs(fNewWave[s]) > abs(wfPars.amp90)) wfPars.amp90_rs = s;
    if (abs(fNewWave[s]) > abs(wfPars.amp10))
      wfPars.amp10_rs = s;
    else break;
  }

  wfPars.fall = wfPars.amp10_rs - wfPars.amp90_rs;
  wfPars.len  = wfPars.amp10_rs - wfPars.amp10_ls;
  wfPars.start= wfPars.amp10_ls;

  // printf("rise %f fall %f len %f\n", wfPars.rise, wfPars.fall, wfPars.len);
  if (wfPars.rise == 0 || wfPars.fall == 0 || wfPars.len == 0)
  {
    printf("Parameters have not been determined\n");
    return false;
  }

  if (true)
  {
    printf("------- Signal parameters: ------\n");
    printf("Amplitude:              %9.2f [%.0f sample]\n", wfPars.amp, float(wfPars.amp_s/TIME_DISCRETIZATION));
    printf("Amplitude [value@10%%]:  %9.2f [%d imp.sample : %.3f on the left side]\n", wfPars.amp10,wfPars.amp10_ls,fNewWave[wfPars.amp10_ls]);
    printf("Amplitude [value@90%%]:  %9.2f [%d imp.sample : %.3f on the left side]\n", wfPars.amp90,wfPars.amp90_ls,fNewWave[wfPars.amp90_ls]);
    printf("Polarity:               %9d [1 - negative, -1 - positive]\n",   wfPars.polarity);
    printf("Rise time [0.1-0.9]:    %9.0f [ns]\n", wfPars.rise);
    printf("Fall time [0.9-0.1]:    %9.0f [ns]\n", wfPars.fall);
    printf("Length    [0.1-0.1]:    %9.2f [ns]\n", wfPars.len);
    printf("Arrival time:           %9.2f [ns]\n", wfPars.start);
  }

  return true;
}

// Logger
void AFIDecoder::LogInit(Bool_t bIsMonintor)
{
  for (int t=0; t<THREAD_NUMBER; t++)
  {
    dataLog[t].used  = true;
    dataLog[t].sn    = 0;
    dataLog[t].cycle = 0;
    dataLog[t].event = 0;
    dataLog[t].mode  = 0;
    dataLog[t].utime = 0;
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      dataLog[t].data[ch] = 0;
      // dataLog[t].wfts[ch] = 0;
    }
    vDataLog[t].clear();
    if (bIsMonintor) break;
  }
}

void  AFIDecoder::LogFill(UInt_t wid, Float_t data[CHANNEL_NUMBER], UInt_t sn, \
                            UInt_t event, UChar_t mode, ULong_t time, UInt_t cycle)//, ULong64_t wfts[CHANNEL_NUMBER])
{
    if (dataLog[wid].used)
    {
      dataLog[wid].event = event;
      dataLog[wid].sn    = sn;
      dataLog[wid].mode  = mode;
      dataLog[wid].cycle = cycle;
      dataLog[wid].utime = time;
      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        dataLog[wid].data[ch] = data[ch];
        // dataLog[wid].wfts[ch] = wfts[ch];
      }
      vDataLog[wid].push_back(dataLog[wid]);
    }
    // printf("sn: %x mode: %c filldata: %.2f\n", sn,mode,dataLog[wid].data[0]);
}

void AFIDecoder::LogWrite(const char* filename)
{
  DATALOG  dataLogRes;
  TFile   *LogDataFile;
  TTree   *LogDataTree;

  LogDataFile = new TFile(Form("%s.root", filename), "UPDATE");
  LogDataTree = (TTree*)LogDataFile->Get(Form("%s", filename));
  if (!LogDataTree)
  {
    // printf("Achtung: The tree doesn't exist!\n");
    LogDataTree = new TTree(Form("%s", filename),Form("%s", filename));
    LogDataTree->Branch("event",&dataLogRes.event,"event/i");
    LogDataTree->Branch("time",&dataLogRes.utime,"time/l");
    LogDataTree->Branch("sn",&dataLogRes.sn,"sn/i");
    LogDataTree->Branch("cycle",&dataLogRes.cycle,"cycle/i");
    LogDataTree->Branch("mode",&dataLogRes.mode,"mode/b");
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      // if (dataLogRes.data)
        LogDataTree->Branch(Form("ch%02d",ch),&dataLogRes.data[ch],Form("ch%02d/F",ch));
      // if (dataLogRes.wfts)
      //   LogDataTree->Branch(Form("ch%02d",ch),&dataLogRes.wfts[ch],Form("ch%02d/F",ch));
    }
  }
  else
  {
    LogDataTree->SetBranchAddress("event", &dataLogRes.event);
    LogDataTree->SetBranchAddress("time", &dataLogRes.utime);
    LogDataTree->SetBranchAddress("sn", &dataLogRes.sn);
    LogDataTree->SetBranchAddress("cycle", &dataLogRes.cycle);
    LogDataTree->SetBranchAddress("mode", &dataLogRes.mode);
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      LogDataTree->SetBranchAddress(Form("ch%02d",ch),&dataLogRes.data[ch]);
  }


  for (int t=0; t<THREAD_NUMBER; t++)
  {
    if (dataLog[t].used)
    {
      for (auto itL=vDataLog[t].begin(); itL!=vDataLog[t].end(); itL++)
      {
        dataLogRes.event = itL->event;
        dataLogRes.sn    = itL->sn;
        dataLogRes.mode  = itL->mode;
        dataLogRes.cycle = itL->cycle;
        dataLogRes.utime = itL->utime;
        for (int ch=0; ch<CHANNEL_NUMBER; ch++)
        {
          dataLogRes.data[ch] = itL->data[ch];
          // dataLogRes.wfts[ch] = itL->wfts[ch];
        }
        LogDataTree->Fill();
      }
    }
  }

  LogDataFile->Write(0, TObject::kOverwrite);
  LogDataFile->Close();
  for (int t=0; t<THREAD_NUMBER; t++)
    vDataLog[t].clear();
  printf("Data saved to %s.root\n", filename);
}

// ASCII Logger
void AFIDecoder::LogDataInit()
{
  for (int t=0; t<THREAD_NUMBER; t++)
  {
    aDataLog[t].event = 0;
    aDataLog[t].sn    = 0;
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      aDataLog[t].amp[ch]    = 0;
      aDataLog[t].time[ch]   = 0;
      aDataLog[t].charge[ch] = 0;
      aDataLog[t].wf[ch].clear();
    }
    vaDataLog[t].clear();
  }
}

void AFIDecoder::LogDataFill(UInt_t wid, UInt_t event, UInt_t devsn, Float_t amp[CHANNEL_NUMBER],
  Float_t charge[CHANNEL_NUMBER], Float_t time[CHANNEL_NUMBER], std::vector<int32_t> *uiWave)
{
  aDataLog[wid].event = event;
  aDataLog[wid].sn    = devsn;
  for (int ch=0; ch<CHANNEL_NUMBER; ch++)
  {
    aDataLog[wid].amp[ch]    = amp[ch];
    aDataLog[wid].time[ch]   = time[ch];
    aDataLog[wid].charge[ch] = charge[ch];
    aDataLog[wid].wf[ch].clear();
    for (int s=0; s<uiWave[ch].size(); s++)
      aDataLog[wid].wf[ch].push_back(uiWave[ch].at(s));
  }
  vaDataLog[wid].push_back(aDataLog[wid]);
}

void AFIDecoder::LogDataWrite(const char* prefix)
{
  const char *fname = Form("%s_%s.txt", prefix, gSystem->BaseName(cDataFileName));
  FILE *fd = fopen(fname, "wt");
  if (!fd)
  {
    printf("Achtung: ASCII log data file couldn't be opened!\n");
    printf(" Check the file: %s %s::%s() line:%d\n", __FILE__, GetClassName(), __func__, __LINE__);
    printf("Achtung: ASCII log data file has not been created.\n");
    return;
  }

  ASCIIDATALOG dataLogRes;
  std::vector<ASCIIDATALOG> vDataLogRes;

  for (int t=0; t<THREAD_NUMBER; t++)
  {
    for (auto itL=vaDataLog[t].begin(); itL!=vaDataLog[t].end(); itL++)
    {
      dataLogRes.event = itL->event;
      dataLogRes.sn    = itL->sn;
      for (int ch=0; ch<CHANNEL_NUMBER; ch++)
      {
        dataLogRes.amp[ch] = itL->amp[ch];
        dataLogRes.time[ch] = itL->time[ch];
        dataLogRes.charge[ch] = itL->charge[ch];
        dataLogRes.wf[ch].clear();
        for (int s=0; s<itL->wf[ch].size(); s++)
          dataLogRes.wf[ch].push_back(itL->wf[ch].at(s));

      }
      vDataLogRes.push_back(dataLogRes);
    }
  }
  std::sort( vDataLogRes.begin(), vDataLogRes.end(),
        [&](ASCIIDATALOG &a, ASCIIDATALOG &b){ return a.event<b.event; });
  fprintf(fd, "# Event  SN  CH  Amp Charge  Time  S0 ... Sn\n");
  for (auto itL=vDataLogRes.begin(); itL!=vDataLogRes.end(); itL++)
  {
    auto devsn = itL->sn;
    auto devPtr = std::find_if(vADCs.begin(), vADCs.end(),
                    [devsn](const ADCS &adc){
                      return adc.devsn==devsn;
                    });
    for (int ch=0; ch<CHANNEL_NUMBER; ch++)
    {
      if (devPtr->ach.test(ch))
      {
        fprintf(fd, "%d %x ", itL->event, itL->sn);
        fprintf(fd, "%02d %f %f %f ", ch, itL->amp[ch], itL->charge[ch], itL->time[ch]);
        for (int s=0; s<itL->wf[ch].size(); s++)
          fprintf(fd, "%d ", itL->wf[ch].at(s));
        fprintf(fd, "\n");
      }
    }
  }
  for (int t=0; t<THREAD_NUMBER; t++)
    vaDataLog[t].clear();
  fclose(fd);
  printf("ASCII data log file %s has been created.\n", fname);
}

// Mapping settings
bool AFIDecoder::IsArcLight(UInt_t devsn,UInt_t ch)
{
  auto devPtr = std::find_if(vADCs.begin(), vADCs.end(),
                  [devsn](const ADCS &adc){
                    return adc.devsn==devsn;
                  });

  auto cfgPtr = std::find_if(devPtr->cfg.begin(), devPtr->cfg.end(),
                  [ch](const ADCSETTINGS &cfg){
                    return cfg.ch==ch;
                  });

  if (cfgPtr->type == 'a') return true;

  return false;
}

bool AFIDecoder::IsArcLight(ADCS *devPtr,UInt_t ch)
{
  auto cfgPtr = std::find_if(devPtr->cfg.begin(), devPtr->cfg.end(),
                  [ch](const ADCSETTINGS &cfg){
                    return cfg.ch==ch;
                  });

  if (cfgPtr->type == 'a') return true;

  return false;
}

bool AFIDecoder::IsLCM(ADCS *devPtr,UInt_t ch)
{
  auto cfgPtr = std::find_if(devPtr->cfg.begin(), devPtr->cfg.end(),
                  [ch](const ADCSETTINGS &cfg){
                    return cfg.ch==ch;
                  });

  if (cfgPtr->type == 'l') return true;

  return false;
}

bool AFIDecoder::IsFrontSide(ADCS *devPtr,UInt_t ch)
{
  auto cfgPtr = std::find_if(devPtr->cfg.begin(), devPtr->cfg.end(),
                  [ch](const ADCSETTINGS &cfg){
                    return cfg.ch==ch;
                  });

  if (cfgPtr->side == 'f') return true;

  return false;
}

bool AFIDecoder::IsRearSide(ADCS *devPtr,UInt_t ch)
{
  auto cfgPtr = std::find_if(devPtr->cfg.begin(), devPtr->cfg.end(),
                  [ch](const ADCSETTINGS &cfg){
                    return cfg.ch==ch;
                  });

  if (cfgPtr->side == 'r') return true;

  return false;
}

bool AFIDecoder::IsLeftPlane(ADCS *devPtr,UInt_t ch)
{
  auto cfgPtr = std::find_if(devPtr->cfg.begin(), devPtr->cfg.end(),
                  [ch](const ADCSETTINGS &cfg){
                    return cfg.ch==ch;
                  });

  if (cfgPtr->plane == 'l') return true;

  return false;
}

bool AFIDecoder::IsRightPlane(ADCS *devPtr,UInt_t ch)
{
  auto cfgPtr = std::find_if(devPtr->cfg.begin(), devPtr->cfg.end(),
                  [ch](const ADCSETTINGS &cfg){
                    return cfg.ch==ch;
                  });

  if (cfgPtr->plane == 'r') return true;

  return false;
}

bool AFIDecoder::IsSum(ADCS *devPtr,UInt_t ch)
{
  auto cfgPtr = std::find_if(devPtr->cfg.begin(), devPtr->cfg.end(),
                  [ch](const ADCSETTINGS &cfg){
                    return cfg.ch==ch;
                  });

  if (cfgPtr->sum == 's') return true;

  return false;
}

bool AFIDecoder::IsSingle(ADCS *devPtr,UInt_t ch)
{
  auto cfgPtr = std::find_if(devPtr->cfg.begin(), devPtr->cfg.end(),
                  [ch](const ADCSETTINGS &cfg){
                    return cfg.ch==ch;
                  });

  if (cfgPtr->sum == 'c') return true;

  return false;
}

UChar_t AFIDecoder::GetEShapeNumber(ADCS *devPtr,UInt_t ch)
{
  auto cfgPtr = std::find_if(devPtr->cfg.begin(), devPtr->cfg.end(),
                  [ch](const ADCSETTINGS &cfg){
                    return cfg.ch==ch;
                  });

  return cfgPtr->eshape;
}
