/***********************************************************
* Artsoft Retro-Game Library                               *
*----------------------------------------------------------*
* (c) 1994-2006 Artsoft Entertainment                      *
*               Holger Schemel                             *
*               Detmolder Strasse 189                      *
*               33604 Bielefeld                            *
*               Germany                                    *
*               e-mail: info@artsoft.org                   *
*----------------------------------------------------------*
* misc.c                                                   *
***********************************************************/

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "platform.h"

#if !defined(PLATFORM_WIN32)
#include <pwd.h>
#include <sys/param.h>
#endif

#include "misc.h"
#include "setup.h"
#include "random.h"
#include "text.h"
#include "image.h"


/* ========================================================================= */
/* some generic helper functions                                             */
/* ========================================================================= */

/* ------------------------------------------------------------------------- */
/* platform independent wrappers for printf() et al. (newline aware)         */
/* ------------------------------------------------------------------------- */

static void vfprintf_newline(FILE *stream, char *format, va_list ap)
{
  char *newline = STRING_NEWLINE;

  vfprintf(stream, format, ap);

  fprintf(stream, "%s", newline);
}

static void fprintf_newline(FILE *stream, char *format, ...)
{
  if (format)
  {
    va_list ap;

    va_start(ap, format);
    vfprintf_newline(stream, format, ap);
    va_end(ap);
  }
}

void fprintf_line(FILE *stream, char *line_chars, int line_length)
{
  int i;

  for (i = 0; i < line_length; i++)
    fprintf(stream, "%s", line_chars);

  fprintf_newline(stream, "");
}

void printf_line(char *line_chars, int line_length)
{
  fprintf_line(stdout, line_chars, line_length);
}

void printf_line_with_prefix(char *prefix, char *line_chars, int line_length)
{
  fprintf(stdout, "%s", prefix);
  fprintf_line(stdout, line_chars, line_length);
}


/* ------------------------------------------------------------------------- */
/* string functions                                                          */
/* ------------------------------------------------------------------------- */

/* int2str() returns a number converted to a string;
   the used memory is static, but will be overwritten by later calls,
   so if you want to save the result, copy it to a private string buffer;
   there can be 10 local calls of int2str() without buffering the result --
   the 11th call will then destroy the result from the first call and so on.
*/

char *int2str(int number, int size)
{
  static char shift_array[10][40];
  static int shift_counter = 0;
  char *s = shift_array[shift_counter];

  shift_counter = (shift_counter + 1) % 10;

  if (size > 20)
    size = 20;

  if (size > 0)
  {
    sprintf(s, "                    %09d", number);
    return &s[strlen(s) - size];
  }
  else
  {
    sprintf(s, "%d", number);
    return s;
  }
}


/* something similar to "int2str()" above, but allocates its own memory
   and has a different interface; we cannot use "itoa()", because this
   seems to be already defined when cross-compiling to the win32 target */

char *i_to_a(unsigned int i)
{
  static char *a = NULL;

  checked_free(a);

  if (i > 2147483647)	/* yes, this is a kludge */
    i = 2147483647;

  a = checked_malloc(10 + 1);

  sprintf(a, "%d", i);

  return a;
}


/* calculate base-2 logarithm of argument (rounded down to integer;
   this function returns the number of the highest bit set in argument) */

int log_2(unsigned int x)
{
  int e = 0;

  while ((1 << e) < x)
  {
    x -= (1 << e);	/* for rounding down (rounding up: remove this line) */
    e++;
  }

  return e;
}

boolean getTokenValueFromString(char *string, char **token, char **value)
{
  return getTokenValueFromSetupLine(string, token, value);
}


/* ------------------------------------------------------------------------- */
/* counter functions                                                         */
/* ------------------------------------------------------------------------- */

#if defined(PLATFORM_MSDOS)
volatile unsigned long counter = 0;

void increment_counter()
{
  counter++;
}

END_OF_FUNCTION(increment_counter);
#endif


/* maximal allowed length of a command line option */
#define MAX_OPTION_LEN		256

#if 1

#ifdef TARGET_SDL
static unsigned long getCurrentMS()
{
  return SDL_GetTicks();
}

#else /* !TARGET_SDL */

#if defined(PLATFORM_UNIX)
static unsigned long getCurrentMS()
{
  struct timeval current_time;

  gettimeofday(&current_time, NULL);

  return current_time.tv_sec * 1000 + current_time.tv_usec / 1000;
}
#endif /* PLATFORM_UNIX */
#endif /* !TARGET_SDL */

static unsigned long mainCounter(int mode)
{
  static unsigned long base_ms = 0;
  unsigned long current_ms;

  /* get current system milliseconds */
  current_ms = getCurrentMS();

  /* reset base timestamp in case of counter reset or wrap-around */
  if (mode == INIT_COUNTER || current_ms < base_ms)
    base_ms = current_ms;

  /* return milliseconds since last counter reset */
  return current_ms - base_ms;
}

#else

#ifdef TARGET_SDL
static unsigned long mainCounter(int mode)
{
  static unsigned long base_ms = 0;
  unsigned long current_ms;
  unsigned long counter_ms;

  current_ms = SDL_GetTicks();

  /* reset base time in case of counter initializing or wrap-around */
  if (mode == INIT_COUNTER || current_ms < base_ms)
    base_ms = current_ms;

  counter_ms = current_ms - base_ms;

  return counter_ms;		/* return milliseconds since last init */
}

#else /* !TARGET_SDL */

#if defined(PLATFORM_UNIX)
static unsigned long mainCounter(int mode)
{
  static struct timeval base_time = { 0, 0 };
  struct timeval current_time;
  unsigned long counter_ms;

  gettimeofday(&current_time, NULL);

  /* reset base time in case of counter initializing or wrap-around */
  if (mode == INIT_COUNTER || current_time.tv_sec < base_time.tv_sec)
    base_time = current_time;

  counter_ms = (current_time.tv_sec  - base_time.tv_sec)  * 1000
             + (current_time.tv_usec - base_time.tv_usec) / 1000;

  return counter_ms;		/* return milliseconds since last init */
}
#endif /* PLATFORM_UNIX */
#endif /* !TARGET_SDL */

#endif

void InitCounter()		/* set counter back to zero */
{
#if !defined(PLATFORM_MSDOS)
  mainCounter(INIT_COUNTER);
#else
  LOCK_VARIABLE(counter);
  LOCK_FUNCTION(increment_counter);
  install_int_ex(increment_counter, BPS_TO_TIMER(100));
#endif
}

unsigned long Counter()	/* get milliseconds since last call of InitCounter() */
{
#if !defined(PLATFORM_MSDOS)
  return mainCounter(READ_COUNTER);
#else
  return (counter * 10);
#endif
}

static void sleep_milliseconds(unsigned long milliseconds_delay)
{
  boolean do_busy_waiting = (milliseconds_delay < 5 ? TRUE : FALSE);

  if (do_busy_waiting)
  {
    /* we want to wait only a few ms -- if we assume that we have a
       kernel timer resolution of 10 ms, we would wait far to long;
       therefore it's better to do a short interval of busy waiting
       to get our sleeping time more accurate */

    unsigned long base_counter = Counter(), actual_counter = Counter();

    while (actual_counter < base_counter + milliseconds_delay &&
	   actual_counter >= base_counter)
      actual_counter = Counter();
  }
  else
  {
#if defined(TARGET_SDL)
    SDL_Delay(milliseconds_delay);
#elif defined(TARGET_ALLEGRO)
    rest(milliseconds_delay);
#else
    struct timeval delay;

    delay.tv_sec  = milliseconds_delay / 1000;
    delay.tv_usec = 1000 * (milliseconds_delay % 1000);

    if (select(0, NULL, NULL, NULL, &delay) != 0)
      Error(ERR_WARN, "sleep_milliseconds(): select() failed");
#endif
  }
}

void Delay(unsigned long delay)	/* Sleep specified number of milliseconds */
{
  sleep_milliseconds(delay);
}

boolean FrameReached(unsigned long *frame_counter_var,
		     unsigned long frame_delay)
{
  unsigned long actual_frame_counter = FrameCounter;

  if (actual_frame_counter >= *frame_counter_var &&
      actual_frame_counter < *frame_counter_var + frame_delay)
    return FALSE;

  *frame_counter_var = actual_frame_counter;

  return TRUE;
}

boolean DelayReached(unsigned long *counter_var,
		     unsigned long delay)
{
  unsigned long actual_counter = Counter();

  if (actual_counter >= *counter_var &&
      actual_counter < *counter_var + delay)
    return FALSE;

  *counter_var = actual_counter;

  return TRUE;
}

void WaitUntilDelayReached(unsigned long *counter_var, unsigned long delay)
{
  unsigned long actual_counter;

  while (1)
  {
    actual_counter = Counter();

    if (actual_counter >= *counter_var &&
	actual_counter < *counter_var + delay)
      sleep_milliseconds((*counter_var + delay - actual_counter) / 2);
    else
      break;
  }

  *counter_var = actual_counter;
}


/* ------------------------------------------------------------------------- */
/* random generator functions                                                */
/* ------------------------------------------------------------------------- */

unsigned int init_random_number(int nr, long seed)
{
  if (seed == NEW_RANDOMIZE)
  {
    /* default random seed */
    seed = (long)time(NULL);			// seconds since the epoch

#if !defined(PLATFORM_WIN32)
    /* add some more randomness */
    struct timeval current_time;

    gettimeofday(&current_time, NULL);

    seed += (long)current_time.tv_usec;		// microseconds since the epoch
#endif

#if defined(TARGET_SDL)
    /* add some more randomness */
    seed += (long)SDL_GetTicks();		// milliseconds since SDL init
#endif

#if 1
    /* add some more randomness */
    seed += GetSimpleRandom(1000000);
#endif
  }

  srandom_linux_libc(nr, (unsigned int) seed);

  return (unsigned int) seed;
}

unsigned int get_random_number(int nr, int max)
{
  return (max > 0 ? random_linux_libc(nr) % max : 0);
}


/* ------------------------------------------------------------------------- */
/* system info functions                                                     */
/* ------------------------------------------------------------------------- */

#if !defined(PLATFORM_MSDOS)
static char *get_corrected_real_name(char *real_name)
{
  char *real_name_new = checked_malloc(MAX_USERNAME_LEN + 1);
  char *from_ptr = real_name;
  char *to_ptr   = real_name_new;

  /* copy the name string, but not more than MAX_USERNAME_LEN characters */
  while (*from_ptr && (long)(to_ptr - real_name_new) < MAX_USERNAME_LEN - 1)
  {
    /* the name field read from "passwd" file may also contain additional
       user information, separated by commas, which will be removed here */
    if (*from_ptr == ',')
      break;

    /* the user's real name may contain '�' characters (german sharp s),
       which have no equivalent in upper case letters (used by our fonts) */
    if (*from_ptr == '�')
    {
      from_ptr++;
      *to_ptr++ = 's';
      *to_ptr++ = 's';
    }
    else
      *to_ptr++ = *from_ptr++;
  }

  *to_ptr = '\0';

  return real_name_new;
}
#endif

char *getLoginName()
{
  static char *login_name = NULL;

#if defined(PLATFORM_WIN32)
  if (login_name == NULL)
  {
    unsigned long buffer_size = MAX_USERNAME_LEN + 1;
    login_name = checked_malloc(buffer_size);

    if (GetUserName(login_name, &buffer_size) == 0)
      strcpy(login_name, ANONYMOUS_NAME);
  }
#else
  if (login_name == NULL)
  {
    struct passwd *pwd;

    if ((pwd = getpwuid(getuid())) == NULL)
      login_name = ANONYMOUS_NAME;
    else
      login_name = getStringCopy(pwd->pw_name);
  }
#endif

  return login_name;
}

char *getRealName()
{
  static char *real_name = NULL;

#if defined(PLATFORM_WIN32)
  if (real_name == NULL)
  {
    static char buffer[MAX_USERNAME_LEN + 1];
    unsigned long buffer_size = MAX_USERNAME_LEN + 1;

    if (GetUserName(buffer, &buffer_size) != 0)
      real_name = get_corrected_real_name(buffer);
    else
      real_name = ANONYMOUS_NAME;
  }
#elif defined(PLATFORM_UNIX)
  if (real_name == NULL)
  {
    struct passwd *pwd;

    if ((pwd = getpwuid(getuid())) != NULL && strlen(pwd->pw_gecos) != 0)
      real_name = get_corrected_real_name(pwd->pw_gecos);
    else
      real_name = ANONYMOUS_NAME;
  }
#else
  real_name = ANONYMOUS_NAME;
#endif

  return real_name;
}

time_t getFileTimestampEpochSeconds(char *filename)
{
  struct stat file_status;

  if (stat(filename, &file_status) != 0)	/* cannot stat file */
    return 0;

  return file_status.st_mtime;
}


/* ------------------------------------------------------------------------- */
/* path manipulation functions                                               */
/* ------------------------------------------------------------------------- */

static char *getLastPathSeparatorPtr(char *filename)
{
  char *last_separator = strrchr(filename, CHAR_PATH_SEPARATOR_UNIX);

  if (last_separator == NULL)	/* also try DOS/Windows variant */
    last_separator = strrchr(filename, CHAR_PATH_SEPARATOR_DOS);

  return last_separator;
}

char *getBaseNamePtr(char *filename)
{
  char *last_separator = getLastPathSeparatorPtr(filename);

  if (last_separator != NULL)
    return last_separator + 1;	/* separator found: strip base path */
  else
    return filename;		/* no separator found: filename has no path */
}

char *getBaseName(char *filename)
{
  return getStringCopy(getBaseNamePtr(filename));
}

char *getBasePath(char *filename)
{
  char *basepath = getStringCopy(filename);
  char *last_separator = getLastPathSeparatorPtr(basepath);

  if (last_separator != NULL)
    *last_separator = '\0';	/* separator found: strip basename */
  else
    basepath = ".";		/* no separator found: use current path */

  return basepath;
}


/* ------------------------------------------------------------------------- */
/* various string functions                                                  */
/* ------------------------------------------------------------------------- */

char *getStringCat2WithSeparator(char *s1, char *s2, char *sep)
{
  char *complete_string = checked_malloc(strlen(s1) + strlen(sep) +
					 strlen(s2) + 1);

  sprintf(complete_string, "%s%s%s", s1, sep, s2);

  return complete_string;
}

char *getStringCat3WithSeparator(char *s1, char *s2, char *s3, char *sep)
{
  char *complete_string = checked_malloc(strlen(s1) + strlen(sep) +
					 strlen(s2) + strlen(sep) +
					 strlen(s3) + 1);

  sprintf(complete_string, "%s%s%s%s%s", s1, sep, s2, sep, s3);

  return complete_string;
}

char *getStringCat2(char *s1, char *s2)
{
  return getStringCat2WithSeparator(s1, s2, "");
}

char *getStringCat3(char *s1, char *s2, char *s3)
{
  return getStringCat3WithSeparator(s1, s2, s3, "");
}

char *getPath2(char *path1, char *path2)
{
  return getStringCat2WithSeparator(path1, path2, STRING_PATH_SEPARATOR);
}

char *getPath3(char *path1, char *path2, char *path3)
{
  return getStringCat3WithSeparator(path1, path2, path3, STRING_PATH_SEPARATOR);
}

char *getStringCopy(char *s)
{
  char *s_copy;

  if (s == NULL)
    return NULL;

  s_copy = checked_malloc(strlen(s) + 1);
  strcpy(s_copy, s);

  return s_copy;
}

char *getStringCopyN(char *s, int n)
{
  char *s_copy;
  int s_len = MAX(0, n);

  if (s == NULL)
    return NULL;

  s_copy = checked_malloc(s_len + 1);
  strncpy(s_copy, s, s_len);
  s_copy[s_len] = '\0';

  return s_copy;
}

char *getStringToLower(char *s)
{
  char *s_copy = checked_malloc(strlen(s) + 1);
  char *s_ptr = s_copy;

  while (*s)
    *s_ptr++ = tolower(*s++);
  *s_ptr = '\0';

  return s_copy;
}

void setString(char **old_value, char *new_value)
{
  checked_free(*old_value);

  *old_value = getStringCopy(new_value);
}

boolean strEqual(char *s1, char *s2)
{
  return (s1 == NULL && s2 == NULL ? TRUE  :
	  s1 == NULL && s2 != NULL ? FALSE :
	  s1 != NULL && s2 == NULL ? FALSE :
	  strcmp(s1, s2) == 0);
}

boolean strEqualN(char *s1, char *s2, int n)
{
  return (s1 == NULL && s2 == NULL ? TRUE  :
	  s1 == NULL && s2 != NULL ? FALSE :
	  s1 != NULL && s2 == NULL ? FALSE :
	  strncmp(s1, s2, n) == 0);
}

boolean strPrefix(char *s, char *prefix)
{
  return (s == NULL && prefix == NULL ? TRUE  :
	  s == NULL && prefix != NULL ? FALSE :
	  s != NULL && prefix == NULL ? FALSE :
	  strncmp(s, prefix, strlen(prefix)) == 0);
}

boolean strSuffix(char *s, char *suffix)
{
  return (s == NULL && suffix == NULL ? TRUE  :
	  s == NULL && suffix != NULL ? FALSE :
	  s != NULL && suffix == NULL ? FALSE :
	  strlen(s) < strlen(suffix)  ? FALSE :
	  strncmp(&s[strlen(s) - strlen(suffix)], suffix, strlen(suffix)) == 0);
}

boolean strPrefixLower(char *s, char *prefix)
{
  char *s_lower = getStringToLower(s);
  boolean match = strPrefix(s_lower, prefix);

  free(s_lower);

  return match;
}

boolean strSuffixLower(char *s, char *suffix)
{
  char *s_lower = getStringToLower(s);
  boolean match = strSuffix(s_lower, suffix);

  free(s_lower);

  return match;
}


/* ------------------------------------------------------------------------- */
/* command line option handling functions                                    */
/* ------------------------------------------------------------------------- */

void GetOptions(char *argv[], void (*print_usage_function)(void))
{
  char *ro_base_path = RO_BASE_PATH;
  char *rw_base_path = RW_BASE_PATH;
  char **options_left = &argv[1];

#if !defined(PLATFORM_MACOSX)
  /* if the program is configured to start from current directory (default),
     determine program package directory (KDE/Konqueror does not do this by
     itself and fails otherwise); on Mac OS X, the program binary is stored
     in an application package directory -- do not try to use this directory
     as the program data directory (Mac OS X handles this correctly anyway) */

  if (strEqual(ro_base_path, "."))
    ro_base_path = program.command_basepath;
  if (strEqual(rw_base_path, "."))
    rw_base_path = program.command_basepath;
#endif

  /* initialize global program options */
  options.display_name = NULL;
  options.server_host = NULL;
  options.server_port = 0;

  options.ro_base_directory = ro_base_path;
  options.rw_base_directory = rw_base_path;
  options.level_directory    = getPath2(ro_base_path, LEVELS_DIRECTORY);
  options.graphics_directory = getPath2(ro_base_path, GRAPHICS_DIRECTORY);
  options.sounds_directory   = getPath2(ro_base_path, SOUNDS_DIRECTORY);
  options.music_directory    = getPath2(ro_base_path, MUSIC_DIRECTORY);
  options.docs_directory     = getPath2(ro_base_path, DOCS_DIRECTORY);

  options.execute_command = NULL;
  options.special_flags = NULL;

  options.serveronly = FALSE;
  options.network = FALSE;
  options.verbose = FALSE;
  options.debug = FALSE;
  options.debug_x11_sync = FALSE;

#if !defined(PLATFORM_UNIX)
  if (*options_left == NULL)	/* no options given -- enable verbose mode */
    options.verbose = TRUE;
#endif

  while (*options_left)
  {
    char option_str[MAX_OPTION_LEN];
    char *option = options_left[0];
    char *next_option = options_left[1];
    char *option_arg = NULL;
    int option_len = strlen(option);

    if (option_len >= MAX_OPTION_LEN)
      Error(ERR_EXIT_HELP, "unrecognized option '%s'", option);

    strcpy(option_str, option);			/* copy argument into buffer */
    option = option_str;

    if (strEqual(option, "--"))			/* stop scanning arguments */
      break;

    if (strPrefix(option, "--"))		/* treat '--' like '-' */
      option++;

    option_arg = strchr(option, '=');
    if (option_arg == NULL)			/* no '=' in option */
      option_arg = next_option;
    else
    {
      *option_arg++ = '\0';			/* cut argument from option */
      if (*option_arg == '\0')			/* no argument after '=' */
	Error(ERR_EXIT_HELP, "option '%s' has invalid argument", option_str);
    }

    option_len = strlen(option);

    if (strEqual(option, "-"))
      Error(ERR_EXIT_HELP, "unrecognized option '%s'", option);
    else if (strncmp(option, "-help", option_len) == 0)
    {
      print_usage_function();

      exit(0);
    }
    else if (strncmp(option, "-display", option_len) == 0)
    {
      if (option_arg == NULL)
	Error(ERR_EXIT_HELP, "option '%s' requires an argument", option_str);

      options.display_name = option_arg;
      if (option_arg == next_option)
	options_left++;
    }
    else if (strncmp(option, "-basepath", option_len) == 0)
    {
      if (option_arg == NULL)
	Error(ERR_EXIT_HELP, "option '%s' requires an argument", option_str);

      /* this should be extended to separate options for ro and rw data */
      options.ro_base_directory = ro_base_path = option_arg;
      options.rw_base_directory = rw_base_path = option_arg;
      if (option_arg == next_option)
	options_left++;

      /* adjust paths for sub-directories in base directory accordingly */
      options.level_directory    = getPath2(ro_base_path, LEVELS_DIRECTORY);
      options.graphics_directory = getPath2(ro_base_path, GRAPHICS_DIRECTORY);
      options.sounds_directory   = getPath2(ro_base_path, SOUNDS_DIRECTORY);
      options.music_directory    = getPath2(ro_base_path, MUSIC_DIRECTORY);
      options.docs_directory     = getPath2(ro_base_path, DOCS_DIRECTORY);
    }
    else if (strncmp(option, "-levels", option_len) == 0)
    {
      if (option_arg == NULL)
	Error(ERR_EXIT_HELP, "option '%s' requires an argument", option_str);

      options.level_directory = option_arg;
      if (option_arg == next_option)
	options_left++;
    }
    else if (strncmp(option, "-graphics", option_len) == 0)
    {
      if (option_arg == NULL)
	Error(ERR_EXIT_HELP, "option '%s' requires an argument", option_str);

      options.graphics_directory = option_arg;
      if (option_arg == next_option)
	options_left++;
    }
    else if (strncmp(option, "-sounds", option_len) == 0)
    {
      if (option_arg == NULL)
	Error(ERR_EXIT_HELP, "option '%s' requires an argument", option_str);

      options.sounds_directory = option_arg;
      if (option_arg == next_option)
	options_left++;
    }
    else if (strncmp(option, "-music", option_len) == 0)
    {
      if (option_arg == NULL)
	Error(ERR_EXIT_HELP, "option '%s' requires an argument", option_str);

      options.music_directory = option_arg;
      if (option_arg == next_option)
	options_left++;
    }
    else if (strncmp(option, "-network", option_len) == 0)
    {
      options.network = TRUE;
    }
    else if (strncmp(option, "-serveronly", option_len) == 0)
    {
      options.serveronly = TRUE;
    }
    else if (strncmp(option, "-verbose", option_len) == 0)
    {
      options.verbose = TRUE;
    }
    else if (strncmp(option, "-debug", option_len) == 0)
    {
      options.debug = TRUE;
    }
    else if (strncmp(option, "-debug-x11-sync", option_len) == 0)
    {
      options.debug_x11_sync = TRUE;
    }
    else if (strPrefix(option, "-D"))
    {
#if 1
      options.special_flags = getStringCopy(&option[2]);
#else
      char *flags_string = &option[2];
      unsigned long flags_value;

      if (*flags_string == '\0')
	Error(ERR_EXIT_HELP, "empty flag ignored");

      flags_value = get_special_flags_function(flags_string);

      if (flags_value == 0)
	Error(ERR_EXIT_HELP, "unknown flag '%s'", flags_string);

      options.special_flags |= flags_value;
#endif
    }
    else if (strncmp(option, "-execute", option_len) == 0)
    {
      if (option_arg == NULL)
	Error(ERR_EXIT_HELP, "option '%s' requires an argument", option_str);

      options.execute_command = option_arg;
      if (option_arg == next_option)
	options_left++;

      /* when doing batch processing, always enable verbose mode (warnings) */
      options.verbose = TRUE;
    }
    else if (*option == '-')
    {
      Error(ERR_EXIT_HELP, "unrecognized option '%s'", option_str);
    }
    else if (options.server_host == NULL)
    {
      options.server_host = *options_left;
    }
    else if (options.server_port == 0)
    {
      options.server_port = atoi(*options_left);
      if (options.server_port < 1024)
	Error(ERR_EXIT_HELP, "bad port number '%d'", options.server_port);
    }
    else
      Error(ERR_EXIT_HELP, "too many arguments");

    options_left++;
  }
}


/* ------------------------------------------------------------------------- */
/* error handling functions                                                  */
/* ------------------------------------------------------------------------- */

/* used by SetError() and GetError() to store internal error messages */
static char internal_error[1024];	/* this is bad */

void SetError(char *format, ...)
{
  va_list ap;

  va_start(ap, format);
  vsprintf(internal_error, format, ap);
  va_end(ap);
}

char *GetError()
{
  return internal_error;
}

void Error(int mode, char *format, ...)
{
  static boolean last_line_was_separator = FALSE;
  char *process_name = "";

  /* display warnings only when running in verbose mode */
  if (mode & ERR_WARN && !options.verbose)
    return;

  if (mode == ERR_INFO_LINE)
  {
    if (!last_line_was_separator)
      fprintf_line(program.error_file, format, 79);

    last_line_was_separator = TRUE;

    return;
  }

  last_line_was_separator = FALSE;

  if (mode & ERR_SOUND_SERVER)
    process_name = " sound server";
  else if (mode & ERR_NETWORK_SERVER)
    process_name = " network server";
  else if (mode & ERR_NETWORK_CLIENT)
    process_name = " network client **";

  if (format)
  {
    va_list ap;

    fprintf(program.error_file, "%s%s: ", program.command_basename,
	    process_name);

    if (mode & ERR_WARN)
      fprintf(program.error_file, "warning: ");

    va_start(ap, format);
    vfprintf_newline(program.error_file, format, ap);
    va_end(ap);
  }
  
  if (mode & ERR_HELP)
    fprintf_newline(program.error_file,
		    "%s: Try option '--help' for more information.",
		    program.command_basename);

  if (mode & ERR_EXIT)
    fprintf_newline(program.error_file, "%s%s: aborting",
		    program.command_basename, process_name);

  if (mode & ERR_EXIT)
  {
    if (mode & ERR_FROM_SERVER)
      exit(1);				/* child process: normal exit */
    else
      program.exit_function(1);		/* main process: clean up stuff */
  }
}


/* ------------------------------------------------------------------------- */
/* checked memory allocation and freeing functions                           */
/* ------------------------------------------------------------------------- */

void *checked_malloc(unsigned long size)
{
  void *ptr;

  ptr = malloc(size);

  if (ptr == NULL)
    Error(ERR_EXIT, "cannot allocate %d bytes -- out of memory", size);

  return ptr;
}

void *checked_calloc(unsigned long size)
{
  void *ptr;

  ptr = calloc(1, size);

  if (ptr == NULL)
    Error(ERR_EXIT, "cannot allocate %d bytes -- out of memory", size);

  return ptr;
}

void *checked_realloc(void *ptr, unsigned long size)
{
  ptr = realloc(ptr, size);

  if (ptr == NULL)
    Error(ERR_EXIT, "cannot allocate %d bytes -- out of memory", size);

  return ptr;
}

void checked_free(void *ptr)
{
  if (ptr != NULL)	/* this check should be done by free() anyway */
    free(ptr);
}

void clear_mem(void *ptr, unsigned long size)
{
#if defined(PLATFORM_WIN32)
  /* for unknown reason, memset() sometimes crashes when compiled with MinGW */
  char *cptr = (char *)ptr;

  while (size--)
    *cptr++ = 0;
#else
  memset(ptr, 0, size);
#endif
}


/* ------------------------------------------------------------------------- */
/* various helper functions                                                  */
/* ------------------------------------------------------------------------- */

inline void swap_numbers(int *i1, int *i2)
{
  int help = *i1;

  *i1 = *i2;
  *i2 = help;
}

inline void swap_number_pairs(int *x1, int *y1, int *x2, int *y2)
{
  int help_x = *x1;
  int help_y = *y1;

  *x1 = *x2;
  *x2 = help_x;

  *y1 = *y2;
  *y2 = help_y;
}

/* the "put" variants of the following file access functions check for the file
   pointer being != NULL and return the number of bytes they have or would have
   written; this allows for chunk writing functions to first determine the size
   of the (not yet written) chunk, write the correct chunk size and finally
   write the chunk itself */

int getFile8BitInteger(FILE *file)
{
  return fgetc(file);
}

int putFile8BitInteger(FILE *file, int value)
{
  if (file != NULL)
    fputc(value, file);

  return 1;
}

int getFile16BitInteger(FILE *file, int byte_order)
{
  if (byte_order == BYTE_ORDER_BIG_ENDIAN)
    return ((fgetc(file) << 8) |
	    (fgetc(file) << 0));
  else		 /* BYTE_ORDER_LITTLE_ENDIAN */
    return ((fgetc(file) << 0) |
	    (fgetc(file) << 8));
}

int putFile16BitInteger(FILE *file, int value, int byte_order)
{
  if (file != NULL)
  {
    if (byte_order == BYTE_ORDER_BIG_ENDIAN)
    {
      fputc((value >> 8) & 0xff, file);
      fputc((value >> 0) & 0xff, file);
    }
    else	   /* BYTE_ORDER_LITTLE_ENDIAN */
    {
      fputc((value >> 0) & 0xff, file);
      fputc((value >> 8) & 0xff, file);
    }
  }

  return 2;
}

int getFile32BitInteger(FILE *file, int byte_order)
{
  if (byte_order == BYTE_ORDER_BIG_ENDIAN)
    return ((fgetc(file) << 24) |
	    (fgetc(file) << 16) |
	    (fgetc(file) <<  8) |
	    (fgetc(file) <<  0));
  else		 /* BYTE_ORDER_LITTLE_ENDIAN */
    return ((fgetc(file) <<  0) |
	    (fgetc(file) <<  8) |
	    (fgetc(file) << 16) |
	    (fgetc(file) << 24));
}

int putFile32BitInteger(FILE *file, int value, int byte_order)
{
  if (file != NULL)
  {
    if (byte_order == BYTE_ORDER_BIG_ENDIAN)
    {
      fputc((value >> 24) & 0xff, file);
      fputc((value >> 16) & 0xff, file);
      fputc((value >>  8) & 0xff, file);
      fputc((value >>  0) & 0xff, file);
    }
    else	   /* BYTE_ORDER_LITTLE_ENDIAN */
    {
      fputc((value >>  0) & 0xff, file);
      fputc((value >>  8) & 0xff, file);
      fputc((value >> 16) & 0xff, file);
      fputc((value >> 24) & 0xff, file);
    }
  }

  return 4;
}

boolean getFileChunk(FILE *file, char *chunk_name, int *chunk_size,
		     int byte_order)
{
  const int chunk_name_length = 4;

  /* read chunk name */
  fgets(chunk_name, chunk_name_length + 1, file);

  if (chunk_size != NULL)
  {
    /* read chunk size */
    *chunk_size = getFile32BitInteger(file, byte_order);
  }

  return (feof(file) || ferror(file) ? FALSE : TRUE);
}

int putFileChunk(FILE *file, char *chunk_name, int chunk_size,
		 int byte_order)
{
  int num_bytes = 0;

  /* write chunk name */
  if (file != NULL)
    fputs(chunk_name, file);

  num_bytes += strlen(chunk_name);

  if (chunk_size >= 0)
  {
    /* write chunk size */
    if (file != NULL)
      putFile32BitInteger(file, chunk_size, byte_order);

    num_bytes += 4;
  }

  return num_bytes;
}

int getFileVersion(FILE *file)
{
  int version_major = fgetc(file);
  int version_minor = fgetc(file);
  int version_patch = fgetc(file);
  int version_build = fgetc(file);

  return VERSION_IDENT(version_major, version_minor, version_patch,
		       version_build);
}

int putFileVersion(FILE *file, int version)
{
  if (file != NULL)
  {
    int version_major = VERSION_MAJOR(version);
    int version_minor = VERSION_MINOR(version);
    int version_patch = VERSION_PATCH(version);
    int version_build = VERSION_BUILD(version);

    fputc(version_major, file);
    fputc(version_minor, file);
    fputc(version_patch, file);
    fputc(version_build, file);
  }

  return 4;
}

void ReadBytesFromFile(FILE *file, byte *buffer, unsigned long bytes)
{
  int i;

  for(i = 0; i < bytes && !feof(file); i++)
    buffer[i] = fgetc(file);
}

void WriteBytesToFile(FILE *file, byte *buffer, unsigned long bytes)
{
  int i;

  for(i = 0; i < bytes; i++)
    fputc(buffer[i], file);
}

void ReadUnusedBytesFromFile(FILE *file, unsigned long bytes)
{
  while (bytes-- && !feof(file))
    fgetc(file);
}

void WriteUnusedBytesToFile(FILE *file, unsigned long bytes)
{
  while (bytes--)
    fputc(0, file);
}


/* ------------------------------------------------------------------------- */
/* functions to translate key identifiers between different format           */
/* ------------------------------------------------------------------------- */

#define TRANSLATE_KEYSYM_TO_KEYNAME	0
#define TRANSLATE_KEYSYM_TO_X11KEYNAME	1
#define TRANSLATE_KEYNAME_TO_KEYSYM	2
#define TRANSLATE_X11KEYNAME_TO_KEYSYM	3

void translate_keyname(Key *keysym, char **x11name, char **name, int mode)
{
  static struct
  {
    Key key;
    char *x11name;
    char *name;
  } translate_key[] =
  {
    /* normal cursor keys */
    { KSYM_Left,	"XK_Left",		"cursor left" },
    { KSYM_Right,	"XK_Right",		"cursor right" },
    { KSYM_Up,		"XK_Up",		"cursor up" },
    { KSYM_Down,	"XK_Down",		"cursor down" },

    /* keypad cursor keys */
#ifdef KSYM_KP_Left
    { KSYM_KP_Left,	"XK_KP_Left",		"keypad left" },
    { KSYM_KP_Right,	"XK_KP_Right",		"keypad right" },
    { KSYM_KP_Up,	"XK_KP_Up",		"keypad up" },
    { KSYM_KP_Down,	"XK_KP_Down",		"keypad down" },
#endif

    /* other keypad keys */
#ifdef KSYM_KP_Enter
    { KSYM_KP_Enter,	"XK_KP_Enter",		"keypad enter" },
    { KSYM_KP_Add,	"XK_KP_Add",		"keypad +" },
    { KSYM_KP_Subtract,	"XK_KP_Subtract",	"keypad -" },
    { KSYM_KP_Multiply,	"XK_KP_Multiply",	"keypad mltply" },
    { KSYM_KP_Divide,	"XK_KP_Divide",		"keypad /" },
    { KSYM_KP_Separator,"XK_KP_Separator",	"keypad ," },
#endif

    /* modifier keys */
    { KSYM_Shift_L,	"XK_Shift_L",		"left shift" },
    { KSYM_Shift_R,	"XK_Shift_R",		"right shift" },
    { KSYM_Control_L,	"XK_Control_L",		"left control" },
    { KSYM_Control_R,	"XK_Control_R",		"right control" },
    { KSYM_Meta_L,	"XK_Meta_L",		"left meta" },
    { KSYM_Meta_R,	"XK_Meta_R",		"right meta" },
    { KSYM_Alt_L,	"XK_Alt_L",		"left alt" },
    { KSYM_Alt_R,	"XK_Alt_R",		"right alt" },
    { KSYM_Super_L,	"XK_Super_L",		"left super" },	 /* Win-L */
    { KSYM_Super_R,	"XK_Super_R",		"right super" }, /* Win-R */
    { KSYM_Mode_switch,	"XK_Mode_switch",	"mode switch" }, /* Alt-R */
    { KSYM_Multi_key,	"XK_Multi_key",		"multi key" },	 /* Ctrl-R */

    /* some special keys */
    { KSYM_BackSpace,	"XK_BackSpace",		"backspace" },
    { KSYM_Delete,	"XK_Delete",		"delete" },
    { KSYM_Insert,	"XK_Insert",		"insert" },
    { KSYM_Tab,		"XK_Tab",		"tab" },
    { KSYM_Home,	"XK_Home",		"home" },
    { KSYM_End,		"XK_End",		"end" },
    { KSYM_Page_Up,	"XK_Page_Up",		"page up" },
    { KSYM_Page_Down,	"XK_Page_Down",		"page down" },
    { KSYM_Menu,	"XK_Menu",		"menu" },	 /* Win-Menu */

    /* ASCII 0x20 to 0x40 keys (except numbers) */
    { KSYM_space,	"XK_space",		"space" },
    { KSYM_exclam,	"XK_exclam",		"!" },
    { KSYM_quotedbl,	"XK_quotedbl",		"\"" },
    { KSYM_numbersign,	"XK_numbersign",	"#" },
    { KSYM_dollar,	"XK_dollar",		"$" },
    { KSYM_percent,	"XK_percent",		"%" },
    { KSYM_ampersand,	"XK_ampersand",		"&" },
    { KSYM_apostrophe,	"XK_apostrophe",	"'" },
    { KSYM_parenleft,	"XK_parenleft",		"(" },
    { KSYM_parenright,	"XK_parenright",	")" },
    { KSYM_asterisk,	"XK_asterisk",		"*" },
    { KSYM_plus,	"XK_plus",		"+" },
    { KSYM_comma,	"XK_comma",		"," },
    { KSYM_minus,	"XK_minus",		"-" },
    { KSYM_period,	"XK_period",		"." },
    { KSYM_slash,	"XK_slash",		"/" },
    { KSYM_colon,	"XK_colon",		":" },
    { KSYM_semicolon,	"XK_semicolon",		";" },
    { KSYM_less,	"XK_less",		"<" },
    { KSYM_equal,	"XK_equal",		"=" },
    { KSYM_greater,	"XK_greater",		">" },
    { KSYM_question,	"XK_question",		"?" },
    { KSYM_at,		"XK_at",		"@" },

    /* more ASCII keys */
    { KSYM_bracketleft,	"XK_bracketleft",	"[" },
    { KSYM_backslash,	"XK_backslash",		"\\" },
    { KSYM_bracketright,"XK_bracketright",	"]" },
    { KSYM_asciicircum,	"XK_asciicircum",	"^" },
    { KSYM_underscore,	"XK_underscore",	"_" },
    { KSYM_grave,	"XK_grave",		"grave" },
    { KSYM_quoteleft,	"XK_quoteleft",		"quote left" },
    { KSYM_braceleft,	"XK_braceleft",		"brace left" },
    { KSYM_bar,		"XK_bar",		"bar" },
    { KSYM_braceright,	"XK_braceright",	"brace right" },
    { KSYM_asciitilde,	"XK_asciitilde",	"~" },

    /* special (non-ASCII) keys */
    { KSYM_degree,	"XK_degree",		"�" },
    { KSYM_Adiaeresis,	"XK_Adiaeresis",	"�" },
    { KSYM_Odiaeresis,	"XK_Odiaeresis",	"�" },
    { KSYM_Udiaeresis,	"XK_Udiaeresis",	"�" },
    { KSYM_adiaeresis,	"XK_adiaeresis",	"�" },
    { KSYM_odiaeresis,	"XK_odiaeresis",	"�" },
    { KSYM_udiaeresis,	"XK_udiaeresis",	"�" },
    { KSYM_ssharp,	"XK_ssharp",		"sharp s" },

    /* end-of-array identifier */
    { 0,                NULL,			NULL }
  };

  int i;

  if (mode == TRANSLATE_KEYSYM_TO_KEYNAME)
  {
    static char name_buffer[30];
    Key key = *keysym;

    if (key >= KSYM_A && key <= KSYM_Z)
      sprintf(name_buffer, "%c", 'A' + (char)(key - KSYM_A));
    else if (key >= KSYM_a && key <= KSYM_z)
      sprintf(name_buffer, "%c", 'a' + (char)(key - KSYM_a));
    else if (key >= KSYM_0 && key <= KSYM_9)
      sprintf(name_buffer, "%c", '0' + (char)(key - KSYM_0));
    else if (key >= KSYM_KP_0 && key <= KSYM_KP_9)
      sprintf(name_buffer, "keypad %c", '0' + (char)(key - KSYM_KP_0));
    else if (key >= KSYM_FKEY_FIRST && key <= KSYM_FKEY_LAST)
      sprintf(name_buffer, "F%d", (int)(key - KSYM_FKEY_FIRST + 1));
    else if (key == KSYM_UNDEFINED)
      strcpy(name_buffer, "(undefined)");
    else
    {
      i = 0;

      do
      {
	if (key == translate_key[i].key)
	{
	  strcpy(name_buffer, translate_key[i].name);
	  break;
	}
      }
      while (translate_key[++i].name);

      if (!translate_key[i].name)
	strcpy(name_buffer, "(unknown)");
    }

    *name = name_buffer;
  }
  else if (mode == TRANSLATE_KEYSYM_TO_X11KEYNAME)
  {
    static char name_buffer[30];
    Key key = *keysym;

    if (key >= KSYM_A && key <= KSYM_Z)
      sprintf(name_buffer, "XK_%c", 'A' + (char)(key - KSYM_A));
    else if (key >= KSYM_a && key <= KSYM_z)
      sprintf(name_buffer, "XK_%c", 'a' + (char)(key - KSYM_a));
    else if (key >= KSYM_0 && key <= KSYM_9)
      sprintf(name_buffer, "XK_%c", '0' + (char)(key - KSYM_0));
    else if (key >= KSYM_KP_0 && key <= KSYM_KP_9)
      sprintf(name_buffer, "XK_KP_%c", '0' + (char)(key - KSYM_KP_0));
    else if (key >= KSYM_FKEY_FIRST && key <= KSYM_FKEY_LAST)
      sprintf(name_buffer, "XK_F%d", (int)(key - KSYM_FKEY_FIRST + 1));
    else if (key == KSYM_UNDEFINED)
      strcpy(name_buffer, "[undefined]");
    else
    {
      i = 0;

      do
      {
	if (key == translate_key[i].key)
	{
	  strcpy(name_buffer, translate_key[i].x11name);
	  break;
	}
      }
      while (translate_key[++i].x11name);

      if (!translate_key[i].x11name)
	sprintf(name_buffer, "0x%04lx", (unsigned long)key);
    }

    *x11name = name_buffer;
  }
  else if (mode == TRANSLATE_KEYNAME_TO_KEYSYM)
  {
    Key key = KSYM_UNDEFINED;

    i = 0;
    do
    {
      if (strEqual(translate_key[i].name, *name))
      {
	key = translate_key[i].key;
	break;
      }
    }
    while (translate_key[++i].x11name);

    if (key == KSYM_UNDEFINED)
      Error(ERR_WARN, "getKeyFromKeyName(): not completely implemented");

    *keysym = key;
  }
  else if (mode == TRANSLATE_X11KEYNAME_TO_KEYSYM)
  {
    Key key = KSYM_UNDEFINED;
    char *name_ptr = *x11name;

    if (strPrefix(name_ptr, "XK_") && strlen(name_ptr) == 4)
    {
      char c = name_ptr[3];

      if (c >= 'A' && c <= 'Z')
	key = KSYM_A + (Key)(c - 'A');
      else if (c >= 'a' && c <= 'z')
	key = KSYM_a + (Key)(c - 'a');
      else if (c >= '0' && c <= '9')
	key = KSYM_0 + (Key)(c - '0');
    }
    else if (strPrefix(name_ptr, "XK_KP_") && strlen(name_ptr) == 7)
    {
      char c = name_ptr[6];

      if (c >= '0' && c <= '9')
	key = KSYM_KP_0 + (Key)(c - '0');
    }
    else if (strPrefix(name_ptr, "XK_F") && strlen(name_ptr) <= 6)
    {
      char c1 = name_ptr[4];
      char c2 = name_ptr[5];
      int d = 0;

      if ((c1 >= '0' && c1 <= '9') &&
	  ((c2 >= '0' && c1 <= '9') || c2 == '\0'))
	d = atoi(&name_ptr[4]);

      if (d >= 1 && d <= KSYM_NUM_FKEYS)
	key = KSYM_F1 + (Key)(d - 1);
    }
    else if (strPrefix(name_ptr, "XK_"))
    {
      i = 0;

      do
      {
	if (strEqual(name_ptr, translate_key[i].x11name))
	{
	  key = translate_key[i].key;
	  break;
	}
      }
      while (translate_key[++i].x11name);
    }
    else if (strPrefix(name_ptr, "0x"))
    {
      unsigned long value = 0;

      name_ptr += 2;

      while (name_ptr)
      {
	char c = *name_ptr++;
	int d = -1;

	if (c >= '0' && c <= '9')
	  d = (int)(c - '0');
	else if (c >= 'a' && c <= 'f')
	  d = (int)(c - 'a' + 10);
	else if (c >= 'A' && c <= 'F')
	  d = (int)(c - 'A' + 10);

	if (d == -1)
	{
	  value = -1;
	  break;
	}

	value = value * 16 + d;
      }

      if (value != -1)
	key = (Key)value;
    }

    *keysym = key;
  }
}

char *getKeyNameFromKey(Key key)
{
  char *name;

  translate_keyname(&key, NULL, &name, TRANSLATE_KEYSYM_TO_KEYNAME);
  return name;
}

char *getX11KeyNameFromKey(Key key)
{
  char *x11name;

  translate_keyname(&key, &x11name, NULL, TRANSLATE_KEYSYM_TO_X11KEYNAME);
  return x11name;
}

Key getKeyFromKeyName(char *name)
{
  Key key;

  translate_keyname(&key, NULL, &name, TRANSLATE_KEYNAME_TO_KEYSYM);
  return key;
}

Key getKeyFromX11KeyName(char *x11name)
{
  Key key;

  translate_keyname(&key, &x11name, NULL, TRANSLATE_X11KEYNAME_TO_KEYSYM);
  return key;
}

char getCharFromKey(Key key)
{
  char *keyname = getKeyNameFromKey(key);
  char c = 0;

  if (strlen(keyname) == 1)
    c = keyname[0];
  else if (strEqual(keyname, "space"))
    c = ' ';

  return c;
}

char getValidConfigValueChar(char c)
{
  if (c == '#' ||	/* used to mark comments */
      c == '\\')	/* used to mark continued lines */
    c = 0;

  return c;
}


/* ------------------------------------------------------------------------- */
/* functions to translate string identifiers to integer or boolean value     */
/* ------------------------------------------------------------------------- */

int get_integer_from_string(char *s)
{
  static char *number_text[][3] =
  {
    { "0",	"zero",		"null",		},
    { "1",	"one",		"first"		},
    { "2",	"two",		"second"	},
    { "3",	"three",	"third"		},
    { "4",	"four",		"fourth"	},
    { "5",	"five",		"fifth"		},
    { "6",	"six",		"sixth"		},
    { "7",	"seven",	"seventh"	},
    { "8",	"eight",	"eighth"	},
    { "9",	"nine",		"ninth"		},
    { "10",	"ten",		"tenth"		},
    { "11",	"eleven",	"eleventh"	},
    { "12",	"twelve",	"twelfth"	},

    { NULL,	NULL,		NULL		},
  };

  int i, j;
  char *s_lower = getStringToLower(s);
  int result = -1;

  for (i = 0; number_text[i][0] != NULL; i++)
    for (j = 0; j < 3; j++)
      if (strEqual(s_lower, number_text[i][j]))
	result = i;

  if (result == -1)
  {
    if (strEqual(s_lower, "false") ||
	strEqual(s_lower, "no") ||
	strEqual(s_lower, "off"))
      result = 0;
    else if (strEqual(s_lower, "true") ||
	     strEqual(s_lower, "yes") ||
	     strEqual(s_lower, "on"))
      result = 1;
    else
      result = atoi(s);
  }

  free(s_lower);

  return result;
}

boolean get_boolean_from_string(char *s)
{
  char *s_lower = getStringToLower(s);
  boolean result = FALSE;

  if (strEqual(s_lower, "true") ||
      strEqual(s_lower, "yes") ||
      strEqual(s_lower, "on") ||
      get_integer_from_string(s) == 1)
    result = TRUE;

  free(s_lower);

  return result;
}

int get_switch3_from_string(char *s)
{
  char *s_lower = getStringToLower(s);
  int result = FALSE;

  if (strEqual(s_lower, "true") ||
      strEqual(s_lower, "yes") ||
      strEqual(s_lower, "on") ||
      get_integer_from_string(s) == 1)
    result = TRUE;
  else if (strEqual(s_lower, "auto"))
    result = AUTO;

  free(s_lower);

  return result;
}


/* ------------------------------------------------------------------------- */
/* functions for generic lists                                               */
/* ------------------------------------------------------------------------- */

ListNode *newListNode()
{
  return checked_calloc(sizeof(ListNode));
}

void addNodeToList(ListNode **node_first, char *key, void *content)
{
  ListNode *node_new = newListNode();

  node_new->key = getStringCopy(key);
  node_new->content = content;
  node_new->next = *node_first;
  *node_first = node_new;
}

void deleteNodeFromList(ListNode **node_first, char *key,
			void (*destructor_function)(void *))
{
  if (node_first == NULL || *node_first == NULL)
    return;

  if (strEqual((*node_first)->key, key))
  {
    checked_free((*node_first)->key);
    if (destructor_function)
      destructor_function((*node_first)->content);
    *node_first = (*node_first)->next;
  }
  else
    deleteNodeFromList(&(*node_first)->next, key, destructor_function);
}

ListNode *getNodeFromKey(ListNode *node_first, char *key)
{
  if (node_first == NULL)
    return NULL;

  if (strEqual(node_first->key, key))
    return node_first;
  else
    return getNodeFromKey(node_first->next, key);
}

int getNumNodes(ListNode *node_first)
{
  return (node_first ? 1 + getNumNodes(node_first->next) : 0);
}

void dumpList(ListNode *node_first)
{
  ListNode *node = node_first;

  while (node)
  {
    printf("['%s' (%d)]\n", node->key,
	   ((struct ListNodeInfo *)node->content)->num_references);
    node = node->next;
  }

  printf("[%d nodes]\n", getNumNodes(node_first));
}


/* ------------------------------------------------------------------------- */
/* functions for checking files and filenames                                */
/* ------------------------------------------------------------------------- */

boolean fileExists(char *filename)
{
  if (filename == NULL)
    return FALSE;

  return (access(filename, F_OK) == 0);
}

boolean fileHasPrefix(char *basename, char *prefix)
{
  static char *basename_lower = NULL;
  int basename_length, prefix_length;

  checked_free(basename_lower);

  if (basename == NULL || prefix == NULL)
    return FALSE;

  basename_lower = getStringToLower(basename);
  basename_length = strlen(basename_lower);
  prefix_length = strlen(prefix);

  if (basename_length > prefix_length + 1 &&
      basename_lower[prefix_length] == '.' &&
      strncmp(basename_lower, prefix, prefix_length) == 0)
    return TRUE;

  return FALSE;
}

boolean fileHasSuffix(char *basename, char *suffix)
{
  static char *basename_lower = NULL;
  int basename_length, suffix_length;

  checked_free(basename_lower);

  if (basename == NULL || suffix == NULL)
    return FALSE;

  basename_lower = getStringToLower(basename);
  basename_length = strlen(basename_lower);
  suffix_length = strlen(suffix);

  if (basename_length > suffix_length + 1 &&
      basename_lower[basename_length - suffix_length - 1] == '.' &&
      strEqual(&basename_lower[basename_length - suffix_length], suffix))
    return TRUE;

  return FALSE;
}

boolean FileIsGraphic(char *filename)
{
  char *basename = getBaseNamePtr(filename);

  return fileHasSuffix(basename, "pcx");
}

boolean FileIsSound(char *filename)
{
  char *basename = getBaseNamePtr(filename);

  return fileHasSuffix(basename, "wav");
}

boolean FileIsMusic(char *filename)
{
  char *basename = getBaseNamePtr(filename);

  if (FileIsSound(basename))
    return TRUE;

#if defined(TARGET_SDL)
  if ((fileHasPrefix(basename, "mod") && !fileHasSuffix(basename, "txt")) ||
      fileHasSuffix(basename, "mod") ||
      fileHasSuffix(basename, "s3m") ||
      fileHasSuffix(basename, "it") ||
      fileHasSuffix(basename, "xm") ||
      fileHasSuffix(basename, "midi") ||
      fileHasSuffix(basename, "mid") ||
      fileHasSuffix(basename, "mp3") ||
      fileHasSuffix(basename, "ogg"))
    return TRUE;
#endif

  return FALSE;
}

boolean FileIsArtworkType(char *basename, int type)
{
  if ((type == TREE_TYPE_GRAPHICS_DIR && FileIsGraphic(basename)) ||
      (type == TREE_TYPE_SOUNDS_DIR && FileIsSound(basename)) ||
      (type == TREE_TYPE_MUSIC_DIR && FileIsMusic(basename)))
    return TRUE;

  return FALSE;
}

/* ------------------------------------------------------------------------- */
/* functions for loading artwork configuration information                   */
/* ------------------------------------------------------------------------- */

char *get_mapped_token(char *token)
{
  /* !!! make this dynamically configurable (init.c:InitArtworkConfig) !!! */
  static char *map_token_prefix[][2] =
  {
    { "char_procent",		"char_percent"	},
    { NULL,					}
  };
  int i;

  for (i = 0; map_token_prefix[i][0] != NULL; i++)
  {
    int len_token_prefix = strlen(map_token_prefix[i][0]);

    if (strncmp(token, map_token_prefix[i][0], len_token_prefix) == 0)
      return getStringCat2(map_token_prefix[i][1], &token[len_token_prefix]);
  }

  return NULL;
}

/* This function checks if a string <s> of the format "string1, string2, ..."
   exactly contains a string <s_contained>. */

static boolean string_has_parameter(char *s, char *s_contained)
{
  char *substring;

  if (s == NULL || s_contained == NULL)
    return FALSE;

  if (strlen(s_contained) > strlen(s))
    return FALSE;

  if (strncmp(s, s_contained, strlen(s_contained)) == 0)
  {
    char next_char = s[strlen(s_contained)];

    /* check if next character is delimiter or whitespace */
    return (next_char == ',' || next_char == '\0' ||
	    next_char == ' ' || next_char == '\t' ? TRUE : FALSE);
  }

  /* check if string contains another parameter string after a comma */
  substring = strchr(s, ',');
  if (substring == NULL)	/* string does not contain a comma */
    return FALSE;

  /* advance string pointer to next character after the comma */
  substring++;

  /* skip potential whitespaces after the comma */
  while (*substring == ' ' || *substring == '\t')
    substring++;

  return string_has_parameter(substring, s_contained);
}

int get_parameter_value(char *value_raw, char *suffix, int type)
{
  char *value = getStringToLower(value_raw);
  int result = 0;	/* probably a save default value */

  if (strEqual(suffix, ".direction"))
  {
    result = (strEqual(value, "left")  ? MV_LEFT :
	      strEqual(value, "right") ? MV_RIGHT :
	      strEqual(value, "up")    ? MV_UP :
	      strEqual(value, "down")  ? MV_DOWN : MV_NONE);
  }
  else if (strEqual(suffix, ".align"))
  {
    result = (strEqual(value, "left")   ? ALIGN_LEFT :
	      strEqual(value, "right")  ? ALIGN_RIGHT :
	      strEqual(value, "center") ? ALIGN_CENTER :
	      strEqual(value, "middle") ? ALIGN_CENTER : ALIGN_DEFAULT);
  }
  else if (strEqual(suffix, ".valign"))
  {
    result = (strEqual(value, "top")    ? VALIGN_TOP :
	      strEqual(value, "bottom") ? VALIGN_BOTTOM :
	      strEqual(value, "middle") ? VALIGN_MIDDLE :
	      strEqual(value, "center") ? VALIGN_MIDDLE : VALIGN_DEFAULT);
  }
  else if (strEqual(suffix, ".anim_mode"))
  {
    result = (string_has_parameter(value, "none")	? ANIM_NONE :
	      string_has_parameter(value, "loop")	? ANIM_LOOP :
	      string_has_parameter(value, "linear")	? ANIM_LINEAR :
	      string_has_parameter(value, "pingpong")	? ANIM_PINGPONG :
	      string_has_parameter(value, "pingpong2")	? ANIM_PINGPONG2 :
	      string_has_parameter(value, "random")	? ANIM_RANDOM :
	      string_has_parameter(value, "ce_value")	? ANIM_CE_VALUE :
	      string_has_parameter(value, "ce_score")	? ANIM_CE_SCORE :
	      string_has_parameter(value, "ce_delay")	? ANIM_CE_DELAY :
	      string_has_parameter(value, "horizontal")	? ANIM_HORIZONTAL :
	      string_has_parameter(value, "vertical")	? ANIM_VERTICAL :
	      string_has_parameter(value, "centered")	? ANIM_CENTERED :
	      ANIM_DEFAULT);

    if (string_has_parameter(value, "reverse"))
      result |= ANIM_REVERSE;

    if (string_has_parameter(value, "opaque_player"))
      result |= ANIM_OPAQUE_PLAYER;

    if (string_has_parameter(value, "static_panel"))
      result |= ANIM_STATIC_PANEL;
  }
  else if (strEqual(suffix, ".class"))
  {
    result = get_hash_from_key(value);
  }
  else if (strEqual(suffix, ".style"))
  {
    result = STYLE_DEFAULT;

    if (string_has_parameter(value, "accurate_borders"))
      result |= STYLE_ACCURATE_BORDERS;

    if (string_has_parameter(value, "inner_corners"))
      result |= STYLE_INNER_CORNERS;
  }
  else if (strEqual(suffix, ".fade_mode"))
  {
    result = (string_has_parameter(value, "none")	? FADE_MODE_NONE :
	      string_has_parameter(value, "fade")	? FADE_MODE_FADE :
	      string_has_parameter(value, "crossfade")	? FADE_MODE_CROSSFADE :
	      string_has_parameter(value, "melt")	? FADE_MODE_MELT :
	      FADE_MODE_DEFAULT);
  }
#if 1
  else if (strPrefix(suffix, ".font"))		/* (may also be ".font_xyz") */
#else
  else if (strEqualN(suffix, ".font", 5))	/* (may also be ".font_xyz") */
#endif
  {
    result = gfx.get_font_from_token_function(value);
  }
  else		/* generic parameter of type integer or boolean */
  {
    result = (strEqual(value, ARG_UNDEFINED) ? ARG_UNDEFINED_VALUE :
	      type == TYPE_INTEGER ? get_integer_from_string(value) :
	      type == TYPE_BOOLEAN ? get_boolean_from_string(value) :
	      ARG_UNDEFINED_VALUE);
  }

  free(value);

  return result;
}

struct ScreenModeInfo *get_screen_mode_from_string(char *screen_mode_string)
{
  static struct ScreenModeInfo screen_mode;
  char *screen_mode_string_x = strchr(screen_mode_string, 'x');
  char *screen_mode_string_copy;
  char *screen_mode_string_pos_w;
  char *screen_mode_string_pos_h;

  if (screen_mode_string_x == NULL)	/* invalid screen mode format */
    return NULL;

  screen_mode_string_copy = getStringCopy(screen_mode_string);

  screen_mode_string_pos_w = screen_mode_string_copy;
  screen_mode_string_pos_h = strchr(screen_mode_string_copy, 'x');
  *screen_mode_string_pos_h++ = '\0';

  screen_mode.width  = atoi(screen_mode_string_pos_w);
  screen_mode.height = atoi(screen_mode_string_pos_h);

  return &screen_mode;
}

void get_aspect_ratio_from_screen_mode(struct ScreenModeInfo *screen_mode,
				       int *x, int *y)
{
  float aspect_ratio = (float)screen_mode->width / (float)screen_mode->height;
  float aspect_ratio_new;
  int i = 1;

  do
  {
    *x = i * aspect_ratio + 0.000001;
    *y = i;

    aspect_ratio_new = (float)*x / (float)*y;

    i++;
  }
  while (aspect_ratio_new != aspect_ratio && *y < screen_mode->height);
}

static void FreeCustomArtworkList(struct ArtworkListInfo *,
				  struct ListNodeInfo ***, int *);

struct FileInfo *getFileListFromConfigList(struct ConfigInfo *config_list,
					   struct ConfigTypeInfo *suffix_list,
					   char **ignore_tokens,
					   int num_file_list_entries)
{
  struct FileInfo *file_list;
  int num_file_list_entries_found = 0;
  int num_suffix_list_entries = 0;
  int list_pos;
  int i, j;

  file_list = checked_calloc(num_file_list_entries * sizeof(struct FileInfo));

  for (i = 0; suffix_list[i].token != NULL; i++)
    num_suffix_list_entries++;

  /* always start with reliable default values */
  for (i = 0; i < num_file_list_entries; i++)
  {
    file_list[i].token = NULL;

    file_list[i].default_filename = NULL;
    file_list[i].filename = NULL;

    if (num_suffix_list_entries > 0)
    {
      int parameter_array_size = num_suffix_list_entries * sizeof(char *);

      file_list[i].default_parameter = checked_calloc(parameter_array_size);
      file_list[i].parameter = checked_calloc(parameter_array_size);

      for (j = 0; j < num_suffix_list_entries; j++)
      {
	setString(&file_list[i].default_parameter[j], suffix_list[j].value);
	setString(&file_list[i].parameter[j], suffix_list[j].value);
      }

      file_list[i].redefined = FALSE;
      file_list[i].fallback_to_default = FALSE;
      file_list[i].default_is_cloned = FALSE;
    }
  }

  list_pos = 0;
  for (i = 0; config_list[i].token != NULL; i++)
  {
    int len_config_token = strlen(config_list[i].token);
    int len_config_value = strlen(config_list[i].value);
    boolean is_file_entry = TRUE;

    for (j = 0; suffix_list[j].token != NULL; j++)
    {
      int len_suffix = strlen(suffix_list[j].token);

      if (len_suffix < len_config_token &&
	  strEqual(&config_list[i].token[len_config_token - len_suffix],
		   suffix_list[j].token))
      {
	setString(&file_list[list_pos].default_parameter[j],
		  config_list[i].value);

	is_file_entry = FALSE;
	break;
      }
    }

    /* the following tokens are no file definitions, but other config tokens */
    for (j = 0; ignore_tokens[j] != NULL; j++)
      if (strEqual(config_list[i].token, ignore_tokens[j]))
	is_file_entry = FALSE;

    if (is_file_entry)
    {
      if (i > 0)
	list_pos++;

      if (list_pos >= num_file_list_entries)
	break;

      /* simple sanity check if this is really a file definition */
      if (!strEqual(&config_list[i].value[len_config_value - 4], ".pcx") &&
	  !strEqual(&config_list[i].value[len_config_value - 4], ".wav") &&
	  !strEqual(config_list[i].value, UNDEFINED_FILENAME))
      {
	Error(ERR_INFO, "Configuration directive '%s' -> '%s':",
	      config_list[i].token, config_list[i].value);
	Error(ERR_EXIT, "This seems to be no valid definition -- please fix");
      }

      file_list[list_pos].token = config_list[i].token;
      file_list[list_pos].default_filename = config_list[i].value;

#if 0
      printf("::: '%s' => '%s'\n", config_list[i].token, config_list[i].value);
#endif
    }

    if (strSuffix(config_list[i].token, ".clone_from"))
      file_list[list_pos].default_is_cloned = TRUE;
  }

  num_file_list_entries_found = list_pos + 1;
  if (num_file_list_entries_found != num_file_list_entries)
  {
    Error(ERR_INFO_LINE, "-");
    Error(ERR_INFO, "inconsistant config list information:");
    Error(ERR_INFO, "- should be:   %d (according to 'src/conf_xxx.h')",
	  num_file_list_entries);
    Error(ERR_INFO, "- found to be: %d (according to 'src/conf_xxx.c')",
	  num_file_list_entries_found);
    Error(ERR_EXIT,   "please fix");
  }

#if 0
  printf("::: ---------- DONE ----------\n");
#endif

  return file_list;
}

static boolean token_suffix_match(char *token, char *suffix, int start_pos)
{
  int len_token = strlen(token);
  int len_suffix = strlen(suffix);

  if (start_pos < 0)	/* compare suffix from end of string */
    start_pos += len_token;

  if (start_pos < 0 || start_pos + len_suffix > len_token)
    return FALSE;

  if (strncmp(&token[start_pos], suffix, len_suffix) != 0)
    return FALSE;

  if (token[start_pos + len_suffix] == '\0')
    return TRUE;

  if (token[start_pos + len_suffix] == '.')
    return TRUE;

  return FALSE;
}

#define KNOWN_TOKEN_VALUE	"[KNOWN_TOKEN_VALUE]"

static void read_token_parameters(SetupFileHash *setup_file_hash,
				  struct ConfigTypeInfo *suffix_list,
				  struct FileInfo *file_list_entry)
{
  /* check for config token that is the base token without any suffixes */
  char *filename = getHashEntry(setup_file_hash, file_list_entry->token);
  char *known_token_value = KNOWN_TOKEN_VALUE;
  int i;

  if (filename != NULL)
  {
    setString(&file_list_entry->filename, filename);

    /* when file definition found, set all parameters to default values */
    for (i = 0; suffix_list[i].token != NULL; i++)
      setString(&file_list_entry->parameter[i], suffix_list[i].value);

    file_list_entry->redefined = TRUE;

    /* mark config file token as well known from default config */
    setHashEntry(setup_file_hash, file_list_entry->token, known_token_value);
  }

  /* check for config tokens that can be build by base token and suffixes */
  for (i = 0; suffix_list[i].token != NULL; i++)
  {
    char *token = getStringCat2(file_list_entry->token, suffix_list[i].token);
    char *value = getHashEntry(setup_file_hash, token);

    if (value != NULL)
    {
      setString(&file_list_entry->parameter[i], value);

      /* mark config file token as well known from default config */
      setHashEntry(setup_file_hash, token, known_token_value);
    }

    free(token);
  }
}

static void add_dynamic_file_list_entry(struct FileInfo **list,
					int *num_list_entries,
					SetupFileHash *extra_file_hash,
					struct ConfigTypeInfo *suffix_list,
					int num_suffix_list_entries,
					char *token)
{
  struct FileInfo *new_list_entry;
  int parameter_array_size = num_suffix_list_entries * sizeof(char *);

  (*num_list_entries)++;
  *list = checked_realloc(*list, *num_list_entries * sizeof(struct FileInfo));
  new_list_entry = &(*list)[*num_list_entries - 1];

  new_list_entry->token = getStringCopy(token);
  new_list_entry->default_filename = NULL;
  new_list_entry->filename = NULL;
  new_list_entry->parameter = checked_calloc(parameter_array_size);

  new_list_entry->redefined = FALSE;
  new_list_entry->fallback_to_default = FALSE;
  new_list_entry->default_is_cloned = FALSE;

  read_token_parameters(extra_file_hash, suffix_list, new_list_entry);
}

static void add_property_mapping(struct PropertyMapping **list,
				 int *num_list_entries,
				 int base_index, int ext1_index,
				 int ext2_index, int ext3_index,
				 int artwork_index)
{
  struct PropertyMapping *new_list_entry;

  (*num_list_entries)++;
  *list = checked_realloc(*list,
			  *num_list_entries * sizeof(struct PropertyMapping));
  new_list_entry = &(*list)[*num_list_entries - 1];

  new_list_entry->base_index = base_index;
  new_list_entry->ext1_index = ext1_index;
  new_list_entry->ext2_index = ext2_index;
  new_list_entry->ext3_index = ext3_index;

  new_list_entry->artwork_index = artwork_index;
}

static void LoadArtworkConfigFromFilename(struct ArtworkListInfo *artwork_info,
					  char *filename)
{
  struct FileInfo *file_list = artwork_info->file_list;
  struct ConfigTypeInfo *suffix_list = artwork_info->suffix_list;
  char **base_prefixes = artwork_info->base_prefixes;
  char **ext1_suffixes = artwork_info->ext1_suffixes;
  char **ext2_suffixes = artwork_info->ext2_suffixes;
  char **ext3_suffixes = artwork_info->ext3_suffixes;
  char **ignore_tokens = artwork_info->ignore_tokens;
  int num_file_list_entries = artwork_info->num_file_list_entries;
  int num_suffix_list_entries = artwork_info->num_suffix_list_entries;
  int num_base_prefixes = artwork_info->num_base_prefixes;
  int num_ext1_suffixes = artwork_info->num_ext1_suffixes;
  int num_ext2_suffixes = artwork_info->num_ext2_suffixes;
  int num_ext3_suffixes = artwork_info->num_ext3_suffixes;
  int num_ignore_tokens = artwork_info->num_ignore_tokens;
  SetupFileHash *setup_file_hash, *valid_file_hash;
  SetupFileHash *extra_file_hash, *empty_file_hash;
  char *known_token_value = KNOWN_TOKEN_VALUE;
  int i, j, k, l;

  if (filename == NULL)
    return;

#if 0
  printf("LoadArtworkConfigFromFilename '%s' ...\n", filename);
#endif

  if ((setup_file_hash = loadSetupFileHash(filename)) == NULL)
    return;

  /* separate valid (defined) from empty (undefined) config token values */
  valid_file_hash = newSetupFileHash();
  empty_file_hash = newSetupFileHash();
  BEGIN_HASH_ITERATION(setup_file_hash, itr)
  {
    char *value = HASH_ITERATION_VALUE(itr);

    setHashEntry(*value ? valid_file_hash : empty_file_hash,
		 HASH_ITERATION_TOKEN(itr), value);
  }
  END_HASH_ITERATION(setup_file_hash, itr)

  /* at this point, we do not need the setup file hash anymore -- free it */
  freeSetupFileHash(setup_file_hash);

  /* map deprecated to current tokens (using prefix match and replace) */
  BEGIN_HASH_ITERATION(valid_file_hash, itr)
  {
    char *token = HASH_ITERATION_TOKEN(itr);
    char *mapped_token = get_mapped_token(token);

    if (mapped_token != NULL)
    {
      char *value = HASH_ITERATION_VALUE(itr);

      /* add mapped token */
      setHashEntry(valid_file_hash, mapped_token, value);

      /* ignore old token (by setting it to "known" keyword) */
      setHashEntry(valid_file_hash, token, known_token_value);

      free(mapped_token);
    }
  }
  END_HASH_ITERATION(valid_file_hash, itr)

  /* read parameters for all known config file tokens */
  for (i = 0; i < num_file_list_entries; i++)
    read_token_parameters(valid_file_hash, suffix_list, &file_list[i]);

  /* set all tokens that can be ignored here to "known" keyword */
  for (i = 0; i < num_ignore_tokens; i++)
    setHashEntry(valid_file_hash, ignore_tokens[i], known_token_value);

  /* copy all unknown config file tokens to extra config hash */
  extra_file_hash = newSetupFileHash();
  BEGIN_HASH_ITERATION(valid_file_hash, itr)
  {
    char *value = HASH_ITERATION_VALUE(itr);

    if (!strEqual(value, known_token_value))
      setHashEntry(extra_file_hash, HASH_ITERATION_TOKEN(itr), value);
  }
  END_HASH_ITERATION(valid_file_hash, itr)

  /* at this point, we do not need the valid file hash anymore -- free it */
  freeSetupFileHash(valid_file_hash);

  /* now try to determine valid, dynamically defined config tokens */

  BEGIN_HASH_ITERATION(extra_file_hash, itr)
  {
    struct FileInfo **dynamic_file_list =
      &artwork_info->dynamic_file_list;
    int *num_dynamic_file_list_entries =
      &artwork_info->num_dynamic_file_list_entries;
    struct PropertyMapping **property_mapping =
      &artwork_info->property_mapping;
    int *num_property_mapping_entries =
      &artwork_info->num_property_mapping_entries;
    int current_summarized_file_list_entry =
      artwork_info->num_file_list_entries +
      artwork_info->num_dynamic_file_list_entries;
    char *token = HASH_ITERATION_TOKEN(itr);
    int len_token = strlen(token);
    int start_pos;
    boolean base_prefix_found = FALSE;
    boolean parameter_suffix_found = FALSE;

#if 0
    printf("::: examining '%s' -> '%s'\n", token, HASH_ITERATION_VALUE(itr));
#endif

    /* skip all parameter definitions (handled by read_token_parameters()) */
    for (i = 0; i < num_suffix_list_entries && !parameter_suffix_found; i++)
    {
      int len_suffix = strlen(suffix_list[i].token);

      if (token_suffix_match(token, suffix_list[i].token, -len_suffix))
	parameter_suffix_found = TRUE;
    }

    if (parameter_suffix_found)
      continue;

    /* ---------- step 0: search for matching base prefix ---------- */

    start_pos = 0;
    for (i = 0; i < num_base_prefixes && !base_prefix_found; i++)
    {
      char *base_prefix = base_prefixes[i];
      int len_base_prefix = strlen(base_prefix);
      boolean ext1_suffix_found = FALSE;
      boolean ext2_suffix_found = FALSE;
      boolean ext3_suffix_found = FALSE;
      boolean exact_match = FALSE;
      int base_index = -1;
      int ext1_index = -1;
      int ext2_index = -1;
      int ext3_index = -1;

      base_prefix_found = token_suffix_match(token, base_prefix, start_pos);

      if (!base_prefix_found)
	continue;

      base_index = i;

#if 0
      if (IS_PARENT_PROCESS())
	printf("===> MATCH: '%s', '%s'\n", token, base_prefix);
#endif

      if (start_pos + len_base_prefix == len_token)	/* exact match */
      {
	exact_match = TRUE;

#if 0
	if (IS_PARENT_PROCESS())
	  printf("===> EXACT MATCH: '%s', '%s'\n", token, base_prefix);
#endif

	add_dynamic_file_list_entry(dynamic_file_list,
				    num_dynamic_file_list_entries,
				    extra_file_hash,
				    suffix_list,
				    num_suffix_list_entries,
				    token);
	add_property_mapping(property_mapping,
			     num_property_mapping_entries,
			     base_index, -1, -1, -1,
			     current_summarized_file_list_entry);
	continue;
      }

#if 0
      if (IS_PARENT_PROCESS())
	printf("---> examining token '%s': search 1st suffix ...\n", token);
#endif

      /* ---------- step 1: search for matching first suffix ---------- */

      start_pos += len_base_prefix;
      for (j = 0; j < num_ext1_suffixes && !ext1_suffix_found; j++)
      {
	char *ext1_suffix = ext1_suffixes[j];
	int len_ext1_suffix = strlen(ext1_suffix);

	ext1_suffix_found = token_suffix_match(token, ext1_suffix, start_pos);

	if (!ext1_suffix_found)
	  continue;

	ext1_index = j;

#if 0
	if (IS_PARENT_PROCESS())
	  printf("===> MATCH: '%s', '%s'\n", token, ext1_suffix);
#endif

	if (start_pos + len_ext1_suffix == len_token)	/* exact match */
	{
	  exact_match = TRUE;

#if 0
	if (IS_PARENT_PROCESS())
	  printf("===> EXACT MATCH: '%s', '%s'\n", token, ext1_suffix);
#endif

	  add_dynamic_file_list_entry(dynamic_file_list,
				      num_dynamic_file_list_entries,
				      extra_file_hash,
				      suffix_list,
				      num_suffix_list_entries,
				      token);
	  add_property_mapping(property_mapping,
			       num_property_mapping_entries,
			       base_index, ext1_index, -1, -1,
			       current_summarized_file_list_entry);
	  continue;
	}

	start_pos += len_ext1_suffix;
      }

      if (exact_match)
	break;

#if 0
      if (IS_PARENT_PROCESS())
	printf("---> examining token '%s': search 2nd suffix ...\n", token);
#endif

      /* ---------- step 2: search for matching second suffix ---------- */

      for (k = 0; k < num_ext2_suffixes && !ext2_suffix_found; k++)
      {
	char *ext2_suffix = ext2_suffixes[k];
	int len_ext2_suffix = strlen(ext2_suffix);

	ext2_suffix_found = token_suffix_match(token, ext2_suffix, start_pos);

	if (!ext2_suffix_found)
	  continue;

	ext2_index = k;

#if 0
	if (IS_PARENT_PROCESS())
	  printf("===> MATCH: '%s', '%s'\n", token, ext2_suffix);
#endif

	if (start_pos + len_ext2_suffix == len_token)	/* exact match */
	{
	  exact_match = TRUE;

#if 0
	  if (IS_PARENT_PROCESS())
	    printf("===> EXACT MATCH: '%s', '%s'\n", token, ext2_suffix);
#endif

	  add_dynamic_file_list_entry(dynamic_file_list,
				      num_dynamic_file_list_entries,
				      extra_file_hash,
				      suffix_list,
				      num_suffix_list_entries,
				      token);
	  add_property_mapping(property_mapping,
			       num_property_mapping_entries,
			       base_index, ext1_index, ext2_index, -1,
			       current_summarized_file_list_entry);
	  continue;
	}

	start_pos += len_ext2_suffix;
      }

      if (exact_match)
	break;

#if 0
      if (IS_PARENT_PROCESS())
	printf("---> examining token '%s': search 3rd suffix ...\n",token);
#endif

      /* ---------- step 3: search for matching third suffix ---------- */

      for (l = 0; l < num_ext3_suffixes && !ext3_suffix_found; l++)
      {
	char *ext3_suffix = ext3_suffixes[l];
	int len_ext3_suffix = strlen(ext3_suffix);

	ext3_suffix_found = token_suffix_match(token, ext3_suffix, start_pos);

	if (!ext3_suffix_found)
	  continue;

	ext3_index = l;

#if 0
	if (IS_PARENT_PROCESS())
	  printf("===> MATCH: '%s', '%s'\n", token, ext3_suffix);
#endif

	if (start_pos + len_ext3_suffix == len_token) /* exact match */
	{
	  exact_match = TRUE;

#if 0
	  if (IS_PARENT_PROCESS())
	    printf("===> EXACT MATCH: '%s', '%s'\n", token, ext3_suffix);
#endif

	  add_dynamic_file_list_entry(dynamic_file_list,
				      num_dynamic_file_list_entries,
				      extra_file_hash,
				      suffix_list,
				      num_suffix_list_entries,
				      token);
	  add_property_mapping(property_mapping,
			       num_property_mapping_entries,
			       base_index, ext1_index, ext2_index, ext3_index,
			       current_summarized_file_list_entry);
	  continue;
	}
      }
    }
  }
  END_HASH_ITERATION(extra_file_hash, itr)

  if (artwork_info->num_dynamic_file_list_entries > 0)
  {
    artwork_info->dynamic_artwork_list =
      checked_calloc(artwork_info->num_dynamic_file_list_entries *
		     artwork_info->sizeof_artwork_list_entry);
  }

  if (options.verbose && IS_PARENT_PROCESS())
  {
    SetupFileList *setup_file_list, *list;
    boolean dynamic_tokens_found = FALSE;
    boolean unknown_tokens_found = FALSE;
    boolean undefined_values_found = (hashtable_count(empty_file_hash) != 0);

    if ((setup_file_list = loadSetupFileList(filename)) == NULL)
      Error(ERR_EXIT, "loadSetupFileHash works, but loadSetupFileList fails");

    BEGIN_HASH_ITERATION(extra_file_hash, itr)
    {
      if (strEqual(HASH_ITERATION_VALUE(itr), known_token_value))
	dynamic_tokens_found = TRUE;
      else
	unknown_tokens_found = TRUE;
    }
    END_HASH_ITERATION(extra_file_hash, itr)

    if (options.debug && dynamic_tokens_found)
    {
      Error(ERR_INFO_LINE, "-");
      Error(ERR_INFO, "dynamic token(s) found in config file:");
      Error(ERR_INFO, "- config file: '%s'", filename);

      for (list = setup_file_list; list != NULL; list = list->next)
      {
	char *value = getHashEntry(extra_file_hash, list->token);

	if (value != NULL && strEqual(value, known_token_value))
	  Error(ERR_INFO, "- dynamic token: '%s'", list->token);
      }

      Error(ERR_INFO_LINE, "-");
    }

    if (unknown_tokens_found)
    {
      Error(ERR_INFO_LINE, "-");
      Error(ERR_INFO, "warning: unknown token(s) found in config file:");
      Error(ERR_INFO, "- config file: '%s'", filename);

      for (list = setup_file_list; list != NULL; list = list->next)
      {
	char *value = getHashEntry(extra_file_hash, list->token);

	if (value != NULL && !strEqual(value, known_token_value))
	  Error(ERR_INFO, "- dynamic token: '%s'", list->token);
      }

      Error(ERR_INFO_LINE, "-");
    }

    if (undefined_values_found)
    {
      Error(ERR_INFO_LINE, "-");
      Error(ERR_INFO, "warning: undefined values found in config file:");
      Error(ERR_INFO, "- config file: '%s'", filename);

      for (list = setup_file_list; list != NULL; list = list->next)
      {
	char *value = getHashEntry(empty_file_hash, list->token);

	if (value != NULL)
	  Error(ERR_INFO, "- undefined value for token: '%s'", list->token);
      }

      Error(ERR_INFO_LINE, "-");
    }

    freeSetupFileList(setup_file_list);
  }

  freeSetupFileHash(extra_file_hash);
  freeSetupFileHash(empty_file_hash);

#if 0
  for (i = 0; i < num_file_list_entries; i++)
  {
    printf("'%s' ", file_list[i].token);
    if (file_list[i].filename)
      printf("-> '%s'\n", file_list[i].filename);
    else
      printf("-> UNDEFINED [-> '%s']\n", file_list[i].default_filename);
  }
#endif
}

void LoadArtworkConfig(struct ArtworkListInfo *artwork_info)
{
  struct FileInfo *file_list = artwork_info->file_list;
  int num_file_list_entries = artwork_info->num_file_list_entries;
  int num_suffix_list_entries = artwork_info->num_suffix_list_entries;
  char *filename_base = UNDEFINED_FILENAME, *filename_local;
  int i, j;

  DrawInitText("Loading artwork config", 120, FC_GREEN);
  DrawInitText(ARTWORKINFO_FILENAME(artwork_info->type), 150, FC_YELLOW);

  /* always start with reliable default values */
  for (i = 0; i < num_file_list_entries; i++)
  {
    setString(&file_list[i].filename, file_list[i].default_filename);

    for (j = 0; j < num_suffix_list_entries; j++)
      setString(&file_list[i].parameter[j], file_list[i].default_parameter[j]);

    file_list[i].redefined = FALSE;
    file_list[i].fallback_to_default = FALSE;
  }

  /* free previous dynamic artwork file array */
  if (artwork_info->dynamic_file_list != NULL)
  {
    for (i = 0; i < artwork_info->num_dynamic_file_list_entries; i++)
    {
      free(artwork_info->dynamic_file_list[i].token);
      free(artwork_info->dynamic_file_list[i].filename);
      free(artwork_info->dynamic_file_list[i].parameter);
    }

    free(artwork_info->dynamic_file_list);
    artwork_info->dynamic_file_list = NULL;

    FreeCustomArtworkList(artwork_info, &artwork_info->dynamic_artwork_list,
			  &artwork_info->num_dynamic_file_list_entries);
  }

  /* free previous property mapping */
  if (artwork_info->property_mapping != NULL)
  {
    free(artwork_info->property_mapping);

    artwork_info->property_mapping = NULL;
    artwork_info->num_property_mapping_entries = 0;
  }

#if 1
  if (!GFX_OVERRIDE_ARTWORK(artwork_info->type))
#else
  if (!SETUP_OVERRIDE_ARTWORK(setup, artwork_info->type))
#endif
  {
    /* first look for special artwork configured in level series config */
    filename_base = getCustomArtworkLevelConfigFilename(artwork_info->type);

#if 0
    printf("::: filename_base == '%s' [%s, %s]\n", filename_base,
	   leveldir_current->graphics_set,
	   leveldir_current->graphics_path);
#endif

    if (fileExists(filename_base))
      LoadArtworkConfigFromFilename(artwork_info, filename_base);
  }

  filename_local = getCustomArtworkConfigFilename(artwork_info->type);

  if (filename_local != NULL && !strEqual(filename_base, filename_local))
    LoadArtworkConfigFromFilename(artwork_info, filename_local);
}

static void deleteArtworkListEntry(struct ArtworkListInfo *artwork_info,
				   struct ListNodeInfo **listnode)
{
  if (*listnode)
  {
    char *filename = (*listnode)->source_filename;

    if (--(*listnode)->num_references <= 0)
      deleteNodeFromList(&artwork_info->content_list, filename,
			 artwork_info->free_artwork);

    *listnode = NULL;
  }
}

static void replaceArtworkListEntry(struct ArtworkListInfo *artwork_info,
				    struct ListNodeInfo **listnode,
				    struct FileInfo *file_list_entry)
{
  char *init_text[] =
  {
    "Loading graphics",
    "Loading sounds",
    "Loading music"
  };

  ListNode *node;
  char *basename = file_list_entry->filename;
  char *filename = getCustomArtworkFilename(basename, artwork_info->type);

  if (filename == NULL)
  {
    Error(ERR_WARN, "cannot find artwork file '%s'", basename);

    basename = file_list_entry->default_filename;

    /* fail for cloned default artwork that has no default filename defined */
    if (file_list_entry->default_is_cloned &&
	strEqual(basename, UNDEFINED_FILENAME))
    {
      int error_mode = ERR_WARN;

      /* we can get away without sounds and music, but not without graphics */
      if (*listnode == NULL && artwork_info->type == ARTWORK_TYPE_GRAPHICS)
	error_mode = ERR_EXIT;

      Error(error_mode, "token '%s' was cloned and has no default filename",
	    file_list_entry->token);

      return;
    }

    /* dynamic artwork has no default filename / skip empty default artwork */
    if (basename == NULL || strEqual(basename, UNDEFINED_FILENAME))
      return;

    file_list_entry->fallback_to_default = TRUE;

    Error(ERR_WARN, "trying default artwork file '%s'", basename);

    filename = getCustomArtworkFilename(basename, artwork_info->type);

    if (filename == NULL)
    {
      int error_mode = ERR_WARN;

      /* we can get away without sounds and music, but not without graphics */
      if (*listnode == NULL && artwork_info->type == ARTWORK_TYPE_GRAPHICS)
	error_mode = ERR_EXIT;

      Error(error_mode, "cannot find default artwork file '%s'", basename);

      return;
    }
  }

  /* check if the old and the new artwork file are the same */
  if (*listnode && strEqual((*listnode)->source_filename, filename))
  {
    /* The old and new artwork are the same (have the same filename and path).
       This usually means that this artwork does not exist in this artwork set
       and a fallback to the existing artwork is done. */

#if 0
    printf("[artwork '%s' already exists (same list entry)]\n", filename);
#endif

    return;
  }

  /* delete existing artwork file entry */
  deleteArtworkListEntry(artwork_info, listnode);

  /* check if the new artwork file already exists in the list of artworks */
  if ((node = getNodeFromKey(artwork_info->content_list, filename)) != NULL)
  {
#if 0
      printf("[artwork '%s' already exists (other list entry)]\n", filename);
#endif

      *listnode = (struct ListNodeInfo *)node->content;
      (*listnode)->num_references++;

      return;
  }

  DrawInitText(init_text[artwork_info->type], 120, FC_GREEN);
  DrawInitText(basename, 150, FC_YELLOW);

  if ((*listnode = artwork_info->load_artwork(filename)) != NULL)
  {
#if 0
      printf("[adding new artwork '%s']\n", filename);
#endif

    (*listnode)->num_references = 1;
    addNodeToList(&artwork_info->content_list, (*listnode)->source_filename,
		  *listnode);
  }
  else
  {
    int error_mode = ERR_WARN;

    /* we can get away without sounds and music, but not without graphics */
    if (artwork_info->type == ARTWORK_TYPE_GRAPHICS)
      error_mode = ERR_EXIT;

    Error(error_mode, "cannot load artwork file '%s'", basename);

    return;
  }
}

static void LoadCustomArtwork(struct ArtworkListInfo *artwork_info,
			      struct ListNodeInfo **listnode,
			      struct FileInfo *file_list_entry)
{
#if 0
  printf("GOT CUSTOM ARTWORK FILE '%s'\n", file_list_entry->filename);
#endif

  if (strEqual(file_list_entry->filename, UNDEFINED_FILENAME))
  {
    deleteArtworkListEntry(artwork_info, listnode);
    return;
  }

  replaceArtworkListEntry(artwork_info, listnode, file_list_entry);
}

void ReloadCustomArtworkList(struct ArtworkListInfo *artwork_info)
{
  struct FileInfo *file_list = artwork_info->file_list;
  struct FileInfo *dynamic_file_list = artwork_info->dynamic_file_list;
  int num_file_list_entries = artwork_info->num_file_list_entries;
  int num_dynamic_file_list_entries =
    artwork_info->num_dynamic_file_list_entries;
  int i;

  for (i = 0; i < num_file_list_entries; i++)
    LoadCustomArtwork(artwork_info, &artwork_info->artwork_list[i],
		      &file_list[i]);

  for (i = 0; i < num_dynamic_file_list_entries; i++)
    LoadCustomArtwork(artwork_info, &artwork_info->dynamic_artwork_list[i],
		      &dynamic_file_list[i]);

#if 0
  dumpList(artwork_info->content_list);
#endif
}

static void FreeCustomArtworkList(struct ArtworkListInfo *artwork_info,
				  struct ListNodeInfo ***list,
				  int *num_list_entries)
{
  int i;

  if (*list == NULL)
    return;

  for (i = 0; i < *num_list_entries; i++)
    deleteArtworkListEntry(artwork_info, &(*list)[i]);
  free(*list);

  *list = NULL;
  *num_list_entries = 0;
}

void FreeCustomArtworkLists(struct ArtworkListInfo *artwork_info)
{
  if (artwork_info == NULL)
    return;

  FreeCustomArtworkList(artwork_info, &artwork_info->artwork_list,
			&artwork_info->num_file_list_entries);

  FreeCustomArtworkList(artwork_info, &artwork_info->dynamic_artwork_list,
			&artwork_info->num_dynamic_file_list_entries);
}


/* ------------------------------------------------------------------------- */
/* functions only needed for non-Unix (non-command-line) systems             */
/* (MS-DOS only; SDL/Windows creates files "stdout.txt" and "stderr.txt")    */
/* (now also added for Windows, to create files in user data directory)      */
/* ------------------------------------------------------------------------- */

char *getErrorFilename(char *basename)
{
  return getPath2(getUserGameDataDir(), basename);
}

void openErrorFile()
{
  InitUserDataDirectory();

  if ((program.error_file = fopen(program.error_filename, MODE_WRITE)) == NULL)
    fprintf_newline(stderr, "ERROR: cannot open file '%s' for writing!",
		    program.error_filename);
}

void closeErrorFile()
{
  if (program.error_file != stderr)	/* do not close stream 'stderr' */
    fclose(program.error_file);
}

void dumpErrorFile()
{
  FILE *error_file = fopen(program.error_filename, MODE_READ);

  if (error_file != NULL)
  {
    while (!feof(error_file))
      fputc(fgetc(error_file), stderr);

    fclose(error_file);
  }
}

void NotifyUserAboutErrorFile()
{
#if defined(PLATFORM_WIN32)
  char *title_text = getStringCat2(program.program_title, " Error Message");
  char *error_text = getStringCat2("The program was aborted due to an error; "
				   "for details, see the following error file:"
				   STRING_NEWLINE, program.error_filename);

  MessageBox(NULL, error_text, title_text, MB_OK);
#endif
}


/* ------------------------------------------------------------------------- */
/* the following is only for debugging purpose and normally not used         */
/* ------------------------------------------------------------------------- */

#if DEBUG

#define DEBUG_NUM_TIMESTAMPS		5
#define DEBUG_TIME_IN_MICROSECONDS	0

#if DEBUG_TIME_IN_MICROSECONDS
static double Counter_Microseconds()
{
  static struct timeval base_time = { 0, 0 };
  struct timeval current_time;
  double counter;

  gettimeofday(&current_time, NULL);

  /* reset base time in case of wrap-around */
  if (current_time.tv_sec < base_time.tv_sec)
    base_time = current_time;

  counter =
    ((double)(current_time.tv_sec  - base_time.tv_sec)) * 1000000 +
    ((double)(current_time.tv_usec - base_time.tv_usec));

  return counter;		/* return microseconds since last init */
}
#endif

char *debug_print_timestamp_get_padding(int padding_size)
{
  static char *padding = NULL;
  int max_padding_size = 100;

  if (padding == NULL)
  {
    padding = checked_calloc(max_padding_size + 1);
    memset(padding, ' ', max_padding_size);
  }

  return &padding[MAX(0, max_padding_size - padding_size)];
}

void debug_print_timestamp(int counter_nr, char *message)
{
  int indent_size = 8;
  int padding_size = 40;
  float timestamp_interval;

  if (counter_nr < 0)
    Error(ERR_EXIT, "debugging: invalid negative counter");
  else if (counter_nr >= DEBUG_NUM_TIMESTAMPS)
    Error(ERR_EXIT, "debugging: increase DEBUG_NUM_TIMESTAMPS in misc.c");

#if DEBUG_TIME_IN_MICROSECONDS
  static double counter[DEBUG_NUM_TIMESTAMPS][2];
  char *unit = "ms";

  counter[counter_nr][0] = Counter_Microseconds();
#else
  static long counter[DEBUG_NUM_TIMESTAMPS][2];
  char *unit = "s";

  counter[counter_nr][0] = Counter();
#endif

  timestamp_interval = counter[counter_nr][0] - counter[counter_nr][1];
  counter[counter_nr][1] = counter[counter_nr][0];

  if (message)
    printf("%s%s%s %.3f %s\n",
	   debug_print_timestamp_get_padding(counter_nr * indent_size),
	   message,
	   debug_print_timestamp_get_padding(padding_size - strlen(message)),
	   timestamp_interval / 1000,
	   unit);
}

void debug_print_parent_only(char *format, ...)
{
  if (!IS_PARENT_PROCESS())
    return;

  if (format)
  {
    va_list ap;

    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);

    printf("\n");
  }
}
#endif
