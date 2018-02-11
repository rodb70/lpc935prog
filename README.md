# lpc935prog
Programmer for old microcontroller NXP LPC935

An old tool that allowed programming of the NXP LPC935 8051 CPU from years gone by.

The tools command line arguments are as follows:
    Usage: lpc935-prog [OPTIONS]* <filename>
      -g, --prog                                                                 Program an intel hex file to micro
      -w, --write=ucfg1|bootv|statb|pofftime|p2icp                               Write a control register
      -r, --read=ids|version|statb|bootv|ucfg1|secx|gcrc|scrc|pofftime|p2icp     Read a control register
      -e, --erase=sector|page                                                    Erase a sector or page from the flash
      -s, --reset                                                                Reset the micro-controller
      -a, --address=SECTOR                                                       Sector address for Op
      -d, --data=DATA                                                            Data byte to write to the micro
      -b, --baud=BAUD                                                            baud rate to communicate with
      -p, --port=PORT                                                            Communications port to use
      -o, --programmer=serial|bridge                                             Use programmer
      -v, --verbose                                                              Print out debug infomation

    Help options:
      -?, --help                                                                 Show this help message
          --usage                                                                Display brief usage message
