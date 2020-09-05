/* Copyright (C) 2014 jaseg <github@jaseg.net>
 *
 * DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 * Version 2, December 2004
 *
 * Everyone is permitted to copy and distribute verbatim or modified
 * copies of this license document, and changing it is allowed as long
 * as the name is changed.
 *
 * DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
 * TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
 *
 * 0. You just DO WHAT THE FUCK YOU WANT TO.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <cerrno>

// Windows?
#include <getopt.h>

#ifdef WINDOWS
#include <Windows.h>
#include <io.h>
#define fileno _fileno
#define isatty _isatty
#else
#include <unistd.h>
#include <sys/time.h>
#endif

static char helpstr[] = "\n"
	"Usage: lolcat [-h horizontal_speed] [-v vertical_speed] [--] [FILES...]\n"
	"\n"
	"Concatenate FILE(s), or standard input, to standard output.\n"
	"With no FILE, or when FILE is -, read standard input.\n"
	"\n"
	"              -h <d>:   Horizontal rainbow frequency (default: 0.23)\n"
	"              -v <d>:   Vertical rainbow frequency (default: 0.1)\n"
	"                  -f:   Force color even when stdout is not a tty\n"
	"                  -l    Use locale instead of UTF-8\n"
	"                  -r:   Random colors\n"
	"           --version:   Print version and exit\n"
	"              --help:   Show this message\n"
	"\n"
	"Examples:\n"
	"  lolcat f - g      Output f's contents, then stdin, then g's contents.\n"
	"  lolcat            Copy standard input to standard output.\n"
	"  fortune | lolcat  Display a rainbow cookie.\n"
	"\n"
	"Report lolcat bugs to <https://github.com/jaseg/lolcat/issues>\n"
	"lolcat home page: <https://github.com/jaseg/lolcat/>\n"
	"Original idea: <https://github.com/busyloop/lolcat/>\n";

#define ARRAY_SIZE(foo) (sizeof(foo) / sizeof(foo[0]))
const unsigned char codes[] = { 39, 38, 44, 43, 49, 48, 84, 83, 119, 118, 154, 148, 184, 178, 214, 208, 209, 203, 204, 198, 199, 163, 164, 128, 129, 93, 99, 63, 69, 33 };


int find_escape_sequences(wchar_t ch, int state) {
	if (ch == '\033') { /* Escape sequence YAY */
		return 1;
	} else if(state == 1) {
		if(('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z')) return 2;
	} else {
		return 0;
	}
	return state;
}

template<class CharT>
void just_copy(std::basic_ostream<CharT> &to, std::basic_istream<CharT> &from) {
	to << from.rdbuf();
}

struct ColorSetting {
	double freq_h = 0.23, freq_v = 0.1;
	double offx = 0;
	int offset = 0;
};

struct ColorState : ColorSetting {
	int line = 0, col = 0;
	int last_color = -1;
	int escape_state = 0;
};

template<class CharT>
void colorise(std::basic_ostream<CharT> &to, std::basic_istream<CharT> &from, ColorState &state) {
	for(auto sbi = std::istreambuf_iterator<CharT>(from.rdbuf()); sbi != std::istreambuf_iterator<CharT>(); sbi++) {
		CharT c = *sbi;

		state.escape_state = find_escape_sequences(c, state.escape_state);
		if (!state.escape_state) {
			if (c == '\n') {
				state.line++;
				state.col = 0;
			} else {
				int newcolor = state.offx * ARRAY_SIZE(codes) + (int)((state.col += wcwidth(c)) * state.freq_h + state.line * state.freq_v);
				if (state.last_color != newcolor) {
					to << "\033[38;5;" << (int)codes[(state.offset + (state.last_color = newcolor)) % ARRAY_SIZE(codes)] << "m";
				}
			}
		}

		to << c;

		if (state.escape_state == 2) {
			to << "\033[38;5;" << codes[(state.offset + state.last_color) % ARRAY_SIZE(codes)] << "m";
		}
	}

	to << "\033[0m";
	return;
}

void print(const std::string &message, ColorState &state, bool color = true) {
	std::stringstream ss(message, std::ios_base::in);
	if(color) colorise(std::cout, ss, state);
	else just_copy(std::cout, ss);
}

void version(ColorState &state) {
	print("lolcat version 1.0, (c) 2014 jaseg\n", state);
	exit(0);
}

void help(ColorState &state) {
	print(helpstr, state);
	exit(0);
}

void usage(ColorState &state) {
	print("Usage: lolcat [-h horizontal_speed] [-v vertical_speed] [--] [FILES...]\n", state);
	exit(1);
}

int main(int argc, char** argv)
{
	std::ios_base::sync_with_stdio(false);

	int colors = isatty(fileno(stdout));
	int force_locale = 0, random = 0;

	ColorState state;

#if WINDOWS
	state.offx = ((GetTickCount()/1000) % 10) / 10.0;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	state.offx = (tv.tv_sec % 10) / 10.0;
#endif

	for(;;) {
		static struct option long_options[] = {
			{ "version", no_argument, 0, 0 },
			{ "help", no_argument, 0, 0 },
		};

		int index;
		int c = getopt_long(argc, argv, "h:v:flr", long_options, &index);
		switch(c) {
			case -1: goto done;
			case 0: switch(index) {
				case 0: version(state); break;
				case 1: help(state); break;
			}
			break;
			case 'h':
			case 'v': {
				char* endptr;
				auto val = strtod(optarg, &endptr);
				if (*endptr) usage(state);
				*(c == 'h' ? &state.freq_h : &state.freq_v) = val;
				break;
			}
			case 'f': colors = 1; break;
			case 'l': force_locale = 1; break;
			case 'r': random = 1; break;
			default: usage(state);
		}
	} done:

	if (random) {
		srand(time(NULL));
		state.offset = rand();
	}

	try {
		std::locale::global(std::locale(force_locale ? "" : "C.UTF-8"));
		std::wcin.imbue(std::locale());
		std::wcout.imbue(std::locale());
	} catch(const std::exception &e) {
		std::wcerr << "lolcat: Error setting locale: " << std::locale("").name().c_str() << std::endl;
	}

	if(optind >= argc) {
		if(colors) colorise(std::wcout, std::wcin, state);
		else just_copy(std::wcout, std::wcin);
	} else for(int i = optind; i < argc; i++) {
		if(std::string("-") == argv[i]) {
			if(colors) colorise(std::wcout, std::wcin, state);
			else just_copy(std::wcout, std::wcin);
		} else {
			std::wifstream in(argv[i]);
			if(!in) {
				std::wcerr << "lolcat: "<< argv[i] << ": " << (errno ? std::strerror(errno) : "Error reading file") << std::endl;
			} else try {
				if(colors) colorise(std::wcout, in, state);
				else just_copy(std::wcout, in);
			} catch(const std::exception &e) {
				std::wcerr << "lolcat: " << argv[i] << ": " << e.what() << std::endl;
			}
		}
	}

	return 0;
}
