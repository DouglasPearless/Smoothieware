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
//actual actions as second parameter to an "action" are:
#define goto_menu_checksum                      CHECKSUM("goto-menu")
#define goto_watch_screen_checksum              CHECKSUM("goto-watch-screen")
#define run_command_checksum                    CHECKSUM("run-command")
#define control_axis_checksum                   CHECKSUM("control-axis")
#define control_extruder_checksum               CHECKSUM("control-extruder")
#define file_select_checksum                    CHECKSUM("file-select")
#define display_watch_screen                    CHECKSUM("display-watch-screen")

//National Language Support
#define label_en_checksum                      CHECKSUM("label-en") //default
#define label_fr_checksum                      CHECKSUM("label-fr")
#define label_es_checksum                      CHECKSUM("label-es")

//Should language be controlled from the Smoothie config file, and picked up by Panel? for example:
//Panel.language                                en  #default is en, optiosn are:
//                                              #en English (default)
//                                              #fr French
//                                              #es Spanish
