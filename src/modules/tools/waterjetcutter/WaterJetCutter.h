#pragma once

#include "Panel.h"
#include "PanelScreen.h"
#include "MainMenuScreen.h"
#include "Module.h"
#include "Pin.h"

#include <stdint.h>
#include <atomic>
#include <string>

namespace mbed {
    class InterruptIn;
}

class StreamOutput;

class CuttingWaterTank: public Module
{
public:
    CuttingWaterTank();
    ~CuttingWaterTank();
    void on_module_loaded();
    void on_main_loop(void* argument);
    void on_second_tick(void* argument);
    void on_console_line_received( void *argument );
    void on_gcode_received(void *argument);

private:
    void on_pin_rise();
    void check_encoder();
    void send_command(std::string msg, StreamOutput *stream);
    uint32_t button_tick(uint32_t dummy);
    void low_water();
    void high_water();
    void clear_filter();
    void start();
    void resume();
    void pause();

    Pin middle_float_pin;
    Pin high_float_pin;
    Pin dump_valve_pin;
    Pin low_pressure_pump_pin;
    Pin high_pressure_pump_pin;

    std::string low_water_detected_menu; //cutting_water_tank.low_water_detected_menu
    std::string high_water_detected_menu; //cutting_water_tank.high_water_detected_menu
    std::string filters_blocked_menu;     //cutting_water_tank.filters_blocked_menu

    #define delay_seconds 20
    unsigned long int seconds_elapsed{0};

    unsigned long int fill_cycle_seconds{0};
    unsigned long int filter_cleaning_seconds{0};
    unsigned long int water_level_too_low_seconds{0};

    unsigned long int fill_timer{0};
    unsigned long int dump_timer{0};

    struct {
        bool middle_float_detected:1;
        bool high_float_detected:1;
        bool suspended:1;
        bool active:1;
    };
};
