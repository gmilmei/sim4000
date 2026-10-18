#define _POSIX_C_SOURCE 200809L

#include "operator.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/history.h>
#include <readline/readline.h>

static bool wrap_output;
static unsigned output_column;
static bool input_ready;
static bool input_active;
static char *input_line;

int rc_operator_putchar(int character)
{
    if (wrap_output) {
        if (character == '\n' || character == '\r') {
            output_column = 0;
        } else {
            if (output_column == 80) {
                (void)fputc('\n', stdout);
                output_column = 0;
            }
            ++output_column;
        }
    }
    return fputc(character, stdout);
}

void rc_operator_output_enable(bool enabled)
{
    wrap_output = enabled;
    output_column = 0;
}

int rc_operator_printf(const char *format, ...)
{
    va_list arguments;
    va_list copy;
    char *text;
    int length;
    int index;

    va_start(arguments, format);
    if (!wrap_output) {
        length = vprintf(format, arguments);
        va_end(arguments);
        return length;
    }
    va_copy(copy, arguments);
    length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0) {
        va_end(arguments);
        return length;
    }
    text = malloc((size_t)length + 1);
    if (text == NULL) {
        va_end(arguments);
        return -1;
    }
    (void)vsnprintf(text, (size_t)length + 1, format, arguments);
    va_end(arguments);
    for (index = 0; index < length; ++index) {
        (void)rc_operator_putchar((unsigned char)text[index]);
    }
    free(text);
    return length;
}

int rc_operator_puts(const char *text)
{
    return rc_operator_printf("%s\n", text);
}

static const char *const commands[] = {
    "run", "r", "continue", "c", "stop", "st", "step", "s", "trace", "t",
    "show", "sh", "examine", "x", "deposit", "d", "load-core", "lc",
    "save-core", "sc", "mkdisc", "attach", "at", "detach", "det", "rewind", "rew",
    "break", "b", "nobreak", "nb", "watch", "w", "nowatch", "nw",
    "reset", "res", "start", "sta", "autoload", "al", "interrupts", "int",
    "attention", "att", "send", "se", "send-date", "sd",
    "discard-input", "di", "help", "h",
    "?", "quit", "q", "exit", "set", NULL
};
static const char *const show_targets[] = {
    "cpu", "registers", "reg", "devices", "media", "breakpoints", "break",
    "watchpoints", "watch", "telnet", "throttle", "input", "keys", NULL
};
static const char *const registers[] = {
    "W0", "W1", "W2", "W3", "IC", "IM", "IR", "EX21", "EX22", "EX23", NULL
};
static const char *const formats[] = {
    "/hex", "/x", "/char", "/characters", "/c", "/binary", "/b",
    "/instruction", "/instructions", "/i", "ic", NULL
};
static const char *const switches[] = {"on", "off", NULL};
static const char *const modes[] = {
    "readonly", "writable", "overlay", "replace", "append", NULL
};
static const char *const all[] = {"all", NULL};
static const char *const set_targets[] = {"throttle", NULL};
static const char *const throttle_values[] = {
    "off", "original", "25%", "50%", "75%", "100%", "250000ips", NULL
};
static const char *const *completion_words;

static char *word_generator(const char *text, int state)
{
    static size_t index;
    size_t length = strlen(text);

    if (state == 0) {
        index = 0;
    }
    while (completion_words[index] != NULL) {
        const char *word = completion_words[index++];
        if (strncmp(word, text, length) == 0) {
            return strdup(word);
        }
    }
    return NULL;
}

static char *file_generator(const char *text, int state)
{
    char *name;
    /* The command parser does not support quoted or whitespace-containing paths. */
    while ((name = rl_filename_completion_function(text, state)) != NULL) {
        if (strpbrk(name, " \t\r\n") == NULL) {
            return name;
        }
        free(name);
        state = 1;
    }
    return NULL;
}

static bool command_is(const char *command, const char *long_name,
                       const char *short_name)
{
    return strcmp(command, long_name) == 0 || strcmp(command, short_name) == 0;
}

static char **complete(const char *text, int start, int end)
{
    char *prefix;
    char *save = NULL;
    char *command;
    char *word;
    char *previous = NULL;
    unsigned argument = 0;
    bool filename = false;
    (void)end;

    rl_attempted_completion_over = 1;
    completion_words = NULL;
    prefix = strndup(rl_line_buffer, (size_t)start);
    if (prefix == NULL) {
        return NULL;
    }
    command = strtok_r(prefix, " \t", &save);
    if (command == NULL) {
        completion_words = commands;
    } else {
        argument = 1;
        while ((word = strtok_r(NULL, " \t", &save)) != NULL) {
            previous = word;
            ++argument;
        }
        if (argument == 1) {
            if (command_is(command, "show", "sh")) {
                completion_words = show_targets;
            } else if (strcmp(command, "set") == 0) {
                completion_words = set_targets;
            } else if (command_is(command, "deposit", "d")) {
                completion_words = registers;
            } else if (command_is(command, "examine", "x")) {
                completion_words = formats;
            } else if (command_is(command, "interrupts", "int")) {
                completion_words = switches;
            } else if (command_is(command, "nobreak", "nb") ||
                       command_is(command, "nowatch", "nw")) {
                completion_words = all;
            } else if (command_is(command, "load-core", "lc") ||
                       command_is(command, "save-core", "sc") ||
                       strcmp(command, "mkdisc") == 0) {
                filename = true;
            }
        } else if (strcmp(command, "set") == 0 && argument == 2 &&
                   previous != NULL && strcmp(previous, "throttle") == 0) {
            completion_words = throttle_values;
        } else if (command_is(command, "attach", "at")) {
            filename = argument == 2 ||
                (argument == 4 && previous != NULL &&
                 strcmp(previous, "overlay") == 0);
            if (argument == 3) {
                completion_words = modes;
            }
        }
    }
    free(prefix);
    if (filename) {
        return rl_completion_matches(text, file_generator);
    }
    return completion_words != NULL
        ? rl_completion_matches(text, word_generator) : NULL;
}

static void display_matches(char **matches, int count, int maximum)
{
    int index;
    (void)maximum;
    (void)rc_operator_putchar('\n');
    for (index = 1; index <= count; ++index) {
        (void)rc_operator_puts(matches[index]);
    }
    rl_on_new_line();
    rl_redisplay();
}

static void accept_line(char *line)
{
    input_line = line;
    input_ready = true;
    rl_callback_handler_remove();
    input_active = false;
    output_column = 0;
    if (line != NULL && line[strspn(line, " \t")] != '\0') {
        HIST_ENTRY *last = history_length > 0
            ? history_get(history_base + history_length - 1) : NULL;
        if (last == NULL || strcmp(last->line, line) != 0) {
            add_history(line);
        }
    }
}

void rc_operator_input_begin(void)
{
    rl_readline_name = "sim4000";
    rl_catch_signals = 0;
    rl_catch_sigwinch = 0;
    rl_attempted_completion_function = complete;
    rl_completion_display_matches_hook = display_matches;
    rl_completer_word_break_characters = " \t\n";
    /* Keep history session-local: no implicit writes to the user's home. */
    stifle_history(500);
    input_ready = false;
    input_line = NULL;
    rl_callback_handler_install("sim4000> ", accept_line);
    input_active = true;
}

bool rc_operator_input_ready(void) { return input_ready; }
void rc_operator_input_read(void) { rl_callback_read_char(); }
char *rc_operator_input_take(void)
{
    char *line = input_line;
    input_line = NULL;
    return line;
}

void rc_operator_input_end(void)
{
    if (input_active) {
        rl_callback_handler_remove();
        input_active = false;
    }
    free(input_line);
    input_line = NULL;
    clear_history();
}
