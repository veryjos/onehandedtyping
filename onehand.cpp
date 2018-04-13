#include "json/src/json.hpp"

#include <algorithm>

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <getopt.h>
#include <errno.h>

#include <string.h>

#include <linux/uinput.h>
#include <linux/input.h>

#include <time.h>

#include <unordered_map>
#include <vector>

#include <iostream>
#include <fstream>
#include <ostream>

#define true 1
#define false 0

#define UINPUT_PATH "/dev/uinput"
#define FAKEBOARD_NAME "OneHand Keyboard"

#define EV_KEY 0x01

// The keyboard device file descriptor
int kbfd;

// JSON configuration
using json = nlohmann::json;

const char* short_opt = "d:c:l:";
struct option long_opt[] =
{
	{"device",	required_argument,	0, 'd'},
	{"configure",	required_argument,	0, 'c'},
	{"load",	required_argument,	0, 'l'},
	{NULL,		0,			0, 0}

};

enum CONFIG_STEP
{
	CONFIG_STEP_SETMODKEY,
	CONFIG_STEP_SETSRCKEY,
	CONFIG_STEP_SETDSTKEY,
	CONFIG_STEP_DONE
} config_step;

void usage(FILE* file, int code)
{
	fprintf(file, "usage: onehand [OPTIONS]\n");
	fprintf(file, "Options:\n");
	fprintf(file, " -h, --help		Print this help message and exit\n");
	fprintf(file, " -d, --device file	Set the keyboard device to use\n");
	fprintf(file, " -c, --configure file	Configure keymapping and store in a file\n");
	fprintf(file, " -l, --load file		Load the keymappings from a file\n");

	exit(code);
}

int main(const int argc, char* const* argv)
{
	int onehandModKeyCode = 0;
	int onehandModKeyValue = 0;

	int configuring = 0;
	enum CONFIG_STEP configStep = CONFIG_STEP_SETMODKEY;

	char device[1024];
	device[0] = '\0';

	char keymapFile[1024];
	keymapFile[0] = '\0';

	std::unordered_map<int, int> valueMap;
	std::unordered_map<int, int> codeMap;

	std::vector<int> keysHeld;

	int onehandActive = false;

	int optionChar;
	while ((optionChar = getopt_long(argc, argv, short_opt, long_opt, 0)) != -1)
	{
		switch (optionChar)
		{
			case -1: // No more arguments
				break;
			case 0:
				usage(stderr, 1);
				break;
			case 'd':
				strncpy(device, optarg, 1024);
				break;
			case 'l':
				strncpy(keymapFile, optarg, 1024);
				break;
			case 'c':
				strncpy(keymapFile, optarg, 1024);
				configuring = true;
				break;
			default:
				fprintf(stderr, "Unknown option %c\n\n", optionChar);
				usage(stderr, 1);
				break;
		}
	}

	if (device[0] == '\0')
	{
		fprintf(stderr, "Invalid device specified\n\n");
		usage(stderr, 1);
	}

	if (!configuring && keymapFile[0] == '\0')
	{
		fprintf(stderr, "No configuration specified\nUse --config <filename> first\n\n");
		usage(stderr, 1);
	}

	// Sleep so the user has time to release the enter key..
	// If they don't, it'll be stuck on when we ioctl exclusive mode.
	// I know.
	sleep(1);

	// Create fake one-handed keyboard
	int uifd = open(UINPUT_PATH, O_WRONLY | O_NONBLOCK);

	if (uifd == -1)
	{
		fprintf(stderr, "Failed to open uinput device at %s\n", UINPUT_PATH);
		fprintf(stderr, "Are you running as sudo?\n");
		exit(1);
	}

	if (ioctl(uifd, UI_SET_EVBIT, EV_KEY))
	{
		fprintf(stderr, "Failed to set EV_KEY bit on emulated device, errno %d\n", errno);
		exit(1);
	}

	if (ioctl(uifd, UI_SET_EVBIT, EV_SYN))
	{
		fprintf(stderr, "Failed to set EV_SYN bit on emulated device, errno %d\n", errno);
		exit(1);
	}

	// Mark all keys as sendable
	for (int i=0;i<256;i++)
	{
		if (ioctl(uifd, UI_SET_KEYBIT, i))
		{
			fprintf(stderr, "Failed to set UI_SET_KEYBIT on %d, errno %d\n", i, errno);
			exit(1);
		}
	}

	// Create device
	struct uinput_user_dev uidevice;

	snprintf(uidevice.name, 80, FAKEBOARD_NAME);
	uidevice.id.bustype = BUS_USB;
	uidevice.id.vendor = 0x1234;
	uidevice.id.product = 0xfedc;
	uidevice.id.version = 1;

	if (write(uifd, &uidevice, sizeof(uidevice)) == -1)
	{
		fprintf(stderr, "Failed to write to uifd, errno %d\n", errno);
		exit(1);
	}

	// Create device
	if (ioctl(uifd, UI_DEV_CREATE) == -1)
	{
		fprintf(stderr, "Failed to create fakeboard device, errno %d\n", errno);
		exit(1);
	}

	// Now to read from our physical keyboard

	// Open keyboard device
	int kbfd = open(device, O_RDONLY);

	if (kbfd == -1)
	{
		fprintf(stderr, "Failed to open keyboard device at %s\n", device);
		fprintf(stderr, "Are you running as sudo?\n");
		exit(1);
	}

	// Get the device name
	char kbName[256] = "Unknown Device";
	ioctl(kbfd, EVIOCGNAME(sizeof(kbName)), kbName);

	printf("Opened device with name %s\n", kbName);

	// Attempt exclusive access to device..
	if (ioctl(kbfd, EVIOCGRAB, 1))
	{
		fprintf(stderr, "Exclusive access to device (EVIOCGRAB) failed with errornum %d\n", errno);
		exit(1);
	}

	if (configuring)
	{
		printf("Configuration mode active, follow onscreen prompts\n");
	}

	// Load your config
	if (!configuring)
	{
		// Get input stream for file
		std::fstream kFileStream;
		kFileStream.open(keymapFile);
		
		if(!kFileStream.good())
		{
			fprintf(stderr, "The file is not in good condition.\n");
			kFileStream.close();
			exit(1);
		}

		try {
			auto config = json::parse(kFileStream);

			onehandModKeyCode = config["onehandModKeyCode"];
			onehandModKeyValue = config["onehandModKeyValue"];

			auto codes = config["codes"];
			for (auto it=codes.begin();it!=codes.end();++it)
				codeMap[std::stoi(it.key())] = it.value();

			auto values = config["values"];
			for (auto it=values.begin();it!=values.end();++it)
				valueMap[std::stoi(it.key())] = it.value();
		}

		catch (std::exception) {
			fprintf(stderr, "Failed to parse user configuration file\n");
			kFileStream.close();
			return -1;
		}

		kFileStream.close();
	}

	// Start read loop
	int configDesiredKeyValue = 0;;
	int configDesiredKeyCode = 0;

	int configDesiredSrcCode = 0;
	int configDesiredSrcValue = 0;

	int configKeyPressed = false;
	int displayConfigMessage = true;

	int suppress = false;

	int readSize;
	int maxReadSize = sizeof(struct input_event);
	struct input_event readEvent[64];
	while (true)
	{
		if (configuring && displayConfigMessage)
		{
			if (configStep == CONFIG_STEP_SETMODKEY)
				printf("Press the key you'd like to set as modifier\n");
			else if (configStep == CONFIG_STEP_SETSRCKEY)
				printf("\n\nPress the source key (Press the original modifier key again to finish configuring)\n");
			else if (configStep == CONFIG_STEP_SETDSTKEY)
				printf("\n\nPress the destination key\n");
			else if (configStep == CONFIG_STEP_DONE)
				break;

			displayConfigMessage = false;
		}

		if ((readSize = read(kbfd, readEvent, maxReadSize * 64)) < maxReadSize)
		{
			// Read error
			exit(1);
			continue;
		}

		for (int i=0;i<readSize / maxReadSize;i++)
		{
			int code = readEvent[i].code;
			int type = readEvent[i].type;
			int value = readEvent[i].value;

			// printf("Type: %d Code: %d Value: %d\n", type, code, value);

			if (configuring && !configKeyPressed)
			{
				if (type != 4 && type != 1)
					continue;

				if (type == 4)
				{
					configDesiredKeyValue = value;
				} else if (type == 1)
				{
					if (value == 1)
					{
						configDesiredKeyCode = code;
						configKeyPressed = true;
					}
				}
			}

			if (type == 1 || type == 4)
			{
				if (code == onehandModKeyCode)
				{
					suppress = true;

					if (type == 1)
					{
						if (value == 1)
							onehandActive = true;
						else if (value == 0)
							onehandActive = false;

						// Suppress this press event, btw
						suppress = true;

						// Release any keys that were held down
						for (auto keyCode : keysHeld)
						{
							struct input_event releaseEvent;

							gettimeofday(&releaseEvent.time, 0);
							releaseEvent.type = 1;
							releaseEvent.code = keyCode;
							releaseEvent.value = 0;

							write(uifd, &releaseEvent, maxReadSize);
						}

						keysHeld.clear();
					}
				}
			}

			if (!configuring)
			{
				// Transform the key event to the mapped event if we're active
				if (type == 4)
				{
					if (onehandActive)
					{
						auto dstValue = valueMap.find(value);
						if (dstValue != valueMap.end())
							readEvent[i].value = dstValue->second;
					}
				} else if (type == 1)
				{
					if (onehandActive)
					{
						auto dstCode = codeMap.find(code);
						if (dstCode != codeMap.end())
						{
							readEvent[i].code = dstCode->second;

							// We have to keep track of keys pressed so we can auto-release them when the mod key is released
							if (value == 1) // Pressed
								keysHeld.push_back(dstCode->second);
							else if (value == 0)
								keysHeld.erase(std::remove(keysHeld.begin(), keysHeld.end(), dstCode->second), keysHeld.end());
						}
					}
				}

				if (!onehandActive)
				{
					if (type == 1)
					{
						if (value == 1)
							keysHeld.push_back(code);
						else if (value == 0)
							keysHeld.erase(std::remove(keysHeld.begin(), keysHeld.end(), code), keysHeld.end());
					}
				}

				// Pump event to our fake keyboard
				if (!suppress)
				{
					if (write(uifd, &readEvent[i], maxReadSize) == -1)
					{
						fprintf(stderr, "Failed to pump event to fakeboard, errno %d\n", errno);
						exit(1);
					}
				}
				else
					suppress = false;
			}
		}

		if (configuring && configKeyPressed)
		{
			if (configStep == CONFIG_STEP_SETMODKEY)
			{
				onehandModKeyCode = configDesiredKeyCode;
				onehandModKeyValue = configDesiredKeyValue;

				configStep = CONFIG_STEP_SETSRCKEY;
			}
			else if (configStep == CONFIG_STEP_SETSRCKEY)
				if (configDesiredKeyCode == onehandModKeyCode &&
				    configDesiredKeyValue == onehandModKeyValue)
				{
					configStep = CONFIG_STEP_DONE;
				}
				else
				{
					configDesiredSrcCode = configDesiredKeyCode;
					configDesiredSrcValue = configDesiredKeyValue;

					configStep = CONFIG_STEP_SETDSTKEY;
				}
			else if (configStep == CONFIG_STEP_SETDSTKEY)
			{
				codeMap[configDesiredSrcCode] = configDesiredKeyCode;
				valueMap[configDesiredSrcValue] = configDesiredKeyValue;

				configStep = CONFIG_STEP_SETSRCKEY;
			}

			configKeyPressed = false;
			displayConfigMessage = true;
		}
	}

	// Write config to file if configuring
	if (configuring)
	{
		json configRoot;

		configRoot["onehandModKeyCode"] = onehandModKeyCode;
		configRoot["onehandModKeyValue"] = onehandModKeyValue;

		auto codes = json::object();
		for (const auto& kv : codeMap)
			codes[std::to_string(kv.first)] = kv.second;

		configRoot["codes"] = codes;

		auto values = json::object();
		for (const auto& kv : valueMap)
			values[std::to_string(kv.first)] = kv.second;

		configRoot["values"] = values;

		std::ofstream outStream;
		outStream.open(keymapFile, std::ios::out);

		configRoot >> outStream;

		outStream.close();
	}

	ioctl(kbfd, EVIOCGRAB, 0);

	close(kbfd);
}
