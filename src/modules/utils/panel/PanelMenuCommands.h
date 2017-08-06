#include "checksumm.h"

//Only commands
#define only_if_playing_is_checksum             CHECKSUM("only-if-playing-is")
#define only_if_halted_is_checksum              CHECKSUM("only-if-halted-is")
#define only_if_suspended_is_checksum           CHECKSUM("only-if-suspended-is")
#define only_if_file_is_gcode_checksum          CHECKSUM("only-if-file-is-gcode")
#define only_if_extruder_checksum               CHECKSUM("only-if-extruder")
#define only_if_temperature_control_checksum    CHECKSUM("only-if-temperature-control")
#define only_if_laser_checksum                  CHECKSUM("only-if-laser")
#define only_if_cnc_checksum                    CHECKSUM("only-if-cnc")

//Is commands
#define is_title_checksum                       CHECKSUM("is-title")

//other commands
#define not_selectable_checksum                 CHECKSUM("not-selectable")
#define file_selector_checksum                  CHECKSUM("file-selector")

//Actions
#define action_checksum                         CHECKSUM("action")
#define goto_menu_checksum                      CHECKSUM("goto-menu")
#define run_command                             CHECKSUM("Run-command")
#define control_axis                            CHECKSUM("control-axis")
#define control_extruder                        CHECKSUM("control-extruder")
#define file_select                             CHECKSUM("file-select")
#define display_watch_screen                    CHECKSUM("display-watch-screen")

//National Language Support
#define action_en_checksum                      CHECKSUM("action_en")
#define action_fr_checksum                      CHECKSUM("action_fr")

//Should language be controlled from the Smoothie config file, and picked up by Panel? for example:
//Panel.language                                en  #default is en, optiosn are:
//                                              #en English (default)
//                                              #fr French
//                                              #es Spanish
