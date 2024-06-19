#include <stdio.h>
#include <string.h>

#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()
#include <time.h>
#include <sys/ioctl.h>

const char *SERIAL_PORT_DEFAULT = "/dev/ttyUSB0";
const int BAUDRATE_DEFAULT = 9600;

int TERM_COLS = 80;


typedef struct {
    char buffer[32];
    int ptr;
    int is_invalid;
} line_reader_t;


int update_line(line_reader_t *line, char character) {
    if (character == '\r')
        return 0;
    if (character == '\n') {
        int old_ptr = line->ptr;
        int old_invalid = line->is_invalid;
        line->ptr = 0;
        line->is_invalid = 0;
        if (!old_invalid) {
            return old_ptr;
        }
    } else {
        line->buffer[(line->ptr) % 32] = character;
        if (line->ptr >= 32) {
            line->is_invalid = 1;
        }
        line->ptr ++;
    }
    return 0;
}

int parse_args(int argc, char** argv, char** serial_port, int* baudrate, char *target, int target_len) {
    if (argc > 4 || argc < 2)
        goto print_usage;
    for (int i = 0; i<argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage:
                printf("Usage:\n\t%s TEXT [serial port] [baudrate]\n",argv[0]);
                return 1;
        }
    }

    int text_len = strlen(argv[1]);
    int j = 0;
    for (int i = 0; i<target_len && i<text_len; i++) {
        if (
            argv[1][i] == 0 ||
            (argv[1][i] >= 'A' && argv[1][i]<='Z')
        ) {
           target[j] = argv[1][i];
           j++;
        } else if (argv[1][i] >='a' && argv[1][i]<='z') {
            target[j] = argv[1][i] - 'a' + 'A';
            j++;
        }
    }
    target[target_len-1] = 0;

    if (argc > 2) {
        *serial_port = argv[2];
    }

    int baudrate_parse = 0;
    if (argc ==4) {
        int res = sscanf(argv[3], "%d", &baudrate_parse);
        if (res < 1) {
            printf("Error parsing baud rate '%s'\n", argv[3]);
        } else {
            *baudrate = baudrate_parse;
        }
    }



    return 0;
}

int open_serial(const char* serial_file, int baudrate, int *port_file) {
    // serial code based on https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/
    int serial_port = open(serial_file, O_RDWR);

    if (serial_port < 0) {
        printf("Error %i from open: %s\n", errno, strerror(errno));
        return -1;
    }

    // Create new termios struct, we call it 'tty' for convention
    // No need for "= {0}" at the end as we'll immediately write the existing
    // config to this struct
    struct termios tty;
    // Read in existing settings, and handle any error
    // NOTE: This is important! POSIX states that the struct passed to tcsetattr()
    // must have been initialized with a call to tcgetattr() overwise behaviour
    // is undefined
    if(tcgetattr(serial_port, &tty) != 0) {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        //close(serial_port);
        return -1;
    }
    // 8N1 transmission
    tty.c_cflag &= ~PARENB; // no parity
    tty.c_cflag &= ~CSTOPB; // 1 stop bit
    tty.c_cflag &= ~CSIZE; tty.c_cflag |= CS8; // 8 bit per frame
    tty.c_cflag &= ~CRTSCTS; // no flow control
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~ICANON; // non-canonical mode
    tty.c_lflag &= ~ECHO; // Disable echo
    tty.c_lflag &= ~ECHOE; // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_lflag &= ~ISIG; // no signal chars
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

    cfsetispeed(&tty, baudrate);
    cfsetospeed(&tty, baudrate);
    tty.c_cc[VTIME] = 0;
    tty.c_cc[VMIN] = 0;

    // Save tty settings, also checking for error
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
        //close(serial_port);
        return -1;
    }
    *port_file = serial_port; 
    return 0;
}

const char* alphabet[26+10][2] = {
    "A", ".-",
    "B", "-...",
    "C", "-.-.",
    "D", "-..",
    "E", ".",
    "F", "..-.",
    "G", "--.",
    "H", "....",
    "I", "..",
    "J", ".---",
    "K", "-.-",
    "L", ".-..",
    "M", "--",
    "N", "-.",
    "O", "---",
    "P", ".--.",
    "Q", "--.-",
    "R", ".-.",
    "S", "...",
    "T", "-",
    "U", "..-",
    "V", "...-",
    "W", ".--",
    "X", "-..-",
    "Y", "-.--",
    "Z", "--..",
    // currently unused
    "0", "-----",
    "1", ".----",
    "2", "..---",
    "3", "...--",
    "4", "....-",
    "5", ".....",
    "6", "-....",
    "7", "--...",
    "8", "---..",
    "9", "----.",
};

typedef struct {
    char val[4];
    int ptr;
    int ok_letters;
    int mistakes;
    double seconds;
    int result;
} player_state_t;

int try_decode_symbol(player_state_t *state) {
    // compare with each
    for (int i = 0; i<36; i++) {
        // compare with ith symbol
        const char *symbol = alphabet[i][1]; 
        int len = strlen(symbol);
        int matches = 1;
        if (state->ptr != len) {
            matches = 0;
        } else {
            for (int j = 0; j<len && j<state->ptr && j<4; j++) {
                if (symbol[j] != state->val[j]) {
                    matches = 0;
                }
            }
        }
        if (matches) {
            return i;
        }
    }
    return -1;
}

/*
Possible results:
- 0 -> nothing happesns (added to buffer)
- 1 -> ok, next letter (clears buffer)
- -1 -> fail, wrong letter (clears buffer)
*/
int add_symbol(player_state_t *state, char character, char *expected) {
    // assumption: state->ptr is always valid to write
    // assumprion: character is . or -
    if (state->ok_letters == strlen(expected)) {
        return 0;
    }
    state->val[state->ptr] = character;
    state->ptr++;

    int val = try_decode_symbol(state);
    if (val >= 0 && alphabet[val][0][0] == expected[state->ok_letters]) {
        state->ptr = 0;
        state->ok_letters++;
        return 1;
    }
    if (state->ptr == 4) {
        state->ptr = 0;
        state->mistakes++;
        return -1;
    }
    return 0;
}

time_t start_time;

void print_state(player_state_t *players, const char* target) {
    printf("\e[2J\e[H");

    // print target in centered box
    printf("+");
    for (int i = 0; i<TERM_COLS - 2; i++)
        printf("-");
    printf("+\n");
    printf("|%*s|\n",TERM_COLS-2,"");
    int target_len = strlen(target);
    int left = (TERM_COLS - 2 + target_len)/2;
    printf("|%*s%*s|\n",left, target, TERM_COLS-2-left, "");
    printf("|%*s|\n",TERM_COLS-2,"");
    /*left = (TERM_COLS - 2 - 8) / 2;
    double secs = start_time ? difftime(time(NULL), start_time) : 0;
    int mins = secs / 60;
    secs -= mins * 60.0;
    printf("|%*s%02d:%05.2f%*s|\n", left, "", mins, secs, TERM_COLS-2-left-8, "");
    */
    printf("|%*s|\n",TERM_COLS-2,"");
    printf("+");
    for (int i = 0; i<TERM_COLS-2; i++)
        printf("-");
    printf("+\n");

    const char *result_strs[4] = {
        "\e[38;5;11m1st place\e[39m",
        "\e[38;5;350m2nd place\e[39m",
        "\e[38;5;208m3rd place\e[39m",
        "\e[38;5;124m4th place\e[39m",
    };

    for (int i = 0; i<4; i++) {
        int is_player_finished = players[i].ok_letters==strlen(target);
        if (is_player_finished) {
            printf("Player %d: \e[32m%s\e[39m\e[24m\t%s (%.02fs)\n", i+1,target, result_strs[players[i].result-1], players[i].seconds);
        } else {
            printf("Player %d: %.*s\e[41m%c\e[49m\t%.*s\n", i+1,players[i].ok_letters, target, target[players[i].ok_letters], players[i].ptr, players[i].val);
        }
    }
}

/*
Usage:
game [DESCRIPTOR] [BAUDRATE]
*/
int main(int argc, char **argv) {
    char target_str[32] = {0};
    const char* serial_port_file = SERIAL_PORT_DEFAULT;
    int baudrate = BAUDRATE_DEFAULT;


    struct winsize ws;
    ioctl(0, TIOCGWINSZ, &ws);
    TERM_COLS = ws.ws_col;

    if (parse_args(argc, argv, (char**)&serial_port_file, &baudrate, target_str, 32)) {
        return -1;
    }
    printf("Using serial file '%s'\n", serial_port_file);
    printf("Using baud rate %d\n", baudrate);

    int serial_port = 0;
    if (open_serial(serial_port_file, baudrate, &serial_port)) {
        return -1;
    }
    line_reader_t readline;
    player_state_t players[4] = {0};

    char recb_char;
    print_state(players, target_str);
    int place_to_give = 1;
    while (1) {
        int num_bytes = read(serial_port, &recb_char, 1);

        if (num_bytes>0) {
            int line_len = update_line(&readline, recb_char);
            if (line_len) {
                if (!start_time) {
                    start_time = time(NULL);
                }
                // check format: Up:v
                // where
                // - p = player no 0-3
                // - v is value 'L' (for -) or 'S' (for .)
                if (line_len == 4 &&
                    readline.buffer[0] == 'U' &&
                    readline.buffer[1] >= '0' && readline.buffer[1] <= '4' &&
                    readline.buffer[2] == ':' &&
                    (readline.buffer[3] == 'S' || readline.buffer[3] == 'L')
                ) {
                    int player_no = readline.buffer[1] - '0';
                    int n = add_symbol(&players[player_no], readline.buffer[3] == 'S' ? '.' : '-', target_str);
                    if (n && players[player_no].ok_letters == strlen(target_str)) {
                        players[player_no].seconds = difftime(time(NULL),start_time);
                        players[player_no].result = place_to_give;
                        place_to_give++;
                    }
                }
                print_state(players, target_str);
            }
        }
    }

    close(serial_port);
    return 0;
}
