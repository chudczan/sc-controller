/**
 * SC-Controller - tools
 * 
 * Various stuff that I don't care to fit anywhere else.
 * This also includes path-related tools originally placed in paths.py module.
 */
#pragma once
#include "scc/controller.h"
#include <stdbool.h>
#ifdef _WIN32
	#include <windows.h>
	// Windows have MAX_PATH instead and that's 250. Like, characters. Really.
	// Luckily, NT can handle even more than this, so this redefines it with more
	#undef PATH_MAX
	#define PATH_MAX 4096
#elif __linux__
	#include <linux/limits.h>
#else
	#include <sys/syslimits.h>
#endif


/**
 * Returns configuration directory, that is ~/.config/scc under normal conditions.
 * Returned value is cached internally and should NOT be free'd by caller.
 */
const char* scc_get_config_path();

/**
 * Returns path to socket that can be used to control scc-daemon,
 * that usually is ~/.config/scc/daemon.socket
 * Returned value is cached internally and should NOT be free'd by caller.
 */
const char* scc_get_daemon_socket();

/**
 * Returns directory where shared files are kept.
 * Usually /usr/share/scc, cwd() or $SCC_SHARED if defined
 * Returned value is cached internally and should NOT be free'd by caller.
 */
const char* scc_get_share_path();

/**
 * Returns directory where profiles are stored.
 * ~/.config/scc/profiles under normal conditions.
 */
const char* scc_get_profiles_path();

/**
 * Returns directory where default profiles are stored.
 * Probably something like /usr/share/scc/default_profiles,
 * or $SCC_SHARED/default_profiles if program is being started from
 * script extracted from source tarball
 */
const char* scc_get_default_profiles_path();

/**
 * Returns directory where menus are stored.
 * ~/.config/scc/menus under normal conditions.
 */
const char* scc_get_menus_path();

/**
 * Returns directory where menu icons are stored.
 * ~/.config/scc/menu-icons under normal conditions.
 */
const char* scc_get_menuicons_path();

/**
 * Returns directory where default menu icons are stored.
 * Probably something like /usr/share/scc/images/menu-icons,
 * or $SCC_SHARED/images/menu-icons if program is being started from
 * script extracted from source tarball
 */
const char* scc_get_default_menuicons_path();

/**
 * Returns directory when python (gui) modules are stored.
 */
const char* scc_get_python_src_path();

/**
 * Returns directory where default menus are stored.
 * Probably something like /usr/share/scc/default_menus,
 * or $SCC_SHARED/default_menus if program is being started from
 * script extracted from source tarball
 */
const char* scc_get_default_menus_path();

/**
 * Returns path to scc-daemon PID file.
 * Usually ~/.config/scc/daemon.pid
 */
const char* scc_get_pid_file();

/**
 * Returns filename for specified profile name.
 * This is done by searching for name + '.sccprofile' in ~/.config/scc/profiles
 * first and in /usr/share/scc/default_profiles if file is not found in first
 * location.
 *
 * Returns NULL if profile cannot be found (or on failed memory allocation)
 * Returned string has to be deallocated by caller.
 */
char* scc_find_profile(const char* name);

/**
 * Returns filename for specified menu.
 * This is done by searching for filename in ~/.config/scc/menus first
 * and in /usr/share/scc/default_menus if file is not found in first
 * location.
 *
 * Returns NULL if profile cannot be found (or on failed memory allocation)
 * Returned string has to be deallocated by caller.
 */
char* scc_find_menu(const char* name);

/**
 * Returns filename for specified icon name.
 * This is done by searching for <name>.png and <name>.bw.png, <name>.svg
 * and <name>.bw.svg in user and default menu-icons folders.
 *
 * If both colored and grayscale version is found, colored is returned, unless
 * prefer_colored is set to false.
 *
 * If has_colors is not set NULL, value it points to is set to true if colored
 * version of icon is found or to false if only grayscale icon is found. If no
 * icon is found at all, value is not changed.
 *
 * both 'paths' and 'extensions' has to be NULL-terminated. If set to NULL, defaults are used.
 * Returns NULL if icon cannot be found.
 * Returned value has to be freed by called.
 */
char* scc_find_icon(const char* name, bool prefer_colored, bool* has_colors, const char** paths, const char** extensions);

/**
 * Returns full path to script or binary.
 * With some exceptions, this is done by searching in paths as defined by PATH environment variable.
 *
 * Returns NULL if binary cannot be found (or on failed memory allocation)
 * Returned string has to be deallocated by caller.
 */
char* scc_find_binary(const char* name);

/**
 * For given value of PadStickTrigger enum (a.k.a What?), returns button
 * signalizing that corresponding pad / stick / trigger is pressed.
 *
 * Returns 0 when conversion is not possible;
 */
SCButton scc_what_to_pressed_button(PadStickTrigger what);

/**
 * For value of PadStickTrigger enum representing left, right or PS4 pad,
 * returns button used to signalize that corresponding pad is being touched.
 *
 * Returns 0 when conversion is not possible;
 */
SCButton scc_what_to_touch_button(PadStickTrigger what);

/**
 * Translates button name (expressed as upper-case string) to corresponding
 * value of SCButton enum.
 *
 * Returns 0 for unknown value.
 */
SCButton scc_string_to_button(const char* s);

/**
 * Translates pad, stuck or trigger name (expressed as upper-case string)
 * to corresponding value of PadStickTrigger enum.
 *
 * Returns 0 for unknown value.
 */
PadStickTrigger scc_string_to_pst(const char* s);

/**
 * Returns string matching PadStickTrigger value or NULL if value is not recognized.
 * Returned string shall not be deallocated.
 */
const char* scc_what_to_string(PadStickTrigger what);

/**
 * Returns string matching SCButton value or NULL if value is not recognized.
 * Returned string shall not be deallocated.
 */
const char* scc_button_to_string(SCButton b);


#ifdef _WIN32
/**
 * Replaces backslashes with forward slashes in given string.
 * String is modified in-place.
 * Returns number of replacements made.
 */
size_t scc_path_fix_slashes(char* path);
#endif

