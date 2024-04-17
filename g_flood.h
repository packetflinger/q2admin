/**
 * Q2Admin
 * Text flood controls
 */

#define FLOODFILE       "q2a_flood.cfg"
#define FLOODCMD        "[sv] !floodcmd [SW/EX/RE] \"command\"\n"
#define FLOODDELCMD     "[sv] !flooddel floodnum\n"
#define FLOOD_MAXCMDS   1024
#define DEFAULTFLOODMSG "%s changed names too many times."

typedef struct {
    char *floodcmd;
    byte type;
    regex_t *r;
}
floodcmd_t;

#define FLOOD_SW  0
#define FLOOD_EX  1
#define FLOOD_RE  2
