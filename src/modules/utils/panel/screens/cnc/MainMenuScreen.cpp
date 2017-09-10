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


//THEPANEL->lcd->setLed(LED_BED_ON, bed_on);


MainMenuScreen::MainMenuScreen()
{
  THEPANEL->enter_menu_mode(); //start in menu mode
  file_mode = false;
  file_select=false; //reset and file-select in progress;
  file_selected = ""; //make sure we do not have a selected file stored
  file_selected_size = 0; //and set the file size to 0 bytes

	//we need to scan the sd card for the menu system

	if (THEPANEL->internal_sd != nullptr){
		//The SD Card is OK so lets start from the root and build a menu
	    // reset to root directory, I think this is less confusing
	    //this->enter_folder("/sd/panel/menu/main");
	    this->enter_folder(menu_root);
	}
    // Children screens
    //this->jog_screen     = (new JogScreen()     )->set_parent(this);
    this->watch_screen   = (new WatchScreen()   )->set_parent(this);
    //this->file_screen    = (new FileScreen()    )->set_parent(this);
    //this->prepare_screen = (new PrepareScreen() )->set_parent(this);
    //this->set_parent(this->watch_screen);
    this->set_parent(this); //TODO check that this is correct
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
    THEPANEL->setup_menu(THEPANEL->has_laser()?8:7);  //DHP TODO
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
  bool ok = false;
  string::size_type file_index_str_1 = string::npos;

  if ( line == 0 ) {
      THEPANEL->lcd->printf("..");
      filename_index = 1;
  } else {
    if (THEPANEL->is_menu_mode()) {
      ok = parse_menu_line(line);
    } else if (THEPANEL->is_file_mode()) {
      //we have just reached a file-select so we need to change to file_mode to list the GCODE contents of the target directory
      ok = parse_directory_file(line);
    }
    if (ok) {
      //TODO we need to check if the flag has been set to identify if there is a *f (file) or *s (file size), and it so substitute for actual value
      file_index_str_1 = label.find_first_of('*');
      if (file_index_str_1 != string::npos) {
          if (label.find_first_of("fF",file_index_str_1+1) != string::npos) {
              //We have a label that ends in *f or *F so we need to substitute for the file name
              label = label.substr(0,file_index_str_1); //remove the *f or *F
              label.append(file_selected);
          } else  if (label.find_first_of("sS",file_index_str_1+1) != string::npos) {
              //We have a label that ends in *s or *S so we need to substitute for the file size
              label = label.substr(0,file_index_str_1); //remove the *s or *S

              char buffer [sizeof(file_selected_size)*8+1]; //allocate a temporary buffer to convert a number to a string as to_string() is not supported
              sprintf(buffer, "%d", file_selected_size);
              string size_str = buffer;
              //std::string size_str = "487";
              label.append(size_str);
          }
       }
      THEPANEL->lcd->printf("%s", label.c_str());
    }
  }
}

bool MainMenuScreen::parse_directory_file(uint16_t line) {
  bool line_processed;

  //we need to read the nth 'line' file in the current directory, open the file and interpret it; if the file is not to be displayed
  //the we need to open the next on in the directory listing, if there are no more, then simply return.

  line_processed = false;
  std::string current_file;
  uint16_t current_line;

  //TODO need to iterate until a file is found
  bool isdir;
  label = "";
  current_line = filename_index; //start at the last remembered place
  while(!line_processed) {
      current_file = this->file_at_gcode(current_line - 1, isdir).substr(0, max_path_length);
      if (current_file.empty()) { //empty file means we have been through all the files from the 'line'th position to the end of the directory and not found a valid file to display
          line_processed = false;
          //TODO Do we need to update the filename_index to the current_line even if we fail?
          break;
      }
      if(isdir || this->filter_file_gcode(current_file.c_str())) {
          label=current_file;
          line_processed = true;
          filename_index = current_line;
          filename_index++; // we have found the next file, keep a note for the next time we are called so we start from this point in the list of files
      }
      current_line++;
  }
  return line_processed; // did we manage to find a valid file (to display)
}

bool MainMenuScreen::parse_menu_line(uint16_t line)
{

  bool line_processed;

	//we need to read the nth 'line' file in the current menu directory, open the file and interpret it; if the file is not to be displayed
  //the we need to open the next on in the directory listing, if there are no more, then simply return.

  line_processed = false;
  std::string current_file;

	    //TODO need to iterate until the "ith" line is actually displayed - there is not a 1:1 map between lines on the screen and files
	    bool isdir;

	    while(!line_processed) {
	        current_file = this->file_at(filename_index - 1, isdir).substr(0, max_path_length);
	        if (current_file.empty()) { //empty file means we have been through all the files from the 'line'th position to the end of the directory and not found a valid file to display
	            line_processed = false;
	            break;
	        }
	        //make sure the path ends in a '/'
	        if (THEKERNEL->current_path.back() != '/')
	            this->filename = THEKERNEL->current_path + '/' + current_file;
	        else
	           	this->filename = THEKERNEL->current_path + current_file;

	        if(!isdir) {
	            //Now we need to open the file and interpret its contents

	            char buf[130]; // lines up to 128 characters are allowed, anything longer is discarded
	            bool discard = false;

	            //prepare tokens & variables
	            only_if_playing_is_token=false;
	            only_if_playing_is=false;
	            only_if_playing_is_conditional=true;
	            only_if_halted_is_token=false;
	            only_if_halted_is=false;
	            only_if_halted_is_conditional=true;
	            only_if_suspended_is_token=false;
	            only_if_suspended_is=false;
	            only_if_suspended_is_conditional=true;
	            only_if_file_is_gcode_token=false;
	            only_if_file_is_gcode=false;
	            only_if_file_is_gcode_conditional=true;
	            only_if_extruder_token=false;
	            only_if_extruder=false;
	            only_if_extruder_conditional=true;
	            only_if_temperature_control_token=false;
	            only_if_temperature_control=false;
	            only_if_temperature_control_conditional=true;
	            only_if_laser_token=false;
	            only_if_laser=false;
	            only_if_laser_conditional=true;
	            only_if_cnc_token=false;
	            only_if_cnc=false;
	            only_if_cnc_conditional=true;
	            is_title_token=false;
	            is_title=false;
	            is_title_conditional=true;
	            not_selectable_token=false;
	            not_selectable=false;
	            not_selectable_conditional=true;
	            //file_select_token=false; //This is a special case as we must remember per menu iteration so set this at a higher calling level
	            //file_select=false;  //This is set per menu not per menu line
	            file_select_conditional=true;
	            action_token=false;
	            the_action_checksum=0;
	            action=false;
	            action_conditional=true;
	            label = "";
	            title = "";

	            this->current_file_handler = fopen( this->filename.c_str(), "r");

	            if(this->current_file_handler == NULL) {
	                //this should never happen
	                return false;
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

	                    std::string str(buf);

	                    vector<string> tokens;
	                    const string delimiters = " \t\n\r";

	                    // skip delimiters at the start of the line
	                    string::size_type lastPos = str.find_first_not_of(delimiters, 0);

	                    // find first non-delimiter
	                    string::size_type pos = str.find_first_of(delimiters, lastPos);

	                    while (string::npos != pos || string::npos != lastPos) {
	                       // found a token, add it to the vector.
	                       tokens.push_back(str.substr(lastPos, pos - lastPos));

	                       // skip delimiters
	                       lastPos = str.find_first_not_of(delimiters, pos);

	                       // find next non-delimiter
	                       pos = str.find_first_of(delimiters, lastPos);
	                    }
                      if (tokens.size()==0)
	                      break; // No tokens, nothing to process

                      // Note checksums are not const expressions when in debug mode, so don't use switch statement
                      uint16_t token_checksum = get_checksum(tokens[0]);

                      if(token_checksum == label_en_checksum) {
                        if(tokens.size()>=1) {
                            //the label is all the rest of the line *after the token*

                            // skip delimiters at the start of the line
                            string::size_type one = str.find_first_not_of(delimiters, 0);

                            // find first non-delimiter
                            string::size_type two = str.find_first_of(delimiters, one);

                            // skip delimiters at the start of the line
                            string::size_type three = str.find_first_not_of(delimiters, two);

                            std::string file_index_selector = label; //TODO temporary copy filename

                            label = str.substr(three,str.size()-three-1);

                        }
                      } else if(token_checksum == only_if_playing_is_checksum) {
                          only_if_playing_is_token = true;
                          if(tokens.size()>=1) {
                              if(tokens[1].substr(0,1).compare("1")==0) //if == 0 then compare is true
                                only_if_playing_is = true; //TODO need a neat way to interpret a '0' or a '1'
                          }
                      } else if(token_checksum == only_if_halted_is_checksum) {
                          only_if_halted_is_token=true;
                          if(tokens.size()>=1) {
                              if(tokens[1].substr(0,1).compare("1")==0) //if == 0 then compare is true
                                only_if_halted_is = true; //TODO need a neat way to interpret a '0' or a '1'
                          }
                      } else if(token_checksum == only_if_suspended_is_checksum) {
                          only_if_suspended_is_token=true;
                          if(tokens.size()>=1) {
                              if(tokens[1].substr(0,1).compare("1")==0) //if == 0 then compare is true
                                only_if_suspended_is = true; //TODO need a neat way to interpret a '0' or a '1'
                          }
                      } else if(token_checksum == only_if_file_is_gcode_checksum) {
                          only_if_file_is_gcode_token=true;
                          if(tokens.size()>=1) {
                              if(tokens[1].substr(0,1).compare("1")==0) //if == 0 then compare is true
                                only_if_file_is_gcode = true; //TODO need a neat way to interpret a '0' or a '1'
                          }
                      } else if(token_checksum == only_if_extruder_checksum) {
                          only_if_extruder_token=true;
                          if(tokens.size()>=1) {
                              if(tokens[1].substr(0,1).compare("1")==0) //if == 0 then compare is true
                                only_if_extruder = true; //TODO need a neat way to interpret a '0' or a '1'
                          }
                      } else if(token_checksum == only_if_temperature_control_checksum) {
                          only_if_temperature_control_token=true;
                          if(tokens.size()>=1) {
                              if(tokens[1].substr(0,1).compare("1")==0) //if == 0 then compare is true
                                only_if_temperature_control = true; //TODO need a neat way to interpret a '0' or a '1'
                          }
                      } else if(token_checksum == only_if_laser_checksum) {
                          only_if_laser_token=true;
                          if(tokens.size()>=1) {
                              if(tokens[1].substr(0,1).compare("1")==0) //if == 0 then compare is true
                                only_if_laser = true; //TODO need a neat way to interpret a '0' or a '1'
                          }
                      } else if(token_checksum == only_if_cnc_checksum) {
                          only_if_cnc_token=true;
                          if(tokens.size()>=1) {
                              if(tokens[1].substr(0,1).compare("1")==0) //if == 0 then compare is true
                                only_if_cnc = true; //TODO need a neat way to interpret a '0' or a '1'
                          }
                      } else if(token_checksum == is_title_checksum) {
                          is_title_token=true;
                          is_title = true;
                      } else if(token_checksum == not_selectable_checksum) {
                          not_selectable_token = true;
                          not_selectable = true; //TODO need a neat way to interpret a '0' or a '1'
                      } else if(token_checksum == file_select_checksum) {
                          file_select_token=true;
                          if(tokens.size()>=2) {
                              // tokens[1] contains where to start exploring the system
                              // tokens[2] contains the path to the menu to execute once a file is selected
                              file_start = tokens[1];
                              file_menu = tokens[2];
                              file_select = true;
                              file_mode = true; //prepare to go into file mode once this file is finished being processed
                              THEPANEL->enter_file_mode(false);
                              THEKERNEL->current_path=file_start; //NEW DHP
                          }
                      } else if(token_checksum == file_selector_checksum) {
                               file_selector_token=true;
                               if(tokens.size()>=2) {
                                    // tokens[1] contains where to start exploring the system
                                    // tokens[2] contains the path above which the user cannot go
                                    file_start = tokens[1];
                                    file_selected_root = tokens[2];
                                    file_selector = true;
                                    //file_mode = true; //prepare to go into file mode once this file is finished being processed
                                    //THEPANEL->enter_file_mode(false);
                                    THEKERNEL->current_path=file_start; //NEW DHP
                               }
                      } else if(token_checksum == action_checksum) {
                          action_token = true;
                          if(tokens.size()>=2) {
                              // tokens[1] contains the first parameter for the action
                              // tokens[2] contains the optional second parameter
                              the_action_checksum = get_checksum(tokens[1]);
                              the_action_parameter = tokens[2];
                              action = true;
                              //NOTE: Only one action is supported, and if more than one, only the last one is 'actioned'
                          }
                      } else if(tokens[0].substr(0,1).compare("#")==0) {
                          // A comment, skip
//                          break;
                      } else {
                        //we have a problem, an unknown token
                      }
	                } else {
	                    // discard long line
	                    //this->current_stream->printf("Warning: Discarded long line\n");
	                    discard = true;
	                }
	            }

	            fclose(this->current_file_handler);
	            //we now have enough information to process the line and display as needed
	            //TODO if is_title the we need to format the label and keep it on the top line if we scroll

	            //TODO we need to work out all the conditions ('only_if...')
	            //Note never assume the use has put any given condition in only once
	            //only_if_playing_is
              if ( only_if_playing_is_token) {
                  if (only_if_playing_is  &&  THEPANEL->is_playing()) only_if_playing_is_conditional = true; else only_if_playing_is_conditional = false;
                  if (!only_if_playing_is && !THEPANEL->is_playing()) only_if_playing_is_conditional = true; else only_if_playing_is_conditional = false;
              }
              // only_if_halted_is
              if ( only_if_halted_is_token) {
                  if (only_if_halted_is  &&  THEKERNEL->is_halted()) only_if_halted_is_conditional = true; else only_if_halted_is_conditional = false;
                  if (!only_if_halted_is && !THEKERNEL->is_halted()) only_if_halted_is_conditional = true; else only_if_halted_is_conditional = false;
              }
              //only_if_suspended_is
              if ( only_if_suspended_is_token) {
                  if (only_if_suspended_is  &&  THEPANEL->is_suspended()) only_if_suspended_is_conditional = true; else only_if_suspended_is_conditional = false;
                  if (!only_if_suspended_is && !THEPANEL->is_suspended()) only_if_suspended_is_conditional = true; else only_if_suspended_is_conditional = false;
              }
              //only_if_file_is_gcode
              if ( only_if_file_is_gcode_token) {
                  bool found_gcode;
                  std::size_t found_place = file_selected.rfind(".gcode");

                  if (found_place!=std::string::npos) // if we found something, make sure the file ends in ".gcode" and not something like ".gcode.txt"
                    {
                      found_gcode = ((file_selected.size()-6) == found_place);
                    }
                  if (only_if_file_is_gcode  && found_gcode ) only_if_file_is_gcode_conditional = true; else only_if_file_is_gcode_conditional = false;
                  //if (!only_if_file_is_gcode && !found_gcode) only_if_file_is_gcode_conditional = true; else only_if_file_is_gcode_conditional = false;
              }

              //only_if_extruder_conditional
              //only_if_temperature_control_conditional
              //only_if_laser_conditional
              //only_if_cnc_conditional
              //is_title_conditional
              //not_selectable_conditional
              //file_select_conditional

              //action_conditional
              if ( action_token) {
                  action_conditional = action;
              }

              //some of the conditionals will be come redundant as the logic for file-panel becomes complete; all possible conditions are considered at this point
              if (only_if_playing_is_conditional &&
                  only_if_halted_is_conditional &&
                  only_if_suspended_is_conditional  &&
                  only_if_file_is_gcode_conditional &&
                  only_if_extruder_conditional &&
                  only_if_temperature_control_conditional &&
                  only_if_laser_conditional &&
                  only_if_cnc_conditional &&
                  is_title_conditional &&
                  not_selectable_conditional &&
                  file_select_conditional &&
                  action_conditional)
                    line_processed = true;
//              filename_index++; // we have found the next file, keep a note for the next time we are called so we start from this point in the list of files
	        }
          filename_index++; // we have found the next file, keep a note for the next time we are called so we start from this point in the list of files
	    }
	    return line_processed; // did we manage to find a valid file (to display)
}

void MainMenuScreen::clicked_menu_entry(uint16_t line)
{
  bool found = false;
  /*
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
    */
  //because menu lines and files do not align 1:1 we have to rescan the lines starting at the first
  //so we know which menu line relates to which underlying file in the directory
 if(line==0) {
     THEPANEL->enter_menu_mode(false); //TODO this crudely forces back to the the main menu system rather than allow to go up a directory
     this->enter_folder(menu_root);
     found = false;
 } else {
     filename_index = 1; //force the parser to start at the 'line'th place
    for (uint16_t i = THEPANEL->menu_start_line; i < THEPANEL->menu_start_line + min( THEPANEL->menu_rows, THEPANEL->panel_lines ); i++ ) {

      if (THEPANEL->is_file_mode())
         found = parse_directory_file(line);
      else
        found = this-parse_menu_line(line);

      if ((i+1)==line) break;
    }
 }

  if (found) { // a file was found

      if (the_action_checksum==goto_menu_checksum){
           //now navigate to the new menu.
           this->enter_folder(the_action_parameter);
       } else if (the_action_checksum==goto_watch_screen_checksum){
           THEPANEL->enter_screen(this->watch_screen);
       } else if (the_action_checksum==run_command_checksum) {
           this->run_command();//run the command
       } else if (THEPANEL->is_file_mode()){
           //file-select the user has selected a GCODE file and we now need to display a menu to allow an action to be performed on that file
           this->process_file_gcode(line);  //we only exit file mode when a file selection is made or we exit the directory
       }
   }
}

void MainMenuScreen::run_command() {
  //the_action_parameter contains the action (nominally "play" or "rm") and file_selected contains the file to perform the action on
  std::string action_this = the_action_parameter;
  action_this.append(" ");
  action_this.append(current_gcode_dir);
  action_this.append("/");
  action_this.append(file_selected);
  send_command(action_this.c_str());
  THEPANEL->enter_menu_mode(); //Now that we have actioned the command, we flip back into menu mode and start again at the root menu
  //this->enter_folder(menu_root);
  THEPANEL->enter_screen(this->watch_screen);
}
void MainMenuScreen::process_file_gcode(uint16_t line)
{
  bool isdir;
  //TODO we need to take the 'line' file and action it based on the contents of the 'file_menu' folder
  //note the line-1 is required as we have to remove the ".." line 0 which is always present on a menu screen
  if (line>0) {
      file_selected = this->file_at_gcode(line-1,isdir); //What is the file we are going to process in some way
      file_selected_size = file_size(file_selected);
      //Note that the variable 'label' also contains the name of the file too, but we are making sure in case something happens to the 'label'
      THEPANEL->enter_menu_mode(); //Now that we have selected a gcode file, we flip back into menu mode to process the files as a menu
      this->enter_folder(file_menu);
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
  //   make sure the path does not end in a '/'
  if ((folder.size() >=1) && (folder.back() == '/')) //path is not root and ends in a '/' so remove it
    THEKERNEL->current_path= folder.substr(0,folder.size()-1);
  else
    THEKERNEL->current_path= folder;

    // We need the number of lines to setup the menu
    uint16_t number_of_files_in_folder = this->count_folder_content();

    // Setup menu screen
    THEPANEL->setup_menu(number_of_files_in_folder + 1); // same number of files as menu items

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
            //if((p->d_isdir && p->d_name[0] != '.') || filter_file(p->d_name)) count++; //This counter includes directories
            if( filter_file(p->d_name) ) count++;
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
            // filter out files that start with a '.'
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
// Find the "line"th file in the current folder
string MainMenuScreen::file_at_gcode(uint16_t line, bool& isdir)
{
    DIR *d;
    struct dirent *p;
    uint16_t count = 0;
    d = opendir(THEKERNEL->current_path.c_str());
    if (d != NULL) {
        while ((p = readdir(d)) != NULL) {
            // filter out files that start with a '.'
          if(((p->d_isdir && p->d_name[0] != '.') || filter_file_gcode(p->d_name)) && count++ == line ) {
                isdir= p->d_isdir;
                string fn= p->d_name;
                current_gcode_dir = THEKERNEL->current_path.c_str();
                closedir(d);
                return fn;
            }
        }
    }

    if (d != NULL) closedir(d);
    isdir= false;
    return "";
}
// Find the "line"th file in the current folder
uint16_t MainMenuScreen::file_size(string current_file)
{
    uint16_t size = 0;

    DIR *d;
    struct dirent *p;

    d = opendir(THEKERNEL->current_path.c_str());
    if (d != NULL) {
        while ((p = readdir(d)) != NULL) {
            // filter out files that start with a '.'
          if((p->d_name[0] != '.') && (current_file.compare(p->d_name)==0)) { //if == 0 then compare is true
                size = p->d_fsize;
                closedir(d);
                return size;
            }
        }
    }

    if (d != NULL) closedir(d);
    return 0;
}

// only filter files that have a .txt in them and does not start with a .
bool MainMenuScreen::filter_file(const char *f)
{
    string fn= lc(f);
    return (fn.at(0) != '.') && (fn.find(".txt") != string::npos);
}

// only filter files that have a .gcode in them and does not start with a .
bool MainMenuScreen::filter_file_gcode(const char *f)
{
    string fn= lc(f);
    return (fn.at(0) != '.') && (fn.find(".gcode") != string::npos);
}
// play a file
void MainMenuScreen::play(const char *path)
{
  std::string cmd;
  cmd = string("play ").append(path);
  send_command(cmd.c_str());
}
