/*
      This file is part of Smoothie (http://smoothieware.org/). The motion control part is heavily based on Grbl (https://github.com/simen/grbl).
      Smoothie is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
      Smoothie is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
      You should have received a copy of the GNU General Public License along with Smoothie. If not, see <http://www.gnu.org/licenses/>.
*/

#include "libs/Kernel.h"
#include "Panel.h"
#include "PanelScreen.h"
#include "LcdBase.h"
#include "MainMenuScreen.h"
#include "WatchScreen.h"
#include "FileScreen.h"
#include "JogScreen.h"
#include "ControlScreen.h"
#include "PrepareScreen.h"
#include "ProbeScreen.h"
#include "libs/nuts_bolts.h"
#include "libs/utils.h"
#include "modules/utils/player/PlayerPublicAccess.h"
#include "PublicData.h"
#include "checksumm.h"
#include "ModifyValuesScreen.h"
#include "Robot.h"
#include "Planner.h"
#include "StepperMotor.h"
#include "EndstopsPublicAccess.h"
#include "LaserScreen.h"
#include "DirHandle.h"
#include "mbed.h"
#include "PanelMenuCommands.h"

using std::vector;

#include <string>
using namespace std;

#define extruder_checksum CHECKSUM("extruder")

MainMenuScreen::MainMenuScreen()
{
	//we need to scan the sd card for the menu system

	if (THEPANEL->internal_sd != nullptr){
		//The SD Card is OK so lets start from the root and build a menu
	    // reset to root directory, I think this is less confusing
	    //this->enter_folder("/sd/menu/main");
	    this->enter_folder(menu_root);
	}
    // Children screens
    this->jog_screen     = (new JogScreen()     )->set_parent(this);
    this->watch_screen   = (new WatchScreen()   )->set_parent(this);
    this->file_screen    = (new FileScreen()    )->set_parent(this);
    this->prepare_screen = (new PrepareScreen() )->set_parent(this);
    this->set_parent(this->watch_screen);
}

// setup and enter the configure screen
void MainMenuScreen::setupConfigureScreen()
{
    auto mvs= new ModifyValuesScreen(true); // delete itself on exit
    mvs->set_parent(this);

   // acceleration
    mvs->addMenuItem("def Acceleration", // menu name
        []() -> float { return THEROBOT->get_default_acceleration(); }, // getter
        [this](float acc) { send_gcode("M204", 'S', acc); }, // setter
        10.0F, // increment
        1.0F, // Min
        10000.0F // Max
        );

    // steps/mm
    mvs->addMenuItem("X steps/mm",
        []() -> float { return THEROBOT->actuators[0]->get_steps_per_mm(); },
        [](float v) { THEROBOT->actuators[0]->change_steps_per_mm(v); },
        0.1F,
        1.0F
        );

    mvs->addMenuItem("Y steps/mm",
        []() -> float { return THEROBOT->actuators[1]->get_steps_per_mm(); },
        [](float v) { THEROBOT->actuators[1]->change_steps_per_mm(v); },
        0.1F,
        1.0F
        );

    mvs->addMenuItem("Z steps/mm",
        []() -> float { return THEROBOT->actuators[2]->get_steps_per_mm(); },
        [](float v) { THEROBOT->actuators[2]->change_steps_per_mm(v); },
        0.1F,
        1.0F
        );

    mvs->addMenuItem("Contrast",
        []() -> float { return THEPANEL->lcd->getContrast(); },
        [this](float v) { THEPANEL->lcd->setContrast(v); },
        1,
        0,
        255,
        true // instant update
        );

    THEPANEL->enter_screen(mvs);
}

void MainMenuScreen::on_enter()
{
    THEPANEL->enter_menu_mode();
    THEPANEL->setup_menu(THEPANEL->has_laser()?8:7);
    this->refresh_menu();
}

void MainMenuScreen::on_refresh()
{
    if ( THEPANEL->menu_change() ) {
        this->refresh_menu();
    }
    if ( THEPANEL->click() ) {
        this->clicked_menu_entry(THEPANEL->get_menu_current_line());
    }
}

void MainMenuScreen::display_menu_line(uint16_t line)
{

	//we need to read the nth file in the current menu directory, open the file and interpret it.
	if ( line == 0 ) {
	        THEPANEL->lcd->printf("..");
	    } else {
	        bool isdir;
//	        THEPANEL->lcd->printf("%s", this->file_at(line - 1, isdir).substr(0, 18).c_str());
	        //make sure the path ends in a '/' note I could not .append ( ).append( ) as the second append never got added, no idea why!
	        if (THEKERNEL->current_path.back() != '/')
	            this->filename = THEKERNEL->current_path + '/' + this->file_at(line - 1, isdir).substr(0, max_path_length);
	        else
	           	this->filename = THEKERNEL->current_path + this->file_at(line - 1, isdir).substr(0, max_path_length);
//	        THEPANEL->lcd->printf("%s", this->file_at(line - 1, isdir).substr(0, 19).c_str()); //TODO 19 should not be hard wired, it should the the panel character width - 2 places

	        //this->filename.append(this->file_at(line - 1, isdir).substr(0, 18));
//	        if(isdir) {
//	            if(fn.size() >= 18) fn.back()= '/';
//	            else fn.append("/");
//	        }
	        if(!isdir) {
	            //THEPANEL->lcd->printf("%s", this->filename.c_str());
	            //Now we need to open the file and interpret its contents

	            char buf[130]; // lines up to 128 characters are allowed, anything longer is discarded
	            bool discard = false;

//	            if(this->current_file_handler != NULL) { //just in case it is open
//	                fclose(this->current_file_handler);
//	            }

	            this->current_file_handler = fopen( this->filename.c_str(), "r");
	            if(this->current_file_handler == NULL) {
	                //stream->printf("File not found: %s\r\n", this->filename.c_str());  //this should never happen
	                return;
	            }
	            while(fgets(buf, sizeof(buf), this->current_file_handler) != NULL) {
	                int len = strlen(buf);
	                if(len == 0) continue; // empty line? should not be possible
	                if(buf[len - 1] == '\n' || feof(this->current_file_handler)) {
	                    if(discard) { // we are discarding a long line
	                        discard = false;
	                        continue;
	                    }
	                    if(len == 1) continue; // empty line

	                    //We now have a line from the file and need to interpret its contents
	                    //Supported tokens are:
	                    //label-<en/es/fr> label-text
	                    //action <action> <parameter>
	                    //[only-if-playing-is |
	                    // only-if-halted-is |
	                    // only-if-suspended-is |
	                    // only-if-file-is-gcode |
	                    // is-title <negative|left|centered|right> |
	                    // not-selectable |
	                    // file-selector <root-of-dirs> <not-not-allow-above-this-file-path
	    	              // only-if-extruder
	                    // only-if-temperature-control
	                    // only-if-laser
	                    // goto-menu <menu-path>
	                    // run-command <command. <parameters>
	                    // action control-axis [X|Y|Z] <distance-in-mm>
	                    // action control-extruder [0..n] <distance-in-nn>
	                    // file-select <start-point-in-file> <file-to-execute>
	                    //
	                    ///THEPANEL->lcd->printf("%s", buf);
	                    //

	                    std::string str(buf);

	                    vector<string> tokens;
	                    const string delimiters = " \t\n\r";

	                      // skip delimiters at beginning.
	                          string::size_type lastPos = str.find_first_not_of(delimiters, 0);

	                      // find first "non-delimiter".
	                          string::size_type pos = str.find_first_of(delimiters, lastPos);

	                          while (string::npos != pos || string::npos != lastPos)
	                          {
	                              // found a token, add it to the vector.
	                              tokens.push_back(str.substr(lastPos, pos - lastPos));

	                              // skip delimiters.  Note the "not_of"
	                              lastPos = str.find_first_not_of(delimiters, pos);

	                              // find next "non-delimiter"
	                              pos = str.find_first_of(delimiters, lastPos);
	                          }
                      if (tokens.size()==0)
	                      break; // No tokens, nothing to process

                      uint16_t token_checksum = get_checksum(tokens[0]);
                      if(token_checksum == label_en_checksum) {
                        if(tokens.size()>=1) {
                          THEPANEL->lcd->printf("%s", tokens[1].c_str());
                          break;
                        }

                      }

	                } else {
	                    // discard long line
	                    //this->current_stream->printf("Warning: Discarded long line\n");
	                    discard = true;
	                }
	            }

	            fclose(this->current_file_handler);
	        }
	    }
}

void MainMenuScreen::clicked_menu_entry(uint16_t line)
{
    switch ( line ) {
        case 0: THEPANEL->enter_screen(this->watch_screen); break;
        case 1:
            if(THEKERNEL->is_halted()){
                send_command("M999");
                THEPANEL->enter_screen(this->watch_screen);
            }else if(THEPANEL->is_playing()) abort_playing();
             else THEPANEL->enter_screen(this->file_screen); break;
        case 2: THEPANEL->enter_screen(this->jog_screen     ); break;
        case 3: THEPANEL->enter_screen(this->prepare_screen ); break;
        case 4: THEPANEL->enter_screen(THEPANEL->custom_screen ); break;
        case 5: setupConfigureScreen(); break;
        case 6: THEPANEL->enter_screen((new ProbeScreen())->set_parent(this)); break;
        case 7: THEPANEL->enter_screen((new LaserScreen())->set_parent(this)); break; // self deleting, only used if THEPANEL->has_laser()
    }
}

void MainMenuScreen::abort_playing()
{
    //PublicData::set_value(player_checksum, abort_play_checksum, NULL);
    send_command("abort");
    THEPANEL->enter_screen(this->watch_screen);
}

// Enter a new folder
void MainMenuScreen::enter_folder(std::string folder)
{
    // Remember where we are
    THEKERNEL->current_path= folder;

    // We need the number of lines to setup the menu
    uint16_t number_of_files_in_folder = this->count_folder_content();

    // Setup menu
    THEPANEL->setup_menu(number_of_files_in_folder + 1); // same number of files as menu items
    THEPANEL->enter_menu_mode();

    // Display menu
    this->refresh_menu();
}

// Count how many files there are in the current folder that have a .txt in them and does not start with a .
uint16_t MainMenuScreen::count_folder_content()
{
    DIR *d;
    struct dirent *p;
    uint16_t count = 0;
    d = opendir(THEKERNEL->current_path.c_str());
    if (d != NULL) {
        while ((p = readdir(d)) != NULL) {
            if((p->d_isdir && p->d_name[0] != '.') || filter_file(p->d_name)) count++;
        }
        closedir(d);
        return count;
    }
    return 0;
}

// Find the "line"th file in the current folder
string MainMenuScreen::file_at(uint16_t line, bool& isdir)
{
    DIR *d;
    struct dirent *p;
    uint16_t count = 0;
    d = opendir(THEKERNEL->current_path.c_str());
    if (d != NULL) {
        while ((p = readdir(d)) != NULL) {
            // only filter files that have a .g in them and directories not starting with a .
          if(((p->d_isdir && p->d_name[0] != '.') || filter_file(p->d_name)) && count++ == line ) {
                isdir= p->d_isdir;
                string fn= p->d_name;
                closedir(d);
                return fn;
            }
        }
    }

    if (d != NULL) closedir(d);
    isdir= false;
    return "";
}

// only filter files that have a .g, .ngc or .nc in them and does not start with a .
bool MainMenuScreen::filter_file(const char *f)
{
    string fn= lc(f);
    return (fn.at(0) != '.') && (fn.find(".txt") != string::npos);
}
