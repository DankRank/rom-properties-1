/***************************************************************************
 * ROM Properties Page shell extension. (rp-stub)                          *
 * rp-stub.c: Stub program to invoke the rom-properties library.           *
 *                                                                         *
 * Copyright (c) 2016-2017 by David Korth.                                 *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

/**
 * rp-stub.c is a wrapper program for the Linux plugins.
 * It parses the command line and then searches for installed
 * rom-properties libraries. If found, it runs the requested
 * function from the library.
 */
#include "config.version.h"
#include "git.h"

#include "libunixcommon/dll-search.h"

// C includes.
#include <dlfcn.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/**
 * rp_create_thumbnail() function pointer.
 * @param source_file Source file. (UTF-8)
 * @param output_file Output file. (UTF-8)
 * @param maximum_size Maximum size.
 * @return 0 on success; non-zero on error.
 */
typedef int (*PFN_RP_CREATE_THUMBNAIL)(const char *source_file, const char *output_file, int maximum_size);

/**
 * rp_show_config_dialog() function pointer.
 * @param argc
 * @param argv
 * @return 0 on success; non-zero on error.
 */
typedef int (*PFN_RP_SHOW_CONFIG_DIALOG)(int argc, char *argv[]);

// Are we running as rp-config?
static uint8_t is_rp_config = 0;
// Is debug logging enabled?
static uint8_t is_debug = 0;

static void show_version(void)
{
	puts(RP_DESCRIPTION "\n"
		"Shared library stub program.\n"
		"Copyright (c) 2016-2017 by David Korth.\n"
		"\n"
		"rom-properties version: " RP_VERSION_STRING "\n"
#ifdef RP_GIT_VERSION
		RP_GIT_VERSION "\n"
# ifdef RP_GIT_DESCRIBE
		RP_GIT_DESCRIBE "\n"
# endif /* RP_GIT_DESCRIBE */
#endif /* RP_GIT_VERSION */
		// TODO: git version
		"\n"
		"This program is licensed under the GNU GPL v2.\n"
		"See http://www.gnu.org/licenses/gpl-2.0.html for more information.");
}

static void show_help(const char *argv0)
{
	show_version();
	if (!is_rp_config) {
		printf("\n"
			"Usage: %s [-s size] source_file output_file\n"
			"\n"
			"If source_file is a supported ROM image, a thumbnail is\n"
			"extracted and saved as output_file.\n"
			"\n"
			"Options:\n"
			"  -s, --size\t\tMaximum thumbnail size. (default is 256px)\n"
			"  -c, --config\t\tShow the configuration dialog instead of thumbnailing.\n"
			"  -d, --debug\t\tShow debug output when searching for rom-properties.\n"
			"  -h, --help\t\tDisplay this help and exit.\n"
			"  -V, --version\t\tOutput version information and exit.\n"
			, argv0);
	} else {
		printf("\n"
			"Usage: %s\n"
			"\n"
			"When invoked as rp-config, this program will open the configuration dialog\n"
			"using an installed plugin that most closely matches the currently running\n"
			"desktop environment.\n"
			, argv0);
	}
}

/**
 * Debug print function for rp_dll_search().
 * @param level Debug level.
 * @param format Format string.
 * @param ... Format arguments.
 * @return vfprintf() return value.
 */
static int ATTR_PRINTF(2, 3) fnDebug(int level, const char *format, ...)
{
	if (level < LEVEL_ERROR && !is_debug)
		return 0;

	va_list args;
	va_start(args, format);
	int ret = vfprintf(stderr, format, args);
	fputc('\n', stderr);
	va_end(args);

	return ret;
}

int main(int argc, char *argv[])
{
	/**
	 * Command line syntax:
	 * - Thumbnail: rp-stub [-s size] path output
	 * - Config:    rp-stub -c
	 *
	 * If invoked as 'rp-config', the configuration dialog
	 * will be shown instead of thumbnailing.
	 *
	 * TODO: Support URIs in addition to paths?
	 */

	// Check if we were invoked as 'rp-config'.
	const char *argv0 = argv[0];
	const char *slash = strrchr(argv0, '/');
	if (slash) {
		// Ignore the subdirectories.
		argv0 = slash + 1;
	}
	if (!strcmp(argv0, "rp-config")) {
		// Invoked as rp-config.
		is_rp_config = 1;
	}

	static const struct option long_options[] = {
		{"size",	required_argument,	nullptr, 's'},
		{"config",	no_argument,		nullptr, 'c'},
		{"debug",	no_argument,		nullptr, 'd'},
		{"help",	no_argument,		nullptr, 'h'},
		{"version",	no_argument,		nullptr, 'V'},
		// TODO: Option to scan for installed plugins.

		{NULL, 0, NULL, 0}
	};

	// Default to 256x256.
	uint8_t config = is_rp_config;
	int maximum_size = 256;
	int c, option_index;
	while ((c = getopt_long(argc, argv, "s:cdhV", long_options, &option_index)) != -1) {
		switch (c) {
			case 's': {
				char *endptr;
				errno = 0;
				long lTmp = (int)strtol(optarg, &endptr, 10);
				if (errno == ERANGE || *endptr != 0) {
					fprintf(stderr, "%s: invalid size '%s'\n"
						"Try '%s --help' for more information.\n",
						argv[0], optarg, argv[0]);
					return EXIT_FAILURE;
				} else if (lTmp <= 0 || lTmp > 32768) {
					fprintf(stderr, "%s: size '%s' is out of range\n"
						"Try '%s --help' for more information.\n",
						argv[0], optarg, argv[0]);
					return EXIT_FAILURE;
				}
				maximum_size = (int)lTmp;
				break;
			}

			case 'c':
				// Show the configuration dialog.
				config = 1;
				break;

			case 'd':
				// Enable debug output.
				is_debug = 1;
				break;

			case 'h':
				show_help(argv[0]);
				return EXIT_SUCCESS;

			case 'V':
				show_version();
				return EXIT_SUCCESS;

			case '?':
			default:
				// Unrecognized option.
				fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
				return EXIT_FAILURE;
		}
	}

	if (!config) {
		// We must have 2 filenames specified.
		if (optind == argc) {
			fprintf(stderr, "%s: missing source and output file parameters\n"
				"Try '%s --help' for more information.\n",
				argv[0], argv[0]);
			return EXIT_FAILURE;
		} else if (optind+1 == argc) {
			fprintf(stderr, "%s: missing output file parameter\n"
				"Try '%s --help' for more information.\n",
				argv[0], argv[0]);
			return EXIT_FAILURE;
		} else if (optind+3 < argc) {
			fprintf(stderr, "%s: too many parameters specified\n"
				"Try '%s --help' for more information.\n",
				argv[0], argv[0]);
			return EXIT_FAILURE;
		}
	}

	// Search for a usable rom-properties library.
	// TODO: Desktop override option?
	const char *const symname = (config ? "rp_show_config_dialog" : "rp_create_thumbnail");
	void *pDll = NULL, *pfn = NULL;
	int ret = rp_dll_search(symname, &pDll, &pfn, fnDebug);
	if (ret != 0) {
		return ret;
	}

	if (!config) {
		// Create the thumbnail.
		const char *const source_file = argv[optind];
		const char *const output_file = argv[optind+1];
		if (is_debug) {
			fprintf(stderr, "Calling function: %s(\"%s\", \"%s\", %d);\n",
				symname, source_file, output_file, maximum_size);
		}
		ret = ((PFN_RP_CREATE_THUMBNAIL)pfn)(source_file, output_file, maximum_size);
	} else {
		// Show the configuration dialog.
		if (is_debug) {
			fprintf(stderr, "Calling function: %s();\n", symname);
		}
		// FIXME: argc/argv may be manipulated by getopt().
		ret = ((PFN_RP_SHOW_CONFIG_DIALOG)pfn)(argc, argv);
	}

	dlclose(pDll);
	if (ret == 0) {
		if (is_debug) {
			fprintf(stderr, "%s() returned %d.\n", symname, ret);
		}
	} else {
		fprintf(stderr, "*** ERROR: %s() returned %d.\n", symname, ret);
	}
	return ret;
}
