/*
      This file is part of Smoothie (http://smoothieware.org/). The motion control part is heavily based on Grbl (https://github.com/simen/grbl).
      Smoothie is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
      Smoothie is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
      You should have received a copy of the GNU General Public License along with Smoothie. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MAINMENUSCREEN_H
#define MAINMENUSCREEN_H

#include "PanelScreen.h"

#define max_path_length 32
#define menu_root "/sd/menu/main"

class MainMenuScreen : public PanelScreen {
    public:
        MainMenuScreen();
        void on_refresh();
        void on_enter();
        void display_menu_line(uint16_t line);
        void clicked_menu_entry(uint16_t line);
        PanelScreen * get_watch_screen() const { return watch_screen; }
        friend class Panel;

    private:
        PanelScreen* watch_screen;
        PanelScreen* file_screen;
        PanelScreen* jog_screen;
        PanelScreen* prepare_screen;

        void abort_playing();
        void setupConfigureScreen();

        void enter_folder(std::string folder);
        uint16_t count_folder_content();
        std::string file_at(uint16_t line, bool& isdir);
        bool filter_file(const char *f);
        std::string filename;
        std::string label;
        std::string title;
        FILE* current_file_handler;
        volatile struct {
            bool only_if_playing_is_token:1;
            bool only_if_playing_is:1;
            bool only_if_halted_is_token:1;
            bool only_if_halted_is:1;
            bool only_if_suspended_is_token:1;
            bool only_if_suspended_is:1;
            bool only_if_file_is_gcode_token:1;
            bool only_if_file_is_gcode:1;
            bool only_if_extruder_token:1;
            bool only_if_extruder:1;
            bool only_if_temperature_control_token:1;
            bool only_if_temperature_control:1;
            bool only_if_laser_token:1;
            bool only_if_laser:1;
            bool only_if_cnc_token:1;
            bool only_if_cnc:1;
            bool is_title_token:1;
            bool is_title:1;
            bool not_selectable_token:1;
            bool not_selectable:1;
            bool file_selector_token:1;
            bool file_selector:1;
            bool action_token:1;
            bool action:1;
        };
};






#endif
