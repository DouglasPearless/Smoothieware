/*
      This file is part of Smoothie (http://smoothieware.org/). The motion control part is heavily based on Grbl (https://github.com/simen/grbl).
      Smoothie is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
      Smoothie is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
      You should have received a copy of the GNU General Public License along with Smoothie. If not, see <http://www.gnu.org/licenses/>.
*/

/*
CounterTimer is an optional module that will respond to a timer event turn on or off a switch, generate an M-Code
or call a menu in the new file-panel menu system

Author: Douglas Pearless, Douglas.Pearless@pearless.co.nz
*/

#ifndef COUNTERTIMER_MODULE_H
#define COUNTERTIMER_MODULE_H

using namespace std;

#include "Panel.h"
#include "PanelScreen.h"
#include "MainMenuScreen.h"
#include "libs/Module.h"
#include <string>
#include <vector>
using namespace std;

class CounterTimer : public Module
{
    public:
        CounterTimer();
        ~CounterTimer();
        void on_module_loaded();
        void on_second_tick(void *argument);
        void on_gcode_received(void *argument);
        CounterTimer* load_config(uint16_t modcs);

        bool is_armed() const { return armed; }

    private:
        //in this case we store a vector of the switch names not their checksums otherwise we have to continually
        //read the config cache whenever we trigger which is very time consuming

        std::array<std::string,5> store;

        enum TRIGGER_TYPE {LEVEL, BELOW};
        enum STATE {NONE, THRESHOLD, BELOW_THRESHOLD};
        enum REPEATABLE {SINGLESHOT, MULTISHOT};

        // turn the switch on or off
        void set_switch(uint16_t countertimer_switch_cs, bool switch_state);

        // changed state
        void set_state(STATE state);

        // countertimer.hotend.threshold_temp
        float countertimer_threshold_seconds;

//        // countertimer.hotend.switch
//        uint16_t countertimer_switch_cs;

        // our internal second counter
        uint16_t second_counter;

        // the mcode that will arm the switch, 0 means always armed
        uint16_t arm_mcode;

        // the mcode that will disarm the switch, 0 means always armed
        uint16_t disarm_mcode;

        //name of the countertimer
        std::string whoami;

        //name of the menu that is called after al the switches are processed
        std::string menu;

        uint16_t switch_1_cs,switch_2_cs,switch_3_cs,switch_4_cs,switch_5_cs;

        struct {
            bool inverted:1;
            bool armed:1;
            TRIGGER_TYPE trigger:1;
            STATE current_state:2;
            REPEATABLE repeatable:1;
        };
};

#endif
