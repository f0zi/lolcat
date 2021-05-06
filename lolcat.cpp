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

#include <wctype.h>

#include "cxxopts.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#define fileno _fileno
#define isatty _isatty
#define DEFAULT_LOCALE ".UTF8"
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#define DEFAULT_LOCALE "C.UTF-8"
#endif

static const char extrahelp[] = "\n"
	"Examples:\n"
	"  lolcat f - g      Output f's contents, then stdin, then g's contents.\n"
	"  lolcat            Copy standard input to standard output.\n"
	"  fortune | lolcat  Display a rainbow cookie.\n"
	"\n"
	"Report lolcat bugs to <https://github.com/jaseg/lolcat/issues>\n"
	"lolcat home page: <https://github.com/jaseg/lolcat/>\n"
	"Original idea: <https://github.com/busyloop/lolcat/>\n";

static const char version[] = "lolcat version 1.0, (c) 2014 jaseg\n";
static const char usage[] = "Usage: lolcat [-h horizontal_speed] [-v vertical_speed] [--] [FILES...]\n";

#define ARRAY_SIZE(foo) (sizeof(foo) / sizeof(foo[0]))
const unsigned char codes[] = { 39, 38, 44, 43, 49, 48, 84, 83, 119, 118, 154, 148, 184, 178, 214, 208, 209, 203, 204, 198, 199, 163, 164, 128, 129, 93, 99, 63, 69, 33 };

int getTerminalWidth() {
#if defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	return (int)(csbi.srWindow.Right - csbi.srWindow.Left + 1);
#elif defined(__linux__)
	struct winsize w;
	ioctl(fileno(stdout), TIOCGWINSZ, &w);
	return (int)(w.ws_col);
#endif
}

int charWidth(wchar_t ch) {
	return iswcntrl(ch) ? 0 : 1;
}

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
				int newcolor = (int)(state.offx * ARRAY_SIZE(codes) + (int)((state.col += charWidth(c)) * state.freq_h + state.line * state.freq_v));
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

int main(int argc, char** argv)
{
	using namespace std::string_literals;
	std::ios_base::sync_with_stdio(false);

	const auto width = getTerminalWidth();

	bool colors = false, force_locale = false, random = false;
	std::vector<std::string> files;

	ColorState state;

#if _WIN32
	{
		DWORD mode;
		auto console = GetStdHandle(STD_OUTPUT_HANDLE);
		if(GetConsoleMode(console, &mode)) SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	}
	state.offx = ((GetTickCount() / 1000) % 10) / 10.0;
#else
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		state.offx = (tv.tv_sec % 10) / 10.0;
	}
#endif

	cxxopts::Options options("lolcat",
		"Concatenate FILE(s), or standard input, to standard output.\n"
		"With no FILE, or when FILE is -, read standard input.\n");
	options
		.set_width(width)
		.custom_help("[-h horizontal_speed] [-v vertical_speed]")
		.positional_help("[--] [FILES...]")
		.show_positional_help()
		.add_options()
			("h", "Horizontal rainbow frequency", cxxopts::value(state.freq_h)->default_value("0.23"), "horizontal_speed")
			("v", "Vertical rainbow frequency", cxxopts::value(state.freq_v)->default_value("0.1"), "vertical_speed")
			("f", "Force color even when stdout is not a tty", cxxopts::value(colors))
			("l", "Use locale instead of UTF-8", cxxopts::value(force_locale))
			("r", "Random colors", cxxopts::value(random))
			("version", "Print version and exit")
			("help", "Show this message");

	options.add_options("positional")("FILES", "", cxxopts::value(files));
	options.parse_positional("FILES");

	try {
		auto args = options.parse(argc, argv);
		colors |= (bool)isatty(fileno(stdout));

		if(random) {
			srand((unsigned int)time(NULL));
			state.offset = rand();
		}

		try {
			std::locale::global(std::locale(force_locale ? "" : DEFAULT_LOCALE));
			std::wcin.imbue(std::locale());
			std::wcout.imbue(std::locale());
		}
		catch (const std::exception &e) {
			std::wcerr << "lolcat: Error setting locale: " << e.what() << std::endl;
		}

		if(args.count("help")) {
			print(options.help({""}), state);
			print(extrahelp, state);
			exit(0);
		}

		if(args.count("version")) {
			print(version, state);
			exit(0);
		}

		if(!files.size()) {
			if(colors) colorise(std::wcout, std::wcin, state);
			else just_copy(std::wcout, std::wcin);
		} else for(const auto &file : files) {
			std::wifstream in(file);
			if(!in) {
				std::wcerr << "lolcat: " << file.c_str() << ": " << (errno ? std::strerror(errno) : "Error reading file") << std::endl;
			} else try {
				if(colors) colorise(std::wcout, in, state);
				else just_copy(std::wcout, in);
			} catch(const std::exception& e) {
				std::wcerr << "lolcat: " << file.c_str() << ": " << e.what() << std::endl;
			}
		}
	} catch(const cxxopts::OptionException& e) {
		print("Error parsing arguments: "s + e.what() + "\n", state);
		print(usage, state);
		exit(1);
	}

	return 0;
}
