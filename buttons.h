#ifndef BUTTONS_H_
#define BUTTONS_H_

void update_rgbhsv_from_pos();
void update_pos_from_rgbhsv();
void set_testing_led(int r, int g, int b);
void debounce_buttons();
void handle_rotary_encoder();
void do_button_animations();
void handle_short_release();
void handle_long_press();

void setup_buttons();
void handle_buttons();

#endif /* BUTTONS_H_ */
