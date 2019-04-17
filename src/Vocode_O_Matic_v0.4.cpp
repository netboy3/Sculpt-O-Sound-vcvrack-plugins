#include "std.hpp"
#include "matrix.hpp"
#include "pan_and_level.hpp"
#include "Sculpt-O-Sound.hpp"
#include "comp_coeffs.hpp"
#include "dsp/digital.hpp"
#include "../deps/SynthDevKit/src/CV.hpp"

#define INITIAL_CARRIER_GAIN 1.0
#define INITIAL_MODULATOR_GAIN 1.0

//#define PANNING

// Dimensions of matrix of buttons.
#define LED_WIDTH 10
#define LED_HEIGHT 10

#define VBASE NR_OF_BANDS * LED_HEIGHT + 40

#define ORIGIN_BOTTOM_LEFT

#define HBASE 140

struct Vocode_O_Matic : Module {

  // Define CV trigger a la synthkit for shifting the matrix.
  SynthDevKit::CV *cv_right = new SynthDevKit::CV(0.1f);
  SynthDevKit::CV *cv_left =  new SynthDevKit::CV(0.1f);

  void shift_buttons_right(int button_value[NR_OF_BANDS][NR_OF_BANDS], int p_cnt[NR_OF_BANDS], bool led_state[1024], int *matrix_shift_position) {
    matrix_shift_buttons_right(button_value, p_cnt);
#ifdef DEBUG
    print_matrix(button_value, p_cnt);
#endif
    // Refresh the visible matrix.
    refresh_led_matrix(lights_offset, p_cnt, button_value, led_state);
    *matrix_shift_position += 1;
    if (*matrix_shift_position >= NR_OF_BANDS) 
        *matrix_shift_position = 0;
  }

  void shift_buttons_left(int button_value[NR_OF_BANDS][NR_OF_BANDS], int p_cnt[NR_OF_BANDS], bool led_state[1024], int *matrix_shift_position) {
    matrix_shift_buttons_left(button_value, p_cnt);
#ifdef DEBUG
    print_matrix(button_value, p_cnt);
#endif
    // Refresh the visible matrix.
    refresh_led_matrix(lights_offset, p_cnt, button_value, led_state);
    *matrix_shift_position -= 1;
    if (*matrix_shift_position < 0) 
        *matrix_shift_position = NR_OF_BANDS - 1;
  }

  void refresh_led_matrix(int lights_offset, int p_cnt[NR_OF_BANDS], int button_value[NR_OF_BANDS][NR_OF_BANDS], bool led_state[1024])
  {
     for (int i = 0; i < NR_OF_BANDS; i++)     
     {
        for (int j = 0; j < NR_OF_BANDS; j++)
        {
            led_state[i * NR_OF_BANDS + j] = false;
            lights[lights_offset + i * NR_OF_BANDS + j].value = false;
        }
        for (int j = 0; j < p_cnt[i]; j++)
        {
            led_state[i * NR_OF_BANDS + button_value[i][j]] = true;
            lights[lights_offset + i * NR_OF_BANDS + button_value[i][j]].value = true;
        }
     }
  }

  enum ParamIds {
    // aplification factor
    DEBUG_PARAM,
    // bypass switch
    BYPASS_SWITCH,
    // toggle button to choose matrix type
    MATRIX_TYPE_TOGGLE_PARAM,
    // switch to start shift of matrix (to the right)
    MATRIX_HOLD_TOGGLE_PARAM,
    MATRIX_ONE_STEP_RIGHT_PARAM,
    MATRIX_ONE_STEP_LEFT_PARAM,
    CARRIER_GAIN_PARAM,
    MODULATOR_GAIN_PARAM,
    PANNING_PARAM,
    MOD_MATRIX_PARAM,
    NUM_PARAMS = MOD_MATRIX_PARAM + NR_OF_BANDS * NR_OF_BANDS
  };

  enum InputIds {
    // input signal
    CARR_INPUT,
    MOD_INPUT, 
    SHIFT_RIGHT_INPUT,
    SHIFT_LEFT_INPUT,
    NUM_INPUTS
  };

  enum OutputIds {
    // output signal
    LEFT_OUTPUT,
    RIGHT_OUTPUT,
    NUM_OUTPUTS
  };

  enum LightIds {
    // bypass light.
    BYPASS_LIGHT,
    // matrix type light (toggles when pressed)
    MATRIX_TYPE_TOGGLE_LIGHT, 
    // matrix shift indicator light
    MATRIX_HOLD_TOGGLE_LIGHT,
    // Step toggle to set shift by hand
    MATRIX_ONE_STEP_RIGHT_LIGHT,
    MATRIX_ONE_STEP_LEFT_LIGHT,
    MOD_MATRIX,
    NUM_LIGHTS = MOD_MATRIX + NR_OF_BANDS * NR_OF_BANDS
  };

  float blinkPhase = -1.0f;
  float oneStepBlinkPhase = 0.0f;

  void step() override;

  // For more advanced Module features, read Rack's engine.hpp header file
  // - toJson, fromJson: serialization of internal data
  // - onSampleRateChange: event triggered by a change of sample rate
  // - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

  void onReset() override;
  void onRandomize() override;

  float smoothing_factor,   
        ym[NR_OF_BANDS][3],  // filter taps (y = output, x = input )
        xm[3] = {0.0,  0.0,  0.0};
  float yc[NR_OF_BANDS][3],  // filter taps (y = output, x = input )
        xc[3] = {0.0,  0.0,  0.0};
  float xm_env = 0.0, 
        ym_env[NR_OF_BANDS][2];

  int freq[33] = {0, 20, 25, 32, 40, 50, 63, 80, 100, 125, 160, 200, 250,
               315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500,
               3150, 4000, 5000, 6300, 8000, 10000,      
               12500, 16000, 20000, 22025};
  int i = 0;
  double carr_bandwidth = INITIAL_CARR_BW_IN_SEMITONES;
  double mod_bandwidth = INITIAL_MOD_BW_IN_SEMITONES;
  double fsamp = FFSAMP;

  // Button for bypass on and off.
  SchmittTrigger bypass_button_trig;
  bool fx_bypass = false;
  // Button to toggle the filter band coupling type (4 * log)
  SchmittTrigger matrix_mode_button_trig;
  bool matrix_mode_button_pressed = false;
  // Start with linear coupling of filters.
  int matrix_mode_selector = INITIAL_MATRIX_TYPE;
  int matrix_mode = matrix_mode_selector;

  // What is the shift position of the matrix.
  int matrix_shift_position = 1;

  SchmittTrigger matrix_hold_button_trig;
  bool matrix_hold_button_pressed = false;
  SchmittTrigger matrix_one_step_right_button_trig;
  bool matrix_one_step_right_button_pressed = false;
  SchmittTrigger matrix_one_step_left_button_trig;
  bool matrix_one_step_left_button_pressed = false;
  SchmittTrigger edit_matrix_trigger;
  bool edit_matrix_state = false;

  int wait = 1;
  int p_cnt[NR_OF_BANDS];
  int button_value[NR_OF_BANDS][NR_OF_BANDS];
  float mod_alpha1[NR_OF_BANDS];
  float mod_alpha2[NR_OF_BANDS];
  float mod_beta[NR_OF_BANDS];
  float carr_alpha1[NR_OF_BANDS];
  float carr_alpha2[NR_OF_BANDS];
  float carr_beta[NR_OF_BANDS];
  float left_pan[NR_OF_BANDS];
  float right_pan[NR_OF_BANDS];
  float left_level[NR_OF_BANDS];
  float right_level[NR_OF_BANDS];
  float start_level[NR_OF_BANDS];
  float envelope_attack_time[NR_OF_BANDS], envelope_release_time[NR_OF_BANDS]; 
  float envelope_attack_factor[NR_OF_BANDS], envelope_release_factor[NR_OF_BANDS]; 
  float width = 1.0;
  float width_old = width;
  bool led_state[1024] = {};
  bool matrix_mode_read_from_settings = false;
  int lights_offset = MOD_MATRIX;

  // Some code to read/save state of bypass button.
  json_t *toJson() override {
    json_t *rootJm = json_object();

    // Store bypass setting
    json_t *bypassJ = json_boolean(fx_bypass);
    json_object_set_new(rootJm, "fx_bypass", bypassJ);

    // Store setting of matrix_shift_position
    json_t *matrix_shift_positionJ = json_real(matrix_shift_position);
    json_object_set_new(rootJm, "matrix_shift_position", matrix_shift_positionJ);

    // Store setting of matrix_mode
    json_t *matrix_modeJ = json_real(matrix_mode);
    json_object_set_new(rootJm, "matrix_mode", matrix_modeJ);
    
    // Store matrix hold button status
    json_t *matrix_hold_button_pressedJ = json_boolean(matrix_hold_button_pressed);
    json_object_set_new(rootJm, "matrix_hold_button_pressed", matrix_hold_button_pressedJ);

    // Store p_cnt
    json_t *p_cntJ = json_array();
    for (int i = 0; i < NR_OF_BANDS; i++) {
       json_array_append_new(p_cntJ, json_real(p_cnt[i]));
    }
    json_object_set_new(rootJm, "p_cnt", p_cntJ);

    // Store matrix button values to patch settings.
    int cnt = 0;
   	json_t *button_valuesJ = json_array();
	for (int i = 0; i < NR_OF_BANDS; i++) {
		for (int j = 0; j < p_cnt[i]; j++) {
			json_array_append_new(button_valuesJ, json_real(button_value[i][j]));
            cnt++;
		}
    }
	json_object_set_new(rootJm, "button_values", button_valuesJ);
    printf("geschreven: %d\n", cnt);

    return rootJm;
  }       
          
  void fromJson(json_t *rootJm) override {
    // Restore bypass state 
    json_t *bypassJ = json_object_get(rootJm, "fx_bypass");
    if (bypassJ) {
        fx_bypass = json_boolean_value(bypassJ);
    }

    // Restore matrix shift position
    json_t *matrix_shift_positionJ = json_object_get(rootJm, "matrix_shift_position");
    if (matrix_shift_positionJ) {
        matrix_shift_position = (int) json_real_value(matrix_shift_positionJ);
    }

    // Restore matrix type 
    json_t *matrix_modeJ = json_object_get(rootJm, "matrix_mode");
    if (matrix_modeJ) {
        matrix_mode = (int) json_real_value(matrix_modeJ);
    }

    // Restore matrix_hold_button_pressed button status
    json_t *matrix_hold_button_pressedJ = json_object_get(rootJm, "matrix_hold_button_pressed");
    if (matrix_hold_button_pressedJ) {
        matrix_hold_button_pressed = json_boolean_value(matrix_hold_button_pressedJ);
    }

    // Restore p_cnt
    json_t *p_cntJ = json_object_get(rootJm, "p_cnt");
    if (p_cntJ) {
        for (int i = 0; i < NR_OF_BANDS; i++) {
            json_t *elementJ = json_array_get(p_cntJ, i);
            if (elementJ) {
                p_cnt[i] = (int) json_real_value(elementJ);
            }
        }
    }

    // Restore button_values
    int cnt = 0;
    json_t *button_valuesJ = json_object_get(rootJm, "button_values");
    if (button_valuesJ) {
        int index = 0;
        for (int i = 0; i < NR_OF_BANDS; i++) {
            for (int j = 0; j < p_cnt[i]; j++) {
                json_t *elementJ = json_array_get(button_valuesJ, index + j);
                if (elementJ) {
                    button_value[i][j] = (int) json_real_value(elementJ);
                    cnt++;
                    printf("gelezen: %d %d %d\n", index, j, p_cnt[i]);
                } else {
                    printf("niet gelezen: %d %d %d\n", index, j, p_cnt[i]);
                }
            }
            index += p_cnt[i];
        }
        matrix_mode_read_from_settings = true;
        refresh_led_matrix(lights_offset, p_cnt, button_value, led_state);
    }
    printf("gelezen: %d\n", cnt);
  } 

  Vocode_O_Matic() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {

    // Initialize the filter coefficients.
    comp_all_coeffs(freq, mod_bandwidth, fsamp, mod_alpha1, mod_alpha2, mod_beta);
    comp_all_coeffs(freq, carr_bandwidth, fsamp, carr_alpha1, carr_alpha2, carr_beta);
  
    // Initialize all filter taps. 
    for (int i = 0; i < NR_OF_BANDS; i++) {
     for (int j = 0; j < 3; j++) { 
          ym[i][j] = 0.0;
      }
      ym_env[i][0] = 0.0; // Envelope of modulator.
      ym_env[i][1] = 0.0;
    }
    // Initialize the levels and pans.
    initialize_start_levels(start_level);
    init_pan_and_level(start_level, left_pan, right_pan, left_level, right_level);
    if (!matrix_mode_read_from_settings) {
        printf("NOT NOT\n");
        choose_matrix(4, button_value, p_cnt); // Initialize linear filter coupling.
    }

    // Refresh LED matrix.
    refresh_led_matrix(lights_offset, p_cnt, button_value, led_state);

    blinkPhase = -1.0f;
    // Reset lights.
    lights[MATRIX_HOLD_TOGGLE_LIGHT].value = 0.0;
    lights[BYPASS_LIGHT].value = 0.0;
  
    comp_attack_times(envelope_attack_time); 
    comp_attack_factors(envelope_attack_factor, envelope_attack_time);
  
    comp_release_times(envelope_release_time); 
    comp_release_factors(envelope_release_factor, envelope_release_time);
  } 
};

struct LButton : SVGSwitch, MomentarySwitch {
    LButton() {
        addFrame(SVG::load(assetPlugin(plugin, "res/L.svg")));
        addFrame(SVG::load(assetPlugin(plugin, "res/Ldown.svg")));
        sw->wrap();
        box.size = sw->box.size;
    }

    void onMouseDown(EventMouseDown &e) override {
       //if ( rack::windowIsShiftPressed() ) { 
           //printf("shift pressed\n");
        //}
#ifdef DEBUG
       if (e.button == 1) {
            printf("Button pressed down\n");
       } else {
            printf("Button value %d\n", e.button);
       }
#endif
       SVGSwitch::onMouseDown(e);
    }

    void onHoverKey(EventHoverKey &e) override {
        printf("Hovering!\n");
        EventMouseDown emd;
        emd.button = 1;
        SVGSwitch::onMouseDown(emd);
        SVGSwitch::onHoverKey(e);
    }
    
    void onChange(EventChange &e) override {
      // Do your own thang.
      //printf("Button change detected!\n");
      // Pass event to super class.
      SVGSwitch::onChange(e);
    }
};

void Vocode_O_Matic::onReset() {
  matrix_mode_button_pressed = false;
  matrix_hold_button_pressed = false;
  matrix_one_step_right_button_pressed = false;
  matrix_one_step_left_button_pressed = false;
  matrix_mode_selector = INITIAL_MATRIX_TYPE;
  matrix_mode = matrix_mode_selector;
  matrix_shift_position = 0;
  choose_matrix(4, button_value, p_cnt); // Initialize linear filter coupling.

  // Refresh LED matrix.
  refresh_led_matrix(lights_offset, p_cnt, button_value, led_state);
  blinkPhase = -1.0f;
  oneStepBlinkPhase = 0.0f;

  // Reset lights.
  lights[MATRIX_HOLD_TOGGLE_LIGHT].value = 0.0;
  lights[BYPASS_LIGHT].value = 0.0;
  params[CARRIER_GAIN_PARAM].value = INITIAL_CARRIER_GAIN;
  params[MODULATOR_GAIN_PARAM].value = INITIAL_MODULATOR_GAIN;
}

void Vocode_O_Matic::onRandomize() {
  int cnt = 3;
  clear_matrix(button_value, p_cnt);
  for (int i = 0; i < NR_OF_BANDS; i++) {
      for (int j = 0; j < cnt; j++) {
          //int jj = (int) ((float) NR_OF_BANDS * rand() / (RAND_MAX + 1.0));
          int jj = i / 2 + (int) ((float) NR_OF_BANDS / 2.0 * rand() / (RAND_MAX + 1.0));
          button_value[i][p_cnt[i]++] = jj;
      }
  }
  refresh_led_matrix(lights_offset, p_cnt, button_value, led_state);
  params[CARRIER_GAIN_PARAM].value = INITIAL_CARRIER_GAIN;
  params[MODULATOR_GAIN_PARAM].value = INITIAL_MODULATOR_GAIN;
}

void Vocode_O_Matic::step() {
  // Implement the Vocoder.

  float deltaTime = engineGetSampleTime();
  float oneStepDeltaTime = engineGetSampleTime();
             
  xc[0] = inputs[CARR_INPUT].value * params[CARRIER_GAIN_PARAM].value;
  xm[0] = inputs[MOD_INPUT].value * params[MODULATOR_GAIN_PARAM].value;
  float smoothing_factor = 1.0;

  for (int i = 0; i < NR_OF_BANDS; i++) {
    //
    // CARRIER
    //  
    // Direct Form I topology is used for filtering. Direct Form II would in this case cost as many multiplications and shifts.
    yc[i][0] = carr_alpha1[i] * (xc[0] - xc[2] - carr_alpha2[i] * yc[i][1] - carr_beta[i] * yc[i][2]);
    //  
    // Shift all carrier filter taps.
    yc[i][2] = yc[i][1]; yc[i][1] = yc[i][0];

    //
    // MODULATOR
    //
    ym[i][0] = mod_alpha1[i] * (xm[0] - xm[2] - mod_alpha2[i] * ym[i][1] - mod_beta[i] * ym[i][2]);
    // 
    // Shift filter taps.
    ym[i][2] = ym[i][1]; ym[i][1] = ym[i][0];
    //  
    // Compute input for envelope for this band.
    // Use only positive values so that energy levels are zero or positive.
    xm_env = fabs(ym[i][0]);
    //
    // Perform AR averager on this band's output.
    if (ym_env[i][1] < xm_env)
        smoothing_factor = envelope_attack_factor[i];
    else
        smoothing_factor = envelope_release_factor[i];
    ym_env[i][0] = (1.0 - smoothing_factor) * xm_env + smoothing_factor * ym_env[i][1];
    //
    // Shift low pass filter taps.
    ym_env[i][1] = ym_env[i][0];
  }

  // Shift modulator input taps.
  xm[2] = xm[1]; xm[1] = xm[0];
  // 
  // Shift carrier input taps.
  xc[2] = xc[1]; xc[1] = xc[0];

  // Blink light at 2Hz.
  blinkPhase += deltaTime;
  if (blinkPhase >= 1.0f) 
    blinkPhase -= 1.0f;
  oneStepBlinkPhase += oneStepDeltaTime;
  if (oneStepBlinkPhase >= 0.1f) { // light will be on for a very short time.
    lights[MATRIX_ONE_STEP_RIGHT_LIGHT].value = 0.0f;
    lights[MATRIX_ONE_STEP_LEFT_LIGHT].value = 0.0f;
  }

  // Process trigger signal on matrix shift input. 
  float shiftRightTriggerIn = inputs[SHIFT_RIGHT_INPUT].value;
  cv_right->update(shiftRightTriggerIn);
  float shiftLeftTriggerIn = inputs[SHIFT_LEFT_INPUT].value;
  cv_left->update(shiftLeftTriggerIn);

  // Handling of buttons and / or lights.
  // If bypass button was pressed:
  if (bypass_button_trig.process(params[BYPASS_SWITCH].value))
  {
    fx_bypass = !fx_bypass;
  }
  lights[BYPASS_LIGHT].value = fx_bypass ? 1.00 : 0.0;
  if (matrix_hold_button_pressed) { // We blink only if the button is toggled in the on position.
    lights[MATRIX_HOLD_TOGGLE_LIGHT].value = (blinkPhase < 0.5f) ? 1.0f : 0.0f;
  }

  // Hold matrix movement if toggle is pressed.
  if (matrix_hold_button_trig.process(params[MATRIX_HOLD_TOGGLE_PARAM].value))
  {
    matrix_hold_button_pressed = !matrix_hold_button_pressed;
    lights[MATRIX_HOLD_TOGGLE_LIGHT].value = matrix_hold_button_pressed ? 1.00 : 0.0;
  }

  // If one step right button was pressed:
  if (matrix_one_step_right_button_trig.process(params[MATRIX_ONE_STEP_RIGHT_PARAM].value))
  {
    // Start a new blink period.
    oneStepBlinkPhase = 0.0f;
    // Light up the button;
    lights[MATRIX_ONE_STEP_RIGHT_LIGHT].value = 1.00;
    // Shift the buttons one step.
    shift_buttons_right(button_value, p_cnt, led_state, &matrix_shift_position);
  }

  // Shift the matrix if there is a new trigger on the shift right input and the 
  // hold button is not pressed and we are not in bypass mode.
  if (not fx_bypass && cv_right->newTrigger() and not matrix_hold_button_pressed) {
    // Shift the buttons one step.
    shift_buttons_right(button_value, p_cnt, led_state, &matrix_shift_position);
  }

  // If one step left button was pressed:
  if (matrix_one_step_left_button_trig.process(params[MATRIX_ONE_STEP_LEFT_PARAM].value))
  {
    // Start a new blink period.
    oneStepBlinkPhase = 0.0f;
    // Light up the button;
    lights[MATRIX_ONE_STEP_LEFT_LIGHT].value = 1.00;
    // Shift the buttons one step.
    shift_buttons_left(button_value, p_cnt, led_state, &matrix_shift_position);
  }

  // Shift the matrix if there is a new trigger on the shift left input and the 
  // hold button is not pressed and we are not in bypass mode.
  if (not fx_bypass && cv_left->newTrigger() and not matrix_hold_button_pressed) {
    // Shift the buttons one step.
    shift_buttons_left(button_value, p_cnt, led_state, &matrix_shift_position);
  }

  // If toggle matrix preset button was pressed.
  if (matrix_mode_button_trig.process(params[MATRIX_TYPE_TOGGLE_PARAM].value))
  {
    matrix_mode_button_pressed = false;
    lights[MATRIX_TYPE_TOGGLE_PARAM].value = matrix_mode_button_pressed ? 1.00 : 0.0;
    matrix_mode_selector++; 
    if (matrix_mode_selector > NR_MATRIX_TYPES - 1) { matrix_mode_selector = 0; }
    choose_matrix(matrix_mode_selector, button_value, p_cnt);

    // Refresh LED matrix.
    refresh_led_matrix(lights_offset, p_cnt, button_value, led_state);

    matrix_mode = matrix_mode_selector;

    // Restart the shift counter at 0.
    matrix_shift_position = 0;
  }

  // Panning
#ifdef PANNING
  width = params[PANNING_PARAM].value;
  if (width != width_old) 
  {
    set_pan_and_level(start_level, left_pan, right_pan, left_level, right_level, width);
    width_old = width;
//#ifdef DEBUG
//    for (int i = 0; i < NR_OF_BANDS; i++) {
//      printf("%f %f\n", left_level[i], right_level[i]);
//    }
//#endif
  }
#endif

  // If a matrix button is pressed, adapt the matrix.
  if (edit_matrix_trigger.process(params[MOD_MATRIX_PARAM].value)) 
  {
    edit_matrix_state = !edit_matrix_state ;
  }

  if (wait == 0) 
  {
    for (int i = 0; i < NR_OF_BANDS * NR_OF_BANDS; i++) 
    {
        if (params[MOD_MATRIX_PARAM + i].value) 
        {
            wait = 20000;
            led_state[i] = !led_state[i]; 
            int index = MOD_MATRIX + i;
            lights[index].value = !lights[index].value;
            int chosen_row = i / NR_OF_BANDS;
            int chosen_col = i % NR_OF_BANDS;
            if ((p_cnt[chosen_row] > 0) && (!led_state[i]))
            {
                for (int col = 0; col < NR_OF_BANDS; col++) 
                { // Find the right column.
                    if (button_value[chosen_row][col] == chosen_col)
                    {
                        button_value[chosen_row][col] = NOT_PRESSED;
                         // Shift all values from unpressed button 1 position to the left.
                        for (int shift_col = col; shift_col < p_cnt[chosen_row]; shift_col++) {
                             button_value[chosen_row][shift_col] = button_value[chosen_row][shift_col + 1];
                        }
                        p_cnt[chosen_row]--;
                        break; // As soon as one button is unpressed we are done.
                    }
                }
            } 
            else 
            {
                button_value[chosen_row][p_cnt[chosen_row]] = chosen_col;
                p_cnt[chosen_row]++;
            }
        }
    }
  } else wait = wait - 1;


  if (fx_bypass) {
    outputs[LEFT_OUTPUT].value = inputs[CARR_INPUT].value * params[CARRIER_GAIN_PARAM].value;
    outputs[RIGHT_OUTPUT].value = inputs[MOD_INPUT].value * params[MODULATOR_GAIN_PARAM].value;
  } 
  else 
  {
    // Initialize output signal.
    outputs[RIGHT_OUTPUT].value = 0.0;
    outputs[LEFT_OUTPUT].value = 0.0;
    // The output is the sum of all carrier band signals multiplied by all envelope outputs for that band.

    for (int i = NR_OF_BANDS -1; i >= 0; i--)
    {  
      for (int j = 0; j < p_cnt[i]; j++)
      {
        int ind = button_value[i][j];
        // Use fl_tmp0 and fl_tmp to speed things up a bit.
        float fl_tmp0 = yc[ind][0] * ym_env[i][0];
        //
        // Left channel.
        float fl_tmp = fl_tmp0 * left_level[i];
        // Compute output value of signal ( superposition )
        outputs[LEFT_OUTPUT].value += fl_tmp;
        //
        // Right channel.
        fl_tmp = fl_tmp0 * right_level[i];
        // Compute output value of signal ( superposition )
        outputs[RIGHT_OUTPUT].value += fl_tmp;
      }
    }
  }
}

struct Vocode_O_MaticWidget : ModuleWidget {
  Vocode_O_MaticWidget(Vocode_O_Matic *module) : ModuleWidget(module) {
    setPanel(SVG::load(assetPlugin(plugin, "res/Sculpt-O-Sound-_-Vocode_O_Matic_v0.4.svg")));

    addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // Dial for carrier gain.
    // Dial for modulator gain.
    // Dial for panning.
    // Note: format is Vec(x-pos, y-pos)
    addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(10,  25), module, Vocode_O_Matic::CARRIER_GAIN_PARAM, 1.0, 10.0, INITIAL_CARRIER_GAIN));
    addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(40,  25), module, Vocode_O_Matic::MODULATOR_GAIN_PARAM, 1.0, 10.0, INITIAL_MODULATOR_GAIN));
#ifdef PANNING
    //addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(70,  47), module, Vocode_O_Matic::PANNING_PARAM, 0.0, MAX_PAN, 1.0 / INITIAL_PAN_OFFSET));
    addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(70,  25), module, Vocode_O_Matic::PANNING_PARAM, 0.0, MAX_PAN, MAX_PAN / 2.0));
#endif

    // INTPUTS (SIGNAL AND PARAMS)
    // Input signals.
    addInput(Port::create<PJ301MPort>(Vec(10, 180), Port::INPUT, module, Vocode_O_Matic::CARR_INPUT));
    addInput(Port::create<PJ301MPort>(Vec(42, 180), Port::INPUT, module, Vocode_O_Matic::MOD_INPUT));
    addInput(Port::create<PJ301MPort>(Vec(105, 140), Port::INPUT, module, Vocode_O_Matic::SHIFT_RIGHT_INPUT));
    addInput(Port::create<PJ301MPort>(Vec(105, 103), Port::INPUT, module, Vocode_O_Matic::SHIFT_LEFT_INPUT));

    // Bypass switch.
    addParam(ParamWidget::create<LEDBezel>(Vec(12,  66), module, Vocode_O_Matic::BYPASS_SWITCH , 0.0f, 1.0f, 0.0f));
    addChild(ModuleLightWidget::create<LedLight<RedLight>>(Vec(14.2, 68), module, Vocode_O_Matic::BYPASS_LIGHT));

    // Matrix type switch: linear, inverse + 4 * log
    addParam(ParamWidget::create<LEDBezel>(Vec(12, 104), module, Vocode_O_Matic::MATRIX_TYPE_TOGGLE_PARAM, 0.0f, 1.0f, 0.0f));
    addChild(ModuleLightWidget::create<LedLight<GreenLight>>(Vec(14.2, 106), module, Vocode_O_Matic::MATRIX_TYPE_TOGGLE_LIGHT));

    // Push button which shifts the matrix to the right one step at a time.
    addParam(ParamWidget::create<LEDBezel>(Vec(76, 142), module, Vocode_O_Matic::MATRIX_ONE_STEP_RIGHT_PARAM, 0.0f, 1.0f, 0.0f));
    addChild(ModuleLightWidget::create<LedLight<GreenLight>>(Vec(78.2, 144), module, Vocode_O_Matic::MATRIX_ONE_STEP_RIGHT_LIGHT));

    // Push button which shifts the matrix to the left one step at a time.
    addParam(ParamWidget::create<LEDBezel>(Vec(76, 104), module, Vocode_O_Matic::MATRIX_ONE_STEP_LEFT_PARAM, 0.0f, 1.0f, 0.0f));
    addChild(ModuleLightWidget::create<LedLight<GreenLight>>(Vec(78.2, 106), module, Vocode_O_Matic::MATRIX_ONE_STEP_LEFT_LIGHT));

    // Matrix hold toggle.
    addParam(ParamWidget::create<LEDBezel>(Vec(12, 142), module, Vocode_O_Matic::MATRIX_HOLD_TOGGLE_PARAM, 0.0f, 1.0f, 0.0f));
    addChild(ModuleLightWidget::create<LedLight<RedLight>>(Vec(14.2, 144), module, Vocode_O_Matic::MATRIX_HOLD_TOGGLE_LIGHT));

    // MS DISPLAY                                                                  
    // Matrix type
    MsDisplayWidget2 *display1 = new MsDisplayWidget2();                            
    display1->box.pos = Vec(38, 105);                                               
    display1->box.size = Vec(30, 20);                                             
    display1->value = &module->matrix_mode;
    addChild(display1);                                                           

    // MS DISPLAY                                                                  
    // Shift position
    MsDisplayWidget2 *display2 = new MsDisplayWidget2();                            
    display2->box.pos = Vec(38, 143);                                               
    display2->box.size = Vec(30, 20);                                             
    display2->value = &module->matrix_shift_position;
    addChild(display2);                                                           

    // Output for filtered signal.
    addOutput(Port::create<PJ301MPort>(Vec(10, 219), Port::OUTPUT, module, Vocode_O_Matic::LEFT_OUTPUT));
    addOutput(Port::create<PJ301MPort>(Vec(42, 219), Port::OUTPUT, module, Vocode_O_Matic::RIGHT_OUTPUT));

    // Matrix, origin is bottom left.
    for (int i = 0; i < NR_OF_BANDS; i++) {
        for (int j = 0; j < NR_OF_BANDS; j++) {
            int x = HBASE + j * LED_WIDTH;
            int y = VBASE - i * (LED_HEIGHT + 1);
            int offset = i * NR_OF_BANDS + j;
            addParam(ParamWidget::create<LButton>(Vec(x, y), module, Vocode_O_Matic::MOD_MATRIX_PARAM + offset, 0.0, 1.0f, 0.0f));
            addChild(ModuleLightWidget::create<MediumLight<BlueLight>>(Vec(x, y), module, Vocode_O_Matic::MOD_MATRIX + offset));
        }
    }

  };
};
// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelVocode_O_Matic = Model::create<Vocode_O_Matic, Vocode_O_MaticWidget>("Sculpt-O-Sound", "Vocode_O_Matic", "Vocode_O_Matic", VOCODER_TAG);