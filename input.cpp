#include "input.hpp"
#include "gl.hpp"
#include "debug.hpp"

#include "GLFW/glfw3.h"
#include <vector>
#include <map>
#include <cstring>

namespace engine {

    double mouseX, mouseY, mouseXold = 0.0, mouseYold = 0.0, mouseMoveX, mouseMoveY;
    bool keyPressed[kb::KeycodesLength];
    int keyStateTest[kb::KeycodesLength];

    struct controlState {
        float value;
        bool state;
        bool pressed;
    } controls[controlSize];

    struct controlMapping {
        controlMapping(int j, bool a, int k) : joystick(j), axis(a), key(k) {}
        controlMapping(int j, bool a, int k, bool ap) : joystick(j), axis(a), key(k), axis_positive(ap) {}
        int joystick;
        bool axis;
        int key;
        bool axis_positive;
    };

    std::multimap<int, controlMapping> *controlMaps;


    //  defaults are for shmup, change later
    struct joystick_t {
        bool enabled = false;
        bool lstick = true;
        bool rstick = false;
        bool dpad = true;
        enum {
            ENGINE_JOYSTICK_DEADZONE_CIRCLE,
            ENGINE_JOYSTICK_DEADZONE_SQUARE,
            ENGINE_JOYSTICK_DEADZONE_DIAMOND
        } lstick_deadzone_type = ENGINE_JOYSTICK_DEADZONE_CIRCLE, rstick_deadzone_type = ENGINE_JOYSTICK_DEADZONE_CIRCLE;
        float lstick_size = 0.3f, rstick_size = 0.3f;
    } joystick_settings;

    void init_input(ini_t *ini) {


        controlMaps = new std::multimap<int, controlMapping>();
        for(int i = 0; i < kb::KeycodesLength; i++) {
            gl::keyState[i] = 0;
        }


       

        //  controls
        const char *inputStrings[] = {
            "up",
            "down",
            "left",
            "right",
            "fire",
            "focus",
            "bomb",
            "pause",
            "quit",
            "restart",
            "skip"
        };

        if(ini) {

            int keys_i = ini_find_section(ini, "Key", 3);
            if(keys_i > -1) {
                int keys_count = ini_property_count(ini, keys_i);
                for(int i = 0; i < keys_count; i++) {
                    const char *name = ini_property_name(ini, keys_i, i);
                    for(int j = 0; j < controlSize; j++) {
                        if(std::strcmp(name, inputStrings[j]) == 0) {
                            int val = strtol(ini_property_value(ini, keys_i, i), nullptr, 0);
                            if(val > 0) {
                                controlMaps->insert(std::make_pair(j, controlMapping(-1, false, val)));
                                log_debug("mapped %s to %d\n", name, val);
                            }
                            break;
                        }
                    } 

                }
            }

            int joystick_i = ini_find_section(ini, "Joystick", 8);
            if(joystick_i > -1) {
                int enabled_i = ini_find_property(ini, joystick_i, "enabled", 7);
                if(enabled_i > -1) {
                    joystick_settings.enabled = std::strcmp(ini_property_value(ini, joystick_i, enabled_i), "true") == 0;
                }

                int lstick_i = ini_find_property(ini, joystick_i, "lstick", 6);
                if(lstick_i > -1) {
                    joystick_settings.lstick = std::strcmp(ini_property_value(ini, joystick_i, lstick_i), "true") == 0;

                    //  insert stick mappings here?
                    if(joystick_settings.lstick) {
                        controlMaps->insert(std::make_pair(inputUp, controlMapping(0, true, GLFW_GAMEPAD_AXIS_LEFT_Y, false)));
                        controlMaps->insert(std::make_pair(inputDown, controlMapping(0, true, GLFW_GAMEPAD_AXIS_LEFT_Y, true)));
                        controlMaps->insert(std::make_pair(inputLeft, controlMapping(0, true, GLFW_GAMEPAD_AXIS_LEFT_X, false)));
                        controlMaps->insert(std::make_pair(inputRight, controlMapping(0, true, GLFW_GAMEPAD_AXIS_LEFT_X, true)));
                    }
                }

                int rstick_i = ini_find_property(ini, joystick_i, "rstick", 6);
                if(rstick_i > -1) {
                    joystick_settings.rstick = std::strcmp(ini_property_value(ini, joystick_i, rstick_i), "true") == 0;
                }

                int dpad_i = ini_find_property(ini, joystick_i, "dpad", 4);
                if(dpad_i > -1) {
                    joystick_settings.dpad = std::strcmp(ini_property_value(ini, joystick_i, dpad_i), "true") == 0;
                }

                int lstick_deadzone_i = ini_find_property(ini, joystick_i, "lstick_deadzone", 15);
                if(lstick_deadzone_i > -1) {
                    const char *lstick_deadzone_v = ini_property_value(ini, joystick_i, lstick_deadzone_i);
                    if(std::strcmp(lstick_deadzone_v, "circle") == 0) {
                        joystick_settings.lstick_deadzone_type = joystick_t::ENGINE_JOYSTICK_DEADZONE_CIRCLE;
                    } else if(std::strcmp(lstick_deadzone_v, "square") == 0) {
                        joystick_settings.lstick_deadzone_type = joystick_t::ENGINE_JOYSTICK_DEADZONE_SQUARE;
                    } else if(std::strcmp(lstick_deadzone_v, "diamond") == 0) {
                        joystick_settings.lstick_deadzone_type = joystick_t::ENGINE_JOYSTICK_DEADZONE_DIAMOND;
                    }
                }

                int rstick_deadzone_i = ini_find_property(ini, joystick_i, "rstick_deadzone", 15);
                if(rstick_deadzone_i > -1) {
                    const char *rstick_deadzone_v = ini_property_value(ini, joystick_i, rstick_deadzone_i);
                    if(std::strcmp(rstick_deadzone_v, "circle") == 0) {
                        joystick_settings.rstick_deadzone_type = joystick_t::ENGINE_JOYSTICK_DEADZONE_CIRCLE;
                    } else if(std::strcmp(rstick_deadzone_v, "square") == 0) {
                        joystick_settings.rstick_deadzone_type = joystick_t::ENGINE_JOYSTICK_DEADZONE_SQUARE;
                    } else if(std::strcmp(rstick_deadzone_v, "diamond") == 0) {
                        joystick_settings.rstick_deadzone_type = joystick_t::ENGINE_JOYSTICK_DEADZONE_DIAMOND;
                    }
                }

                int lstick_size_i = ini_find_property(ini, joystick_i, "lstick_size", 11);
                if(lstick_size_i > -1) {
                    joystick_settings.lstick_size = std::strtof(ini_property_value(ini, joystick_i, lstick_size_i), nullptr);
                }

                int rstick_size_i = ini_find_property(ini, joystick_i, "rstick_size", 11);
                if(rstick_size_i > -1) {
                    joystick_settings.rstick_size = std::strtof(ini_property_value(ini, joystick_i, rstick_size_i), nullptr);
                }

                int joystick_keys_count = ini_property_count(ini, joystick_i);
                for(int i = 0; i < joystick_keys_count; i++) {
                    const char *name = ini_property_name(ini, joystick_i, i);
                    for(int j = 0; j < controlSize; j++) {
                        if(std::strcmp(name, inputStrings[j]) == 0) {
                            int val = strtol(ini_property_value(ini, joystick_i, i), nullptr, 0);
                            if(val > -1) {
                                controlMaps->insert(std::make_pair(j, controlMapping(0, false, val)));
                                log_debug("mapped joystick %s to %d\n", name, val);
                            }
                            break;
                        }
                    } 
                }

                // joysticks = new std::vector<joystick_t>();
            }
        } else {
            //  default controls if there's none in config
            // controlMaps->insert(std::make_pair(inputUp, controlMapping(-1, false, kb::Up)));
            // controlMaps->insert(std::make_pair(inputDown, controlMapping(-1, false, kb::Down)));
            // controlMaps->insert(std::make_pair(inputLeft, controlMapping(-1, false, kb::Left)));
            // controlMaps->insert(std::make_pair(inputRight, controlMapping(-1, false, kb::Right)));
            // controlMaps->insert(std::make_pair(inputFire, controlMapping(-1, false, kb::Z)));
            // controlMaps->insert(std::make_pair(inputFocus, controlMapping(-1, false, kb::LShift)));
            // controlMaps->insert(std::make_pair(inputBomb, controlMapping(-1, false, kb::X)));
            // controlMaps->insert(std::make_pair(inputPause, controlMapping(-1, false, kb::Escape)));
            // controlMaps->insert(std::make_pair(inputQuit, controlMapping(-1, false, kb::Q)));
            // controlMaps->insert(std::make_pair(inputRestart, controlMapping(-1, false, kb::R)));
            // controlMaps->insert(std::make_pair(inputSkip, controlMapping(-1, false, kb::LControl)));

        }


    }

    void inputs() {
        if(glfwWindowShouldClose(gl::window)) {
            gl::quit = true;
        }

        glfwPollEvents();
        glfwGetCursorPos(gl::window, &mouseX, &mouseY);
        mouseMoveX = mouseX - mouseXold;
        mouseMoveY = mouseY - mouseYold;
        mouseXold = mouseX;
        mouseYold = mouseY;

        for(int i=0;i<kb::KeycodesLength;i++) {
            if(gl::keyState[i] > 0 && keyStateTest[i] == 0) {
                keyPressed[i] = true;
            } else {
                keyPressed[i] = false;
            }
            keyStateTest[i] = gl::keyState[i];
        }

        if(gl::gamepads) {
            for(auto i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++) {
                if(gl::gamepads->at(i) != nullptr) {
                    glfwGetGamepadState(i, gl::gamepads->at(i));
                }
            }
        } else {
            //  reinit gl::gamepads, same code as engine
            gl::gamepads = new std::vector<GLFWgamepadstate*>();
            for(auto i = GLFW_JOYSTICK_1; i <= GLFW_JOYSTICK_LAST; i++) {
                if(glfwJoystickPresent(i)) {
                    if(glfwJoystickIsGamepad(i)) {
                        gl::gamepads->push_back(new GLFWgamepadstate);
                        log_debug("Gamepad %d detected\n", i);
                    } else {
                        gl::gamepads->push_back(nullptr);
                        log_debug("Gamepad %d detected but no mapping available, Name: %s | GUID: %s\n", i, glfwGetJoystickName(i), glfwGetJoystickGUID(i));
                    }
                } else {
                    gl::gamepads->push_back(nullptr);
                }
            }
        }

        for(int i = 0; i < controlSize; i++) {
            //  TODO check for each control for multiple input overrides
            bool updated = false;
            for(auto range = controlMaps->equal_range(i); range.first != range.second; range.first++) {
                controlMapping map = range.first->second;
                if(map.joystick > -1) {
                    if(joystick_settings.enabled) {
                        //  skip entire section if joystick is disabled
                        if(map.axis) {
                            //  is axis
                            if(map.key <= GLFW_GAMEPAD_AXIS_LAST) {
                                //  get joystick
                                if(gl::gamepads->at(map.joystick)) {
                                    controls[i].value = gl::gamepads->at(map.joystick)->axes[map.key];
                                    //  axis to button test
                                    //  temp with hardcoded deadzone
                                    if(map.axis_positive) {
                                        if(controls[i].value > 0.3f) {
                                            controls[i].state = true;
                                            controls[i].pressed = true;
                                        } else {
                                            controls[i].state = false;
                                            controls[i].pressed = false;
                                        }
                                    } else {
                                        if(controls[i].value < -0.3f) {
                                            controls[i].state = true;
                                            controls[i].pressed = true;
                                        } else {
                                            controls[i].state = false;
                                            controls[i].pressed = false;
                                        }
                                    }


                                    //  actual implenentation here
                                    // switch(map.key) {
                                        //  switch for axes to check correct deadzone setting
                                    // }
                                }
                                //  else joystick unplugged
                            }
                        } else {
                            //  is button
                            if(map.key <= GLFW_GAMEPAD_BUTTON_LAST) {
                                if(gl::gamepads->at(map.joystick)) {
                                    controls[i].state = gl::gamepads->at(map.joystick)->buttons[map.key];
                                    controls[i].pressed = gl::gamepads->at(map.joystick)->buttons[map.key]; // TODO fix later
                                    //  button to axis test
                                    controls[i].value = controls[i].state ? 1.f : 0.f;
                                }
                                //  else joystick unplugged
                            }
                        }
                    }
                } else {
                    controls[i].state = gl::keyState[map.key];
                    controls[i].pressed = keyPressed[map.key];  //  need to reimplement this too later
                    if(controls[i].state) {
                        controls[i].value = 1.f;
                    }
                }
            }
        }
    }

    bool checkKey(int key) {
        if(controls[key].state) return true;

        

        return false;
    }

    bool checkKeyPressed(int key) {
        if(controls[key].pressed) return true;
        
        return false;
    }

    float checkKeyAxis(int key) {
        return controls[key].value;

    }

    void mouseCapture() {
        glfwSetInputMode(gl::window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if(glfwRawMouseMotionSupported()) {
            glfwSetInputMode(gl::window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    }

    void mouseRelease() {
        glfwSetInputMode(gl::window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}