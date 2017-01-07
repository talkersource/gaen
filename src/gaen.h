/*** Header file for NUTS version 3.3.3, GAEN M (18.2) version ***/

#define DATAFILES   "datafiles"	/* some generic filenames */
#define USERFILES   "userfiles"
#define CONFFILES   "conffiles"
#define HELPFILES   "helpfiles"
#define HINTFILES   "hintfiles"
#define QUOTEFILES  "quotefiles"
#define KILLFILES   "killfiles"
#define IMAGFILES   "pictfiles"
#define DICTFILES   "dictfiles"
#define LOGFILES    "logfiles"
#define MISCFILES   "miscfiles"
#define PRAYFILES   "prayfiles"
#define PROCFILES   "/proc"	/* system dependent (this value is for Linux) */
#define WORDSFILE   "words"
#define MAILSPOOL   "mailspool"
#define CONFIGFILE  "config"
#define NEWSFILE    "newsfile"
#define MAPFILE     "mapfile"
#define HOSTSFILE   "hostsfile"
#define ULISTFILE   "users_list"
#define ALERTFILE   "alert"
#define SWEARFILE   ".swear"
#define MIRRORFILE  "mirror"
#define EDENFILE    "eden"
#define ALLOWFILE   "allow"
#define WHOFILE     "who"
#define LASTFILE    "last"
#define CMDSFILE    ".commands"
#define UTMPFILE    "/var/run/utmp"	/* system dependent (this value is for Red Hat Linux) */
#define WTMPFILE    "/var/log/wtmp"	/* system dependent */
#define SITEBAN     "siteban"
#define USERBAN     "userban"
#define SYSLOG      "syslog"
#define PASS_SALT   "NU"	/* password encryption salt - used by crypt() */

/* talker messages used on connecting time */
#define PRELOG_MSG  "msgsfiles/prelog_msg"
#define POSTLOG_MSG "msgsfiles/postlog_msg"
#define MINLOG_MSG  "msgsfiles/minlog_msg"
#define NEWLOG_MSG  "msgsfiles/newlog_msg"
#define STPORT_MSG  "msgsfiles/stport_msg"

#define NOPRELOG_MSG	"\n\n--->>> Prelogin commands are not accepted, sorry! <<<---\n\n"
#define AFK_DEF_MSG     "unknown reason"
#define DEF_NAME_MSG	">>>Dreamer, give me a name: "
#define DEF_DESC	"~LIwants to talk?!"
			/* default description of new users */
#define BHOLE_DESC	"~RVwants another blackhole"
			/* default description in blackhole */

#define MAX_MIRRORS 	4	/* max. number of mirrors */
#define MAX_WATCH  	8	/* max. watch entries     */
#define MAX_DIMENS 	3	/* max. sky dimensions    */
#define MAX_SWEARS     10	/* max. size of swears list */
#define MAX_LEVELS 	9	/* max. levels (0=GUEST .. 8=SGOD) */
#define MAX_WUSERS     10	/* max. number of whisper users */
#define MAX_ALIASES    10	/* max. number of aliases */
#define MAX_PRELOG_MSGS 3	/* max. of prelogin messages files */
#define ALIAS_NAME_LEN 10	/* max. length of an alias */
#define AFK_MESG_LEN   50	/* max. length of AFK message */
#define WAKEMSG_LEN    50	/* max. length of wake message */
#define EXAM_MESG_LEN  60	/* max. length of examine reply message */
#define COMMAND_LEN   100	/* max. length of command line */
#define ALARM_CMD_LEN  80	/* max. length of alarm command */
#define MAX_LEN_LANG   80	/* max. length of language string */
#define DILI_OFFSET    13	/* used in DILI dimension, by exec_com() */

#define BLACKHOLE_RND  40	/* used by blackhole() */
#define BLACKHOLE_VAL   1

#define REN_BASE	0	/* used by rename_user() */
#define REN_REV		1
#define REN_GEN		2
#define REN_MAX_SUF	40	/* maximum number of suffixes */

#define GM_MAX_STATE 	8	/* used by hangman functions */
#define GM_MAX_WORDS   50
#define GM_TDIM         4	/* used by puzzle functions */
#define GM_LT           0
#define GM_RG           1
#define GM_FD           2
#define GM_BK           3

#define GM_MAX_PBALLS   9	/* default # of balls for Paintball */
#define GM_FIT_SCORE    9471	/* used by Fit to compute the personal score */

#define PRAY_RND       10	/* used by pray() */
#define PRAY_VAL        5

#define GUESS_RND     100	/* used by guess() */

#define MAX_USER_MACROS    5	/* user macros constants */
#define MAX_LEN_MACRONAME  3
#define MAX_LEN_MACRODESC 60

#define OUT_BUFF_SIZE 1000	/* input buffer size */
#define MAX_WORDS 10		/* max. words processed by commands */
#define WORD_LEN 40		/* length of words */
#define ARR_SIZE 2000		/* array chars size */
#define MAX_LINES 12		/* max. lines in editor */
#define WRAP_COLS 80		/* max. columns (lenght) of wrapped lines */

#define USER_NAME_LEN 	12	/* user name maximum length */
#define USER_MIN_LEN 	3	/* user name minimum length */
#define USER_DESC_LEN 	30	/* user desc length */
#define PHRASE_LEN 	40	/* in/out phrase length */
#define PASS_MAX_LEN 	20	/* only the 1st 8 chars will be used by crypt() */
#define PASS_MIN_LEN 	5	/* min. password length */
#define BUFSIZE 	1000	/* another buffer size */
#define ROOM_NAME_LEN 	12	/* room name length */
#define ROOM_LABEL_LEN 	2	/* room label length */
#define ROOM_DESC_LEN 	811	/* 10 lines of 80 chars each + 10 nl + 1 \0 */
#define TOPIC_LEN 	80	/* room topic length */
#define MAX_LINKS 	10	/* max. links from a room */
#define SERV_NAME_LEN 	12	/* server name length */
#define SITE_NAME_LEN 	80	/* site name length */
#define VERIFY_LEN 	20	/* verify string length */
#define REVIEW_LINES 	12	/* review conversation buffer lines */
#define REVTELL_LINES 	9	/* review .tells buffer lines */
#define REVPICT_LINES 	5	/* review .pictures buffer lines */
#define PICT_NAME_LEN 	12	/* picture name length */
#define REVIEW_LEN 	200	/* review conversation buffer line capacity */
#define REVPICT_LEN USER_NAME_LEN+PICT_NAME_LEN+5	/* username length + pict. name length + delim. */
#define EVENT_DEF_LEN 	50	/* event definition length */
#define NUM_COLS 	23	/* number of colour codes */
#define DEF_LINES 	23	/* default paged lines for more() */
#define MAX_IGNRD_USERS	10	/* maximum number of users that can be ignored */


/* DNL (Date Number Length) will have to become 12 on Sun Sep 9 02:46:40 2001
   when all the unix timers will flip to 1000000000 :) */
#define DNL 12

#define PUBLIC 		0	/* type of rooms access */
#define PRIVATE 	1
#define FIXED   	2
#define FIXED_PUBLIC 	2
#define FIXED_PRIVATE 	3

/* Levels numbers */
#define GUEST	 0
#define SOUL	 1
#define SPARK	 2
#define SPRITE	 3
#define ANGEL	 4
#define SPIRIT	 5
#define SAINT	 6
#define GOD	 7
#define SGOD     8

/* User types */
#define USER_TYPE 	0
#define CLONE_TYPE 	1
#define REMOTE_TYPE 	2

/* Clone hear types */
#define CLONE_HEAR_NOTHING 	0
#define CLONE_HEAR_SWEARS 	1
#define CLONE_HEAR_ALL 		2

/* Maximum number of login attempts */
#define MAX_ATTEMPTS		3

/* Maximum number of quotes */
#define MAX_QUOTES  	      700

#define E_NONE		-1	/* used by events */
#define E_IN		0
#define E_OUT		1
#define E_SHOUT		2
#define E_BHOLE		3
#define E_BCAST		4
#define E_KILL		5
#define E_MIRROR	6
#define E_AFK		7
#define E_NOAFK		8
#define E_SWEAR		9
#define E_SWAP	       10
#define MAX_EVENTS     11

#define MAX_LS_COLS	2	/* used by gsh_ls() */

#define GSH_BUFSIZE   512	/* buffer size for gsh_cp() */

#define GSH_HELP        0
#define GSH_LS		1	/* used by gshell() */
#define GSH_MV		2
#define GSH_RM		3
#define GSH_MORE	4
#define GSH_MKDIR	5
#define GSH_RMDIR	6
#define GSH_CD		7
#define GSH_PWD		8
#define GSH_HOME        9
#define GSH_CHMOD      10
#define GSH_LN	       11
#define GSH_SCAN       12
#define GSH_GREP       13
#define GSH_RGREP      14
#define GSH_TAIL       15
#define GSH_CP	       16
#define GSH_LE	       17
#define GSH_WHO	       18
#define GSH_LAST       19
#define GSH_CHOWN      20
#define GSH_STAT       21
#define GSH_PS         22

#define GSH_MAX_LAST  100	/* max. entries showed by gsh_last() */

#define ID_OK		0	/* error codes used by identify() */
#define ID_CONNERR     -1
#define ID_NOFOUND     -2
#define ID_CLOSED      -3
#define ID_READERR     -4
#define ID_WRITEERR    -5
#define ID_NOMEM       -6
#define ID_TIMEOUT     -7
#define ID_CRAP        -8
#define ID_NOUSER      -9
#define ID_INVPORT    -10
#define ID_UNKNOWNERR -11
#define ID_COMERR     -12
#define ID_UNKNOWN    -13
#define ID_HIDDENUSER -14

#define ID_BUFFLEN    196	/* max. buffer length for reads and
				   writes in identify() */
#define ID_READTIMEOUT 30	/* number of seconds after a socket
				   read operation is timed out. */

#define LU_ALL 		0	/* used by list_users() */
#define LU_LEVEL	1
#define LU_NAME		2
#define LU_SITE		3
#define LU_EMAIL	4
#define LU_IDENT	5

#define TN_SAY		0	/* used by tone() */
#define TN_TELL		1
#define TN_BOTH		2

#define CHK_SPASS_NONE  0	/* check secure passwords tests */
#define CHK_SPASS_ALNUM 1
#define CHK_SPASS_SIMIL 2
#define CHK_SPASS_ALL 3

#define CHK_MSG_EVENT	0	/* check board messages types */
#define CHK_MSG_BOOT	1
#define CHK_MSG_RECOUNT	2

#define SAFETY_EXT 	"_"	/* used by s_unlink() */
#define MAX_SAFETY_VAL  3333

/* Sweep criteria constants */
#define SW_LEVEL 	0
#define SW_NOINFO	1
#define SW_TOTAL	2
#define SW_LAST		3
#define SW_ONLYMAIL	4
#define SW_ONLYENV	5
#define SW_ONLYSUBST	6
#define SW_ONLYSET	7
#define SW_ONLYPROF	8
#define SW_ONLYRUN	9
#define SW_ONLYRESTR	10

/* Autosave constants */
#define AUTOSAVE_MAXITER 300	/* max. iterations in autosave */
#define AS_NONE		 0	/* autosave actions */
#define AS_GREET	 1
#define AS_QUOTE	 2
#define AS_ALL		 3

/* Restrictions stuff */
#define MAX_RESTRICT   	14	/* max. restrictions */

#define RESTRICT_GO    	0	/* .go */
#define RESTRICT_MOVE  	1	/* .move user */
#define RESTRICT_PROM  	2	/* .promote */
#define RESTRICT_DEMO  	3	/* .demote */
#define RESTRICT_MUZZ  	4	/* .muzzle */
#define RESTRICT_UNMU  	5	/* .unmuzzle */
#define RESTRICT_KILL  	6	/* .kill */
#define RESTRICT_HELP  	7	/* access .help */
#define RESTRICT_SUIC  	8	/* .suicide */
#define RESTRICT_WHO   	9	/* .who */
#define RESTRICT_RUN  	10	/* .run commands */
#define RESTRICT_CLON 	11	/* .create, .destroy clones */
#define RESTRICT_VIEW 	12	/* .review, .revtell */
#define RESTRICT_EXEC	13	/* execution of commands */

#define RESTRICT_MASK  ".............."	/* mask used by load_user_details() */

/* Restrictions codes */
char restrict_string[MAX_RESTRICT + 1] = "GMPDZUKHSWRCVX";

/* Log files stuff - used by write_syslog() */
#define LOG_CHAT 	0	/* important messages */
#define LOG_ERR  	1	/* errors */
#define LOG_IO   	2	/* inputs/outputs (logins/logouts) */
#define LOG_COM  	3	/* effects of some commands */
#define LOG_LINK 	4	/* linking messages */
#define LOG_NOTE 	5	/* special log file used by note() */
#define LOG_SWEAR 	6	/* swearing stuff */
#define LOG_ERASE	7	/* safety unlink files */

#define MAX_LOGS 	8

#define LOG_TIME	1	/* log current time */
#define LOG_NOTIME	0	/* don't log current time */

/* types of system log files */
char *logtype[MAX_LOGS] = {
  "main", "err", "io", "com", "link", "note", "swear", "unlink"
};

/* Levels used by certain commands */

#define MIN_LEV_BLIND	ANGEL	/* minimum level to use .terminal blind */
#define MIN_LEV_HREV	SGOD	/* minimum level to .review a hidden sky */
#define MIN_LEV_IPICT	GOD	/* minimum level to ignore .ignpict flag */
#define MIN_LEV_SEVT	GOD	/* minimum level to send events */
#define MIN_LEV_UREN	GOD	/* minimum level to use unrestricted rename */
#define MIN_LEV_OREN	GOD	/* minimum level to rename other user */
#define MIN_LEV_OSUBST	GOD	/* minimum level to .subst other user */
#define MIN_LEV_OSTAT	SPIRIT	/* minimum level to .status other user */
#define MIN_LEV_PASS	GOD	/* minimum level to change other's passwd */
#define MIN_LEV_NOSWR   SGOD	/* minimum level to ignore swear_action() */
#define MIN_LEV_ACCPRIV SAINT	/* minimum level to access private sky */
#define MIN_LEV_TRANSP  SGOD	/* minimum level of transparency */
#define MIN_LEV_GO      SPIRIT	/* minimum level to .go anywhere */
#define MIN_LEV_IBHOLE  SAINT	/* minimum level to ignore blackhole effects */
#define MIN_LEV_VIEWDIM SPIRIT	/* minimum level to view dimensions in .who */
#define MIN_LEV_VIEWREN SAINT	/* minimum level to view renamed users in .who */
#define MIN_LEV_NOTIFY  SGOD	/* minimum level to notify changed logs */
#define MIN_LEV_EX	SPIRIT	/* minimum level to detect .examine/.status */
#define MIN_LEV_STSHOUT SPIRIT	/* minimum level to .stshout */
#define MIN_LEV_AUTORST SAINT	/* minumum level to set default restrictions */
#define MIN_LEV_NOTIFY  SGOD	/* minimum level for notify facility */
#define MIN_LEV_ALLOW	SAINT	/* minimum level for an allowed user */
#define MAX_LEV_ALLOW	GOD	/* maximum level for an allowed users */
#define MAX_LEV_LOGHLP  SOUL	/* maximum level to view help at login time */
#define MAX_LEV_DILI    SPIRIT	/* maximum level of users who can't ign. DILI */
#define MAX_LEV_PPRAY	SPRITE	/* maximum level to be promoted by .pray */
#define MAX_LEV_DPRAY	SAINT	/* maximum level to be demoted by .pray */
#define MAX_LEV_NOPICK  GUEST	/* maximum level with no pick sky facility */
#define MAX_LEV_NEWUSER SPIRIT	/* maximum level of default level for a new user */

/* Constants used by who() */
#define LIST_WHO	0
#define LIST_PEOPLE	1

/* GAEN Hangman structure */
struct hangman_struct
{
  int state;			/* game state (0..8)  */
  char to_guess[WORD_LEN + 1];	/* word to be guessed */
  char current[WORD_LEN + 1];	/* current word       */
  char letters[27];		/* used letters       */
};

/* GAEN Puzzle structure */
struct puzzle_struct
{
  int table[GM_TDIM][GM_TDIM];	/* table with numbers */
  int clin, ccol;		/* current position */
};

/* Ignored user structure */
struct ignored_user
{
  int asked_for_forgiveness;	/* if user asked for forgiveness */
  char name[USER_NAME_LEN + 1];	/* ignored user name */
};

/* The elements vis, ignall, prompt, command_mode etc could all be bits in
   one flag variable as they're only ever 0 or 1, but we tried it and it
   made the code unreadable. Better to waste a few bytes */
struct user_struct
{
  char name[USER_NAME_LEN + 1];	/* user name         */
  char savename[USER_NAME_LEN + 1];	/* user real name    */
  char desc[USER_DESC_LEN + 1];	/* description       */
  char pass[PASS_MAX_LEN + 6];	/* password          */
  char in_phrase[PHRASE_LEN + 1], out_phrase[PHRASE_LEN + 1];
  /* in/out phrases    */
  int slevel, subst;		/* substitution...   */
  int dimension;		/* dimension         */
  char restrict[MAX_RESTRICT + 1];	/* restrictions...   */
  char ssite[SITE_NAME_LEN + 1];	/* subst. site name  */
  char w_users[MAX_WUSERS][USER_NAME_LEN + 1];
  int w_number;			/* whisper user list */
  char reply_user[USER_NAME_LEN + 1];	/* reply user     */
  int say_tone, tell_tone;	/* say/tell tone     */
  int macros;			/* user macros...    */
  char var1[WORD_LEN + 1], var2[WORD_LEN + 1];
  /* user variables    */
  char macro_name[MAX_USER_MACROS][MAX_LEN_MACRONAME + 1];
  char macro_desc[MAX_USER_MACROS][MAX_LEN_MACRODESC + 1];
  /* private macros    */
  char inpstr_old[REVIEW_LEN + 1];	/* previous command  */
  char events_def[MAX_EVENTS][EVENT_DEF_LEN + 1];
  /* event definitions */
  int stereo;			/* stereo voice      */
  int alarm_hour, alarm_min,	/* alarm hours, minutes, control... */
    alarm_activated;
  char alarm_cmd[ALARM_CMD_LEN + 1];	/* alarm command */
  int blind;			/* 1 if user is in blind mode */
  char buff[BUFSIZE],		/* buffer used by some commands */
    site[SITE_NAME_LEN + 1],	/* login site */
    last_site[SITE_NAME_LEN + 1],	/* last login site name */
    page_file[SITE_NAME_LEN + 1],	/* filename of more()   */
    exam_mesg[EXAM_MESG_LEN + 1];	/* examine reply message */
  char mail_to[WORD_LEN + 1],	/* mail destination */
    revbuff[REVTELL_LINES][REVIEW_LEN + 2],	/* .revtell buffer */
    revpict[REVPICT_LINES][REVPICT_LEN + 2];	/* .revpict buffer */
  char aliases_name[MAX_ALIASES][ALIAS_NAME_LEN + 1];	/* aliases stuff */
  int aliases_cmd[MAX_ALIASES], aliases;
  struct room_struct *room, *invite_room;	/* current & invited room */
  int type,			/* user type: normal, clone, remote */
    port, site_port,		/* login server and client ports */
    login,			/* login stage */
    socket,			/* login socket */
    attempts,			/* attempts to enter */
    buffpos, filepos;		/* used by more() */
  int vis,			/* visibility flag */
    ignpict, ignall,		/* ignoring picts & all users flags */
    prompt,			/* visibility of prompt flag */
    command_mode,		/* speech/command mode flag */
    muzzled, charmode_echo;	/* muzzle state and charechoing flag */
  int level,			/* user level (0..MAX_LEVELS-1) */
    misc_op,			/* used by misc_ops() */
    remote_com,			/* remote command */
    edit_line, charcnt,		/* used by editor() */
    warned;			/* log off warning state */
  int accreq,			/* account request state */
    last_login_len,		/* last login site length */
    ignall_store,		/* ignore flag save variable */
    clone_hear,			/* clone hearing type */
    afk,			/* AFK flag */
    wrap,			/* wrapping lines flag */
    hint_at_help;		/* show a hint at .help flag */
  int edit_op,			/* editor option */
    colour;			/* 0 = no colours, 1 = colours, 2 = transparent */
  int ignshout, igntell,	/* ignoring shouts & tells flags */
    revline, revpictline;	/* review lines */
  time_t last_input,		/* last input time */
    last_login, total_login,	/* last and total login time */
    read_mail;			/* last mail reading time */
  char *malloc_start, *malloc_end;	/* used by editor() */
  struct netlink_struct *netlink, *pot_netlink;	/* remote links */
  struct user_struct *prev, *next, *owner;
  /* preview, next item of users lists; owner of clone structure */
  char afkmesg[AFK_MESG_LEN + 1];	/* AFK message */
  int plines;			/* paged lines used by more() */
  int pballs;			/* Paintball #balls */
  struct hangman_struct *hangman;	/* Hangman field */
  struct puzzle_struct *puzzle;	/* Puzzle field  */
  /* ignored users data */
  struct ignored_user ignored_users_list[MAX_IGNRD_USERS + 1];
  int ign_number;
};

typedef struct user_struct *UR_OBJECT;
UR_OBJECT user_first, user_last;

/* Room structure */
struct room_struct
{
  char name[ROOM_NAME_LEN + 1];	/* room name */
  char label[ROOM_LABEL_LEN + 1];	/* room label loaded from config */
  char desc[ROOM_DESC_LEN + 1];	/* description */
  char topic[TOPIC_LEN + 1];	/* current topic */
  char revbuff[REVIEW_LINES][REVIEW_LEN + 2];	/* review buffer */
  int inlink;			/* 1 if room accepts incoming net links */
  int hidden;			/* >0 if room is hidden */
  int hidden_board;		/* >0 if room has the hidden board */
  int access;			/* public , private etc */
  int revline;			/* conversation line number for recording */
  int mesg_cnt;			/* number of board messages */
  char netlink_name[SERV_NAME_LEN + 1];	/* temp store for config parse */
  char link_label[MAX_LINKS][ROOM_LABEL_LEN + 1];	/* temp store for parse */
  struct netlink_struct *netlink;	/* for net links, 1 per room */
  struct room_struct *link[MAX_LINKS];	/* links to other rooms */
  struct room_struct *next;	/* next item of rooms list */
};

typedef struct room_struct *RM_OBJECT;
RM_OBJECT room_first[MAX_DIMENS], room_last[MAX_DIMENS];
RM_OBJECT create_room ();

/* Netlink stuff */
#define UNCONNECTED 0
#define INCOMING 1
#define OUTGOING 2
#define ALL  0
#define IN   1
#define OUT  2
#define DOWN      0
#define VERIFYING 1
#define UP        2

/* Structure for net links, ie server initiates them */
struct netlink_struct
{
  char service[SERV_NAME_LEN + 1];	/* service name         */
  char site[SITE_NAME_LEN + 1];	/* remote site name     */
  char verification[VERIFY_LEN + 1];	/* verification string  */
  char buffer[ARR_SIZE * 2];	/* commands buffer      */
  char mail_to[WORD_LEN + 1];	/* mail destination     */
  char mail_from[WORD_LEN + 1];	/* mail expeditor       */
  FILE *mailfile;		/* mail file pointer    */
  time_t last_recvd;		/* last verifying time  */
  int port, socket,		/* service port & socket */
    type, connected;		/* link type (IN, OUT,...) */
  int stage,			/* connection stage     */
    lastcom,			/* last remotely cmd.  */
    allow,			/* allow connection flag */
    warned, keepalive_cnt;	/* used to keep alive the connection */
  int ver_major, ver_minor, ver_patch;	/* remote NUTS version */
  struct user_struct *mesg_user;	/* user structure for messages */
  struct room_struct *connect_room;	/* connection room structure */
  struct netlink_struct *prev, *next;	/* previous & next item of links list */
};

typedef struct netlink_struct *NL_OBJECT;
NL_OBJECT nl_first, nl_last;
NL_OBJECT create_netlink ();

/* Some string constants (if you want to modify them,
   check length to avoid buffer overflows) */
char *syserror = "~FR~OL~UL~EL>>>Sorry, a system error has occured";
char *nosuchroom = "~FR~OL>>>There is no such sky. Sorry...\n";
char *nosuchuser = "~FR~OL>>>There is no such user. Sorry...\n";
char *notloggedon =
  "~FR~OL>>>There is no one of that name logged on. Sorry...\n";
char *noswearing =
  "~FR~OL>>>Swearing is not allowed in ~FG~OLGAEN~RS!! Sorry...\n";
char *jailsky = "Sadness";
char *exam_mesg = "Please, don't try to do this again... I'm very shy...:)";
char *excellent = "\n~OLGAEN:~RS Excellent!\n\n";
char *prelog_msg = ">>>GAEN thinks...";
char *enter_pass = ">>>Please, enter a password: ";

/* Level names used in mirror_chat() */
char *level_name_mirror[MAX_MIRRORS][MAX_LEVELS + 1] = {
  {				/* level names of EDEN mirror */
   "~FRGUEST",
   "~FMSOUL",
   "~FYSPARK",
   "~OL~FTSPRITE",
   "~OL~FMANGEL",
   "~OL~FBSPIRIT",
   "~OL~FYSAINT",
   "~OL~FGGOD",
   "~FG~OL !!! ",
   "*"},
  {				/* level names of GAEN mirror */
   "~FR0",
   "~FYI",
   "~FTII",
   "~FMIII",
   "~FR~OLIV",
   "~FY~OLV",
   "~FT~OLVI",
   "~FM~OLVII",
   "~FG~OL !!! ",
   "*"},
  {				/* level names of HELL mirror */
   "~FMGUEST",
   "~FYFLESH",
   "~FT~OLBODY",
   "~FG~OLCORPSE",
   "~FY~OLSHADOW",
   "~FB~OLGHOST",
   "~FM~OLDEVIL",
   "~FR~OLSATAN",
   "~FG~OL !!! ",
   "*"},
  {				/* level names of DILI mirror */
   "~FMGUEST",
   "~FM~OLPSYCHO",
   "~FM~OLPSYCHO",
   "~FM~OLPSYCHO",
   "~FM~OLPSYCHO",
   "~FM~OLPSYCHO",
   "~FM~OL !",
   "~FR~OL !!",
   "~FG~OL !!! ",
   "*"}
};

/* Maximum level (SGOD) users who don't need check_allow();
   add here desired names, before "*";
   Don't forget to place "*" at the end! */
char *max_level_users[] = { "*" };

char level_name[MAX_LEVELS + 1][15];	/* level names */
char invisname[20], invisenter[40], invisleave[40];
				     /* invisibility phrases and name */

/* Commands names */
char *command[] = {
  "quit", "look", "mode", "say", "shout",
  "tell", "emote", "semote", "pemote", "echo",
  "go", "ignore", "prompt", "desc", "inphr",
  "outphr", "public", "private", "to", "invite",
  "topic", "move", "bcast", "who", "people",
  "home", "shutdown", "news", "read", "write",
  "wipe", "search", "review", "help", "status",
  "version", "rmail", "smail", "dmail", "from",
  "entpro", "examine", "rmst", "rmsn", "netstat",
  "netdata", "connect", "disconnect", "passwd", "kill",
  "promote", "demote", "listbans", "ban", "unban",
  "vis", "invis", "site", "wake", "stshout",
  "muzzle", "unmuzzle", "map", "logging", "minlogin",
  "system", "terminal", "clearline", "fix", "unfix",
  "viewlog", "accreq", "revclr", "clone", "destroy",
  "myclones", "allclones", "switch", "csay", "chear",
  "rstat", "swears", "afk", "cls", "colour",
  "ignshout", "igntell", "suicide", "delete", "reboot",
  "set", "macro", "think", "picture", "flash",
  "social", "subst", "dsubst", "recount", "revtell",
  "clsall", "reality", "ignpict", "save", "hint",
  "alert", "pray", "mshout", "modify", "listafks",
  "watch", "dwatch", "lwatch", "up", "down",
  "rename", "restrict", "lock", "mirror", "note",
  "whisper", "guess", "addmacro", "lmacros", "dmacro",
  "umacro", "tone", "hide", "show", "param",
  "gossip", "reply", "execute", "arun", "drun",
  "run", "aliases", "crename", "allow", "dallow",
  "stereo", "lio", "quote", "alarm", "born",
  "if", "for", "blackhole", "btell", "language",
  "environ", "lusers", "event", "gsh", "hangman",
  "identify", "puzzle", "paintball", "sing", "sweep",
  "revpict", "fit", "scommands", "letmein", "*"
};


/* Values of commands, used in switch in exec_com() */
enum comvals
{
  QUIT, LOOK, MODE, SAY, SHOUT,
  TELL, EMOTE, SEMOTE, PEMOTE, ECHOU,
  GO, IGNORE, PROMPT, DESC, INPHRASE,
  OUTPHRASE, PUBCOM, PRIVCOM, TO, INVITE,
  TOPIC, MOVE, BCAST, WHO, PEOPLE,
  HOME, SHUTDOWN, NEWS, READ, WRITE,
  WIPE, SEARCH, REVIEW, HELP, STATUS,
  VER, RMAIL, SMAIL, DMAIL, FROM,
  ENTPRO, EXAMINE, RMST, RMSN, NETSTAT,
  NETDATA, CONN, DISCONN, PASSWD, KILL,
  PROMOTE, DEMOTE, LISTBANS, BAN, UNBAN,
  VIS, INVIS, SITE, WAKE, STSHOUT,
  MUZZLE, UNMUZZLE, MAP, LOGGING, MINLOGIN,
  SYSTEM, TERMINAL, CLEARLINE, FIX, UNFIX,
  VIEWLOG, ACCREQ, REVCLR, CREATE, DESTROY,
  MYCLONES, ALLCLONES, SWITCH, CSAY, CHEAR,
  RSTAT, SWBAN, AFK, CLS, COLOUR,
  IGNSHOUT, IGNTELL, SUICIDE, DELETE, REBOOT,
  SETDATA, MACRO, THINK, PICTURE, FLASH,
  SOCIAL, SUBST, DSUBST, RECOUNT, REVTELL,
  CLSALL, REALITY, IGNPICT, SAVE, HINT,
  ALERT, PRAY, MSHOUT, MODIFY, LISTAFKS,
  WATCH, DWATCH, LWATCH, UPDIM, DOWNDIM,
  RENAME, RESTRICT, LOCK, MIRROR, NOTE,
  WHISPER, GUESS, ADDMACRO, LMACROS, DMACRO,
  UMACRO, TONE, HIDESKY, SHOWSKY, PARAM,
  GOSSIP, REPLY, EXECUTE, ARUN, DRUN,
  RUN, ALIASES, CRENAME, ALLOW, DALLOW,
  STEREO, LISTIO, QUOTE, ALARM, BORN,
  IFCOM, FORCOM, BHOLE, BTELL, LANGUAGE,
  UENVIRON, LUSERS, EVENT, GSHELL, HANGMAN,
  IDENTIFY, PUZZLE, PAINTBALL, SING, SWEEP,
  REVPICT, FIT, COMMANDS, LETMEIN
}
com_num;


/* These are the minimum levels at which the commands can be executed.
   Alter to suit. */
int com_level[] = {
  GUEST, GUEST, SOUL, GUEST, SPARK,
  SOUL, SOUL, SPARK, SOUL, ANGEL,
  SOUL, SPARK, GUEST, GUEST, SPARK,
  SPARK, SPIRIT, SPIRIT, SPRITE, SPIRIT,
  ANGEL, SPIRIT, SPIRIT, GUEST, SPRITE,
  SOUL, SGOD, GUEST, SOUL, SPARK,
  SPIRIT, SPRITE, SPRITE, GUEST, SPRITE,
  GUEST, SOUL, SPARK, SOUL, SOUL,
  GUEST, SOUL, ANGEL, ANGEL, SAINT,
  SAINT, GOD, GOD, SPARK, SAINT,
  SPIRIT, SPIRIT, SAINT, GOD, GOD,
  SPIRIT, SPIRIT, ANGEL, ANGEL, SPIRIT,
  SAINT, SAINT, SOUL, GOD, GOD,
  SAINT, GUEST, SGOD, GOD, GOD,
  GOD, GUEST, ANGEL, SPIRIT, SPIRIT,
  SPIRIT, SAINT, SPIRIT, SPIRIT, SAINT,
  SAINT, GOD, SOUL, SPARK, GUEST,
  ANGEL, ANGEL, GUEST, GOD, SGOD,
  GUEST, SPRITE, SPARK, SPRITE, SAINT,
  SPARK, SPIRIT, SPIRIT, GOD, SPRITE,
  SPIRIT, SAINT, ANGEL, SAINT, GUEST,
  SOUL, SOUL, SPARK, GOD, SPRITE,
  ANGEL, ANGEL, SPIRIT, SPIRIT, SPIRIT,
  SPIRIT, GOD, SPARK, GOD, SPARK,
  SPARK, SOUL, ANGEL, ANGEL, ANGEL,
  ANGEL, SPRITE, SPIRIT, SPIRIT, GOD,
  SOUL, SPRITE, GOD, SPIRIT, SPIRIT,
  SPIRIT, SOUL, SAINT, GOD, GOD,
  SPIRIT, SPIRIT, SOUL, SPRITE, SGOD,
  SPIRIT, SPIRIT, SGOD, ANGEL, SPRITE,
  SPRITE, GOD, SPIRIT, SGOD, SPARK,
  SGOD, SPARK, SPRITE, SPARK, SGOD,
  ANGEL, ANGEL, SGOD, SOUL
};

/* Maximum number of hints */
#define MAX_HINTS  	130

/* Socials stuff */
#define MAX_SOCIALS 	44

char *soc_com[] = {		/* social commands codes */
  "huggle",
  "kiss",
  "wkiss",
  "shake",
  "dig",
  "chase",
  "humile",
  "wink",
  "sing",
  "khand",
  "pie",
  "cut",
  "tea",
  "beer",
  "water",
  "wine",
  "smile",
  "box",
  "wash",
  "control",
  "warn",
  "heart",
  "scoff",
  "torture",
  "bomb",
  "scratch",
  "lick",
  "feed",
  "dance",
  "mew",
  "flowers",
  "accuse",
  "bite",
  "cuddle",
  "fondle",
  "giggle",
  "snap",
  "punch",
  "wave",
  "play",
  "propose",
  "promise",
  "grin",
  "wish"
};

char *soc_text[] = {		/* social texts */
  "huggle tight",
  "send a big kiss to",
  "~OLsend a ~FYreally ~FTBIG ~FMwet ~FRkiss ~FGto",
  "come closer, take a bow and shake the hand of",
  "~LI~OL~FBdig a deep, dark grave for",
  ", using a big magical wand, chase",
  "self-humiliate by taking off your aura, asking forgiveness to",
  "wink at",
  "sing a ~FTsweet~RS lullaby to",
  "kiss the hand of",
  "~FTthrow ~FYa creamy ~FMpie on ~FRthe face ~FGof",
  ", using a sharp scissors, try to cut the wings of",
  "offer a cup of GAEN's tea to",
  "~OLoffer a bottle of fresh beer to",
  "~FRthrow ~FGa bucket of ~FBicy water ~FMon the head ~ELof",
  "offer a glass of ~FRred~RS wine to",
  "smile to",
  "box the ears of",
  "wash the feet of",
  "want to control the mind of",
  "warn",
  "break the heart of",
  "scoff at",
  "are preparing to torture",
  "send a little bomb to",
  "are scratching the face of",
  "want to lick the eyes of",
  "want to feed",
  "are dancing with",
  "mew to",
  "give a bouquet of flowers to",
  "are trying to accuse",
  "want to bite the nose of",
  "want to cuddle in the arms of",
  "fondle",
  "giggle to ear of",
  "snap the fingers of",
  "are punching",
  "wave to",
  "play up to",
  "want to propose different ``dirty'' things to",
  "promise: ~LI~ULI'll be good!~RS to",
  "grin to",
  "wish health and good luck to"
};

char *soc_text_you[] = {	/* social texts send to an user */
  "huggles you tight",
  "sends you a big kiss",
  "~OLgives ~FGyou a ~FYreally ~FTBIG ~FMwet ~FRkiss",
  "comes closer, takes a bow and shakes your hand",
  "~LI~OL~FBis digging a deep, dark grave for you",
  "is chasing you, using a big magical wand",
  "self-humiliates in front of you by taking off his aura, asking you to forgive him ",
  "winks at you... You maybe wonder: ~FMWhy?",
  "sings you a ~FTsweet~RS lullaby",
  "comes closer, takes a bow and gently kisses your delicate hand",
  "~FTthrows ~FYa creamy ~FMpie on ~FRyour ugly ~FGface",
  "tries to cut your wonderful wings, using a sharp scissors",
  "offers you to savurate a cup of GAEN's spiritual tea",
  "~OLoffers you to drink a bottle of fresh and cold beer",
  "~FRthrows ~FGa bucket of ~FBicy water ~FMon your lunatic ~FThead~EL",
  "offers you to enjoy a glass of ~FRred~RS and old wine",
  "smiles to you, angelical",
  "boxes your big ears",
  "washes your holy feet",
  "wants to control your mind",
  "warns you. Be careful!",
  "breaks your fragile heart",
  "scoffs at you... :)",
  "~OL~FYis preparing to ~FRtorture~FY you with dreadful devices and wheels!! Scream or run!",
  "sends you a little pinky bomb to make-up your body... You're ready?",
  "scratches you deeply",
  "wants to lick your eyes",
  "wants to feed you",
  "is dancing with you",
  "mews to you like a real stupid cat",
  "gives you a splendid bouquet of flowers",
  "is trying to accuse you!",
  "wants to bite your nose",
  "wants to cuddle in your cosy arms",
  "fondles you very gently",
  "giggles to your ear, very closer",
  "snaps your fingers!",
  "~LIis punching you, very angry!!",
  "waves to you and says: ~FG~OLGood bye!",
  "plays up to you",
  "wants to propose you some ``dirty'' things... :)",
  "promises you: ~LI~ULI'll be good!",
  "grins to you, very happy",
  "wishes you health and good luck in everything..."
};

/* 'Spatial' voice directions (used by .say, .tell etc.) */
#define MAX_STEREO 7

char *stereo_str[] = {
  "",				/* no direction */
  "~OL[<] (From left...)\n",
  "~OL[-] (From center...)\n",
  "~OL[>] (From right...)\n",
  "~OL[ ] (From nowhere...)\n",
  "~OL[^] (From above...)\n",
  "~OL[v] (From below...)\n"
};

/* Run commands file types (used by .run) */
#define RUNCOM_IN       10
#define RUNCOM_OUT      11
#define RUNCOM_NORMAL   12
#define RUNCOM_SWAP	13

#define MAX_RUNS	14

/* Macros stuff (used by .macro command) */
#define MAX_MACROS 	30

char *macros_name[] = {		/* macros codes */
  "ha",
  "by",
  "gm",
  "gd",
  "ga",
  "gn",
  "ge",
  "is",
  "hp",
  "vi",
  "hl",
  "co",
  "un",
  "ws",
  "hd",
  "cga",
  "sa",
  "ta",
  "sha",
  "sia",
  "bl",
  "ed",
  "agr",
  "dag",
  "cat",
  "pud",
  "th",
  "tk",
  "lag",
  "nc"
};

char *macros[] = {		/* macros texts */
  "~OL~FYHello all!",
  "Bye, I must go! See you later...",
  "~FRGood morning!",
  "~FYGood day!",
  "~FGGood afternoon!",
  "~FBGood night!",
  "~FTGood evening!",
  "I'm sorry... :-(",
  "I'm happy now! :-)",
  "~LIThis is very important, listen:",
  "~FMHelp me, please.",
  "~LI~BY~OL~FGYeah! ~OL~FRhuh... huhhuh... huh... ~FMthat's ~FBCOOL!",
  "I don't understand what you say...",
  "~FTI wish to ask you something:",
  "How do you do?",
  "~LI~OL~BG~FBC~FRo~FMn~FBg~FWr~FYa~RS~BG~LI~FRt~FBu~FTl~FMa~FGt~OL~FRi~FMons!",
  "~LI~OL~FG~BYYou bet! ~OL~FRYou can ~FBsay ~FKthat ~FMagain! ~FBCOOL!",
  "~LI~OL~FY~BGYou bet! ~OL~FRYou can ~FBthink ~FKthat ~FMagain! ~FBCOOL!",
  "~LI~OL~FY~BRYou bet! ~OL~FGYou can ~FBshout ~FKthat ~FMagain! ~FBCOOL!",
  "~LI~OL~FK~BTYou bet! ~OL~FGYou can ~FBsing ~FRthat ~FYagain! ~FBCOOL!",
  "~LI~OL~FR~BYLife ~FGis ~FBbeautiful!!! ~FMThat's ~FBCOOL!",
  "~LI~BR~OL~FBCheck ~FGout ~FKGAEN page: ~FYhttp://www.infoiasi.ro//~busaco/gaen/",
  "~FG~OLI agree this idea! I agree!!",
  "~FR~OLI disagree this point of view! I don't think it's a good idea!...",
  "~BT~OL~FBI'm just climbing on the walls like a real stupid cat!",
  "I want to puddle all day in this garden... :)",
  "I'm very thirsty! You have some drink?",
  "Sorry, today I talked a lot of rubbish... Forget me!",
  "~OLI t ' s   t o o   s l o w   f o r   m e !  ~FRL A G ! ?",
  "~LI~OL~FG~BYYeah! ~OL~FRThis ~FBchat ~FKRULZ!!! ~FMThat's ~FBCOOL!"
};

/* Tones stuff (used by .tone and .say) */
#define MAX_TONES 40

/* the 'tones' of the 'voice' */
char *tones[] = {
  ": ~RS",
  ", using acute sounds: ~RS~FM~OL~LI",
  ", very angry: ~RS~FR~OL~LI",
  ", bashful: ~RS~FB",
  ", with perfidy: ~RS~BM~OL",
  ", with a toady voice: ~RS~FY",
  ", with a perplex tone: ~RS~UL",
  ", with a melodious voice: ~RS~FT~OL",
  ", with a grave voice: ~RS~UL~LI",
  ", using a huge megaphone: ~RS~BB~FY~OL",
  ", very sad: ~RS~FM",
  ", wondering: ~RS~OL~LI~BG",
  ", in a low voice: ~RS~FT",
  ", in a high voice: ~RS~FR~OL~LI",
  ", with an innocent voice: ~RS~FT~OL~LI",
  ", persuasive: ~RS~BY~FR~OL",
  ", kidding: ~RS~BG~OL",
  ", with a peaceful voice: ~RS~FG~OL",
  ", sarcastic: ~RS~BY~FB~OL",
  ", very sleepy: ~RS~BW~FK",
  ", politely: ~RS~BG~FK",
  ", barking, very noisy: ~RS~BR~FY~OL~LI",
  ", very hoarse: ~RS~BM~FK",
  ", with a rusty voice: ~RS~FR",
  ", fairy: ~RS",
  ", gloomily: ~RS~FB~BW~OL",
  ", with a godly voice: ~RS~LI~BR~BW~OL",
  ", irritated: ~RS~LI~OL~FR",
  ", tenderly: ~RS~FG",
  ", very spasmodic: ~RS~LI~FT",
  ", with a melted voice: ~RS~BT~OL~FG",
  " (whistles): ~RS~BB~OL~LI~FY",
  ", with a puffy voice: ~RS~BY~OL~FB",
  ", with a puerile voice: ~RS~BR~OL",
  ", with a grotesque tone: ~RS~LI~RV",
  ", verily: ~RS~BM~OL~LI",
  ", vividly: ~RS~OL~LI",
  ", warily: ~RS~OL~FR~LI",
  " (suspiciously): ~RS~FM~BY~OL",
  ", very shocked: ~RS~BR~FY~OL~LI"
};

char *tone_codes[] = {		/* tones codes */
  "normal",
  "acute",
  "angry",
  "bash",
  "perfid",
  "toady",
  "perplex",
  "melody",
  "grave",
  "mega",
  "sad",
  "wonder",
  "low",
  "high",
  "innocent",
  "persuas",
  "kidding",
  "peace",
  "sarcasm",
  "sleepy",
  "polite",
  "bark",
  "hoarse",
  "rusty",
  "fair",
  "gloom",
  "godly",
  "irritate",
  "tender",
  "spasm",
  "melt",
  "whistle",
  "puffy",
  "puerile",
  "grotesq",
  "verily",
  "vivid",
  "warily",
  "suspicious",
  "shock"
};

#define LANG_SUP '^'		/* used by language() */
#define LANG_SUB '_'

#define MAX_MAINPHR 3		/* login extra phrases */
#define MAX_PHR1   15
#define MAX_PHR2   11
#define MAX_PHR3   18

char *prefix_login_phr[MAX_MAINPHR] = {
  "I wonder if your face is",
  "Wow! Your name is so",
  "Let me guess: you are"
};

char *login_phr1[MAX_PHR1] = {
  "beautiful like the Moon",
  "ugly",
  "green",
  "interesting",
  "unusual",
  "ready for a creamy pie",
  "angelical",
  "like a square",
  "evil",
  "like a good bread",
  "ready to be painted in green",
  "ready to be like an icon",
  "blue",
  "like an angelic sphere",
  "ready to become a mirror"
};

char *login_phr2[MAX_PHR2] = {
  "interesting",
  "unusual",
  "common",
  "important",
  "boring",
  "spooky",
  "eternal",
  "green",
  "childish",
  "forgotten",
  "blue"
};

char *login_phr3[MAX_PHR3] = {
  "a sleepy dreamer",
  "a bloody hacker",
  "a good person",
  "an ugly foolish child",
  "a looser",
  "a painter of the universe",
  "an absurde philosopher",
  "a little dog",
  "a smart programmer",
  "the root",
  "a lover of this chat",
  "nothing",
  "a stupid cat",
  "a bad shrew",
  "a sleep walker",
  "a shifty truant",
  "a mad blue cat",
  "a real GOD"
};

/* Week day greetings */
char *greetings[] = {
  "~OLGAEN:~RS It's ~OLSunday~RS, time to rest... Don't work, just laugh!\n",
  "~OLGAEN:~RS It's ~OL~FRMonday~RS, the beginning of a new wonderful week...\n      It's time to work again!\n",
  "~OLGAEN:~RS Today it's ~OL~FGTuesday~RS, a good day for busy people...\n",
  "~OLGAEN:~RS ~OL~FYWednesday~RS again, middle of the week...Don't be lazy!\n",
  "~OLGAEN:~RS It's only ~OL~FBThursday~RS... Hurry, hurry!\n",
  "~OLGAEN:~RS Today it's ~OL~FMFriday~RS, it's time to slow down?...\n",
  "~OLGAEN:~RS Yes, it's ~OL~FTSaturday~RS, again!\n      You're ready to enjoy this week-end?\n"
};

/*
Colcode values equal the following:
RESET,BOLD,UNDERLINE,BLINK,REVERSE

Foreground & background colours in order..
BLACK,RED,GREEN,YELLOW/ORANGE,
BLUE,MAGENTA,TURQUIOSE,WHITE

'Bell' colour and 'New Line' colour :)
BELL,NEWLINE
*/

char *colcode[NUM_COLS] = {
/* Standard stuff */
  "\033[0m", "\033[1m", "\033[4m", "\033[5m", "\033[7m",
/* Foreground colour */
  "\033[30m", "\033[31m", "\033[32m", "\033[33m",
  "\033[34m", "\033[35m", "\033[36m", "\033[37m",
/* Background colour */
  "\033[40m", "\033[41m", "\033[42m", "\033[43m",
  "\033[44m", "\033[45m", "\033[46m", "\033[47m",
/* so-called colours: bell (\007) and newline */
  "\07", "\n\r"
};

/* Codes used in a string to produce the colours when prepended with a '~' */
char *colcom[NUM_COLS] = {
  "RS", "OL", "UL", "LI", "RV",
  "FK", "FR", "FG", "FY",
  "FB", "FM", "FT", "FW",
  "BK", "BR", "BG", "BY",
  "BB", "BM", "BT", "BW",
/* so-called colours: bell (\007) and newline */
  "EL", "NL"
};

/* Names of months and days */
char *month[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

char *day[7] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

/* NO/YES and OFF/ON text flags */
char *noyes1[] = { " NO", "YES" };
char *noyes2[] = { "NO ", "YES" };
char *offon[] = { "OFF", "ON " };

/* Swears list loaded from swear file */
char swear_words[MAX_SWEARS + 1][WORD_LEN + 1];

/* Some global variables */
char verification[SERV_NAME_LEN + 1];	/* verification string */
char text[ARR_SIZE * 2];	/* text used by some commands */
char word[MAX_WORDS][WORD_LEN + 1];	/* typed words */
char wrd[8][81];		/* words in config file */
char progname[40], confile[40];	/* program/config file names */
time_t boot_time;		/* boot time value */
jmp_buf jmpvar;			/* used in reboot by a signal action */

/* Watch entries */
int watch_pos;
char watch_source[MAX_WATCH][USER_NAME_LEN + 1];
char watch_dest[MAX_WATCH][USER_NAME_LEN + 1];
int watch_runcom[MAX_WATCH];

char homedir[SITE_NAME_LEN + 1];	/* talker's home directory */

/* Dimensions names */
char *dimens_name[MAX_DIMENS] =
  { "~OL~FGGala", "~OL~FRCo~FGlou~FBrs", "~OLUniverse" };

/* Important global variables */
int port[3],			/* talker's ports */
  listen_sock[3],		/* listen sockets associated with ports */
  stport_level,			/* SAINT port maxim level */
  minlogin_level;		/* minimum level of accepted users */
int colour_def,			/* default colour flag */
  password_echo,		/* echoing password flag */
  ignore_sigterm;		/* ignoring SIGTERM flag */
int max_users,			/* maximum # of main port connected users */
  max_clones,			/* maximum clones/user */
  num_of_users,			/* # of connected users (reported by .who) */
  num_of_logins,		/* # of login users (reported by .who) */
  heartbeat;			/* heart beat value */
int login_idle_time,		/* max. idle time at login (in seconds) */
  user_idle_time,		/* max. idle time of users (in seconds) */
  config_line,			/* config file current line */
  word_count;			/* # of typed words (used by word_count()) */
int tyear,			/* current time values */
  tmonth, tday, tmday, twday, thour, tmin, tsec;
int mesg_life,			/* life (in days) of board messages */
  system_logging[MAX_LOGS],	/* logfiles writing-state flags */
  prompt_def,			/* prompt user flag */
  no_prompt;			/* no prompt value */
int force_listen,		/* broadcast force listen flag */
  gatecrash_level,		/* level of users who can enter private skies */
  min_private_users,		/* minimum # of users to set private a sky */
  def_lines;			/* default lines in more() */
int ignore_mp_level,		/* ignoring min_private level */
  rem_user_maxlevel,		/* maximum level of remote users */
  rem_user_deflevel,		/* default level of remote users without local account */
  timeout_maxlevel,		/* maximum level of timed-out users */
  newuser_deflevel,		/* default level of new users */
  accept_prelog_comm;		/* accept prelogin commands flag */
int destructed,			/* destructed user flag */
  mesg_check_hour, mesg_check_min,	/* check messages time */
  net_idle_time;		/* idle of netlinks (in seconds) */
int keepalive_interval,		/* keep alive netlinks value */
  auto_connect,			/* auto connect remote sites flag */
  ban_swearing,			/* ban swearing words flag */
  crash_action;			/* action when the talker is crashing */
int time_out_afks,		/* time out AFK users flag */
  allow_caps_in_name,		/* allow capitals in users name flag */
  rs_countdown,			/* reboot/shutdown countdown (in seconds) */
  event,			/* current event */
  use_hostsfile;		/* using hosts file flag */
int safety,			/* using safety unlink() flag */
  gm_max_words,			/* maximum Hangman words */
  def_pballs,			/* default value of Paintball munition */
  exec_gshell,			/* allow .gsh flag */
  chk_spass_tests;		/* check secure passwords number of tests */
int prayer_number,		/* current prayer number */
  max_hints, max_quotes,	/* maximum # of hints/quotes */
  autosave_counter,		/* autosave users counter */
  autosave_maxiter,		/* autosave users maximum iterations */
  autosave_action,		/* autosave users performed actions */
  mirror,			/* current mirror */
  no_disable_mirror,		/* no disable mirror in autosave */
  cnt_in, cnt_out, cnt_swp,	/* counters of in/out/swap users */
  enable_event,			/* execution of events flag */
  enable_rndgo,			/* enable random .go sky at login time */
  id_readtimeout,		/* # of seconds for identify() timeout */
  random_bhole,			/* random blackhole flag */
  prelog_msgs,			/* # of prelogin messages files */
  wrap_cols,			/* # of columns of wrapped lines */
  hint_at_help,			/* view a hint if you using help flag */
  emote_spacer,			/* add a space to each emote speech flag */
  allow_born_maxlev;		/* allow to born a maxlevel (superior) user flag */
char event_var[WORD_LEN + 1];	/* event variable value */
char utmpfile[SITE_NAME_LEN + 1],	/* common filenames used by gsh_who()... */
  wtmpfile[SITE_NAME_LEN + 1];	/* ... and gsh_last() - system dependent! */
char susers_restrict[MAX_RESTRICT + 1];
				/* superior users default restrictions */
time_t rs_announce, rs_which;	/* used by reboot/shutdown procedures */
UR_OBJECT rs_user;		/* reboot/shutdown user */

/* check some incompatibilities between different Unix-flavours... */
#ifndef GAEN__NO_SUPPORT__
#ifndef __linux
extern char *sys_errlist[];
#define ut_host ut_line		/* for compatibility with SYSV */
#endif
#endif

extern char *long_date ();
