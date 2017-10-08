
/*
    Handles a water tank under a waterjet cutter
*/

#include "LcdBase.h"
#include "CuttingWaterTank.h"
#include "Kernel.h"
#include "Config.h"
#include "checksumm.h"
#include "ConfigValue.h"
#include "SlowTicker.h"
#include "PublicData.h"
#include "StreamOutputPool.h"
#include "StreamOutput.h"
#include "SerialMessage.h"
#include "CuttingWaterTank.h"
#include "utils.h"
#include "Gcode.h"

#define cutting_water_tank_checksum          CHECKSUM("cutting_water_tank")
#define enable_checksum                      CHECKSUM("enable")

#define middle_float_pin_checksum            CHECKSUM("middle_float_pin")
#define high_float_pin_checksum              CHECKSUM("high_float_pin")
#define dump_valve_pin_checksum              CHECKSUM("dump_valve_pin")
#define low_pressure_pump_pin_checksum       CHECKSUM("low_pressure_pump_pin")

#define fill_cycle_seconds_checksum          CHECKSUM("fill_cycle_seconds")
#define filter_cleaning_seconds_checksum     CHECKSUM("filter_cleaning_seconds")
#define water_level_too_low_seconds_checksum CHECKSUM("water_level_too_low_seconds")

#define low_water_detected_menu_checksum     CHECKSUM("low_water_detected_menu")
#define high_water_detected_menu_checksum    CHECKSUM("high_water_detected_menu")
#define filters_blocked_menu_checksum        CHECKSUM("filters_blocked_menu")

#define seconds_per_check_checksum           CHECKSUM("seconds_per_check")

CuttingWaterTank::CuttingWaterTank()
{
    suspended= false;
    middle_float_detected= false;
    high_float_detected=false;
    active= true;
    fill_timer=0;
    dump_timer=0;
    seconds_elapsed=0;
 //   e_last_moved= NAN;
}

CuttingWaterTank::~CuttingWaterTank()
{

}

void CuttingWaterTank::on_module_loaded()
{
    // if the module is disabled -> do nothing
    if(!THEKERNEL->config->value( cutting_water_tank_checksum, enable_checksum )->by_default(false)->as_bool()) {
        // as this module is not needed free up the resource
        delete this;
        return;
    }

    //middle_float detector
    middle_float_pin.from_string( THEKERNEL->config->value(cutting_water_tank_checksum, middle_float_pin_checksum)->by_default("nc" )->as_string())->as_input();
    //high_float detector
    high_float_pin.from_string( THEKERNEL->config->value(cutting_water_tank_checksum, high_float_pin_checksum)->by_default("nc" )->as_string())->as_input();
    //dump valve
    dump_valve_pin.from_string( THEKERNEL->config->value(cutting_water_tank_checksum, dump_valve_pin_checksum)->by_default("nc" )->as_string())->as_input();
    //low pressure pump
    low_pressure_pump_pin.from_string( THEKERNEL->config->value(cutting_water_tank_checksum, low_pressure_pump_pin_checksum)->by_default("nc" )->as_string())->as_input();

    //now check for the menu definitions
    low_water_detected_menu =  THEKERNEL->config->value(cutting_water_tank_checksum, low_water_detected_menu_checksum)->by_default(0)->as_string();
    high_water_detected_menu = THEKERNEL->config->value(cutting_water_tank_checksum, high_water_detected_menu_checksum)->by_default(0)->as_string();
    filters_blocked_menu =     THEKERNEL->config->value(cutting_water_tank_checksum, filters_blocked_menu_checksum)->by_default(0)->as_string();

    //we must have all the input and output pins and menus defined otherwise generate an error message and delete ourselves
    if (!middle_float_pin.connected() || !high_float_pin.connected() || !dump_valve_pin.connected() || !low_pressure_pump_pin.connected() ||
        low_water_detected_menu.empty() || high_water_detected_menu.empty() || filters_blocked_menu.empty()) {
      if (!middle_float_pin.connected())
        THEKERNEL->streams->printf("*ERROR* config missing definition for the cutting_water_tank.middle_float_pin\n");
      if (!high_float_pin.connected())
          THEKERNEL->streams->printf("*ERROR* config missing definition for the cutting_water_tank.high_float_pin\n");
      if (!dump_valve_pin.connected())
        THEKERNEL->streams->printf("*ERROR* config missing definition for the cutting_water_tank.dump_valve_pin\n");
      if (!low_pressure_pump_pin.connected())
          THEKERNEL->streams->printf("*ERROR* config missing definition for the cutting_water_tank.low_pressure_pump_pin\n");
      if (low_water_detected_menu.empty())
          THEKERNEL->streams->printf("*ERROR* config missing definition for the cutting_water_tank.low_water_detected_menu\n");
      if (high_water_detected_menu.empty())
          THEKERNEL->streams->printf("*ERROR* config missing definition for the cutting_water_tank.high_water_detected_menu\n");
      if (filters_blocked_menu.empty())
          THEKERNEL->streams->printf("*ERROR* config missing definition for the cutting_water_tank.filters_blocked_menu\n");
      delete this;
      return;
    }

    //defaults to 1 hour
    fill_cycle_seconds          = THEKERNEL->config->value(cutting_water_tank_checksum, fill_cycle_seconds_checksum         )->by_default(3600.0f)->as_number();
    //defaults to 2 hours
    filter_cleaning_seconds     = THEKERNEL->config->value(cutting_water_tank_checksum, filter_cleaning_seconds_checksum    )->by_default(7200.0f)->as_number();
    //defaults to 1 week
    water_level_too_low_seconds = THEKERNEL->config->value(cutting_water_tank_checksum, water_level_too_low_seconds_checksum)->by_default(655200.0f)->as_number();


    if(middle_float_pin.connected() && high_float_pin.connected()) {
        // input pin polling
        THEKERNEL->slow_ticker->attach( 100, this, &CuttingWaterTank::button_tick);
    }
    register_for_event(ON_SECOND_TICK);
    register_for_event(ON_MAIN_LOOP);
    register_for_event(ON_CONSOLE_LINE_RECEIVED);
    this->register_for_event(ON_GCODE_RECEIVED);
}


void CuttingWaterTank::send_command(std::string msg, StreamOutput *stream)
{
    struct SerialMessage message;
    message.message = msg;
    message.stream = stream;
    THEKERNEL->call_event(ON_CONSOLE_LINE_RECEIVED, &message );
}

// needed to detect when we resume
void CuttingWaterTank::on_console_line_received( void *argument )
{
    if(!suspended) return;

    SerialMessage new_message = *static_cast<SerialMessage *>(argument);
    string possible_command = new_message.message;
    string cmd = shift_parameter(possible_command);
    if(cmd == "resume" || cmd == "M601") {
        suspended= false;
    }
}

void CuttingWaterTank::on_gcode_received(void *argument)
{
    Gcode *gcode = static_cast<Gcode *>(argument);
    if (gcode->has_m) {
        if (gcode->m == 1405) { // disable Cutting Water Tank
            active= false;
        }else if (gcode->m == 1406) { // enable Cutting Water Tank
            active= true;

        }
    }
}

void CuttingWaterTank::low_water() {
  if (seconds_elapsed > delay_seconds) { // to prevent the menu being contantly displayed
      seconds_elapsed = 0; //reset counter
      //Note: when the user selects "OK" on the menu it must also issue an M601 or "resume"
      THEKERNEL->current_path = this->low_water_detected_menu.c_str();
      THEPANEL->setup_menu(THEPANEL->max_screen_lines());
      THEPANEL->menu_update(); //force the panel to update the menu
      THEPANEL->lcd->setLed(LED_ORANGE, true);
      THEPANEL->lcd->setLed(LED_GREEN, false);
  }
}
void CuttingWaterTank::high_water() {
  if (seconds_elapsed > delay_seconds) { // to prevent the menu being contantly displayed
      seconds_elapsed = 0; //reset counter
      //Note: when the user selects "OK" on the menu it must also issue an M601 or "resume"
      THEKERNEL->current_path = this->high_water_detected_menu.c_str();
      THEPANEL->setup_menu(THEPANEL->max_screen_lines());
      THEPANEL->menu_update(); //force the panel to update the menu
      THEPANEL->lcd->setLed(LED_ORANGE, true);
      THEPANEL->lcd->setLed(LED_GREEN, false);
  }
}

void CuttingWaterTank::clear_filter() {
  if (seconds_elapsed > delay_seconds) { // to prevent the menu being contantly displayed
      seconds_elapsed = 0; //reset counter
      //Note: when the user selects "OK" on the menu it must also issue an M601 or "resume"
      THEKERNEL->current_path = this->filters_blocked_menu.c_str();
      THEPANEL->setup_menu(THEPANEL->max_screen_lines());
      THEPANEL->menu_update(); //force the panel to update the menu
      THEPANEL->lcd->setLed(LED_ORANGE, true);
      THEPANEL->lcd->setLed(LED_GREEN, false);
  }
}

void CuttingWaterTank::on_main_loop(void *argument)
{
    if (active ) {

        //first lets determine if we have been idle for a very long time and there
        //is not enough water in the tank so the user needs to fill it before we start
        if(fill_timer > water_level_too_low_seconds) {
            dump_valve_pin.set(false); //turn off the dump valve so no more water leaves
            low_pressure_pump_pin.set(false); //stop the low_pressure_pump_pin in case there is far too little water
            // fire suspend command so Smoothie will not process any G or M-Codes until this is fixed
            if(!suspended) {
                this->suspended= true;
                // fire suspend command
                this->send_command( "M600", &(StreamOutput::NullStream) );
            }
            //is the low_water menu being displayed?
            if (this->filters_blocked_menu.compare(THEKERNEL->current_path) !=0) {
                //tell the user
                low_water();
            }
        } else if(!middle_float_detected) {
            //water is below the middle float level, need to fill up
            dump_valve_pin.set(false); //turn off the dump valve so no more water leaves
            dump_timer = 0; //reset the dump_timer as we are filling
            if (fill_timer > fill_cycle_seconds) {
                //the fill timer has been exceeded and the middle_float is still not set so there
                //is not enough water in the tank so the user needs to fill it before we start
                // fire suspend command so Smoothie will not process any G or M-Codes until this is fixed
                if(!suspended) {
                    this->suspended= true;
                    // fire suspend command
                    this->send_command( "M600", &(StreamOutput::NullStream) );
                }
                //is the low_water menu being displayed?
                if (this->filters_blocked_menu.compare(THEKERNEL->current_path) !=0) {
                    //tell the user
                    //Note: when the user selects "OK" on the menu it must also issue an M601 or "resume"
                    low_water();
                }
           }
        } else if(middle_float_detected) {
            //water level level is above middle float level
            dump_valve_pin.set(true); //turn on the dump valve to let water out
            fill_timer = 0; //reset fill_timer as we are dumping
            if (dump_timer > filter_cleaning_seconds){
                  //the dump timer has been exceeded and the middle_float is still set
                  //this indicates the filters are blocked so tell the user
                  if(!suspended) {
                      this->suspended= true;
                      // fire suspend command
                      this->send_command( "M600", &(StreamOutput::NullStream) );
                  }
                  //is the clear_filter menu being displayed?
                  if (this->filters_blocked_menu.compare(THEKERNEL->current_path) !=0) {
                      //tell the user
                      //Note: when the user selects "OK" on the menu it must also issue an M601 or "resume"
                      clear_filter();
                  }
             }
        } else if (high_float_detected) {
            //danger the water has exceeded the maximum float, time to take drastic action
            dump_valve_pin.set(true); //turn on the dump valve to let water out
            if(!suspended) {
                this->suspended= true;
                // fire suspend command
                this->send_command( "M600", &(StreamOutput::NullStream) );
                //TODO add logic to turn off the main high pressure pump and whatever else we need to do
            }
            //is the high_water menu being displayed?
            if (this->filters_blocked_menu.compare(THEKERNEL->current_path) !=0) {
                //tell the user
                //Note: when the user selects "OK" on the menu it must also issue an M601 or "resume"
                high_water();
            }
        } else {
            // All good so reset the LEDs
            THEPANEL->lcd->setLed(LED_ORANGE, false);
            THEPANEL->lcd->setLed(LED_GREEN, true);
        }
    }
}

void CuttingWaterTank::on_second_tick(void *argument)
{
  fill_timer++;
  dump_timer++;
  seconds_elapsed++;
}


uint32_t CuttingWaterTank::button_tick(uint32_t dummy)
{
    if (suspended || !active) return 0;

    if (middle_float_pin.connected()) {
        if(middle_float_pin.get()) {
            // we got a trigger from the middle_float detector
            this->middle_float_detected= true;
        }
    }

    if (high_float_pin.connected()) {
        if(high_float_pin.get()) {
            // we got a trigger from the high_float detector
            this->high_float_detected= true;
        }
    }
    return 0;
}
