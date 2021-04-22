#define ERROR_DATAFILE_OPEN     10    // Open datafile error
#define ERROR_CONFIGFILE_OPEN   11    // Open config file error
#define ERROR_EVENT_READ        12    // Read event error
#define ERROR_BAD_EVENT         13    // Event incomplete or corrupted
#define ERROR_DATADIR_OPEN      14    // Data directory is wrong

#define ERROR_GATE_EMPTY        20    // Gate vector is empty
#define ERROR_GATE_MISSING_SN   21    // No device SN found in the gate file
#define ERROR_GATE_FILE_OPEN    22    // No file found or wrong file format

#define ERROR_KERNEL_LOAD       30    // Kernels have not been loaded
#define ERROR_KERNEL_FILE       31    // Kernel file open error
#define ERROR_KERNEL_TREE       32    // Kernel tree access error
#define ERROR_KERNEL_WRONG_TRIG_CH 33    // Kernel tree access error
#define ERROR_KERNEL_EMPTY      34

#define ERROR_DATADIR_NOT_FOUND   40    // Directory is not found
#define ERROR_DATAFILE_NOT_FOUND  41    // Directory doesn't contain data files
