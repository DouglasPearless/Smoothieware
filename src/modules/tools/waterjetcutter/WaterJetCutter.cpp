
/*
    Handles a Water Jet Cutter
*/

#include "LcdBase.h"
#include "Kernel.h"
#include "Config.h"
#include "checksumm.h"
#include "ConfigValue.h"
#include "SlowTicker.h"
#include "PublicDataRequest.h"
#include "StreamOutputPool.h"
#include "StreamOutput.h"
#include "SerialMessage.h"
#include "WaterJetCutter.h"
#include "utils.h"

#define waterjetcutter_checksum              CHECKSUM("waterjetcutter")
#define enable_checksum                      CHECKSUM("enable")

#define door_switch_pin_checksum             CHECKSUM("door_switch_pin")
#define middle_float_pin_checksum            CHECKSUM("middle_float_pin")
#define high_float_pin_checksum              CHECKSUM("high_float_pin")
#define dump_valve_pin_checksum              CHECKSUM("dump_valve_pin")
#define low_pressure_pump_pin_checksum       CHECKSUM("low_pressure_pump_pin")
#define high_pressure_pump_pin_checksum      CHECKSUM("high_pressure_pump_pin")

#define fill_cycle_seconds_checksum          CHECKSUM("fill_cycle_seconds")
#define filter_cleaning_seconds_checksum     CHECKSUM("filter_cleaning_seconds")
#define water_level_too_low_seconds_checksum CHECKSUM("water_level_too_low_seconds")

#define ready_to_run_mode_opened_door_menu_checksum  CHECKSUM("ready_to_run_mode_opened_door")
#define ready_to_run_mode_closed_door_menu_checksum  CHECKSUM("ready_to_run_mode_closed_door")
#define ready_to_run_mode_exit_cut_menu_checksum     CHECKSUM("ready_to_run_mode_exit_cut")
#define error_wl_low_menu_checksum                   CHECKSUM("error_wl_low_menu")
#define error_clean_filters_menu_checksum            CHECKSUM("error_clean_filters_menu")
#define error_wl_high_menu_checksum                  CHECKSUM("error_wl_high_menu")
#define cut_completed_menu_checksum                  CHECKSUM("cut_completed_menu")
#define run_mode_opened_door_menu_checksum           CHECKSUM("run_mode_opened_door_menu")
#define run_mode_triggered_end_stops_menu_checksum   CHECKSUM("run_mode_triggered_end_stops_menu")

#define seconds_per_check_checksum           CHECKSUM("seconds_per_check")

WaterJetCutter::WaterJetCutter()
{
    debug = false;
    config_error=false; //as a sfety feature make sure all the inputs and outpus are defined
    active=true;     //is this module active?
    paused=false; //is Smoothie in a suspended state
    door_switch=false;
    middle_float_detected=false;
    high_float_detected  =false;

    //timers
    fill_timer=0;            fill_timer_run=false;
    dump_timer=0;            dump_timer_run=false;
    abrasive_hopper_timer=0; abrasive_hopper_timer_run=false;
    spent_abrasive_timer=0;  spent_abrasive_timer_run=false;
    seconds_elapsed=0;
}

WaterJetCutter::~WaterJetCutter()
{

}

void WaterJetCutter::on_get_public_data(void *argument)
{
    PublicDataRequest *pdr = static_cast<PublicDataRequest *>(argument);

    if(!pdr->starts_with(waterjetcutter_checksum)) return;
/*
    if(!pdr->second_element_is(this->name_checksum)) return; // likely fan, but could be anything

    // ok this is targeted at us, so send back the requested data
    // caller has provided the location to write the state to
    struct pad_switch *pad= static_cast<struct pad_switch *>(pdr->get_data_ptr());
    pad->name = this->name_checksum;
    pad->state = this->switch_state;
    pad->value = this->switch_value;
    pdr->set_taken();
    */
}

void WaterJetCutter::on_set_public_data(void *argument)
{
    PublicDataRequest *pdr = static_cast<PublicDataRequest *>(argument);

    if(!pdr->starts_with(waterjetcutter_checksum)) return;

    uint16_t t = *static_cast<uint16_t *>(pdr->get_data_ptr());
    uint16_t rm = get_checksum("run-mode");  //TODO REMOVE THIS HARDWIRED AND CHANGE TO A LOOKUP
    uint16_t mm = get_checksum("menu-mode");  //TODO REMOVE THIS HARDWIRED AND CHANGE TO A LOOKUP
    uint16_t rtr = get_checksum("ready-to-run-mode");  //TODO REMOVE THIS HARDWIRED AND CHANGE TO A LOOKUP
    uint16_t p_r_t = get_checksum("pause-resume-toggle"); //TODO REMOVE THIS HARDWIRED AND CHANGE TO A LOOKUP
    if (t==rm)
       this->change_state(RUN_MODE);
    else if (t==mm)
      this->change_state(MENU_MODE);
    else if (t==rtr)
      this->change_state(READY_TO_RUN_MODE);
    else if (t==p_r_t) {
        if (current_state == RUN_MODE)
           this->change_state(PAUSE);
    }
    else
      THEKERNEL->streams->printf("*ERROR* unknown state change received from the menu system, checksum is %i\n",t);
    /*
    string *s = static_cast<std::string *>(pdr->get_data_ptr());

    std::string message; //debug
    if (s->size() > 40) {
        message = s->substr(0, 40);
    } else {
        message= *s;
    }
*/
    //STATE t = *static_cast<STATE *>(pdr->get_data_ptr());
    //this->change_state(t);
    //this->switch_state = t;
    pdr->set_taken();
/*
    if(!pdr->second_element_is(this->name_checksum)) return; // likely fan, but could be anything

    // ok this is targeted at us, so set the value
    if(pdr->third_element_is(state_checksum)) {
        bool t = *static_cast<bool *>(pdr->get_data_ptr());
        this->switch_state = t;
        this->switch_changed= true;
        pdr->set_taken();

        // if there is no gcode to be sent then we can do this now (in on_idle)
        // Allows temperature switch to turn on a fan even if main loop is blocked with heat and wait
        if(this->output_on_command.empty() && this->output_off_command.empty()) on_main_loop(nullptr);

    } else if(pdr->third_element_is(value_checksum)) {
        float t = *static_cast<float *>(pdr->get_data_ptr());
        this->switch_value = t;
        this->switch_changed= true;
        pdr->set_taken();
    }
    */
}





void WaterJetCutter::printf_state(Gcode *gcode, STATE state)
{
  switch(state) {
            case NONE:                             gcode->stream->printf("NONE");                             break;
            case MENU_MODE:                        gcode->stream->printf("MENU_MODE");                        break;
            case PREP_MODE:                        gcode->stream->printf("PREP_MODE");                        break;
            case DRY_RUN_MODE:                     gcode->stream->printf("DRY_RUN_MODE");                     break;
            case READY_TO_RUN_MODE:                gcode->stream->printf("READY_TO_RUN_MODE");                break;
            case RUN_MODE:                         gcode->stream->printf("RUN_MODE");                         break;
            case RUN_MODE_AND_WATCH_WATER_LEVEL:   gcode->stream->printf("RUN_MODE_AND_WATCH_WATER_LEVEL");   break;
            case PAUSE:                            gcode->stream->printf("PAUSE");                            break;
            case RESUME:                           gcode->stream->printf("RESUME");                           break;
            case END_JOB:                          gcode->stream->printf("END_JOB");                          break;
            case IDLE_TOO_LONG:                    gcode->stream->printf("IDLE_TOO_LONG");                    break;
            case WAITING_FOR_WATER_LEVEL_TO_RAISE: gcode->stream->printf("WAITING_FOR_WATER_LEVEL_TO_RAISE"); break;
            case DUMP_WATER:                       gcode->stream->printf("DUMP_WATER");                       break;
            case FILTERS_BLOCKED:                  gcode->stream->printf("FILTERS_BLOCKED");                  break;
            case WATER_LEVEL_TOO_HIGH:             gcode->stream->printf("WATER_LEVEL_TOO_HIGH");             break;
            case GANTRY_STALLED:                   gcode->stream->printf("GANTRY_STALLED");                   break;
            case END_STOPS_TRIGGERED:              gcode->stream->printf("END_STOPS_TRIGGERED");              break;
            case MENU_PASSED_BAD_STATE: gcode->stream->printf("MENU SYSTEM HAS PASSED A BAD STATE");          break;
            case NUMBER_OF_STATES:        gcode->stream->printf("MENU SYSTEM HAS PASSED AN UNDEFINED STATE"); break;
    }
}
bool WaterJetCutter::validate_config()
{
  bool config_ok = true;
  if (!door_switch_pin.connected()        ||
      !middle_float_pin.connected()       ||
      !high_float_pin.connected()         ||
      !dump_valve_pin.connected()         ||
      !low_pressure_pump_pin.connected()  ||
      !high_pressure_pump_pin.connected() ||
       error_wl_low_menu.empty()    ||
       error_wl_high_menu.empty()   ||
       error_clean_filters_menu.empty())
    {
      if (!door_switch_pin.connected())
          THEKERNEL->streams->printf("*ERROR* config missing definition for the waterjetcutter.door_switch_pin\n");

      if (!middle_float_pin.connected())
          THEKERNEL->streams->printf("*ERROR* config missing definition for the waterjetcutter.middle_float_pin\n");

      if (!high_float_pin.connected())
            THEKERNEL->streams->printf("*ERROR* config missing definition for the waterjetcutter.high_float_pin\n");

      if (!dump_valve_pin.connected())
          THEKERNEL->streams->printf("*ERROR* config missing definition for the waterjetcutter.dump_valve_pin\n");

      if (!low_pressure_pump_pin.connected())
            THEKERNEL->streams->printf("*ERROR* config missing definition for the waterjetcutter.low_pressure_pump_pin\n");

      if (!high_pressure_pump_pin.connected())
            THEKERNEL->streams->printf("*ERROR* config missing definition for the waterjetcutter.high_pressure_pump_pin\n");

      if (error_wl_low_menu.empty())
            THEKERNEL->streams->printf("*ERROR* config missing definition for the waterjetcutter.error_wl_low_menu\n");

      if (error_wl_high_menu.empty())
            THEKERNEL->streams->printf("*ERROR* config missing definition for the waterjetcutter.error_wl_high_menu\n");

      if (error_clean_filters_menu.empty())
            THEKERNEL->streams->printf("*ERROR* config missing definition for the waterjetcutter.error_clean_filters_menu\n");
    //    delete this;
        config_ok=false;
  }
  return config_ok;
}

void WaterJetCutter::on_module_loaded()
{
    // if the module is disabled -> do nothing
    if(!THEKERNEL->config->value( waterjetcutter_checksum, enable_checksum )->by_default(false)->as_bool()) {
        // as this module is not needed free up the resource
        delete this;
        return;
    }

    previous_state = NONE;
    current_state = NONE;

    //door_switch detector
    door_switch_pin.from_string( THEKERNEL->config->value(waterjetcutter_checksum, door_switch_pin_checksum)->by_default("P2.12" )->as_string())->as_input();

    //middle_float detector
    middle_float_pin.from_string( THEKERNEL->config->value(waterjetcutter_checksum, middle_float_pin_checksum)->by_default("P1.28" )->as_string())->as_input();

    //high_float detector
    high_float_pin.from_string( THEKERNEL->config->value(waterjetcutter_checksum, high_float_pin_checksum)->by_default("P1.29" )->as_string())->as_input();

    //dump valve
    dump_valve_pin.from_string( THEKERNEL->config->value(waterjetcutter_checksum, dump_valve_pin_checksum)->by_default("P2.5" )->as_string())->as_output();

    //low pressure pump
    low_pressure_pump_pin.from_string( THEKERNEL->config->value(waterjetcutter_checksum, low_pressure_pump_pin_checksum)->by_default("P2.7" )->as_string())->as_output();

    //high pressure pump
    high_pressure_pump_pin.from_string( THEKERNEL->config->value(waterjetcutter_checksum, high_pressure_pump_pin_checksum)->by_default("P1.22" )->as_string())->as_output();

    //now check for the menu definitions
    error_wl_low_menu =  THEKERNEL->config->value(waterjetcutter_checksum, error_wl_low_menu_checksum)->by_default(0)->as_string();
    error_wl_high_menu = THEKERNEL->config->value(waterjetcutter_checksum, error_wl_high_menu_checksum)->by_default(0)->as_string();
    error_clean_filters_menu =     THEKERNEL->config->value(waterjetcutter_checksum, error_clean_filters_menu_checksum)->by_default(0)->as_string();

    //we must have all the input and output pins and menus defined otherwise generate an error message and delete ourselves
    if (!validate_config())
        return;

    //defaults to 1 hour
    fill_cycle_seconds          = THEKERNEL->config->value(waterjetcutter_checksum, fill_cycle_seconds_checksum         )->by_default(20.0f)->as_number();
    //defaults to 2 hours
    filter_cleaning_seconds     = THEKERNEL->config->value(waterjetcutter_checksum, filter_cleaning_seconds_checksum    )->by_default(40.0f)->as_number();
    //defaults to 1 week
    water_level_too_low_seconds = THEKERNEL->config->value(waterjetcutter_checksum, water_level_too_low_seconds_checksum)->by_default(2400.0f)->as_number();


    if(middle_float_pin.connected() && high_float_pin.connected()) {
        // input pin polling
        THEKERNEL->slow_ticker->attach( 100, this, &WaterJetCutter::button_tick);
    }
    register_for_event(ON_SECOND_TICK);
    register_for_event(ON_MAIN_LOOP);
    register_for_event(ON_CONSOLE_LINE_RECEIVED);
   // this->register_for_event(ON_GCODE_RECEIVED);
    register_for_event(ON_GCODE_RECEIVED);
    register_for_event(ON_SET_PUBLIC_DATA);
    register_for_event(ON_GET_PUBLIC_DATA);
}


void WaterJetCutter::send_command(std::string msg, StreamOutput *stream)
{
    struct SerialMessage message;
    message.message = msg;
    message.stream = stream;
    THEKERNEL->call_event(ON_CONSOLE_LINE_RECEIVED, &message );
}

// needed to detect when we resume
void WaterJetCutter::on_console_line_received( void *argument )
{
//    if(suspended) {
      SerialMessage new_message = *static_cast<SerialMessage *>(argument);
      string possible_command = new_message.message;
      string cmd = shift_parameter(possible_command);
      if(cmd == "resume" || cmd == "M601") {
          paused= false;
      }
//   }
}

void WaterJetCutter::on_gcode_received(void *argument)
{
    Gcode *gcode = static_cast<Gcode *>(argument);
    if (gcode->has_m){
        if (gcode->m == 1400){ //print out the status of all the pins and outputs
            if (!validate_config())
              gcode->stream->printf("Fatal Config Errors, WaterJet Cutter will not operate with \\sd\\config file errors\n");
            gcode->stream->printf("Previous State was "); printf_state(gcode,this->previous_state);  gcode->stream->printf("\n");
            gcode->stream->printf("Current  State  is "); printf_state(gcode,this->current_state); gcode->stream->printf("\n");
            gcode->stream->printf("active %s\n",                 this->active ? "true" : "false");
            gcode->stream->printf("paused %s\n",                 this->paused ? "true" : "false");
            gcode->stream->printf("middle_float_pin %s\n",       this->middle_float_pin.get() ? "on" : "off");
            gcode->stream->printf("high_float_pin %s\n",         this->high_float_pin.get() ? "on" : "off");
            gcode->stream->printf("dump_valve_pin %s\n",         this->dump_valve_pin.get() ? "on" : "off");
            gcode->stream->printf("low_pressure_pump_pin %s\n",  this->low_pressure_pump_pin.get() ? "on" : "off");
            gcode->stream->printf("high_pressure_pump_pin %s\n", this->high_pressure_pump_pin.get() ? "on" : "off");
            gcode->stream->printf("fill_timer %lu trigger limit %lu\n", this->fill_timer, this->fill_cycle_seconds);
            gcode->stream->printf("dump_timer %lu trigger limit %lu\n", this->dump_timer, this->filter_cleaning_seconds);
            gcode->stream->printf("M1401 to enable this module\n");
            gcode->stream->printf("M1402 to disable this module\n");
            gcode->stream->printf("M1403 is required at the very start of the stream of G-codes to turn on  the water and abrasive\n");
            gcode->stream->printf("M1404 to required at the very  end  of the stream of G-codes to turn off the water and abrasive\n");
//            gcode->stream->printf("M1405 to run on debug mode which issues a M1400 every second\n");
//            gcode->stream->printf("M1406 to disable debug mode\n");
        } else if (gcode->m == 1401) { // disable Cutting Water Tank
            active=true;
        } else if (gcode->m == 1402) { // enable Cutting Water Tank
            active= false;
        } else if (gcode->m == 1403){ //prepare to receive G-codes from the host program
            change_state(RUN_MODE); //change to MENU_MODE later in development
        } else if (gcode->m == 1404) { //end of G-codes from the host program
            change_state(END_JOB);
        } else if (gcode->m == 1405) { // enable debug mode
            debug = true;
        } else if (gcode->m == 1406) { // disable debug mode
            debug = false;
        } else gcode->stream->printf("M-Code %d ignored\n", gcode->m);
    } else if (gcode->has_g)
      gcode->stream->printf("G-Code %d ignored\n", gcode->g);
}

void WaterJetCutter::low_water() {
  if (seconds_elapsed > delay_seconds) { // to prevent the menu being constantly displayed
      seconds_elapsed = 0; //reset counter
      //Note: when the user selects "OK" on the menu it must also issue an M601 or "resume"
      THEKERNEL->current_path = this->error_wl_low_menu.c_str();
      THEPANEL->setup_menu(THEPANEL->max_screen_lines());
      THEPANEL->menu_update(); //force the panel to update the menu
      THEPANEL->lcd->setLed(LED_ORANGE, true);
      THEPANEL->lcd->setLed(LED_GREEN, false);
  }
}
void WaterJetCutter::high_water() {
  if (seconds_elapsed > delay_seconds) { // to prevent the menu being contantly displayed
      seconds_elapsed = 0; //reset counter
      //Note: when the user selects "OK" on the menu it must also issue an M601 or "resume"
      THEKERNEL->current_path = this->error_wl_high_menu.c_str();
      THEPANEL->setup_menu(THEPANEL->max_screen_lines());
      THEPANEL->menu_update(); //force the panel to update the menu
      THEPANEL->lcd->setLed(LED_ORANGE, true);
      THEPANEL->lcd->setLed(LED_GREEN, false);
  }
}

void WaterJetCutter::clear_filter() {
  if (seconds_elapsed > delay_seconds) { // to prevent the menu being contantly displayed
      seconds_elapsed = 0; //reset counter
      //Note: when the user selects "OK" on the menu it must also issue an M601 or "resume"
      THEKERNEL->current_path = this->error_clean_filters_menu.c_str();
      THEPANEL->setup_menu(THEPANEL->max_screen_lines());
      THEPANEL->menu_update(); //force the panel to update the menu
      THEPANEL->lcd->setLed(LED_ORANGE, true);
      THEPANEL->lcd->setLed(LED_GREEN, false);
  }
}

void WaterJetCutter::on_main_loop(void *argument)
{
  if (!active) return;

  switch(this->current_state) {
          case NONE:           //default power on state module not enabled, or after a M1402 disable module command
            break;
          case MENU_MODE:      //after module is enabled with a M1401, or you can return to this after a M1404 job finished
            change_state(RUN_MODE);
            break;
          case PREP_MODE:       //user stuff to get the system set up

            break;
          case DRY_RUN_MODE:    //user may or may not do this to run a job with no water or abrasive

            break;
          case READY_TO_RUN_MODE: //make sure door closed, user presses "Start/Stop" or OK on the appropriate menu to move onto be able to run the G-Code, go to START

            break;
          case RUN_MODE:          //high pressure pump and then abrasive aka M1403, typically then go to RUN_GCODE_MODE_AND_WATCH_WATER_LEVEL
            _run_mode();
            break;
          case RUN_MODE_AND_WATCH_WATER_LEVEL:  //g-code is being executed, the water tank under the cutter is being monitored

            break;
          case PAUSE:          //pause, this stops motion, high pressue water, abrasive ?what happens to the fill and dump timers, low pressure pump etc??
            pause();
            break;
          case RESUME:         //back to RUN_GCODE_MODE_AND_WATCH_WATER_LEVEL (typically)
            this->change_state(RUN_MODE);
            break;
          case END_JOB:        //this is like PAUSE but it can only return to the MENU_MODE as it is intended to complete the job, triggered by a M1404 from the host

          //The next four only occur during RUN_GCODE_MODE_AND_WATCH_WATER_LEVEL
          case IDLE_TOO_LONG:  //unit has been sitting idle for a long time and the water tank may need topping up

            break;
          case WAITING_FOR_WATER_LEVEL_TO_RAISE: //filling up the water tank, then to RUN_GCODE_MODE_AND_WATCH_WATER_LEVEL when fixed

            break;
          case DUMP_WATER:  //water has reached the middle float and we need to release some to prevent overflow

            break;
          case FILTERS_BLOCKED: //not draining quick enough; check the filters return to previous MODE which should be RUN_GCODE_MODE_AND_WATCH_WATER_LEVEL when fixed

            break;
          case WATER_LEVEL_TOO_HIGH:  //we must PAUSE to turn off the water, something is wrong and we risk a water overflow; this could be triggered when in the FILTERS_BLOCKED state

            break;
          case GANTRY_STALLED: //not yet implemented, just a place-holder

            break;
          case END_STOPS_TRIGGERED: //display the error, job is effectively lost, go back to MENU_MODE

            break;
          case MENU_PASSED_BAD_STATE: //oh no!!!, the menu system on sd card has an "action state XXX" where XXX is not a valid state

            break;
          case NUMBER_OF_STATES: //this is a dummy state and should never be used

            break;
  }
}

void WaterJetCutter::_run_mode(void)
{
  if (!active) return;

  if(!middle_float_detected)
  {
    if (!fill_timer_run) {
      fill_timer_start=true;  //if the timer is not running, start the timer from 0
    }
    dump_valve_pin.set(false); //turn off the dump valve so no more water leaves
    if (fill_timer > fill_cycle_seconds) {
        error = true;
        //the fill timer has been exceeded and the middle_float is still not set so there
        //is not enough water in the tank so the user needs to fill it before we start
        // fire suspend command so Smoothie will not process any G or M-Codes until this is fixed
        this->pause();
        //is the low_water menu being displayed?
        if (this->error_wl_low_menu.compare(THEKERNEL->current_path) !=0) {
            //tell the user
            //Note: when the user selects "OK" on the menu it must also issue an M601 or "resume"
            low_water();
        }
    }
  }

  if(middle_float_detected)
  {
    if(!dump_timer_run)
      dump_timer_start=true;  //if the timer is not running, start the timer from 0
    dump_valve_pin.set(true); //turn on the dump valve so water leaves the tank under the cutter
    if (dump_timer > filter_cleaning_seconds)
        { //YYY seconds
        error = true;
        //the dump timer has been exceeded and the middle_float is still set
        //this indicates the filters are blocked so tell the user
        this->pause();
        //is the clear_filter menu being displayed?
        if (this->error_clean_filters_menu.compare(THEKERNEL->current_path) !=0)
          {
            //tell the user
            //Note: when the user selects "OK" on the menu it must also issue an M601 or "resume"
            clear_filter();
          }
    }

    if (high_float_detected)
    {
        error = true;
        //danger the water has exceeded the maximum float, time to take drastic action
        dump_valve_pin.set(true); //turn on the dump valve to let water out
        low_pressure_pump_pin.set(true); //start the low_pressure_pump_pin to help expel the water
        this->pause();
        //is the high_water menu being displayed?
        if (this->error_wl_high_menu.compare(THEKERNEL->current_path) !=0)
          {
            //tell the user
            //Note: when the user selects "OK" on the menu it must also issue an M601 or "resume"
            high_water();
          }
    }
    if (error == false)
    {
      // All good so reset the LEDs
      THEPANEL->lcd->setLed(LED_ORANGE, false);
      THEPANEL->lcd->setLed(LED_GREEN, true);
    }
  }
}

void WaterJetCutter::on_second_tick(void *argument)
{
  if ((!active) || (paused)) return;

  if (fill_timer_start) {
      fill_timer_start = false;
      fill_timer_run = true;
      fill_timer = 0;
      dump_timer_run = false; //force it to stop
  }
  if (fill_timer_stop) {
      fill_timer_stop = false;
      fill_timer_run = false;
  }

  if (dump_timer_start) {
      dump_timer_start = false;
      dump_timer_run = true;
      dump_timer = 0;
      fill_timer_run = false; //force it to stop
  }
  if (dump_timer_stop) {
      dump_timer_stop = false;
      dump_timer_run = false;
  }

  if (abrasive_hopper_timer_start) {
      abrasive_hopper_timer_start = false;
      abrasive_hopper_timer_run = true;
      abrasive_hopper_timer = 0;
  }
  if (abrasive_hopper_timer_stop) {
      abrasive_hopper_timer_stop = false;
      abrasive_hopper_timer_run = false;
   }

  if (spent_abrasive_timer_start) {
      spent_abrasive_timer_start = false;
      spent_abrasive_timer_run = true;
      spent_abrasive_timer = 0;
  }

  if (fill_timer_run)            fill_timer++;
  if (dump_timer_run)            dump_timer++;
  if (abrasive_hopper_timer_run) abrasive_hopper_timer++;
  if (spent_abrasive_timer_run)  spent_abrasive_timer++;

  seconds_elapsed++;
/*
  if (debug) {
    Gcode gc("M42", &(StreamOutput::NullStream));  // send a gcode to the kernel for processing DHP
    THEKERNEL->call_event(ON_GCODE_RECEIVED, &gc);
  }
*/
}

uint32_t WaterJetCutter::button_tick(uint32_t dummy)
{
    //if (suspended || !active) return 0;

    if (middle_float_pin.connected()) {
        // did we get a trigger from the middle_float detector
        this->middle_float_detected= middle_float_pin.get();
    }

    if (high_float_pin.connected()) {
        // did we get a trigger from the high_float detector
        this->high_float_detected= high_float_pin.get();
    }
    return 0;
}

void WaterJetCutter::play(void)
{
  active= true;
  /*
Check to see if "Input 11" is activated
   If yes, then continue
   If no, then display "Close Door" for 5 seconds and return to previous state
   */

  if (this->paused) return; // paused

  THEPANEL->lcd->setLed(LED_ORANGE, false);
  THEPANEL->lcd->setLed(LED_GREEN, true);
  abrasive_hopper_timer_run = true; //Start the "Abrasive Hopper Timer"
  spent_abrasive_timer_run = true;  //Start the "Spent Abrasive Timer"
  //Note we need to convert these to direct pin control as M-codes are suspended at this point!!
  this->send_command( "M5", &(StreamOutput::NullStream) ); //M5 #turn the HP valve off
  this->send_command( "M42", &(StreamOutput::NullStream) ); //M42 #Turn the SSR/electric motor/pump on
  this->send_command( "M46", &(StreamOutput::NullStream) ); //M46 #turn the vibration motors on
  this->send_command( "G4 S1.0", &(StreamOutput::NullStream) ); //G4 S2. #pause to allow abrasive in the line to flush
  this->send_command( "M3", &(StreamOutput::NullStream) ); //M3 #open high pressure valve so water jet starts
  this->send_command( "M8", &(StreamOutput::NullStream) ); //M8 #turn the abrasive feed on
}

void WaterJetCutter::resume(void)
    {
/*
Check to see if "Input 11" is activated
  If yes, then continue
  If no, then display "Close Door" for 5 seconds and return to previous state
  */
  abrasive_hopper_timer_run = true; //Start the "Abrasive Hopper Timer"
  spent_abrasive_timer_run = true;  //Start the "Spent Abrasive Timer"

//Initiate appropriate M-Commands (NOTE: this only happens upon restart of playing a G-code file.  These commands are not needed for the initial G-code file playing as it is done through SW G-code file)
  this->send_command( "M42", &(StreamOutput::NullStream) ); //M42 #turn the ssr/electric motor/pump on
  this->send_command( "M46", &(StreamOutput::NullStream) ); //M46 #turn abrasive motors on
  this->send_command( "G4 S1.0", &(StreamOutput::NullStream) ); //G4 S1.0 #Pause to allow for pressure to be built up
  this->send_command( "M3", &(StreamOutput::NullStream) ); //M3 #open high pressure valve so water jet starts
  this->send_command( "M8", &(StreamOutput::NullStream) ); //M8 #turn the abrasive feed on
}

void WaterJetCutter::pause(void)
{
  if (this->paused) return; //already paused

  this->paused= true;
  THEPANEL->lcd->setLed(LED_ORANGE, true);
  THEPANEL->lcd->setLed(LED_GREEN, false);
  this->send_command( "M600", &(StreamOutput::NullStream) ); // fire suspend command
  abrasive_hopper_timer_run = false; //Stop the "Abrasive Hopper Timer"
  spent_abrasive_timer_run = false;  //Stop the "Spent Abrasive Timer"

  //Note we need to convert these to direct pin control as M-codes are suspended at this point!!
  this->send_command( "M9", &(StreamOutput::NullStream) ); //M9 #turn the abrasive feed off
  this->send_command( "G4 S2.0", &(StreamOutput::NullStream) ); //G4 S2. #pause to allow abrasive in the line to flush
  this->send_command( "M47", &(StreamOutput::NullStream) ); //M47 #turn the vibration motors off
  this->send_command( "M5", &(StreamOutput::NullStream) ); //M5 #turn the HP valve off
  this->send_command( "M43", &(StreamOutput::NullStream) ); //M43 #Turn the SSR/electric motor/pump off

}

void WaterJetCutter::change_state(STATE new_state)
{
  if (current_state != new_state)
    {
      previous_state = current_state;
      if (new_state < NUMBER_OF_STATES)
        current_state = new_state;
      else
        current_state = MENU_PASSED_BAD_STATE;
    }
}

