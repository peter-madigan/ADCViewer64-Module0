  ROOTFLAGS = $(shell root-config --cflags)
  ROOTLIBS = $(shell root-config --glibs)

HEADERS  = AFISettings.h AFIDecoder.h AFIProcessor.h AFIGUI.h Dialogs.h RLogger.h
SOURCES = main.cpp AFIDecoder.cpp AFIProcessor.cpp AFIGUI.cpp AFIGUIDict.cpp Dialogs.cpp RLogger.cpp

all: adc

AFIGUIDict.cpp: $(HEADERS) Linkdef.h
	rootcint -f $@ $^

adc: $(SOURCES)
	g++ $^ -o ADC64Viewer $(ROOTFLAGS) $(ROOTLIBS) -Ddevice_type=0
	# rm -rf *Dict.* *.dSYM

qdc: $(SOURCES)
	g++ $^ -o TQDCViewer $(ROOTFLAGS) $(ROOTLIBS) -Ddevice_type=1
	#rm -rf *Dict.* *.dSYM

clean:
	rm -rvf ADC64Viewer TQDCViewer *.pcm *Dict.* *.dSYM
