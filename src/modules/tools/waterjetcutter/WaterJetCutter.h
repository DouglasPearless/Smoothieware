#pragma once

#include "Panel.h"
#include "PanelScreen.h"
#include "MainMenuScreen.h"
#include "Module.h"
#include "Pin.h"
#include "Gcode.h"

#include <stdint.h>
#include <atomic>
#include <string>

namespace mbed {
    class InterruptIn;
}

class StreamOutput;

class WaterJetCutter: public Module
{
public:
    WaterJetCutter();
    ~WaterJetCutter();
    void on_module_loaded();
    void on_main_loop(void* argument);
    void on_second_tick(void* argument);
    void on_get_public_data(void* argument);
    void on_set_public_data(void* argument);
    void on_console_line_received( void *argument );
    void on_gcode_received(void *argument);
    enum STATE {NONE,           //default power on state module not enabled, or after a M1402 disable module command

                MENU_MODE,      //after module is enabled with a M1401, or you can return to this after a M1404 job finished
                PREP_MODE,       //user stuff to get the system set up
                DRY_RUN_MODE,    //user may or may not do this to run a job with no water or abrasive
                READY_TO_RUN_MODE, //make sure door closed, user presses "Start/Stop" or OK on the appropriate menu to move onto be able to run the G-Code, go to START
                RUN_MODE,       //high pressure pump and then abrasive aka M1403, typically then go to RUN_GCODE_MODE_AND_WATCH_WATER_LEVEL
                RUN_MODE_AND_WATCH_WATER_LEVEL, //g-code is being executed, the water tank under the cutter is being monitored
                PAUSE,          //pause, this stops motion, high pressue water, abrasive ?what happens to the fill and dump timers, low pressure pump etc??
                RESUME,         //back to RUN_GCODE_MODE_AND_WATCH_WATER_LEVEL (typically)
                END_JOB,        //this is like PAUSE but it can only return to the MENU_MODE as it is intended to complete the job, triggered by a M1404 from the host

                //The next four only occur during RUN_GCODE_MODE_AND_WATCH_WATER_LEVEL
                IDLE_TOO_LONG,  //unit has been sitting idle for a long time and the water tank may need topping up
                WAITING_FOR_WATER_LEVEL_TO_RAISE, //filling up the water tank, then to RUN_GCODE_MODE_AND_WATCH_WATER_LEVEL when fixed
                DUMP_WATER, //water has reached the middle float and we need to release some to prevent overflow
                FILTERS_BLOCKED, //not draining quick enough; check the filters return to previous MODE which should be RUN_GCODE_MODE_AND_WATCH_WATER_LEVEL when fixed
                WATER_LEVEL_TOO_HIGH,  //we must PAUSE to turn off the water, something is wrong and we risk a water overflow; this could be triggered when in the FILTERS_BLOCKED state

                GANTRY_STALLED, //not yet implemented, just a place-holder
                END_STOPS_TRIGGERED, //display the error, job is effectively lost, go back to MENU_MODE
                MENU_PASSED_BAD_STATE, //this allows us to record if the menu system passed a bad state from a menu
                NUMBER_OF_STATES //This is a special state which only exists to determine the number of STATEs in total
    };

private:
    void printf_state(Gcode *gcode, STATE state);
    bool validate_config();
    void on_pin_rise();
    void check_encoder();
    void send_command(std::string msg, StreamOutput *stream);
    uint32_t button_tick(uint32_t dummy);
    void low_water();
    void high_water();
    void clear_filter();
    void play();
    void resume();
    void pause();
    void _run_mode();


    STATE previous_state {NONE};
    STATE current_state {NONE};

    Pin door_switch_pin;
    Pin middle_float_pin;
    Pin high_float_pin;
    Pin dump_valve_pin;
    Pin low_pressure_pump_pin;
    Pin high_pressure_pump_pin;

    std::string low_water_detected_menu; //waterjetcutter.low_water_detected_menu
    std::string high_water_detected_menu; //waterjetcutter.high_water_detected_menu
    std::string filters_blocked_menu;     //waterjetcutter.filters_blocked_menu
    std::string ready_to_run_mode_opened_door_menu;
    std::string ready_to_run_mode_closed_door_menu;
    std::string ready_to_run_mode_exit_cut_menu;
    std::string error_wl_low_menu;
    std::string error_clean_filters_menu;
    std::string error_wl_high_menu;
    std::string cut_completed_menu;
    std::string run_mode_opened_door_menu;
    std::string run_mode_triggered_end_stops_menu;

    #define delay_seconds 20
    unsigned long int seconds_elapsed{0};

    unsigned long int fill_cycle_seconds{0};
    unsigned long int filter_cleaning_seconds{0};
    unsigned long int water_level_too_low_seconds{0};

    unsigned long int fill_timer{0};
    unsigned long int dump_timer{0};
    unsigned long int abrasive_hopper_timer{0};
    unsigned long int spent_abrasive_timer{0};

    void change_state(STATE new_state);

    struct {
        bool debug:1;
        bool config_error:1;
        bool door_switch:1;
        bool middle_float_detected:1;
        bool high_float_detected:1;
        bool paused:1;
        bool active:1;
        bool error:1;
        bool fill_timer_start:1;
        bool fill_timer_run:1;
        bool fill_timer_stop:1;
        bool dump_timer_start:1;
        bool dump_timer_run:1;
        bool dump_timer_stop:1;
        bool abrasive_hopper_timer_start:1;
        bool abrasive_hopper_timer_run:1;
        bool abrasive_hopper_timer_stop:1;
        bool spent_abrasive_timer_start:1;
        bool spent_abrasive_timer_run:1;
        bool spent_abrasive_timer_stop:1;
    };

};
