/*
      This file is part of Smoothie (http://smoothieware.org/). The motion control part is heavily based on Grbl (https://github.com/simen/grbl).
      Smoothie is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
      Smoothie is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
      You should have received a copy of the GNU General Public License along with Smoothie. If not, see <http://www.gnu.org/licenses/>.
*/

/*
CounterTimer is an optional module that will automatically turn on or off a switch, generate an M-Code or invoke
a menu in the new file-panel menu system.

Author: Douglas Pearless, Douglas.Pearless@pearless.co.nz
*/

#include "CounterTimer.h"
#include "libs/Module.h"
#include "libs/Kernel.h"
#include "SwitchPublicAccess.h"

#include "utils.h"
#include "Gcode.h"
#include "Config.h"
#include "ConfigValue.h"
#include "checksumm.h"
#include "PublicData.h"
#include "StreamOutputPool.h"
#include "mri.h"

#define countertimer_checksum                    CHECKSUM("countertimer")
#define enable_checksum                          CHECKSUM("enable")
#define countertimer_threshold_seconds_checksum  CHECKSUM("threshold_seconds")
#define countertimer_type_checksum               CHECKSUM("type")
#define countertimer_switch_checksum             CHECKSUM("switch")
#define countertimer_menu_checksum               CHECKSUM("menu")
#define countertimer_trigger_checksum            CHECKSUM("trigger")
#define countertimer_inverted_checksum           CHECKSUM("inverted")
#define countertimer_arm_command_checksum        CHECKSUM("arm_mcode")
#define countertimer_disarm_command_checksum     CHECKSUM("disarm_mcode")

CounterTimer::CounterTimer()
{
}

CounterTimer::~CounterTimer()
{
    THEKERNEL->unregister_for_event(ON_SECOND_TICK, this);
    THEKERNEL->unregister_for_event(ON_GCODE_RECEIVED, this);
}

// Load module
void CounterTimer::on_module_loaded()
{
    vector<uint16_t> modulist;
    // allow for multiple countertimers
    THEKERNEL->config->get_module_list(&modulist, countertimer_checksum);
    for (auto m : modulist) {
        load_config(m);
    }

    // no longer need this instance as it is just used to load the other instances
    delete this; //TODO should these resources be kept or released? uncommented 2017-08-22
}

CounterTimer* CounterTimer::load_config(uint16_t modcs)
{
    // see if enabled
    if (!THEKERNEL->config->value(countertimer_checksum, modcs, enable_checksum)->by_default(false)->as_bool()) {
        return nullptr;
    }

    // create a new countertimer module
    CounterTimer *ct= new CounterTimer();

    // load settings from config file
    string switchname = THEKERNEL->config->value(countertimer_checksum, modcs, countertimer_switch_checksum)->by_default("")->as_string();
    if(switchname.empty()) {
       THEKERNEL->streams->printf("WARNING TIMERCOUNTER: no switch specified\n");
       return nullptr;
    } else ct->whoami = switchname;

    ct->menu = THEKERNEL->config->value(countertimer_checksum, modcs, countertimer_menu_checksum)->by_default(0)->as_string();

    string switchtype = THEKERNEL->config->value(countertimer_checksum, modcs, countertimer_type_checksum)->by_default("")->as_string();
    if(switchtype == "singleshot") ct->repeatable = SINGLESHOT;
    else ct->repeatable= MULTISHOT;

    // if we should turn the switch on or off when trigger is hit
    ct->inverted = THEKERNEL->config->value(countertimer_checksum, modcs, countertimer_inverted_checksum)->by_default(false)->as_bool();

    // if we should trigger when above and below, or when below through, or when above through the specified temp
    string trig = THEKERNEL->config->value(countertimer_checksum, modcs, countertimer_trigger_checksum)->by_default("level")->as_string();
    if(trig == "level") ct->trigger= LEVEL;
    else if(trig == "below") ct->trigger= BELOW;
    else ct->trigger= LEVEL;

    // the mcode used to arm the switch
    ct->arm_mcode = THEKERNEL->config->value(countertimer_checksum, modcs, countertimer_arm_command_checksum)->by_default(0)->as_number();

    // the mcode used to disarm the switch
    ct->disarm_mcode = THEKERNEL->config->value(countertimer_checksum, modcs, countertimer_disarm_command_checksum)->by_default(0)->as_number();

    ct->countertimer_switch_cs= get_checksum(switchname); // checksum of the switch to use

    ct->countertimer_threshold_seconds = THEKERNEL->config->value(countertimer_checksum, modcs, countertimer_threshold_seconds_checksum)->by_default(50.0f)->as_number();

    // set initial state
    ct->current_state= NONE;
    ct->second_counter = 0; // do test immediately on first second_tick
    // if not defined then always armed, otherwise start out disarmed
    ct->armed= (ct->arm_mcode == 0);

    // Register for events
    ct->register_for_event(ON_SECOND_TICK);

    if(ct->arm_mcode != 0) {
        ct->register_for_event(ON_GCODE_RECEIVED);
    }
    return ct;
}

void CounterTimer::on_gcode_received(void *argument)
{
    Gcode *gcode = static_cast<Gcode *>(argument);

    if(gcode->has_m && gcode->m == this->arm_mcode) {
        this->armed = true;
        this->current_state = BELOW_THRESHOLD;
        gcode->stream->printf("timercounter %s armed\n",whoami.c_str());
    }
    if(gcode->has_m && gcode->m == this->disarm_mcode) {
        this->armed = false;
        this->current_state = NONE;
        gcode->stream->printf("timercounter %s disarmed\n", whoami.c_str());
    }
}

// Called once a second
void CounterTimer::on_second_tick(void *argument)
{
  if (!this->armed) return; //nothing happening

  if (this->current_state == NONE) return; //not in an operational state

  second_counter++;

  if (second_counter >= this->countertimer_threshold_seconds) {
        set_state(THRESHOLD);
  } else {
        set_state(BELOW_THRESHOLD);
  }
}

void CounterTimer::set_state(STATE state)
{
    vector<uint16_t> modulist;
    if(state == this->current_state) return; // state did not change

    // state has changed
    switch(this->trigger) {
        case LEVEL:
            // switch on or off depending on HIGH or LOW
            set_switch(state == THRESHOLD); //TODO we need to iterate through all the define switches
            second_counter = 0;
            if (this->repeatable == SINGLESHOT) {
               this->current_state = NONE;
               this->armed = false; //turn it off
               THEKERNEL->streams->printf("timercounter %s triggered\n",whoami.c_str());
               //TODO lastly we want to trigger the menu
               THEKERNEL->current_path = this->menu.c_str();
               PanelScreen* current_screen = THEPANEL->current_screen;
               //current_screen->refresh_screen(true);

               THEPANEL->setup_menu(THEPANEL->max_screen_lines());

               //current_screen->refresh_menu(true);
               THEPANEL->menu_update(); //force the panel to update the menu
            } else {
                this->current_state = BELOW_THRESHOLD;
            }
            break;

        case BELOW:
            // switch on if below edge
            //if(this->current_state == BELOW_THRESHOLD) set_switch(true);
            set_switch(state == BELOW_THRESHOLD);
            this->current_state= state;
            break;
    }
}

// Turn the switch on (true) or off (false)
void CounterTimer::set_switch(bool switch_state)
{

    if(!this->armed) return; // do not actually switch anything if not armed

    if(this->arm_mcode != 0 && this->trigger != LEVEL) {
        // if edge triggered we only trigger once per arming, if level triggered we switch as long as we are armed
        this->armed= false;
    } //TODO WE NEED TO FIX AND CHECK THIS LOGI ABOVE

    if(this->inverted) switch_state= !switch_state; // turn switch on or off inverted

    // get current switch state
    struct pad_switch pad;
    bool ok = PublicData::get_value(switch_checksum, this->countertimer_switch_cs, 0, &pad);
    if (!ok) {
        THEKERNEL->streams->printf("// Failed to get switch state.\r\n");
        return;
    }

    if(pad.state == switch_state) return; // switch is already in the requested state

    ok = PublicData::set_value(switch_checksum, this->countertimer_switch_cs, state_checksum, &switch_state);
    if (!ok) {
        THEKERNEL->streams->printf("// Failed changing switch state.\r\n");
    }
}
