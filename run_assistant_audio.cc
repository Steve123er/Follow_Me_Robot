/*
Copyright 2017 Google Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <grpc++/grpc++.h>

#include <getopt.h>

#include <chrono>  // NOLINT
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>  // NOLINT

#ifdef __linux__
#define ENABLE_ALSA
#endif

#ifdef ENABLE_ALSA
#include "assistant/audio_input_alsa.h"
#include "assistant/audio_output_alsa.h"
#endif

#include "google/assistant/embedded/v1alpha2/embedded_assistant.grpc.pb.h"
#include "google/assistant/embedded/v1alpha2/embedded_assistant.pb.h"

#include "assistant/assistant_config.h"
#include "assistant/audio_input.h"
#include "assistant/audio_input_file.h"
#include "assistant/base64_encode.h"
#include "assistant/json_util.h"

// MATRIX GLOBALS //
#include "assistant/robot_movement.h"
//// System calls
//#include <unistd.h>
// Interfaces with GPIO
#include "driver/gpio_control.h"
// Interfaces with IMU sensor
#include "driver/imu_sensor.h"
// Holds data from IMU sensor
#include "driver/imu_data.h"
// Communicates with MATRIX device
#include "driver/matrixio_bus.h"
// Interfaces with Everloop
#include "driver/everloop.h"
// Holds data for Everloop
#include "driver/everloop_image.h"
// END MATRIX GLOBALS // 

// DOA GLOBALS
//#include <fftw3.h>
//#include <stdint.h>
//#include <string.h>
//#include <wiringPi.h>

//#include <valarray>

//#include "driver/direction_of_arrival.h"
//#include "driver/everloop.h"
//#include "driver/everloop_image.h"
//#include "driver/microphone_array.h"
//#include "driver/microphone_core.h"

//DEFINE_bool(big_menu, true, "Include 'advanced' options in the menu listing");
//DEFINE_int32(sampling_frequency, 16000, "Sampling Frequency");

//int led_offset[] = {23, 27, 32, 1, 6, 10, 14, 19};
//int lut[] = {1, 2, 10, 200, 10, 2, 1};
// END DOA GLOBALS

namespace assistant = google::assistant::embedded::v1alpha2;

using assistant::AssistRequest;
using assistant::AssistResponse;
using assistant::AssistResponse_EventType_END_OF_UTTERANCE;
using assistant::AudioInConfig;
using assistant::AudioOutConfig;
using assistant::EmbeddedAssistant;
using assistant::ScreenOutConfig;

using grpc::CallCredentials;
using grpc::Channel;
using grpc::ClientReaderWriter;

static const char kCredentialsTypeUserAccount[] = "USER_ACCOUNT";
static const char kALSAAudioInput[] = "ALSA_INPUT";
static const char kLanguageCode[] = "en-US";
static const char kDeviceModelId[] = "default";
static const char kDeviceInstanceId[] = "default";

bool verbose = false;

// Creates a channel to be connected to Google.
std::shared_ptr<Channel> CreateChannel(const std::string& host) {
  std::ifstream file("robots.pem");
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string roots_pem = buffer.str();

  if (verbose) {
    std::clog << "assistant_sdk robots_pem: " << roots_pem << std::endl;
  }
  ::grpc::SslCredentialsOptions ssl_opts = {roots_pem, "", ""};
  auto creds = ::grpc::SslCredentials(ssl_opts);
  std::string server = host + ":443";
  if (verbose) {
    std::clog << "assistant_sdk CreateCustomChannel(" << server
              << ", creds, arg)" << std::endl
              << std::endl;
  }
  ::grpc::ChannelArguments channel_args;
  return CreateCustomChannel(server, creds, channel_args);
}

void PrintUsage() {
  std::cerr << "Usage: ./run_assistant_audio "
            << "--credentials <credentials_file> "
            << "[--api_endpoint <API endpoint>] "
            << "[--locale <locale>]"
            << "[--html_out <command to load HTML page>]" << std::endl;
}

bool GetCommandLineFlags(int argc, char** argv,
                         std::string* credentials_file_path,
                         std::string* api_endpoint, std::string* locale,
                         std::string* html_out_command) {
  const struct option long_options[] = {
      {"credentials", required_argument, nullptr, 'c'},
      {"api_endpoint", required_argument, nullptr, 'e'},
      {"locale", required_argument, nullptr, 'l'},
      {"verbose", no_argument, nullptr, 'v'},
      {"html_out", required_argument, nullptr, 'h'},
      {nullptr, 0, nullptr, 0}};
  *api_endpoint = ASSISTANT_ENDPOINT;
  while (true) {
    int option_index;
    int option_char =
        getopt_long(argc, argv, "c:e:l:v:h", long_options, &option_index);
    if (option_char == -1) {
      break;
    }
    switch (option_char) {
      case 'c':
        *credentials_file_path = optarg;
        break;
      case 'e':
        *api_endpoint = optarg;
        break;
      case 'l':
        *locale = optarg;
        break;
      case 'v':
        verbose = true;
        break;
      case 'h':
        *html_out_command = optarg;
        break;
      default:
        PrintUsage();
        return false;
    }
  }
  return true;
}

int main(int argc, char** argv) {
  std::string credentials_file_path, api_endpoint, locale, html_out_command;
#ifndef ENABLE_ALSA
  std::cerr << "ALSA audio input is not supported on this platform."
            << std::endl;
  return -1;
#endif

  // Initialize gRPC and DNS resolvers
  // https://github.com/grpc/grpc/issues/11366#issuecomment-328595941
  grpc_init();
  if (!GetCommandLineFlags(argc, argv, &credentials_file_path, &api_endpoint,
                           &locale, &html_out_command)) {
    return -1;
  }
  
  // MATRIX INITIALIZATIONS //
  // Create MatrixIOBus object for hardware communication
	matrix_hal::MatrixIOBus bus;
	// Initialize bus and exit program if error occurs
	if (!bus.Init()) return false;

	// Create IMUData object
	matrix_hal::IMUData imu_data;
	// Create IMUSensor object
	matrix_hal::IMUSensor imu_sensor;
	// Set imu_sensor to use MatrixIOBus bus
	imu_sensor.Setup(&bus);
	
	// Create GPIOControl object
	matrix_hal::GPIOControl gpio;
	// Set gpio to use MatrixIOBus bus
	gpio.Setup(&bus);
  gpioInit(&gpio);
  
  // Holds the number of LEDs on MATRIX device
  int ledCount = bus.MatrixLeds();
  // Create EverloopImage object, with size of ledCount
  matrix_hal::EverloopImage everloop_image(ledCount);
  // Create Everloop object
  matrix_hal::Everloop everloop;
  // Set everloop to use MatrixIOBus bus
  everloop.Setup(&bus);
  // led brightness
  int ledBright = 50;
  // END MATRIX INITIALIZATIONS //
  
  // DOA INTIALIZATIONS
  //if (!bus.IsDirectBus()) {
    //std::cerr << "Kernel Modules has been loaded. Use ALSA examples "
              //<< std::endl;
  //}

  //int sampling_rate = 16000;

  //matrix_hal::Everloop everloop;
  //everloop.Setup(&bus);

  //matrix_hal::EverloopImage image1d(bus.MatrixLeds());

  //matrix_hal::MicrophoneArray mics;
  //mics.Setup(&bus);
  //mics.SetSamplingRate(sampling_rate);
  //mics.ShowConfiguration();

  //matrix_hal::MicrophoneCore mic_core(mics);
  //mic_core.Setup(&bus);

  //matrix_hal::DirectionOfArrival doa(mics);
  //doa.Init();

  //float azimutal_angle;
  //float polar_angle;
  //int mic;
  // END DOA INITIALIZATIONS

  while (true) {
    // DOA LOOP CODE
    //mics.Read(); /* Reading 8-mics buffer from de FPGA */

    //doa.Calculate();

    //azimutal_angle = doa.GetAzimutalAngle() * 180 / M_PI;
    //polar_angle = doa.GetPolarAngle() * 180 / M_PI;
    //mic = doa.GetNearestMicrophone();

    //std::cout << "azimutal angle = " << azimutal_angle
              //<< ", polar angle = " << polar_angle << ", mic = " << mic
              //<< std::endl;

    //for (matrix_hal::LedValue &led : image1d.leds) {
      //led.blue = 0;
    //}

    //for (int i = led_offset[mic] - 3, j = 0; i < led_offset[mic] + 3;
         //++i, ++j) {
      //if (i < 0) {
        //image1d.leds[image1d.leds.size() + i].blue = lut[j];
      //} else {
        //image1d.leds[i % image1d.leds.size()].blue = lut[j];
      //}

      //everloop.Write(&image1d);
    //}
    // END DOA LOOP CODE
/***************ADDED BY ME - NOT ORIGINAL GOOGLE ASSISTANT CODE******************/
    for (matrix_hal::LedValue &led : everloop_image.leds) {
      // Turn off Everloop
      led.red = 0;
      led.green = 0;
      led.blue = 0;
      led.white = 0;
    }
    everloop_image.leds[0].blue = ledBright;
    everloop_image.leds[5].blue = ledBright;
    everloop_image.leds[10].blue = ledBright;
    everloop_image.leds[15].blue = ledBright;
    everloop_image.leds[20].blue = ledBright;
    everloop_image.leds[25].blue = ledBright;
    everloop_image.leds[30].blue = ledBright;
    everloop.Write(&everloop_image);
    
    // Create an AssistRequest
    AssistRequest request;
    auto* assist_config = request.mutable_config();

    if (locale.empty()) {
      locale = kLanguageCode;  // Default locale
    }
    if (verbose) {
      std::clog << "Using locale " << locale << std::endl;
    }
    // Set the DialogStateIn of the AssistRequest
    assist_config->mutable_dialog_state_in()->set_language_code(locale);

    // Set the DeviceConfig of the AssistRequest
    assist_config->mutable_device_config()->set_device_id(kDeviceInstanceId);
    assist_config->mutable_device_config()->set_device_model_id(kDeviceModelId);

    // Set parameters for audio output
    assist_config->mutable_audio_out_config()->set_encoding(
        AudioOutConfig::LINEAR16);
    assist_config->mutable_audio_out_config()->set_sample_rate_hertz(16000);

    // Set parameters for screen config
    assist_config->mutable_screen_out_config()->set_screen_mode(
        html_out_command.empty() ? ScreenOutConfig::SCREEN_MODE_UNSPECIFIED
                                 : ScreenOutConfig::PLAYING);

    std::unique_ptr<AudioInput> audio_input;
    // Set the AudioInConfig of the AssistRequest
    assist_config->mutable_audio_in_config()->set_encoding(
        AudioInConfig::LINEAR16);
    assist_config->mutable_audio_in_config()->set_sample_rate_hertz(16000);

    // Read credentials file.
    std::ifstream credentials_file(credentials_file_path);
    if (!credentials_file) {
      std::cerr << "Credentials file \"" << credentials_file_path
                << "\" does not exist." << std::endl;
      return -1;
    }
    std::stringstream credentials_buffer;
    credentials_buffer << credentials_file.rdbuf();
    std::string credentials = credentials_buffer.str();
    std::shared_ptr<CallCredentials> call_credentials;
    call_credentials = grpc::GoogleRefreshTokenCredentials(credentials);
    if (call_credentials.get() == nullptr) {
      std::cerr << "Credentials file \"" << credentials_file_path
                << "\" is invalid. Check step 5 in README for how to get valid "
                << "credentials." << std::endl;
      return -1;
    }

    // Begin a stream.
    auto channel = CreateChannel(api_endpoint);
    std::unique_ptr<EmbeddedAssistant::Stub> assistant(
        EmbeddedAssistant::NewStub(channel));

    grpc::ClientContext context;
    context.set_fail_fast(false);
    context.set_credentials(call_credentials);

    std::shared_ptr<ClientReaderWriter<AssistRequest, AssistResponse>> stream(
        std::move(assistant->Assist(&context)));
    // Write config in first stream.
    if (verbose) {
      std::clog << "assistant_sdk wrote first request: "
                << request.ShortDebugString() << std::endl;
    }
    stream->Write(request);

    audio_input.reset(new AudioInputALSA());

    audio_input->AddDataListener(
        [stream, &request](std::shared_ptr<std::vector<unsigned char>> data) {
          request.set_audio_in(&((*data)[0]), data->size());
          stream->Write(request);
        });
    audio_input->AddStopListener([stream]() { stream->WritesDone(); });
    audio_input->Start();

    AudioOutputALSA audio_output;
    audio_output.Start();

    // Read responses.
    if (verbose) {
      std::clog << "assistant_sdk waiting for response ... " << std::endl;
    }
    AssistResponse response;
    while (stream->Read(&response)) {  // Returns false when no more to read.
      if (response.has_audio_out() ||
          response.event_type() == AssistResponse_EventType_END_OF_UTTERANCE) {
        // Synchronously stops audio input if there is one.
        if (audio_input != nullptr && audio_input->IsRunning()) {
          audio_input->Stop();
        }
      }
      if (response.has_audio_out()) {
        // CUSTOMIZE: play back audio_out here.

        std::shared_ptr<std::vector<unsigned char>> data(
            new std::vector<unsigned char>);
        data->resize(response.audio_out().audio_data().length());
        memcpy(&((*data)[0]), response.audio_out().audio_data().c_str(),
               response.audio_out().audio_data().length());
        audio_output.Send(data);
      }
      // CUSTOMIZE: render spoken request on screen
      for (int i = 0; i < response.speech_results_size(); i++) {
        auto result = response.speech_results(i);
        if (verbose) {
          std::clog << "assistant_sdk request: \n"
                    << result.transcript() << " ("
                    << std::to_string(result.stability()) << ")" << std::endl;
        }
        
        std::clog << "assistant_sdk request: \n"
                  << result.transcript() << " ("
                  << std::to_string(result.stability()) << ")" << std::endl;
        
/***************ADDED BY ME - NOT ORIGINAL GOOGLE ASSISTANT CODE******************/
        if (result.stability() > 0) {
          for (matrix_hal::LedValue &led : everloop_image.leds) {
              // Turn off Everloop
              led.red = 0;
              led.green = 0;
              led.blue = 0;
              led.white = 0;
          }
          everloop_image.leds[2].green = ledBright;
          everloop_image.leds[7].green = ledBright;
          everloop_image.leds[12].green = ledBright;
          everloop_image.leds[17].green = ledBright;
          everloop_image.leds[22].green = ledBright;
          everloop_image.leds[27].green = ledBright;
          everloop_image.leds[32].green = ledBright;
          everloop.Write(&everloop_image);
        }
        if (result.stability() == 1 && (result.transcript() == "come to me" ||
                                        result.transcript() == "follow me" ||
                                        result.transcript() == "go forward" ||
                                        result.transcript() == "go backward" ||
                                        result.transcript() == "turn right" ||
                                        result.transcript() == "turn left" ||
                                        result.transcript() == "turn around")) {
          for (matrix_hal::LedValue &led : everloop_image.leds) {
              // Turn off Everloop
              led.red = 0;
              led.green = 0;
              led.blue = 0;
              led.white = 0;
          }
          everloop_image.leds[4].red = ledBright;
          everloop_image.leds[9].red = ledBright;
          everloop_image.leds[14].red = ledBright;
          everloop_image.leds[19].red = ledBright;
          everloop_image.leds[24].red = ledBright;
          everloop_image.leds[29].red = ledBright;
          everloop_image.leds[34].red = ledBright;
          everloop.Write(&everloop_image);
          
          if (result.transcript() == "come to me") {
            audio_output.Stop();
            std::ifstream coordFile;
            std::string coordBuffer;
            std::string::size_type sz;
            float x;
            float dx;
            float angle;
            float dist;
            
            do {
              std::string file = "/home/pi/real-time-object-detection/person_detection.py --prototxt /home/pi/real-time-object-detection/MobileNetSSD_deploy.prototxt.txt --model /home/pi/real-time-object-detection/MobileNetSSD_deploy.caffemodel";
              std::string command = "python ";
              command += file;
              system(command.c_str());
              
              coordFile.open("/home/pi/Desktop/coordinates.txt");
              if (!coordFile) {
                std::cout << "Unable to open coordinates.txt\n";
              } else {
                std::getline(coordFile, coordBuffer);
              }
              coordFile.close();
                
              if (coordBuffer == "no person") {
                while(!movementTurn(&gpio, &imu_data, &imu_sensor, 'r', 'p', 45));
              } else {
                break;
              }
            } while (coordBuffer == "no person");
            
            if (coordBuffer != "no person") {
              x = std::stof(coordBuffer, &sz);
              dist = std::stof(coordBuffer.substr(sz));
              if (x < 200) { // left of center of frame
                angle = 30*(200 - x)/200; // camera has a 78 degree FoV
                while(!movementTurn(&gpio, &imu_data, &imu_sensor, 'l', 's', angle)); // turn to subject
                while(!movementStraight(&gpio, &imu_data, &imu_sensor, 'f', dist)); // go to subject
              } else if (x > 200) { // right of center of frame
                angle = 30*(x - 200)/200;
                while(!movementTurn(&gpio, &imu_data, &imu_sensor, 'r', 's', angle)); // turn to subject
                while(!movementStraight(&gpio, &imu_data, &imu_sensor, 'f', dist)); // go to subject
              } else { // center of frame
                while(!movementStraight(&gpio, &imu_data, &imu_sensor, 'f', dist)); // go to subject
              }
            }
          } else if (result.transcript() == "follow me") {
            audio_output.Stop();
            std::ifstream coordFile;
            std::string coordBuffer;
            std::string::size_type sz;
            float x;
            float xNew;
            float angle;
            float dist;
            float distNew;
            
            // Find subject
            do {
              std::string file = "/home/pi/real-time-object-detection/person_detection.py --prototxt /home/pi/real-time-object-detection/MobileNetSSD_deploy.prototxt.txt --model /home/pi/real-time-object-detection/MobileNetSSD_deploy.caffemodel";
              std::string command = "python ";
              command += file;
              system(command.c_str());
              
              coordFile.open("/home/pi/Desktop/coordinates.txt");
              if (!coordFile) {
                std::cout << "Unable to open coordinates.txt\n";
              } else {
                std::getline(coordFile, coordBuffer);
              }
              coordFile.close();
                
              if (coordBuffer == "no person") {
                while(!movementTurn(&gpio, &imu_data, &imu_sensor, 'r', 'p', 45)); // rotate if subject is not found
              } else {
                break; // Break out of loop if subject is found
              }
            } while (coordBuffer == "no person");
            
            if (coordBuffer != "no person") {
              x = std::stof(coordBuffer, &sz);
              dist = std::stof(coordBuffer.substr(sz));
              if (x < 200) { // left of center of frame
                angle = 30*(200 - x)/200; // camera has a 78 degree FoV
                while(!movementTurn(&gpio, &imu_data, &imu_sensor, 'l', 's', angle)); // turn to subject
                while(!movementStraight(&gpio, &imu_data, &imu_sensor, 'f', dist)); // go to subject
              } else if (x > 200) { // right of center of frame
                angle = 30*(x - 200)/200;
                while(!movementTurn(&gpio, &imu_data, &imu_sensor, 'r', 's', angle)); // turn to subject
                while(!movementStraight(&gpio, &imu_data, &imu_sensor, 'f', dist)); // go to subject
              } else { // center of frame
                while(!movementStraight(&gpio, &imu_data, &imu_sensor, 'f', dist)); // go to subject
              }
            }
            
            // Track subject
            while (1) {
              std::string file = "/home/pi/real-time-object-detection/person_detection.py --prototxt /home/pi/real-time-object-detection/MobileNetSSD_deploy.prototxt.txt --model /home/pi/real-time-object-detection/MobileNetSSD_deploy.caffemodel";
              std::string command = "python ";
              command += file;
              system(command.c_str());
              
              coordFile.open("/home/pi/Desktop/coordinates.txt");
              if (!coordFile) {
                std::cout << "Unable to open coordinates.txt\n";
              } else {
                std::getline(coordFile, coordBuffer);
              }
              coordFile.close();
              
              if (coordBuffer == "no person") {
                while (coordBuffer == "no person") {
                  std::string file = "/home/pi/real-time-object-detection/person_detection.py --prototxt /home/pi/real-time-object-detection/MobileNetSSD_deploy.prototxt.txt --model /home/pi/real-time-object-detection/MobileNetSSD_deploy.caffemodel";
                  std::string command = "python ";
                  command += file;
                  system(command.c_str());
                  
                  coordFile.open("/home/pi/Desktop/coordinates.txt");
                  if (!coordFile) {
                    std::cout << "Unable to open coordinates.txt\n";
                  } else {
                    std::getline(coordFile, coordBuffer);
                  }
                  coordFile.close();
                    
                  if (coordBuffer == "no person") {
                    while(!movementTurn(&gpio, &imu_data, &imu_sensor, 'r', 'p', 45));
                  } else {
                    break;
                  }
                }
              }
              xNew = std::stof(coordBuffer, &sz);
              distNew = std::stof(coordBuffer.substr(sz));
              if ((xNew < x - 10) || (xNew > x + 10)) { // Track latteral movement
                if (xNew < 200) { // left of center of frame
                  angle = 30*(200 - xNew)/200; // camera has a 78 degree FoV
                  while(!movementTurn(&gpio, &imu_data, &imu_sensor, 'l', 's', angle)); // turn to subject
                } else if (xNew > 200) { // right of center of frame
                  angle = 30*(xNew - 200)/200;
                  while(!movementTurn(&gpio, &imu_data, &imu_sensor, 'r', 's', angle)); // turn to subject
                } 
              }
              if ((distNew > dist + 2) || (distNew < dist + 2)) { // Track longitudinal movement
                if (distNew > dist) {
                  while(!movementStraight(&gpio, &imu_data, &imu_sensor, 'f', distNew)); // go to subject
                } else if (distNew < dist) {
                  while(!movementStraight(&gpio, &imu_data, &imu_sensor, 'r', distNew)); // go to subject
                }
              }
              x = xNew;
              dist = distNew;
            }
          } else if (result.transcript() == "go forward") {
            audio_output.Stop();
            while(!movementStraight(&gpio, &imu_data, &imu_sensor, 'f', 6));
          } else if (result.transcript() == "go backward") {
            audio_output.Stop();
            while(!movementStraight(&gpio, &imu_data, &imu_sensor, 'b', 6));
          } else if (result.transcript() == "turn right") {
            audio_output.Stop();
            while(!movementTurn(&gpio, &imu_data, &imu_sensor, 'r', 'p', 90));
          } else if (result.transcript() == "turn left") {
            audio_output.Stop();
            while(!movementTurn(&gpio, &imu_data, &imu_sensor, 'l', 'p', 90));
          } else if (result.transcript() == "turn around") {
            audio_output.Stop();
            while(!movementTurn(&gpio, &imu_data, &imu_sensor, 'l', 'p', 180));
          } 
        }
/***********************************************************************************/        
      }
      if (!html_out_command.empty() &&
          response.screen_out().data().size() > 0) {
        std::string html_out_base64 =
            base64_encode(response.screen_out().data());
        system((html_out_command + " \"data:text/html;base64, " +
                html_out_base64 + "\"")
                   .c_str());
      } else if (html_out_command.empty()) {
        if (response.dialog_state_out().supplemental_display_text().size() >
            0) {
          // CUSTOMIZE: render spoken response on screen
          std::clog << "assistant_sdk response:" << std::endl; // changed clog to cout
          std::cout << response.dialog_state_out().supplemental_display_text()
                    << std::endl;
        }
      }
    }

    audio_output.Stop();

    grpc::Status status = stream->Finish();
    if (!status.ok()) {
      // Report the RPC failure.
      std::cerr << "assistant_sdk failed, error: " << status.error_message()
                << std::endl;
      return -1;
    }
  }

  return 0;
}
