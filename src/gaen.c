/*****************************************************************************
	    NUTS version 3.3.3 - Copyright (C) Neil Robertson 1996

 This software is provided as is. It is not intended as any sort of bullet
 proof system for commercial operation and I accept no liability for any
 problems that may arise from you using it. Since this is freeware (NOT public
 domain, I have not relinquished the copyright) you may distribute it as you
 see fit and you may alter the code to suit your needs.

 Neil Robertson.
 Email	  : neil@ogham.demon.co.uk
 Home page: http://www.wso.co.uk/neil.html (need JavaScript enabled browser)
 NUTS page: http://www.wso.co.uk/nuts.html
 Newsgroup: alt.talkers.nuts

 *****************************************************************************/
/* Written and modified by Sabin-Corneliu Buraga (c)1996-2001 
   (final revision: 17-December-2001)
   
   Email    : busaco@infoiasi.ro
   Home page: http://www.infoiasi.ro/~busaco/
   GAEN page: http://www.infoiasi.ro/~busaco/gaen/	      

   Portions of code written by Victor Tarhon-Onu <mituc@ac.tuiasi.ro>
   For other details, please consult help_credits() function.
  
  ---------------------------------------------------------------------------
*/
#include <stdio.h>
#ifdef _AIX
#include <sys/select.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <netdb.h>
#include <dirent.h>
#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <errno.h>
#include <utmp.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

#include "gaen.h"

#define VERSION  "3.3.3"
#define GAEN_VER "M (18)"
#define GAENDTD_VER "1.2"
#define GSH_VER  "2.7"
#define GSL_VER  "4.2"

#define GAEN_UPDATE "17-Dec-2001"

/* main function */
main (argc, argv)
     int argc;
     char *argv[];
{
  fd_set readmask;
  int i, len;
  char inpstr[ARR_SIZE];
  char *remove_first ();
  UR_OBJECT user, next;
  NL_OBJECT nl;

  strcpy (progname, argv[0]);
/* processing command line parameters... */
  if (argc < 2)
    strcpy (confile, CONFIGFILE);
  else
    {
      if (argc < 2)
	help_usage ();
      else
	{			/* show version */
	  if (!strcmp (argv[1], "-v") || !strcmp (argv[1], "--version"))
	    {
	      printf ("\nGAEN %s @ %s based on NUTS %s\n",
		      GAEN_VER, GAEN_UPDATE, VERSION);
	      exit (14);
	    }
	  if (argc < 3)
	    help_usage ();
	  /* set config file */
	  if (!strcmp (argv[1], "-c") || !strcmp (argv[1], "--config"))
	    strcpy (confile, argv[2]);
	  else
	    /* born a super user account */
	  if (!strcmp (argv[1], "-b") || !strcmp (argv[1], "--born"))
	    born_maxleveluser (argv[2]);
	  else
	    /* wrong options */
	    help_usage ();
	}
    }

/* Startup */
  printf ("\n--->>> GAEN %s - %s (based on NUTS %s) booting... <<<---\n\n",
	  GAEN_VER, GAEN_UPDATE, VERSION);
  init_globals ();
  write_syslog (LOG_CHAT, "\n>>> GAEN ADVANCED TALKER SERVER BOOTING...\n",
		LOG_NOTIME);
  set_date_time ();
  init_signals ();
  load_and_parse_config ();
  load_swear_file (NULL);
  load_commands_file (NULL);
  init_sockets ();
  init_hostsfile ();

  if (auto_connect)
    init_connections ();
  else
    printf ("Skipping connect stage.\n");
  check_messages (NULL, 1);

/* Run in background automatically. */
  switch (fork ())
    {
    case -1:
      boot_exit (11);		/* fork failure */
    case 0:
      break;			/* child continues */
    default:
      sleep (1);
      exit (0);			/* parent dies */
    }
  reset_alarm ();
  printf ("\n--->>> Booted with PID %d <<<---\n\n", getpid ());
  sprintf (text,
	   ">>> GAEN Advanced Server Talker version %s @ %s ...\n>>> Booted successfully with PID %d %s <<<\n\n",
	   GAEN_VER, GAEN_UPDATE, getpid (), long_date (1));
  for (i = 0; i < MAX_LOGS; i++)
    write_syslog (i, text, LOG_NOTIME);

/**** Main program loop. *****/
  setjmp (jmpvar);		/* rescue if we crash and crash_action = IGNORE */
  while (1)
    {
      /* set up mask then wait */
      setup_readmask (&readmask);
      if (select (FD_SETSIZE, &readmask, 0, 0, 0) == -1)
	continue;

      /* check for connection to listen sockets */
      for (i = 0; i < 3; ++i)
	{
	  if (FD_ISSET (listen_sock[i], &readmask))
	    accept_connection (listen_sock[i], i);
	}

      /* Cycle through client-server connections to other talkers */
      for (nl = nl_first; nl != NULL; nl = nl->next)
	{
	  no_prompt = 0;
	  if (nl->type == UNCONNECTED || !FD_ISSET (nl->socket, &readmask))
	    continue;
	  /* See if remote site has disconnected */
	  if (!(len = read (nl->socket, inpstr, sizeof (inpstr) - 3)))
	    {
	      if (nl->stage == UP)
		sprintf (text, "NETLINK: Remote disconnect by %s.\n",
			 nl->service);
	      else
		sprintf (text, "NETLINK: Remote disconnect by site %s.\n",
			 nl->site);
	      write_syslog (LOG_LINK, text, LOG_TIME);
	      sprintf (text,
		       "~OLGAEN:~RS Lost link to service %s in the %s.\n",
		       nl->service, nl->connect_room->name);
	      write_room (NULL, text);
	      shutdown_netlink (nl);
	      continue;
	    }
	  inpstr[len] = '\0';
	  exec_netcom (nl, inpstr);
	}

      /* Cycle through users. Use a while loop instead of a for because
         user structure may be destructed during loop in which case we
         may lose the user->next link. */
      user = user_first;
      while (user != NULL)
	{
	  next = user->next;	/* store in case user object is destructed */
	  /* If remote user or clone ignore */
	  if (user->type != USER_TYPE)
	    {
	      user = next;
	      continue;
	    }

	  /* see if any data on socket else continue */
	  if (!FD_ISSET (user->socket, &readmask))
	    {
	      user = next;
	      continue;
	    }

	  /* see if client (eg telnet) has closed socket */
	  inpstr[0] = '\0';
	  if (!(len = read (user->socket, inpstr, sizeof (inpstr))))
	    {
	      disconnect_user (user);
	      user = next;
	      continue;
	    }
	  /* ignore control code replies */
	  if ((unsigned char) inpstr[0] == 255)
	    {
	      user = next;
	      continue;
	    }
	  /* Deal with input chars. If the following if test succeeds we
	     are dealing with a character mode client so call function. */
	  if (inpstr[len - 1] >= 32 || user->buffpos)
	    {
	      if (get_charclient_line (user, inpstr, len))
		goto GOT_LINE;
	      user = next;
	      continue;
	    }
	  else
	    terminate (inpstr);

	GOT_LINE:
	  no_prompt = 0;
	  com_num = -1;
	  force_listen = 0;
	  destructed = 0;
	  user->buff[0] = '\0';
	  user->buffpos = 0;
	  user->last_input = time (0);
	  if (user->login)
	    {
	      login_user (user, inpstr);
	      user = next;
	      continue;
	    }
	  /* If a dot on its own then execute last inpstr unless its a misc
	     op or the user is on a remote site */
	  if (!user->misc_op)
	    {
	      if (!strcmp (inpstr, ".") && user->inpstr_old[0])
		{
		  strcpy (inpstr, user->inpstr_old);
		  sprintf (text, "%s\n", inpstr);
		  write_user (user, text);
		}
	      /* else save current one for next time */
	      else
		{
		  if (inpstr[0])
		    strncpy (user->inpstr_old, inpstr, REVIEW_LEN);
		}
	    }

	  /* Main input check */
	  clear_words ();
	  word_count = wordfind (user, inpstr);
	  if (user->afk)
	    {
	      write_user (user, ">>>You are no longer AFK.\n");
	      if (user->vis)
		{
		  sprintf (text, ">>>%s comes back from being AFK.\n",
			   user->name);
		  write_room_except (user->room, text, user);
		}
	      user->afk = 0;
	      event = E_NOAFK;
	      strcpy (event_var, user->savename);
	    }

	  if (!word_count)
	    {
	      if (misc_ops (user, inpstr))
		{
		  user = next;
		  continue;
		}
	      if (user->room == NULL)
		{
		  sprintf (text, "ACT %s NL\n", user->savename);
		  write_sock (user->netlink->socket, text);
		}
	      if (user->command_mode)
		prompt (user);
	      user = next;
	      continue;
	    }
	  if (misc_ops (user, inpstr))
	    {
	      user = next;
	      continue;
	    }
	  com_num = -1;
	  /* Checking for some short-cuts... */
	  if (user->command_mode || strchr (".`;!<>-#?$/'[=\\", inpstr[0]))
	    exec_com (user, inpstr);
	  else
	    say (user, inpstr);
	  if (!destructed)
	    {
	      if (user->room != NULL)
		prompt (user);
	      else
		{
		  switch (com_num)
		    {
		    case -1:	/* Unknown command */
		    case HOME:
		    case QUIT:
		    case MODE:
		    case PROMPT:
		    case SUICIDE:
		    case REBOOT:
		    case SHUTDOWN:
		      prompt (user);
		    }
		}
	    }
	  user = next;
	}
    }				/* end while */
}

/************ COMMAND LINE RESOLVE FUNCTIONS ************/

/*** Write help usage ***/
help_usage ()
{
  fprintf (stderr, "%s: help information (GAEN %s @ %s)\n\n",
	   progname, GAEN_VER, GAEN_UPDATE);
  fprintf (stderr, ">>>Usage: %s [ <option> ]\n", progname);
  fprintf (stderr, ">>>Valid options are:\n\n");
  fprintf (stderr,
	   "\t-b, --born <name>   to create an administration account\n");
  fprintf (stderr,
	   "\t-c, --config <file> to use the configuration file <file>\n");
  fprintf (stderr,
	   "\t-v, --version       output version information and exit\n");
  fprintf (stderr,
	   "\n>>>If no options, default configuration file is used.\n\n");
  exit (14);
}

/*** Born a super-user (maximum level) account ***/
born_maxleveluser (name)
     char *name;
{
  int i;
  FILE *fp;
  char filename[80];

  if (strlen (name) < PASS_MIN_LEN || strlen (name) > PASS_MAX_LEN)
    {
      fprintf (stderr, "GAEN: Invalid username length.\n");
      exit (13);
    }
  for (i = 0; i < strlen (name); ++i)
    {
      if (!isalpha (name[i]))
	{
	  fprintf (stderr, "GAEN: Only letters are allowed in a name.\n");
	  exit (13);
	}
    }
  strtolower (name);
  name[0] = toupper (name[0]);
  /* check if user is banned */
  if (user_banned (name))
    {
      fprintf (stderr,
	       "GAEN: That user is banned. Check '%s/%s' file, please.\n",
	       DATAFILES, USERBAN);
      exit (13);
    }
  sprintf (filename, "%s/%s.D", USERFILES, name);
  s_unlink (filename);		/* to backup old file if exists */
  if ((fp = fopen (filename, "w")) == NULL)
    {
      fprintf (stderr, "GAEN: Cannot create user file.\n");
      exit (13);
    }
  /* write the .D file */
  fprintf (fp, "%s\n", (char *) crypt (name, PASS_SALT));
  fprintf (fp, "%d %d %d %d %d %d %d %d %d %d\n", 0, 0, 0, 0, SGOD, 0, 0, 0,
	   0, 0);
  fprintf (fp, "gaen.ro\na real ~FG~OL!!!\nenters\ngoes\n");
  fclose (fp);
  printf ("\nGAEN: Maximum level user %s created (see '%s' file).\n", name,
	  filename);
  /* adjust the allow user files */
  for (i = MIN_LEV_ALLOW; i <= MAX_LEV_ALLOW; i++)
    {
      sprintf (filename, "%s/%s.%d", DATAFILES, ALLOWFILE, i);
      if ((fp = fopen (filename, "a")) == NULL)
	{
	  fprintf (stderr, "GAEN: Warning, cannot append allow file '%s'.\n",
		   filename);
	}
      else
	{
	  fprintf (fp, "%s\n", name);
	  printf ("GAEN: User added to allow file '%s'...\n", filename);
	  fclose (fp);
	}
    }
  exit (12);
}

/************ MAIN LOOP FUNCTIONS ************/

/*** Set up readmask for select ***/
setup_readmask (mask)
     fd_set *mask;
{
  UR_OBJECT user;
  NL_OBJECT nl;
  int i;

  FD_ZERO (mask);
  for (i = 0; i < 3; ++i)
    FD_SET (listen_sock[i], mask);
/* Do users */
  for (user = user_first; user != NULL; user = user->next)
    if (user->type == USER_TYPE)
      FD_SET (user->socket, mask);

/* Do client-server stuff */
  for (nl = nl_first; nl != NULL; nl = nl->next)
    if (nl->type != UNCONNECTED)
      FD_SET (nl->socket, mask);
}


/*** Accept incoming connections on listen sockets ***/
accept_connection (lsock, num)
     int lsock, num;
{
  UR_OBJECT user, create_user ();
  NL_OBJECT create_netlink ();
  char *get_ip_address (), site[80], filename[80];
  struct sockaddr_in acc_addr;
  int accept_sock, size;

  size = sizeof (struct sockaddr_in);
  accept_sock = accept (lsock, (struct sockaddr *) &acc_addr, &size);
  if (num == 2)
    {
      accept_server_connection (accept_sock, acc_addr);
      return;
    }
  strcpy (site, get_ip_address (acc_addr));
  if (site_banned (site))
    {
      write_sock (accept_sock,
		  "\n\r>>>Logins from your site are banned. Sorry...\n\n\r");
      close (accept_sock);
      sprintf (text, "~OLGAEN:~RS Attempted login from banned site %s.\n",
	       site);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, NULL);
      return;
    }
  sprintf (filename, "%s.%d", PRELOG_MSG, random () % prelog_msgs + 1);
  more (NULL, accept_sock, filename);	/* send pre-login message */
  if (num_of_users + num_of_logins >= max_users && !num)
    {
      write_sock (accept_sock,
		  "\n\r>>>Sorry, the talker is full at the moment. Please, return later...\n\n\r");
      close (accept_sock);
      return;
    }
  if ((user = create_user ()) == NULL)
    {
      sprintf (text,
	       "\n\r%s: unable to create session. Sorry... Please, return later...\n\n\r",
	       syserror);
      write_sock (accept_sock, text);
      close (accept_sock);
      return;
    }
  user->socket = accept_sock;
  user->login = 3;
  user->last_input = time (0);
  if (!num)
    user->port = port[0];
  else
    {
      user->port = port[1];
      write_user (user,
		  "--->>> Super port login (only for superior users) <<<---\n\n");
      user->dimension = MAX_DIMENS - 1;
    }
  strcpy (user->site, site);
  user->site_port = (int) ntohs (acc_addr.sin_port);
  if (user->site_port == port[0] || user->site_port == port[1] ||
      user->site_port == port[2] || !user->site_port)
    {
      sprintf (text, "~OLGAEN:~RS Wrong port for %s from site %s\n",
	       user->savename, site);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, NULL);
      disconnect_user (user);
    }
  echo_on (user);
  write_user (user, DEF_NAME_MSG);
  num_of_logins++;
}


/*** Get net address of accepted connection ***/
char *
get_ip_address (acc_addr)
     struct sockaddr_in acc_addr;
{
  static char site[80];
  struct hostent *host;

  strcpy (site, (char *) inet_ntoa (acc_addr.sin_addr));	/* get number addr */
  if (!check_host (site))
    {				/* host name not found in hosts file */
      if ((host = gethostbyaddr ((char *) &acc_addr.sin_addr, 4, AF_INET)) !=
	  NULL)
	{			/* not found in hosts file... then it will be added */
	  add_host (site, host->h_name);

	  strcpy (site, host->h_name);	/* copy name addr. */
	}
    }
  strtolower (site);
  return site;
}

/*** Check if a host is in hosts file ***/
check_host (site)
     char *site;
{
  FILE *fp;
  char siteaddr[82], sitename[82], filename[80];

  if (!use_hostsfile)
    return 0;
  sprintf (filename, "%s/%s", DATAFILES, HOSTSFILE);
  if (!(fp = fopen (filename, "r")))
    return 0;
  fscanf (fp, "%s %s", siteaddr, sitename);
  while (!feof (fp))
    {
      if (!strcmp (site, siteaddr))
	{
	  fclose (fp);
	  strcpy (site, sitename);
	  return 1;
	}
      fscanf (fp, "%s %s", siteaddr, sitename);
    }
  fclose (fp);
  return 0;
}

/*** Init the hosts file ***/
init_hostsfile ()
{
  FILE *fp;
  char filename[80];

  if (!use_hostsfile)
    return;
  sprintf (filename, "%s/%s", DATAFILES, HOSTSFILE);
  if (!(fp = fopen (filename, "r")))
    {
      if (!(fp = fopen (filename, "w")))
	{
	  write_syslog (LOG_ERR, "Cannot read file in add_host().\n", LOG_NOTIME);
	  return;
	}
      /* store the default IP for localhost */
      fprintf (fp, "127.0.0.1 localhost\n");
      fclose (fp);
      return;
    }
}

/*** Add a site in hosts file ***/
add_host (siteaddr, sitename)
     char *siteaddr, *sitename;
{
  FILE *fp;
  char filename[80], asite[80], aname[80];

  if (!use_hostsfile)
    return;
  sprintf (filename, "%s/%s", DATAFILES, HOSTSFILE);
  if (!(fp = fopen (filename, "r")))
    {
      write_syslog (LOG_ERR, "Cannot read file in add_host().\n", LOG_NOTIME);
      return;
    }
  fscanf (fp, "%s %s", asite, aname);
  while (!feof (fp))
    {
      if (!strcmp (asite, siteaddr))
	{
	  fclose (fp);		/* found, so don't want to write it to file */
	  return;
	}
      fscanf (fp, "%s %s", asite, aname);
    }
  fclose (fp);
  if (!(fp = fopen (filename, "a")))
    {
      write_syslog (LOG_ERR, "Cannot append file in add_host().\n", LOG_NOTIME);
      return;
    }
  fprintf (fp, "%s %s\n", siteaddr, sitename);
  fclose (fp);
}

/*** See if users site is banned ***/
site_banned (site)
     char *site;
{
  FILE *fp;
  char line[82], filename[80];

  sprintf (filename, "%s/%s", DATAFILES, SITEBAN);
  if (!(fp = fopen (filename, "r")))
    return 0;
  fscanf (fp, "%s", line);
  while (!feof (fp))
    {
      if (strstr (site, line))
	{
	  fclose (fp);
	  return 1;
	}
      fscanf (fp, "%s", line);
    }
  fclose (fp);
  return 0;
}


/*** See if user is banned ***/
user_banned (name)
     char *name;
{
  FILE *fp;
  char line[82], filename[80];

  sprintf (filename, "%s/%s", DATAFILES, USERBAN);
  if (!(fp = fopen (filename, "r")))
    return 0;
  fscanf (fp, "%s", line);
  while (!feof (fp))
    {
      if (!strcmp (line, name))	/* found */
	{
	  fclose (fp);
	  return 1;
	}
      fscanf (fp, "%s", line);
    }
  fclose (fp);
/* not found */
  return 0;
}


/*** Attempt to get '\n' terminated line of input from a character
     mode client else store data read so far in user buffer. ***/
get_charclient_line (user, inpstr, len)
     UR_OBJECT user;
     char *inpstr;
     int len;
{
  int l;

  for (l = 0; l < len; ++l)
    {
      /* see if delete entered */
      if (inpstr[l] == 8 || inpstr[l] == 127)
	{
	  if (user->buffpos)
	    {
	      user->buffpos--;
	      if (user->charmode_echo)
		write_user (user, "\b \b");
	    }
	  continue;
	}
      user->buff[user->buffpos] = inpstr[l];
      /* See if end of line */
      if (inpstr[l] < 32 || user->buffpos + 2 == ARR_SIZE)
	{
	  terminate (user->buff);
	  strcpy (inpstr, user->buff);
	  if (user->charmode_echo)
	    write_user (user, "\n");
	  return 1;
	}
      user->buffpos++;
    }
  if (user->charmode_echo
      && ((user->login != 2 && user->login != 1) || password_echo))
    write (user->socket, inpstr, len);
  return 0;
}


/*** Put string terminate char. at first char < 32 ***/
terminate (str)
     char *str;
{
  int i;
  for (i = 0; i < ARR_SIZE; ++i)
    {
      if (*(str + i) < 32)
	{
	  *(str + i) = 0;
	  return;
	}
    }
  str[i - 1] = 0;
}


/*** Get words from sentence. This function prevents the words in the
     sentence from writing off the end of a word array element. This is
     difficult to do with sscanf() hence we use this function instead. ***/
wordfind (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  int wn, wpos;

  wn = 0;
  wpos = 0;
  do
    {
      while (*inpstr < 33)
	if (!*inpstr++)
	  return wn;
      while (*inpstr > 32 && wpos < WORD_LEN - 1)
	{
	  word[wn][wpos] = *inpstr++;
	  wpos++;
	}
      word[wn][wpos] = '\0';
      if (word[wn][0] == '{')	/* substitution of pre-defined variables */
	strcpy (word[wn], user->var1);
      else if (word[wn][0] == '}')
	strcpy (word[wn], user->var2);
      else if (word[wn][0] == '*')
	strcpy (word[wn], event_var);
      wn++;
      wpos = 0;
    }
  while (wn < MAX_WORDS);

  return wn - 1;
}


/*** clear word array etc. ***/
clear_words ()
{
  int w;
  for (w = 0; w < MAX_WORDS; ++w)
    word[w][0] = '\0';
  word_count = 0;
}

/************ PARSE CONFIG FILE **************/

load_and_parse_config ()
{
  FILE *fp;
  char line[81];		/* Should be long enough */
  char c, filename[80];
  int d, i, section_in, got_init, got_rooms;
  RM_OBJECT rm1, rm2;
  NL_OBJECT nl;

  section_in = 0;
  got_init = 0;
  got_rooms = 0;

  sprintf (filename, "%s/%s", CONFFILES, confile);
  printf ("Parsing config file \"%s\"... ", filename);
  fflush (stdout);
  if (!(fp = fopen (filename, "r")))
    {
      perror ("cannot open");
      boot_exit (1);
    }
  printf ("found.\n");
/* Main reading loop */
  config_line = 0;
  fgets (line, 81, fp);
  while (!feof (fp))
    {
      config_line++;
      for (i = 0; i < 8; ++i)
	wrd[i][0] = '\0';
      sscanf (line, "%s %s %s %s %s %s %s %s",
	      wrd[0], wrd[1], wrd[2], wrd[3], wrd[4], wrd[5], wrd[6], wrd[7]);
      if (wrd[0][0] == '#' || wrd[0][0] == '\0')
	{
	  fgets (line, 81, fp);
	  continue;
	}
      /* See if new section */
      if (wrd[0][strlen (wrd[0]) - 1] == ':')
	{
	  if (!strcmp (wrd[0], "INIT:"))	/* INIT section */
	    section_in = 1;
	  else if (!strcmp (wrd[0], "ROOMS1:"))	/* ROOMSx sections */
	    section_in = 2;
	  else if (!strcmp (wrd[0], "ROOMS2:"))
	    section_in = 3;
	  else if (!strcmp (wrd[0], "ROOMS3:"))
	    section_in = 4;
	  else if (!strcmp (wrd[0], "SITES:"))	/* SITES section */
	    section_in = 5;
	  else
	    {			/* unknown section */
	      fprintf (stderr,
		       "GAEN: Unknown section header on line %d.\n",
		       config_line);
	      fclose (fp);
	      boot_exit (1);
	    }
	}
      switch (section_in)
	{			/* parse sections */
	case 1:
	  parse_init_section ();
	  got_init = 1;
	  break;
	case 2:
	  parse_rooms_section (0);
	  got_rooms = 1;
	  break;
	case 3:
	  parse_rooms_section (1);
	  got_rooms = 2;
	  break;
	case 4:
	  parse_rooms_section (2);
	  got_rooms = 3;
	  break;
	case 5:
	  parse_sites_section ();
	  break;
	default:
	  fprintf (stderr, "GAEN: Section header expected on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      fgets (line, 100, fp);
    }
  fclose (fp);

/* See if required sections were present (SITES is optional) and if
   required parameters were set. */
  if (!got_init)
    {
      fprintf (stderr, "GAEN: INIT section missing from config file.\n");
      boot_exit (1);
    }
  if (!got_rooms)
    {
      fprintf (stderr,
	       "GAEN: One of the ROOMSx sections missing from config file.\n");
      boot_exit (1);
    }
  if (!verification[0])
    {
      fprintf (stderr, "GAEN: Verification not set in config file.\n");
      boot_exit (1);
    }
  if (!port[0])
    {
      fprintf (stderr, "GAEN: Main port number not set in config file.\n");
      boot_exit (1);
    }
  if (!port[1])
    {
      fprintf (stderr, "GAEN: Saint port number not set in config file.\n");
      boot_exit (1);
    }
  if (!port[2])
    {
      fprintf (stderr, "GAEN: Link port number not set in config file.\n");
      boot_exit (1);
    }
  if (port[0] == port[1] || port[1] == port[2] || port[0] == port[2])
    {
      fprintf (stderr, "GAEN: Port numbers must be unique.\n");
      boot_exit (1);
    }
  if (room_first == NULL)
    {
      fprintf (stderr, "GAEN: No skies configured in config file.\n");
      boot_exit (1);
    }

/* Parsing done, now check data is valid. Check room stuff first. */
  for (d = 0; d < MAX_DIMENS; d++)
    for (rm1 = room_first[d]; rm1 != NULL; rm1 = rm1->next)
      {
	for (i = 0; i < MAX_LINKS; ++i)
	  {
	    if (!rm1->link_label[i][0])
	      break;
	    for (rm2 = room_first[d]; rm2 != NULL; rm2 = rm2->next)
	      {
		if (rm1 == rm2)
		  continue;
		if (!strcmp (rm1->link_label[i], rm2->label))
		  {
		    rm1->link[i] = rm2;
		    break;
		  }
	      }
	    if (rm1->link[i] == NULL)
	      {
		fprintf (stderr,
			 "GAEN: Sky %s has undefined link label '%s'.\n",
			 rm1->name, rm1->link_label[i]);
		boot_exit (1);
	      }
	  }
      }

/* Check external links */
  for (d = 0; d < MAX_DIMENS; d++)
    for (rm1 = room_first[d]; rm1 != NULL; rm1 = rm1->next)
      {
	for (nl = nl_first; nl != NULL; nl = nl->next)
	  {
	    if (!strcmp (nl->service, rm1->name))
	      {
		fprintf (stderr,
			 "GAEN: Service name %s is also the name of a sky.\n",
			 nl->service);
		boot_exit (1);
	      }
	    if (rm1->netlink_name[0]
		&& !strcmp (rm1->netlink_name, nl->service))
	      {
		rm1->netlink = nl;
		break;
	      }
	  }
	if (rm1->netlink_name[0] && rm1->netlink == NULL)
	  {
	    fprintf (stderr,
		     "GAEN: Service name %s not defined for sky %s.\n",
		     rm1->netlink_name, rm1->name);
	    boot_exit (1);
	  }
      }

/*** Load room descriptions & default topics (if any) ***/
  for (d = 0; d < MAX_DIMENS; d++)
    for (rm1 = room_first[d]; rm1 != NULL; rm1 = rm1->next)
      {
	sprintf (filename, "%s/%s.R", DATAFILES, rm1->name);
	if (!(fp = fopen (filename, "r")))
	  {
	    fprintf (stderr,
		     "GAEN: Can't open description file for sky %s.\n",
		     rm1->name);
	    sprintf (text,
		     "Warning: Couldn't open description file for sky %s.\n",
		     rm1->name);
	    write_syslog (LOG_ERR, text, LOG_TIME);
	    continue;
	  }
	i = 0;
	c = getc (fp);
	while (!feof (fp))
	  {
	    if (i == ROOM_DESC_LEN)
	      {
		fprintf (stderr, "GAEN: Description too long for sky %s.\n",
			 rm1->name);
		sprintf (text, "Warning: Description too long for sky %s.\n",
			 rm1->name);
		write_syslog (LOG_ERR, text, LOG_TIME);
		break;
	      }
	    rm1->desc[i] = c;
	    c = getc (fp);
	    ++i;
	  }
	rm1->desc[i] = '\0';
	fclose (fp);
	/* try to load topic for a room... */
	sprintf (filename, "%s/%s.T", DATAFILES, rm1->name);
	if (!(fp = fopen (filename, "r")))
	  {
	    sprintf (text, "Warning: Couldn't open topic file for sky %s.\n",
		     rm1->name);
	    write_syslog (LOG_ERR, text, LOG_TIME);
	    continue;
	  }
	fgets (line, TOPIC_LEN + 2, fp);
	line[strlen (line) - 1] = '\0';
	strcpy (rm1->topic, line);
	fclose (fp);
      }
}

/*** Parse init section ***/
parse_init_section ()
{
  static int in_section = 0;
  int op, val;
  char *options[] = {
    "mainport", "stport", "linkport", "logging", "minlogin_level",
    "mesg_life",
    "stport_level", "prompt_def", "gatecrash_level", "min_private",
    "ignore_mp_level",
    "rem_user_maxlevel", "rem_user_deflevel", "verification",
    "mesg_check_time",
    "max_users", "heartbeat", "login_idle_time", "user_idle_time",
    "password_echo",
    "ignore_sigterm", "auto_connect", "max_clones", "ban_swearing",
    "crash_action",
    "colour_def", "time_out_afks", "allow_caps_in_name", "max_hints",
    "max_quotes",
    "def_mirror", "safety", "use_hosts_file", "words_dict", "gshell",
    "paged_lines",
    "time_out_maxlevel", "def_pballs", "chk_spass_tests", "utmpfile",
    "wtmpfile",
    "autosave_action", "enable_events", "susers_restrict", "enable_rndgo",
    "random_bhole", "prelog_msgs", "newuser_deflevel", "wrap_cols",
    "hint_at_help",
    "emote_spacer", "accept_prelog_comm", "allow_born_maxlev", "*"
  };

  if (!strcmp (wrd[0], "INIT:"))
    {
      if (++in_section > 1)
	{
	  fprintf (stderr,
		   "GAEN: Unexpected INIT section header on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;
    }
  op = 0;
  while (strcmp (options[op], wrd[0]))
    {
      if (options[op][0] == '*')
	{
	  fprintf (stderr, "GAEN: Unknown INIT option on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      ++op;
    }
  if (!wrd[1][0])
    {
      fprintf (stderr, "GAEN: Required parameter missing on line %d.\n",
	       config_line);
      boot_exit (1);
    }
  if (wrd[2][0] && wrd[2][0] != '#')
    {
      fprintf (stderr,
	       "GAEN: Unexpected word following init parameter on line %d.\n",
	       config_line);
      boot_exit (1);
    }
  val = atoi (wrd[1]);
  switch (op)
    {
    case 0:			/* ports used for connect (main, superior, link) */
    case 1:
    case 2:
      if ((port[op] = val) < 1 || val > 65535)
	{
	  fprintf (stderr, "GAEN: Illegal port number on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 3:			/* system logging flag */
      if ((system_logging[LOG_CHAT] = yn_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: System_logging must be YES or NO on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      for (val = 0; val < MAX_LOGS; val++)
	system_logging[val] = yn_check (wrd[1]);
      return;

    case 4:			/* minimum level of users who can login */
      if ((minlogin_level = get_level (wrd[1])) == -1)
	{
	  if (strcmp (wrd[1], "NONE"))
	    {
	      fprintf (stderr, "GAEN: Unknown level specifier on line %d.\n",
		       config_line);
	      boot_exit (1);
	    }
	  minlogin_level = -1;
	}
      return;

    case 5:			/* message lifetime */
      if ((mesg_life = val) < 1)
	{
	  fprintf (stderr, "GAEN: Illegal message lifetime on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 6:			/* stport_level */
      if ((stport_level = get_level (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Unknown level specifier for superusers port on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 7:			/* prompt defaults */
      if ((prompt_def = onoff_check (wrd[1])) == -1)
	{
	  fprintf (stderr, "GAEN: Prompt_def must be ON or OFF on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 8:			/* gatecrash level */
      if ((gatecrash_level = get_level (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Unknown level specifier for gatecrash level on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 9:			/* minimum number of users for a private room */
      if (val < 1)
	{
	  fprintf (stderr,
		   "GAEN: Minimum number of users for a private room too low on line %d (.\n",
		   config_line);
	  boot_exit (1);
	}
      min_private_users = val;
      return;

    case 10:			/* minimum level to ignore above */
      if ((ignore_mp_level = get_level (wrd[1])) == -1)
	{
	  fprintf (stderr, "GAEN: Unknown level specifier on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 11:
      /* Max level a remote user can remotely log in as if he has an account
         ie if level set to SAINT a GOD can only be a SAINT if logging in from
         another server. */
      if ((rem_user_maxlevel = get_level (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Unknown level specifier for maximum level of a remote user on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 12:
      /* Default level of remote user who does not have an account on site */
      if ((rem_user_deflevel = get_level (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Unknown level of remote user who does not have an account on site specifier on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 13:			/* verification string to connect to other talkers */
      if (strlen (wrd[1]) > SERV_NAME_LEN)
	{
	  fprintf (stderr, "GAEN: Verification too long on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      strcpy (verification, wrd[1]);
      return;

    case 14:			/* mesg_check_time */
      if (wrd[1][2] != ':'
	  || strlen (wrd[1]) > 5
	  || !isdigit (wrd[1][0])
	  || !isdigit (wrd[1][1])
	  || !isdigit (wrd[1][3]) || !isdigit (wrd[1][4]))
	{
	  fprintf (stderr,
		   "GAEN: Invalid time on line %d (correct syntax is 'hh:mm').\n",
		   config_line);
	  boot_exit (1);
	}
      wrd[1][2] = ' ';
      sscanf (wrd[1], "%d %d", &mesg_check_hour, &mesg_check_min);
      if (mesg_check_hour > 23 || mesg_check_min > 59)
	{
	  fprintf (stderr, "GAEN: Invalid time on line %d.\n", config_line);
	  boot_exit (1);
	}
      return;

    case 15:			/* number of users who can connect to usual port */
      if ((max_users = val) < 1)
	{
	  fprintf (stderr, "GAEN: Invalid value for max_users on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 16:			/* heartbeat value */
      if ((heartbeat = val) < 1)
	{
	  fprintf (stderr, "GAEN: Invalid value for heartbeat on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 17:			/* login idle time (in seconds) */
      if ((login_idle_time = val) < 10)
	{
	  fprintf (stderr,
		   "GAEN: Invalid value for login_idle_time on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 18:			/* user idle time (in seconds) */
      if ((user_idle_time = val) < 10)
	{
	  fprintf (stderr,
		   "GAEN: Invalid value for user_idle_time on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 19:			/* echo password flag */
      if ((password_echo = yn_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Password_echo must be YES or NO on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 20:			/* ignoring SIGTERM flag */
      if ((ignore_sigterm = yn_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Ignore_sigterm must be YES or NO on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 21:			/* auto connection to other talkers flag */
      if ((auto_connect = yn_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Auto_connect must be YES or NO on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 22:			/* user maximum clones */
      if ((max_clones = val) < 0)
	{
	  fprintf (stderr, "GAEN: Invalid value for max_clones on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 23:			/* ban swears flag */
      if ((ban_swearing = yn_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Ban_swearing must be YES or NO on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 24:			/* action when talker is crashing (NONE, IGNORE, REBOOT) */
      if (!strcmp (wrd[1], "NONE"))
	crash_action = 0;
      else if (!strcmp (wrd[1], "IGNORE"))
	crash_action = 1;
      else if (!strcmp (wrd[1], "REBOOT"))
	crash_action = 2;
      else
	{
	  fprintf (stderr,
		   "GAEN: Crash_action must be NONE, IGNORE or REBOOT on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 25:			/* colour default flag for new users */
      if ((colour_def = onoff_check (wrd[1])) == -1)
	{
	  fprintf (stderr, "GAEN: Colour_def must be ON or OFF on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 26:			/* if AFK users are timed out flag */
      if ((time_out_afks = yn_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Time_out_afks must be YES or NO on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 27:			/* allow caps in user name flag */
      if ((allow_caps_in_name = yn_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Allow_caps_in_name must be YES or NO on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

      /* options added in GAEN versions */
    case 28:			/* maximum hints files */
      if ((max_hints = val) < 0)
	{
	  fprintf (stderr,
		   "GAEN: Invalid value for max_hints on line %d. A decimal number is required.\n",
		   config_line);
	  boot_exit (1);
	}
      if (max_hints == 0)
	max_hints = MAX_HINTS;
      return;

    case 29:			/* maximum quotes files */
      if ((max_quotes = val) < 0)
	{
	  fprintf (stderr,
		   "GAEN: Invalid value for max_quotes on line %d. A decimal number is required.\n",
		   config_line);
	  boot_exit (1);
	}
      if (max_quotes == 0)
	max_quotes = MAX_QUOTES;
      return;

    case 30:			/* default mirror */
      if ((mirror = val) < 0 || mirror >= MAX_MIRRORS)
	{
	  fprintf (stderr, "GAEN: Invalid value for def_mirror on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      for (val = 0; val <= MAX_LEVELS; val++)	/* level names */
	strcpy (level_name[val], level_name_mirror[mirror][val]);
      return;

    case 31:			/* safety erase file flag */
      if ((safety = yn_check (wrd[1])) == -1)
	{
	  fprintf (stderr, "GAEN: Safety must be YES or NO on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 32:			/* using hostfile flag */
      if ((use_hostsfile = yn_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Use_hostsfile parameter must be YES or NO on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 33:			/* number of dictionaries words for Hangman */
      if ((gm_max_words = val) < 0)
	{
	  fprintf (stderr,
		   "GAEN: Invalid value for words_dict on line %d. A decimal number is required.\n",
		   config_line);
	  boot_exit (1);
	}
      if (!gm_max_words)
	gm_max_words = GM_MAX_WORDS;
      return;

    case 34:			/* using gshell flag */
      if ((exec_gshell = onoff_check (wrd[1])) == -1)
	{
	  fprintf (stderr, "GAEN: gshell must be ON or OFF on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 35:			/* default lines in more() */
      if ((def_lines = val) <= 0)
	{
	  fprintf (stderr,
		   "GAEN: Invalid value for def_lines on line %d. A decimal number is required.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 36:			/* maximum level of time-out users  */
      if ((timeout_maxlevel = get_level (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Unknown level specifier for time_out_maxlevel on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 37:			/* default munition for Paintball */
      if ((def_pballs = val) < 0)
	{
	  fprintf (stderr,
		   "GAEN: Invalid value for def_pballs on line %d. A decimal number is required.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 38:			/* check secure passwords number of tests */
      if ((chk_spass_tests = val) < 0)
	{
	  fprintf (stderr,
		   "GAEN: Invalid value for chk_spass_tests on line %d. A decimal number is required.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 39:			/* set utmpfile name for gsh_last() */
      strcpy (utmpfile, wrd[1]);
      return;

    case 40:			/* set wtmpfile name for gsh_who() */
      strcpy (wtmpfile, wrd[1]);
      return;

    case 41:			/* set autosave users performed actions */
      if (!strcmp (wrd[1], "none"))
	autosave_action = AS_NONE;
      else if (!strcmp (wrd[1], "greet"))
	autosave_action = AS_GREET;
      else if (!strcmp (wrd[1], "quote"))
	autosave_action = AS_QUOTE;
      else if (!strcmp (wrd[1], "all"))
	autosave_action = AS_ALL;
      else
	{
	  fprintf (stderr,
		   "GAEN: Invalid value for autosave_action on line %d.\n(Use only 'none', 'quote', 'greet' or 'all').\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 42:			/* execute events flag */
      if ((enable_event = onoff_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: enable_events must be ON or OFF on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 43:			/* default restrictions mask for superior users */
      strcpy (susers_restrict, wrd[1]);
      return;

    case 44:			/* enable random .go sky at login time */
      if ((enable_rndgo = onoff_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: enable_rndgo must be ON or OFF on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 45:			/* enable random .blackhole */
      if ((random_bhole = onoff_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: random_bhole must be ON or OFF on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 46:			/* number of prelogin messages */
      if ((prelog_msgs = val) <= 0)
	{
	  fprintf (stderr,
		   "GAEN: Invalid value for prelog_msgs on line %d. A decimal number is required.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 47:			/* new users default level */
      if ((newuser_deflevel = get_level (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: Unknown level specifier for newuser_deflevel on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      /* security check */
      if (newuser_deflevel > MAX_LEV_NEWUSER)
	newuser_deflevel = MAX_LEV_NEWUSER;
      return;

    case 48:			/* wrap columns */
      if ((wrap_cols = val) <= 1)
	wrap_cols = WRAP_COLS;
      return;

    case 49:			/* hint at help flag */
      if ((hint_at_help = onoff_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: hint_at_help must be ON or OFF on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 50:			/* add a space to each emote speech flag */
      if ((emote_spacer = yn_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: emote_spacer must be YES or NO on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 51:			/* accept prelogin commands ('who', 'version', 'quit') flag */
      if ((accept_prelog_comm = yn_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: accept_prelog_comm must be YES or NO on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    case 52:			/* allow to .born max-level (superior) users flag */
      if ((allow_born_maxlev = yn_check (wrd[1])) == -1)
	{
	  fprintf (stderr,
		   "GAEN: allow_born_maxlev must be YES or NO on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;

    }				/* end switch */
}

/*** Parse rooms section ***/
parse_rooms_section (dimension)
     int dimension;
{
  static int in_section = 0;
  int i;
  char *ptr1, *ptr2, c;
  RM_OBJECT room;

  sprintf (text, "ROOMS%d:", dimension + 1);
  if (!strcmp (wrd[0], text))
    {
      if (++in_section > 5)
	{
	  fprintf (stderr,
		   "GAEN: Unexpected ROOMSx section header on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;
    }
  if (!wrd[2][0])
    {
      fprintf (stderr, "GAEN: Required parameter(s) missing on line %d.\n",
	       config_line);
      boot_exit (1);
    }
  if (strlen (wrd[0]) > ROOM_LABEL_LEN)
    {
      fprintf (stderr, "GAEN: Room label too long on line %d.\n",
	       config_line);
      boot_exit (1);
    }
  if (strlen (wrd[1]) > ROOM_NAME_LEN)
    {
      fprintf (stderr, "GAEN: Room name too long on line %d.\n", config_line);
      boot_exit (1);
    }
/* Check for duplicate label or name */
  for (room = room_first[dimension]; room != NULL; room = room->next)
    {
      if (!strcmp (room->label, wrd[0]))
	{
	  fprintf (stderr, "GAEN: Duplicate room label on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      if (!strcmp (room->name, wrd[1]))
	{
	  fprintf (stderr, "GAEN: Duplicate room name on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
    }
  room = create_room (dimension);
  strcpy (room->label, wrd[0]);
  strcpy (room->name, wrd[1]);

/* Parse internal links bit, ie hl,gd,of etc. 
   MUST NOT be any spaces between the commas */
  i = 0;
  ptr1 = wrd[2];
  ptr2 = wrd[2];
  while (1)
    {
      while (*ptr2 != ',' && *ptr2 != '\0')
	++ptr2;
      if (*ptr2 == ',' && *(ptr2 + 1) == '\0')
	{
	  fprintf (stderr, "GAEN: Missing link label on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      c = *ptr2;
      *ptr2 = '\0';
      if (!strcmp (ptr1, room->label))
	{
	  fprintf (stderr, "GAEN: Room has a link to itself on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      strcpy (room->link_label[i], ptr1);
      if (c == '\0')
	break;
      if (++i >= MAX_LINKS)
	{
	  fprintf (stderr, "GAEN: Too many links on line %d.\n", config_line);
	  boot_exit (1);
	}
      *ptr2 = c;
      ptr1 = ++ptr2;
    }

/* Parse access privs */
  if (wrd[3][0] == '#')
    {
      room->access = PUBLIC;
      return;
    }
  if (!wrd[3][0] || !strcmp (wrd[3], "BOTH"))
    room->access = PUBLIC;
  else if (!strcmp (wrd[3], "PUB"))
    room->access = FIXED_PUBLIC;
  else if (!strcmp (wrd[3], "PRIV"))
    room->access = FIXED_PRIVATE;
  else
    {
      fprintf (stderr, "GAEN: Unknown room access type on line %d.\n",
	       config_line);
      boot_exit (1);
    }
/* Parse external link stuff */
  if (!wrd[4][0] || wrd[4][0] == '#')
    return;
  if (!strcmp (wrd[4], "ACCEPT"))
    {
      if (wrd[5][0] && wrd[5][0] != '#')
	{
	  fprintf (stderr,
		   "GAEN: Unexpected word following ACCEPT keyword on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      room->inlink = 1;
      return;
    }
  if (!strcmp (wrd[4], "CONNECT"))
    {
      if (!wrd[5][0])
	{
	  fprintf (stderr, "GAEN: External link name missing on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      if (wrd[6][0] && wrd[6][0] != '#')
	{
	  fprintf (stderr,
		   "GAEN: Unexpected word following external link name on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      strcpy (room->netlink_name, wrd[5]);
      return;
    }
  fprintf (stderr, "GAEN: Unknown connection option on line %d.\n",
	   config_line);
  boot_exit (1);
}



/*** Parse sites section ***/
parse_sites_section ()
{
  NL_OBJECT nl;
  static int in_section = 0;

  if (!strcmp (wrd[0], "SITES:"))
    {
      if (++in_section > 1)
	{
	  fprintf (stderr,
		   "GAEN: Unexpected SITES section header on line %d.\n",
		   config_line);
	  boot_exit (1);
	}
      return;
    }
  if (!wrd[3][0])
    {
      fprintf (stderr, "GAEN: Required parameter(s) missing on line %d.\n",
	       config_line);
      boot_exit (1);
    }
  if (strlen (wrd[0]) > SERV_NAME_LEN)
    {
      fprintf (stderr, "GAEN: Link name length too long on line %d.\n",
	       config_line);
      boot_exit (1);
    }
  if (strlen (wrd[3]) > SERV_NAME_LEN)
    {
      fprintf (stderr, "GAEN: Verification too long on line %d.\n",
	       config_line);
      boot_exit (1);
    }
  if ((nl = create_netlink ()) == NULL)
    {
      fprintf (stderr,
	       "GAEN: Memory allocation failure creating netlink on line %d.\n",
	       config_line);
      boot_exit (1);
    }
  if (!wrd[4][0] || wrd[4][0] == '#' || !strcmp (wrd[4], "ALL"))
    nl->allow = ALL;
  else if (!strcmp (wrd[4], "IN"))
    nl->allow = IN;
  else if (!strcmp (wrd[4], "OUT"))
    nl->allow = OUT;
  else
    {
      fprintf (stderr, "GAEN: Unknown netlink access type on line %d.\n",
	       config_line);
      boot_exit (1);
    }
  if ((nl->port = atoi (wrd[2])) < 1 || nl->port > 65535)
    {
      fprintf (stderr, "GAEN: Illegal port number on line %d.\n",
	       config_line);
      boot_exit (1);
    }
  strcpy (nl->service, wrd[0]);
  strtolower (wrd[1]);
  strcpy (nl->site, wrd[1]);
  strcpy (nl->verification, wrd[3]);
}

/*** Load swear words list from file ***/
load_swear_file (user)
     UR_OBJECT user;
{
  FILE *fp;
  char filename[80], line[WORD_LEN + 1];
  int i = 0;

  sprintf (filename, "%s/%s", DATAFILES, SWEARFILE);
  if (user == NULL)
    printf ("Loading swear words file \"%s\"...", filename);
  else
    write_user (user, ">>>Loading swear words file...");
  if (!(fp = fopen (filename, "r")))
    {
      strcpy (swear_words[0], "*");
      if (user == NULL)
	printf (" not found.\n");
      else
	write_user (user, " not found.\n");
      return;
    }
  fgets (line, WORD_LEN + 2, fp);
  while (!feof (fp))
    {
      line[strlen (line) - 1] = '\0';
      strcpy (swear_words[i], line);
      if (user == NULL)
	printf (" '%s'", swear_words[i]);
      else
	{
	  sprintf (text, " '%s'", swear_words[i]);
	  write_user (user, text);
	}
      i++;
      if (i >= MAX_SWEARS)
	break;
      fgets (line, WORD_LEN + 2, fp);
    }
  fclose (fp);
  strcpy (swear_words[i], "*");
  if (user == NULL)
    printf (" done (%d words).\n", i);
  else
    {
      sprintf (text, " done (~FT%d~RS words).\n", i);
      write_user (user, text);
    }
}

yn_check (wd)
     char *wd;
{
  if (!strcmp (wd, "YES"))
    return 1;
  if (!strcmp (wd, "NO"))
    return 0;
  return -1;
}


onoff_check (wd)
     char *wd;
{
  if (!strcmp (wd, "ON"))
    return 1;
  if (!strcmp (wd, "OFF"))
    return 0;
  return -1;
}


/************ INITIALISATION FUNCTIONS *************/

/*** Initialise globals ***/
init_globals ()
{
  int i;
  verification[0] = '\0';
  port[0] = 0;
  port[1] = 0;
  port[2] = 0;
  auto_connect = 1;
  max_users = 50;
  max_clones = 1;
  ban_swearing = 0;
  heartbeat = 2;
  keepalive_interval = 60;	/* DO NOT TOUCH!!! */
  net_idle_time = 300;		/* Must be > than the above */
  login_idle_time = 180;
  user_idle_time = 300;
  time_out_afks = 0;
  stport_level = SPIRIT;
  newuser_deflevel = GUEST;
  minlogin_level = -1;
  mesg_life = 1;
  no_prompt = 0;
  num_of_users = 0;
  num_of_logins = 0;
  for (i = 0; i < MAX_LOGS; i++)
    system_logging[i] = 1;
  password_echo = 0;
  ignore_sigterm = 0;
  crash_action = 2;
  prompt_def = 1;
  colour_def = 1;
  mesg_check_hour = 0;
  mesg_check_min = 0;
  allow_caps_in_name = 1;
  rs_countdown = 0;
  rs_announce = 0;
  rs_which = -1;
  rs_user = NULL;
  gatecrash_level = GOD;	/* minimum user level which can enter private rooms */
  min_private_users = 2;	/* minimum num. of users in room before can set to priv */
  ignore_mp_level = GOD;	/* user level which can ignore the above var. */
  rem_user_maxlevel = SOUL;
  rem_user_deflevel = SOUL;
  user_first = NULL;
  user_last = NULL;
  for (i = 0; i < MAX_DIMENS; i++)
    {
      room_first[i] = NULL;
      room_last[i] = NULL;	/* This variable isn't used yet */
    }
  clear_watch ();
  nl_first = NULL;
  nl_last = NULL;
  autosave_counter = 0;
  autosave_maxiter = AUTOSAVE_MAXITER;
  autosave_action = AS_ALL;
  max_quotes = MAX_QUOTES;
  max_hints = MAX_HINTS;
  prayer_number = 0;
  use_hostsfile = 0;
  mirror = 1;
  for (i = 0; i <= MAX_LEVELS; i++)	/* invisibility ... */
    strcpy (level_name[i], level_name_mirror[mirror][i]);
  strcpy (invisname, "A digit");
  strcpy (invisenter, "~FT>>>A digit enters the sky...\n");
  strcpy (invisleave, "~FT>>>A digit leaves the sky...\n");
  no_disable_mirror = 0;
  cnt_in = 0;
  cnt_out = 0;
  cnt_swp = 0;
  safety = 0;
  if (getcwd (homedir, SITE_NAME_LEN) == NULL)
    {
      printf ("Warning! Cannot get talker's home directory.\n");
      write_syslog (LOG_CHAT,
		    "Warning! Cannot get talker's home directory.\n", LOG_NOTIME);
      write_syslog (LOG_ERR,
		    "Warning! Cannot get talker's home directory.\n", LOG_NOTIME);		    
    }
  event = E_NONE;
  enable_event = 1;
  enable_rndgo = 1;
  strcpy (event_var, "*");
  clear_words ();
  for (i = 0; i <= MAX_SWEARS; i++)
    swear_words[i][0] = '\0';
  gm_max_words = GM_MAX_WORDS;
  exec_gshell = 1;
  def_lines = DEF_LINES;
  timeout_maxlevel = SPIRIT;
  def_pballs = GM_MAX_PBALLS;
  chk_spass_tests = CHK_SPASS_ALL;
  strcpy (utmpfile, UTMPFILE);
  strcpy (wtmpfile, WTMPFILE);
  strcpy (susers_restrict, RESTRICT_MASK);
  id_readtimeout = ID_READTIMEOUT;
  random_bhole = 1;
  prelog_msgs = MAX_PRELOG_MSGS;
  wrap_cols = WRAP_COLS;
  hint_at_help = 1;
  emote_spacer = 1;
  accept_prelog_comm = 1;
  allow_born_maxlev = 1;

/* set time and random seed */
  time (&boot_time);
  srandom (boot_time);
}


/*** Initialise the signal traps etc ***/
init_signals ()
{
  void sig_handler ();

  signal (SIGTERM, sig_handler);
  signal (SIGSEGV, sig_handler);
  signal (SIGBUS, sig_handler);
  signal (SIGILL, SIG_IGN);
  signal (SIGTRAP, SIG_IGN);
  signal (SIGIOT, SIG_IGN);
  signal (SIGTSTP, SIG_IGN);
  signal (SIGCONT, SIG_IGN);
  signal (SIGHUP, SIG_IGN);
  signal (SIGINT, SIG_IGN);
  signal (SIGQUIT, SIG_IGN);
  signal (SIGABRT, SIG_IGN);
  signal (SIGFPE, SIG_IGN);
/*signal (SIGURG, SIG_IGN);*/
  signal (SIGPIPE, SIG_IGN);
  signal (SIGTTIN, SIG_IGN);
  signal (SIGTTOU, SIG_IGN);
}

/*** Some signal trapping functions ***/

void
sig_handler (sig)
     int sig;
{
  force_listen = 1;
  switch (sig)
    {
    case SIGTERM:
      if (ignore_sigterm)
	{
	  write_syslog (LOG_CHAT, "SIGTERM signal received - ignoring.\n", LOG_TIME);
	  return;
	}
      write_room (NULL,
		  "\n\n~OLGAEN:~FY~LI SIGTERM recieved, initiating shutdown.\n\n");
      talker_shutdown (NULL, "a termination signal (SIGTERM)", 0);

    case SIGSEGV:
      sprintf (text, "Last command: %s (%d)\n", command[com_num], com_num);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      switch (crash_action)
	{
	case 0:
	  write_room (NULL,
		      "\n\n\07~OLGAEN:~FR~LI PANIC - Segmentation fault, initiating shutdown! Return later...\n\n");
	  talker_shutdown (NULL, "a segmentation fault (SIGSEGV)", 0);

	case 1:
	  write_room (NULL,
		      "\n\n\07~OLGAEN:~FR~LI WARNING - A segmentation fault has just occured!\n\n");
	  write_syslog (LOG_CHAT, "WARNING: A segmentation fault occured!\n",
			LOG_TIME);
	  longjmp (jmpvar, 0);

	case 2:
	  write_room (NULL,
		      "\n\n\07~OLGAEN:~FR~LI PANIC - Segmentation fault, initiating reboot! Return later...\n\n");
	  talker_shutdown (NULL, "a segmentation fault (SIGSEGV)", 1);
	}

    case SIGBUS:
      sprintf (text, "Last command: %s (%d)\n", command[com_num], com_num);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      switch (crash_action)
	{
	case 0:
	  write_room (NULL,
		      "\n\n\07~OLGAEN:~FR~LI PANIC - Bus error, initiating shutdown! Return later...\n\n");
	  talker_shutdown (NULL, "a bus error (SIGBUS)", 0);

	case 1:
	  write_room (NULL,
		      "\n\n\07~OLGAEN:~FR~LI WARNING - A bus error has just occured!\n\n");
	  write_syslog (LOG_CHAT, "WARNING: A bus error occured!\n", LOG_TIME);
	  longjmp (jmpvar, 0);

	case 2:
	  write_room (NULL,
		      "\n\n\07~OLGAEN:~FR~LI PANIC - Bus error, initiating reboot! Return later...\n\n");
	  talker_shutdown (NULL, "a bus error (SIGBUS)", 1);
	}
    }
}


/*** Initialise sockets on ports ***/
init_sockets ()
{
  struct sockaddr_in bind_addr;
  int i, on, size;

  printf ("Initialising sockets on ports: %d, %d, %d.\n", port[0], port[1],
	  port[2]);
  on = 1;
  size = sizeof (struct sockaddr_in);
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = INADDR_ANY;
  for (i = 0; i < 3; ++i)
    {
      /* create sockets */
      if ((listen_sock[i] = socket (AF_INET, SOCK_STREAM, 0)) == -1)
	boot_exit (i + 2);

      /* allow reboots on port even with TIME_WAITS */
      setsockopt (listen_sock[i], SOL_SOCKET, SO_REUSEADDR, (char *) &on,
		  sizeof (on));

      /* bind sockets and set up listen queues */
      bind_addr.sin_port = htons (port[i]);
      if (bind (listen_sock[i], (struct sockaddr *) &bind_addr, size) == -1)
	boot_exit (i + 5);
      if (listen (listen_sock[i], 10) == -1)
	boot_exit (i + 8);

      /* Set to non-blocking , do we need this? not really */
      fcntl (listen_sock[i], F_SETFL, O_NDELAY);
    }
}

/*** Initialise connections to remote servers. Basically this does all the
     client shit which creates standard sockets in the NL_OBJECT linked list
     which the talker then uses ***/
init_connections ()
{
  NL_OBJECT nl;
  RM_OBJECT rm;
  int i, ret, cnt = 0;

  printf ("Connecting to remote servers...\n");
  errno = 0;
  for (i = 0; i < MAX_DIMENS; i++)
    for (rm = room_first[i]; rm != NULL; rm = rm->next)
      {
	if ((nl = rm->netlink) == NULL)
	  continue;
	++cnt;
	printf ("  Trying service %s at %s %d: ", nl->service, nl->site,
		nl->port);
	fflush (stdout);
	if ((ret = connect_to_site (nl)))
	  {
	    if (ret == 1)
	      {
		printf ("%s.\n", sys_errlist[errno]);
		sprintf (text, "NETLINK: Failed to connect to %s: %s.\n",
			 nl->service, sys_errlist[errno]);
	      }
	    else
	      {
		printf ("Unknown hostname.\n");
		sprintf (text,
			 "NETLINK: Failed to connect to %s: Unknown hostname.\n",
			 nl->service);
	      }
	    write_syslog (LOG_LINK, text, LOG_TIME);
	  }
	else
	  {
	    printf ("CONNECTED.\n");
	    sprintf (text, "NETLINK: Connected to %s (%s %d).\n",
		     nl->service, nl->site, nl->port);
	    write_syslog (LOG_LINK, text, LOG_TIME);
	    nl->connect_room = rm;
	  }
      }
  if (cnt)
    printf
      ("  See system log (in '%s' directory) for any further information.\n",
       LOGFILES);
  else
    printf ("  No remote connections configured.\n");
}

/*** Do the actual connection ***/
connect_to_site (nl)
     NL_OBJECT nl;
{
  struct sockaddr_in con_addr;
  struct hostent *he;
  int inetnum;
  char *sn;

  sn = nl->site;
/* See if number address */
  while (*sn && (*sn == '.' || isdigit (*sn)))
    sn++;

/* Name address given */
  if (*sn)
    {
      if (!(he = gethostbyname (nl->site)))
	return 2;
      memcpy ((char *) &con_addr.sin_addr, he->h_addr, (size_t) he->h_length);
    }
/* Number address given */
  else
    {
      if ((inetnum = inet_addr (nl->site)) == -1)
	return 1;
      memcpy ((char *) &con_addr.sin_addr, (char *) &inetnum,
	      (size_t) sizeof (inetnum));
    }
/* Set up stuff and disable interrupts */
  if ((nl->socket = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    return 1;
  con_addr.sin_family = AF_INET;
  con_addr.sin_port = htons (nl->port);
  signal (SIGALRM, SIG_IGN);

/* Attempt the connect. This is where the talker may hang. */
  if (connect (nl->socket, (struct sockaddr *) &con_addr, sizeof (con_addr))
      == -1)
    {
      reset_alarm ();
      return 1;
    }
  reset_alarm ();
  nl->type = OUTGOING;
  nl->stage = VERIFYING;
  nl->last_recvd = time (0);
  return 0;
}

/************* WRITE FUNCTIONS ************/

/*** Write a NULL terminated string to a socket ***/
write_sock (sock, str)
     int sock;
     char *str;
{
  write (sock, str, strlen (str));
}


/*** Send message to user ***/
write_user (user, str)
     UR_OBJECT user;
     char *str;
{
  int buffpos, sock, i, cnt, wrapping;
  char *start, buff[OUT_BUFF_SIZE], mesg[ARR_SIZE], *colour_com_strip ();

  if (user == NULL)
    return;
  if (user->blind)
    return;
  if (user->type == REMOTE_TYPE)
    {
      if (user->netlink->ver_major <= 3 && user->netlink->ver_minor < 2)
	str = colour_com_strip (str);
      if (str[strlen (str) - 1] != '\n')
	sprintf (mesg, "MSG %s\n%s\nEMSG\n", user->savename, str);
      else
	sprintf (mesg, "MSG %s\n%sEMSG\n", user->savename, str);
      write_sock (user->netlink->socket, mesg);
      return;
    }
  start = str;
  buffpos = 0;
  sock = user->socket;
/* Process string and write to buffer. We use pointers here instead of arrays 
   since these are supposedly much faster (though in reality I guess it depends
   on the compiler) which is necessary since this routine is used all the 
   time. */
  cnt = 0;
  wrapping = 0;
  while (*str)
    {
      if (*str == '\n')
	{
	  if (wrapping)
	    {
	      wrapping = 0;
	      ++str;
	    }
	  else
	    {
	      if (buffpos > OUT_BUFF_SIZE - 6)
		{
		  write (sock, buff, buffpos);
		  buffpos = 0;
		}
	      /* Reset terminal before every newline */
	      if (user->colour % 2)
		{
		  memcpy (buff + buffpos, "\033[0m", 4);
		  buffpos += 4;
		}
	      *(buff + buffpos) = '\n';
	      *(buff + buffpos + 1) = '\r';
	      buffpos += 2;
	      ++str;
	      cnt = 0;
	    }
	}
      else if (*str == '\b')
	{
	  if (wrapping)
	    {
	      *(buff + buffpos) = '\b';
	      ++buffpos;
	    }
	  ++str;
	  --cnt;
	}
      else if (user->wrap && cnt == wrap_cols)
	{
	  if (buffpos > OUT_BUFF_SIZE - 2)
	    {
	      write (sock, buff, buffpos);
	      buffpos = 0;
	    }
	  *(buff + buffpos) = '\n';
	  *(buff + buffpos + 1) = '\r';
	  buffpos += 2;
	  cnt = 0;
	  wrapping = 1;
	}
      else
	{
	  /* See if its a / before a ~ , if so then we print colour command
	     as text */
	  if (*str == '/' && *(str + 1) == '~')
	    {
	      ++str;
	      continue;
	    }
	  if (str > start && *str == '~' && *(str - 1) == '/')
	    {
	      *(buff + buffpos) = *str;
	      wrapping = 0;
	      goto CONT;
	    }
	  /* Process colour commands eg ~FR. We have to strip out the commands 
	     from the string even if user doesnt have colour switched on hence 
	     the user->colour check isnt done just yet */
	  if (*str == '~')
	    {
	      if (buffpos > OUT_BUFF_SIZE - 6)
		{
		  write (sock, buff, buffpos);
		  buffpos = 0;
		}
	      ++str;
	      for (i = 0; i < NUM_COLS; ++i)
		{
		  if (!strncmp (str, colcom[i], 2))
		    {
		      if (user->colour % 2)
			{
			  memcpy (buff + buffpos, colcode[i],
				  strlen (colcode[i]));
			  buffpos += strlen (colcode[i]) - 1;
			}
		      else
			buffpos--;
		      ++str;
		      --cnt;
		      goto CONT;
		    }
		}
	      *(buff + buffpos) = *(--str);
	      wrapping = 0;
	    }
	  else
	    *(buff + buffpos) = *str;
	  wrapping = 0;
	CONT:
	  ++buffpos;
	  ++str;
	  ++cnt;
	}
      if (buffpos == OUT_BUFF_SIZE)
	{
	  write (sock, buff, OUT_BUFF_SIZE);
	  buffpos = 0;
	}
    }
  if (buffpos)
    write (sock, buff, buffpos);
/* Reset terminal at end of string */
  if (user->colour % 2)
    write_sock (sock, "\033[0m");
}

/*** Write to users of level 'level' and above. ***/
write_level (level, str, user)
     int level;
     char *str;
     UR_OBJECT user;
{
  UR_OBJECT u;

  for (u = user_first; u != NULL; u = u->next)
    {
      if (u != user && u->level >= level &&
	  !u->login && u->type != CLONE_TYPE)
	write_user (u, str);
    }
}



/*** Subsid function to below but this one is used the most ***/
write_room (rm, str)
     UR_OBJECT rm;
     char *str;
{
  write_room_except (rm, str, NULL);
}



/*** Write to everyone in room rm except for "user". If rm is NULL write
     to all rooms. ***/
write_room_except (rm, str, user)
     RM_OBJECT rm;
     char *str;
     UR_OBJECT user;
{
  UR_OBJECT u;
  char text2[ARR_SIZE];

  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->login
	  || u->room == NULL
	  || (u->room != rm && rm != NULL)
	  || (u->ignall && !force_listen)
	  || (u->ignshout &&
	      (com_num == SHOUT || com_num == MSHOUT || com_num == SEMOTE))
	  || u == user || is_ignoring (u, user))
	continue;
      if (u->type == CLONE_TYPE)
	{
	  if (u->clone_hear == CLONE_HEAR_NOTHING || u->owner->ignall)
	    continue;
	  /* Ignore anything not in clones room, eg shouts, system messages
	     and semotes since the clones owner will hear them anyway. */
	  if (rm != u->room)
	    continue;

	  if (is_ignoring (u->owner, user))
	    continue;

	  if (u->clone_hear == CLONE_HEAR_SWEARS)
	    {
	      if (!contains_swearing (str))
		continue;
	    }
	  sprintf (text2, "~FT[ %s ]:~RS %s", u->room->name, str);
	  write_user (u->owner, text2);
	}
      else
	write_user (u, str);
    }
}


/*** Write a string to system logs ***/
write_syslog (type, str, write_time)
     int type;
     char *str;
     int write_time;
{
  FILE *fp;
  char filename[80];

  sprintf (filename, "%s/%s.%s", LOGFILES, SYSLOG, logtype[type]);

  if (!system_logging[type] || !(fp = fopen (filename, "a")))
    return;
  if (write_time == LOG_NOTIME)
    fputs (str, fp);
  else
    fprintf (fp, "%02d-%02d %02d:%02d:%02d: %s",
	     tmday, tmonth + 1, thour, tmin, tsec, str);
  fclose (fp);
}

/*** Write usage of a command ***/
write_usage (user, msg)
     UR_OBJECT user;
     char *msg;
{
  sprintf (text, ">>>Usage: ~FT%s\n", msg);
  write_user (user, text);
}


/******** LOGIN/LOGOUT FUNCTIONS ********/

/*** Login function - all mostly inline code ***/
login_user (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  UR_OBJECT u;
  int i;
  char name[ARR_SIZE], passwd[ARR_SIZE];

  name[0] = '\0';
  passwd[0] = '\0';
  switch (user->login)
    {
    case 3:
      sscanf (inpstr, "%s", name);
      if (name[0] < 33)
	{
	  write_user (user, "\n");
	  write_user (user, DEF_NAME_MSG);
	  return;
	}
      if (!strcmp (name, "quit"))
	{
	  if (!accept_prelog_comm)
	    {
	      write_user (user, NOPRELOG_MSG);
	      disconnect_user (user);
	      return;
	    }
	  write_user (user, "\n\n--->>> Abandoning login attempt <<<---\n\n");
	  disconnect_user (user);
	  return;
	}
      if (!strcmp (name, "who"))
	{
	  if (!accept_prelog_comm)
	    {
	      write_user (user, NOPRELOG_MSG);
	      disconnect_user (user);
	      return;
	    }
	  who (user, LIST_WHO);
	  write_user (user, "\n");
	  write_user (user, DEF_NAME_MSG);
	  return;
	}
      if (accept_prelog_comm && !strcmp (name, "version"))
	{
	  if (!accept_prelog_comm)
	    {
	      write_user (user, NOPRELOG_MSG);
	      disconnect_user (user);
	      return;
	    }
	  version (user);
	  write_user (user, "\n");
	  write_user (user, DEF_NAME_MSG);
	  return;
	}
      if (accept_prelog_comm && !strcmp (name, "quote"))
	{
	  if (!accept_prelog_comm)
	    {
	      write_user (user, NOPRELOG_MSG);
	      disconnect_user (user);
	      return;
	    }
	  quote (user, 0);
	  write_user (user, "\n");
	  write_user (user, DEF_NAME_MSG);
	  return;
	}
      if (strlen (name) < USER_MIN_LEN)
	{
	  write_user (user, "\n>>>Name too short.\n\n");
	  attempts (user);
	  return;
	}
      if (strlen (name) > USER_NAME_LEN)
	{
	  write_user (user, "\n>>>Name too long.\n\n");
	  attempts (user);
	  return;
	}
      /* see if only letters in login */
      for (i = 0; i < strlen (name); ++i)
	{
	  if (!isalpha (name[i]))
	    {
	      write_user (user,
			  "\n>>>Only letters are allowed in a name.\n\n");
	      attempts (user);
	      return;
	    }
	}
      if (!allow_caps_in_name)
	strtolower (name);
      name[0] = toupper (name[0]);
      if (user_banned (name))
	{
	  write_user (user, "\n>>>You are banned from this talker.\n\n");
	  disconnect_user (user);
	  sprintf (text, "~OLGAEN:~RS Attempted login by banned user %s.\n",
		   name);
	  write_syslog (LOG_CHAT, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, NULL);
	  return;
	}
      strcpy (user->name, name);
      strcpy (user->savename, name);
      /* If user has hung on another login clear that session */
      for (u = user_first; u != NULL; u = u->next)
	{
	  if (u->login && u != user && !strcmp (u->savename, user->savename))
	    {
	      disconnect_user (u);
	      break;
	    }
	}
      if (!load_user_details (user))
	{
	  if (user->port == port[1])
	    {
	      write_user (user,
			  "\n>>>Sorry, new logins cannot be created on this port.\n\n");
	      sprintf (text,
		       "~OLGAEN:~RS Attempt to create new user %s from %s on port %d.\n",
		       user->savename, user->site, port[1]);
	      write_syslog (LOG_CHAT, text, LOG_TIME);
	      more (user, user->socket, NEWLOG_MSG);
	      disconnect_user (user);
	      return;
	    }
	  if (minlogin_level > -1)
	    {
	      write_user (user,
			  "\n>>>Sorry, new logins cannot be created at this time.\n\n");
	      sprintf (text,
		       "~OLGAEN:~RS Attempt to create new user %s from %s on port %d since minlogin was greater than NONE.\n",
		       user->savename, user->site, port[0]);
	      write_syslog (LOG_CHAT, text, LOG_TIME);
	      more (user, user->socket, MINLOG_MSG);
	      disconnect_user (user);
	      return;
	    }
	  write_user (user, ">>>New user...\n");
	}
      else
	{
	  if (user->port == port[1] && user->level < stport_level)
	    {
	      sprintf (text,
		       "\n>>>Sorry, only users of level %s~RS and above can log in on this port.\n\n",
		       level_name[stport_level]);
	      write_user (user, text);
	      sprintf (text,
		       "~OLGAEN:~RS Attempt to connect user %s from %s with level below stport_level.\n",
		       user->savename, user->site);
	      write_syslog (LOG_CHAT, text, LOG_TIME);
	      write_level (MIN_LEV_NOTIFY, text, NULL);
	      more (user, user->socket, STPORT_MSG);
	      disconnect_user (user);
	      return;
	    }
	  if (user->level < minlogin_level)
	    {
	      write_user (user,
			  "\n>>>Sorry, the talker is locked out to users of your level. Return later...\n\n");
	      sprintf (text,
		       "~OLGAEN:~RS Attempt to connect user %s from %s with level below minlogin_level.\n",
		       user->savename, user->site);
	      write_syslog (LOG_CHAT, text, LOG_TIME);
	      write_level (MIN_LEV_NOTIFY, text, NULL);
	      disconnect_user (user);
	      more (user, user->socket, MINLOG_MSG);
	      return;
	    }
	}
      switch (random () % MAX_MAINPHR)	/* write login extra phrases */
	{
	case 0:
	  sprintf (text, "%s'%s %s! :-)'\n",
		   prelog_msg, prefix_login_phr[0],
		   login_phr1[random () % MAX_PHR1]);
	  break;
	case 1:
	  sprintf (text, "%s'%s %s! :-)'\n",
		   prelog_msg, prefix_login_phr[1],
		   login_phr2[random () % MAX_PHR2]);
	  break;
	case 2:
	  sprintf (text, "%s'%s %s! :-)'\n",
		   prelog_msg, prefix_login_phr[2],
		   login_phr3[random () % MAX_PHR3]);
	}
      write_user (user, text);
      write_user (user, enter_pass);
      echo_off (user);
      user->login = 2;
      return;

    case 2:
      sscanf (inpstr, "%s", passwd);
      if (strlen (passwd) < PASS_MIN_LEN)
	{
	  write_user (user, "\n\n>>>Password too short.\n\n");
	  attempts (user);
	  return;
	}
      if (strlen (passwd) > PASS_MAX_LEN)
	{
	  write_user (user, "\n\n>>>Password too long.\n\n");
	  attempts (user);
	  return;
	}
      /* if new user... */

      if (!user->pass[0])
	{
	  if (!secure_pass (passwd))	/* check password */
	    {
	      write_user (user,
			  "\n\n>>>Password too simple, try to use special characters, please.\n\n");
	      attempts (user);
	      return;
	    }
	  strcpy (user->pass, (char *) crypt (passwd, PASS_SALT));
	  write_user (user, "\n>>>Please confirm password: ");
	  user->login = 1;
	}
      else
	{
	  if (!strcmp (user->pass, (char *) crypt (passwd, PASS_SALT)))
	    {
	      echo_on (user);
	      connect_user (user);
	      if (!secure_pass (passwd))
		write_user (user,
			    "\n\n~EL~EL~FR~OL~LI>>>Warning! Your password is not secure! Please, change it!\n~FR~OL~LI>>>Use special characters...\n");
	      return;
	    }
	  write_user (user, "\n\n>>>Incorrect login! :-(\n\n");
	  attempts (user);
	}
      return;

    case 1:
      sscanf (inpstr, "%s", passwd);
      if (strcmp (user->pass, (char *) crypt (passwd, PASS_SALT)))
	{
	  write_user (user,
		      "\n\n>>>Passwords do not match. Sorry... :-(\n\n");
	  attempts (user);
	  return;
	}
      echo_on (user);
      strcpy (user->desc, DEF_DESC);
      strcpy (user->in_phrase, "enters");
      strcpy (user->out_phrase, "goes");
      user->last_site[0] = '\0';
      user->dimension = 0;
      user->level = newuser_deflevel;
      user->prompt = prompt_def;
      user->charmode_echo = 0;
      user->afkmesg[0] = '\0';
      user->slevel = newuser_deflevel;
      strcpy (user->ssite, user->site);
      strcpy (user->restrict, RESTRICT_MASK);
      save_user_details (user, 1);
      sprintf (text, "New user %s from %s created.\n",
	       user->savename, user->site);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      connect_user (user);
    }
}

/*** Count up attempts made by user to login ***/
attempts (user)
     UR_OBJECT user;
{

  user->attempts++;
  if (user->attempts == MAX_ATTEMPTS)
    {
      write_user (user, "\n>>>Maximum attempts reached. Sorry...\n\n");
      disconnect_user (user);
      return;
    }
  user->login = 3;
  user->pass[0] = '\0';
  write_user (user, DEF_NAME_MSG);
  echo_on (user);
}


/*** Load the users stats ***/
load_user_details (user)
     UR_OBJECT user;
{
  FILE *fp;
  char line[81], filename[80];
  int temp1, temp2, temp3, i;

  sprintf (filename, "%s/%s.D", USERFILES, user->savename);
  if (!(fp = fopen (filename, "r")))
    return 0;
  fscanf (fp, "%s", user->pass);	/* password */
  fscanf (fp, "%d %d %d %d %d %d %d %d %d %d",
	  &temp1, &temp2, &user->last_login_len, &temp3,
	  &user->level, &user->prompt, &user->muzzled,
	  &user->charmode_echo, &user->command_mode, &user->colour);
  user->last_login = (time_t) temp1;
  user->total_login = (time_t) temp2;
  user->read_mail = (time_t) temp3;
  fscanf (fp, "%s\n", user->last_site);

/* Some verifications */
  if (user->level < GUEST || user->level > SGOD)
    user->level = newuser_deflevel;
  if (user->muzzled < GUEST || user->muzzled > SGOD)
    user->muzzled = GUEST;

/* Need to do the rest like this 'cos they may be more than 1 word each */
  fgets (line, USER_DESC_LEN + 2, fp);
  line[strlen (line) - 1] = 0;
  strcpy (user->desc, line);
  fgets (line, PHRASE_LEN + 2, fp);
  line[strlen (line) - 1] = 0;
  strcpy (user->in_phrase, line);
  fgets (line, PHRASE_LEN + 2, fp);
  line[strlen (line) - 1] = 0;
  strcpy (user->out_phrase, line);
  fclose (fp);
  sprintf (filename, "%s/%s.S", USERFILES, user->savename);	/* load subst file */
  if (!(fp = fopen (filename, "r")))
    {
      user->slevel = user->level;
      strcpy (user->ssite, user->site);
      user->subst = 0;
    }
  else
    {
      fgets (line, SITE_NAME_LEN + 2, fp);
      line[strlen (line) - 1] = 0;
      strcpy (user->ssite, line);
      fscanf (fp, "%d", &user->slevel);
      if (user->slevel < GUEST || user->slevel > SGOD)
	user->slevel = GUEST;
      user->subst = 1;
      fclose (fp);
    }
  if (user->slevel > SGOD)
    user->slevel = GUEST;
  sprintf (filename, "%s/%s.R", USERFILES, user->savename);	/* load restrict file */
  if (!(fp = fopen (filename, "r")))
    strcpy (user->restrict, RESTRICT_MASK);
  else
    {
      fgets (line, MAX_RESTRICT + 2, fp);
      line[strlen (line) - 1] = 0;
      strcpy (user->restrict, line);
      fclose (fp);
    }
  if (user->socket < 0)
    return 1;			/* don't want to check virtual 
				   users created by some functions */
  temp1 = 0;
  while (max_level_users[temp1][0] != '*')
    {
      if (!strcmp (user->savename, max_level_users[temp1]))
	{
	  user->level = SGOD;
	  return 1;
	}
      temp1++;
    }
/* check allow */
  for (i = MAX_LEV_ALLOW; i >= MIN_LEV_ALLOW; i--)
    {
      if (!check_allow (user, i))
	{
	  user->level--;
	  sprintf (text, "~OLGAEN:~RS Allow %s~RS failed for %s\n",
		   level_name[i], user->savename);
	  write_syslog (LOG_CHAT, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, NULL);
	}
    }
  return 1;
}

/*** Save an users stats ***/
save_user_details (user, save_current)
     UR_OBJECT user;
     int save_current;
{
  FILE *fp;
  char filename[80];
  int i;

  if (user->type == REMOTE_TYPE || user->type == CLONE_TYPE)
    return 0;
  sprintf (filename, "%s/%s.D", USERFILES, user->savename);
  if (!(fp = fopen (filename, "w")))
    {
      sprintf (text, ">>>%s: failed to save your details.\n", syserror);
      write_user (user, text);
      sprintf (text, "SAVE_USER_STATS: Failed to save %s's details.\n",
	       user->savename);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text, "\07~OLGAEN:~RS Failed to save %s's details!\n",
	       user->savename);
      write_level (MIN_LEV_NOTIFY, text, NULL);
      return 0;
    }
/* Insurance against any odd values so we dont crash. Shouldnt be needed if
   there are no bugs but it does no harm to have. */
  if (user->level < GUEST)
    user->level = newuser_deflevel;
  if (user->level > SGOD)
    user->level = SGOD;
  if (user->muzzled < GUEST)
    user->muzzled = GUEST;
  if (user->muzzled > SGOD)
    user->muzzled = SGOD;
  fprintf (fp, "%s\n", user->pass);
  if (save_current)
    fprintf (fp, "%d %d %d ",
	     (int) time (0), (int) user->total_login,
	     (int) (time (0) - user->last_login));
  else
    fprintf (fp, "%d %d %d ", (int) user->last_login, (int) user->total_login,
	     user->last_login_len);
  fprintf (fp, "%d %d %d %d %d %d %d\n", (int) user->read_mail, user->level,
	   user->prompt, user->muzzled, user->charmode_echo,
	   user->command_mode, user->colour);
  if (save_current)
    fprintf (fp, "%s\n", user->site);
  else
    fprintf (fp, "%s\n", user->last_site);
  fprintf (fp, "%s\n", user->desc);
  fprintf (fp, "%s\n", user->in_phrase);
  fprintf (fp, "%s\n", user->out_phrase);
  fclose (fp);
  sprintf (filename, "%s/%s.R", USERFILES, user->savename);	/* save restrictions file */
  if (!(fp = fopen (filename, "w")))
    {
      sprintf (text, ">>>%s: failed to save your restrictions.\n", syserror);
      write_user (user, text);
      sprintf (text, "SAVE_USER_STATS: Failed to save %s's restrictions.\n",
	       user->savename);
      write_syslog (LOG_ERR, text, LOG_TIME);
      return 0;
    }
  if (user->level >= MIN_LEV_AUTORST)	/* set default restriction for superior users */
    {
      for (i = 0; i < strlen (susers_restrict); i++)
	if (susers_restrict[i] != '.')
	  user->restrict[i] = susers_restrict[i];
    }
  fprintf (fp, "%s\n", user->restrict);
  fclose (fp);
  return 1;
}

/*** Connect the user to the talker proper ***/
connect_user (user)
     UR_OBJECT user;
{
  UR_OBJECT u, u2;
  RM_OBJECT rm;
  char temp[30];
  char *strrepl ();

/* See if user already connected */
  for (u = user_first; u != NULL; u = u->next)
    {
      if (user != u && user->type != CLONE_TYPE &&
	  !strcmp (user->savename, u->savename))
	{
	  rm = u->room;
	  if (u->type == REMOTE_TYPE)
	    {
	      write_user (u,
			  "\n~FB~OL>>>You are pulled back through cyberspace...\n");
	      sprintf (text, "REMVD %s\n", u->savename);
	      write_sock (u->netlink->socket, text);
	      sprintf (text, ">>>%s vanishes like an insane comet.\n",
		       u->savename);
	      destruct_user (u);
	      write_room (rm, text);
	      reset_access (rm);
	      num_of_users--;
	      break;
	    }
	  write_user (user,
		      "\n\n>>>You are already connected - switching to old session...\n");
	  sprintf (text, ">>>%s %s~RS swapped sessions (%s).\n", user->name,
		   strrepl (user->desc, "{name}", "somebody"), user->site);
	  write_room_except (NULL, text, user);
	  sprintf (text, "%s swapped sessions (%s).\n",
		   user->savename, user->site);
	  write_syslog (LOG_IO, text, LOG_TIME);
	  /* send Swap event */
	  event = E_SWAP;
	  strcpy (event_var, user->savename);
	  cnt_swp++;
	  close (u->socket);
	  u->socket = user->socket;
	  strcpy (u->site, user->site);
	  u->site_port = user->site_port;
	  strcpy (u->site, user->site);
	  u->misc_op = user->misc_op;
	  destruct_user (user);
	  num_of_logins--;
	  /* if user old session was locked, we must unlock it */
	  echo_on (u);
	  u->blind = 0;
	  u->ignall = 0;
	  /* remote... */
	  if (rm == NULL)
	    {
	      sprintf (text, "ACT %s look\n", u->savename);
	      write_sock (u->netlink->socket, text);
	    }
	  /* normal... */
	  else
	    {
	      look (u);
	      prompt (u);
	    }
	  /* Reset the sockets on any clones */
	  for (u2 = user_first; u2 != NULL; u2 = u2->next)
	    {
	      if (u2->type == CLONE_TYPE && u2->owner == user)
		{
		  u2->socket = u->socket;
		  u->owner = u;
		}
	    }
	  run_commands (u, RUNCOM_SWAP);	/* run Swap commands file */
	  return;
	}
    }

  write_user (user, "\n");
  cnt_in++;
  more (user, user->socket, POSTLOG_MSG);	/* send post-login messages */

  if (user->last_site[0])
    {
      sprintf (temp, "%s", ctime (&user->last_login));
      temp[strlen (temp) - 1] = 0;
      sprintf (text,
	       ">>>Welcome %s...\n\n~FB~OL>>>You were last logged in on %s from %s.\n\n",
	       user->name, temp, user->last_site);
    }
  else
    sprintf (text, ">>>Welcome %s...\n\n", user->name);
  write_user (user, text);

  pick_room (user);
/* check if user is transparent */
  if (!(user->level >= MIN_LEV_TRANSP && user->colour == 2))
    {
      sprintf (text,
	       "~OL~FG~EL>>>IN:~RS %s %s ~RS[%s:%d]...\n~OL~FG>>> --------- %-12s ----------------------------> (%s~RS~OL~FG?)\n",
	       user->name, strrepl (user->desc, "{name}", "somebody"),
	       user->ssite, user->site_port, user->room->name,
	       level_name[user->slevel]);
      write_room (NULL, text);
    }
  else
    {
      sprintf (text,
	       "~OL~FG~EL>>>IN:~RS %s %s ~RS[%s:%d]...(transparent)\n~OL~FG>>> --------- %-12s ----------------------------> (%s~RS~OL~FG?)\n",
	       user->name, strrepl (user->desc, "{name}", "somebody"),
	       user->ssite, user->site_port, user->room->name,
	       level_name[user->slevel]);
      write_level (SGOD, text, user);
    }
  user->last_login = time (0);	/* set to now */
  sprintf (text, "~FT>>>Your level is:~RS~OL %s\n", level_name[user->slevel]);
  write_user (user, text);
  check_watch (user);		/* check watch... */
  look (user);
  run_commands (user, RUNCOM_IN);	/* run IN commands file */
  if (has_unread_mail (user))	/* check mail */
    write_user (user, "~EL~FT~OL~LI--->>> UNREAD MAIL! <<<---\n");
  else
    mail_from (user, 1);
  prompt (user);
/* write some greetings and help messages */
  write_greetings (user);
  if (user->level <= MAX_LEV_LOGHLP)
    {
      version (user);
      write_user (user,
		  "~OLGAEN:~RS If you need help just type ~FG~OL?~RS or ~FG~OL.help~RS then press ~FG~OLENTER~RS key.\n");
      hint (user, 0);
    }
  sprintf (text, ">>>%s logged in on port %d from %s:%d.\n",
	   user->savename, user->port, user->site, user->site_port);
  write_syslog (LOG_IO, text, LOG_TIME);
  num_of_users++;
  num_of_logins--;
  user->login = 0;
/* send In event */
  event = E_IN;
  strcpy (event_var, user->savename);
}


/*** Disconnect user from talker ***/
disconnect_user (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  NL_OBJECT nl;
  char *strrepl ();

  rm = user->room;
  if (user->login)
    {
      close (user->socket);
      destruct_user (user);
      num_of_logins--;
      return;
    }
  if (user->type != REMOTE_TYPE)
    {
      save_user_details (user, 1);
      sprintf (text, ">>>%s logged out.\n", user->savename);
      write_syslog (LOG_IO, text, LOG_TIME);
      run_commands (user, RUNCOM_OUT);	/* run OUT commands file */
      write_user (user,
		  "\n~OL~FY--->>> You are removed from this dimension... Go and sleep no more! <<<---\n\n");
      write_user (user, "\n~OL~FG>>>Thank you for using...\n");
      version (user);
      close (user->socket);
      /* check if user is transparent */
      if (!(user->level >= MIN_LEV_TRANSP && user->colour == 2))
	{
	  sprintf (text,
		   "~OL~FR<<<OUT:~RS %s %s\n~OL~FR<<< ------------------------------------------------------< going to sleep...\n",
		   user->name, strrepl (user->desc, "{name}", "somebody"));
	  write_room (NULL, text);
	}
      else
	{
	  sprintf (text,
		   "~OL~FR<<<OUT:~RS %s %s\n~OL~FR<<< ---------------------------- transparent -------------< going to sleep...\n",
		   user->name, strrepl (user->desc, "{name}", "somebody"));
	  write_level (SGOD, text, user);
	}
      /* remote ... */
      if (user->room == NULL)
	{
	  sprintf (text, "REL %s\n", user->savename);
	  write_sock (user->netlink->socket, text);
	  for (nl = nl_first; nl != NULL; nl = nl->next)
	    if (nl->mesg_user == user)
	      {
		nl->mesg_user = (UR_OBJECT) - 1;
		break;
	      }
	}
    }
  else
    {
      write_user (user,
		  "\n~FR~OL>>>You are pulled back in disgrace to your own domain...\n");
      sprintf (text, "REMVD %s\n", user->savename);
      write_sock (user->netlink->socket, text);
      sprintf (text, "~FR~OL>>>%s is banished from here!\n", user->savename);
      write_room_except (rm, text, user);
      sprintf (text, "NETLINK: Remote user %s removed.\n", user->savename);
      write_syslog (LOG_LINK, text, LOG_TIME);
    }
  if (user->malloc_start != NULL)
    free (user->malloc_start);
  num_of_users--;
  cnt_out++;
/* send Out event */
  event = E_OUT;
  strcpy (event_var, user->savename);

/* Destroy any clones */
  destroy_user_clones (user);
  destruct_user (user);
  reset_access (rm);
  destructed = 0;
}

/*** Tell telnet not to echo characters - for password entry and lock command ***/
echo_off (user)
     UR_OBJECT user;
{
  char seq[4];

  if (password_echo)
    return;
  sprintf (seq, "%c%c%c", 255, 251, 1);
  write_user (user, seq);
}


/*** Tell telnet to echo characters ***/
echo_on (user)
     UR_OBJECT user;
{
  char seq[4];

  if (password_echo)
    return;
  sprintf (seq, "%c%c%c", 255, 252, 1);
  write_user (user, seq);
}

/*** check if password is 'secure' ( 1 means 'secure' ) ***/
secure_pass (pass)
     char *pass;
{
  int i, k;

  /* test 1 */
  if (chk_spass_tests <= CHK_SPASS_NONE)
    return 1;
  k = 1;			/* test if password contains only letters or digits */
  for (i = 0; i < strlen (pass); i++)
    if (!isalnum (pass[i]))
      {
	k = 0;
	break;
      }
  if (k)
    return 0;
  /* test 2 */
  if (chk_spass_tests <= CHK_SPASS_ALNUM)
    return 1;
  i = 0;			/* test if there are similar characters... */
  while (pass[i] != '\0' && pass[i + 1] != '\0')
    {
      if (toupper (pass[i]) == toupper (pass[i + 1]) ||
	  pass[i] == pass[i + 1] + 1 || pass[i] == pass[i + 1] - 1)
	return 0;
      i++;
    }
  /* test 3 */
  if (chk_spass_tests <= CHK_SPASS_SIMIL)
    return 1;
  i = 0;			/* test if password is palindrome */
  k = strlen (pass) - 1;
  while (i < k)
    {
      if (tolower (pass[i]) == tolower (pass[k]))
	return 0;
      i++;
      k--;
    }
  return 1;
}

/************ MISCELLANIOUS FUNCTIONS *************/

/*** Actions executed when an user was sweared ***/
swear_action (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  if (user->level >= MIN_LEV_NOSWR)
    {
      write_user (user, "~OLGAEN~RS: Hmm... Superusers don't swear! :-)\n");
      return;			/* for super-users, no effect */
    }
  user->muzzled = 1;		/* set muzzle flag */
  user->restrict[RESTRICT_PROM] = restrict_string[RESTRICT_PROM];
  sprintf (text, "~EL~OL~LI>>>%s just shot himself in the mouth!!!\n",
	   user->savename);
  write_room_except (NULL, text, user);
  write_user (user, noswearing);
  sprintf (text, "%s (%s)\n", user->savename, inpstr);
  write_syslog (LOG_SWEAR, text, LOG_TIME);
  event = E_SWEAR;
  strcpy (event_var, user->savename);
}

/*** Miscellanious operations from user that are not speech or commands ***/
misc_ops (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  switch (user->misc_op)
    {
    case 1:			/* shutdown question... */
      if (toupper (inpstr[0]) == 'Y')
	{
	  if (rs_countdown && !rs_which)
	    {
	      if (rs_countdown > 60)
		sprintf (text,
			 "\n\07~OLGAEN: ~FR~LISHUTDOWN INITIATED, shutdown in %d minutes, %d seconds!\n\n",
			 rs_countdown / 60, rs_countdown % 60);
	      else
		sprintf (text,
			 "\n\07~OLGAEN: ~FR~LISHUTDOWN INITIATED, shutdown in %d seconds!\n\n",
			 rs_countdown);
	      write_room (NULL, text);
	      sprintf (text,
		       "%s initiated a %d seconds SHUTDOWN countdown.\n",
		       user->savename, rs_countdown);
	      write_syslog (LOG_CHAT, text, LOG_TIME);
	      rs_user = user;
	      rs_announce = time (0);
	      user->misc_op = 0;
	      prompt (user);
	      return 1;
	    }
	  talker_shutdown (user, NULL, 0);
	}
      /* This will reset any reboot countdown that was started, oh well */
      rs_countdown = 0;
      rs_announce = 0;
      rs_which = -1;
      rs_user = NULL;
      user->misc_op = 0;
      write_user (user, excellent);
      prompt (user);
      return 1;

    case 2:			/* more() question... */
      if (toupper (inpstr[0]) == 'E'
	  || more (user, user->socket, user->page_file) != 1)
	{
	  user->misc_op = 0;
	  user->filepos = 0;
	  user->page_file[0] = '\0';
	  prompt (user);
	}
      return 1;

    case 3:			/* writing on board */
    case 4:			/* writing mail */
    case 5:			/* doing profile */
    case 10:			/* writing a prayer */
      editor (user, inpstr);
      return 1;

    case 6:			/* suicide question... */
      if (toupper (inpstr[0]) == 'Y')
	{
	  sprintf (text, "~EL~FR~OL>>>%s is commiting suicide!\n",
		   user->savename);
	  write_room_except (NULL, text, user);
	  delete_user (user, 1);
	}
      else
	{
	  user->misc_op = 0;
	  write_user (user, excellent);
	  prompt (user);
	}
      return 1;

    case 7:			/* reboot question... */
      if (toupper (inpstr[0]) == 'Y')
	{
	  if (rs_countdown && rs_which == 1)
	    {
	      if (rs_countdown > 60)
		sprintf (text,
			 "\n\07~OLGAEN: ~FY~LIREBOOT INITIATED, rebooting in %d minutes, %d seconds!\n\n",
			 rs_countdown / 60, rs_countdown % 60);
	      else
		sprintf (text,
			 "\n\07~OLGAEN: ~FY~LIREBOOT INITIATED, rebooting in %d seconds!\n\n",
			 rs_countdown);
	      write_room (NULL, text);
	      sprintf (text, "%s initiated a %d seconds REBOOT countdown.\n",
		       user->savename, rs_countdown);
	      write_syslog (LOG_CHAT, text, LOG_TIME);
	      rs_user = user;
	      rs_announce = time (0);
	      user->misc_op = 0;
	      prompt (user);
	      return 1;
	    }
	  talker_shutdown (user, NULL, 1);
	}
      if (rs_which == 1 && rs_countdown && rs_user == NULL)
	{
	  rs_countdown = 0;
	  rs_announce = 0;
	  rs_which = -1;
	}
      user->misc_op = 0;
      write_user (user, excellent);
      prompt (user);
      return 1;

    case 8:			/* alert question... */
      user->blind = 0;
      if (toupper (inpstr[0]) == 'Y')
	{
	  user->ignall = user->ignall_store;
	  user->afk = 0;
	  event = E_NOAFK;
	  strcpy (event_var, user->savename);
	  user->misc_op = 0;
	  sprintf (text, ">>>%s escapes from the alien creatures.\n",
		   user->vis ? user->name : invisname);
	  write_room_except (NULL, text, user);
	  write_user (user, ">>>You exit from alert mode...\n");
	  prompt (user);
	  return 1;
	}
      user->blind = 1;
      user->misc_op = 8;
      return 1;

    case 9:			/* lock question... */
      user->blind = 0;
      echo_off (user);
      if (!strcmp (word[0], "who"))	/* even the user is locked, he/she can use .who */
	who (user, LIST_WHO);
      else if (!strcmp ((char *) crypt (word[0], PASS_SALT), user->pass))
	{
	  echo_on (user);
	  user->ignall = user->ignall_store;
	  user->afk = 0;
	  event = E_NOAFK;
	  strcpy (event_var, user->savename);
	  user->misc_op = 0;
	  write_user (user, ">>>You exit from lock mode...\n");
	  prompt (user);
	  sprintf (text, ">>>%s exits from lock mode.\n",
		   user->vis ? user->name : invisname);
	  write_room_except (NULL, text, user);
	  return 1;
	}
      /* invalid password */
      sprintf (text,
	       "~OLGAEN:~RS Wrong password - Session owner: %s { ~FT%s~RS }\n",
	       user->savename, user->afkmesg);
      write_user (user, text);
      user->blind = 1;
      user->afk = 1;
      user->misc_op = 9;
      return 1;
    }
  return 0;
}

/*** Show version and other useful info ***/
version (user)
     UR_OBJECT user;
{
  char hostname[SITE_NAME_LEN + 1];

  sprintf (text,
	   "\n~OL>>>NUTS version ~FG%s~RS~OL, GAEN version ~FG%s~RS~OL (update on %s)\n~OL>>>GSL version ~FG%s~RS~OL, gsh version ~FG%s~RS~OL, XML DTD version ~FG%s~RS~OL.\n",
	   VERSION, GAEN_VER, GAEN_UPDATE, GSL_VER, GSH_VER, GAENDTD_VER);
  write_user (user, text);
  if (gethostname (hostname, SITE_NAME_LEN) != -1)
    {
      sprintf (text, "~OL>>>Running on ~FG%s~RS~OL...\n", hostname);
      write_user (user, text);
    }
  write_user (user,
	      "~OL>>>Authors: ~FGNeil Robertson~RS~OL <neil@ogham.demon.co.uk>\n            ~OL~FGSabin Corneliu Buraga~RS~OL <busaco@infoiasi.ro> helped by\n            ~OL~FGVictor Tarhon-Onu~RS~OL <mituc@ac.tuiasi.ro>\n");
  write_user (user,
	      "~OL>>>Made in ~FBEng~FRland~RS~OL and ~FRRo~FYman~FBia~RS~OL.\n\n");
}

/*** See if an user has access to a room. If room is fixed to private then
     it's considered a saint room so grant permission to any user of SAINT 
     and above for those. ***/
has_room_access (user, rm)
     UR_OBJECT user;
     RM_OBJECT rm;
{
  if ((rm->access & PRIVATE) && user->level < gatecrash_level &&
      user->invite_room != rm && !((rm->access & FIXED) &&
				   user->level >= MIN_LEV_ACCPRIV))
    return 0;
  return 1;
}

/*** Safety unlink file ***/
s_unlink (filename)
     char *filename;
{
  char rname[80];
  if (safety)			/* not unlink file, only rename it */
    {
      sprintf (rname, "%s%s%d", filename, SAFETY_EXT,
	       random () % MAX_SAFETY_VAL);
      sprintf (text, "File %s (safety) unlinked - renamed in: %s.\n",
	       filename, rname);
      write_syslog (LOG_ERASE, text, LOG_TIME);
      return rename (filename, rname);
    }
  else
    {
      sprintf (text, "File %s unlinked.\n", filename);
      write_syslog (LOG_ERASE, text, LOG_TIME);
      return unlink (filename);
    }
}

/*** Generate an unique file name ***/
char *
genfilename (basename)
     char *basename;
{
  static char filename[SITE_NAME_LEN + 1];
  sprintf (filename, "%s_%d@%02d:%02d", basename, getpid (), thour, tmin);
  return (filename);
}

/*** Pick a room randomly; used in login process... ***/
pick_room (user)
     UR_OBJECT user;
{
  int num = random () % (user->level + 1);
  RM_OBJECT rm = room_first[user->dimension];

  if (!enable_rndgo)		/* random .go sky is disabled */
    {
      user->room = room_first[user->dimension];
      return;
    }
  while (num >= 0 && rm != NULL)
    {
      rm = rm->next;
      num--;
    }
  user->room = (rm == NULL || user->level <= MAX_LEV_NOPICK) ?
    room_first[user->dimension] : rm;
}

/*** The editor used for writing profiles, mail and messages on the boards ***/
editor (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  int cnt, line, i;
  char *edprompt =
    "\n>>>Your options are: ~FG~OLSave~RS, ~FY~OLRedo~RS, ~FR~OLAbort~RS, ~FB~OLView info~RS (s/r/a/v): ";
  char *ptr, *c;
  char text1[30];

  if (user->edit_op)
    {
      switch (toupper (*inpstr))
	{
	case 'S':		/* save text */
	  sprintf (text, ">>>%s finishes composing some text.\n",
		   user->vis ? user->name : invisname);
	  write_room_except (user->room, text, user);

	  switch (user->misc_op)
	    {
	    case 3:
	      write_board (user, NULL, 1);
	      break;
	    case 4:
	      smail (user, NULL, 1);
	      break;
	    case 5:
	      enter_profile (user, 1);
	      break;
	    case 10:
	      enter_prayer (user, 1);
	      break;
	    }

	  editor_done (user);
	  return;

	case 'R':		/* redo editing */
	  user->edit_op = 0;
	  user->edit_line = 1;
	  user->charcnt = 0;
	  user->malloc_end = user->malloc_start;
	  *user->malloc_start = '\0';
	  sprintf (text, "\n>>>Redo message...\n\n~FG%d>~RS",
		   user->edit_line);
	  write_user (user, text);
	  return;

	case 'A':		/* abort editing */
	  sprintf (text, ">>>%s gives up composing some text.\n",
		   user->vis ? user->name : invisname);
	  write_user (user, "\n>>>Message aborted.\n");
	  editor_done (user);
	  return;

	case 'V':		/* view text info */
	  switch (user->misc_op)
	    {
	    case 3:
	      strcpy (text1, "a message on the board");
	      break;
	    case 4:
	      strcpy (text1, "a mail");
	      break;
	    case 5:
	      strcpy (text1, "a profile");
	      break;
	    case 10:
	      strcpy (text1, "a prayer");
	      break;
	    default:
	      strcpy (text1, "an unknown letter");
	    }
	  i = 0;
	  c = user->malloc_start;
	  while (c != user->malloc_end)	/* count number of chars */
	    {
	      i++;
	      c++;
	    }
	  sprintf (text,
		   ">>>~FT%d~RS lines (~FT%d~RS characters) to write %s.\n",
		   user->edit_line, i, text1);
	  write_user (user, text);
	  write_user (user, edprompt);
	  return;

	default:
	  write_user (user, edprompt);
	  return;
	}
    }
  if (user->malloc_start == NULL)
    {
      if ((user->malloc_start = (char *) malloc (MAX_LINES * 81)) == NULL)
	{
	  sprintf (text, ">>>%s: failed to allocate buffer memory.\n",
		   syserror);
	  write_user (user, text);
	  write_syslog (LOG_ERR,
			"ERROR: Failed to allocate memory in editor().\n", 
			LOG_NOTIME);
	  user->misc_op = 0;
	  prompt (user);
	  return;
	}
      user->ignall_store = user->ignall;
      user->ignall = 1;		/* Dont want chat mucking up the edit screen */
      user->edit_line = 1;
      user->charcnt = 0;
      user->malloc_end = user->malloc_start;
      *user->malloc_start = '\0';
      sprintf (text,
	       "~FT>>>Maximum of ~FG%d~FT lines, end with a '~FG.~FT' on a line by itself.\n\n~FG1>~RS",
	       MAX_LINES);
      write_user (user, text);
      sprintf (text, ">>>%s starts composing some text...\n",
	       user->vis ? user->name : invisname);
      write_room_except (user->room, text, user);
      return;
    }

/* Check for empty line */
  if (!word_count)
    {
      if (!user->charcnt)
	{
	  sprintf (text, "~FG%d>~RS", user->edit_line);
	  write_user (user, text);
	  return;
	}
      *user->malloc_end++ = '\n';
      if (user->edit_line == MAX_LINES)
	goto END;
      sprintf (text, "~FG%d>~RS", ++user->edit_line);
      write_user (user, text);
      user->charcnt = 0;
      return;
    }
/* If nothing carried over and a dot is entered then end */
  if (!user->charcnt && !strcmp (inpstr, "."))
    goto END;

  line = user->edit_line;
  cnt = user->charcnt;

/* loop through input and store in allocated memory */
  while (*inpstr)
    {
      *user->malloc_end++ = *inpstr++;
      if (++cnt == 80)
	{
	  user->edit_line++;
	  cnt = 0;
	}
      if (user->edit_line > MAX_LINES
	  || user->malloc_end - user->malloc_start >= MAX_LINES * 81)
	goto END;
    }
  if (line != user->edit_line)
    {
      ptr = (char *) (user->malloc_end - cnt);
      *user->malloc_end = '\0';
      sprintf (text, "~FG%d>~RS%s", user->edit_line, ptr);
      write_user (user, text);
      user->charcnt = cnt;
      return;
    }
  else
    {
      *user->malloc_end++ = '\n';
      user->charcnt = 0;
    }
  if (user->edit_line != MAX_LINES)
    {
      sprintf (text, "~FG%d>~RS", ++user->edit_line);
      write_user (user, text);
      return;
    }

/* User has finished his message , prompt for what to do now */
END:
  *user->malloc_end = '\0';
  if (*user->malloc_start)
    {
      write_user (user, edprompt);
      user->edit_op = 1;
      return;
    }
  write_user (user, "\n>>>No text.\n");
  sprintf (text, ">>>%s gives up composing some text.\n",
	   user->vis ? user->name : invisname);
  write_room_except (user->room, text, user);
  editor_done (user);
}

/* Done editing */
editor_done (user)
     UR_OBJECT user;
{
  user->misc_op = 0;
  user->edit_op = 0;
  user->edit_line = 0;
  free (user->malloc_start);
  user->malloc_start = NULL;
  user->malloc_end = NULL;
  user->ignall = user->ignall_store;
  prompt (user);
}


/*** Record speech and emotes in the room. It stores 2 lines of speech
     per room. ***/
record (rm, str)
     RM_OBJECT rm;
     char *str;
{
  strncpy (rm->revbuff[rm->revline], str, REVIEW_LEN);
  rm->revbuff[rm->revline][REVIEW_LEN] = '\n';
  rm->revbuff[rm->revline][REVIEW_LEN + 1] = '\0';
  rm->revline = (rm->revline + 1) % REVIEW_LINES;
}

/*** Records tells, whispers, replies and pemotes sent to the user. ***/
record_tell (user, str)
     UR_OBJECT user;
     char *str;
{
  strncpy (user->revbuff[user->revline], str, REVIEW_LEN);
  user->revbuff[user->revline][REVIEW_LEN] = '\n';
  user->revbuff[user->revline][REVIEW_LEN + 1] = '\0';
  user->revline = (user->revline + 1) % REVTELL_LINES;
}

/*** Records pictures sent to the user. ***/
record_pict (user, str)
     UR_OBJECT user;
     char *str;
{
  strncpy (user->revpict[user->revpictline], str, REVPICT_LEN);
  user->revpict[user->revpictline][REVPICT_LEN] = '\n';
  user->revpict[user->revpictline][REVPICT_LEN + 1] = '\0';
  user->revpictline = (user->revpictline + 1) % REVPICT_LINES;
}

/*** Set room access back to public if not enough users in room ***/
reset_access (rm)
     RM_OBJECT rm;
{
  UR_OBJECT u;
  int cnt;

  if (rm == NULL || rm->access != PRIVATE)
    return;
  cnt = 0;
  for (u = user_first; u != NULL; u = u->next)
    if (u->room == rm)
      ++cnt;
  if (cnt < min_private_users)
    {
      write_room (rm, ">>>Sky access returned to ~FGPUBLIC.\n");
      rm->access = PUBLIC;

      /* Reset any invites into the room & clear review buffer */
      for (u = user_first; u != NULL; u = u->next)
	{
	  if (u->invite_room == rm)
	    u->invite_room = NULL;
	}
      clear_rbuff (rm);
    }
}



/*** Exit cos of error during bootup ***/
boot_exit (code)
     int code;
{
  switch (code)
    {
    case 1:
      write_syslog (LOG_CHAT,
		    "BOOT FAILURE: Error while parsing configuration file.\n",
		    LOG_NOTIME);
      exit (1);

    case 2:
      write_syslog (LOG_CHAT,
		    "BOOT FAILURE: Can't open main port listen socket.\n", 
		    LOG_NOTIME);
      perror ("GAEN: Can't open main listen socket");
      exit (2);

    case 3:
      write_syslog (LOG_CHAT,
		    "BOOT FAILURE: Can't open superuser port listen socket.\n",
		    LOG_NOTIME);
      perror ("GAEN: Can't open superuser listen socket");
      exit (3);

    case 4:
      write_syslog (LOG_CHAT,
		    "BOOT FAILURE: Can't open link port listen socket.\n", 
		    LOG_NOTIME);
      perror ("GAEN: Can't open info listen socket");
      exit (4);

    case 5:
      write_syslog (LOG_CHAT, "BOOT FAILURE: Can't bind to main port.\n", LOG_NOTIME);
      perror ("GAEN: Can't bind to main port");
      exit (5);

    case 6:
      write_syslog (LOG_CHAT, "BOOT FAILURE: Can't bind to superuser port.\n",
		    LOG_NOTIME);
      perror ("GAEN: Can't bind to superuser port");
      exit (6);

    case 7:
      write_syslog (LOG_CHAT, "BOOT FAILURE: Can't bind to link port.\n", LOG_NOTIME);
      perror ("GAEN: Can't bind to link port");
      exit (7);

    case 8:
      write_syslog (LOG_CHAT, "BOOT FAILURE: Listen error on main port.\n",
		    LOG_NOTIME);
      perror ("GAEN: Listen error on main port");
      exit (8);

    case 9:
      write_syslog (LOG_CHAT,
		    "BOOT FAILURE: Listen error on superuser port.\n", LOG_NOTIME);
      perror ("GAEN: Listen error on superuser port");
      exit (9);

    case 10:
      write_syslog (LOG_CHAT, "BOOT FAILURE: Listen error on link port.\n",
		    LOG_NOTIME);
      perror ("GAEN: Listen error on link port");
      exit (10);

    case 11:
      write_syslog (LOG_CHAT, "BOOT FAILURE: Failed to fork.\n", LOG_NOTIME);
      perror ("GAEN: Failed to fork");
      exit (11);
    }
}

/*** User prompt ***/
prompt (user)
     UR_OBJECT user;
{
  int hr, min;

  if (no_prompt)
    return;
  if (user->type == REMOTE_TYPE)
    {
      sprintf (text, "PRM %s\n", user->savename);
      write_sock (user->netlink->socket, text);
      return;
    }
  if (user->command_mode && !user->misc_op)
    {
      write_user (user, "~FTCOM>~RS ");
      return;
    }
  if (!user->prompt || user->misc_op)
    return;
  hr = (int) (time (0) - user->last_login) / 3600;
  min = ((int) (time (0) - user->last_login) % 3600) / 60;
  if (user->room != NULL)
    sprintf (text,
	     "~FY[%02d:%02d, %02d:%02d, ~FT%s~FY * %s * reply: %s~RS~FY * %d ball(s)]\n",
	     thour, tmin, hr, min, user->name, user->room->name,
	     user->reply_user[0] ? user->reply_user : "?", user->pballs);
  else
    sprintf (text,
	     "~FY[%02d:%02d, %02d:%02d, ~FT%s~FY * %s * reply: %s~RS~FY * %d ball(s)]\n",
	     thour, tmin, hr, min, user->name,
	     user->netlink->connect_room->name, "?", user->pballs);
  write_user (user, text);
}

/*** Write greetings in function of week day ***/
write_greetings (user)
     UR_OBJECT user;
{
  write_user (user, greetings[twday]);
}

/*** Page a file out to user. Colour commands in files will only work if the
     user!=NULL since if NULL we dont know if his terminal can support colour
     or not.
     Return value 0=cannot find file, 1= found file, 2=found and finished ***/
more (user, sock, filename)
     UR_OBJECT user;
     int sock;
     char *filename;
{
  int i, buffpos, num_chars, lines, retval, len;
  char buff[OUT_BUFF_SIZE], text2[81], *str, *colour_com_strip ();
  FILE *fp;

  if (!(fp = fopen (filename, "r")))
    {
      if (user != NULL)
	user->filepos = 0;
      return 0;
    }
/* jump to reading posn in file */
  if (user != NULL)
    fseek (fp, user->filepos, 0);

  text[0] = '\0';
  buffpos = 0;
  num_chars = 0;
  retval = 1;
  len = 0;

/* This is a hack to fix an annoying bug I cannot solve */
  if (sock == -1)
    {
      lines = 1;
      fgets (text2, 82, fp);
    }
  else
    {
      lines = 0;
      fgets (text, sizeof (text) - 1, fp);
    }

/* Go through file */
  while (!feof (fp)
	 && (lines < ((user == NULL) ? DEF_LINES : user->plines)
	     || user == NULL))
    {
      if (sock == -1)
	{
	  lines++;
	  if (user->netlink->ver_major <= 3 && user->netlink->ver_minor < 2)
	    str = colour_com_strip (text2);
	  else
	    str = text2;
	  if (str[strlen (str) - 1] != '\n')
	    sprintf (text, "MSG %s\n%s\nEMSG\n", user->savename, str);
	  else
	    sprintf (text, "MSG %s\n%sEMSG\n", user->savename, str);
	  write_sock (user->netlink->socket, text);
	  num_chars += strlen (str);
	  fgets (text2, 82, fp);
	  continue;
	}
      str = text;

      /* Process line from file */
      while (*str)
	{
	  if (*str == '\n')
	    {
	      if (buffpos > OUT_BUFF_SIZE - 6)
		{
		  write (sock, buff, buffpos);
		  buffpos = 0;
		}
	      /* Reset terminal before every newline */
	      if (user != NULL && user->colour % 2)
		{
		  memcpy (buff + buffpos, "\033[0m", 4);
		  buffpos += 4;
		}
	      *(buff + buffpos) = '\n';
	      *(buff + buffpos + 1) = '\r';
	      buffpos += 2;
	      ++str;
	    }
	  else
	    {
	      /* Process colour commands in the file. See write_user()
	         function for full comments on this code. */
	      if (*str == '/' && *(str + 1) == '~')
		{
		  ++str;
		  continue;
		}
	      if (str != text && *str == '~' && *(str - 1) == '/')
		{
		  *(buff + buffpos) = *str;
		  goto CONT;
		}
	      if (*str == '~')
		{
		  if (buffpos > OUT_BUFF_SIZE - 6)
		    {
		      write (sock, buff, buffpos);
		      buffpos = 0;
		    }
		  ++str;
		  for (i = 0; i < NUM_COLS; ++i)
		    {
		      if (!strncmp (str, colcom[i], 2))
			{
			  if (user != NULL && user->colour % 2)
			    {
			      memcpy (buffpos + buff, colcode[i],
				      strlen (colcode[i]));
			      buffpos += strlen (colcode[i]) - 1;
			    }
			  else
			    buffpos--;
			  ++str;
			  goto CONT;
			}
		    }
		  *(buff + buffpos) = *(--str);
		}
	      else
		*(buff + buffpos) = *str;
	    CONT:
	      ++buffpos;
	      ++str;
	    }
	  if (buffpos == OUT_BUFF_SIZE)
	    {
	      write (sock, buff, OUT_BUFF_SIZE);
	      buffpos = 0;
	    }
	}
      len = strlen (text);
      num_chars += len;
      lines += len / 80 + (len < 80);
      fgets (text, sizeof (text) - 1, fp);
    }
  if (buffpos && sock != -1)
    write (sock, buff, buffpos);

/* if user is logging in, then don't page it */
  if (user == NULL)
    {
      fclose (fp);
      return 2;
    }
  if (feof (fp))
    {
      user->filepos = 0;
      no_prompt = 0;
      retval = 2;
    }
  else
    {
      /* store file position and file name */
      user->filepos += num_chars;
      strcpy (user->page_file, filename);
      /* We use E here instead of Q because when on a remote system and
         in COMMAND mode the Q will be intercepted by the home system and 
         quit the user */
      write_user (user,
		  "~FB     --->>> Press ~FG~OL<return>~RS~FB to continue, ~FG~OLE<return>~RS~FB to exit... <<<---     ");
      no_prompt = 1;
    }
  fclose (fp);
  return retval;
}


/*** Set global vars. hours,minutes,seconds,date,day,month,year ***/
set_date_time ()
{
  struct tm *tm_struct;		/* structure is defined in time.h */
  time_t tm_num;

/* Set up the structure */
  time (&tm_num);
  tm_struct = localtime (&tm_num);

/* Get the values */
  tday = tm_struct->tm_yday;
  tyear = 1900 + tm_struct->tm_year;
  tmonth = tm_struct->tm_mon;
  tmday = tm_struct->tm_mday;
  twday = tm_struct->tm_wday;
  thour = tm_struct->tm_hour;
  tmin = tm_struct->tm_min;
  tsec = tm_struct->tm_sec;
}


clear_rbuff (rm)
     RM_OBJECT rm;
{
  int c;
  for (c = 0; c < REVIEW_LINES; ++c)
    rm->revbuff[c][0] = '\0';
  rm->revline = 0;
}


/*** Return pos. of second word in inpstr ***/
char *
remove_first (inpstr)
     char *inpstr;
{
  char *pos = inpstr;
  while (*pos < 33 && *pos)
    ++pos;
  while (*pos > 32)
    ++pos;
  while (*pos < 33 && *pos)
    ++pos;
  return pos;
}


/*** Returns 1 if string is a positive number ***/
isnumber (str)
     char *str;
{
  while (*str)
    if (!isdigit (*str++))
      return 0;
  return 1;
}

/*** Get real user struct pointer from name ***/
UR_OBJECT get_real_user (name)
     char *
       name;
{
  UR_OBJECT u;

  name[0] = toupper (name[0]);
/* Search for exact name */
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->login || u->type == CLONE_TYPE)
	continue;
      if (!strcmp (u->savename, name))
	return u;
    }
/* Search for close match name */
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->login || u->type == CLONE_TYPE)
	continue;
      if (strstr (u->savename, name))
	return u;
    }
  return NULL;
}

/*** Get exact user struct pointer from name ***/
UR_OBJECT get_exact_user (name)
     char *
       name;
{
  UR_OBJECT u;

  name[0] = toupper (name[0]);
/* Search for exact name */
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->login || u->type == CLONE_TYPE)
	continue;
      if (!strcmp (u->savename, name))
	return u;
    }
  return NULL;
}

/*** Get user struct pointer from name ***/
UR_OBJECT get_user (name)
     char *
       name;
{
  UR_OBJECT u;

  if (name[0] == ':')
    return get_real_user (name + 1);
  if (name[0] == '$')
    return get_exact_user (name + 1);
  name[0] = toupper (name[0]);
/* Search for exact name */
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->login || u->type == CLONE_TYPE)
	continue;
      if (!strcmp (u->name, name))
	return u;
    }
/* Search for close match name */
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->login || u->type == CLONE_TYPE)
	continue;
      if (strstr (u->name, name))
	return u;
    }
  return NULL;
}

/*** Get room struct pointer from abbreviated name ***/
RM_OBJECT get_room (name)
     char *
       name;
{
  RM_OBJECT rm;
  int j;

  for (j = 0; j < MAX_DIMENS; j++)
    for (rm = room_first[j]; rm != NULL; rm = rm->next)
      if (!strncmp (rm->name, name, strlen (name)))
	return rm;
  return NULL;
}


/*** Return level value based on level name or level number ***/
get_level (name)
     char *name;
{
  int i;
  char *colour_com_strip ();

  if (isnumber (name))
    return atoi (name) % MAX_LEVELS;
  i = 0;
  while (level_name[i][0] != '*')
    {
      if (!stricmp (level_name[i], name) ||
	  !stricmp (colour_com_strip (level_name[i]), name))
	return i;
      ++i;
    }
  return -1;
}


/*** See if user has unread mail, mail file has last read time on its
     first line ***/
has_unread_mail (user)
     UR_OBJECT user;
{
  FILE *fp;
  int tm;
  char filename[80];

  sprintf (filename, "%s/%s.M", USERFILES, user->savename);
  if (!(fp = fopen (filename, "r")))
    return 0;
  fscanf (fp, "%d", &tm);
  fclose (fp);
  return (tm > (int) user->read_mail);
}

/*** This is function that sends mail to other users ***/
send_mail (user, to, ptr)
     UR_OBJECT user;
     char *to, *ptr;
{
  NL_OBJECT nl;
  FILE *infp, *outfp;
  char *c, d, *service, filename[80], line[DNL + 1];
  char *long_date ();

/* See if remote mail */
  c = to;
  service = NULL;
  while (*c)
    {
      if (*c == '@')
	{
	  service = c + 1;
	  *c = '\0';
	  for (nl = nl_first; nl != NULL; nl = nl->next)
	    {
	      if (!strcmp (nl->service, service) && nl->stage == UP)
		{
		  send_external_mail (nl, user, to, ptr);
		  return;
		}
	    }
	  sprintf (text, ">>>Service %s unavailable. Sorry...\n", service);
	  write_user (user, text);
	  return;
	}
      ++c;
    }

/* Local mail */
  if (!(outfp = fopen ("tempfile", "w")))
    {
      write_user (user, ">>>Error in mail delivery.\n");
      write_syslog (LOG_ERR,
		    "ERROR: Couldn't open tempfile in send_mail().\n", LOG_NOTIME);
      return;
    }
/* Write current time on first line of tempfile */
  fprintf (outfp, "%d\r", (int) time (0));

/* Copy current mail file into tempfile if it exists */
  sprintf (filename, "%s/%s.M", USERFILES, to);
  if ((infp = fopen (filename, "r")) != NULL)
    {
      /* Discard first line of mail file. */
      fgets (line, DNL, infp);

      /* Copy rest of file */
      d = getc (infp);
      while (!feof (infp))
	{
	  putc (d, outfp);
	  d = getc (infp);
	}
      fclose (infp);
    }

/* Put new mail in tempfile */
  if (user != NULL)
    {
      if (user->type == REMOTE_TYPE)
	fprintf (outfp, "From: %s@%s  %s\n",
		 user->savename, user->netlink->service, long_date (0));
      else
	fprintf (outfp, "From: %s  %s\n", user->savename, long_date (0));
    }
  else
    fprintf (outfp, "From: MAILER  %s\n", long_date (0));

  fputs (ptr, outfp);
  fputs ("\n", outfp);
  fclose (outfp);
  rename ("tempfile", filename);
  write_user (user, ">>>Mail sent.\n");
  if (get_user (to) != NULL)
    write_user (get_user (to), "~EL~FT~OL~LI--->>> NEW MAIL! <<<---\n");
}

/* Used by send_mail() */
send_external_mail (nl, user, to, ptr)
     NL_OBJECT nl;
     UR_OBJECT user;
     char *to, *ptr;
{
  FILE *fp;
  char filename[80];

/* Write out to spool file first */
  sprintf (filename, "%s/OUT_%s_%s@%s", MAILSPOOL,
	   user->savename, to, nl->service);
  if (!(fp = fopen (filename, "a")))
    {
      sprintf (text, "%s: unable to spool mail.\n", syserror);
      write_user (user, text);
      sprintf (text,
	       "ERROR: Couldn't open file %s to append in send_external_mail().\n",
	       filename);
      write_syslog (LOG_ERR, text, LOG_NOTIME);
      return;
    }
  putc ('\n', fp);
  fputs (ptr, fp);
  fclose (fp);

/* Ask for verification of users existence */
  sprintf (text, "EXISTS? %s %s\n", to, user->savename);
  write_sock (nl->socket, text);

/* Rest of delivery process now up to netlink functions */
  write_user (user, ">>>Mail sent.\n");
}


/*** Convert string to upper case ***/
strtoupper (str)
     char *str;
{
  while (*str)
    {
      *str = toupper (*str);
      str++;
    }
}


/*** Convert string to lower case ***/
strtolower (str)
     char *str;
{
  while (*str)
    {
      *str = tolower (*str);
      str++;
    }
}

/*** Mix-up string ***/
strmix (str)
     char *str;
{
  char c;
  int i, len;

  len = strlen (str);
  for (i = 0; i < len - 2; i++, i++)
    {
      c = str[i];
      str[i] = str[i + 1];
      str[i + 1] = c;
    }
}

/*** Reverse string ***/
strrev (str)
     char *str;
{
  char c;
  int i, len;

  len = strlen (str);
  for (i = 0; i < (len >> 1); i++)
    {
      c = str[i];
      str[i] = str[len - i - 1];
      str[len - i - 1] = c;
    }
}

/*** Upper/Lower alternating case string ***/
str_upper_lower (str)
     char *str;
{
  int i, is_lower = 0;

  for (i = 0; i < strlen (str); i++)
    {
      if (isalpha (str[i]))
	{
	  str[i] = is_lower ? tolower (str[i]) : toupper (str[i]);
	  is_lower = !is_lower;
	}
    }
}

/*** Swap characters in string, randomly ***/
strswp (str)
     char *str;
{
  int swaps, pos1, pos2, len;
  char c;
  if ((len = strlen (str)) < 3)
    return;
  swaps = (len / 3) + 1;
  while (swaps > 0)
    {
      pos1 = random () % len;
      pos2 = random () % len;
      c = str[pos1];
      str[pos1] = str[pos2];
      str[pos2] = c;
      swaps--;
    }
}

/*** Compare two strings (case insensitive) ***/
stricmp (str1, str2)
     char *str1, *str2;
{
  for (; toupper (*str1) == toupper (*str2); str1++, str2++)
    if (*str1 == '\0')
      return 0;
  return (toupper (*str1) - toupper (*str2));
}

/*** String replacement: 'oldstr' is replaced by 'newstr' within 'str' ***/
char *
strrepl (str, oldstr, newstr)
     char *str, *oldstr, *newstr;
{
  static char text2[ARR_SIZE * 2 + 1];
  char *s, *p;
  int len;

  s = str;
  text2[0] = '\0';
  len = strlen (oldstr);
  do
    {
      if ((p = (char *) strstr (s, oldstr)) != NULL)	/* found */
	{
	  strncat (text2, s, p - s);
	  strcat (text2, newstr);
	  s = p + len;
	}
    }
  while (p && (*s));
  if (*s)
    strcat (text2, s);
  return (text2);
}

/*** See if string contains any swearing ***/
contains_swearing (str)
     char *str;
{
  char *s;
  int i;

  if ((s = (char *) malloc (strlen (str) + 1)) == NULL)
    {
      write_syslog (LOG_ERR,
		    "ERROR: Failed to allocate memory in contains_swearing().\n",
		    LOG_NOTIME);
      return 0;
    }
  strcpy (s, str);
  strtolower (s);
  i = 0;
  while (swear_words[i][0] != '*')
    {
      if (strstr (s, swear_words[i]))
	{
	  free (s);
	  return 1;
	}
      ++i;
    }
  free (s);
  return 0;
}

/*** Count the number of colour commands in a string ***/
colour_com_count (str)
     char *str;
{
  char *s;
  int i, cnt;

  s = str;
  cnt = 0;
  while (*s)
    {
      if (*s == '~')
	{
	  ++s;
	  for (i = 0; i < NUM_COLS; ++i)
	    {
	      if (!strncmp (s, colcom[i], 2))
		{
		  cnt++;
		  s++;
		  continue;
		}
	    }
	  continue;
	}
      ++s;
    }
  return cnt;
}


/*** Strip out colour commands from string for when we are sending strings
     over a netlink to a talker that doesn't support them ***/
char *
colour_com_strip (str)
     char *str;
{
  char *s, *t;
  static char text2[ARR_SIZE];
  int i;

  s = str;
  t = text2;
  while (*s)
    {
      if (*s == '~')
	{
	  ++s;
	  for (i = 0; i < NUM_COLS; ++i)
	    {
	      if (!strncmp (s, colcom[i], 2))
		{
		  s++;
		  goto CONT;
		}
	    }
	  --s;
	  *t++ = *s;
	}
      else
	*t++ = *s;
    CONT:
      s++;
    }
  *t = '\0';
  return text2;
}

/*** Clear user's screen ***/
cls (user)
     UR_OBJECT user;
{
  int i;

  for (i = 0; i < 6; ++i)
    write_user (user, "\n\n\n\n\n\n\n\n\n\n");
}

/*** Date string for board messages, mail, .who and .allclones etc. ***/
char *
long_date (which)
     int which;
{
  static char dstr[80];

  if (which)
    sprintf (dstr, "on %s %d %s %d at %02d:%02d (day %d)",
	     day[twday], tmday, month[tmonth], tyear, thour, tmin, tday + 1);
  else
    sprintf (dstr, "// %s %d %s %d at %02d:%02d (day %d) // ",
	     day[twday], tmday, month[tmonth], tyear, thour, tmin, tday + 1);
  return dstr;
}

/************ OBJECT FUNCTIONS ************/

/*** Construct user/clone object ***/
UR_OBJECT create_user ()
{
  UR_OBJECT user;
  int i;

  if ((user = (UR_OBJECT) malloc (sizeof (struct user_struct))) == NULL)
    {
      write_syslog (LOG_ERR,
		    "GAEN: Memory allocation failure in create_user().\n", 
		    LOG_NOTIME);
      return NULL;
    }

/* Append object into linked list. */
  if (user_first == NULL)
    {
      user_first = user;
      user->prev = NULL;
    }
  else
    {
      user_last->next = user;
      user->prev = user_last;
    }
  user->next = NULL;
  user_last = user;

/* initialise users - general */
  user->dimension = 0;
  user->type = USER_TYPE;
  user->savename[0] = '\0';
  user->name[0] = '\0';
  user->desc[0] = '\0';
  user->in_phrase[0] = '\0';
  user->out_phrase[0] = '\0';
  user->pass[0] = '\0';
  user->site[0] = '\0';
  user->last_site[0] = '\0';
  user->page_file[0] = '\0';
  user->mail_to[0] = '\0';
  user->buff[0] = '\0';
  user->buffpos = 0;
  user->filepos = 0;
  user->read_mail = time (0);
  user->room = NULL;
  user->invite_room = NULL;
  user->port = 0;
  user->login = 0;
  user->socket = -1;
  user->attempts = 0;
  user->command_mode = 0;
  user->level = newuser_deflevel;
  user->vis = 1;
  user->ignall = 0;
  user->ignpict = 0;
  user->ignall_store = 0;
  user->ignshout = 0;
  user->igntell = 0;
  user->muzzled = 0;
  user->remote_com = -1;
  user->netlink = NULL;
  user->pot_netlink = NULL;
  user->last_input = time (0);
  user->last_login = time (0);
  user->last_login_len = 0;
  user->total_login = 0;
  user->prompt = prompt_def;
  user->charmode_echo = 0;
  user->misc_op = 0;
  user->edit_op = 0;
  user->edit_line = 0;
  user->charcnt = 0;
  user->warned = 0;
  user->accreq = 0;
  user->afk = 0;
  user->colour = colour_def;
  user->clone_hear = CLONE_HEAR_ALL;
  user->malloc_start = NULL;
  user->malloc_end = NULL;
  user->owner = NULL;
  user->site_port = 0;
  user->level = newuser_deflevel;
  user->ssite[0] = '\0';
  user->reply_user[0] = '\0';
  user->subst = 0;
  user->afkmesg[0] = '\0';
  for (i = 0; i < REVTELL_LINES; ++i)
    user->revbuff[i][0] = '\0';
  user->revline = 0;
  for (i = 0; i < REVPICT_LINES; ++i)
    user->revpict[i][0] = '\0';
  user->revpictline = 0;
  user->restrict[0] = '\0';
  user->w_number = 0;
  for (i = 0; i < MAX_WUSERS; i++)
    user->w_users[i][0] = '\0';
  user->inpstr_old[0] = '\0';
  user->macros = 0;
  for (i = 0; i < MAX_USER_MACROS; i++)
    {
      user->macro_name[i][0] = '\0';
      user->macro_desc[i][0] = '\0';
    }
  user->say_tone = 0;
  user->tell_tone = 0;
  user->stereo = 0;
  strcpy (user->var1, "{");
  strcpy (user->var2, "}");
  user->alarm_hour = -1;
  user->alarm_min = -1;
  user->alarm_activated = 0;
  user->alarm_cmd[0] = '\0';
  user->blind = 0;
  user->wrap = 0;
  for (i = 0; i < MAX_EVENTS; i++)
    user->events_def[i][0] = '\0';
  user->hangman = NULL;
  user->puzzle = NULL;
  user->pballs = def_pballs;
  user->plines = def_lines;
  for (i = 0; i < MAX_IGNRD_USERS; i++)
    user->ignored_users_list[i].name[0] = '\0';
  user->ign_number = 0;
  user->exam_mesg[0] = '\0';
  user->hint_at_help = hint_at_help;
  return user;
}


/*** Destruct an object. ***/
destruct_user (user)
     UR_OBJECT user;
{
/* Remove from linked list */
  if (user == user_first)
    {
      user_first = user->next;
      if (user == user_last)
	user_last = NULL;
      else
	user_first->prev = NULL;
    }
  else
    {
      user->prev->next = user->next;
      if (user == user_last)
	{
	  user_last = user->prev;
	  user_last->next = NULL;
	}
      else
	user->next->prev = user->prev;
    }
  free (user);
  destructed = 1;
}


/*** Construct room object ***/
RM_OBJECT create_room (dimension)
     int
       dimension;
{
  RM_OBJECT room;
  int i;

  if ((room = (RM_OBJECT) malloc (sizeof (struct room_struct))) == NULL)
    {
      fprintf (stderr, "GAEN: Memory allocation failure in create_room().\n");
      boot_exit (1);
    }
  room->name[0] = '\0';
  room->label[0] = '\0';
  room->desc[0] = '\0';
  room->topic[0] = '\0';
  room->access = -1;
  room->revline = 0;
  room->mesg_cnt = 0;
  room->inlink = 0;
  room->netlink = NULL;
  room->netlink_name[0] = '\0';
  room->next = NULL;
  room->hidden = 0;
  room->hidden_board = 0;
  for (i = 0; i < MAX_LINKS; ++i)
    {
      room->link_label[i][0] = '\0';
      room->link[i] = NULL;
    }
  for (i = 0; i < REVIEW_LINES; ++i)
    room->revbuff[i][0] = '\0';
  if (room_first[dimension] == NULL)
    room_first[dimension] = room;
  else
    room_last[dimension]->next = room;
  room_last[dimension] = room;
  return room;
}

/*** Construct link object ***/
NL_OBJECT create_netlink ()
{
  NL_OBJECT nl;

  if ((nl = (NL_OBJECT) malloc (sizeof (struct netlink_struct))) == NULL)
    {
      sprintf (text,
	       "NETLINK: Memory allocation error in create_netlink().\n");
      write_syslog (LOG_LINK, text, LOG_TIME);
      return NULL;
    }
  if (nl_first == NULL)
    {
      nl_first = nl;
      nl->prev = NULL;
      nl->next = NULL;
    }
  else
    {
      nl_last->next = nl;
      nl->next = NULL;
      nl->prev = nl_last;
    }
  nl_last = nl;

  nl->service[0] = '\0';
  nl->site[0] = '\0';
  nl->verification[0] = '\0';
  nl->mail_to[0] = '\0';
  nl->mail_from[0] = '\0';
  nl->mailfile = NULL;
  nl->buffer[0] = '\0';
  nl->ver_major = 0;
  nl->ver_minor = 0;
  nl->ver_patch = 0;
  nl->keepalive_cnt = 0;
  nl->last_recvd = 0;
  nl->port = 0;
  nl->socket = 0;
  nl->mesg_user = NULL;
  nl->connect_room = NULL;
  nl->type = UNCONNECTED;
  nl->connected = 0;
  nl->stage = DOWN;
  nl->lastcom = -1;
  nl->allow = ALL;
  nl->warned = 0;
  return nl;
}


/*** Destruct a netlink (usually a closed incoming one). ***/
destruct_netlink (nl)
     NL_OBJECT nl;
{
  if (nl != nl_first)
    {
      nl->prev->next = nl->next;
      if (nl != nl_last)
	nl->next->prev = nl->prev;
      else
	{
	  nl_last = nl->prev;
	  nl_last->next = NULL;
	}
    }
  else
    {
      nl_first = nl->next;
      if (nl != nl_last)
	nl_first->prev = NULL;
      else
	nl_last = NULL;
    }
  free (nl);
}


/*** Destroy all clones belonging to given user ***/
destroy_user_clones (user)
     UR_OBJECT user;
{
  UR_OBJECT u;

  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->type == CLONE_TYPE && u->owner == user)
	{
	  sprintf (text,
		   ">>>The clone of %s shimmers and vanishes like a crazy phantom...\n",
		   u->vis ? u->name : "a digit");
	  write_room (u->room, text);
	  destruct_user (u);
	}
    }
}

/************ NUTS PROTOCOL AND LINK MANAGEMENT FUNCTIONS ************/
/* Please don't alter these functions. If you do you may introduce 
   incompatabilities which may prevent other systems connecting or cause
   bugs on the remote site and yours. You may think it looks simple but
   even the newline count is important in some places. */

accept_server_connection (sock, acc_addr)
     int sock;
     struct sockaddr_in acc_addr;
{
  NL_OBJECT nl, nl2, create_netlink ();
  RM_OBJECT rm;
  char site[81];
  int d;

/* Send server type id and version number */
  sprintf (text, "NUTS %s\n", VERSION);
  write_sock (sock, text);
  strcpy (site, get_ip_address (acc_addr));
  sprintf (text, "NETLINK: Received request connection from site %s.\n",
	   site);
  write_syslog (LOG_LINK, text, LOG_TIME);

/* See if legal site, ie site is in config sites list. */
  for (nl2 = nl_first; nl2 != NULL; nl2 = nl2->next)
    if (!strcmp (nl2->site, site))
      goto OK;
  write_sock (sock, "DENIED CONNECT 1\n");
  close (sock);
  write_syslog (LOG_LINK,
		"NETLINK: Request denied, remote site not in valid sites list.\n",
		LOG_TIME);
  return;

/* Find free room link */
OK:
  for (d = 0; d < MAX_DIMENS; d++)
    for (rm = room_first[d]; rm != NULL; rm = rm->next)
      {
	if (rm->netlink == NULL && rm->inlink)
	  {
	    if ((nl = create_netlink ()) == NULL)
	      {
		write_sock (sock, "DENIED CONNECT 2\n");
		close (sock);
		write_syslog (LOG_LINK,
			      "NETLINK: Request denied, unable to create netlink object.\n",
			      LOG_TIME);
		return;
	      }
	    rm->netlink = nl;
	    nl->socket = sock;
	    nl->type = INCOMING;
	    nl->connect_room = rm;
	    nl->stage = VERIFYING;
	    nl->allow = nl2->allow;
	    nl->last_recvd = time (0);
	    strcpy (nl->service, "<verifying>");
	    strcpy (nl->site, site);
	    write_sock (sock, "GRANTED CONNECT\n");
	    write_syslog (LOG_LINK, "NETLINK: Request granted.\n", LOG_TIME);
	    return;
	  }
      }
  write_sock (sock, "DENIED CONNECT 3\n");
  close (sock);
  write_syslog (LOG_LINK, "NETLINK: Request denied, no free room links.\n",
		LOG_TIME);
}


/*** Deal with netlink data on link nl. This function could do with being
	drastically shortened but I cant be bothered to do it. ***/
exec_netcom (nl, inpstr)
     NL_OBJECT nl;
     char *inpstr;
{
  int netcom_num, lev;
  char w1[ARR_SIZE], w2[ARR_SIZE], w3[ARR_SIZE], *c, ctemp;

/* The most used commands have been truncated to save bandwidth, ie ACT is
   short for action, EMSG is short for end message. Commands that don't get
   used much ie VERIFICATION have been left long for readability. */
  char *netcom[] = {
    "DISCONNECT", "TRANS", "REL", "ACT", "GRANTED",
    "DENIED", "MSG", "EMSG", "PRM", "VERIFICATION",
    "VERIFY", "REMVD", "ERROR", "EXISTS?", "EXISTS_NO",
    "EXISTS_YES", "MAIL", "ENDMAIL", "MAILERROR", "KA",
    "RSTAT", "*"
  };

/* The buffer is large (ARR_SIZE*2) but if a bug occurs with a remote system
   and no newlines are sent for some reason it may overflow and this will 
   probably cause a crash. Oh well, such is life. */
  if (nl->buffer[0])
    {
      if (strlen (nl->buffer) > ARR_SIZE * 2 - 255)	/* to prevent the overflow of this buffer */
	nl->buffer[0] = '\0';
      strcat (nl->buffer, inpstr);
      inpstr = nl->buffer;
    }
  nl->last_recvd = time (0);

/* Go through data */
  while (*inpstr)
    {
      w1[0] = '\0';
      w2[0] = '\0';
      w3[0] = '\0';
      lev = 0;
      if (*inpstr != '\n')
	sscanf (inpstr, "%s %s %s %d", w1, w2, w3, &lev);
      /* Find first newline */
      c = inpstr;
      ctemp = 1;		/* hopefully we'll never get char 1 in the string */
      while (*c)
	{
	  if (*c == '\n')
	    {
	      ctemp = *(c + 1);
	      *(c + 1) = '\0';
	      break;
	    }
	  ++c;
	}
      /* If no newline then input is incomplete, store and return */
      if (ctemp == 1)
	{
	  if (inpstr != nl->buffer)
	    strcpy (nl->buffer, inpstr);
	  return;
	}
      /* Get command number */
      netcom_num = 0;
      while (netcom[netcom_num][0] != '*')
	{
	  if (!strcmp (netcom[netcom_num], w1))
	    break;
	  netcom_num++;
	}
      /* Deal with initial connects */
      if (nl->stage == VERIFYING)
	{
	  if (nl->type == OUTGOING)
	    {
	      if (strcmp (w1, "NUTS"))
		{
		  sprintf (text,
			   "NETLINK: Incorrect connect message from %s.\n",
			   nl->service);
		  write_syslog (LOG_LINK, text, LOG_TIME);
		  shutdown_netlink (nl);
		  return;
		}
	      /* Store remote version for compat checks */
	      nl->stage = UP;
	      w2[10] = '\0';
	      sscanf (w2, "%d.%d.%d", &nl->ver_major, &nl->ver_minor,
		      &nl->ver_patch);
	      goto NEXT_LINE;
	    }
	  else
	    {
	      /* Incoming */
	      if (netcom_num != 9)
		{
		  /* No verification, no connection */
		  sprintf (text,
			   "NETLINK: No verification sent by site %s.\n",
			   nl->site);
		  write_syslog (LOG_LINK, text, LOG_TIME);
		  shutdown_netlink (nl);
		  return;
		}
	      nl->stage = UP;
	    }
	}
      /* If remote is currently sending a message relay it to user, don't
         interpret it unless its EMSG or ERROR */
      if (nl->mesg_user != NULL && netcom_num != 7 && netcom_num != 12)
	{
	  /* If -1 then user logged off before end of mesg received */
	  if (nl->mesg_user != (UR_OBJECT) - 1)
	    write_user (nl->mesg_user, inpstr);
	  goto NEXT_LINE;
	}
      /* Same goes for mail except its ENDMAIL or ERROR */
      if (nl->mailfile != NULL && netcom_num != 17)
	{
	  fputs (inpstr, nl->mailfile);
	  goto NEXT_LINE;
	}
      nl->lastcom = netcom_num;
      switch (netcom_num)
	{
	case 0:
	  if (nl->stage == UP)
	    {
	      sprintf (text,
		       "~OLGAEN:~FY~RS Disconnecting from service %s in the %s.\n",
		       nl->service, nl->connect_room->name);
	      write_room (NULL, text);
	    }
	  shutdown_netlink (nl);
	  break;

	case 1:
	  nl_transfer (nl, w2, w3, lev, inpstr);
	  break;
	case 2:
	  nl_release (nl, w2);
	  break;
	case 3:
	  nl_action (nl, w2, inpstr);
	  break;
	case 4:
	  nl_granted (nl, w2);
	  break;
	case 5:
	  nl_denied (nl, w2, inpstr);
	  break;
	case 6:
	  nl_mesg (nl, w2);
	  break;
	case 7:
	  nl->mesg_user = NULL;
	  break;
	case 8:
	  nl_prompt (nl, w2);
	  break;
	case 9:
	  nl_verification (nl, w2, w3, 0);
	  break;
	case 10:
	  nl_verification (nl, w2, w3, 1);
	  break;
	case 11:
	  nl_removed (nl, w2);
	  break;
	case 12:
	  nl_error (nl);
	  break;
	case 13:
	  nl_checkexist (nl, w2, w3);
	  break;
	case 14:
	  nl_user_notexist (nl, w2, w3);
	  break;
	case 15:
	  nl_user_exist (nl, w2, w3);
	  break;
	case 16:
	  nl_mail (nl, w2, w3);
	  break;
	case 17:
	  nl_endmail (nl);
	  break;
	case 18:
	  nl_mailerror (nl, w2, w3);
	  break;
	case 19:		/* Keepalive signal, do nothing */
	  break;
	case 20:
	  nl_rstat (nl, w2);
	  break;
	default:
	  sprintf (text, "NETLINK: Received unknown command '%s' from %s.\n",
		   w1, nl->service);
	  write_syslog (LOG_LINK, text, LOG_TIME);
	  write_sock (nl->socket, "ERROR\n");
	}
    NEXT_LINE:
      /* See if link has closed */
      if (nl->type == UNCONNECTED)
	return;
      *(c + 1) = ctemp;
      inpstr = c + 1;
    }
  nl->buffer[0] = '\0';
}


/*** Deal with user being transfered over from remote site ***/
nl_transfer (nl, name, pass, lev, inpstr)
     NL_OBJECT nl;
     char *name, *pass, *inpstr;
     int lev;
{
  UR_OBJECT u, create_user ();
  int no_exists = 0;

/* link for outgoing users only */
  if (nl->allow == OUT)
    {
      sprintf (text, "DENIED %s 4\n", name);
      write_sock (nl->socket, text);
      return;
    }
  if (strlen (name) > USER_NAME_LEN)
    name[USER_NAME_LEN] = '\0';

/* See if user is banned */
  if (user_banned (name))
    {
      if (nl->ver_major == 3 && nl->ver_minor >= 3 && nl->ver_patch >= 3)
	sprintf (text, "DENIED %s 9\n", name);	/* new error for 3.3.3 */
      else
	sprintf (text, "DENIED %s 6\n", name);	/* old error to old versions */
      write_sock (nl->socket, text);
      return;
    }

/* See if user already on here */
  if ((u = get_exact_user (name)) != NULL)
    {
      sprintf (text, "DENIED %s 5\n", name);
      write_sock (nl->socket, text);
      return;
    }

/* See if user of this name exists on this system by trying to load up
   datafile */
  if ((u = create_user ()) == NULL)
    {
      sprintf (text, "DENIED %s 6\n", name);
      write_sock (nl->socket, text);
      return;
    }
  u->type = REMOTE_TYPE;
  strcpy (u->name, name);
  strcpy (u->savename, name);
  if (load_user_details (u))
    {				/* user exists */
      if (strcmp (u->pass, pass))
	{
	  /* Incorrect password sent */
	  sprintf (text, "DENIED %s 7\n", name);
	  write_sock (nl->socket, text);
	  destruct_user (u);
	  destructed = 0;
	  return;
	}
    }
  else
    {				/* user no exists */
      no_exists = 1;
      /* Get the users description */
      if (nl->ver_major <= 3 && nl->ver_minor <= 3 && nl->ver_patch < 1)
	strcpy (text, remove_first (remove_first (remove_first (inpstr))));
      else
	strcpy (text,
		remove_first (remove_first
			      (remove_first (remove_first (inpstr)))));
      text[USER_DESC_LEN] = '\0';
      terminate (text);
      strcpy (u->desc, text);
      strcpy (u->in_phrase, "enters");
      strcpy (u->out_phrase, "goes");
      if (nl->ver_major == 3 && nl->ver_minor >= 3 && nl->ver_patch >= 1)
	{
	  if (lev > rem_user_maxlevel)
	    u->level = rem_user_maxlevel;
	  else
	    u->level = lev;
	}
      else
	u->level = rem_user_deflevel;
    }
  if (no_exists)		/* if user hasn't a local account */
    {
      u->level = rem_user_deflevel;
      u->slevel = u->level;	/* real level and subst level are same */
    }
/* See if users level is below minlogin level */
  if (u->level < minlogin_level)
    {
      if (nl->ver_major == 3 && nl->ver_minor >= 3 && nl->ver_patch >= 3)
	sprintf (text, "DENIED %s 8\n", u->savename);	/* new error for 3.3.3 */
      else
	sprintf (text, "DENIED %s 6\n", u->savename);	/* old error to old versions */
      write_sock (nl->socket, text);
      destruct_user (u);
      destructed = 0;
      return;
    }
  strcpy (u->site, nl->service);
  sprintf (text, "~OL[~FG>~RS~OL] %s %s~RS~OL enters from cyberspace (%s).\n",
	   u->name, strrepl (u->desc, "{name}", "somebody"), nl->service);
  write_room (nl->connect_room, text);
  sprintf (text,
	   "NETLINK: Remote user %s of level %d (%s local account) received from %s.\n",
	   u->savename, u->level, no_exists ? "no" : "with", nl->service);
  write_syslog (LOG_LINK, text, LOG_TIME);
  u->room = nl->connect_room;
  u->netlink = nl;
  u->read_mail = time (0);
  u->last_login = time (0);
  num_of_users++;
  sprintf (text, "GRANTED %s\n", name);
  write_sock (nl->socket, text);
}

/*** User is leaving this system ***/
nl_release (nl, name)
     NL_OBJECT nl;
     char *name;
{
  UR_OBJECT u;

  if ((u = get_exact_user (name)) != NULL && u->type == REMOTE_TYPE)
    {
      sprintf (text,
	       "~OL[~FR<~RS~OL]%s %s~RS~OL leaves this plain of existence.\n",
	       u->name, strrepl (u->desc, "{name}", "somebody"));
      write_room_except (u->room, text, u);
      sprintf (text, "NETLINK: Remote user %s released.\n", u->savename);
      write_syslog (LOG_LINK, text, LOG_TIME);
      destroy_user_clones (u);
      destruct_user (u);
      num_of_users--;
      return;
    }
  sprintf (text,
	   "NETLINK: Release requested for unknown/invalid user %s from %s.\n",
	   name, nl->service);
  write_syslog (LOG_LINK, text, LOG_TIME);
}


/*** Remote user performs an action on this system ***/
nl_action (nl, name, inpstr)
     NL_OBJECT nl;
     char *name, *inpstr;
{
  UR_OBJECT u;
  char *c, ctemp;

  if (!(u = get_exact_user (name)))
    {
      sprintf (text, "DENIED %s 8\n", name);
      write_sock (nl->socket, text);
      return;
    }
  if (u->socket != -1)
    {
      sprintf (text, "NETLINK: Action requested for local user %s from %s.\n",
	       name, nl->service);
      write_syslog (LOG_LINK, text, LOG_TIME);
      return;
    }
  inpstr = remove_first (remove_first (inpstr));
/* remove newline character */
  c = inpstr;
  ctemp = '\0';
  while (*c)
    {
      if (*c == '\n')
	{
	  ctemp = *c;
	  *c = '\0';
	  break;
	}
      ++c;
    }
  u->last_input = time (0);
  if (u->misc_op)
    {
      if (!strcmp (inpstr, "NL"))
	misc_ops (u, "\n");
      else
	misc_ops (u, inpstr + 4);
      return;
    }
  if (u->afk)
    {
      write_user (u, ">>>You are no longer AFK.\n");
      if (u->vis)
	{
	  sprintf (text, ">>>%s comes back from being AFK.\n", u->name);
	  write_room_except (u->room, text, u);
	}
      u->afk = 0;
      event = E_NOAFK;
      strcpy (event_var, u->savename);
    }
  word_count = wordfind (u, inpstr);
  if (!strcmp (inpstr, "NL"))
    return;
  exec_com (u, inpstr);
  if (ctemp)
    *c = ctemp;
  if (!u->misc_op)
    prompt (u);
}


/*** Grant received from remote system ***/
nl_granted (nl, name)
     NL_OBJECT nl;
     char *name;
{
  UR_OBJECT u;
  RM_OBJECT old_room;

  if (!strcmp (name, "CONNECT"))
    {
      sprintf (text, "NETLINK: Connection to %s granted.\n", nl->service);
      write_syslog (LOG_LINK, text, LOG_TIME);
      /* Send our verification and version number */
      sprintf (text, "VERIFICATION %s %s\n", verification, VERSION);
      write_sock (nl->socket, text);
      return;
    }
  if (!(u = get_exact_user (name)))
    {
      sprintf (text, "NETLINK: Grant received for unknown user %s from %s.\n",
	       name, nl->service);
      write_syslog (LOG_LINK, text, LOG_TIME);
      return;
    }
/* This will probably occur if an user tried to go to the other site , got 
   lagged then changed his mind and went elsewhere. Don't worry about it. */
  if (u->remote_com != GO)
    {
      sprintf (text, "NETLINK: Unexpected grant for %s received from %s.\n",
	       name, nl->service);
      write_syslog (LOG_LINK, text, LOG_TIME);
      return;
    }
/* User has been granted permission to move into remote talker */
  write_user (u, "~FB~OL>>>You traverse cyberspace...\n");
  if (u->vis)
    {
      sprintf (text, ">>>%s %s to the %s.\n", u->name, u->out_phrase,
	       nl->service);
      write_room_except (u->room, text, u);
    }
  else
    write_room_except (u->room, invisleave, u);
  sprintf (text, "NETLINK: %s transfered to %s.\n", u->savename, nl->service);
  write_syslog (LOG_LINK, text, LOG_TIME);
  old_room = u->room;
  u->room = NULL;		/* Means on remote talker */
  u->netlink = nl;
  u->pot_netlink = NULL;
  u->remote_com = -1;
  u->misc_op = 0;
  u->filepos = 0;
  u->page_file[0] = '\0';
  reset_access (old_room);
  sprintf (text, "ACT %s look\n", u->savename);
  write_sock (nl->socket, text);
}


/*** Deny received from remote system ***/
nl_denied (nl, name, inpstr)
     NL_OBJECT nl;
     char *name, *inpstr;
{
  UR_OBJECT u, create_user ();
  int errnum;
  char *neterr[] = {
    "this site is not in the remote services valid sites list",
    "the remote service is unable to create a link",
    "the remote service has no free sky links",
    "the link is for incoming users only",
    "an user with your name is already logged on the remote site",
    "the remote service was unable to create a session for you",
    "incorrect password. Use '.go <service> <remote password>'",
    "your level there is below the remote services current minlogin level",
    "you are banned from that service (try later)"
  };

  errnum = 0;
  sscanf (remove_first (remove_first (inpstr)), "%d", &errnum);
  if (!strcmp (name, "CONNECT"))
    {
      sprintf (text, "NETLINK: Connection to %s denied, %s.\n",
	       nl->service, neterr[errnum - 1]);
      write_syslog (LOG_LINK, text, LOG_TIME);
      /* If a superuser initiated connect let them know its failed */
      sprintf (text, "~OLGAEN:~RS Connection to %s failed, %s.\n",
	       nl->service, neterr[errnum - 1]);
      write_level (SPIRIT, text, NULL);
      close (nl->socket);
      nl->type = UNCONNECTED;
      nl->stage = DOWN;
      return;
    }
/* Is for an user */
  if (!(u = get_exact_user (name)))
    {
      sprintf (text, "NETLINK: Deny for unknown user %s received from %s.\n",
	       name, nl->service);
      write_syslog (LOG_LINK, text, LOG_TIME);
      return;
    }
  sprintf (text, "NETLINK: Deny %d for user %s received from %s.\n",
	   errnum, name, nl->service);
  write_syslog (LOG_LINK, text, LOG_TIME);
  sprintf (text, ">>>Sorry, %s.\n", neterr[errnum - 1]);
  write_user (u, text);
  prompt (u);
  u->remote_com = -1;
  u->pot_netlink = NULL;
}


/*** Text received to display to an user on here ***/
nl_mesg (nl, name)
     NL_OBJECT nl;
     char *name;
{
  UR_OBJECT u;

  if (!(u = get_exact_user (name)))
    {
      sprintf (text,
	       "NETLINK: Message received for unknown user %s from %s.\n",
	       name, nl->service);
      write_syslog (LOG_LINK, text, LOG_TIME);
      nl->mesg_user = (UR_OBJECT) - 1;
      return;
    }
  nl->mesg_user = u;
}


/*** Remote system asking for prompt to be displayed ***/
nl_prompt (nl, name)
     NL_OBJECT nl;
     char *name;
{
  UR_OBJECT u;

  if (!(u = get_exact_user (name)))
    {
      sprintf (text,
	       "NETLINK: Prompt received for unknown user %s from %s.\n",
	       name, nl->service);
      write_syslog (LOG_LINK, text, LOG_TIME);
      return;
    }
  if (u->type == REMOTE_TYPE)
    {
      sprintf (text, "NETLINK: Prompt received for remote user %s from %s.\n",
	       name, nl->service);
      write_syslog (LOG_LINK, text, LOG_TIME);
      return;
    }
  prompt (u);
}


/*** Verification received from remote site ***/
nl_verification (nl, w2, w3, com)
     NL_OBJECT nl;
     char *w2, *w3;
     int com;
{
  NL_OBJECT nl2;

  if (!com)
    {
      /* We're verifiying a remote site */
      if (!w2[0])
	{
	  shutdown_netlink (nl);
	  return;
	}
      for (nl2 = nl_first; nl2 != NULL; nl2 = nl2->next)
	{
	  if (!strcmp (nl->site, nl2->site)
	      && !strcmp (w2, nl2->verification))
	    {
	      switch (nl->allow)
		{
		case IN:
		  write_sock (nl->socket, "VERIFY OK IN\n");
		  break;
		case OUT:
		  write_sock (nl->socket, "VERIFY OK OUT\n");
		  break;
		case ALL:
		  write_sock (nl->socket, "VERIFY OK ALL\n");
		}
	      strcpy (nl->service, nl2->service);
	      /* Only 3.2.0 and above send version number with verification */
	      sscanf (w3, "%d.%d.%d", &nl->ver_major, &nl->ver_minor,
		      &nl->ver_patch);
	      sprintf (text, "NETLINK: Connected to %s in the %s.\n",
		       nl->service, nl->connect_room->name);
	      write_syslog (LOG_LINK, text, LOG_TIME);
	      sprintf (text,
		       "~OLGAEN:~RS New connection to service %s in the %s.\n",
		       nl->service, nl->connect_room->name);
	      write_room (NULL, text);
	      return;
	    }
	}
      write_sock (nl->socket, "VERIFY BAD\n");
      shutdown_netlink (nl);
      return;
    }
/* The remote site has verified us */
  if (!strcmp (w2, "OK"))
    {
      /* Set link permissions */
      if (!strcmp (w3, "OUT"))
	{
	  if (nl->allow == OUT)
	    {
	      sprintf (text,
		       "NETLINK: WARNING - Permissions deadlock, both sides are outgoing only.\n");
	      write_syslog (LOG_LINK, text, LOG_TIME);
	    }
	  else
	    nl->allow = IN;
	}
      else
	{
	  if (!strcmp (w3, "IN"))
	    {
	      if (nl->allow == IN)
		{
		  sprintf (text,
			   "NETLINK: WARNING - Permissions deadlock, both sides are incoming only.\n");
		  write_syslog (LOG_LINK, text, LOG_TIME);
		}
	      else
		nl->allow = OUT;
	    }
	}
      sprintf (text, "NETLINK: Connection to %s verified.\n", nl->service);
      write_syslog (LOG_LINK, text, LOG_TIME);
      sprintf (text, "~OLGAEN:~RS New connection to service %s in the %s.\n",
	       nl->service, nl->connect_room->name);
      write_room (NULL, text);
      return;
    }
  if (!strcmp (w2, "BAD"))
    {
      sprintf (text, "NETLINK: Connection to %s has bad verification.\n",
	       nl->service);
      write_syslog (LOG_LINK, text, LOG_TIME);
      /* Let spirits know its failed , may be spirit initiated */
      sprintf (text,
	       "~OLGAEN:~RS Connection to %s failed, bad verification.\n",
	       nl->service);
      write_level (com_level[CONN], text, NULL);
      shutdown_netlink (nl);
      return;
    }
  sprintf (text, "NETLINK: Unknown verify return code from %s.\n",
	   nl->service);
  write_syslog (LOG_LINK, text, LOG_TIME);
  shutdown_netlink (nl);
}


/* Remote site only sends REMVD (removed) notification if user on remote site 
   tries to .go back to his home site or user is booted off. Home site doesn't
   bother sending reply since remote site will remove user no matter what. */
nl_removed (nl, name)
     NL_OBJECT nl;
     char *name;
{
  UR_OBJECT u;

  if (!(u = get_exact_user (name)))
    {
      sprintf (text,
	       "NETLINK: Removed notification for unknown user %s received from %s.\n",
	       name, nl->service);
      write_syslog (LOG_LINK, text, LOG_TIME);
      return;
    }
  if (u->room != NULL)
    {
      sprintf (text,
	       "NETLINK: Removed notification of local user %s received from %s.\n",
	       name, nl->service);
      write_syslog (LOG_LINK, text, LOG_TIME);
      return;
    }
  sprintf (text, "NETLINK: %s returned from %s.\n", u->savename,
	   u->netlink->service);
  write_syslog (LOG_LINK, text, LOG_TIME);
  u->room = u->netlink->connect_room;
  u->netlink = NULL;
  if (u->vis)
    {
      sprintf (text, ">>>%s %s\n", u->name, u->in_phrase);
      write_room_except (u->room, text, u);
    }
  else
    write_room_except (u->room, invisenter, u);
  look (u);
  prompt (u);
}


/*** Got an error back from site, deal with it ***/
nl_error (nl)
     NL_OBJECT nl;
{
  if (nl->mesg_user != NULL)
    nl->mesg_user = NULL;
/* lastcom value may be misleading, the talker may have sent off a whole load
   of commands before it gets a response due to lag, any one of them could
   have caused the error */
  sprintf (text, "NETLINK: Received ERROR from %s, lastcom = %d.\n",
	   nl->service, nl->lastcom);
  write_syslog (LOG_LINK, text, LOG_TIME);
}


/*** Does user exist? This is a question sent by a remote mailer to
     verifiy mail id's. ***/
nl_checkexist (nl, to, from)
     NL_OBJECT nl;
     char *to, *from;
{
  FILE *fp;
  char filename[80];

  sprintf (filename, "%s/%s.D", USERFILES, to);
  if (!(fp = fopen (filename, "r")))
    {
      sprintf (text, "EXISTS_NO %s %s\n", to, from);
      write_sock (nl->socket, text);
      return;
    }
  fclose (fp);
  sprintf (text, "EXISTS_YES %s %s\n", to, from);
  write_sock (nl->socket, text);
}


/*** Remote user doesnt exist ***/
nl_user_notexist (nl, to, from)
     NL_OBJECT nl;
     char *to, *from;
{
  UR_OBJECT user;
  char filename[80];
  char text2[ARR_SIZE];

  if ((user = get_exact_user (from)) != NULL)
    {
      sprintf (text,
	       "~OLGAEN:~RS %s does not exist at %s, your mail bounced.\n",
	       to, nl->service);
      write_user (user, text);
    }
  else
    {
      sprintf (text2, "There is no user named %s at %s, your mail bounced.\n",
	       to, nl->service);
      send_mail (NULL, from, text2);
    }
  sprintf (filename, "%s/OUT_%s_%s@%s", MAILSPOOL, from, to, nl->service);
  unlink (filename);
}


/*** Remote users exists, send him some mail ***/
nl_user_exist (nl, to, from)
     NL_OBJECT nl;
     char *to, *from;
{
  UR_OBJECT user;
  FILE *fp;
  char text2[ARR_SIZE], filename[80], line[82];

  sprintf (filename, "%s/OUT_%s_%s@%s", MAILSPOOL, from, to, nl->service);
  if (!(fp = fopen (filename, "r")))
    {
      if ((user = get_exact_user (from)) != NULL)
	{
	  sprintf (text,
		   "~OLGAEN:~RS An error occured during mail delivery to %s@%s.\n",
		   to, nl->service);
	  write_user (user, text);
	}
      else
	{
	  sprintf (text2, "An error occured during mail delivery to %s@%s.\n",
		   to, nl->service);
	  send_mail (NULL, from, text2);
	}
      return;
    }
  sprintf (text, "MAIL %s %s\n", to, from);
  write_sock (nl->socket, text);
  fgets (line, 80, fp);
  while (!feof (fp))
    {
      write_sock (nl->socket, line);
      fgets (line, 80, fp);
    }
  fclose (fp);
  write_sock (nl->socket, "\nENDMAIL\n");
  unlink (filename);
}


/*** Got some mail coming in ***/
nl_mail (nl, to, from)
     NL_OBJECT nl;
     char *to, *from;
{
  char filename[80];

  sprintf (text, "NETLINK: Mail received for %s from %s.\n", to, nl->service);
  write_syslog (LOG_LINK, text, LOG_TIME);
  sprintf (filename, "%s/IN_%s_%s@%s", MAILSPOOL, to, from, nl->service);
  if (!(nl->mailfile = fopen (filename, "w")))
    {
      sprintf (text, "ERROR: Couldn't open file %s to write in nl_mail().\n",
	       filename);
      write_syslog (LOG_ERR, text, LOG_NOTIME);
      sprintf (text, "MAILERROR %s %s\n", to, from);
      write_sock (nl->socket, text);
      return;
    }
  strcpy (nl->mail_to, to);
  strcpy (nl->mail_from, from);
}


/*** End of mail message being sent from remote site ***/
nl_endmail (nl)
     NL_OBJECT nl;
{
  FILE *infp, *outfp;
  char infile[80], mailfile[80], line[DNL + 1];
  int c;

  fclose (nl->mailfile);
  nl->mailfile = NULL;

  sprintf (mailfile, "%s/IN_%s_%s@%s", MAILSPOOL, nl->mail_to, nl->mail_from,
	   nl->service);

/* Copy to users mail file to a tempfile */
  if (!(outfp = fopen ("tempfile", "w")))
    {
      write_syslog (LOG_ERR,
		    "ERROR: Couldn't open tempfile in netlink_endmail().\n",
		    LOG_NOTIME);
      sprintf (text, "MAILERROR %s %s\n", nl->mail_to, nl->mail_from);
      write_sock (nl->socket, text);
      goto END;
    }
  fprintf (outfp, "%d\r", (int) time (0));

/* Copy old mail file to tempfile */
  sprintf (infile, "%s/%s.M", USERFILES, nl->mail_to);
  if (!(infp = fopen (infile, "r")))
    goto SKIP;
  fgets (line, DNL, infp);
  c = getc (infp);
  while (!feof (infp))
    {
      putc (c, outfp);
      c = getc (infp);
    }
  fclose (infp);

/* Copy received file */
SKIP:
  if (!(infp = fopen (mailfile, "r")))
    {
      sprintf (text,
	       "ERROR: Couldn't open file %s to read in netlink_endmail().\n",
	       mailfile);
      write_syslog (LOG_ERR, text, LOG_NOTIME);
      sprintf (text, "MAILERROR %s %s\n", nl->mail_to, nl->mail_from);
      write_sock (nl->socket, text);
      goto END;
    }
  fprintf (outfp, "From: %s@%s  %s", nl->mail_from, nl->service,
	   long_date (0));
  c = getc (infp);
  while (!feof (infp))
    {
      putc (c, outfp);
      c = getc (infp);
    }
  fclose (infp);
  fclose (outfp);
  rename ("tempfile", infile);
  if (get_exact_user (nl->mail_to) != NULL)
    write_user (get_user (nl->mail_to),
		"~EL~FT~OL~LI--->>> NEW MAIL <<<---\n");

END:
  nl->mail_to[0] = '\0';
  nl->mail_from[0] = '\0';
  unlink (mailfile);
}


/*** An error occured at remote site ***/
nl_mailerror (nl, to, from)
     NL_OBJECT nl;
     char *to, *from;
{
  UR_OBJECT user;

  if ((user = get_exact_user (from)) != NULL)
    {
      sprintf (text,
	       "~OLGAEN:~RS An error occured during mail delivery to %s@%s.\n",
	       to, nl->service);
      write_user (user, text);
    }
  else
    {
      sprintf (text, "An error occured during mail delivery to %s@%s.\n",
	       to, nl->service);
      send_mail (NULL, from, text);
    }
}


/*** Send statistics of this server to requesting one. This doesn't do 
     anything useful yet, its mearly a demonstration of the facility. ***/
nl_rstat (nl, to)
     NL_OBJECT nl;
     char *to;
{
  char str[80];

  if (gethostname (str, 80) < 0)
    strcpy (str, "localhost");
  sprintf (text, "MSG %s\n\n--->>> Remote statistics <<<---\n\n", to);
  write_sock (nl->socket, text);
  sprintf (text, "NUTS version          : %s\nHost                  : %s\n",
	   VERSION, str);
  write_sock (nl->socket, text);
  sprintf (text, "Ports (Main/St./Link) : %d ,%d, %d\n", port[0], port[1],
	   port[2]);
  write_sock (nl->socket, text);
  sprintf (text, "Number of users       : %d\nRemote user maxlevel  : %s\n",
	   num_of_users, level_name[rem_user_maxlevel]);
  write_sock (nl->socket, text);
  sprintf (text, "Remote user def. level: %s\n\nEMSG\nPRM %s\n",
	   level_name[rem_user_deflevel], to);
  write_sock (nl->socket, text);
}


/*** Shutdown the netlink and pull any remote users back home ***/
shutdown_netlink (nl)
     NL_OBJECT nl;
{
  UR_OBJECT u;
  char mailfile[80];

  if (nl->type == UNCONNECTED)
    return;

/* See if any mail halfway through being sent */
  if (nl->mail_to[0])
    {
      sprintf (text, "MAILERROR %s %s\n", nl->mail_to, nl->mail_from);
      write_sock (nl->socket, text);
      fclose (nl->mailfile);
      sprintf (mailfile, "%s/IN_%s_%s@%s", MAILSPOOL, nl->mail_to,
	       nl->mail_from, nl->service);
      unlink (mailfile);
      nl->mail_to[0] = '\0';
      nl->mail_from[0] = '\0';
    }
  write_sock (nl->socket, "DISCONNECT\n");
  close (nl->socket);
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->pot_netlink == nl)
	{
	  u->remote_com = -1;
	  continue;
	}
      if (u->netlink == nl)
	{
	  if (u->room == NULL)
	    {
	      write_user (u,
			  ">>>You feel yourself dragged back across the ether...\n");
	      u->room = u->netlink->connect_room;
	      u->netlink = NULL;
	      if (u->vis)
		{
		  sprintf (text, ">>>%s %s\n", u->name, u->in_phrase);
		  write_room_except (u->room, text, u);
		}
	      else
		write_room_except (u->room, invisenter, u);
	      look (u);
	      prompt (u);
	      sprintf (text, "NETLINK: %s recovered from %s.\n",
		       u->name, nl->service);
	      write_syslog (LOG_LINK, text, LOG_TIME);
	      continue;
	    }
	  if (u->type == REMOTE_TYPE)
	    {
	      sprintf (text, ">>>%s vanishes!\n", u->savename);
	      write_room (u->room, text);
	      destruct_user (u);
	      num_of_users--;
	    }
	}
    }
  if (nl->stage == UP)
    sprintf (text, "NETLINK: Disconnected from %s.\n", nl->service);
  else
    sprintf (text, "NETLINK: Disconnected from site %s.\n", nl->site);
  write_syslog (LOG_LINK, text, LOG_TIME);
  if (nl->type == INCOMING)
    {
      nl->connect_room->netlink = NULL;
      destruct_netlink (nl);
      return;
    }
  nl->stage = DOWN;
  nl->warned = 0;
  nl->type = UNCONNECTED;
}



/************ START OF COMMAND FUNCTIONS AND THEIR SUBSIDS ************/

/*** Deal with user input ***/
exec_com (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  int i, len;
  char filename[80], *comword = NULL;

  com_num = -1;
  if (word[0][0] == '/' && word[0][1] != '\0')
    comword = (word[0] + 1);
  else if (word[0][0] == '.')
    comword = (word[0] + 1);
  else
    comword = word[0];
  if (!comword[0])
    {
      write_user (user, "~FM~OL>>>Unknown command, yet...\n");
      return;
    }

/* get com_num (for short-cuts) */
  if (!strcmp (word[0], "`"))
    strcpy (word[0], "who");
  if (!strcmp (word[0], ">"))
    strcpy (word[0], "tell");
  if (!strcmp (word[0], "<"))
    strcpy (word[0], "pemote");
  if (!strcmp (word[0], "-"))
    strcpy (word[0], "echo");
  if (!strcmp (word[0], "!"))
    strcpy (word[0], "shout");
  if (!strcmp (word[0], "?"))
    strcpy (word[0], "help");
  if (!strcmp (word[0], "$"))
    strcpy (word[0], "macro");
  if (!strcmp (word[0], "/"))
    strcpy (word[0], "think");
  if (!strcmp (word[0], "'"))
    strcpy (word[0], "csay");
  if (!strcmp (word[0], "["))
    strcpy (word[0], "reply");
  if (!strcmp (word[0], "="))
    strcpy (word[0], "social");
  if (!strcmp (word[0], "\\"))
    strcpy (word[0], "to");
  if (inpstr[0] == ';')
    strcpy (word[0], "emote");
  else if (inpstr[0] == '#')
    strcpy (word[0], "semote");
  else
    inpstr = remove_first (inpstr);

  i = 0;
  len = strlen (comword);
  while (command[i][0] != '*')
    {
      if (!strncmp (command[i], comword, len))
	{
	  com_num = i;
	  break;
	}
      ++i;
    }
/* Fun mix-up of commands (for DILI mirror) */
  if (mirror == 3 && user->level > GUEST &&
      user->level < MAX_LEV_DILI && com_num != -1)
    {
      com_num -= (random () % DILI_OFFSET);
      switch (com_num)
	{			/* don't want to execute certain commands */
	case QUIT:
	case PRAY:
	case HOME:
	case GSHELL:
	case COMMANDS:
	case REBOOT:
	case SHUTDOWN:
	case GUESS:
	  com_num = SAY;
	}
      if (com_num < 0 || com_level[com_num] > SAINT)
	com_num = SAY;
      sprintf (text, "~OLDILI: Trying to execute '.%s'...\n",
	       command[com_num]);
      write_user (user, text);
    }
/* Searching in user aliases... */
  if (com_num == -1)
    {
      for (i = 0; i < user->aliases; i++)
	if (!strncmp (user->aliases_name[i], comword, len))
	  {
	    com_num = user->aliases_cmd[i];
	    break;
	  }
    }
  if (user->room != NULL)
    {
      if (com_num == -1)
	{
	  write_user (user,
		      "~FM~OL>>>Unknown command, yet... Try ~FG.help commands~FM to see available commands.\n");
	  return;
	}
      if (com_level[com_num] > user->level)
	{
	  write_user (user,
		      "~FM~OL>>>Unavailable command, yet... You must wait to have a higher level for that...\n");
	  return;
	}
    }
/* Check if user has restriction to execute commands */
  if (user->restrict[RESTRICT_EXEC] == restrict_string[RESTRICT_EXEC])
    {
      if (com_num != SAY || com_num != QUIT || com_num != HELP
	  || com_num != SUICIDE)
	{
	  write_user (user,
		      "~FM~OL>>>You have no right to execute that command... Sorry!\n");
	  return;
	}
    }
/* See if user has gone across a netlink and intercept certain commands to
   be run on home site */
  if (user->room == NULL)
    {
      switch (com_num)
	{
	case HOME:
	case QUIT:
	case MODE:
	case PROMPT:
	case COLOUR:
	case SUICIDE:
	case TERMINAL:
	  write_user (user, "~FY~OL---> Home execution <---\n");
	  break;

	default:
	  sprintf (text, "ACT %s %s %s\n", user->savename, word[0], inpstr);
	  write_sock (user->netlink->socket, text);
	  no_prompt = 1;
	  return;
	}
    }
/* Don't want certain commands executed by remote users */
  if (user->type == REMOTE_TYPE)
    {
      switch (com_num)
	{
	case PASSWD:
	case ENTPRO:
	case ACCREQ:
	case LOCK:
	case SETDATA:
	case SUBST:
	case DSUBST:
	case RENAME:
	case CRENAME:
	case WATCH:
	case DWATCH:
	case LWATCH:
	case RESTRICT:
	case EXECUTE:
	case ARUN:
	case DRUN:
	case UENVIRON:
	case BORN:
	case EVENT:
	case PRAY:
	case SHUTDOWN:
	case REBOOT:
	case GSHELL:
	  write_user (user,
		      "~FT~OL>>>Sorry, remote users cannot use that command.\n");
	  return;
	}
    }

/* Main switch */
  switch (com_num)
    {
    case QUIT:
      quit_user (user, inpstr);
      break;
    case LOOK:
      look (user);
      break;
    case MODE:
      toggle_mode (user);
      break;
    case SAY:
      if (word_count < 2)
	{
	  write_user (user, ">>>Say what?\n");
	  return;
	}
      say (user, inpstr);
      break;
    case SHOUT:
      shout (user, inpstr);
      break;
    case MSHOUT:
      megashout (user, inpstr);
      break;
    case TELL:
      user_tell (user, inpstr);
      break;
    case EMOTE:
      emote (user, inpstr);
      break;
    case SEMOTE:
      semote (user, inpstr);
      break;
    case PEMOTE:
      pemote (user, inpstr);
      break;
    case ECHOU:
      echo (user, inpstr);
      break;
    case GO:
      go (user);
      break;
    case IGNORE:
      ignore (user);
      break;
    case PROMPT:
      toggle_prompt (user);
      break;
    case DESC:
      set_desc (user, inpstr);
      break;
    case INPHRASE:
    case OUTPHRASE:
      set_iophrase (user, inpstr);
      break;
    case PUBCOM:
    case PRIVCOM:
      set_room_access (user);
      break;
    case LETMEIN:
      letmein (user);
      break;
    case INVITE:
      invite (user);
      break;
    case TOPIC:
      set_topic (user, inpstr);
      break;
    case MOVE:
      move (user);
      break;
    case BCAST:
      bcast (user, inpstr);
      break;
    case WHO:
      who (user, LIST_WHO);
      break;
    case PEOPLE:
      who (user, LIST_PEOPLE);
      break;
    case HOME:
      home (user);
      break;
    case SHUTDOWN:
      shutdown_com (user);
      break;
    case NEWS:
      sprintf (filename, "%s/%s", DATAFILES, NEWSFILE);
      switch (more (user, user->socket, filename))
	{
	case 0:
	  write_user (user, ">>>There is no news, sorry...\n");
	  break;
	case 1:
	  user->misc_op = 2;
	}
      break;
    case READ:
      read_board (user);
      break;
    case WRITE:
      write_board (user, inpstr, 0);
      break;
    case WIPE:
      wipe_board (user);
      break;
    case SEARCH:
      search_boards (user);
      break;
    case REVIEW:
      review (user);
      break;
    case HELP:
      help (user);
      break;
    case STATUS:
      status (user);
      break;
    case VER:
      version (user);
      break;
    case RMAIL:
      rmail (user);
      break;
    case SMAIL:
      smail (user, inpstr, 0);
      break;
    case DMAIL:
      dmail (user);
      break;
    case FROM:
      mail_from (user, 0);
      break;
    case ENTPRO:
      enter_profile_com (user, inpstr);
      break;
    case EXAMINE:
      examine (user, inpstr);
      break;
    case RMST:
      rooms (user, 1);
      break;
    case RMSN:
      rooms (user, 0);
      break;
    case NETSTAT:
      netstat (user);
      break;
    case NETDATA:
      netdata (user);
      break;
    case CONN:
      connect_netlink (user);
      break;
    case DISCONN:
      disconnect_netlink (user);
      break;
    case PASSWD:
      change_pass (user);
      break;
    case KILL:
      kill_user (user);
      break;
    case PROMOTE:
      promote (user);
      break;
    case DEMOTE:
      demote (user);
      break;
    case LISTBANS:
      listbans (user);
      break;
    case BAN:
      ban (user);
      break;
    case UNBAN:
      unban (user);
      break;
    case VIS:
      visibility (user, 1);
      break;
    case INVIS:
      visibility (user, 0);
      break;
    case SITE:
      site (user);
      break;
    case WAKE:
      wake (user, inpstr);
      break;
    case STSHOUT:
      stshout (user, inpstr);
      break;
    case MUZZLE:
      muzzle (user);
      break;
    case UNMUZZLE:
      unmuzzle (user);
      break;
    case MAP:
      showmap (user);
      break;
    case LOGGING:
      logging (user);
      break;
    case MINLOGIN:
      minlogin (user);
      break;
    case SYSTEM:
      system_details (user);
      break;
    case TERMINAL:
      terminal (user);
      break;
    case CLEARLINE:
      clearline (user);
      break;
    case FIX:
      change_room_fix (user, 1);
      break;
    case UNFIX:
      change_room_fix (user, 0);
      break;
    case VIEWLOG:
      viewlog_com (user);
      break;
    case ACCREQ:
      account_request (user, inpstr);
      break;
    case REVCLR:
      revclr (user);
      break;
    case CREATE:
      create_clone (user);
      break;
    case DESTROY:
      destroy_clone (user);
      break;
    case MYCLONES:
      myclones (user);
      break;
    case ALLCLONES:
      allclones (user);
      break;
    case SWITCH:
      clone_switch (user);
      break;
    case CSAY:
      clone_say (user, inpstr);
      break;
    case CHEAR:
      clone_hear (user);
      break;
    case RSTAT:
      remote_stat (user);
      break;
    case SWBAN:
      swear_com (user);
      break;
    case AFK:
      afk (user, inpstr);
      break;
    case CLS:
      cls (user);
      break;
    case CLSALL:
      clsall (user);
      break;
    case COLOUR:
      toggle_colour (user);
      break;
    case IGNSHOUT:
      toggle_ignshout (user);
      break;
    case IGNTELL:
      toggle_igntell (user);
      break;
    case SUICIDE:
      suicide (user);
      break;
    case DELETE:
      delete_user (user, 0);
      break;
    case REBOOT:
      reboot_com (user);
      break;
    case SETDATA:		/* GAEN new commands... */
      setdata (user);
      break;
    case MACRO:
      macro (user);
      break;
    case THINK:
      think (user, inpstr);
      break;
    case PICTURE:
      picture (user);
      break;
    case FLASH:
      flash (user);
      break;
    case SUBST:
      subst (user);
      break;
    case DSUBST:
      dsubst (user);
      break;
    case SOCIAL:
      social (user);
      break;
    case RECOUNT:		/* this command was added in NUTS 3.3.3 */
      check_messages (user, 2);
      break;
    case REVTELL:
      revtell (user);
      break;
    case REALITY:
      reality (user);
      break;
    case IGNPICT:
      ignpict (user);
      break;
    case SAVE:
      savedetails (user);
      break;
    case HINT:
      hint (user, 1);
      break;
    case ALERT:
      alert (user);
      break;
    case PRAY:
      pray (user, 0);
      break;
    case LISTAFKS:
      listafks (user);
      break;
    case MODIFY:
      modify (user);
      break;
    case WATCH:
      watch (user);
      break;
    case LWATCH:
      lwatch (user);
      break;
    case DWATCH:
      dwatch (user);
      break;
    case UPDIM:
      up (user);
      break;
    case DOWNDIM:
      down (user);
      break;
    case RENAME:
      rename_user (user);
      break;
    case RESTRICT:
      restrict (user);
      break;
    case LOCK:
      lock_user (user, inpstr);
      break;
    case MIRROR:
      mirror_chat (user);
      break;
    case NOTE:
      note (user, inpstr);
      break;
    case WHISPER:
      whisper (user, inpstr);
      break;
    case GUESS:
      guess (user);
      break;
    case UMACRO:
      umacro (user);
      break;
    case LMACROS:
      lmacros (user);
      break;
    case DMACRO:
      dmacro (user);
      break;
    case ADDMACRO:
      addmacro (user, inpstr);
      break;
    case TONE:
      tone (user);
      break;
    case HIDESKY:
      hidesky (user);
      break;
    case SHOWSKY:
      showsky (user);
      break;
    case PARAM:
      parameters (user);
      break;
    case GOSSIP:
      gossip (user, inpstr);
      break;
    case REPLY:
      reply (user, inpstr);
      break;
    case EXECUTE:
      execute (user, inpstr);
      break;
    case ARUN:
      arun (user, inpstr);
      break;
    case DRUN:
      drun (user);
      break;
    case RUN:
      run (user);
      break;
    case CRENAME:
      clone_rename (user);
      break;
    case ALLOW:
      allow (user);
      break;
    case DALLOW:
      dallow (user);
      break;
    case STEREO:
      stereo_user (user);
      break;
    case LISTIO:
      list_io (user);
      break;
    case QUOTE:
      quote (user, 1);
      break;
    case ALARM:
      alarm_user (user, inpstr);
      break;
    case BORN:
      born (user);
      break;
    case IFCOM:
      if_com (user, inpstr);
      break;
    case FORCOM:
      for_com (user, inpstr);
      break;
    case BHOLE:
      blackhole (user);
      break;
    case BTELL:
      beeptell (user, inpstr);
      break;
    case LANGUAGE:
      language (user, inpstr);
      break;
    case UENVIRON:
      user_environment (user);
      break;
    case LUSERS:
      list_users (user);
      break;
    case EVENT:
      def_events (user, inpstr);
      break;
    case GSHELL:
      gshell (user, inpstr);
      break;
    case HANGMAN:
      hangman_com (user);
      break;
    case PUZZLE:
      puzzle_com (user);
      break;
    case IDENTIFY:
      identify (user);
      break;
    case PAINTBALL:
      paintball (user);
      break;
    case SING:
      sing (user, inpstr);
      break;
    case SWEEP:
      sweep (user);
      break;
    case REVPICT:
      review_pict (user);
      break;
    case ALIASES:
      aliases (user);
      break;
    case FIT:
      fit_score (user);
      break;
    case COMMANDS:
      commands_com (user);
      break;
    case TO:
      user_to (user, inpstr);
      break;
    default:
      write_user (user, ">>>Command not executed in exec_com().\n>>>Please, contact the code developer at ~OLbusaco@infoiasi.ro~RS.\n");
      write_syslog (LOG_ERR, "Warning! Command not executed in exec_com().\n", LOG_TIME);
    }
}

/*** some GAEN new commands ***/

/*** Change the level of accessibility for certain commands ***/
commands_com (user)
     UR_OBJECT user;
{
  char filename[80];
  if (word_count < 2)
    {
      write_usage (user, "scommands list/reload/erase/<command> <level>");
      return;
    }
  sprintf (filename, "%s/%s", DATAFILES, CMDSFILE);
  if (!strcmp (word[1], "?") || !strcmp (word[1], "list"))	/* list the commands file */
    {
      write_user (user, "\n\n~FB~OL--->>> Commands file <<<---\n\n");
      switch (more (user, user->socket, filename))
	{
	case 0:
	  write_user (user, ">>>Commands file is unavailable.\n");
	  return;
	case 1:
	  user->misc_op = 2;
	}
      return;
    }
  if (!strcmp (word[1], "reload"))	/* reload the commands file */
    {
      load_commands_file (user);
      return;
    }
  if (!strcmp (word[1], "erase"))	/* erase the commands file */
    {
      s_unlink (filename);
      write_user (user, ">>>Commands file erased.\n");
      sprintf (text, "~OLGAEN:~RS %s erased the commands file.\n",
	       user->savename);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      return;
    }
  /* set the new level for a command */
  if (word_count < 3)
    {
      write_usage (user, "scommands <command> <level>");
      return;
    }
  set_command_level (user, word[1], word[2], 1);
}

/*** Set access' level of a command ***/
set_command_level (user, comword, comlevel, appendfile)
     UR_OBJECT user;
     char *comword, *comlevel;
     int appendfile;
{
  FILE *fp;
  char filename[80];
  int i, com_num, len;
  com_num = -1;
  i = 0;
  len = strlen (comword);
  while (command[i][0] != '*')
    {
      if (!strncmp (comword, command[i], len))	/* found */
	{
	  com_num = i;
	  break;
	}
      ++i;
    }
  if (com_num == -1)
    {
      if (user)
	write_user (user, ">>>Unknown command, yet!\n");
      else
	printf ("\t'%s' is an unknown command.\n", comword);
      return;
    }
  switch (com_num)
    {				/* certain commands are too dangerous */
    case QUIT:
    case REBOOT:
    case SHUTDOWN:
    case GSHELL:
    case ALLOW:
    case DALLOW:
    case DELETE:
    case BORN:
    case MINLOGIN:
    case BAN:
    case UNBAN:
    case COMMANDS:
      sprintf (text, "\n>>>The access level of '%s' can not be modified.\n",
	       comword);
      if (user)
	write_user (user, text);
      else
	printf (text);
      return;
    }
  /* All maximum level commands cannot be touched */
  if (com_level[com_num] >= SGOD)
    {
      if (user)
	write_user (user,
		    "\n>>>Commands of highest level cannot be altered!\n");
      return;
    }
  if ((i = get_level (comlevel)) == -1)
    {
      com_level[com_num] = MAX_LEVELS;
      if (user)
	sprintf (text,
		 "~OLGAEN:~RS Command ~FT%s~RS is no longer available!\n",
		 command[com_num]);
      else
	sprintf (text, "\t'%s' set to be unavailable.\n", command[com_num]);
    }
  else
    {
      com_level[com_num] = i;
      if (user)
	sprintf (text,
		 "\n~OLGAEN:~RS Command ~FT%s~RS is now available to be used by users of %s~RS or greater level!\n",
		 command[com_num], level_name[i]);
      else
	sprintf (text, "\t'%s' set to '%d' level of access.\n",
		 command[com_num], i);
    }
  if (user)
    {
      if (appendfile)
	{
	  sprintf (filename, "%s/%s", DATAFILES, CMDSFILE);	/* append the file */
	  if (!(fp = fopen (filename, "a")))
	    {
	      write_syslog (LOG_ERR,
			    "Warning: Cannot append commands file in set_command_level().\n",
			    LOG_NOTIME);
	      return;
	    }
	  fprintf (fp, "%s %d\n", command[com_num], i);
	  fclose (fp);
	}
      write_room (NULL, text);
      sprintf (text, "%s change %s's level of access to %s.\n",
	       user->savename, command[com_num],
	       i == -1 ? "NONE (-1)" : level_name[i]);
    }
  else
    printf ("%s", text);
  write_syslog (LOG_CHAT, text, LOG_TIME);
}

/*** Load commands file ***/
load_commands_file (user)
     UR_OBJECT user;
{
  char filename[80];
  char comword[80], comlev[80];
  FILE *fp;

  sprintf (filename, "%s/%s", DATAFILES, CMDSFILE);
  if (user)
    write_user (user, ">>>Processing commands file... ");
  else
    printf ("Processing commands file \"%s\"... ", filename);
  if (!(fp = fopen (filename, "r")))
    {
      if (user)
	write_user (user, "~FRnot found.\n");
      else
	printf ("not found.\n");
      return;
    }
  if (user)
    write_user (user, "~FGfound.\n");
  else
    printf ("found.\n");
  /* scan the entire file... */
  fscanf (fp, "%s %s", comword, comlev);
  while (!feof (fp))
    {
      set_command_level (user, comword, comlev, 0);
      fscanf (fp, "%s %s", comword, comlev);
    }
  fclose (fp);
}

/*** GAEN Fit: Show the 'compatibility' percentage of 2 persons ***/
fit_score (user)
     UR_OBJECT user;
{
  int score1, score2;
  char name1[WORD_LEN + 1], name2[WORD_LEN + 1];

  if (word_count < 2)
    {
      write_usage (user, "fit <person> [ <another_person> ]");
      return;
    }
  strcpy (name1, word[1]);
  name1[0] = toupper (name1[0]);
  strcpy (name2, word_count > 2 ? word[2] : user->savename);
  name2[0] = toupper (name2[0]);
  if (!strcmp (name1, name2))
    {
      write_user (user,
		  ">>>Of course, two identical persons are fitting 100%...\n");
      return;
    }
  if ((score1 = person_score (name1)) == -1)
    {
      sprintf (text,
	       ">>>%s's account or personal details not found, sorry...\n",
	       name1);
      write_user (user, text);
      return;
    }
  if ((score2 = person_score (name2)) == -1)
    {
      sprintf (text,
	       ">>>%s's account or personal details not found, sorry...\n",
	       name2);
      write_user (user, text);
      return;
    }
  sprintf (text,
	   ">>>Fitting score for ~FT%s~RS and ~FT%s~RS is: ~FT~OL%5.2f%%...\n",
	   name1, name2,
	   (((score1 + score2)) % GM_FIT_SCORE) * 100.0 / GM_FIT_SCORE);
  write_user (user, text);
}

/*** Compute the personal score, using set information ***/
person_score (name)
     char *name;
{
  char filename[80], firstname[WORD_LEN + 1], lastname[WORD_LEN + 1];
  FILE *fp;
  int i, score = 0;

  sprintf (filename, "%s/%s.U", USERFILES, name);
  if (!(fp = fopen (filename, "r")))
    return (-1);
  fgets (firstname, WORD_LEN + 2, fp);
  firstname[strlen (firstname) - 1] = '\0';
  fgets (lastname, WORD_LEN + 2, fp);
  lastname[strlen (lastname) - 1] = '\0';
  fclose (fp);
  strtoupper (firstname);
  strtoupper (lastname);
  for (i = 0; i < strlen (firstname); i++)
    {
      if (isalpha (firstname[i]))
	score += firstname[i];
    }
  for (i = 0; i < strlen (lastname); i++)
    {
      if (isalpha (lastname[i]))
	score += lastname[i];
    }
  return (score);
}

/*** User aliases interface (create, list or erase aliases) ***/
aliases (user)
     UR_OBJECT user;
{
  int i, len, cmd;
  char *wcmd;

  if (word_count < 2)
    {
      write_usage (user, "aliases [<name> <command>]/erase/?");
      return;
    }
  if (word[1][0] == '?')	/* list current defined aliases */
    {
      for (i = 0; i < user->aliases; i++)
	{
	  if (!i)
	    write_user (user, "\n~FB~OL--->>> Aliases <<<---\n\n");
	  sprintf (text, "\t%s\t:\t~FT%s\n",
		   user->aliases_name[i], command[user->aliases_cmd[i]]);
	  write_user (user, text);
	}
      if (user->aliases)
	write_user (user, "\n>>>End.\n");
      else
	write_user (user, ">>>No aliases.\n");
      return;
    }
  if (!strcmp (word[1], "erase"))	/* erase all aliases */
    {
      for (i = 0; i < MAX_ALIASES; i++)
	{
	  user->aliases_name[i][0] = '\0';
	  user->aliases_cmd[i] = -1;
	}
      user->aliases = 0;
      write_user (user, ">>>Aliases erased.\n");
      return;
    }
  /* Define an alias... */
  if (word_count < 3)
    {
      write_usage (user, "alias <alias name> <command>");
      return;
    }
  if (user->aliases >= MAX_ALIASES)
    {
      write_user (user, ">>>You cannot define another alias!\n");
      return;
    }
  i = 0;
  cmd = -1;
  wcmd = word[2];
  if (word[2][0] == '.')
    wcmd++;
  len = strlen (wcmd);
  while (command[i][0] != '*')	/* search for a command... */
    {
      if (!strncmp (command[i], wcmd, len))
	{
	  cmd = i;
	  break;
	}
      ++i;
    }
  if (cmd == -1)
    {
      write_user (user, ">>>Unknown command! Cannot define an alias...\n");
      return;
    }
  if (strlen (word[1]) > ALIAS_NAME_LEN)
    word[1][ALIAS_NAME_LEN] = '\0';
  strcpy (user->aliases_name[user->aliases], word[1]);
  user->aliases_cmd[user->aliases] = cmd;
  user->aliases++;
  sprintf (text, ">>>Alias ('~FG%s~RS' <- '~FG%s~RS') defined.\n", word[1], command[cmd]);
  write_user (user, text);
}

/*** Sweep action: 
     to delete users or some users' files using multiple criteria ***/
sweep (user)
     UR_OBJECT user;
{
  int crit, i, lev = 0, tm = 0, total, count;
  UR_OBJECT u;
  DIR *dp;
  struct dirent *dir_struct;
  char filename[80], username[80];
  FILE *fp;
  char *sw_crit[] = { "level", "noinfo", "total", "last",
    "onlymail", "onlyenv", "onlysubst",
    "onlyset", "onlyprof", "onlyrun",
    "onlyrestr", "*"
  };

  if (word_count < 2)
    {
      write_usage (user, "sweep ?/<criterion>");
      return;
    }
  if (word[1][0] == '?')	/* show criteria */
    {
      write_user (user, ">>>Sweep criteria:\n");
      i = 0;
      while (sw_crit[i][0] != '*')
	{
	  sprintf (text, "\t%s\n", sw_crit[i]);
	  write_user (user, text);
	  i++;
	}
      return;
    }
  crit = -1;
  i = 0;
  total = strlen (word[1]);
  while (sw_crit[i][0] != '*')	/* try to detect criterion */
    {
      if (!strncmp (word[1], sw_crit[i], total))	/* found */
	{
	  crit = i;
	  break;
	}
      i++;
    }
  if (crit == -1)
    {
      write_user (user, ">>>Unknown criterion, yet!...\n");
      return;
    }
  if ((dp = opendir (USERFILES)) == NULL)
    {
      write_user (user, ">>>Cannot open users directory.\n");
      write_syslog (LOG_ERR, "Failed to opendir() in sweep().\n", LOG_NOTIME);
      return;
    }
  if ((u = create_user ()) == NULL)
    {
      sprintf (text, "%s: unable to create temporary user object.\n",
	       syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Unable to create temporary user object in sweep().\n",
		    LOG_NOTIME);
      closedir (dp);
      return;
    }
  total = 0;
  switch (crit)
    {
    case SW_LEVEL:
      if (word_count < 3)
	lev = GUEST;
      else if ((lev = atoi (word[2])) < 0 || lev >= user->level)	/* security feature */
	lev = GUEST;
      break;
    case SW_TOTAL:
      if (word_count < 3)
	tm = 1;
      else if ((tm = atoi (word[2])) < 0 || tm > 23)
	tm = 1;
      break;
    case SW_LAST:
      if (word_count < 3)
	tm = 365;		/* a year */
      else if ((tm = atoi (word[2])) < 0)
	tm = 365;
      break;
    }
  while ((dir_struct = readdir (dp)) != NULL)	/* scan users directory */
    {
      sprintf (filename, "%s/%s", USERFILES, dir_struct->d_name);
      if (filename[strlen (filename) - 1] != 'D')
	continue;		/* isn't a .D file */
      strcpy (username, dir_struct->d_name);
      username[strlen (username) - 2] = '\0';
      strcpy (u->savename, username);
      u->socket = -2;
      if (!load_user_details (u))
	{
	  sprintf (text, ">>>Warning! Cannot load %s's details!\n", username);
	  write_user (user, text);
	  sprintf (text, "Warning: cannot load %s's details in sweep().\n",
		   username);
	  write_syslog (LOG_ERR, text, LOG_TIME);
	  continue;
	}
      i = 0;
      switch (crit)
	{
	case SW_LEVEL:
	  if (u->level <= lev)
	    i = 1;
	  break;
	case SW_TOTAL:
	  if ((u->total_login % 86400) / 3600 <= tm
	      && u->total_login / 86400 <= 0)
	    i = 1;
	  break;
	case SW_LAST:
	  if ((time (0) - u->last_login) / 86400 > tm)
	    i = 1;
	  break;
	case SW_NOINFO:
	  if (u->level == GUEST)
	    {
	      sprintf (filename, "%s/%s.P", USERFILES, username);
	      if ((fp = fopen (filename, "r")) == NULL)	/* no profile (.P) */
		{
		  sprintf (filename, "%s/%s.U", USERFILES, username);
		  if ((fp = fopen (filename, "r")) == NULL)	/* no set (.U) */
		    i = 1;
		  else
		    fclose (fp);
		}
	      else
		fclose (fp);
	    }
	  break;
	case SW_ONLYMAIL:
	  sprintf (filename, "%s/%s.M", USERFILES, username);
	  s_unlink (filename);
	  sprintf (text, ">>>File %s swept.\n", filename);
	  write_user (user, text);
	  sprintf (text, "~OLGAEN:~RS %s SWEPT '%s' file.\n", user->savename,
		   filename);
	  write_syslog (LOG_COM, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, user);
	  break;
	case SW_ONLYENV:
	  for (count = 0; count < 10; count++)
	    {
	      sprintf (filename, "%s/%s.E%d", USERFILES, username, count);
	      s_unlink (filename);
	    }
	  sprintf (filename, "%s/%s.E", USERFILES, username);
	  s_unlink (filename);
	  sprintf (text, ">>>File %s swept (or other environment files).\n",
		   filename);
	  write_user (user, text);
	  sprintf (text,
		   "~OLGAEN:~RS %s SWEPT '%s' file (or other environment files).\n",
		   user->savename, filename);
	  write_syslog (LOG_COM, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, user);
	  break;
	case SW_ONLYSUBST:
	  sprintf (filename, "%s/%s.S", USERFILES, username);
	  s_unlink (filename);
	  sprintf (text, ">>>File %s swept.\n", filename);
	  write_user (user, text);
	  sprintf (text, "~OLGAEN:~RS %s SWEPT '%s' file.\n", user->savename,
		   filename);
	  write_syslog (LOG_COM, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, user);
	  break;
	case SW_ONLYSET:
	  sprintf (filename, "%s/%s.U", USERFILES, username);
	  s_unlink (filename);
	  sprintf (text, ">>>File %s swept.\n", filename);
	  write_user (user, text);
	  sprintf (text, "~OLGAEN:~RS %s SWEPT '%s' file.\n", user->savename,
		   filename);
	  write_syslog (LOG_COM, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, user);
	  break;
	case SW_ONLYPROF:
	  sprintf (filename, "%s/%s.P", USERFILES, username);
	  s_unlink (filename);
	  sprintf (text, ">>>File %s swept.\n", filename);
	  write_user (user, text);
	  sprintf (text, "~OLGAEN:~RS %s SWEPT '%s' file.\n", user->savename,
		   filename);
	  write_syslog (LOG_COM, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, user);
	  break;
	case SW_ONLYRUN:
	  for (count = 0; count < 10; count++)
	    {
	      sprintf (filename, "%s/%s.%d", USERFILES, username, count);
	      s_unlink (filename);
	    }
	  sprintf (filename, "%s/%s.I", USERFILES, username);
	  s_unlink (filename);
	  sprintf (filename, "%s/%s.O", USERFILES, username);
	  s_unlink (filename);
	  sprintf (filename, "%s/%s.W", USERFILES, username);
	  s_unlink (filename);
	  sprintf (filename, "%s/%s.N", USERFILES, username);
	  s_unlink (filename);
	  sprintf (text, ">>>File %s swept (or other run commands files).\n",
		   filename);
	  write_user (user, text);
	  sprintf (text,
		   "~OLGAEN:~RS %s SWEPT '%s' file (or other run commands files).\n",
		   user->savename, filename);
	  write_syslog (LOG_COM, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, user);
	  break;
	case SW_ONLYRESTR:
	  sprintf (filename, "%s/%s.R", USERFILES, username);
	  s_unlink (filename);
	  sprintf (text, ">>>File %s swept.\n", filename);
	  write_user (user, text);
	  sprintf (text, "~OLGAEN:~RS %s SWEPT '%s' file.\n", user->savename,
		   filename);
	  write_syslog (LOG_COM, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, user);
	  break;
	}			/* end switch */
      if (i)			/* found a candidate */
	{
	  delete_files (username);
	  sprintf (text, ">>>%s swept.\n", username);
	  write_user (user, text);
	  sprintf (text, "~OLGAEN:~RS %s SWEPT %s (using %s criterion).\n",
		   user->savename, username, sw_crit[crit]);
	  write_syslog (LOG_CHAT, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, user);
	  total++;
	}
    }				/* end while */
  closedir (dp);
  destruct_user (u);
  destructed = 0;
  if (total)
    {
      sprintf (text, ">>>Total of ~FM%d~RS swept users.\n", total);
      write_user (user, text);
    }
  else
    write_user (user, ">>>No swept users.\n");
}

/*** Paintball game ***/
paintball (user)
     UR_OBJECT user;
{
  int colour;
  UR_OBJECT u;
  char *colnames[] = { 
    "~FRred", "~FGgreen", "~FYyellow",
    "~FBblue", "~FMmagenta", "~FTturquiose"
  };

  if (word_count < 2)
    {
      sprintf (text, ">>>Your paintball munition: ~FT%d~RS ball%s.\n",
	       user->pballs, user->pballs == 1 ? "" : "s");
      write_user (user, text);
      return;
    }
  if (user->pballs <= 0)
    {
      write_user (user, ">>>Sorry, no other balls available...\n");
      return;
    }
  if ((u = get_user (word[1])) == NULL)
    {
      write_user (user, nosuchuser);
      return;
    }
  if (user == u)
    {
      write_user (user,
		  ">>>Trying to play to yourself is the sixteenth sign of madness or genius.\n");
      return;
    }
  if (u->afk || u->ignall)
    {
      write_user (user, ">>>That user is ignoring everyone or is AFK.\n");
      return;
    }
  colour = random () % 6;	/* 6 colours: red, green, yellow, blue, magenta, turquoise */
  user->pballs--;
  sprintf (text, "~OL>>>You throw in %s with a %s~RS~OL ball!\n",
	   u->name, colnames[colour]);
  write_user (user, text);
  sprintf (text, "~OL>>>%s throws in you with a %s~RS~OL ball!\n",
	   user->vis ? user->name : invisname, colnames[colour]);
  write_user (u, text);
  if (colour == 1)		/* a green ball */
    {
      user->pballs++;
      if (u->pballs > 0)
	u->pballs--;
      write_user (user, "~OL>>>You just are winning another ball! :-)\n");
      write_user (u, "~OL>>>You just are loosing a ball... :-(\n");
      return;
    }
  if (colour == 0)		/* a red ball */
    {
      user->pballs -= 2;
      if (user->pballs < 0)
	user->pballs = 0;
      u->pballs++;
      write_user (user, "~OL>>>You just are loosing two balls... :-(\n");
      write_user (u, "~OL>>>You just are winning another ball! :-)\n");
      return;
    }
  if (colour == 3)		/* a blue ball */
    {
      user->pballs++;
      u->pballs++;
      write_user (user, "~OL>>>Both of you are winning another ball! :-)\n");
      write_user (u, "~OL>>>Both of you are winning another ball! :-)\n");
      return;
    }
}

/*** Puzzle game interface ***/
puzzle_com (user)
     UR_OBJECT user;
{
  int i;
  char moves[] = "lrfb"; /* directions: left, right, forward, backward */

  if (word_count < 2)
    {
      write_usage (user, "puzzle ?/start/stop/status/<direction>");
      return;
    }
  if (!strcmp (word[1], "?") || !strcmp (word[1], "status"))
    {
      puzzle_status (user, "\n~BG~OL--->>> GAEN Puzzle status <<<---\n\n");
      return;
    }
  if (!strcmp (word[1], "start"))
    {
      puzzle_start (user);
      return;
    }
  if (!strcmp (word[1], "stop"))
    {
      puzzle_stop (user);
      return;
    }
  for (i = 0; i < 4; i++)
    if (moves[i] == word[1][0])
      {
	puzzle_play (user, i);
	return;
      }
  write_usage (user, "puzzle l/r/f/b");
}

/*** Puzzle swap free position on table ***/
puzzle_swap (user, oldlin, oldcol, newlin, newcol)
     UR_OBJECT user;
     int oldlin, oldcol, newlin, newcol;
{
  int aux;
  aux = user->puzzle->table[oldlin][oldcol];
  user->puzzle->table[oldlin][oldcol] = user->puzzle->table[newlin][newcol];
  user->puzzle->table[newlin][newcol] = aux;
}

/*** Puzzle status ***/
puzzle_status (user, msg)
     UR_OBJECT user;
     char *msg;
{
  int i, j, k;
  if (user->puzzle == NULL)
    {
      write_user (user, ">>>You don't play GAEN Puzzle at this moment.\n");
      return;
    }
  write_user (user, msg);
  for (i = 0; i < GM_TDIM; i++)
    {
      strcpy (text, "~BT -");
      for (k = 0; k < GM_TDIM * 3; k++)
	strcat (text, "-");
      write_user (user, text);
      write_user (user, "\n~BT |");
      for (j = 0; j < GM_TDIM; j++)
	{
	  if (user->puzzle->table[i][j] != -1)
	    sprintf (text, "~OL%2d~RS~BT|", user->puzzle->table[i][j]);
	  else
	    strcpy (text, "~OL  ~RS~BT|");
	  write_user (user, text);
	}
      write_user (user, "\n");
    }
  strcpy (text, "~BT -");
  for (k = 0; k < GM_TDIM * 3; k++)
    strcat (text, "-");
  write_user (user, text);
  write_user (user, "\n\n");
}

/*** Puzzle start a game ***/
puzzle_start (user)
     UR_OBJECT user;
{
  int i, j, c;
  if (user->puzzle != NULL)
    {
      write_user (user,
		  ">>>You must stop your current game, then use this command.\n");
      return;
    }
  user->puzzle =
    (struct puzzle_struct *) malloc (sizeof (struct puzzle_struct));
  if (user->puzzle == NULL)
    {
      write_user (user, ">>>No memory to play GAEN Puzzle, sorry...\n");
      write_syslog (LOG_ERR, "Couldn't allocate memory in puzzle_start().\n",
		    LOG_NOTIME);
      return;
    }
  for (i = 0; i < GM_TDIM; i++)	/* fill puzzle table */
    for (j = 0; j < GM_TDIM; j++)
      user->puzzle->table[i][j] = -1;	/* -1 means free */
  for (c = 1; c < GM_TDIM * GM_TDIM; c++)	/* randomly, fill with numbers */
    {
      do
	{
	  i = random () % GM_TDIM;
	  j = random () % GM_TDIM;
	}
      while (user->puzzle->table[i][j] != -1);
      user->puzzle->table[i][j] = c;
    }
  for (i = 0; i < GM_TDIM; i++)	/* get free position on table */
    for (j = 0; j < GM_TDIM; j++)
      if (user->puzzle->table[i][j] == -1)	/* the position is free */
	{
	  user->puzzle->clin = i;
	  user->puzzle->ccol = j;
	  break;
	}
  puzzle_status (user, "\n~BG~OL--->>> Starting GAEN Puzzle... <<<---\n\n");
}

/*** Puzzle stop game ***/
puzzle_stop (user)
     UR_OBJECT user;
{
  if (user->puzzle == NULL)
    {
      write_user (user, ">>>You don't play GAEN Puzzle!\n");
      return;
    }
  free (user->puzzle);
  user->puzzle = NULL;
  write_user (user, ">>>GAEN Puzzle stopped.\n");
}

/*** Puzzle play interface ***/
puzzle_play (user, dir)
     UR_OBJECT user;
     int dir;
{
  int i, j, err = 0, ok = 0;
  if (user->puzzle == NULL)
    {
      write_user (user,
		  ">>>You don't play GAEN Puzzle at this time...\n>>>Use .puzzle start to play a new game.\n");
      return;
    }
  switch (dir)
    {
    case GM_LT:
      if (user->puzzle->ccol == 0)
	err = 1;
      else
	{
	  puzzle_swap (user, user->puzzle->clin, user->puzzle->ccol,
		       user->puzzle->clin, user->puzzle->ccol - 1);
	  user->puzzle->ccol--;
	}
      break;
    case GM_RG:
      if (user->puzzle->ccol == GM_TDIM - 1)
	err = 1;
      else
	{
	  puzzle_swap (user, user->puzzle->clin, user->puzzle->ccol,
		       user->puzzle->clin, user->puzzle->ccol + 1);
	  user->puzzle->ccol++;
	}
      break;
    case GM_FD:
      if (user->puzzle->clin == 0)
	err = 1;
      else
	{
	  puzzle_swap (user, user->puzzle->clin, user->puzzle->ccol,
		       user->puzzle->clin - 1, user->puzzle->ccol);
	  user->puzzle->clin--;
	}
      break;
    case GM_BK:
      if (user->puzzle->clin == GM_TDIM - 1)
	err = 1;
      else
	{
	  puzzle_swap (user, user->puzzle->clin, user->puzzle->ccol,
		       user->puzzle->clin + 1, user->puzzle->ccol);
	  user->puzzle->clin++;
	}
    }
  if (err)
    puzzle_status (user,
		   "\n~BG~OL--->>> GAEN Puzzle - illegal move! <<<---\n\n");
  else
    {
      for (i = 0; i < GM_TDIM; i++)	/* check if game is over */
	for (j = 0; j < GM_TDIM; j++)
	  {
	    if (user->puzzle->table[i][j] == -1)
	      continue;
	    ok = user->puzzle->table[i][j] == i * GM_TDIM + j + 1;
	  }
      if (ok)
	{
	  puzzle_status (user,
			 "\n~BG~OL--->>> GAEN Puzzle - Congratulations! <<<---\n\n");
	  puzzle_stop (user);
	}
      else
	puzzle_status (user,
		       "\n~BG~OL--->>> GAEN Puzzle - current status <<<---\n\n");
    }
}

/*** Get user account, using ident daemon of remote site 
     (written by Victor Tarhon-Onu aka Mituc <mituc@ac.tuiasi.ro>) ***/
ident_request (rhost, rport, lport, accname)
     struct hostent *rhost;
     int rport, lport;
     char *accname;
{
  int sockid, i, n_msgs, nbread, partial_nbread, read_count;
  struct sockaddr_in forident;
  char *inbuf, outbuf[ID_BUFFLEN + 1], *temp, *msgs[4], mask;
  struct timeval read_timeout;
  fd_set readfds;

  if ((temp = (char *) malloc (ID_BUFFLEN + 1)) == NULL)
    return ID_NOMEM;

  if ((inbuf = (char *) malloc (ID_BUFFLEN + 1)) == NULL)
    return ID_NOMEM;

  if ((sockid = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)	/* open a socket for ident */
    return ID_CONNERR;

  forident.sin_family = rhost->h_addrtype;
  forident.sin_addr.s_addr = *((long *) rhost->h_addr_list[0]);
  forident.sin_port = htons (113);

  if ((connect (sockid, (struct sockaddr *) &forident, sizeof (forident))) !=
      0)
    {				/* ident is not running */
      close (sockid);
      return ID_NOFOUND;
    }

  /* begin interogation... */

  sprintf (outbuf, "%d,%d\n", rport, lport);
  if (write (sockid, outbuf, strlen (outbuf)) == -1)
    {
      close (sockid);
      return ID_WRITEERR;
    }

  resetbuff (inbuf);

  read_timeout.tv_sec = ID_READTIMEOUT;
  read_timeout.tv_usec = 0;
  FD_ZERO (&readfds);
  FD_SET (sockid, &readfds);

  nbread = 0;
  read_count = 0;
  partial_nbread = 0;
  while (partial_nbread < 2 && read_count < 3)
    {
      if (select (FD_SETSIZE, &readfds, NULL, NULL, &read_timeout) == -1)
	return ID_TIMEOUT;

      /* There is no need to see if FD_ISSET(sockid,&readfds) is true because
         we have only one file descriptor in readfds, and a select error will
         refere only this file descriptor (sockid).
       */

      read_count++;
      if (
	  (partial_nbread =
	   read (sockid, &inbuf[nbread], ID_BUFFLEN - nbread)) == -1)
	{
	  close (sockid);
	  return ID_CLOSED;
	}
      nbread += partial_nbread;
    }

  close (sockid);
  /* end interogation */

  temp = strtok (inbuf, ":");
  n_msgs = 0;

  /* We'll restrict the numbers of items read here to 4 just in case 
     a lasy ident daemon runs on a given host... and who can give us
     gigabytes of auth infos...
   */
  while (temp != NULL && n_msgs < 4)

    {
      if ((msgs[n_msgs] = (char *) malloc (ID_BUFFLEN + 1)) == NULL)
	return ID_NOMEM;
      strcpy (msgs[n_msgs], temp);

      /* We'll clean the return ident messages... 
         We wasted a lot of CPU time already...:-|
       */

      while (strlen (msgs[n_msgs]) > 0 && !isalnum (msgs[n_msgs][0]))
	(msgs[n_msgs])++;

      i = strlen (msgs[n_msgs]) - 1;
      while (i > 0 && !isalnum (msgs[n_msgs][i]))
	msgs[n_msgs][i--] = '\0';

      if (msgs[n_msgs] != NULL && strlen (msgs[n_msgs]) > 2)
	n_msgs++;

      temp = strtok (NULL, ":");
    }

  /* The returned value should be like: "item1 : item2 : item3" in 
     case of an error or "item1 : item2 : item3 :item4" on success.
   */

  if (n_msgs < 2)
    return ID_READERR;		/* Or this should be ID_CRAP...? */

  /* Conforming to the RFC 1413 document, the second item (n_msgs=1)
     should contain the keyword "ERROR" or "USERID"...
   */

  if (!stricmp (msgs[1], "ERROR"))
    {
      mask = 2 * (!stricmp (msgs[2], "NO-USER")) +
	4 * (!stricmp (msgs[2], "INVALID-PORT")) +
	8 * (!stricmp (msgs[2], "HIDDEN-USER")) +
	16 * (!stricmp (msgs[2], "UNKNOWN-ERROR"));

      switch (mask)
	{
	case 2:
	  return ID_NOUSER;
	  break;
	case 4:
	  return ID_INVPORT;
	  break;
	case 8:
	  return ID_HIDDENUSER;
	  break;
	case 16:
	  return ID_UNKNOWNERR;
	default:
	  return ID_CRAP;
	}
    }
  else if (stricmp (msgs[1], "USERID"))
    return ID_READERR;

  /* 
     strcmp() can be used succesfuly too, but a case
     insensitive comparation seems better to me...
   */

  /* It's ok now... all tests where succesfuly passed. */
  strcpy (accname, msgs[n_msgs - 1]);

  return ID_OK;			/* Here we go... */
}

/*** Get user's real name via mail daemon
     (written by Victor Tarhon-Onu aka Mituc <mituc@ac.tuiasi.ro>) ***/
mail_id_request (rhost, accname, email)
     struct hostent *rhost;
     char *accname, *email;
{
  struct hostent *lhost;
  struct sockaddr_in forident;
  char *inbuf, outbuf[ID_BUFFLEN + 1], localhostname[SITE_NAME_LEN + 1];
  char *temp, *msgs[10];
  int sockmail, i;
  int nbread, read_count, partial_nbread, n_msgs;
  struct timeval read_timeout;
  fd_set readfds;

  if ((inbuf = (char *) malloc (ID_BUFFLEN + 1)) == NULL)
    return ID_NOMEM;

  if ((sockmail = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)	/* open a socket for mail ident */
    return ID_CONNERR;

  forident.sin_family = rhost->h_addrtype;
  forident.sin_addr.s_addr = *((long *) rhost->h_addr_list[0]);
  forident.sin_port = htons (25);

  if ((connect (sockmail, (struct sockaddr *) &forident, sizeof (forident)))
      == -1)
    {
      /* mail daemon is not running or another error occured */
      close (sockmail);
      return ID_NOFOUND;
    }


  /* preparing to interogate mail daemon... */

  strncpy (localhostname, (char *) getenv ("HOSTNAME"), SITE_NAME_LEN);

  /* On missconfigurated systems we may have the unpleasant surprise to 
     see that the HOSTNAME variable is not set or there is no host 
     entry for the value it is set to.
   */

  if ((lhost = gethostbyname (localhostname)) == NULL)
    strcpy (localhostname, "localhost");
  /* Some problems with "helo" may occure now...!:-/ */
  else
    strncpy (localhostname, lhost->h_name, SITE_NAME_LEN);

  /* begin interogation... */

  resetbuff (inbuf);

  read_timeout.tv_sec = ID_READTIMEOUT;
  read_timeout.tv_usec = 0;
  FD_ZERO (&readfds);
  FD_SET (sockmail, &readfds);

  nbread = 0;
  read_count = 0;
  partial_nbread = 0;
  while (partial_nbread < 2 && read_count < 3)
    {
      if (select (FD_SETSIZE, &readfds, NULL, NULL, &read_timeout) == -1)
	return ID_TIMEOUT;

      /* There is no need to see if FD_ISSET(sockmail,&readfds) is true because
         we have only one file descriptor in readfds, and a select error will
         refere only this file descriptor (sockmail). This comment is for the
         select()s below, too.
       */

      read_count++;

      if (
	  (partial_nbread =
	   read (sockmail, &inbuf[nbread], ID_BUFFLEN - nbread)) == -1)
	{
	  close (sockmail);
	  return ID_CLOSED;
	}
      nbread += partial_nbread;
    }

  /* send 'questions'... */

  resetbuff (inbuf);
  resetbuff (outbuf);
  sprintf (outbuf, "helo %s\n", localhostname);

  if (write (sockmail, outbuf, strlen (outbuf)) == -1)
    {
      close (sockmail);
      return ID_WRITEERR;
    }

  read_timeout.tv_sec = ID_READTIMEOUT;
  read_timeout.tv_usec = 0;
  FD_ZERO (&readfds);
  FD_SET (sockmail, &readfds);

  nbread = 0;
  read_count = 0;
  partial_nbread = 0;
  while (partial_nbread < 2 && read_count < 3)
    {
      if (select (FD_SETSIZE, &readfds, NULL, NULL, &read_timeout) == -1)
	return ID_TIMEOUT;

      read_count++;

      if (
	  (partial_nbread =
	   read (sockmail, &inbuf[nbread], ID_BUFFLEN - nbread)) == -1)
	{
	  close (sockmail);
	  return ID_CLOSED;
	}
      nbread += partial_nbread;
    }

  resetbuff (inbuf);
  resetbuff (outbuf);
  sprintf (outbuf, "vrfy %s\n", accname);
  if (write (sockmail, outbuf, strlen (outbuf)) == -1)
    {
      close (sockmail);
      return ID_WRITEERR;
    }

  read_timeout.tv_sec = id_readtimeout;
  read_timeout.tv_usec = 0;
  FD_ZERO (&readfds);
  FD_SET (sockmail, &readfds);

  nbread = 0;
  read_count = 0;
  partial_nbread = 0;
  while (partial_nbread < 2 && read_count < 3)
    {
      if (select (FD_SETSIZE, &readfds, NULL, NULL, &read_timeout) == -1)
	return ID_TIMEOUT;

      read_count++;

      if (
	  (partial_nbread =
	   read (sockmail, &inbuf[nbread], ID_BUFFLEN - nbread)) == -1)
	{
	  close (sockmail);
	  return ID_READERR;
	}
      nbread += partial_nbread;
    }

  sprintf (outbuf, "quit\n");
  write (sockmail, outbuf, strlen (outbuf));
  /* it isn't interesting here if write fails... */

  close (sockmail);
  /* end interogation */

  temp = strtok (inbuf, " ");
  n_msgs = 0;

  /*
     We will restrict the number of items read here just like in
     ident_request(), not to 4, but 10, 'cause there are some lasy people
     over the world ( Like me, yeah !:) ) whom name can contain a lot of
     initials, father's initial(s), mother's (why not?), and another
     one bilion of names.
   */

  while (temp != NULL && n_msgs < 10)
    {
      if ((msgs[n_msgs] = (char *) malloc (ID_BUFFLEN + 1)) == NULL)
	return ID_NOMEM;
      strcpy (msgs[n_msgs], temp);

      /* We'll clean this messages right now! We wasted 
         a lot of CPU time already and this is not all...:-|
       */

      while (strlen (msgs[n_msgs]) > 0 && !isalnum (msgs[n_msgs][0]))
	(msgs[n_msgs])++;

      i = strlen (msgs[n_msgs]) - 1;

      while (i > 0 && !isalnum (msgs[n_msgs][i]))
	msgs[n_msgs][i--] = '\0';


      if (msgs[n_msgs] != NULL && strlen (msgs[n_msgs]) > 0)
	n_msgs++;

      temp = strtok (NULL, " ");
    }

  if (n_msgs < 1)
    return ID_READERR;

  i = atoi (msgs[0]);

  switch (i)
    {
    case 500:
      return ID_COMERR;
      break;
    case 550:
      return ID_UNKNOWN;
      break;
      /* I should add more values here, so the return messages will
         be mode comprehensible...
       */
    case 250:
      if (!strchr (msgs[n_msgs - 1], '@'))
	return ID_CRAP;
      strcpy (email, "");
      for (i = 1; i < n_msgs - 1; i++)
	sprintf (email, "%s %s", email, msgs[i]);
      sprintf (email, "%s <%s>", email, msgs[n_msgs - 1]);
      return ID_OK;
      break;
    default:
      return ID_CRAP;
    }
}

/*** Get host structure, using hostsfile info 
     (written by Victor Tarhon-Onu aka Mituc <mituc@ac.tuiasi.ro>) ***/
struct hostent *
fgethostbyname (hostname)
     char *hostname;
{
  FILE *fp;
  char filename[80], siteaddr[SITE_NAME_LEN + 1], sitename[SITE_NAME_LEN + 1];
  char int_h_addr_seq, *temp;
  struct hostent *host;
  int i, found = 0;

  if (!use_hostsfile)
    return NULL;
  if ((temp = (char *) malloc (SITE_NAME_LEN)) == NULL)
    return NULL;

  sprintf (filename, "%s/%s", DATAFILES, HOSTSFILE);
  if (!(fp = fopen (filename, "r")))
    return NULL;

  fscanf (fp, "%s %s", siteaddr, sitename);

  while (!feof (fp))
    {
      if (!strcmp (hostname, sitename))	/* found */
	{
	  found = 1;
	  break;
	}
      fscanf (fp, "%s %s", siteaddr, sitename);
    }
  fclose (fp);

  if (!found)
    return NULL;

  if ((host = (struct hostent *) malloc (sizeof (struct hostent))) == NULL)
    return NULL;

  if ((host->h_addr_list = (char **) malloc (2)) == NULL)
    return NULL;

  if ((host->h_addr_list[0] = (char *) malloc (10)) == NULL)
    return NULL;

  if ((host->h_name = (char *) malloc (strlen (sitename) + 1)) == NULL)
    return NULL;

  strcpy (temp, siteaddr);
  temp = (char *) strtok (temp, ".");

  i = 0;			/* get IP components */
  while (temp != NULL && strlen (temp) > 0)
    {
      int_h_addr_seq = atoi (temp);
      host->h_addr_list[0][i] = int_h_addr_seq;
      i++;
      temp = (char *) strtok (NULL, ".");
    }

  host->h_addrtype = AF_INET;
  host->h_length = strlen (host->h_addr_list[0]);
  return host;
}

/*** Fill buff with '\0' ***/
resetbuff (buff)
     char *buff;
{
  int i;
  for (i = strlen (buff) - 1; i >= 0; buff[i--] = '\0')
    ;
}

/*** Get identification info about an user 
     (mainly written by Victor Tarhon-Onu aka Mituc <mituc@ac.tuiasi.ro>) ***/
identify (user)
     UR_OBJECT user;
{
  char accname[ID_BUFFLEN + 1], email[2 * ID_BUFFLEN + 1],
    username[USER_NAME_LEN + 1];
  struct hostent *rhost;
  int remoteport, localport, line, found = 0, retval;
  UR_OBJECT u;

  if (word_count < 2)
    {
      write_usage (user, "identify <user>/<line>");
      return;
    }
  if (!isnumber (word[1]))	/* user name */
    {
      if ((u = get_user (word[1])) == NULL)
	{
	  write_user (user, nosuchuser);
	  return;
	}
      if (u->type == REMOTE_TYPE)
	{
	  write_user (user,
		      ">>>You cannot use this command to identify remote users.\n");
	  return;
	}
    }
  else				/* socket line */
    {
      line = atoi (word[1]);
      for (u = user_first; u != NULL; u = u->next)
	if (u->type != CLONE_TYPE && u->socket == line)
	  {
	    found = 1;
	    break;
	  }
      if (!found)
	{
	  write_user (user, ">>>That line is not currently active.\n");
	  return;
	}
    }
  remoteport = u->site_port;
  localport = u->port;
  if (found || strlen (u->savename) < USER_MIN_LEN)
    strcpy (username, "Login user");
  else
    strcpy (username, u->savename);
  if ((rhost = fgethostbyname (u->site)) == NULL)
    {
      if (use_hostsfile)
	{
	  sprintf (text,
		   "Warning: Host %s not found by fgethostbyname() for %s.\n",
		   u->site, username);
	  write_syslog (LOG_ERR, text, LOG_NOTIME);
	  write_user (user,
		      ">>>Warning! Cannot found user site in hostsfile...\n");
	}
      if ((rhost = gethostbyname (u->site)) == NULL)	/* try directly */
	{
	  sprintf (text,
		   ">>>Host %s not found by gethostbyname() for %s in identify().\n",
		   u->site, username);
	  write_syslog (LOG_ERR, text, LOG_NOTIME);
	  sprintf (text, "%s: %s not found.\n", syserror, u->site);
	  write_user (user, text);
	  return;
	}
    }

  retval = ident_request (rhost, remoteport, localport, accname);

  switch (retval)
    {
    case ID_OK:
      sprintf (text, "%s gets account name of %s: %s.\n", user->savename,
	       username, accname);
      write_syslog (LOG_COM, text, LOG_TIME);
      break;

    case ID_CONNERR:
      write_syslog (LOG_ERR,
		    "GAEN: Cannot open local connection in ident_request().\n",
		    LOG_TIME);
      sprintf (text,
	       ">>>Cannot open local connection error occured while getting %s's account name.\n",
	       username);
      break;

    case ID_NOFOUND:
      sprintf (text,
	       "GAEN: Ident not running error on %s while getting %s's account name.\n",
	       u->site, username);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text, ">>>Ident daemon is not running on %s.\n", u->site);
      break;

    case ID_CLOSED:
      sprintf (text,
	       "GAEN: Closed connection error to %s in ident_request().\n",
	       u->site);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text,
	       ">>>Closed connection error occured while getting %s's account name.\n",
	       username);
      break;

    case ID_READERR:
      sprintf (text,
	       "GAEN: Read error from %s in ident_request() while getting %s's account name.\n",
	       u->site, username);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text,
	       ">>>A read error occured while getting %s's account name.\n",
	       username);
      break;

    case ID_WRITEERR:
      sprintf (text,
	       "GAEN: Write error to %s in ident_request() while getting %s's account name.\n",
	       u->site, username);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text,
	       ">>>A write error occured while getting %s's account name.\n",
	       username);
      break;

    case ID_NOMEM:
      write_syslog (LOG_ERR, "GAEN: No memory error in ident_request().\n",
		    LOG_TIME);
      sprintf (text,
	       ">>>Not enough memory error occured while getting %s's account name.\n",
	       username);
      break;

    case ID_NOUSER:
      sprintf (text,
	       "GAEN: NO-USER error occured while getting %s's account name. This means that %s's ident daemon cannot (doesn't want to) associate connected sockets with users or the port pair (%d,%d) is invalid.\n",
	       username, u->site, remoteport, localport);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text,
	       ">>>NO-USER error occured while getting %s's account name.\n",
	       username);
      break;

    case ID_INVPORT:
      sprintf (text,
	       "GAEN: INVALID-PORT error occured while getting %s's account name. This means that the port pair (%d,%d) wasn't received properly by %s\'s ident daemon or it's a value out of range.\n",
	       username, remoteport, localport, u->site);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text,
	       ">>>INVALID-PORT error occured while getting %s's account name.\n",
	       username);
      break;

    case ID_UNKNOWNERR:
      sprintf (text,
	       "GAEN: UNKNOWN-ERROR error for %s's ident daemon on request (%d,%d).\n",
	       u->site, remoteport, localport);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text,
	       ">>>UNKNOWN-ERROR error occured while getting %s's account name.\n",
	       username);
      break;

    case ID_HIDDENUSER:
      sprintf (text,
	       "GAEN: HIDDEN-USER error for %s's ident daemon on request (%d,%d). This means that %s's administrator allows users to hide themselfs on ident requests.\n",
	       u->site, remoteport, localport, u->site);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text,
	       ">>>HIDDEN-USER error occured while getting %s's account name.\n",
	       username);
      break;

    case ID_CRAP:
      sprintf (text,
	       "GAEN: %s returned crap while receiving data for request (%d,%d).\n",
	       u->site, remoteport, localport);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text,
	       ">>>An undefined type error occured while getting %s's account name.\n",
	       username);
      break;

    case ID_TIMEOUT:
      sprintf (text,
	       "GAEN: Read timed out from %s while getting %s's account name.\n",
	       u->site, username);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text, ">>>Read timed out from %s's site.\n", username);
      break;
    }


  if (retval != ID_OK)		/* it's unknown, so exit */
    {
      write_user (user, text);
      return;
    }

  switch (mail_id_request (rhost, accname, email))
    {
    case ID_OK:
      sprintf (text, "%s gets e-mail address of %s: %s.\n", username,
	       username, email);
      write_syslog (LOG_COM, text, LOG_TIME);
      sprintf (text, ">>>%s's e-mail address is: ~OL%s\n", username, email);
      break;

    case ID_CONNERR:
      write_syslog (LOG_ERR,
		    "GAEN: Cannot open local connection in mail_id_request().\n",
		    LOG_TIME);
      sprintf (text, ">>>E-mail address: ~OL<%s@%s>\n", accname, u->site);
      break;

    case ID_NOFOUND:
      sprintf (text,
	       "GAEN: Mail daemon not running error on %s while getting %s's e-mail address.\n",
	       u->site, username);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text, ">>>E-mail address: ~OL<%s@%s>\n", accname, u->site);
      break;

    case ID_CLOSED:
      sprintf (text,
	       "GAEN: Connection closed error by %s in mail_id_request() while getting %s's e-mail address.\n",
	       u->site, username);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text, ">>>E-mail address: ~OL<%s@%s>\n", accname, u->site);
      break;

    case ID_READERR:
      sprintf (text,
	       "GAEN: Read error from %s in mail_id_request() while getting %s's e-mail address.\n",
	       u->site, username);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text, ">>>E-mail address: ~OL<%s@%s>\n", accname, u->site);
      break;

    case ID_WRITEERR:
      sprintf (text,
	       "GAEN: Write error to %s in mail_id_request() while getting %s's e-mail address.\n",
	       u->site, username);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text, ">>>E-mail address: ~OL<%s@%s>\n", accname, u->site);
      break;

    case ID_NOMEM:
      write_syslog (LOG_ERR, "GAEN: No memory error in mail_id_request().\n",
		    LOG_TIME);
      sprintf (text, ">>>E-mail address: ~OL<%s@%s>\n", accname, u->site);
      break;

    case ID_COMERR:
      sprintf (text,
	       "GAEN: Received invalid command error from %s's mail daemon while getting %s's e-mail address.\n",
	       u->site, username);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text, ">>>E-mail address: ~OL<%s@%s>\n", accname, u->site);
      break;

    case ID_UNKNOWN:
      sprintf (text,
	       "GAEN: Could not determine an exact e-mail address for %s. This means that %s is a mail alias to an account from another server or %s is running ip_masquerade.\n",
	       username, accname, u->site);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text, ">>>E-mail address: ~OL<%s@%s>\n", accname, u->site);
      break;

    case ID_CRAP:
      sprintf (text,
	       "GAEN: %s returned crap while getting %s's e-mail address.\n",
	       u->site, username);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text, ">>>E-mail address: ~OL<%s@%s>\n", accname, u->site);
      break;

    case ID_TIMEOUT:
      sprintf (text,
	       "GAEN: Read timed out from %s while getting %s's e-mail address.\n",
	       u->site, username);
      write_syslog (LOG_ERR, text, LOG_TIME);
      sprintf (text, ">>>E-mail address: ~OL<%s@%s>\n", accname, u->site);
      break;
    }
  write_user (user, text);
}

/*** Hangman game interface ***/
hangman_com (user)
     UR_OBJECT user;
{
  if (word_count < 2)
    {
      write_usage (user, "hangman ?/start/stop/status/<letter>");
      return;
    }
  if (!strcmp (word[1], "?") || !strcmp (word[1], "status"))
    {
      hangman_status (user, "\n~BG~OL--->>> GAEN Hangman status <<<---\n\n");
      return;
    }
  if (!strcmp (word[1], "start"))
    {
      if (word_count < 3
	  || (strcmp (word[2], "ro") && strcmp (word[2], "en")))
	{
	  write_usage (user, "hangman start ro/en\n");
	  return;
	}
      hangman_start (user, word[2]);
      return;
    }
  if (!strcmp (word[1], "stop"))
    {
      hangman_stop (user);
      return;
    }
  if (!isalpha (word[1][0]))
    {
      write_user (user, ">>>Only letters are accepted as an answer...\n");
      return;
    }
  hangman_play (user, toupper (word[1][0]));
}

/*** Hangman get a word to be guessed ***/
char *
get_hangman_word (dict)
     char *dict;
{
  static char text2[WORD_LEN + 1];
  FILE *fp;
  char filename[80], line[80];
  int rnd = random () % gm_max_words, i = 0;

  strcpy (text2, "TALKER");
  sprintf (filename, "%s/%s.%s", DICTFILES, WORDSFILE, dict);
  if ((fp = fopen (filename, "r")) == NULL)
    return text2;
  fscanf (fp, "%s", line);
  while (!feof (fp))
    {
      if (i == rnd)
	{
	  if (strlen (line) <= WORD_LEN)	/* the word's length is OK */
	    {
	      strcpy (text2, line);
	      strtoupper (text2);
	      break;
	    }
	  rnd++;		/* to get the next word... */
	}
      fscanf (fp, "%s", line);
      i++;
    }
  fclose (fp);
  return (text2);
}

/*** Hangman status ***/
hangman_status (user, msg)
     UR_OBJECT user;
     char *msg;
{
  if (user->hangman == NULL)
    {
      write_user (user, ">>>You don't play GAEN Hangman at this moment.\n");
      return;
    }
  write_user (user, msg);
  sprintf (text, ">>>Word to guess: ~OL~FY%s\n>>>Used letters : ~OL~FY%s\n\n",
	   user->hangman->current, user->hangman->letters);
  write_user (user, text);
  /* display the hangman :) */
  write_user (user, "~FT  =======\n");
  write_user (user, "~FT ==    ~\n");
  sprintf (text, "~FT%s\n", user->hangman->state > 0 ? " ==    ~OLO" : " ==");
  write_user (user, text);
  sprintf (text, "~FT%s\n", user->hangman->state > 1 ? " ==    ~OL|" : " ==");
  write_user (user, text);
  switch (user->hangman->state)
    {
    case 0:
    case 1:
    case 2:
      strcpy (text, "~FT ==");
      break;
    case 3:
    case 4:
      strcpy (text, "~FT ==    ~OL|");
      break;
    case 5:
      strcpy (text, "~FT ==   ~OL\\|");
      break;
    case 6:
    case 7:
    case 8:
      strcpy (text, "~FT ==   ~OL\\|/");
    }
  write_user (user, text);
  write_user (user, "\n");
  sprintf (text, "~FT%s\n", user->hangman->state > 3 ? " ==    ~OL|" : " ==");
  write_user (user, text);
  switch (user->hangman->state)
    {
    case 7:
      strcpy (text, "~FT ==   ~OL/");
      break;
    case 8:
      strcpy (text, "~FT ==   ~OL/ \\");
      break;
    default:
      strcpy (text, "~FT ==");
    }
  write_user (user, text);
  write_user (user, "\n~FT ==");
  write_user (user, "\n~FT=========\n\n");
}

/*** Hangman start a game ***/
hangman_start (user, dict)
     UR_OBJECT user;
     char *dict;
{
  int i;
  if (user->hangman != NULL)
    {
      write_user (user,
		  ">>>You must stop your current game, then use this command.\n");
      return;
    }
  if (
      (user->hangman =
       (struct hangman_struct *) malloc (sizeof (struct hangman_struct))) ==
      NULL)
    {
      write_user (user, ">>>No memory to play GAEN Hangman, sorry...\n");
      write_syslog (LOG_ERR, "Couldn't allocate memory in hangman_start().\n",
		    LOG_NOTIME);
      return;
    }
  user->hangman->state = 0;
  /* no used letters */
  for (i = 0; i < 26; i++)
    user->hangman->letters[i] = ' ';
  user->hangman->letters[26] = '\0';
  strcpy (user->hangman->to_guess, get_hangman_word (dict));	/* get word from dictionary */
  strcpy (user->hangman->current, user->hangman->to_guess);
  for (i = 1; i < strlen (user->hangman->to_guess) - 1; i++)	/* fill with _ */
    user->hangman->current[i] = '_';
  hangman_status (user, "\n~BG~OL--->>> Starting GAEN Hangman... <<<---\n\n");
}

/*** Hangman stop game ***/
hangman_stop (user)
     UR_OBJECT user;
{
  if (user->hangman == NULL)
    {
      write_user (user, ">>>You don't play GAEN Hangman!\n");
      return;
    }
  free (user->hangman);
  user->hangman = NULL;
  write_user (user, ">>>GAEN Hangman stopped.\n");
}

/*** Hangman play interface ***/
hangman_play (user, letter)
     UR_OBJECT user;
     char letter;
{
  int i, guessed = 0;
  if (user->hangman == NULL)
    {
      write_user (user,
		  ">>>You don't play GAEN Hangman at this time...\n>>>Use .hangman start to play a new game.\n");
      return;
    }
  if (strchr (user->hangman->letters, letter))
    {
      hangman_status (user,
		      "\n~BG~OL--->>> GAEN Hangman status - you already used this character! <<<---\n\n");
      return;
    }
  user->hangman->letters[letter - 'A'] = letter;
  for (i = 1; i < strlen (user->hangman->to_guess) - 1; i++)
    {
      if (user->hangman->to_guess[i] == letter)
	{
	  user->hangman->current[i] = letter;
	  guessed = 1;
	}
    }
  if (!guessed)
    {
      if (++user->hangman->state >= GM_MAX_STATE)
	{
	  hangman_status (user,
			  "\n~BG~OL--->>> Sorry... GAME OVER! <<<---\n\n");
	  sprintf (text, ">>>Correct word was: ~FY~OL%s\n",
		   user->hangman->to_guess);
	  write_user (user, text);
	  hangman_stop (user);
	}
      else
	hangman_status (user,
			"\n~BG~OL--->>> GAEN Hangman - wrong! <<<---\n\n");
    }
  else
    {
      if (!strcmp (user->hangman->to_guess, user->hangman->current))
	{
	  hangman_status (user,
			  "\n~BG~OL--->>> GAEN Hangman - Congratulations! <<<---\n\n");
	  hangman_stop (user);
	}
      else
	hangman_status (user,
			"\n~BG~OL--->>> GAEN Hangman - guess! <<<---\n\n");
    }
}


/*** Write a message to user; maybe useful in run commands file ***/
note (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  if (word_count < 2)
    sprintf (text, "~OLGAEN:~RS%s - This is a simple note...\n",
	     long_date (0));
  else
    {
      if (inpstr[0] == '!' && user->level > SPIRIT)
	{
	  inpstr++;
	  sprintf (text, "%s: %s\n", user->savename, inpstr);
	  write_syslog (LOG_NOTE, text, LOG_TIME);
	}
      sprintf (text, "~OLGAEN:~RS %s\n", inpstr);
    }
  write_user (user, text);
}

/*** GAEN Shell user interface ***/
gshell (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char *commands[] =
    { "?", "ls", "mv", "rm", "more", "mkdir", "rmdir", "cd", "pwd",
    "home", "chmod", "ln", "scan", "grep", "rgrep", "tail", "cp",
    "le", "who", "last", "chown", "stat", "ps", "*"
  };
  int com, i;

  if (!exec_gshell)
    {
      write_user (user,
		  ">>>Execution of gshell commands is disabled... Sorry...\n");
      return;
    }
  if (word_count < 2)
    {
      write_usage (user, "gsh ?/<command> [<parameters>]");
      return;
    }
  com = -1;
  i = 0;
  while (commands[i][0] != '*')
    {
      if (!strcmp (word[1], commands[i]))	/* found */
	{
	  com = i;
	  sprintf (text, "gsh: %s executed '%s'.\n", user->savename, inpstr);
	  write_syslog (LOG_COM, text, LOG_TIME);
	  break;
	}
      i++;
    }
  switch (com)			/* execute gsh commmands */
    {
    case GSH_HELP:		/* show available gsh commands */
      write_user (user,
		  "\n~FB~OL--->>> Available gshell commands <<<---\n\n");
      while (commands[i][0] != '*')
	{
	  sprintf (text, "\t%s\n", commands[i++]);
	  write_user (user, text);
	}
      return;
    case GSH_LS:
      gsh_ls (user);
      break;
    case GSH_MV:
      gsh_mv (user);
      break;
    case GSH_RM:
      gsh_rm (user);
      break;
    case GSH_MORE:
      gsh_more (user);
      break;
    case GSH_MKDIR:
      gsh_mkdir (user);
      break;
    case GSH_RMDIR:
      gsh_rmdir (user);
      break;
    case GSH_CD:
      gsh_cd (user);
      break;
    case GSH_PWD:
      gsh_getcwd (user);
      break;
    case GSH_HOME:
      gsh_home (user);
      break;
    case GSH_CHMOD:
      gsh_chmod (user);
      break;
    case GSH_LN:
      gsh_ln (user);
      break;
    case GSH_SCAN:
      gsh_scan (user, inpstr);
      break;
    case GSH_GREP:
      gsh_grep (user, inpstr);
      break;
    case GSH_RGREP:
      gsh_rgrep (user, inpstr);
      break;
    case GSH_TAIL:
      gsh_tail (user);
      break;
    case GSH_CP:
      gsh_cp (user);
      break;
    case GSH_LE:
      gsh_le (user, inpstr);
      break;
    case GSH_WHO:
      gsh_who (user);
      break;
    case GSH_LAST:
      gsh_last (user);
      break;
    case GSH_CHOWN:
      gsh_chown (user);
      break;
    case GSH_STAT:
      gsh_stat (user);
      break;
    case GSH_PS:
      gsh_ps (user);
      break;
    default:
      write_user (user, ">>>Unknown gsh command, yet...\n");
    }
}

/*** gsh: ps ( process status ) ***/
gsh_ps (user)
     UR_OBJECT user;
{
  int nproc, pid, ppid, procs = 0, run_procs = 0;
  struct dirent *d_str;
  DIR *dp;
  FILE *infp, *outfp;
  char psfilename[80], tmpfilename[80], comm[80], state;

  sprintf (text, "%s/ps", MISCFILES);
  strcpy (psfilename, genfilename (text));
  if ((outfp = fopen (psfilename, "w")) == NULL)
    {
      sprintf (text, "%s: Couldn't open tempfile.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR, "gsh: Couldn't open tempfile in gsh_ps().\n", LOG_NOTIME);
      return;
    }
  if ((dp = opendir (PROCFILES)) == NULL)
    {
      write_user (user, ">>>Fail to open proc directory.\n");
      write_syslog (LOG_ERR,
		    "gsh: Couldn't open proc directory in gsh_ps().\n", 
		    LOG_NOTIME);
      fclose (outfp);
      return;
    }
  fprintf (outfp,
	   "\n\n--->>> Processes status %s <<<---\n\n PID PPID STAT COMMAND\n",
	   long_date (0));
  while ((d_str = readdir (dp)) != NULL)	/* scan all proc directory */
    {
      if (!isdigit (d_str->d_name[0]))	/* not a process directory */
	continue;
      nproc = atoi (d_str->d_name);
      /* some statistics (PID, PPID, STAT, COMMAND) */
      sprintf (tmpfilename, "%s/%s/stat", PROCFILES, d_str->d_name);
      if ((infp = fopen (tmpfilename, "r")) == NULL)
	{
	  fprintf (outfp, "%4d - cannot show info\n", nproc);
	  sprintf (text, "gsh: Couldn't open %s file in gsh_ps().\n",
		   tmpfilename);
	  write_syslog (LOG_ERR, text, LOG_NOTIME);
	  continue;
	}
      /* load info from proc stat file... */
      fscanf (infp, "%d %s %c %d", &pid, comm, &state, &ppid);
      fprintf (outfp, "%4d %4d  %c   %s\n", pid, ppid, state, comm);
      if (state == 'R')
	run_procs++;
      fclose (infp);
      procs++;
    }
  closedir (dp);
  fprintf (outfp, "\n\n>>>Total of %d processes - %d running process(es).\n",
	   procs, run_procs);
  fclose (outfp);
  switch (more (user, user->socket, psfilename))
    {
    case 0:
      write_user (user, ">>>Temp file not found.\n");
      return;
    case 1:
      user->misc_op = 2;
    }
  sprintf (text, "gsh: .gsh ps info in '%s'.\n", psfilename);
  write_syslog (LOG_COM, text, LOG_TIME);
}

/*** gsh: stat ( get file statistics ) ***/
gsh_stat (user)
     UR_OBJECT user;
{
  struct stat st_str;
  struct passwd *pw_str;
  struct group *gr_str;
  int isdev = 0;
  char name[10];

  if (word_count < 3)
    {
      write_usage (user, "gsh stat <file>");
      return;
    }
  if (stat (word[2], &st_str) == -1)
    {
      write_user (user, ">>>Fail to get file status.\n");
      return;
    }
  sprintf (text, ">>>File name: ~FG\"%s\"\t", word[2]);
  write_user (user, text);
  if ((st_str.st_mode & S_IFMT) == S_IFDIR)
    write_user (user, "directory");
  else if ((st_str.st_mode & S_IFMT) == S_IFBLK)
    {
      write_user (user, "block");
      isdev = 1;
    }
  else if ((st_str.st_mode & S_IFMT) == S_IFCHR)
    {
      write_user (user, "character");
      isdev = 1;
    }
  else if ((st_str.st_mode & S_IFMT) == S_IFREG)
    write_user (user, "ordinary");
  else if ((st_str.st_mode & S_IFMT) == S_IFIFO)
    write_user (user, "FIFO");
  else
    write_user (user, "socket");
  if (isdev)
    {
      sprintf (text, "\n>>>Device number: ~FG%d, %d",
	       (int) (st_str.st_rdev >> 8) & 0377,
	       (int) st_str.st_rdev & 0377);
      write_user (user, text);
    }
  sprintf (text, "\n>>>Resides on device: ~FG%d, %d\n",
	   (int) (st_str.st_dev >> 8) & 0377, (int) st_str.st_dev & 0377);
  write_user (user, text);
  sprintf (text, ">>>I-node: ~FG%d~RS; Links: ~FG%d~RS; Size: ~FG%ld\n",
	   (int) st_str.st_ino, (int) st_str.st_nlink,
	   (long int) st_str.st_size);
  write_user (user, text);
  if (!(pw_str = getpwuid (st_str.st_uid)))
    strcpy (name, "<unknown>");
  else
    strcpy (name, pw_str->pw_name);
  sprintf (text, ">>>Owner ID: ~FG%d (%s)\n", (int) st_str.st_uid, name);
  write_user (user, text);
  if (!(gr_str = getgrgid (st_str.st_gid)))
    strcpy (name, "<unknown>");
  else
    strcpy (name, gr_str->gr_name);
  sprintf (text, ">>>Group ID: ~FG%d (%s)\n", (int) st_str.st_gid, name);
  write_user (user, text);
  write_user (user, ">>>Special permissions: ~FG");
  if ((st_str.st_mode & S_ISUID) == S_ISUID)
    write_user (user, "SUID ");
  if ((st_str.st_mode & S_ISGID) == S_ISGID)
    write_user (user, "SGID ");
  if ((st_str.st_mode & S_ISVTX) == S_ISVTX)
    write_user (user, "Sticky ");
  sprintf (text, "\n>>>Permissions: ~FG%o\n", st_str.st_mode & 0777);
  write_user (user, text);
  sprintf (text, ">>>Last access               : ~FG%s",
	   asctime (localtime (&st_str.st_atime)));
  write_user (user, text);
  sprintf (text, ">>>Last modification         : ~FG%s",
	   asctime (localtime (&st_str.st_mtime)));
  write_user (user, text);
  sprintf (text, ">>>Last status change        : ~FG%s",
	   asctime (localtime (&st_str.st_ctime)));
  write_user (user, text);
}

/*** gsh: who ( show logged on users on talker's site ) ***/
gsh_who (user)
     UR_OBJECT user;
{
#ifdef GAEN__NO_SUPPORT__
  FILE *infp, *outfp;
  struct utmp wutmp;
  struct stat tty_st;
  int wusers = 0, idle;
  char *ptr_date, whofilename[80], idle_str[6], utty[80];

  sprintf (text, "%s/who", MISCFILES);
  strcpy (whofilename, genfilename (text));
  if ((infp = fopen (utmpfile, "r")) == NULL)
    {
      write_user (user, ">>>Fail to open utmp file.\n");
      write_syslog (LOG_ERR, "gsh: Open error of utmp file in gsh_who().\n",
		    LOG_NOTIME);
      return;
    }
  if ((outfp = fopen (whofilename, "w")) == NULL)
    {
      sprintf (text, "%s: Couldn't open tempfile.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR, "gsh: Couldn't open tempfile in gsh_who().\n",
		    LOG_NOTIME);
      fclose (infp);
      return;
    }
  fprintf (outfp,
	   "\n\n--->>> Login info %s <<<---\n\nUSER        TTY        IDLE       LOGIN@            FROM\n\n",
	   long_date (0));
  while (fread (&wutmp, sizeof (wutmp), 1, infp) != 0)
    {
      if (wutmp.ut_name != NULL && wutmp.ut_type == USER_PROCESS)
	{			/* a true user process */
	  ptr_date = ctime (&wutmp.ut_time);
	  ptr_date[strlen (ptr_date) - 1] = '\0';
	  /* try to get users' idle */
	  sprintf (utty, "/dev/%s", wutmp.ut_line);
	  if (stat (utty, &tty_st))
	    idle = time (0) - tty_st.st_atime;
	  else
	    {
	      sprintf (utty, "/dev/pts/%s", wutmp.ut_line);
	      if (stat (utty, &tty_st))
		idle = time (0) - tty_st.st_atime;
	      else
		idle = -1;
	    }
	  if (idle != -1)
	    sprintf (idle_str, "%02d'", (idle % 3600) / 60);
	  else
	    strcpy (idle_str, "-");
	  fprintf (outfp, "%-10s %-12s %-3s %12s %16s\n",
		   wutmp.ut_name, wutmp.ut_line, idle_str, ptr_date + 4,
		   wutmp.ut_host != NULL ? wutmp.ut_host : "");
	  wusers++;
	}
    }
  fprintf (outfp, "\n>>>Total of %d users.\n", wusers);
  fclose (infp);
  fclose (outfp);
  if (wusers)
    {
      switch (more (user, user->socket, whofilename))
	{
	case 0:
	  write_user (user, ">>>Tempfile not found.\n");
	  return;
	case 1:
	  user->misc_op = 2;
	}
      sprintf (text, "gsh: .gsh who info in '%s'.\n", whofilename);
      write_syslog (LOG_COM, text, LOG_TIME);
    }
  else
    write_user (user, ">>>No one logged on.\n");
#else
  write_user (user, ">>>This command is not supported on this system, sorry...\n");
#endif      
}

/*** gsh: last ( show last logged on users on talker's site ) ***/
gsh_last (user)
     UR_OBJECT user;
{
#ifdef GAEN__NO_SUPPORT__
  FILE *infp, *outfp;
  struct utmp wwtmp;
  int last, wusers = 0;
  char *ptr_date, lastfilename[80];

  if (word_count < 3)
    last = 10;
  else if ((last = atoi (word[2])) < 0)
    {
      write_usage (user, "gsh last [<entries>]");
      return;
    }
  last %= GSH_MAX_LAST;
  sprintf (text, "%s/last", MISCFILES);
  strcpy (lastfilename, genfilename (text));
  if ((infp = fopen (wtmpfile, "r")) == NULL)
    {
      write_user (user, ">>>Fail to open wtmp file.\n");
      write_syslog (LOG_ERR, "gsh: Open error of wtmp file in gsh_who().\n",
		    LOG_NOTIME);
      return;
    }
  if ((outfp = fopen (lastfilename, "w")) == NULL)
    {
      sprintf (text, "%s: Couldn't open tempfile.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR, "gsh: Couldn't open tempfile in gsh_who().\n",
		    LOG_NOTIME);
      fclose (infp);
      return;
    }
  fprintf (outfp, "\n\n--->>> Login info %s <<<---\n\n", long_date (0));
  while (fread (&wwtmp, sizeof (wwtmp), 1, infp) != 0)
    {
      if (wusers >= last)
	break;
      if (wwtmp.ut_name != NULL && wwtmp.ut_type == USER_PROCESS)
	{			/* a true user process */
	  ptr_date = ctime (&wwtmp.ut_time);
	  ptr_date[strlen (ptr_date) - 1] = '\0';
	  fprintf (outfp, "%-10s %-12s %12s %16s\n",
		   wwtmp.ut_name, wwtmp.ut_line, ptr_date + 4,
		   wwtmp.ut_host != NULL ? wwtmp.ut_host : "");
	  wusers++;
	}
    }
  fprintf (outfp, "\n>>>Total of %d entries.\n", wusers);
  fclose (infp);
  fclose (outfp);
  if (wusers)
    {
      switch (more (user, user->socket, lastfilename))
	{
	case 0:
	  write_user (user, ">>>Tempfile not found.\n");
	  return;
	case 1:
	  user->misc_op = 2;
	}
      sprintf (text, "gsh: .gsh last info in '%s'.\n", lastfilename);
      write_syslog (LOG_COM, text, LOG_TIME);
    }
  else
    write_user (user, ">>>No entries found.\n");
#else
  write_user (user, ">>>This command is not supported on this system, sorry...\n");
#endif          
}

/*** gsh: le ( line editor ) ***/
gsh_le (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  FILE *infp, *outfp;
  char line[255];
  int i, l;

  if (word_count < 5)
    {
      write_usage (user, "gsh le <file> add/del/ins <text>");
      return;
    }
  if (!strcmp (word[3], "add"))	/* add a new line to file */
    {
      if ((infp = fopen (word[2], "a")) == NULL)
	{
	  write_user (user, ">>>Fail to append file.\n");
	  return;
	}
      fprintf (infp, "%s\n",
	       remove_first (remove_first (remove_first (inpstr))));
      fclose (infp);
      write_user (user, ">>>Line added.\n");
      return;
    }
  if (!strcmp (word[3], "del"))	/* delete a line from file */
    {
      if ((l = atoi (word[4])) <= 0)
	{
	  write_usage (user, "gsh le del <#line>\n");
	  return;
	}
      if ((infp = fopen (word[2], "r")) == NULL)
	{
	  write_user (user, ">>>Fail to open file.\n");
	  return;
	}
      if ((outfp = fopen ("tempfile", "w")) == NULL)
	{
	  sprintf (text, "%s: Couldn't open tempfile.\n", syserror);
	  write_user (user, text);
	  write_syslog (LOG_ERR, "gsh: Couldn't open tempfile in gsh_le().\n",
			LOG_NOTIME);
	  fclose (infp);
	  return;
	}
      i = 0;
      fgets (line, 255, infp);
      while (!feof (infp))
	{
	  ++i;
	  if (i != l)
	    fputs (line, outfp);
	  fgets (line, 255, infp);
	}
      fclose (infp);
      fclose (outfp);
      s_unlink (word[2]);
      rename ("tempfile", word[2]);
      write_user (user, ">>>Line deleted.\n");
      return;
    }
  if (!strcmp (word[3], "ins"))	/* insert a line into file */
    {
      if ((l = atoi (word[4])) <= 0)
	{
	  write_usage (user, "gsh le ins <#line> [ <text> ]\n");
	  return;
	}
      if (word_count > 5)
	inpstr =
	  remove_first (remove_first (remove_first (remove_first (inpstr))));
      else
	strcpy (inpstr, "");
      strcat (inpstr, "\n");
      if ((infp = fopen (word[2], "r")) == NULL)
	{
	  write_user (user, ">>>Fail to open file.\n");
	  return;
	}
      if ((outfp = fopen ("tempfile", "w")) == NULL)
	{
	  sprintf (text, "%s: Couldn't open tempfile.\n", syserror);
	  write_user (user, text);
	  write_syslog (LOG_ERR, "gsh: Couldn't open tempfile in gsh_le().\n",
			LOG_NOTIME);
	  fclose (infp);
	  return;
	}
      i = 0;
      fgets (line, 255, infp);
      while (!feof (infp))
	{
	  ++i;
	  if (i == l)
	    fputs (inpstr, outfp);
	  fputs (line, outfp);
	  fgets (line, 255, infp);
	}
      fclose (infp);
      fclose (outfp);
      s_unlink (word[2]);
      rename ("tempfile", word[2]);
      write_user (user, ">>>Line inserted.\n");
      return;
    }
  write_user (user, ">>>Unknown option, yet...\n");
}

/*** gsh: cp ( copy a file ) ***/
gsh_cp (user)
     UR_OBJECT user;
{
  int fromfd, tofd, nread, nwrite, n;
  char buf[GSH_BUFSIZE];

  if (word_count < 4)
    {
      write_usage (user, "gsh cp <source> <destination>");
      return;
    }
  if ((fromfd = open (word[2], O_RDONLY)) == -1)
    {
      write_user (user, ">>>Fail to open source file.\n");
      return;
    }
  if ((tofd = creat (word[3], 0666)) == -1)
    {
      close (fromfd);
      write_user (user, ">>>Fail to create destination file.\n");
      return;
    }
  while ((nread = read (fromfd, buf, sizeof (buf))) != 0)
    {
      if (nread == -1)
	{
	  close (fromfd);
	  close (tofd);
	  write_user (user, ">>>Reading error.\n");
	  return;
	}
      nwrite = 0;
      do
	{
	  if ((n = write (tofd, &buf[nwrite], nread - nwrite)) == -1)
	    {
	      close (fromfd);
	      close (tofd);
	      write_user (user, ">>>Writing error.\n");
	      return;
	    }
	  nwrite += n;
	}
      while (nwrite < nread);
    }
  close (fromfd);
  close (tofd);
  write_user (user, ">>>File copied.\n");
}

/*** gsh: grep ( list file's lines which contain a pattern ) ***/
gsh_grep (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  FILE *infp, *outfp;
  int lines = 0;
  char line[ARR_SIZE], line_tmp[ARR_SIZE], str[ARR_SIZE];

  if (word_count < 4)
    {
      write_usage (user, "gsh grep <file> <pattern>");
      return;
    }
  if (!(infp = fopen (word[2], "r")))
    {
      write_user (user, ">>>Fail to open file.\n");
      return;
    }
  if (!(outfp = fopen ("tempfile", "w")))
    {
      fclose (infp);
      sprintf (text, "%s: Fail to open tempfile.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR, "gsh: Couldn't open tempfile in gsh_grep().\n",
		    LOG_NOTIME);
      return;
    }
  strcpy (str, remove_first (remove_first (inpstr)));
  strtolower (str);
  fgets (line, ARR_SIZE, infp);
  while (!feof (infp))
    {
      line[strlen (line) - 1] = '\0';
      strcpy (line_tmp, line);
      strtolower (line);
      if (strstr (line, str))
	{
	  fprintf (outfp, "%s\n", line_tmp);
	  lines++;
	}
      fgets (line, ARR_SIZE, infp);
    }
  fclose (infp);
  fclose (outfp);
  if (lines)
    {
      sprintf (text, "\n>>>Total of ~FT%d~RS lines found.\n", lines);
      write_user (user, text);
      switch (more (user, user->socket, "tempfile"))
	{
	case 0:
	  write_user (user, ">>>Tempfile not found.\n");
	  return;
	case 1:
	  user->misc_op = 2;
	}
    }
  else
    write_user (user, ">>>No lines found.\n");
}

/*** gsh: rgrep ( list file's lines which not contain a pattern ) ***/
gsh_rgrep (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  FILE *infp, *outfp;
  int lines = 0;
  char line[ARR_SIZE], line_tmp[ARR_SIZE], str[ARR_SIZE];

  if (word_count < 4)
    {
      write_usage (user, "gsh rgrep <file> <pattern>");
      return;
    }
  if (!(infp = fopen (word[2], "r")))
    {
      write_user (user, ">>>Fail to open file.\n");
      return;
    }
  if (!(outfp = fopen ("tempfile", "w")))
    {
      fclose (infp);
      sprintf (text, "%s: Fail to open tempfile.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR, "gsh: Couldn't open tempfile in gsh_grep().\n",
		    LOG_NOTIME);
      return;
    }
  strcpy (str, remove_first (remove_first (inpstr)));
  strtolower (str);
  fgets (line, ARR_SIZE, infp);
  while (!feof (infp))
    {
      line[strlen (line) - 1] = '\0';
      strcpy (line_tmp, line);
      strtolower (line);
      if (!strstr (line, str))
	{
	  fprintf (outfp, "%s\n", line_tmp);
	  lines++;
	}
      fgets (line, ARR_SIZE, infp);
    }
  fclose (infp);
  fclose (outfp);
  if (lines)
    {
      sprintf (text, "\n>>>Total of ~FT%d~RS lines found.\n", lines);
      write_user (user, text);
      switch (more (user, user->socket, "tempfile"))
	{
	case 0:
	  write_user (user, ">>>Tempfile not found.\n");
	  return;
	case 1:
	  user->misc_op = 2;
	}
    }
  else
    write_user (user, ">>>No lines found.\n");
}

/*** gsh: tail ( get a file's tail ) ***/
gsh_tail (user)
     UR_OBJECT user;
{
  FILE *fp;
  char c;
  int lines, cnt, cnt2;

  if (word_count < 4 || (lines = atoi (word[2])) < 1)
    {
      write_usage (user, "gsh tail <lines> <file>");
      return;
    }
  /* count total lines */
  if (!(fp = fopen (word[3], "r")))
    {
      write_user (user, ">>>File not found.\n");
      return;
    }
  cnt = 0;
  c = getc (fp);
  while (!feof (fp))
    {
      if (c == '\n')
	++cnt;
      c = getc (fp);
    }
  if (cnt < lines)
    {
      sprintf (text, ">>>There are only ~FT%d~RS lines in the file.\n", cnt);
      write_user (user, text);
      fclose (fp);
      return;
    }
  if (cnt == lines)		/* gsh tail == gsh more :) */
    {
      fclose (fp);
      switch (more (user, user->socket, word[3]))
	{
	case 0:
	  write_user (user, ">>>Fail to tail.\n");
	  break;
	case 1:
	  user->misc_op = 2;
	}
      return;
    }
  /* find line to start on */
  fseek (fp, 0, 0);		/* go top */
  cnt2 = 0;
  c = getc (fp);
  while (!feof (fp))
    {
      if (c == '\n')
	++cnt2;
      c = getc (fp);
      if (cnt2 == cnt - lines)
	{
	  user->filepos = ftell (fp) - 1;
	  fclose (fp);
	  if (more (user, user->socket, word[3]) != 1)
	    user->filepos = 0;
	  else
	    user->misc_op = 2;
	  return;
	}
    }
  fclose (fp);
  sprintf (text, "%s: Line count error.\n", syserror);
  write_user (user, text);
  write_syslog (LOG_ERR, "gsh: Line count error in gsh_tail().\n", LOG_NOTIME);
}

/*** gsh: scan ( scan a directory and execute a command for each entry ) ***/
gsh_scan (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  struct dirent *d_str;
  DIR *dp;
  char save_var[WORD_LEN + 1], cmd[COMMAND_LEN + 1];

  if (word_count < 4)
    {
      write_usage (user, "gsh scan <dir> <command>");
      return;
    }
  if ((dp = opendir (word[2])) == NULL)
    {
      write_user (user, ">>>Fail to scan directory.\n");
      return;
    }
  strcpy (save_var, user->var1);
  while ((d_str = readdir (dp)) != NULL)
    {
      strcpy (user->var1, d_str->d_name);
      if (strlen (inpstr) > COMMAND_LEN)
	inpstr[COMMAND_LEN] = '\0';
      strcpy (cmd, remove_first (remove_first (inpstr)));
      clear_words ();
      word_count = wordfind (user, cmd);
      sprintf (text, "~OL>>>Scanning... %s\n", user->var1);
      write_user (user, text);
      com_num = -1;
      exec_com (user, cmd);
    }
  closedir (dp);
  strcpy (user->var1, save_var);
}

/*** gsh: ln ( link or symlink a file ) ***/
gsh_ln (user)
     UR_OBJECT user;
{
  if (word_count < 4)
    {
      write_usage (user, "gsh ln <src> <dest> [ -s ]");
      return;
    }
  if (word_count > 4 && !strcmp (word[4], "-s"))
    {
      if (symlink (word[2], word[3]))
	write_user (user, ">>>Fail to symlink file.\n");
      else
	write_user (user, ">>>Files symbolic linked.\n");
    }
  else
    {
      if (link (word[2], word[3]))
	write_user (user, ">>>Fail to link file.\n");
      else
	write_user (user, ">>>Files linked.\n");
    }
}

/*** gsh: home ( returns to talker home directory ) ***/
gsh_home (user)
     UR_OBJECT user;
{
  if (chdir (homedir))
    {
      write_user (user, ">>>Fail to return to talker's home directory.\n");
      write_syslog (LOG_ERR,
		    "gsh: Fail to return to talker's home directory.", LOG_TIME);
    }
  else
    write_user (user, ">>>We are now in the talker's home directory.\n");
}

/*** gsh: more ( displays a file ) ***/
gsh_more (user)
     UR_OBJECT user;
{
  if (word_count < 3)
    {
      write_usage (user, "gsh more <file>");
      return;
    }
  switch (more (user, user->socket, word[2]))
    {
    case 0:
      write_user (user, ">>>File not found.\n");
      return;
    case 1:
      user->misc_op = 2;
    }
}

/*** gsh: chmod ( change file mode ) ***/
gsh_chmod (user)
     UR_OBJECT user;
{
  mode_t mode = 0;
  if (word_count < 3)
    {
      write_usage (user, "gsh chmod <mode> <file>");
      return;
    }
  if (sscanf (word[2], "%o", &mode) != 1)
    {
      write_user (user, ">>>Invalid mode.\n");
      return;
    }
  if (chmod (word[3], mode))
    write_user (user, ">>>Fail to change file mode.\n");
  else
    write_user (user, ">>>File mode changed.\n");
}

/*** gsh: chown ( change file owner ) ***/
gsh_chown (user)
     UR_OBJECT user;
{
  uid_t uid;
  gid_t gid;
  if (word_count < 4)
    {
      write_usage (user, "gsh chown <uid> <gid> <file>");
      return;
    }
  if (!isnumber (word[2]) || !isnumber (word[3]))
    {
      write_user (user, ">>>Invalid UID or GID.\n");
      return;
    }
  uid = atoi (word[2]);
  gid = atoi (word[3]);
  if (chown (word[4], uid, gid))
    write_user (user, ">>>Fail to change file owner.\n");
  else
    write_user (user, ">>>File owner changed.\n");
}

/*** gsh: rm ( remove file(s) ) ***/
gsh_rm (user)
     UR_OBJECT user;
{
  int i;
  if (word_count < 3)
    {
      write_usage (user, "gsh rm <file>...");
      return;
    }
  for (i = 2; i < word_count; i++)
    {
      if (unlink (word[i]))
	write_user (user, ">>>Fail to remove file.\n");
      else
	write_user (user, ">>>File removed.\n");
    }
}

/*** gsh: mv ( move/rename file ) ***/
gsh_mv (user)
     UR_OBJECT user;
{
  if (word_count < 4)
    {
      write_usage (user, "gsh mv <src> <dest>");
      return;
    }
  if (rename (word[2], word[3]))
    write_user (user, ">>>Fail to move/rename file.\n");
  else
    write_user (user, ">>>File moved/renamed.\n");
}

/*** gsh: mkdir ( make a directory ) ***/
gsh_mkdir (user)
     UR_OBJECT user;
{
  if (word_count < 3)
    {
      write_usage (user, "gsh mkdir <dir>");
      return;
    }
  if (mkdir (word[2], 0700))
    write_user (user, ">>>Fail to make directory.\n");
  else
    write_user (user, ">>>New directory created.\n");
}

/*** gsh: rmdir ( remove a directory ) ***/
gsh_rmdir (user)
     UR_OBJECT user;
{
  if (word_count < 3)
    {
      write_usage (user, "gsh rmdir <dir>");
      return;
    }
  if (rmdir (word[2]))
    write_user (user, ">>>Fail to remove directory.\n");
  else
    write_user (user, ">>>Directory removed.\n");
}

/*** gsh: getcwd ( get current directory ) ***/
gsh_getcwd (user)
     UR_OBJECT user;
{
  char dir[SITE_NAME_LEN + 1];
  if (getcwd (dir, SITE_NAME_LEN) == NULL)
    write_user (user, ">>>Cannot get current directory.\n");
  else
    {
      sprintf (text, ">>>Current directory is \"~FT%s~RS\".\n", dir);
      write_user (user, text);
    }
}

/*** gsh: cd ( change current directory ) ***/
gsh_cd (user)
     UR_OBJECT user;
{
  if (word_count < 3)
    {
      write_usage (user, "gsh cd <dir>");
      return;
    }
  if (chdir (word[2]))
    write_user (user, ">>>Fail to change directory.\n");
  else
    write_user (user,
		">>>Directory changed. Please, don't forget to return to talker's home directory!\n");
}

/*** gsh: ls ( list entries of current directory ) ***/
gsh_ls (user)
     UR_OBJECT user;
{
  int cl = 0;
  struct dirent *d_str;
  DIR *dp;
  if (word_count < 3)
    {
      write_usage (user, "gsh ls <dir>");
      return;
    }
  if ((dp = opendir (word[2])) == NULL)
    {
      write_user (user, ">>>Fail to open directory.\n");
      return;
    }
  while ((d_str = readdir (dp)) != NULL)
    {
      sprintf (text, "%-20s", d_str->d_name);
      write_user (user, text);
      if (++cl > MAX_LS_COLS)
	{
	  write_user (user, "\n");
	  cl = 0;
	}
    }
  closedir (dp);
  write_user (user, "\n");
}

/*** Define user events response ***/
def_events (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  int i;
  char *events[MAX_EVENTS] = { "in", "out", "shout", "bhole", "bcast",
    "kill", "mirror", "afk", "noafk", "swear", "swap"
  };

  if (word_count < 2)
    {
      write_usage (user, "event ?/none/[ <event_name> [<command>] ]");
      if (user->level >= MIN_LEV_SEVT)
	write_usage (user, "event send [ <event_name> [<value>] ]");
      return;
    }
  if (!strcmp (word[1], "?"))
    {
      write_user (user, "\n~FB~OL--->>> Events definitions <<<---\n\n");
      for (i = 0; i < MAX_EVENTS; i++)
	{
	  sprintf (text, "~FT%-7s~RS: %s\n", events[i],
		   user->events_def[i][0] ? user->events_def[i] : "~FRNONE");
	  write_user (user, text);
	}
      return;
    }
  if (!strcmp (word[1], "send"))	/* send an event */
    {
      if (user->level < MIN_LEV_SEVT)
	{
	  write_user (user, ">>>You cannot use this option!\n");
	  return;
	}
      if (word_count < 3)
	{
	  write_user (user, ">>>Event none sent.\n");
	  event = E_NONE;
	  strcpy (event_var, "*");
	  return;
	}
      if (word_count <= 3)
	strcpy (word[3], user->savename);
      for (i = 0; i < MAX_EVENTS; i++)
	{

	  if (!strcmp (word[2], events[i]))	/* found */
	    {
	      sprintf (text, ">>>Event %s sent with event variable = %s.\n",
		       word[2], word[3]);
	      write_user (user, text);
	      event = i;
	      strcpy (event_var, word[3]);
	      return;
	    }
	}
      write_user (user, ">>>Unknown event, yet...\n");
      return;
    }
  if (!strcmp (word[1], "none"))	/* clear all events definitions */
    {
      for (i = 0; i < MAX_EVENTS; i++)
	user->events_def[i][0] = '\0';
      write_user (user, ">>>All events set to ~FRNONE.\n");
      return;
    }
  for (i = 0; i < MAX_EVENTS; i++)	/* define an event */
    {
      if (!strcmp (word[1], events[i]))	/* found */
	{
	  if (word_count == 2)
	    {
	      user->events_def[i][0] = '\0';
	      sprintf (text, ">>>Event %s set to ~FRNONE.\n", events[i]);
	    }
	  else
	    {
	      inpstr = remove_first (inpstr);
	      if (strlen (inpstr) > EVENT_DEF_LEN)
		inpstr[EVENT_DEF_LEN] = '\0';
	      strcpy (user->events_def[i], inpstr);
	      sprintf (text, ">>>Event %s defined.\n", events[i]);
	    }
	  write_user (user, text);
	  return;
	}
    }
  write_user (user, ">>>Unknown event, yet...\n");
}

/*** Execute an event ***/
exec_event (user)
     UR_OBJECT user;
{
  if (event == E_NONE || !user->events_def[event][0])
    return;
  clear_words ();
  word_count = wordfind (user, user->events_def[event]);
  com_num = -1;
  exec_com (user, user->events_def[event]);
}

/*** List chat users ***/
list_users (user)
     UR_OBJECT user;
{
  FILE *infp, *outfp, *outfp_xml;
  DIR *dp;
  struct dirent *dir_struct;
  int total = 0;
  int i1, i2, i5, i, total_level[MAX_LEVELS], type, lev = 0;
  char filename[80], ulfilename[80], ulfilename_xml[80],
    line[81], username[80], str[WORD_LEN + 1], line2[81], desc[81],
    ufname[80], ulname[80], uage[80], umail[80], ugender[80], timestr[80];
  char *lu_type[] = { "all", "level", "name", "site", "email", "ident", "*" };

  if (word_count < 2)
    {
      write_usage (user, "lusers ?/all/<condition>");
      return;
    }
  if (word[1][0] == '?')	/* show conditions */
    {
      write_user (user, ">>>List users conditions:\n");
      i = 0;
      while (lu_type[i][0] != '*')
	{
	  sprintf (text, "\t%s\n", lu_type[i]);
	  write_user (user, text);
	  i++;
	}
      return;
    }
  for (i = 0; i < MAX_LEVELS; total_level[i++] = 0)
    ;
  type = -1;
  i = 0;
  while (lu_type[i][0] != '*')	/* try to detect listing type */
    {
      if (!strncmp (word[1], lu_type[i], strlen (word[1])))	/* found */
	{
	  type = i;
	  break;
	}
      i++;
    }
  if (type == -1)
    {
      write_user (user, ">>>Unknown option, yet!...\n");
      return;
    }
  if ((dp = opendir (USERFILES)) == NULL)
    {
      write_user (user, ">>>Cannot open users directory.\n");
      write_syslog (LOG_ERR, "Failed to opendir() in list_users().\n", LOG_NOTIME);
      return;
    }
  sprintf (text, "%s/%s", MISCFILES, ULISTFILE);
  strcpy (ulfilename, genfilename (text));
  if (!(outfp = fopen (ulfilename, "w")))
    {
      sprintf (text, "%s: failed to generate users list.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR, "Failed to create file in list_users().\n", LOG_NOTIME);
      closedir (dp);
      return;
    }
  sprintf (text, "%s/%s", MISCFILES, ULISTFILE);
  strcpy (ulfilename_xml, genfilename (text));
  strcat (ulfilename_xml, ".xml");
  if (!(outfp_xml = fopen (ulfilename_xml, "w")))
    {
      sprintf (text, "%s: failed to generate users list.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR, "Failed to create XML file in list_users().\n",
		    LOG_NOTIME);
      closedir (dp);
      return;
    }
  /* Header file */
  fprintf (outfp, "--->>> Users list %s ( ", long_date (1));
  fprintf (outfp_xml,
	   "<!DOCTYPE gaen PUBLIC \"-//GAEN %s//EN\">\n<?xml version=\"1.0\" ?>\n<!-- Created by GAEN %s (c)1996-2001 Sabin-Corneliu Buraga -->\n<gaen userslist=\"%s\"\n"
	   "      date=\"%s\" criterion=", GAENDTD_VER, GAEN_VER, ulfilename,
	   long_date (0));
  switch (type)
    {
    case LU_ALL:
      fprintf (outfp, "all ) <<<---\n\n");
      fprintf (outfp_xml, "\"all\">\n");
      break;
    case LU_LEVEL:
      if (word_count < 3)
	lev = user->level;
      else if ((lev = atoi (word[2])) < 0 || lev > SGOD)
	lev = user->level;
      fprintf (outfp, "level %s ) <<<---\n\n",
	       colour_com_strip (level_name[lev]));
      fprintf (outfp_xml, "\"level: %s\">\n",
	       colour_com_strip (level_name[lev]));
      break;
    case LU_NAME:
      if (word_count < 3)
	strcpy (str, user->savename);
      else
	strcpy (str, word[2]);
      fprintf (outfp, "match name '%s' ) <<<---\n\n", str);
      fprintf (outfp_xml, "\"name: %s\">\n", str);
      break;
    case LU_SITE:
      if (word_count < 3)
	strcpy (str, user->site);
      else
	strcpy (str, word[2]);
      fprintf (outfp, "site '%s' ) <<<---\n\n", str);
      fprintf (outfp_xml, "\"site: %s\">\n", str);
      break;
    case LU_EMAIL:
      if (word_count < 3)
	strcpy (str, ".ro");
      else
	strcpy (str, word[2]);
      fprintf (outfp, "e-mail '%s' ) <<<---\n\n", str);
      fprintf (outfp_xml, "\"e-mail: %s\">\n", str);
      break;
    case LU_IDENT:
      if (word_count < 3)
	strcpy (str, "Anonymous");
      else
	strcpy (str, word[2]);
      fprintf (outfp, "match identity '%s' ) <<<---\n\n", str);
      fprintf (outfp_xml, "\"identity: %s\">\n", str);
    }
  while ((dir_struct = readdir (dp)) != NULL)	/* scan users directory */
    {
      sprintf (filename, "%s/%s", USERFILES, dir_struct->d_name);
      if (filename[strlen (filename) - 1] != 'D')
	continue;		/* it isn't a .D file */
      strcpy (username, dir_struct->d_name);
      username[strlen (username) - 2] = '\0';
      if (type == LU_NAME)
	{
	  if (!strstr (username, str))	/* names not match */
	    continue;
	}
      if (!(infp = fopen (filename, "r")))
	{
	  fprintf (outfp, ">%s - cannot open %s\n", username, filename);
	  fprintf (outfp_xml,
		   "  <user name=\"%s\" errmsg=\"error: cannot open %s\">\n  </user>\n",
		   username, filename);
	  continue;
	}
      /* load from .D file various information... */
      fscanf (infp, "%s\n%d %d %d %d %d %d %d %d %d %d\n",
	      line, &i1, &i2, &i, &i, &i5, &i, &i, &i, &i, &i);
      fgets (line2, 80, infp);
      line2[strlen (line2) - 1] = '\0';
      fgets (desc, 80, infp);
      desc[strlen (desc) - 1] = '\0';
      fclose (infp);
      if (type == LU_LEVEL)
	{
	  if (i5 != lev)	/* levels not match */
	    continue;
	}
      if (type == LU_SITE)
	{
	  if (!strstr (line2, str))	/* sites not match */
	    continue;
	}
      filename[strlen (filename) - 1] = 'U';	/* load set details */
      if (!(infp = fopen (filename, "r")))
	i = 0;			/* file not found */
      else
	{
	  fgets (ufname, 80, infp);	/* first name */
	  ufname[strlen (ufname) - 1] = '\0';
	  fgets (ulname, 80, infp);	/* last name */
	  ulname[strlen (ulname) - 1] = '\0';
	  fgets (ugender, 80, infp);	/* gender */
	  ugender[strlen (ugender) - 1] = '\0';
	  fgets (uage, 80, infp);	/* age */
	  uage[strlen (uage) - 1] = '\0';
	  fgets (umail, 80, infp);	/* e-mail */
	  umail[strlen (umail) - 1] = '\0';
	  fclose (infp);
	  i = 1;
	  if (ugender[0] == 'm' || ugender[0] == 'M')
	    strcpy (ugender, "Male");
	  else if (ugender[0] == 'f' || ugender[0] == 'F')
	    strcpy (ugender, "Female");
	  else
	    strcpy (ugender, "Unknown");
	}
      if (type == LU_EMAIL)
	{
	  if (!i || !strstr (umail, str))	/* e-mails not match */
	    continue;
	}
      if (type == LU_IDENT)
	{
	  if (!i || !(strstr (ufname, str) || !strstr (ulname, str)))	/* identities not match */
	    continue;
	}
      total++;
      total_level[i5]++;
      strcpy (timestr, ctime ((time_t *) & i1));
      timestr[strlen (timestr) - 1] = '\0';
      fprintf (outfp,
	       ">%-20s - %6s - total login: %3d days %2d hours %2d mins.\n   last login %s\n",
	       username, colour_com_strip (level_name[i5]), i2 / 86400,
	       (i2 % 86400) / 3600, (i2 % 3600) / 60, timestr);
      fprintf (outfp_xml,
	       "  <user name=\"%s\" err_msg=\"ok\">\n" "    <info>\n"
	       "      <level number=\"%d\">%s</level>\n"
	       "      <desc>%s</desc>\n" "      <host>%s</host>\n"
	       "      <total_login>%d days, %d hours, %d mins</total_login>\n"
	       "      <last_login>%s</last_login>\n" "    </info>\n",
	       username, i5, colour_com_strip (level_name[i5]), desc, line2,
	       i2 / 86400, (i2 % 86400) / 3600, (i2 % 3600) / 60, timestr);
      /* user identity? */
      if (i)
	{
	  fprintf (outfp, "   %s %s - %s years %s - ", ufname, ulname, uage,
		   umail);
	  fprintf (outfp_xml,
		   "    <identity firstname=\"%s\" lastname=\"%s\"\n"
		   "              age=\"%s\" gender=\"%s\" e-mail=\"%s\" />\n",
		   ufname, ulname, uage, ugender, umail);
	}
      else
	{
	  fprintf (outfp, "   Anonymous Dreamer - ?? years anon@gaen.ro - ");
	  fprintf (outfp_xml,
		   "    <identity firstname=\"Anonymous\" lastname=\"Dreamer\"\n"
		   "              age=\"0\" gender=\"Unknown\" e-mail=\"anon@gaen.ro\" />\n");
	}
      filename[strlen (filename) - 1] = 'R';	/* load restrictions */
      if (!(infp = fopen (filename, "r")))
	{
	  fprintf (outfp, RESTRICT_MASK);
	  fprintf (outfp_xml, "    <restrict>%s</restrict>\n  </user>\n",
		   RESTRICT_MASK);
	}
      else
	{
	  fgets (line, 80, infp);
	  line[strlen (line) - 1] = '\0';
	  fprintf (outfp, "%s", line);
	  fprintf (outfp_xml, "    <restrict>%s</restrict>\n  </user>\n",
		   line);
	  fclose (infp);
	}
      fprintf (outfp, "\n");
      fprintf (outfp_xml, "\n");
    }				/* end while */
  closedir (dp);
  /* some useful statistics */
  fprintf (outfp, "\n\n>>>Statistics:\n");
  fprintf (outfp_xml, "  <statistics>\n");
  for (i = 0; i < MAX_LEVELS; i++)
    {
      fprintf (outfp, "\t%6s (%d) = %3d\n",
	       colour_com_strip (level_name[i]), i, total_level[i]);
      fprintf (outfp_xml, "    <total for=\"level: %s\" users=\"%d\" />\n",
	       colour_com_strip (level_name[i]), total_level[i]);
    }
  fprintf (outfp, "\n\n>>>Total of %d users.\n", total);
  fprintf (outfp_xml,
	   "    <total users=\"%d\" />\n  </statistics>\n</gaen>\n", total);
  fclose (outfp);
  fclose (outfp_xml);
  sprintf (text, "Users list file '%s' generated by %s.\n",
	   ulfilename, user->savename);
  write_syslog (LOG_CHAT, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
  switch (more (user, user->socket, ulfilename))
    {
    case 0:
      write_user (user, ">>>Cannot show users list.\n");
      return;
    case 1:
      user->misc_op = 2;
    }
}

/*** Save/restore user environment ***/
user_environment (user)
     UR_OBJECT user;
{
  FILE *fp;
  char filename[80], line[81];
  RM_OBJECT rm;
  int env;

  if (word_count < 2)
    {
      write_usage (user, "environ load/save/erase [<digit>]");
      return;
    }
  if (word_count < 3 || !isdigit (word[2][0]))
    env = -1;
  else
    env = word[2][0] - '0';
  sprintf (filename, "%s/%s.E%d", USERFILES, user->savename, env);
  if (!strcmp (word[1], "save"))	/* save environment */
    {
      if (!(fp = fopen (filename, "w")))
	{
	  sprintf (text, ">>>%s: failed to save your environment.\n",
		   syserror);
	  write_user (user, text);
	  sprintf (text,
		   "ENVIRON: Failed to save %s's environment file '%s'.\n",
		   user->savename, filename);
	  write_syslog (LOG_ERR, text, LOG_TIME);
	  return;
	}
      if (user->room != NULL)
	fprintf (fp, "%s\n", user->room->name);
      else
	fprintf (fp, "%s\n", room_first[0]->name);
      fprintf (fp, "%d %d %d %d %d %d %d %d %d %d %d %d\n",
	       user->say_tone, user->tell_tone,
	       user->stereo, user->dimension, user->vis, user->ignpict,
	       user->ignall, user->ignshout, user->igntell, user->plines,
	       user->wrap, user->hint_at_help);
      fclose (fp);
      write_user (user,
		  ">>>Environment saved: sky, tones, stereo, dim., vis., ignore flags, paged lines, wrap and hint at help flags.\n");
      return;
    }
  if (!strcmp (word[1], "load"))
    {
      if (!(fp = fopen (filename, "r")))
	{
	  write_user (user, ">>>Environment file not found.\n");
	  return;
	}
      fgets (line, 80, fp);
      line[strlen (line) - 1] = 0;
      if ((rm = get_room (line)) != NULL)
	user->room = rm;
      else
	write_user (user, ">>>Invalid sky name in environment file.\n");
      fscanf (fp, "%d %d %d %d %d %d %d %d %d %d %d %d",
	      &user->say_tone, &user->tell_tone,
	      &user->stereo, &user->dimension, &user->vis, &user->ignpict,
	      &user->ignall, &user->ignshout, &user->igntell,
	      &user->plines, &user->wrap, &user->hint_at_help);
      /* some verifications */
      if (user->say_tone < 0 || user->say_tone >= MAX_TONES)
	user->say_tone = 0;
      if (user->tell_tone < 0 || user->tell_tone >= MAX_TONES)
	user->tell_tone = 0;
      if (user->dimension < 0 || user->dimension >= MAX_DIMENS)
	user->dimension = 0;
      look (user);
      write_user (user,
		  ">>>Environment loaded: sky, tones, stereo, dim., vis., ignore flags, paged lines, wrap and hint at help flags.\n");
      return;
    }
  if (!strcmp (word[1], "erase"))	/* erase an environment file */
    {
      if (s_unlink (filename))
	write_user (user, ">>>Fail to erase environment file.\n");
      else
	write_user (user, ">>>Environment file erased.\n");
      return;
    }

  write_user (user, ">>>Invalid argument.\n");
}


/*** Multi-language suport ***/
language (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  int pos;
  char message[3][MAX_LEN_LANG + 1];
  UR_OBJECT u;

  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, you cannot use language.\n");
      return;
    }
  if (word_count < 2)
    {
      write_usage (user, "language [<user>] <text_def_language>");
      return;
    }
  if ((u = get_user (word[1])) != NULL)
    inpstr = remove_first (inpstr);
  inpstr = colour_com_strip (inpstr);
  for (pos = 0; pos < MAX_LEN_LANG; pos++)
    {
      message[0][pos] = ' ';
      message[1][pos] = ' ';
      message[2][pos] = ' ';
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  pos = -1;
  while (*inpstr && pos < MAX_LEN_LANG)
    {
      pos++;
      if (*inpstr == LANG_SUP && *(inpstr + 1) && *(inpstr + 2))
	{
	  message[1][pos] = *(inpstr + 2);
	  message[0][pos] = *(inpstr + 1);
	  inpstr += 3;
	  continue;
	}
      if (*inpstr == LANG_SUB && *(inpstr + 1) && *(inpstr + 2))
	{
	  message[1][pos] = *(inpstr + 2);
	  message[2][pos] = *(inpstr + 1);
	  inpstr += 3;
	  continue;
	}
      message[1][pos] = *inpstr;
      inpstr++;
    }
  ++pos;
  message[0][pos] = '\0';
  message[1][pos] = '\0';
  message[2][pos] = '\0';
  if (ban_swearing && (contains_swearing (message[0]) ||
		       contains_swearing (message[1]) ||
		       contains_swearing (message[2])))
    {
      swear_action (user, inpstr);
      return;
    }
  sprintf (text, "~OL~FT>>>From %s...\n%s\n%s\n%s\n",
	   user->vis ? user->name : "a digit", message[0], message[1],
	   message[2]);
  if (u)
    {
      if (u == user)
	{
	  write_user (user,
		      ">>>Sending language to yourself is the fifteenth sign of madness...or genius.\n");
	  return;
	}
      if (u->afk)
	{
	  sprintf (text, ">>>%s is AFK at the moment... { ~FT%s~RS }\n",
		   u->name, u->afkmesg);
	  write_user (user, text);
	  return;
	}
      if (u->ignall && (user->level < SPIRIT || u->level > user->level))
	{
	  if (u->malloc_start != NULL)
	    sprintf (text, ">>>%s is using the editor at the moment.\n",
		     u->name);
	  else
	    sprintf (text, ">>>%s is ignoring everyone at the moment.\n",
		     u->name);
	  write_user (user, text);
	  return;
	}
      if (u->igntell && (user->level < SPIRIT || u->level > user->level))
	{
	  sprintf (text, ">>>%s is ignoring tells at the moment.\n", u->name);
	  write_user (user, text);
	  return;
	}
      if (u->room == NULL)
	{
	  sprintf (text,
		   ">>>%s is offsite and would not be able to reply to you.\n",
		   u->name);
	  write_user (user, text);
	  return;
	}
      if (is_ignoring (u, user))
	{
	  sprintf (text,
		   ">>>%s is ignoring you. You must ask him to forgive you first.\n",
		   u->name);
	  write_user (user, text);
	  return;
	}
      write_user (user, ">>>Message sent.\n");
      write_user (u, text);
      record_tell (u, text);
      strcpy (u->reply_user, user->savename);
      return;
    }
  /* like a .say */
  write_room (user->room, text);
  record (user->room, text);
}

/*** Same as tell() but add a beep to each tell message ***/
beeptell (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  strcat (inpstr, "\07");
  user_tell (user, inpstr);
}

/*** Black hole simulation :) ***/
blackhole (user)
     UR_OBJECT user;
{
  int i;
  RM_OBJECT rm;
  UR_OBJECT u;

  if ((random () % BLACKHOLE_RND) < BLACKHOLE_VAL)
    {
      for (i = 0; i < MAX_DIMENS; i++)
	for (rm = room_first[i]; rm != NULL; rm = rm->next)
	  rm->hidden = !rm->hidden;
      i = 0;
      for (u = user_first; u != NULL; u = u->next)
	{
	  if (u->type != USER_TYPE || u->level >= MIN_LEV_IBHOLE ||
	      u->login || u == user || u->ignall)
	    continue;
	  u->dimension = random () % MAX_DIMENS;
	  u->pballs = random () % 100;
	  u->slevel = random () % (MAX_LEVELS - 2);
	  strcpy (u->desc, BHOLE_DESC);
	  cls (u);
	  i++;
	}
      write_room (NULL,
		  "~EL~OL~FG~LI>>>A powerful black hole appears from nowhere...\n~EL~OL~FG~LI>>>...and the talker's implosion begins!\n");
      write_room (NULL, "~EL~FG~OL~LI>>>Where is the light?...\n");
      if (user == NULL)
	sprintf (text, "Black hole auto-activated (%d victims).\n", i);
      else
	sprintf (text, "Black hole activated by %s.\n", user->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      event = E_BHOLE;
      strcpy (event_var, "*");
    }
  else
    {
      if (user != NULL)
	write_user (user, ">>>Black hole not activated.\n");
    }
}

/*** Get user gender from set file; returns: 0=female, 1=male, -1=error ***/
get_gender (username)
     char *username;
{
  char filename[80], line[WORD_LEN + 1];
  FILE *fp;

  sprintf (filename, "%s/%s.U", USERFILES, username);
  if (!(fp = fopen (filename, "r")))
    return -1;
  fgets (line, WORD_LEN + 2, fp);	/* first name (skip) */
  fgets (line, WORD_LEN + 2, fp);	/* last name (skip) */
  fgets (line, WORD_LEN + 2, fp);	/* user gender */
  fclose (fp);
  return (line[0] == 'm' || line[0] == 'M');
}

/*** GSL - If statement ***/
if_com (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  int t, neg;
  FILE *fp;
  UR_OBJECT u;
  RM_OBJECT rm;
  char *onoff_err = ">>>You must specify 'on' or 'off'...\n";

  if (word_count < 4)
    {
      write_usage (user, "if <condition> <digit1> <digit2>");
      write_usage (user, "if !<condition> <digit1> <digit2>");
      write_usage (user, "if <condition> : <command>");
      write_usage (user, "if !<condition> : <command>");
      return;
    }
  if (word[1][0] == '!')
    {
      neg = 1;
      strcpy (word[1], &word[1][1]);
    }
  else
    neg = 0;
  if (word[3][0] == ':')	/* see if it's a command after condition */
    inpstr = remove_first (remove_first (remove_first (inpstr)));
  else
    {
      inpstr[0] = '\0';
      if (word_count < 5 || !isnumber (word[3]) || !isnumber (word[4]))	/* see it's a digit after condition */
	{
	  write_user (user, ">>>Wrong parameters.\n");
	  return;
	}
    }
  /* check condition */
  if (!strcmp (word[1], "={"))
    {
      t = !strcmp (word[2], user->var1);
      goto RUN_L;
    }
  if (!strcmp (word[1], "=}"))
    {
      t = !strcmp (word[2], user->var2);
      goto RUN_L;
    }
  if (!strcmp (word[1], "=*"))
    {
      t = !strcmp (word[2], event_var);
      goto RUN_L;
    }
  if (!strcmp (word[1], "in{"))
    {
      t = strstr (user->var1, word[2]) != NULL;
      goto RUN_L;
    }
  if (!strcmp (word[1], "in}"))
    {
      t = strstr (user->var2, word[2]) != NULL;
      goto RUN_L;
    }
  if (!strcmp (word[1], "in*"))
    {
      t = strstr (user->var1, event_var) != NULL;
      goto RUN_L;
    }
  if (!strcmp (word[1], "<{"))
    {
      t = atoi (user->var1) < atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], "<}"))
    {
      t = atoi (user->var2) < atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], "<*"))
    {
      t = atoi (event_var) < atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], ">{"))
    {
      t = atoi (user->var1) > atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], ">}"))
    {
      t = atoi (user->var2) > atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], ">*"))
    {
      t = atoi (event_var) > atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], "exist"))
    {
      word[2][0] = toupper (word[2][0]);
      sprintf (text, "%s/%s.D", USERFILES, word[2]);
      if ((fp = fopen (text, "r")) != NULL)
	{
	  t = 1;
	  fclose (fp);
	}
      else
	t = 0;
      goto RUN_L;
    }
  if (!strcmp (word[1], "board"))
    {
      word[2][0] = toupper (word[2][0]);
      sprintf (text, "%s/%s.B", DATAFILES, word[2]);
      if ((fp = fopen (text, "r")) != NULL)
	{
	  t = 1;
	  fclose (fp);
	}
      else
	t = 0;
      goto RUN_L;
    }
  if (!strcmp (word[1], "safety"))
    {
      strtolower (word[2]);
      if (!strcmp (word[2], "on"))
	t = safety;
      else if (!strcmp (word[2], "off"))
	t = !safety;
      else
	{
	  write_user (user, onoff_err);
	  return;
	}
      goto RUN_L;
    }
  if (!strcmp (word[1], "toutafks"))
    {
      strtolower (word[2]);
      if (!strcmp (word[2], "on"))
	t = time_out_afks;
      else if (!strcmp (word[2], "off"))
	t = !time_out_afks;
      else
	{
	  write_user (user, onoff_err);
	  return;
	}
      goto RUN_L;
    }
  if (!strcmp (word[1], "banswear"))
    {
      strtolower (word[2]);
      if (!strcmp (word[2], "on"))
	t = ban_swearing;
      else if (!strcmp (word[2], "off"))
	t = !ban_swearing;
      else
	{
	  write_user (user, onoff_err);
	  return;
	}
      goto RUN_L;
    }

  if (!strcmp (word[1], "set"))
    {
      word[2][0] = toupper (word[2][0]);
      sprintf (text, "%s/%s.U", USERFILES, word[2]);
      if ((fp = fopen (text, "r")) != NULL)
	{
	  t = 1;
	  fclose (fp);
	}
      else
	t = 0;
      goto RUN_L;
    }
  if (!strcmp (word[1], "profile"))
    {
      word[2][0] = toupper (word[2][0]);
      sprintf (text, "%s/%s.P", USERFILES, word[2]);
      if ((fp = fopen (text, "r")) != NULL)
	{
	  t = 1;
	  fclose (fp);
	}
      else
	t = 0;
      goto RUN_L;
    }
  if (!strcmp (word[1], "subst"))
    {
      word[2][0] = toupper (word[2][0]);
      sprintf (text, "%s/%s.S", USERFILES, word[2]);
      if ((fp = fopen (text, "r")) != NULL)
	{
	  t = 1;
	  fclose (fp);
	}
      else
	t = 0;
      goto RUN_L;
    }
  if (!strcmp (word[1], "mail"))
    {
      word[2][0] = toupper (word[2][0]);
      sprintf (text, "%s/%s.M", USERFILES, word[2]);
      if ((fp = fopen (text, "r")) != NULL)
	{
	  t = 1;
	  fclose (fp);
	}
      else
	t = 0;
      goto RUN_L;
    }
  if (!strcmp (word[1], "file"))
    {
      if ((fp = fopen (word[2], "r")) != NULL)
	{
	  t = 1;
	  fclose (fp);
	}
      else
	t = 0;
      goto RUN_L;
    }
  if (!strcmp (word[1], "user"))
    {
      word[2][0] = toupper (word[2][0]);
      t = get_user (word[2]) != NULL;
      goto RUN_L;
    }
  if (!strcmp (word[1], "minlogin"))
    {
      t = minlogin_level == atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], "event"))
    {
      t = event == atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], "pballs"))
    {
      t = user->pballs == atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], "alarmhour"))
    {
      t = user->alarm_hour == atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], "alarmmin"))
    {
      t = user->alarm_min == atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], "sky"))
    {
      if (user->room != NULL)
	t = !strcmp (user->room->name, word[2]);
      else
	t = 1;			/* if user is remote, condition will be true */
      goto RUN_L;
    }
  if (!strcmp (word[1], "random"))
    {
      t = random () % 256 > atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], "hangman"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->hangman != NULL;
      else
	t = 0;			/* if user don't exist, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "puzzle"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->puzzle != NULL;
      else
	t = 0;			/* if user don't exist, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "dim"))
    {
      t = (user->dimension + 1) == atoi (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], "reply"))
    {
      t = !strcmp (word[2], user->reply_user);
      goto RUN_L;
    }

  if (!strcmp (word[1], "muzzle"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->muzzled > 0;
      else
	t = 0;
      goto RUN_L;
    }
  if (!strcmp (word[1], "invis"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = !u->vis;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "afk"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->afk;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "renamed"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = strcmp (u->savename, u->name);
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "level0"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->level == 0;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "level1"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->level == 1;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "level2"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->level == 2;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "level3"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->level == 3;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "level4"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->level == 4;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "level5"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->level == 5;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "level6"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->level == 6;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "level7"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->level == 7;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "level8"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->level == 8;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "ignall"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->ignall;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "igntell"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->igntell;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "ignshout"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->ignshout;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "ignpict"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->ignpict;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "stereo"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->stereo;
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "here"))
    {
      if ((u = get_user (word[2])) != NULL)
	{
	  if (user->room == NULL && u->room == NULL)
	    t = 1;
	  else if (user->room != NULL && u->room != NULL)
	    t = user->room == u->room;
	  else
	    t = 0;
	}
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "newmail"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = has_unread_mail (u);
      else
	t = 0;			/* if user isn't logged on, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "hidden"))
    {
      if ((rm = get_room (word[2])) != NULL)
	t = rm->hidden > 0;
      else
	t = 0;			/* if sky not exists, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "fixed"))
    {
      if ((rm = get_room (word[2])) != NULL)
	t = rm->access & FIXED;
      else
	t = 0;			/* if sky not exists, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "private"))
    {
      if ((rm = get_room (word[2])) != NULL)
	t = rm->access & PRIVATE;
      else
	t = 0;			/* if sky not exists, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "hasaccess"))
    {
      if ((rm = get_room (word[2])) != NULL)
	t = has_room_access (user, rm);
      else
	t = 0;			/* if sky not exists, condition will be false */
      goto RUN_L;
    }
  if (!strcmp (word[1], "topic"))
    {
      if ((rm = get_room (word[2])) != NULL)
	t = rm->topic[0] != '\0';
      else
	t = 0;			/* if sky not exists, condition will be false */
      goto RUN_L;
    }

  if (!strcmp (word[1], "mirror"))
    {
      if (!strcmp (word[2], "EDEN"))
	t = mirror == 0;
      else if (!strcmp (word[2], "GAEN"))
	t = mirror == 1;
      else if (!strcmp (word[2], "HELL"))
	t = mirror == 2;
      else if (!strcmp (word[2], "DILI"))
	t = mirror == 3;
      else
	{
	  write_user (user, ">>>Invalid parameter!\n");
	  return;
	}
      goto RUN_L;
    }
  if (!strcmp (word[1], "restrict"))
    {
      t = strstr (restrict_string, word[2]) != NULL;
      goto RUN_L;
    }
  if (!strcmp (word[1], "secure"))
    {
      t = secure_pass (user, word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], "male"))
    {
      t = get_gender (word[2]) == 1;
      goto RUN_L;
    }
  if (!strcmp (word[1], "female"))
    {
      t = !get_gender (word[2]);
      goto RUN_L;
    }
  if (!strcmp (word[1], "yearday"))
    {
      t = atoi (word[2]) == tday;
      goto RUN_L;
    }
  if (!strcmp (word[1], "weekday"))
    {
      t = atoi (word[2]) == twday;
      goto RUN_L;
    }
  if (!strcmp (word[1], "hour"))
    {
      t = atoi (word[2]) == thour;
      goto RUN_L;
    }
  if (!strcmp (word[1], "min"))
    {
      t = atoi (word[2]) == tmin;
      goto RUN_L;
    }
  if (!strcmp (word[1], "month"))
    {
      t = atoi (word[2]) == tmonth;
      goto RUN_L;
    }
  if (!strcmp (word[1], "day"))
    {
      t = atoi (word[2]) == tmday;
      goto RUN_L;
    }
  if (!strcmp (word[1], "year"))
    {
      t = atoi (word[2]) == tyear;
      goto RUN_L;
    }
  if (!strcmp (word[1], "remote"))
    {
      if ((u = get_user (word[2])) != NULL)
	t = u->type == REMOTE_TYPE;
      else
	t = 0;
      goto RUN_L;
    }
  write_user (user, ">>>Unknown condition, yet!\n");
  return;
RUN_L:
  write_user (user, "~OL>>>Condition is ");
  if (neg)
    t = !t;
  if (t)
    {
      write_user (user, "~FGTRUE~RS.\n");
      if (inpstr[0] == '\0')
	run_commands (user, word[3][0] - '0');
      else
	{
	  sprintf (text, ">>>Executing... ~FG%s\n", inpstr);
	  write_user (user, text);
	  clear_words ();
	  word_count = wordfind (user, inpstr);
	  com_num = -1;
	  exec_com (user, inpstr);
	}
    }
  else
    {
      write_user (user, "~FRFALSE~RS.\n");
      if (inpstr[0] == '\0')
	run_commands (user, word[4][0] - '0');
    }
  prompt (user);
}

/*** GSL - For statement (first form) ***/
for_com (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char save_var[WORD_LEN + 1];
  char *options[] = { "users", "dims", "links", "cmds", "levels", "*" };
  UR_OBJECT u, next;
  RM_OBJECT rm;
  NL_OBJECT nl;
  int i, cmd;

  if (word_count < 2)
    {
      write_usage (user, "for <digit> <option>/<list_of_words>");
      write_usage (user, "for <option> : <command>");
      write_usage (user, "for help/?");
      return;
    }
  if (word[1][0] == '?' || !strcmp (word[1], "help") || word_count < 3)
    {
      i = 0;
      write_user (user, ">>>~FG~OLGSL for~RS available options:\n");
      while (options[i][0] != '*')
	{
	  sprintf (text, "\t\t%s\n", options[i]);
	  write_user (user, text);
	  i++;
	}
      return;
    }
  if (!isnumber (word[1]))
    {
      if (!strcmp (word[2], ":"))
	{			/* maybe, it's the second form... */
	  for2_com (user, inpstr);
	}
      else
	{
	  write_user (user, ">>>Invalid run commands file.\n");
	}
      return;
    }
  strcpy (save_var, user->var1);
  cmd = word[1][0] - '0';
  if (!strcmp (word[2], "users"))
    {
      sprintf (text, "~OL>>>For each user run commands file ~FT%d~RS...\n",
	       cmd);
      write_user (user, text);
      u = user_first;
      while (u)
	{
	  next = u->next;
	  if (!u->login && u->type != CLONE_TYPE && u != user)
	    {
	      strcpy (user->var1, u->name);
	      run_commands (user, cmd);
	    }
	  u = next;
	}
      write_user (user, "~OL>>>End for.\n");
      strcpy (user->var1, save_var);
      return;
    }
  if (!strcmp (word[2], "dims"))
    {
      sprintf (text,
	       "~OL>>>For each dimension's sky run commands file ~FT%d~RS...\n",
	       cmd);
      write_user (user, text);
      for (i = 0; i < MAX_DIMENS; i++)
	for (rm = room_first[i]; rm != NULL; rm = rm->next)
	  {
	    strcpy (user->var1, rm->name);
	    run_commands (user, cmd);
	  }
      write_user (user, "~OL>>>End for.\n");
      strcpy (user->var1, save_var);
      return;
    }
  if (!strcmp (word[2], "links"))
    {
      sprintf (text, "~OL>>>For each netlink run commands file ~FT%d~RS...\n",
	       cmd);
      write_user (user, text);
      for (nl = nl_first; nl != NULL; nl = nl->next)
	{
	  strcpy (user->var1, nl->service);
	  run_commands (user, cmd);
	}
      write_user (user, "~OL>>>End for.\n");
      strcpy (user->var1, save_var);
      return;
    }
  if (!strcmp (word[2], "levels"))
    {
      sprintf (text, "~OL>>>For each level run commands file ~FT%d~RS...\n",
	       cmd);
      write_user (user, text);
      for (i = 0; i < MAX_LEVELS; i++)
	{
	  sprintf (user->var1, "%d", i);
	  run_commands (user, cmd);
	}
      write_user (user, "~OL>>>End for.\n");
      strcpy (user->var1, save_var);
      return;
    }
  if (!strcmp (word[2], "cmds"))
    {
      sprintf (text,
	       "~OL>>>For each available command run commands file ~FT%d~RS...\n",
	       cmd);
      write_user (user, text);
      i = 0;
      while (command[i][0] != '*')
	{
	  if (com_level[i] > user->level)
	    {
	      i++;
	      continue;
	    }
	  strcpy (user->var1, command[i]);
	  run_commands (user, cmd);
	  i++;
	}
      write_user (user, "~OL>>>End for.\n");
      strcpy (user->var1, save_var);
      return;
    }
  for (i = 2; i < word_count; i++)
    {
      strcpy (user->var1, word[i]);
      sprintf (text, "~OL>>>For variable { = %s run commands file ~FT%d...\n",
	       word[i], word[1][0] - '0');
      write_user (user, text);
      run_commands (user, cmd);
    }
  strcpy (user->var1, save_var);
  write_user (user, "~OL>>>End for.\n");
}

/*** GSL - For statement (second form) ***/
for2_com (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char save_var[WORD_LEN + 1], cmd_line[ARR_SIZE];
  UR_OBJECT u, next;
  RM_OBJECT rm;
  NL_OBJECT nl;
  int i;

  strcpy (save_var, user->var1);
  strcpy (cmd_line, remove_first (remove_first (inpstr)));
  if (!cmd_line)
    {
      write_usage (user, "for <option> : <command>");
      return;
    }
  if (!strcmp (word[1], "users"))
    {
      sprintf (text, "~OL>>>For each user execute '%s'...\n", cmd_line);
      write_user (user, text);
      u = user_first;
      while (u)
	{
	  next = u->next;
	  if (!u->login && u->type != CLONE_TYPE && u != user)
	    {
	      strcpy (user->var1, u->name);
	      clear_words ();
	      word_count = wordfind (user, cmd_line);
	      com_num = -1;
	      exec_com (user, cmd_line);
	    }
	  u = next;
	}
      write_user (user, "~OL>>>End for.\n");
      strcpy (user->var1, save_var);
      return;
    }
  if (!strcmp (word[1], "dims"))
    {
      sprintf (text, "~OL>>>For each dimension's sky execute '%s'...\n",
	       cmd_line);
      write_user (user, text);
      for (i = 0; i < MAX_DIMENS; i++)
	for (rm = room_first[i]; rm != NULL; rm = rm->next)
	  {
	    strcpy (user->var1, rm->name);
	    clear_words ();
	    word_count = wordfind (user, cmd_line);
	    com_num = -1;
	    exec_com (user, cmd_line);
	  }
      write_user (user, "~OL>>>End for.\n");
      strcpy (user->var1, save_var);
      return;
    }
  if (!strcmp (word[1], "links"))
    {
      sprintf (text, "~OL>>>For each netlink execute '%s'...\n", cmd_line);
      write_user (user, text);
      for (nl = nl_first; nl != NULL; nl = nl->next)
	{
	  strcpy (user->var1, nl->service);
	  clear_words ();
	  word_count = wordfind (user, cmd_line);
	  com_num = -1;
	  exec_com (user, cmd_line);
	}
      write_user (user, "~OL>>>End for.\n");
      strcpy (user->var1, save_var);
      return;
    }
  if (!strcmp (word[1], "levels"))
    {
      sprintf (text, "~OL>>>For each level execute '%s'...\n", cmd_line);
      write_user (user, text);
      for (i = 0; i < MAX_LEVELS; i++)
	{
	  sprintf (user->var1, "%d", i);
	  clear_words ();
	  word_count = wordfind (user, cmd_line);
	  com_num = -1;
	  exec_com (user, cmd_line);
	}
      write_user (user, "~OL>>>End for.\n");
      strcpy (user->var1, save_var);
      return;
    }
  if (!strcmp (word[1], "cmds"))
    {
      sprintf (text, "~OL>>>For each available command execute '%s'...\n",
	       cmd_line);
      write_user (user, text);
      i = 0;
      while (command[i][0] != '*')
	{
	  if (com_level[i] > user->level)
	    {
	      i++;
	      continue;
	    }
	  strcpy (user->var1, command[i]);
	  clear_words ();
	  word_count = wordfind (user, cmd_line);
	  com_num = -1;
	  exec_com (user, cmd_line);
	  i++;
	}
      write_user (user, "~OL>>>End for.\n");
      strcpy (user->var1, save_var);
      return;
    }
  write_user (user, ">>>Unknown option, yet...\n");
}

/*** set the alarm clock ***/
alarm_user (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  int hour, min;
  if (word_count < 2)
    {
      if (user->alarm_hour == -1 && user->alarm_min == -1)
	write_user (user, ">>>Your alarm clock isn't set.\n");
      else
	{
	  sprintf (text,
		   ">>>Your alarm clock is set to ~FT%02d:%02d~RS - command: ~FT%s~RS.\n",
		   user->alarm_hour, user->alarm_min,
		   user->alarm_cmd[0] ? user->alarm_cmd : "none");
	  write_user (user, text);
	}
      return;
    }
  if (!strcmp (word[1], "none"))
    {
      write_user (user, ">>>Alarm clock set to none.\n");
      user->alarm_hour = -1;
      user->alarm_min = -1;
      user->alarm_cmd[0] = '\0';
      user->alarm_activated = 0;
      return;
    }
  if (word_count < 3)
    {
      write_usage (user, "alarm <hour> <min> [ <command> ]");
      write_usage (user, "alarm none");
      return;
    }
  if (word_count == 3)
    user->alarm_cmd[0] = '\0';
  else
    {
      inpstr = remove_first (remove_first (inpstr));
      if (strlen (inpstr) > ALARM_CMD_LEN)
	inpstr[ALARM_CMD_LEN] = '\0';
      strcpy (user->alarm_cmd, inpstr);
    }
  hour = atoi (word[1]);
  if (hour < 0 || hour > 23)
    hour = thour;
  min = atoi (word[2]);
  if (min < 0 || min > 59)
    min = tmin;
  user->alarm_hour = hour;
  user->alarm_min = min;
  user->alarm_activated = 1;
  sprintf (text,
	   ">>>Alarm clock set to ~FT%02d:%02d~RS - command ~FT%s~RS.\n",
	   user->alarm_hour, user->alarm_min,
	   user->alarm_cmd[0] ? user->alarm_cmd : "none");
  write_user (user, text);
}

/*** Born a new user ***/
born (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  int lev, i;
  FILE *fp;
  char filename[80];

  if (word_count < 4)
    {
      write_usage (user, "born <name> <password> <level>");
      return;
    }
  if (strlen (word[1]) > USER_NAME_LEN || strlen (word[1]) < USER_MIN_LEN)
    {
      write_user (user, ">>>Invalid name length.\n");
      return;
    }
  for (i = 0; i < strlen (word[1]); i++)
    if (!isalpha (word[1][i]))
      {
	write_user (user,
		    ">>>Only letters are allowed in a name. Try again...\n");
	return;
      }
  strtolower (word[1]);
  word[1][0] = toupper (word[1][0]);
  sprintf (filename, "%s/%s.D", USERFILES, word[1]);
  /* check if user already exists */
  if ((fp = fopen (filename, "r")) != NULL)
    {
      write_user (user, ">>>This user already exists!\n");
      fclose (fp);
      return;
    }
  if (strlen (word[2]) > PASS_MAX_LEN || strlen (word[2]) < PASS_MIN_LEN)
    {
      write_user (user, ">>>Invalid password length.\n");
      return;
    }
  if (!secure_pass (word[2]))
    {
      write_user (user,
		  ">>>Password is not secure! Try with other password...\n");
      return;
    }
  if ((lev = atoi (word[3])) < 0 || lev > user->level)	/* invalid level, so it will be set to default */
    lev = newuser_deflevel;
  if (!allow_born_maxlev && lev >= MAX_LEVELS)	/* if giving birth of a super-user is not allowed */
    lev = newuser_deflevel;	/* ...so we'll adjust the level to default */
  if ((u = create_user ()) == NULL)
    {
      write_user (user, ">>>Unable to create a new user! Sorry...\n");
      write_syslog (LOG_ERR,
		    "Couldn't create a temporary user object in born().\n",
		    LOG_NOTIME);
      return;
    }
  /* Fill user information */
  u->socket = -2;
  u->last_input = time (0);
  u->level = lev;
  u->prompt = prompt_def;
  u->charmode_echo = 0;
  u->colour = colour_def;
  strcpy (u->savename, word[1]);
  strcpy (u->pass, (char *) crypt (word[2], PASS_SALT));
  strcpy (u->site, "gaen.ro");
  strcpy (u->desc, DEF_DESC);
  strcpy (u->in_phrase, "enters");
  strcpy (u->out_phrase, "goes");
  strcpy (u->restrict, RESTRICT_MASK);
  save_user_details (u, 1);
  sprintf (text, ">>>New user %s created by ~FG~OLGAEN~RS.\n", u->savename);
  write_user (user, text);
  sprintf (text, "~OLGAEN:~RS User %s (level %d) BORN by %s.\n", u->savename,
	   lev, user->savename);
  write_syslog (LOG_CHAT, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
  if (lev >= MIN_LEV_ALLOW)
    {
      write_user (user, ">>>Adding user name to allow files...\n");
      for (i = MIN_LEV_ALLOW; i <= lev; i++)
	{
	  if (i > MAX_LEV_ALLOW)
	    break;
	  /* Add the user name to allow files */
	  strcpy (word[2], u->savename);
	  sprintf (word[1], "%d", i);
	  word_count = 3;
	  allow (user);
	}
    }
  destruct_user (u);
  destructed = 0;
}

/*** List login/logout entries ***/
list_io (user)
     UR_OBJECT user;
{
  char filename[80];
  sprintf (filename, "%s/%s.%s", LOGFILES, SYSLOG, logtype[LOG_IO]);
  write_user (user, "\n~FB~OL--->>> I/O log users entries <<<---\n");
  switch (more (user, user->socket, filename))
    {
    case 0:
      write_user (user, ">>>Log file is unavailable, sorry...\n");
      return;
    case 1:
      user->misc_op = 2;
    }
}

/*** Stereo enable/disable flag ***/
stereo_user (user)
     UR_OBJECT user;
{
  if (user->stereo)
    {
      write_user (user, ">>>Stereo voice ~FROFF.\n");
      user->stereo = 0;
      return;
    }
  write_user (user, ">>>Stereo voice ~FGON.\n");
  user->stereo = 1;
}

/*** Allow users to be superusers ***/
allow (user)
     UR_OBJECT user;
{
  int lev = 0, i, allowall = 0;
  char filename[80];

  if (word_count < 2)
    {
      write_usage (user, "allow <level>/all [<name>]");
      return;
    }
  if (!strcmp (word[1], "all"))
    allowall = 1;
  else if (!isnumber (word[1]))
    lev = user->level;
  else
    lev = atoi (word[1]) % MAX_LEVELS;
  if (!allowall && lev < MIN_LEV_ALLOW)
    {
      write_user (user, ">>>Level too low.\n");
      return;
    }
  if (!allowall && lev > MAX_LEV_ALLOW)
    {
      write_user (user, ">>>Level too high.\n");
      return;
    }
  if (word_count == 2 && strcmp (word[1], "all"))	/* list allow file */
    {
      sprintf (filename, "%s/%s.%d", DATAFILES, ALLOWFILE, lev);
      write_user (user, "\n~FB~OL--->>> Allow file <<<---\n\n");
      switch (more (user, user->socket, filename))
	{
	case 0:
	  write_user (user, ">>>Allow file is unavailable.\n");
	  return;
	case 1:
	  user->misc_op = 2;
	}
      return;
    }
  else if (word_count == 2)
    {
      write_user (user, ">>>Use 'all' option followed by an user name.\n");
      return;
    }
  word[2][0] = toupper (word[2][0]);
  if (allowall)
    {
      for (i = MIN_LEV_ALLOW; i <= MAX_LEV_ALLOW; i++)
	{
	  sprintf (text, ">>>Checking allow level %s~RS...\n", level_name[i]);
	  write_user (user, text);
	  sprintf (filename, "%s/%s.%d", DATAFILES, ALLOWFILE, i);
	  allow_user (user, word[2], filename);
	}
    }
  else
    {
      sprintf (filename, "%s/%s.%d", DATAFILES, ALLOWFILE, lev);
      allow_user (user, word[2], filename);
    }
}

/*** Additional function used by allow() ***/
allow_user (user, username, filename)
     UR_OBJECT user;
     char *username, *filename;
{
  FILE *fp;
  char name[80];

/*See if allowed name already exist */
  if ((fp = fopen (filename, "r")) != NULL)
    {
      fscanf (fp, "%s", name);
      while (!feof (fp))
	{
	  if (!strcmp (username, name))
	    {
	      write_user (user, ">>>That user is already allowed.\n");
	      fclose (fp);
	      return;
	    }
	  fscanf (fp, "%s", name);
	}
      fclose (fp);
    }
/* Write new allowed name to file */
  if (!(fp = fopen (filename, "a")))
    {
      sprintf (text, "%s: Can't open file to append.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Couldn't open file to append in allow().\n", 
		    LOG_NOTIME);
      return;
    }
  fprintf (fp, "%s\n", username);
  fclose (fp);
  sprintf (text, ">>>Allow file '%s' updated.\n", filename);
  write_user (user, text);
  sprintf (text, "~OLGAEN~RS: %s added %s to allow file: %s\n",
	   user->savename, username, filename);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
}

/*** Delete an user from allow file ***/
dallow (user)
     UR_OBJECT user;
{
  int lev = 0, i, dallowall = 0;
  char filename[80];

  if (word_count < 3)
    {
      write_usage (user, "dallow <level>/all <name>");
      return;
    }
  if (!isnumber (word[1]))
    {
      if (!strcmp (word[1], "all"))
	dallowall = 1;
      else
	lev = user->level;
    }
  else
    lev = atoi (word[1]) % MAX_LEVELS;
  if (!dallowall && lev < MIN_LEV_ALLOW)
    {
      write_user (user, ">>>Level too low.\n");
      return;
    }
  if (!dallowall && lev > MAX_LEV_ALLOW)
    {
      write_user (user, ">>>Level too high.\n");
      return;
    }
  word[2][0] = toupper (word[2][0]);
  if (dallowall)
    {
      for (i = MIN_LEV_ALLOW; i <= MAX_LEV_ALLOW; i++)
	{
	  sprintf (text, ">>>Checking allow level %s~RS...\n", level_name[i]);
	  sprintf (filename, "%s/%s.%d", DATAFILES, ALLOWFILE, i);
	  write_user (user, text);
	  dallow_user (user, word[2], filename);
	}
    }
  else
    {
      sprintf (filename, "%s/%s.%d", DATAFILES, ALLOWFILE, lev);
      dallow_user (user, word[2], filename);
    }
}

/*** Additional function used by dallow() ***/
dallow_user (user, username, filename)
     UR_OBJECT user;
     char *username, *filename;
{
  FILE *infp, *outfp, *fp;
  char name[80], p[80];
  int found, a, b, c, d, lev;
  UR_OBJECT u;

  if (!(infp = fopen (filename, "r")))
    {
      write_user (user, ">>>Allow file not found.\n");
      return;
    }
  if ((u = get_user (username)) != NULL)	/* security facility */
    {
      if (u->level >= user->level)
	{
	  write_user (user,
		      ">>>You cannot delete from allow file an user of equal or higher level than yourself.\n");
	  return;
	}
    }
  else
    {
      /* User not on so load up his data */
      sprintf (name, "%s/%s.D", USERFILES, username);
      if ((fp = fopen (name, "r")) != NULL)	/* found */
	{
	  fscanf (fp, "%s\n%d %d %d %d %d", p, &a, &b, &c, &d, &lev);
	  fclose (fp);
	  if (lev >= user->level)
	    {
	      write_user (user,
			  ">>>You cannot delete from allow file an user of equal or higher level than yourself.\n");
	      return;
	    }
	}
    }
  if (!(outfp = fopen ("tempfile", "w")))
    {
      sprintf (text, ">>>%s: Couldn't open tempfile.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Couldn't open tempfile to write in dallow().\n",
		    LOG_NOTIME);
      fclose (infp);
      return;
    }
  /* Delete the user from allow file */
  found = 0;
  fscanf (infp, "%s", name);
  while (!feof (infp))
    {
      if (strcmp (username, name))
	fprintf (outfp, "%s\n", name);
      else
	found = 1;
      fscanf (infp, "%s", name);
    }
  fclose (infp);
  fclose (outfp);
  if (!found)
    {
      write_user (user, ">>>That user is not currently allowed.\n");
      unlink ("tempfile");
      return;
    }
  sprintf (text, "%s%s", filename, SAFETY_EXT);
  rename (filename, text);	/* safety feature */
  rename ("tempfile", filename);
  sprintf (text, ">>>User removed from allow file '%s'.\n", filename);
  write_user (user, text);
  sprintf (text, "~OLGAEN~RS: %s removed %s from allow file: %s.\n",
	   user->savename, username, filename);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
}

/*** Check user level ***/
check_allow (user, level)
     UR_OBJECT user;
     int level;
{
  FILE *fp;
  char filename[80], name[80];

  if (user->level < level)
    return 1;
  sprintf (filename, "%s/%s.%d", DATAFILES, ALLOWFILE, level);
  if (!(fp = fopen (filename, "r")))
    return 0;
  fscanf (fp, "%s", name);
  while (!feof (fp))
    {
      name[0] = toupper (name[0]);
      if (!strcmp (user->savename, name))
	{
	  fclose (fp);
	  return 1;
	}
      fscanf (fp, "%s", name);
    }
  /* not found */
  fclose (fp);
  return 0;
}

/*** Clone rename ***/
clone_rename (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  UR_OBJECT u;
  char filename[80];
  FILE *fp;
  int i;

  if (word_count < 2)
    {
      write_usage (user, "crename <sky clone is in> [<new_name>]");
      return;
    }
  if ((rm = get_room (word[1])) == NULL)
    {
      write_user (user, nosuchroom);
      return;
    }
  if (word_count >= 3)
    {
      if (strlen (word[2]) > USER_NAME_LEN)
	word[2][USER_NAME_LEN] = '\0';
      word[2][0] = toupper (word[2][0]);
      sprintf (filename, "%s/%s.D", USERFILES, word[2]);
      if ((fp = fopen (filename, "r")) != NULL)
	{
	  fclose (fp);
	  write_user (user, ">>>This name is already used!\n");
	  return;
	}
      for (i = 0; i < strlen (word[2]); i++)
	if (!isalpha (word[2][i]))
	  {
	    write_user (user, ">>>You cannot use this name!\n");
	    return;
	  }
    }
  else
    strcpy (word[2], user->savename);	/* restore clone's original name */
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->type == CLONE_TYPE && u->room == rm && u->owner == user)
	{
	  strcpy (u->name, word[2]);
	  write_user (user, ">>>Clone name changed!\n");
	  return;
	}
    }
  write_user (user, ">>>You do not have a clone in that sky.\n");
}

/*** Add run commands ***/
arun (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char *com_type[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "in", "out", "normal", "swap"
  };
  char com_ext[] = "0123456789IONW";
  FILE *fp;
  int i;
  char filename[80];

  if (word_count < 2)
    {
      write_usage (user, "arun in/out/normal/swap/<digit> [<command>]");
      return;
    }
  if (word_count == 2)		/* list commands */
    {
      for (i = 0; i < MAX_RUNS; i++)
	{
	  if (!strcmp (word[1], com_type[i]))
	    {
	      sprintf (filename, "%s/%s.%c", USERFILES, user->savename,
		       com_ext[i]);
	      sprintf (text, "\n~FB~OL--->>> Run %s commands <<<---\n\n",
		       com_type[i]);
	      write_user (user, text);
	      switch (more (user, user->socket, filename))
		{
		case 0:
		  write_user (user, ">>>No commands.\n");
		  return;
		case 1:
		  user->misc_op = 2;
		}
	      return;
	    }
	}
      write_user (user, ">>>Invalid parameter!\n");
      return;
    }
  if (strstr (word[2], "q") || (user->level < GOD && strstr (word[2], "ru")))
    {
      write_user (user, ">>>Invalid command!\n");
      return;
    }
  inpstr = remove_first (inpstr);	/* write a command */
  if (strlen (inpstr) > COMMAND_LEN)
    inpstr[COMMAND_LEN] = '\0';
  for (i = 0; i < MAX_RUNS; i++)
    {
      if (!strcmp (word[1], com_type[i]))
	{
	  sprintf (filename, "%s/%s.%c", USERFILES, user->savename,
		   com_ext[i]);
	  if ((fp = fopen (filename, "a")) == NULL)
	    {
	      sprintf (text, ">>>%s: couldn't save command.\n", syserror);
	      write_user (user, text);
	      sprintf (text,
		       "ERROR: Couldn't open file %s to write in arun().\n",
		       filename);
	      write_syslog (LOG_ERR, text, LOG_NOTIME);
	      return;
	    }
	  strcat (inpstr, "\n");
	  fputs (inpstr, fp);
	  fclose (fp);
	  write_user (user, ">>>Command added.\n");
	  return;
	}
    }
  write_user (user, ">>>Invalid argument!\n");
}

/*** Delete run commands ***/
drun (user)
     UR_OBJECT user;
{
  char *com_type[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "in", "out", "normal", "swap"
  };
  char com_ext[] = "0123456789IONW";
  int i;
  char filename[80];

  if (word_count < 2)
    {
      write_usage (user, "drun in/out/normal/swap/<digit>");
      return;
    }
  for (i = 0; i < MAX_RUNS; i++)
    {
      if (!strcmp (word[1], com_type[i]))
	{
	  sprintf (filename, "%s/%s.%c", USERFILES, user->savename,
		   com_ext[i]);
	  unlink (filename);
	  sprintf (text, ">>>Run commands ( %s ) file deleted.\n",
		   com_type[i]);
	  write_user (user, text);
	  return;
	}
    }
  write_user (user, ">>>Invalid parameter!\n");
}

/*** Run commands interface ***/
run (user)
     UR_OBJECT user;
{
  if (word_count < 2)
    {
      run_commands (user, RUNCOM_NORMAL);
      return;
    }
  if (!strcmp (word[1], "in"))
    {
      run_commands (user, RUNCOM_IN);
      return;
    }
  if (!strcmp (word[1], "out"))
    {
      run_commands (user, RUNCOM_OUT);
      return;
    }
  if (!strcmp (word[1], "swap"))
    {
      run_commands (user, RUNCOM_SWAP);
      return;
    }
  if (isdigit (word[1][0]))
    {
      run_commands (user, word[1][0] - '0');
      return;
    }
  write_usage (user, "run [ in/out/swap/<digit> ]");
}

/*** Run commands from file ***/
run_commands (user, type_command)
     UR_OBJECT user;
     int type_command;
{
  char com_type_ext[] = "0123456789IONW";	/* number, In, Out, Normal, sWap */
  int i, show_status;
  char filename[80];
  char c;
  FILE *fp;
  char inpstr[COMMAND_LEN + 1];

  if (user->restrict[RESTRICT_RUN] == restrict_string[RESTRICT_RUN])
    {
      write_user (user,
		  ">>>You have no right to run a command file. Sorry...\n");
      return;
    }
  sprintf (filename, "%s/%s.%c", USERFILES, user->savename,
	   com_type_ext[type_command % MAX_RUNS]);
  show_status = user->level >= com_level[RUN];
  if (show_status)
    {
      sprintf (text, "~FY>>>Running %s... ", filename);
      write_user (user, text);
    }
  if ((fp = fopen (filename, "r")) == NULL)
    {
      if (show_status)
	write_user (user, "~FRnot found.\n");
      return;
    }
  if (show_status)
    write_user (user, "~FGOK.\n");
  inpstr[0] = '\0';
  c = getc (fp);
  i = 0;
  while (!feof (fp))
    {
      if (c == '\n')
	{
	  inpstr[i] = '\0';
	  if (inpstr[0] != '\0')
	    {
	      if (inpstr[0] != '#')
		{
		  clear_words ();
		  word_count = wordfind (user, inpstr);
		  com_num = -1;
		  exec_com (user, inpstr);
		}
	      inpstr[0] = '\0';
	      i = 0;
	    }
	}
      if (i < COMMAND_LEN)
	inpstr[i++] = c;
      c = getc (fp);
    }
  fclose (fp);
}

/*** Reply to last .tell user ***/
reply (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  UR_OBJECT u;
  if (user->muzzled)
    {
      write_user (user,
		  ">>>You are muzzled, you cannot reply anyone anything...\n");
      return;
    }
  if ((u = get_real_user (user->reply_user)) == NULL)
    {
      write_user (user, notloggedon);
      return;
    }
  if (u == user)
    {
      write_user (user,
		  ">>>Talking to yourself is the first sign of madness...or genius.\n");
      return;
    }
  if (word_count < 2)
    {
      write_user (user, ">>>Reply what?\n");
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  if (u->afk)
    {
      sprintf (text, ">>>%s is AFK at the moment... { ~FT%s~RS }\n", u->name,
	       u->afkmesg);
      write_user (user, text);
      return;
    }
  if (u->ignall && (user->level < SPIRIT || u->level > user->level))
    {
      if (u->malloc_start != NULL)
	sprintf (text, ">>>%s is using the editor at the moment.\n", u->name);
      else
	sprintf (text, ">>>%s is ignoring everyone at the moment.\n",
		 u->name);
      write_user (user, text);
      return;
    }
  if (u->igntell && (user->level < SPIRIT || u->level > user->level))
    {
      sprintf (text, ">>>%s is ignoring tells at the moment.\n", u->name);
      write_user (user, text);
      return;
    }
  if (u->room == NULL)
    {
      sprintf (text,
	       ">>>%s is offsite and would not be able to reply to you.\n",
	       u->name);
      write_user (user, text);
      return;
    }
  if (is_ignoring (u, user))
    {
      sprintf (text,
	       ">>>%s is ignoring you. You must ask him to forgive you first.\n",
	       u->name);
      write_user (user, text);
      return;
    }

  sprintf (text, "~FM~OL>>>%s replies to you%s%s\n",
	   user->vis ? user->name : invisname, tones[user->tell_tone],
	   inpstr);
  write_user (u, text);
  record_tell (u, text);
  strcpy (u->reply_user, user->savename);
  sprintf (text, "~FM~OL>>>You reply to %s:~RS %s\n", u->name, inpstr);
  write_user (user, text);
}

/*** Execute a command for other user ***/
execute (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  UR_OBJECT u;
  int cnt;
  char nm[USER_NAME_LEN + 1];

  if (word_count < 3)
    {
      write_usage (user, "execute <user> [<number>] <command>");
      return;
    }
  if ((u = get_user (word[1])) == NULL)
    {
      write_user (user, notloggedon);
      return;
    }
  if (u->level >= user->level)
    {
      write_user (user, ">>>You cannot execute commands for that user!\n");
      return;
    }
  if (u->login || u->room == NULL)
    {
      write_user (user, ">>>That user cannot execute this command!\n");
      return;
    }
  strcpy (nm, word[1]);
  inpstr = remove_first (inpstr);
  if (isnumber (word[2]))
    {
      cnt = atoi (word[2]);
      inpstr = remove_first (inpstr);
    }
  else
    cnt = 1;
  sprintf (text,
	   ">>>Command \"~FT%s~RS\" sent to be executed for ~FT%s~RS - ~FG%d~RS time(s)...\n",
	   inpstr, u->name, cnt);
  write_user (user, text);
  sprintf (text, "\"%s\" executed by %s for %s (%d times)\n", inpstr,
	   user->savename, u->savename, cnt);
  write_syslog (LOG_COM, text, LOG_TIME);
  while (cnt > 0)
    {
      clear_words ();
      word_count = wordfind (user, inpstr);
      com_num = -1;
      if ((u = get_user (nm)) == NULL)
	{
	  write_user (user, notloggedon);
	  return;
	}
      if ((u->ignall && u->malloc_start != NULL))
	{
	  sprintf (text,
		   ">>>%s is using the editor at the moment, cannot execute your commands.\n",
		   u->name);
	  write_user (user, text);
	  return;
	}
      exec_com (u, inpstr);
      cnt--;
    }
}

/*** Gossip to equal level users ***/
gossip (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  UR_OBJECT u;
  int lev;
  if (user->muzzled)
    {
      write_user (user,
		  ">>>You are muzzled, you cannot gossip anyone anything.\n");
      return;
    }
  if (word_count < 3)
    {
      write_usage (user, "gossip <level> <message>");
      return;
    }
  lev = atoi (word[1]);
  if (lev < GUEST || lev > SGOD)
    lev = user->level;
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  inpstr = remove_first (inpstr);
  sprintf (text, "~OL~FT>>>You gossip to level %s~RS: %s\n", level_name[lev],
	   inpstr);
  write_user (user, text);
  sprintf (text, "~OL~FT>>>%s gossips you: %s\n",
	   user->vis ? user->name : invisname, inpstr);
  for (u = user_first; u != NULL; u = u->next)
    if (u != user && u->level == lev && !u->login &&
	!u->ignall && u->type != CLONE_TYPE && !is_ignoring (u, user))
      write_user (u, text);
}

/*** view/change talker parameters ***/
parameters (user)
     UR_OBJECT user;
{				/* available parameters */
  char *params[] =
    { "col_default", "prompt_default", "max_users", "max_clones",
    "idle_tout", "remote_max", "remote_default", "stport_level",
    "tout_afks", "autosave_iter", "mesg_life", "safety", "max_quotes",
    "max_hints", "no_dis_mirror", "hosts_file", "words_dict",
    "use_gshell", "chk_spass", "autosave_action", "enable_events",
    "id_readtout", "tout_maxlev", "hint_at_help", "emote_spacer",
    "accept_prelog_comm", "allow_born_maxlev", "newuser_deflevel", "*"
  };

  char *wrong_val = ">>>Wrong value!\n";
  int val = 0, i;

  if (word_count < 3)
    {
      write_user (user, ">>>Current values of GAEN system parameters are:\n\n");
      i = 0;
      while (params[i][0] != '*')
	{
	  sprintf (text, "%24s", params[i]);
	  write_user (user, text);
	  switch (i)
	    {
	    case 0:
	      val = colour_def;
	      break;
	    case 1:
	      val = prompt_def;
	      break;
	    case 2:
	      val = max_users;
	      break;
	    case 3:
	      val = max_clones;
	      break;
	    case 4:
	      val = user_idle_time;
	      break;
	    case 5:
	      val = rem_user_maxlevel;
	      break;
	    case 6:
	      val = rem_user_deflevel;
	      break;
	    case 7:
	      val = stport_level;
	      break;
	    case 8:
	      val = time_out_afks;
	      break;
	    case 9:
	      val = autosave_maxiter;
	      break;
	    case 10:
	      val = mesg_life;
	      break;
	    case 11:
	      val = safety;
	      break;
	    case 12:
	      val = max_quotes;
	      break;
	    case 13:
	      val = max_hints;
	      break;
	    case 14:
	      val = no_disable_mirror;
	      break;
	    case 15:
	      val = use_hostsfile;
	      break;
	    case 16:
	      val = gm_max_words;
	      break;
	    case 17:
	      val = exec_gshell;
	      break;
	    case 18:
	      val = chk_spass_tests;
	      break;
	    case 19:
	      val = autosave_action;
	      break;
	    case 20:
	      val = enable_event;
	      break;
	    case 21:
	      val = id_readtimeout;
	      break;
	    case 22:
	      val = timeout_maxlevel;
	      break;
	    case 23:
	      val = hint_at_help;
	      break;
	    case 24:
	      val = emote_spacer;
	      break;
	    case 25:
	      val = accept_prelog_comm;
	      break;
	    case 26:
	      val = allow_born_maxlev;
	      break;
	    case 27:
	      val = newuser_deflevel;
	    }
	  sprintf (text, " ~FG%4d%c", val, i % 2 ? '\n' : ' ');
	  write_user (user, text);
	  i++;
	}
      write_user (user, "\n");
      return;
    }
  i = 0;
  while (strncmp (params[i], word[1], strlen (word[1])))
    {
      if (params[i][0] == '*')
	{
	  write_user (user, ">>>Unknown parameter, yet...\n");
	  return;
	}
      ++i;
    }
  val = atoi (word[2]);
  switch (i)
    {
    case 0:
      if (val < 0 || val > 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      colour_def = val;
      break;
    case 1:
      if (val < 0 || val > 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      prompt_def = val;
      break;
    case 2:
      if (val < 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      max_users = val;
      break;
    case 3:
      if (val < 0)
	{
	  write_user (user, wrong_val);
	  return;
	}
      max_clones = val;
      break;
    case 4:
      if (val < 10)
	{
	  write_user (user, wrong_val);
	  return;
	}
      user_idle_time = val;
      break;
    case 5:
      if (val < GUEST || val > SGOD)
	{
	  write_user (user, wrong_val);
	  return;
	}
      rem_user_maxlevel = val;
      break;
    case 6:
      if (val < GUEST || val > SGOD)
	{
	  write_user (user, wrong_val);
	  return;
	}
      rem_user_deflevel = val;
      break;
    case 7:
      if (val < GUEST || val > SGOD)
	{
	  write_user (user, wrong_val);
	  return;
	}
      stport_level = val;
      break;
    case 8:
      if (val < 0 || val > 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      time_out_afks = val;
      break;
    case 9:
      if (val < 0)
	{
	  write_user (user, wrong_val);
	  return;
	}
      autosave_maxiter = val;
      break;
    case 10:
      if (val < 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      mesg_life = val;
      break;
    case 11:
      if (val < 0 || val > 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      safety = val;
      break;
    case 12:
      if (val <= 0)
	{
	  write_user (user, wrong_val);
	  return;
	}
      max_quotes = val;
      break;
    case 13:
      if (val <= 0)
	{
	  write_user (user, wrong_val);
	  return;
	}
      max_hints = val;
      break;
    case 14:
      if (val < 0 || val > 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      no_disable_mirror = val;
      break;
    case 15:
      if (val < 0 || val > 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      use_hostsfile = val;
      break;
    case 16:
      if (val <= 0)
	{
	  write_user (user, wrong_val);
	  return;
	}
      gm_max_words = val;
      break;
    case 17:
      if (val < 0 || val > 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      exec_gshell = val;
      break;
    case 18:
      if (val < CHK_SPASS_NONE || val > CHK_SPASS_ALL)
	{
	  write_user (user, wrong_val);
	  return;
	}
      chk_spass_tests = val;
      break;
    case 19:
      if (val < AS_NONE || val > AS_ALL)
	{
	  write_user (user, wrong_val);
	  return;
	}
      autosave_action = val;
      break;
    case 20:
      if (val < 0 || val > 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      enable_event = val;
      break;
    case 21:
      if (val < 0)
	{
	  write_user (user, wrong_val);
	  return;
	}
      id_readtimeout = val;
      break;
    case 22:
      if (val < GUEST || val > SGOD)
	{
	  write_user (user, wrong_val);
	  return;
	}
      timeout_maxlevel = val;
      break;
    case 23:
      if (val < 0 || val > 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      hint_at_help = val;
      break;
    case 24:
      if (val < 0 || val > 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      emote_spacer = val;
      break;
    case 25:
      if (val < 0 || val > 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      accept_prelog_comm = val;
      break;
    case 26:
      if (val < 0 || val > 1)
	{
	  write_user (user, wrong_val);
	  return;
	}
      allow_born_maxlev = val;
      break;
    case 27:
      if (val < GUEST || val > MAX_LEV_NEWUSER)
	{
	  write_user (user, wrong_val);
	  return;
	}
      newuser_deflevel = val;
      break;
    }
  sprintf (text, ">>>Parameter ~FT%s = %d~RS.\n", params[i], val);
  write_user (user, text);
  sprintf (text, "~OLGAEN:~RS Parameter %s = %d (changed by %s).\n",
	   params[i], val, user->savename);
  write_syslog (LOG_CHAT, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
}

/*** Hide a sky ***/
hidesky (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  if (word_count < 2)
    rm = user->room;
  else if ((rm = get_room (word[1])) == NULL)
    {
      write_user (user, nosuchroom);
      return;
    }
  if (word_count > 2 && !strcmp (word[2], "board"))
    {
      if (rm->hidden_board)
	{
	  write_user (user, ">>>That sky's board is already hidden!\n");
	  return;
	}
      rm->hidden_board = user->level;
      sprintf (text, ">>>%s covers %s's board with a dark cloud...\n",
	       user->vis ? user->name : invisname, rm->name);
      write_level (SAINT, text, user);
      write_user (user, ">>>Message board hidden.\n");
      return;
    }
  if (rm->hidden)
    {
      write_user (user, ">>>That sky is already hidden!\n");
      return;
    }
  rm->hidden = user->level;
  sprintf (text, ">>>%s covers %s with a dark cloud...\n",
	   user->vis ? user->name : invisname, rm->name);
  write_level (SAINT, text, user);
  write_user (user, ">>>Sky hidden.\n");
}

/*** Show a sky ***/
showsky (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  if (word_count < 2)
    rm = user->room;
  else if ((rm = get_room (word[1])) == NULL)
    {
      write_user (user, nosuchroom);
      return;
    }
  if (word_count > 2 && !strcmp (word[2], "board"))
    {
      if (!rm->hidden_board)
	{
	  write_user (user, ">>>That sky's board is not hidden!\n");
	  return;
	}
      if (rm->hidden_board > user->level)
	{
	  write_user (user,
		      ">>>Your level is too lower to show that board!\n");
	  return;
	}
      rm->hidden_board = 0;
      sprintf (text, ">>>%s shows agian %s's board with a dark cloud...\n",
	       user->vis ? user->name : invisname, rm->name);
      write_level (SAINT, text, user);
      write_user (user, ">>>Message board unhidden.\n");
      return;
    }
  if (!rm->hidden)
    {
      write_user (user, ">>>That sky is not hidden!\n");
      return;
    }
  if (rm->hidden > user->level)
    {
      write_user (user, ">>>Your level is too lower to show that sky!\n");
      return;
    }
  rm->hidden = 0;
  sprintf (text, ">>>%s shows again %s...\n",
	   user->vis ? user->name : invisname, rm->name);
  write_level (SAINT, text, user);
  write_user (user, ">>>Sky unhidden.\n");
}

/*** Change the 'tone' of the voice ***/
tone (user)
     UR_OBJECT user;
{
  int i, n, say_mode;
  if (word_count < 2)
    {
      user->say_tone = 0;
      user->tell_tone = 0;
      write_user (user, ">>>Say/tell tone set to normal.\n");
      return;
    }
  if (word_count == 2)
    say_mode = TN_BOTH;
  else
    {
      if (!strcmp (word[2], "say"))
	say_mode = TN_SAY;
      else if (!strcmp (word[2], "tell"))
	say_mode = TN_TELL;
      else
	say_mode = TN_BOTH;
    }
  if (isnumber (word[1]))
    {
      i = atoi (word[1]) % MAX_TONES;
      switch (say_mode)
	{
	case TN_SAY:
	  user->say_tone = i;
	  sprintf (text, ">>>You'll say%s <message>\n", tones[i]);
	  break;
	case TN_TELL:
	  user->tell_tone = i;
	  sprintf (text, ">>>You'll tell%s<message>\n", tones[i]);
	  break;
	case TN_BOTH:
	  user->say_tone = i;
	  user->tell_tone = i;
	  sprintf (text, ">>>You'll say/tell%s<message>\n", tones[i]);
	}
      write_user (user, text);
      return;
    }
  n = strlen (word[1]);
  for (i = 0; i < MAX_TONES; i++)
    if (!strncmp (tone_codes[i], word[1], n))	/* found */
      {
	switch (say_mode)
	  {
	  case TN_SAY:
	    user->say_tone = i;
	    sprintf (text, ">>>You'll say%s<message>\n", tones[i]);
	    break;
	  case TN_TELL:
	    user->tell_tone = i;
	    sprintf (text, ">>>You'll tell%s<message>\n", tones[i]);
	    break;
	  case TN_BOTH:
	    user->say_tone = i;
	    user->tell_tone = i;
	    sprintf (text, ">>>You'll say/tell%s<message>\n", tones[i]);
	  }
	write_user (user, text);
	return;
      }
  write_user (user, ">>>Unknown tone, yet...\n");
}

/*** List user macros ***/
lmacros (user)
     UR_OBJECT user;
{
  int i;

  if (user->macros)
    {
      write_user (user, "\n~FB~OL--->>> Your macros <<<---\n\n");
      for (i = 0; i < user->macros; i++)
	{
	  sprintf (text, "\t~FT%3s~RS : %s\n", user->macro_name[i],
		   user->macro_desc[i]);
	  write_user (user, text);
	}
    }
  else
    write_user (user, ">>>No defined macros!\n");
  return;
}

/*** Define new macros ***/
addmacro (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  if (word_count < 3)
    {
      write_usage (user, "addmacro <macro-name> <description>");
      return;
    }
  if (user->macros >= MAX_USER_MACROS)
    {
      write_user (user, ">>>You cannot define another macro!\n");
      return;
    }
  if (strlen (word[1]) > MAX_LEN_MACRONAME)
    word[1][MAX_LEN_MACRONAME] = '\0';
  strcpy (user->macro_name[user->macros], word[1]);
  inpstr = remove_first (inpstr);
  if (strlen (inpstr) > MAX_LEN_MACRODESC)
    inpstr[MAX_LEN_MACRODESC] = '\0';
  strcpy (user->macro_desc[user->macros], inpstr);
  user->macros++;
  write_user (user, ">>>Macro added.\n");
}

/*** Use user macros ***/
umacro (user)
     UR_OBJECT user;
{
  int i;
  if (word_count < 2)
    {
      write_usage (user, "umacro <macro-name>");
      return;
    }
  for (i = 0; i < user->macros; i++)
    {
      if (!strcmp (word[1], user->macro_name[i]))
	{
	  say (user, user->macro_desc[i]);
	  return;
	}
    }
  write_user (user, ">>>Unknown macro name, yet...\n");
}

/*** delete user macro ***/
dmacro (user)
     UR_OBJECT user;
{
  int i;
  if (word_count < 2)
    {
      write_usage (user, "dmacro <macro-name>/all");
      return;
    }
  if (!strcmp (word[1], "all"))
    {
      for (i = 0; i < MAX_USER_MACROS; i++)
	{
	  user->macro_name[i][0] = '\0';
	  user->macro_desc[i][0] = '\0';
	}
      user->macros = 0;
      write_user (user, ">>>All macros deleted!\n");
      return;
    }
  for (i = 0; i < user->macros; i++)
    {
      if (!strcmp (word[1], user->macro_name[i]))
	{
	  strcpy (user->macro_name[i], user->macro_name[user->macros - 1]);
	  strcpy (user->macro_desc[i], user->macro_desc[user->macros - 1]);
	  user->macro_name[user->macros - 1][0] = '\0';
	  user->macro_desc[user->macros - 1][0] = '\0';
	  user->macros--;
	  write_user (user, ">>>Macro deleted!\n");
	  return;
	}
    }
  write_user (user, ">>>Macro not found!\n");
}

/*** if user guess a number, he/she win some restrictions... ***/
guess (user)
     UR_OBJECT user;
{
  int number;
  if (word_count < 2)
    {
      write_usage (user, "guess <number>");
      return;
    }
  if (!isnumber (word[1]))
    {
      write_user (user, ">>>Wrong parameter!\n");
      return;
    }
  number = random () % GUESS_RND;
  sprintf (text, "~OLGAEN:~RS Generated number is... ~FM%d~RS.\n", number);
  write_user (user, text);
  if (number == atoi (word[1]))
    {
      number = random () % MAX_RESTRICT;
      user->restrict[number] = restrict_string[number];
      sprintf (text,
	       "~FT~OL>>>Congratulations! Now, you have set ~LI%c~RS~FT~OL restriction...\n",
	       restrict_string[number]);
      write_user (user, text);
      sprintf (text, "Guess: %c restriction for %s.\n",
	       restrict_string[number], user->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      sprintf (text, "~FT~OL>>>%s was lucky!\n",
	       user->vis ? user->name : invisname);
      write_room_except (NULL, text, user);
      return;
    }
  /* clear all restrictions except one */
  for (number = 0; number < MAX_RESTRICT; number++)
    {
      if (number == RESTRICT_SUIC)
	continue;
      user->restrict[number] = '.';
    }
  write_room (NULL,
	      "~FR~OL>>>The dark snake comes the malefic energies...\n");
  sprintf (text, "%s KILLED by .guess\n", user->savename);
  write_syslog (LOG_COM, text, LOG_TIME);
  sprintf (text,
	   "~FR~OL>>>The voice of thunder poisons the %s's mind with madness!...\n",
	   user->name);
  write_room (NULL, text);
  disconnect_user (user);
  write_room (NULL,
	      "~FR~OL>>>You hear insane laughter from the hell's doors...\n");
}

/*** whisper ( define, list, tell ) ***/
whisper (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  int i, j;
  UR_OBJECT u, u2;
  char *name;
  if (word_count < 2)
    {
      write_usage (user,
		   "whisper <text>/+ <user1>...<user10>/- [<user>...]/? [<user>]");
      return;
    }
  if (word[1][0] == '?')	/* view whisper list */
    {
      if (word_count < 3)
	u = user;
      else if ((u = get_user (word[2])) == NULL)
	{
	  write_user (user, nosuchuser);
	  return;
	}
      sprintf (text, "\n~FB~OL--->>> Whisper info for %s~RS~FB~OL <<<---\n\n",
	       u->name);
      write_user (user, text);
      if (!user->w_number)
	{
	  write_user (user, ">>>The whisper list is empty.\n");
	}
      else
	{
	  write_user (user, ">>>Whisper users:\n");
	  for (i = 0; i < u->w_number; i++)
	    {
	      sprintf (text, "\t~FT%s\n", u->w_users[i]);
	      write_user (user, text);
	    }
	}
      write_user (user, "\n>>>Presence in other whisper lists...");
      i = 0;
      for (u2 = user_first; u2 != NULL; u2 = u2->next)
	{
	  if (u2->type == CLONE_TYPE || !u2->w_number || u == u2)
	    continue;
	  for (j = 0; j < u2->w_number; j++)
	    if (strstr (u->name, u2->w_users[j])
		|| strstr (u->savename, u2->w_users[j]))	/* found */
	      {
		sprintf (text, "\n\t~FT%s", u2->name);
		write_user (user, text);
		i++;
	      }
	}
      if (i)
	write_user (user, "\n");
      else
	write_user (user, " none.\n");
      return;
    }
  if (word[1][0] == '+')	/* add to whisper list */
    {
      for (i = 2; i < word_count; i++)
	{
	  if (user->w_number >= MAX_WUSERS)
	    {
	      write_user (user,
			  ">>>Maximum capacity of whisper list reached! Sorry...\n");
	      break;
	    }
	  if (strlen (word[i]) > USER_NAME_LEN)
	    word[i][USER_NAME_LEN] = '\0';
	  word[i][0] = toupper (word[i][0]);
	  if ((u = get_user (word[i])) != NULL)
	    {
	      sprintf (text,
		       ">>>You were added to %s's whisper list...\n>>>To view the list of whisper partners, type ~FT.whisper ? %s~RS...\n",
		       user->name, user->name);
	      write_user (u, text);
	      strcpy (word[i], u->name);
	    }
	  strcpy (user->w_users[user->w_number], word[i]);
	  user->w_number++;
	  sprintf (text, ">>>%s added to whisper list.\n", word[i]);
	  write_user (user, text);
	}
      for (i = 0; i < user->w_number; i++)
	if ((u = get_user (user->w_users[i])) != NULL)
	  {
	    sprintf (text, "\n~OLGAEN:~RS %s's whisper list was changed.\n",
		     user->name);
	    write_user (u, text);
	  }
      return;
    }
  if (word[1][0] == '-')	/* remove from whisper list */
    {
      if (word_count < 3)
	{
	  if (!user->w_number)
	    {
	      write_user (user, ">>>Whisper list is already empty.\n");
	      return;
	    }
	  for (i = 0; i < user->w_number; i++)
	    {
	      if ((u = get_user (user->w_users[i])) != NULL)
		{
		  sprintf (text,
			   ">>>You are removed from %s's whisper list...\n>>>%s was removed all whisper partners.\n",
			   user->name, user->name);
		  write_user (u, text);
		}
	    }
	  user->w_number = 0;
	  for (i = 0; i < MAX_WUSERS; i++)
	    user->w_users[i][0] = '\0';
	  write_user (user, ">>>Your whisper list is now empty.\n");
	  return;
	}
      if (strlen (word[2]) > USER_NAME_LEN)
	word[2][USER_NAME_LEN] = '\0';
      word[2][0] = toupper (word[2][0]);
      /* first, we'll see if user is in whisper list */
      if (!user->w_number)
	{
	  write_user (user, ">>>Your whisper list is already empty...\n");
	  return;
	}
      for (i = 0; i < user->w_number; i++)
	{
	  if (!strcmp (word[2], user->w_users[i]))	/* found */
	    {
	      if ((u = get_user (word[2])) != NULL)
		{
		  sprintf (text,
			   ">>>You were removed from %s's whisper list...\n",
			   user->name);
		  write_user (u, text);
		}
	      strcpy (user->w_users[i], user->w_users[user->w_number - 1]);
	      user->w_users[user->w_number - 1][0] = '\0';
	      user->w_number--;
	      write_user (user, ">>>User removed from your whisper list.\n");
	      for (i = 0; i < user->w_number; i++)
		if ((u = get_user (user->w_users[i])) != NULL)
		  {
		    sprintf (text,
			     "\n~OLGAEN:~RS %s's whisper list was changed.\n",
			     user->name);
		    write_user (u, text);
		  }
	      return;
	    }
	}
      write_user (user, ">>>User not found.\n");
      return;
    }
  /* send whispers... */
  if (!user->w_number)
    {
      write_user (user, ">>>The whisper list is empty.\n");
      return;
    }
  if (user->muzzled)
    {
      write_user (user,
		  ">>>You are muzzled, you cannot whisper anyone anything...:-(\n");
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  for (i = 0; i < user->w_number; i++)
    {
      if (!(u = get_user (user->w_users[i])))
	{
	  write_user (user, notloggedon);
	  continue;
	}
      if (u == user)
	{
	  write_user (user,
		      ">>>Whispering to yourself is the fourteenth sign of madness... or genius.\n");
	  continue;
	}
      if (u->afk)
	{
	  sprintf (text, ">>>%s is AFK at the moment... { ~FT%s~RS }\n",
		   u->name, u->afkmesg);
	  write_user (user, text);
	  continue;
	}
      if (u->ignall && (user->level < SPIRIT || u->level > user->level))
	{
	  if (u->malloc_start != NULL)
	    sprintf (text, ">>>%s is using the editor at the moment.\n",
		     u->name);
	  else
	    sprintf (text, ">>>%s is ignoring everyone at the moment.\n",
		     u->name);
	  write_user (user, text);
	  continue;
	}
      if (u->igntell && (user->level < SPIRIT || u->level > user->level))
	{
	  sprintf (text, ">>>%s is ignoring tells at the moment.\n", u->name);
	  write_user (user, text);
	  continue;
	}
      if (u->room == NULL)
	{
	  sprintf (text,
		   ">>>%s is offsite and would not be able to reply to you.\n",
		   u->name);
	  write_user (user, text);
	  continue;
	}
      if (is_ignoring (u, user))
	{
	  sprintf (text,
		   ">>>%s is ignoring you. You must ask him to forgive you first.\n",
		   u->name);
	  write_user (user, text);
	  continue;
	}

      sprintf (text, "~OL~FY>>>You whisper %s:~RS %s\n", u->name, inpstr);
      write_user (user, text);
      sprintf (text, "%s~OL~FY>>>%s whispers you%s%s\n",
	       user->stereo ? stereo_str[random () % MAX_STEREO] : "", name,
	       tones[user->tell_tone], inpstr);
      write_user (u, text);
      record_tell (u, text);
    }
}


/*** Change mirror of the chat ***/
mirror_chat (user)
     UR_OBJECT user;
{
  int i, no_opt = 1;
  UR_OBJECT u;
  char filename[80];
  char mirrors[MAX_MIRRORS][5] = { "EDEN", "GAEN", "HELL", "DILI" };
  event = E_MIRROR;
  strcpy (event_var, "*");
  if (word_count >= 2)
    {
      strtoupper (word[1]);
      for (i = 0; i < MAX_MIRRORS; i++)
	{
	  if (word[1][0] == mirrors[i][0] || !strcmp (word[1], mirrors[i]))	/* found */
	    {
	      mirror = i;
	      no_opt = 0;
	      break;
	    }
	}
      if (no_opt)
	{
	  if (user)
	    write_usage (user, "mirror <name>");
	  return;
	}
    }
  else
    {
      mirror++;
      mirror %= MAX_MIRRORS;
    }
  sprintf (text, "Mirror changed into %s by %s\n", mirrors[mirror],
	   user == NULL ? "GAEN" : user->savename);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_room (NULL, "~FR~OL>>>A mirror appears from the Emptyness...\n");
  switch (mirror)
    {
    case 2:			/* HELL mirror */
      for (i = 0; i <= MAX_LEVELS; i++)
	strcpy (level_name[i], level_name_mirror[2][i]);
      strcpy (invisname, "A flame");
      strcpy (invisenter, "~FR>>>An evil flame enters the sky...\n");
      strcpy (invisleave, "~FR>>>An evil flame leaves the sky...\n");
      sprintf (filename, "%s/%s", DATAFILES, MIRRORFILE);
      for (u = user_first; u != NULL; u = u->next)
	{
	  if (u->type == CLONE_TYPE || u->ignall || u->login)
	    continue;
	  switch (more (u, u->socket, filename))
	    {
	    case 0:
	      sprintf (text, "Fail to show %s in mirror().\n", filename);
	      write_syslog (LOG_ERR, text, LOG_TIME);
	      break;
	    case 1:
	      u->misc_op = 2;
	    }
	}
      write_room (NULL,
		  "~FY~OL>>>...and the unknown power of the universe transforms ~FG~OLGAEN~RS~FY~OL into ~FR~OLHELL~RS~FY~OL!!\n");
      write_room (NULL, "~FR~OL>>>An evil flame screams: ~LIWelcome!\n");
      return;
    case 0:			/* EDEN mirror */
      for (i = 0; i <= MAX_LEVELS; i++)
	strcpy (level_name[i], level_name_mirror[0][i]);
      strcpy (invisname, "A light");
      strcpy (invisenter, "~FT>>>A light enters the sky...\n");
      strcpy (invisleave, "~FT>>>A light leaves the sky...\n");
      sprintf (filename, "%s/%s", DATAFILES, EDENFILE);
      for (u = user_first; u != NULL; u = u->next)
	{
	  if (u->type == CLONE_TYPE || u->ignall || u->login)
	    continue;
	  switch (more (u, u->socket, filename))
	    {
	    case 0:
	      sprintf (text, "Fail to show %s in mirror().\n", filename);
	      write_syslog (LOG_ERR, text, LOG_TIME);
	      break;
	    case 1:
	      u->misc_op = 2;
	    }
	}
      write_room (NULL,
		  "~FY~OL>>>...and the unknown power of the universe transforms ~FG~OLGAEN~RS~FY~OL into ~FG~OLEDEN~RS~FY~OL!!\n");
      write_room (NULL, "~FY~OL>>>A light sings: ~LIWelcome!\n");
      return;
    case 1:			/* GAEN mirror */
      for (i = 0; i <= MAX_LEVELS; i++)
	strcpy (level_name[i], level_name_mirror[1][i]);
      strcpy (invisname, "A digit");
      strcpy (invisenter, "~FT>>>A digit enters the sky...\n");
      strcpy (invisleave, "~FT>>>A digit leaves the sky...\n");
      write_room (NULL,
		  ">>>...and the unknown power of the universe build a new order: ~FG~OLGAEN~RS!\n");
      return;
    case 3:			/* DILI mirror */
      for (i = 0; i <= MAX_LEVELS; i++)
	strcpy (level_name[i], level_name_mirror[3][i]);
      strcpy (invisname, "A psycho");
      strcpy (invisenter, "~FT>>>A psycho enters the sky...\n");
      strcpy (invisleave, "~FT>>>A psycho leaves the sky...\n");
      write_room (NULL,
		  ">>>...and the unknown power of the universe...\n>>>...transforms ~FG~OLGAEN~RS into a new chat: ~FM~OLDILI~RS!\n");
      write_room (NULL, "~FM~OL>>>A psycho smiles to you: ~LIWelcome!\n");
      return;
    }
}

/*** Lock an user ***/
lock_user (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char lock_msg[AFK_MESG_LEN];

  cls (user);
  if (strlen (inpstr) > AFK_MESG_LEN - 20)
    inpstr[AFK_MESG_LEN] = '\0';
  strcpy (lock_msg, (inpstr[0] && !user->muzzled) ? inpstr : AFK_DEF_MSG);

  sprintf (text, ">>>To unlock your session { %s }, type your password...\n",
	   lock_msg);
  write_user (user, text);
  sprintf (text, "~FT>>>%s locks the session { %s }...\n",
	   user->vis ? user->name : invisname, lock_msg);
  write_room_except (NULL, text, user);
  user->ignall_store = user->ignall;
  user->ignall = 1;
  user->afk = 1;
  user->blind = 1;
  event = E_AFK;
  strcpy (event_var, user->savename);
  sprintf (user->afkmesg, "~ULLocked:~RS %s", lock_msg);
  echo_off (user);
  user->misc_op = 9;
}

/*** set/reset restrictions ***/
restrict (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  int i, no_exists = 0;
  char *restrict_name[] = { "go", "move", "promote", "demote", "muzzle",
    "unmuzzle", "kill", "help", "suicide", "who",
    "run", "clone", "review", "execmds"
  };
  if (word_count < 2)
    {
      write_usage (user, "restrict <user> [<restrict_string>]");
      write_usage (user, "restrict <user> +/- <restriction>");
      return;
    }
  if ((u = get_user (word[1])) == NULL)
    {
      /* User not logged on */
      if ((u = create_user ()) == NULL)
	{
	  sprintf (text, ">>>%s: unable to create temporary user object.\n",
		   syserror);
	  write_user (user, text);
	  write_syslog (LOG_ERR,
			"ERROR: Unable to create temporary user object in restrict().\n",
			LOG_NOTIME);
	  return;
	}
      strcpy (u->savename, word[1]);
      if (!load_user_details (u))
	{
	  write_user (user, nosuchuser);
	  destruct_user (u);
	  destructed = 0;
	  return;
	}
      no_exists = 1;
    }
  if (word_count == 2)		/* list user's restrictions */
    {
      sprintf (text, "\n~FB~OL--->>> %s's restrictions <<<---\n\n",
	       u->savename);
      write_user (user, text);
      for (i = 0; i < MAX_RESTRICT; i++)
	{
	  sprintf (text, ">>>%-8s (%c) restriction... %s\n",
		   restrict_name[i], restrict_string[i],
		   u->restrict[i] ==
		   restrict_string[i] ? "~FGset" : "~FRreset");
	  write_user (user, text);
	}
      if (no_exists)
	{
	  destruct_user (u);
	  destructed = 0;
	}
      return;
    }
  if (u->level >= user->level)
    {
      write_user (user, ">>>You cannot use this command for this user!\n");
      if (no_exists)
	{
	  destruct_user (u);
	  destructed = 0;
	}
      return;
    }
  if (word_count >= 4)		/* set only a single restriction */
    {
      if (word[2][0] != '+' && word[2][0] != '-')
	{
	  write_usage (user, "restrict <user> +/- <restriction>");
	  if (no_exists)
	    {
	      destruct_user (u);
	      destructed = 0;
	    }
	  return;
	}
      strtoupper (word[3]);
      for (i = 0; i < MAX_RESTRICT; i++)
	if (restrict_string[i] == word[3][0])
	  {
	    u->restrict[i] = (word[2][0] == '+') ? restrict_string[i] : '.';
	    sprintf (text, ">>>%s's %s (%c) restriction set to %s.\n",
		     u->savename, restrict_name[i], restrict_string[i],
		     offon[word[2][0] == '+']);
	    write_user (user, text);
	    sprintf (text,
		     "~OLGAEN:~RS %s turn %s restriction %s (%c) for %s.\n",
		     user->savename, offon[word[2][0] == '+'],
		     restrict_name[i], restrict_string[i], u->savename);
	    write_syslog (LOG_COM, text, LOG_TIME);
	    write_level (MIN_LEV_NOTIFY, text, user);
	    if (no_exists)
	      {
		save_user_details (u, 0);
		destruct_user (u);
		destructed = 0;
	      }
	    return;
	  }
      write_user (user, ">>>Unknown restriction, yet!...\n");
      return;
    }
  if (strlen (word[2]) != MAX_RESTRICT)	/* global settings */
    {
      sprintf (text,
	       ">>>Restrictions string must has exactly ~FT%d~RS characters!\n",
	       MAX_RESTRICT);
      write_user (user, text);
      if (no_exists)
	{
	  destruct_user (u);
	  destructed = 0;
	}
      return;
    }
  for (i = 0; i < MAX_RESTRICT; i++)
    if (word[2][i] == restrict_string[i])
      {
	u->restrict[i] = restrict_string[i];
	sprintf (text, ">>>Restriction %-8s (%c) ~FGset~RS for %s...\n",
		 restrict_name[i], restrict_string[i], u->savename);
	write_user (user, text);
      }
    else if (word[2][i] == '?')
      continue;
    else
      {
	u->restrict[i] = '.';
	sprintf (text, ">>>Restriction %-8s (%c) ~FRreset~RS for %s...\n",
		 restrict_name[i], restrict_string[i], u->savename);
	write_user (user, text);
      }
  sprintf (text, "~OLGAEN:~RS Restrictions for user %s changed by %s: %s\n",
	   u->savename, user->savename, u->restrict);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
  if (no_exists)
    {
      save_user_details (u, 0);
      destruct_user (u);
      destructed = 0;
    }
}

/*** show map(s) ***/
showmap (user)
     UR_OBJECT user;
{
  int i;
  char filename[80];
  if (has_unread_mail (user))
    write_user (user, "~EL~FT~OL~LI--->>> UNREAD MAIL! <<<---\n");
  if (word_count < 2)
    {
      sprintf (text,
	       "~FB~OL--->>> Current dimension %d ( %s~RS~FB~OL ) <<<---\n\n",
	       user->dimension + 1, dimens_name[user->dimension]);
      write_user (user, text);
      sprintf (filename, "%s/%s%d", DATAFILES, MAPFILE, user->dimension);
      switch (more (user, user->socket, filename))
	{
	case 0:
	  write_user (user,
		      ">>>There is no map. Sorry... Try ~FG~OL.look~RS command.\n");
	  break;
	case 1:
	  user->misc_op = 2;
	}
      return;
    }
  if (isnumber (word[1]))
    {
      i = atoi (word[1]) - 1;
      if (i < 0 || i >= MAX_DIMENS)
	i = user->dimension;
      sprintf (text, "~FB~OL--->>> Dimension %d ( %s~RS~FB~OL ) <<<---\n\n",
	       i + 1, dimens_name[i]);
      write_user (user, text);
      sprintf (filename, "%s/%s%d", DATAFILES, MAPFILE, i);
      switch (more (user, user->socket, filename))
	{
	case 0:
	  write_user (user,
		      ">>>There is no map. Sorry... Try ~FG~OL.look~RS command.\n");
	  break;
	case 1:
	  user->misc_op = 2;
	}
      return;
    }
  if (!strcmp (word[1], "all"))
    {
      write_user (user, "~FB~OL--->>> All dimensions <<<---\n");
      for (i = MAX_DIMENS - 1; i >= 0; i--)
	{
	  sprintf (filename, "%s/%s%d", DATAFILES, MAPFILE, i);
	  switch (more (user, user->socket, filename))
	    {
	    case 0:
	      write_user (user,
			  ">>>There is no map. Sorry... Try ~FG~OL.look~RS command.\n");
	      break;
	    case 1:
	      user->misc_op = 2;
	    }
	}
      return;
    }
  else
    write_usage (user, "map [ <dimension>/all ]");
}

/*** List all users who are AFK ***/
listafks (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  int afks = 0;
  write_user (user, "~FB~OL--->>> AFK users <<<---\n\n");
  for (u = user_first; u != NULL; u = u->next)
    {
      if (!u->afk || (!u->vis && u->level > user->level))
	continue;
      sprintf (text, "   %-16s : { ~FT%s~RS } - idle for ~FT%d~RS minutes\n",
	       u->name, u->afkmesg, (int) (time (0) - u->last_input) / 60);
      write_user (user, text);
      afks++;
    }
  if (afks)
    {
      sprintf (text, "\n>>>Total of ~FT%d~RS AFK users.\n", afks);
      write_user (user, text);
    }
  else
    write_user (user, ">>>There are no AFK users.\n");
}

/*** set an entry watch for an user ***/
watch (user)
     UR_OBJECT user;
{
  if (word_count < 2)
    {
      write_usage (user, "watch <user> [<digit>]");
      return;
    }
  word[1][0] = toupper (word[1][0]);
  if (strlen (word[1]) > USER_NAME_LEN)
    {
      write_user (user, ">>>User name too long!\n");
      return;
    }
  if (get_user (word[1]) != NULL || get_real_user (word[1]) != NULL)
    {
      write_user (user, ">>>This user is already here!\n");
      return;
    }
  if (watch_pos >= MAX_WATCH)
    {
      write_user (user,
		  ">>>Sorry, but you cannot watch anyone! Try later or delete a watch entry.\n");
      return;
    }
  watch_pos++;
  watch_runcom[watch_pos] = -1;
  if (word_count == 3)
    {
      if (!isnumber (word[2]))
	write_user (user, ">>>Invalid number of run commands file!\n");
      else
	watch_runcom[watch_pos] = word[2][0] - '0';
    }
  strcpy (watch_source[watch_pos], user->savename);
  strcpy (watch_dest[watch_pos], word[1]);
  sprintf (text, ">>>~FG~OLGAEN~RS will wake you when %s will be here.\n",
	   word[1]);
  write_user (user, text);
}

/*** list watch list ***/
lwatch (user)
     UR_OBJECT user;
{
  int i, j = 0;
  if (watch_pos < 0)
    {
      write_user (user, ">>>There are no watch entries.\n");
      return;
    }
  if (word_count >= 2)
    {
      if (strcmp (word[1], "all"))
	{
	  write_usage (user, "lwatch [ all ]");
	}
      else
	{
	  write_user (user, "\n~FB~OL--->>> Watch list <<<---\n\n");
	  for (i = 0; i <= watch_pos; i++)
	    {
	      sprintf (text, "%-16s is watched by %s (run file: %c)\n",
		       watch_dest[i], watch_source[i],
		       watch_runcom[i] >= 0 ? watch_runcom[i] + '0' : '-');
	      write_user (user, text);
	    }
	  sprintf (text, ">>>Total of ~FT%d~RS entries found.\n",
		   watch_pos + 1);
	  write_user (user, text);
	}
      return;
    }
  for (i = 0; i <= watch_pos; i++)
    {
      if (!strcmp (watch_source[i], user->savename))	/* found */
	{
	  j++;
	  if (j == 1)
	    write_user (user,
			"\n~FB~OL--->>> Your watch entries list <<<---\n\n");
	  sprintf (text, "\t%-16s (run file: %c)\n", watch_dest[i],
		   watch_runcom[i] >= 0 ? watch_runcom[i] + '0' : '-');
	  write_user (user, text);
	}
    }
  if (j)
    {
      sprintf (text, ">>>Total of ~FT%d~RS entries found.\n", j);
      write_user (user, text);
    }
  else
    write_user (user, ">>>No watch entries found.\n");
}

/*** check if the user is in watching list ***/
check_watch (user)
     UR_OBJECT user;
{
  int i, j;
  UR_OBJECT u;
  i = 0;
  while (i <= watch_pos)
    {
      if (!strcmp (user->savename, watch_dest[i]))
	{
	  sprintf (text, "~EL~FR~OL~LI>>>You were watched by %s!...\n",
		   watch_source[i]);
	  write_user (user, text);
	  if ((u = get_user (watch_source[i])) != NULL)
	    {
	      sprintf (text,
		       "~EL~FR~OL~LI>>>Wake up! %s is here, ready to talk to you!\n",
		       user->savename);
	      write_user (u, text);
	      if (watch_runcom[i] >= 0)
		run_commands (u, watch_runcom[i]);
	    }
	  for (j = i; j < watch_pos; j++)	/* delete this entry */
	    {
	      strcpy (watch_source[j], watch_source[j + 1]);
	      strcpy (watch_dest[j], watch_dest[j + 1]);
	      watch_runcom[j] = watch_runcom[j + 1];
	    }
	  watch_source[watch_pos][0] = '\0';
	  watch_dest[watch_pos][0] = '\0';
	  watch_runcom[watch_pos] = -1;
	  watch_pos--;
	}
      else
	i++;
    }
}

/*** delete entries from the watch list ***/
dwatch (user)
     UR_OBJECT user;
{
  int i, j;
  int counter = 0;

  if (word_count < 2)
    {
      write_usage (user, "dwatch <user>/all");
      return;
    }
  if (!strcmp (word[1], "all"))
    {
      clear_watch ();
      write_user (user, ">>>All watch entries were deleted!\n");
      sprintf (text, ">>>%s was deleted all watch entries.\n",
	       user->vis ? user->name : invisname);
      write_room (NULL, text);
      sprintf (text, "~OLGAEN:~RS %s was deleted all watch entries.\n",
	       user->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_level (SAINT, text, user);
      return;
    }
  word[1][0] = toupper (word[1][0]);
  for (i = 0; i <= watch_pos; i++)
    {
      if (!strcmp (user->savename, watch_source[i]))
	{
	  if (!strcmp (word[1], watch_dest[i]))
	    {
	      for (j = i; j < watch_pos; j++)	/* delete this entry */
		{
		  strcpy (watch_source[j], watch_source[j + 1]);
		  strcpy (watch_dest[j], watch_dest[j + 1]);
		}
	      watch_source[watch_pos][0] = '\0';
	      watch_dest[watch_pos][0] = '\0';
	      watch_pos--;
	      sprintf (text, ">>>The %s's watch entry was deleted!\n",
		       word[1]);
	      write_user (user, text);
	      counter++;
	    }
	}
    }
  sprintf (text, "\n>>>~FT%d~RS entries were deleted!\n", counter);
  write_user (user, text);
}

/*** clear watch list ***/
clear_watch ()
{
  int i;
  for (i = 0; i < MAX_WATCH; i++)
    {
      watch_source[i][0] = '\0';
      watch_dest[i][0] = '\0';
      watch_runcom[i] = -1;
    }
  watch_pos = -1;
}

/*** increase user dimension ***/
up (user)
     UR_OBJECT user;
{
  if (user->restrict[RESTRICT_GO] == restrict_string[RESTRICT_GO])
    {
      write_user (user, ">>>You cannot access another dimension! Sorry...\n");
      return;
    }

  if (user->dimension >= MAX_DIMENS - 1)
    {
      write_user (user, ">>>A higher dimension is not available, yet!\n");
      return;
    }
  user->dimension++;
  user->room = room_first[user->dimension];
  write_user (user,
	      ">>>The space has collapsed... You win another dimension...\n");
  look (user);
  sprintf (text,
	   "~FT~OL>>>%s flies high to dimension %d (%s~RS~FT~OL)...\n",
	   user->vis ? user->name : invisname, user->dimension + 1,
	   dimens_name[user->dimension]);
  write_room_except (NULL, text, user);
}

/*** decrease user dimension ***/
down (user)
     UR_OBJECT user;
{
  if (user->restrict[RESTRICT_GO] == restrict_string[RESTRICT_GO])
    {
      write_user (user, ">>>You cannot access another dimension! Sorry...\n");
      return;
    }

  if (user->dimension <= 0)
    {
      write_user (user, ">>>A lower dimension is not available, yet!\n");
      return;
    }
  user->dimension--;
  user->room = room_first[user->dimension];
  write_user (user,
	      ">>>The space has collapsed... You loose a dimension...\n");
  look (user);
  sprintf (text, "~FT~OL>>>%s falls in dimension %d (%s~RS~FT~OL)...\n",
	   user->vis ? user->name : invisname, user->dimension + 1,
	   dimens_name[user->dimension]);
  write_room_except (NULL, text, user);
}

/*** Suggest a name for rename_user() ***/
char *
suggest_name (name, type)
     char *name;
     int type;
{
  static char sname[WORD_LEN + 1];
  int j, i = 0;
  char *sufix[REN_MAX_SUF] =
    { "uc", "oi", "as", "ea", "in", "na", "pi", "ron", "gaa", "os", "ups",
      "tr", "hass", "nul", "xe", "uck", "ath", "phe", "itz", "ix",
      "oq", "vov", "exi", "qou", "iz", "bog", "ant", "ay", "ol", "his",
      "pul", "uva", "eban", "fy", "ob", "cio", "awa", "luss", "izd", "gee"
  };
  char voc[] = "aeiou";
  char con[] = "bcdfghijklmnpqrstvwyxz";

  if (type == REN_BASE)		/* to generate a name based on 'name' */
    {
      sprintf (sname, "%s%s", name, sufix[random () % 30]);
      return (sname);
    }
  if (type == REN_REV)		/* to generate a reverse name */
    {
      strcpy (sname, name);
      strrev (sname);
      return (sname);
    }
  if (random () % 2)
    {
      sname[0] = voc[random () % 5];
      i++;
    }
  sname[i] = con[random () % 21];
  i++;
  sname[i] = voc[random () % 5];
  i++;
  j = random () % 4;
  while (j)
    {
      sname[i] = con[random () % 21];
      i++;
      sname[i] = voc[random () % 5];
      i++;
      j--;
    }
  sname[i] = '\0';
  strcat (sname, sufix[random () % REN_MAX_SUF]);
  return (sname);
}

/*** rename an user ***/
rename_user (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  FILE *fp;
  int i;

  if (word_count < 2)
    {
      strcpy (user->name, user->savename);
      write_user (user, ">>>You have the real name, again!\n");
      sprintf (text, ">>>%s returns to a real identity.\n", user->savename);
      write_level (SAINT, text, user);
      return;
    }
  /* list all renamed users */
  if (!strcmp (word[1], "list") && user->level >= MIN_LEV_UREN)
    {
      i = 0;
      for (u = user_first; u != NULL; u = u->next)
	{
	  if (u->type != USER_TYPE || (!u->vis && u->level > user->level))
	    continue;
	  if (strcmp (u->savename, u->name))
	    {
	      if (!i)
		write_user (user,
			    "\n~FB~OL--->>> List of renamed users <<<---\n\n");
	      sprintf (text, "\t%20s known as: %s\n", u->savename, u->name);
	      write_user (user, text);
	      i++;
	    }
	}
      if (i)
	{
	  sprintf (text, "\n>>>Total of ~FT%d~RS renamed users.\n", i);
	  write_user (user, text);
	}
      else
	write_user (user, ">>>No renamed users found.\n");
      return;
    }
  /* check if user want a computer-generated name */
  if (!strcmp (word[1], "?"))
    strcpy (word[1], suggest_name (NULL, REN_GEN));
  else if (!strcmp (word[1], "??"))
    strcpy (word[1], suggest_name (user->savename, REN_BASE));
  else if (!strcmp (word[1], "~"))
    strcpy (word[1], suggest_name (user->name, REN_REV));
  word[1][0] = toupper (word[1][0]);
  if (strlen (word[1]) > USER_NAME_LEN)
    word[1][USER_NAME_LEN] = '\0';
  if (user->level < MIN_LEV_UREN)
    {
      sprintf (text, "%s/%s.D", USERFILES, word[1]);
      if ((fp = fopen (text, "r")) != NULL)
	{
	  fclose (fp);
	  write_user (user, ">>>This name is already used... Try other...\n");
	  return;
	}
      strtolower (word[1]);
      word[1][0] = toupper (word[1][0]);
      sprintf (text, "%s/%s.D", USERFILES, word[1]);
      if ((fp = fopen (text, "r")) != NULL)
	{
	  fclose (fp);
	  write_user (user, ">>>This name is already used... Try other...\n");
	  return;
	}
      for (i = 0; i < strlen (word[1]); i++)
	if (!isalpha (word[1][i]))
	  {
	    write_user (user, ">>>You cannot use this name!...\n");
	    return;
	  }
    }
  if (word_count == 2)
    {
      strcpy (user->name, word[1]);
      sprintf (text, ">>>Your name is ~FT%s~RS now.\n", word[1]);
      write_user (user, text);
      sprintf (text, ">>>Now, %s is known as ~FT%s~RS.\n", user->savename,
	       word[1]);
      write_level (SAINT, text, user);
      return;
    }
  if (word_count > 2 && user->level < MIN_LEV_OREN)
    {
      write_user (user, ">>>Invalid parameter!\n");
      return;
    }
  if ((u = get_user (word[2])) == NULL)
    {
      write_user (user, nosuchuser);
      return;
    }
  if (u->type == REMOTE_TYPE)
    {
      write_user (user, ">>>You cannot rename a remote user, sorry...!\n");
      return;
    }
  if (user->level < MIN_LEV_OREN || user->level <= u->level)
    {
      write_user (user, ">>>You cannot rename this user!\n");
      return;
    }
  strcpy (u->name, word[1]);
  sprintf (text, ">>>You change the ~FT%s~RS's name into ~FT%s~RS.\n",
	   u->savename, word[1]);
  write_user (user, text);
  sprintf (text, "%s CHANGES the %s's name into %s.\n", user->savename,
	   u->savename, word[1]);
  write_syslog (LOG_COM, text, LOG_TIME);
  if (u->level >= SPIRIT)
    {
      sprintf (text, ">>>%s changes your name into %s.\n",
	       user->vis ? user->name : invisname, word[1]);
      write_user (u, text);
    }
}

/*** modify user information ***/
modify (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  char s[OUT_BUFF_SIZE + 1], filename[80], newfilename[80];
  char extens[] = "DPMSRUIONWE0123456789";
  int i, hours, days, no_exists = 0;
  FILE *fp;
  if (word_count < 4)
    {
      write_usage (user,
		   "modify <user> desc/in/out/site/vis/time/tone/ident/pballs/level <text>");
      return;
    }
  if ((u = get_user (word[1])) == NULL)
    {
      /* User not logged on */
      if ((u = create_user ()) == NULL)
	{
	  sprintf (text, ">>>%s: unable to create temporary user object.\n",
		   syserror);
	  write_user (user, text);
	  write_syslog (LOG_ERR,
			"ERROR: Unable to create temporary user object in modify().\n",
			LOG_NOTIME);
	  return;
	}
      strcpy (u->savename, word[1]);
      if (!load_user_details (u))
	{
	  write_user (user, nosuchuser);
	  destruct_user (u);
	  destructed = 0;
	  return;
	}
      no_exists = 1;
    }
  if (u->level >= user->level)
    {
      write_user (user,
		  ">>>You cannot modify the information about this user!\n");
      if (no_exists)
	{
	  destruct_user (u);
	  destructed = 0;
	}
      return;
    }
  strcpy (s, word[3]);
  for (i = 4; i < word_count; i++)
    {
      strcat (s, " ");
      strcat (s, word[i]);
    }
  if (!strncmp (word[2], "desc", strlen (word[2])))
    {
      if (strlen (s) > USER_DESC_LEN)
	s[USER_DESC_LEN] = '\0';
      strcpy (u->desc, s);
      sprintf (text, ">>>You change %s's description...\n", u->savename);
      write_user (user, text);
      if (no_exists)
	{
	  save_user_details (u, 0);
	  destruct_user (u);
	  destructed = 0;
	}
      return;
    }
  if (!strncmp (word[2], "in", strlen (word[2])))
    {
      if (strlen (s) > PHRASE_LEN)
	s[PHRASE_LEN] = '\0';
      strcpy (u->in_phrase, s);
      sprintf (text, ">>>You change %s's in phrase...\n", u->savename);
      write_user (user, text);
      if (no_exists)
	{
	  save_user_details (u, 0);
	  destruct_user (u);
	  destructed = 0;
	}
      return;
    }
  if (!strncmp (word[2], "out", strlen (word[2])))
    {
      if (strlen (s) > PHRASE_LEN)
	s[PHRASE_LEN] = '\0';
      strcpy (u->out_phrase, s);
      sprintf (text, ">>>You change %s's out phrase...\n", u->savename);
      write_user (user, text);
      if (no_exists)
	{
	  save_user_details (u, 0);
	  destruct_user (u);
	  destructed = 0;
	}
      return;
    }
  if (!strncmp (word[2], "site", strlen (word[2])))
    {
      if (strlen (s) > SITE_NAME_LEN)
	s[SITE_NAME_LEN] = '\0';
      strcpy (u->site, s);
      strcpy (u->ssite, s);
      sprintf (text, ">>>You change %s's site...\n", u->savename);
      write_user (user, text);
      if (no_exists)
	{
	  save_user_details (u, 0);
	  destruct_user (u);
	  destructed = 0;
	}
      return;
    }
  if (!strncmp (word[2], "time", strlen (word[2])))
    {
      if (word_count < 5)
	{
	  write_usage (user, "modify <user> time <hours> <days>");
	  if (no_exists)
	    {
	      destruct_user (u);
	      destructed = 0;
	    }
	  return;
	}
      hours = atoi (word[3]);
      if (hours < 0 || hours > 23)
	hours = 0;
      days = atoi (word[4]);
      if (days < 0)
	days = 0;
      u->total_login = days * 86400 + hours * 3600;
      sprintf (text, ">>>You change %s's login time...\n", u->savename);
      write_user (user, text);
      sprintf (text,
	       "~OLGAEN:~RS %s changed %s's login time: %d days, %d minutes.\n",
	       user->savename, u->savename, days, hours);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      if (no_exists)
	{
	  save_user_details (u, 0);
	  destruct_user (u);
	  destructed = 0;
	}
      return;
    }
  if (!strncmp (word[2], "vis", strlen (word[2])))
    {
      if (no_exists)
	{
	  write_user (user, ">>>Use this option only for logged users!\n");
	  destruct_user (u);
	  destructed = 0;
	  return;
	}
      if (!strcmp (word[3], "on"))
	{
	  if (u->vis)
	    {
	      write_user (user, ">>>That user is already visible.\n");
	      return;
	    }
	  u->vis = 1;
	  write_user (u,
		      "~FB~OL>>>The holy spirit of this talker makes you visible!\n");
	  sprintf (text, ">>>%s is visible now!\n", u->name);
	  write_user (user, text);
	  return;
	}
      if (!strcmp (word[3], "off"))
	{
	  if (!u->vis)
	    {
	      write_user (user, ">>>That user is already invisible.\n");
	      return;
	    }
	  u->vis = 0;
	  write_user (u,
		      "~FB~OL>>>The holy spirit of this talker makes you invisible!\n");
	  sprintf (text, ">>>%s is invisible now!\n", u->name);
	  write_user (user, text);
	  return;
	}
      write_usage (user, "modify <user> vis on/off");
      return;
    }
  if (!strncmp (word[2], "tone", strlen (word[2])))
    {
      if (no_exists)
	{
	  write_user (user, ">>>Use this option only for logged users!\n");
	  destruct_user (u);
	  destructed = 0;
	  return;
	}
      if ((u->say_tone = u->tell_tone = atoi (word[3])) < 0
	  || u->say_tone >= MAX_TONES)
	u->say_tone = u->tell_tone = 0;
      sprintf (text, ">>>You change %s's tone...\n", u->savename);
      write_user (user, text);
      return;
    }
  if (!strncmp (word[2], "ident", strlen (word[2])))
    {
      if (!no_exists)
	{
	  write_user (user,
		      ">>>Use this option only for not logged users!\n");
	  return;
	}
      destruct_user (u);
      destructed = 0;
      if (!strcmp (word[1], word[3]))
	{
	  write_user (user, ">>>The old and new names are not different.\n");
	  return;
	}
      if (strlen (word[3]) < USER_MIN_LEN)
	{
	  write_user (user, ">>>Name length too short.\n");
	  return;
	}
      if (strlen (word[3]) > USER_NAME_LEN)
	word[3][USER_NAME_LEN] = '\0';
      for (i = 0; i < strlen (word[3]); i++)
	if (!isalpha (word[3][i]))
	  {
	    write_user (user,
			">>>New user name must contains only letters!\n");
	    return;
	  }
      word[3][0] = toupper (word[3][0]);
      sprintf (newfilename, "%s/%s.D", USERFILES, word[3]);
      if ((fp = fopen (newfilename, "r")) != NULL)
	{
	  write_user (user, ">>>That name is already used!\n");
	  fclose (fp);
	  return;
	}
      sprintf (filename, "%s/%s.D", USERFILES, word[1]);
      for (i = 0; i < strlen (extens); i++)
	{
	  filename[strlen (filename) - 1] = extens[i];
	  newfilename[strlen (newfilename) - 1] = extens[i];
	  rename (filename, newfilename);
	}
      sprintf (text,
	       ">>>%s's identity changed into %s.\n>>>Don't forget to update the allow/ban user files!\n",
	       word[1], word[3]);
      write_user (user, text);
      sprintf (text, "~OLGAEN:~RS %s's identity changed into %s by %s.\n",
	       word[1], word[3], user->savename);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      return;
    }
  if (!strncmp (word[2], "pballs", strlen (word[2])))
    {
      if (no_exists)
	{
	  write_user (user, ">>>Use this option only for logged users!\n");
	  destruct_user (u);
	  destructed = 0;
	  return;
	}
      if (!isnumber (word[3]))
	{
	  write_usage (user, "modify <user> pballs <number>");
	  return;
	}
      u->pballs = atoi (word[3]);
      sprintf (text, ">>>You change %s's number of balls...\n", u->savename);
      write_user (user, text);
      return;
    }
  if (!strncmp (word[2], "level", strlen (word[2])))
    {
      if ((i = get_level (word[3])) == -1 || i > user->level)
	{
	  write_user (user, ">>>Invalid level.\n");
	  if (no_exists)
	    {
	      destruct_user (u);
	      destructed = 0;
	    }
	  return;
	}
      u->level = i;		/* silently promote/demote an user */
      u->slevel = i;
      sprintf (text, ">>>You change %s's level...\n", u->savename);
      write_user (user, text);
      if (i >= MIN_LEV_ALLOW)
	write_user (user,
		    ">>>Don't forget to update the allow user files!\n");
      sprintf (text, "~OLGAEN:~RS %s's level changed into %d by %s.\n",
	       word[1], i, user->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      if (no_exists)
	{
	  save_user_details (u, 0);
	  destruct_user (u);
	  destructed = 0;
	}
      return;
    }
  write_user (user, ">>>Unknown argument, yet!...\n");
  if (no_exists)
    {
      destruct_user (u);
      destructed = 0;
    }
}

/*** View the system log command interface ***/
viewlog_com (user)
     UR_OBJECT user;
{
  int type;
  char filename[80];

  if (word_count < 2)
    {
      type = LOG_CHAT;
    }
  else
    {
      if (isnumber (word[1]))
	type = atoi (word[1]) % MAX_LOGS;
      else
	{
	  for (type = 0; type < MAX_LOGS; type++)
	    {
	      if (!strncmp (word[1], logtype[type], strlen (word[1])))
		break;
	    }
	  if (type >= MAX_LOGS)
	    {
	      write_usage (user, "viewlog [<type>]");
	      return;
	    }
	}
    }
  sprintf (filename, "%s/%s.%s", LOGFILES, SYSLOG, logtype[type]);
  sprintf (text, "\n~FB~OL--->>> System log ~FT%s~RS~FB~OL <<<---\n\n",
	   logtype[type]);
  write_user (user, text);
  switch (more (user, user->socket, filename))
    {
    case 0:
      write_user (user, ">>>The system log is empty!\n");
      return;
    case 1:
      user->misc_op = 2;
    }
}

/*** Prayers for promoting/demoting ***/
pray (user, done_editing)
     UR_OBJECT user;
     int done_editing;
{
  sprintf (text, "~OL>>>%s is praying...\n",
	   user->vis ? user->name : invisname);
  write_room_except (NULL, text, user);
  if (random () % PRAY_RND < PRAY_VAL)	/* promoting...? */
    {
      if (random () % PRAY_RND <= PRAY_VAL)
	{
	  if (user->level <= MAX_LEV_PPRAY)
	    {
	      user->level++;
	      if (user->slevel < GOD)
		user->slevel++;
	      write_user (user,
			  "~OL>>>Your prayer was listen! You have been promoted! :-)\n");
	      sprintf (text,
		       ">>>%s has been promoted by ~FG~OLGAEN~RS! The prayer was good!\n",
		       user->name);
	      write_room_except (NULL, text, user);
	      write_syslog (LOG_COM, text, LOG_TIME);
	    }
	  return;
	}
    }
  else				/* demoting...? */
    {
      if (random () % PRAY_RND <= PRAY_VAL)
	{
	  if (user->level > GUEST && user->level <= MAX_LEV_DPRAY)
	    {
	      user->level--;
	      if (user->slevel > GUEST)
		user->slevel--;
	      write_user (user,
			  "~OL>>>Your prayer was boring! You have been demoted! :-(\n");
	      sprintf (text,
		       ">>>%s has been demoted by ~FG~OLGAEN~RS! The prayer was bad!\n",
		       user->name);
	      write_room_except (NULL, text, user);
	      write_syslog (LOG_COM, text, LOG_TIME);
	    }
	}
    }
  enter_prayer (user, done_editing);
}

/*** enter prayer ***/
enter_prayer (user, done_editing)
     UR_OBJECT user;
     int done_editing;
{
  FILE *fp;
  char *c, filename[80];

  if (!done_editing)
    {
      write_user (user,
		  "\n~FB~OL--->>> Please, writing a prayer... <<<---\n\n");
      user->misc_op = 10;
      editor (user, NULL);
      return;
    }
  sprintf (filename, "%s/%s.pray%d", PRAYFILES, user->savename,
	   prayer_number++);
  if (!(fp = fopen (filename, "w")))
    {
      sprintf (text, ">>>%s: couldn't save your prayer.\n", syserror);
      write_user (user, text);
      sprintf (text,
	       "ERROR: Couldn't open file %s to write in enter_prayer().\n",
	       filename);
      write_syslog (LOG_ERR, text, LOG_NOTIME);
      return;
    }
  c = user->malloc_start;
  while (c != user->malloc_end)
    putc (*c++, fp);
  fclose (fp);
  write_user (user, ">>>Prayer stored. Thanks!\n");
  sprintf (text, ">>>Prayer in file '%s'\n", filename);
  write_level (SAINT, text, NULL);
  write_syslog (LOG_COM, text, LOG_TIME);
}

/*** Alerting ***/
alert (user)
     UR_OBJECT user;
{
  char filename[80];
  if (word_count < 2)
    sprintf (filename, "%s/c_%s", DATAFILES, ALERTFILE);
  else
    sprintf (filename, "%s/%c_%s", DATAFILES, word[1][0], ALERTFILE);
  if (!more (user, user->socket, filename))
    {
      write_user (user, ">>>Unknown alert type, yet...\n");
      return;
    }
  user->ignall_store = user->ignall;
  user->afk = 1;
  event = E_AFK;
  strcpy (event_var, user->savename);
  strcpy (user->afkmesg, "~UL~FGAlert!");
  user->ignall = 1;
  user->blind = 1;
  sprintf (text, ">>>%s is watched by the alien creatures!\n",
	   user->vis ? user->name : invisname);
  write_room_except (NULL, text, user);
  user->misc_op = 8;
}

/*** Quits... ***/
quit_user (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  if (word_count < 2 || user->muzzled)
    {
      disconnect_user (user);
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      strcpy (inpstr, "now, I have no mouth...");
    }
  sprintf (text, ">>>%s wants to fly from ~FG~OLGAEN~RS [ ~FR~OL%s~RS ]\n",
	   user->name, inpstr);
  write_room (NULL, text);
  disconnect_user (user);
}

/*** Switch between auto-showing hints at help or not ***/
toggle_hint (user)
     UR_OBJECT user;
{
  if (user->hint_at_help)
    {
      write_user (user, ">>>Auto-show hints at help set to ~FROFF.\n");
      user->hint_at_help = 0;
      return;
    }
  write_user (user, ">>>Auto-show hints at help set to ~FGON.\n");
  user->hint_at_help = 1;
}

/*** Show random hints ***/
hint (user, options)
     UR_OBJECT user;
     int options;
{
  char filename[80];
  int n;

  if (has_unread_mail (user))
    write_user (user, "~EL~FT~OL~LI--->>> UNREAD MAIL! <<<---\n");

  if (options)
    {
      if (word_count >= 2 && !strcmp (word[1], "auto"))
	{
	  toggle_hint (user);
	  return;
	}
    }
  n = random () % max_hints;
  sprintf (text, "~FM~OL--->>> Hint %d <<<---\n", n);
  write_user (user, text);
  sprintf (filename, "%s/hint%d", HINTFILES, n);
  switch (more (user, user->socket, filename))
    {
    case 0:
      write_user (user, ">>>Cannot show hints... Try again.\n");
      return;
    case 1:
      user->misc_op = 2;
    }
}

/*** Show random quotes ***/
quote (user, type)
     UR_OBJECT user;
     int type;
{
  char filename[80], line[80];
  int n;
  FILE *fp;

  n = random () % max_quotes;

  if (type)
    {
      if (word_count >= 2 && isnumber (word[1]))
	n = atoi (word[1]) % max_quotes;
    }

  sprintf (filename, "%s/quote%d", QUOTEFILES, n);
  if (user == NULL)
    {				/* send a quote to all users */
      if (!(fp = fopen (filename, "r")))	/* not found */
	return;
      sprintf (text, "~FM~OL--->>> Quote %d <<<---\n", n);
      write_room (NULL, text);
      fgets (line, 82, fp);	/* load and send to all */
      while (!feof (fp))
	{
	  write_room (NULL, line);
	  fgets (line, 82, fp);
	}
      fclose (fp);
      return;
    }
  if (has_unread_mail (user))
    write_user (user, "~EL~FT~OL~LI--->>> UNREAD MAIL! <<<---\n");
  switch (more (user, user->socket, filename))
    {
    case 0:
      write_user (user, ">>>Cannot show quotes... Try again.\n");
      return;
    case 1:
      user->misc_op = 2;
    }
}

/*** Ignore picture ***/
ignpict (user)
     UR_OBJECT user;
{
  if (!user->ignpict)
    {
      write_user (user, ">>>You are now ignoring pictures.\n");
      sprintf (text, ">>>%s is now ignoring pictures.\n", user->name);
      write_room_except (user->room, text, user);
      user->ignpict = 1;
      return;
    }
  write_user (user, ">>>You will now be able to view pictures.\n");
  sprintf (text, ">>>%s is now able to see pictures.\n", user->name);
  write_room_except (user->room, text, user);
  user->ignpict = 0;
}

/*** Show the real level & site ***/
reality (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  int no_exists = 0;

  if (word_count < 2)
    {
      write_usage (user, "reality <user>");
      return;
    }
  if ((u = get_user (word[1])) == NULL)
    {
      /* User not logged on */
      if ((u = create_user ()) == NULL)
	{
	  sprintf (text, ">>>%s: unable to create temporary user object.\n",
		   syserror);
	  write_user (user, text);
	  write_syslog (LOG_ERR,
			"ERROR: Unable to create temporary user object in reality().\n",
			LOG_NOTIME);
	  return;
	}
      strcpy (u->savename, word[1]);
      if (!load_user_details (u))
	{
	  write_user (user, nosuchuser);
	  destruct_user (u);
	  destructed = 0;
	  return;
	}
      strcpy (u->site, u->last_site);
      strcpy (u->name, u->savename);
      no_exists = 1;
    }
  if (user->level < u->level)
    {
      write_user (user, ">>>You cannot use this command for this user!\n");
      if (no_exists)
	{
	  destruct_user (u);
	  destructed = 0;
	}
      return;
    }
  sprintf (text, ">>>%s's real name is    : ~FG~OL%s\n", u->name,
	   u->savename);
  write_user (user, text);
  sprintf (text, ">>>%s's real level is   : %s~RS (%d)\n", u->name,
	   level_name[u->level], u->level);
  write_user (user, text);
  if (!no_exists)
    {
      sprintf (text, ">>>%s's real sky is     : ~FG~OL%s~RS\n", u->name,
	       u->room !=
	       NULL ? u->room->name : u->netlink->connect_room->name);
      write_user (user, text);
    }
  sprintf (text, ">>>%s's real site is    : ~FG~OL%s\n", u->name, u->site);
  write_user (user, text);
  if (no_exists)
    {
      destruct_user (u);
      destructed = 0;
    }
  else if (u->level > SPIRIT)
    {
      if (u == user)
	return;
      sprintf (text, "~UL>>>%s wants to know your real identity!\n",
	       user->vis ? user->name : invisname);
      write_user (u, text);
    }
}

/*** Save users details files ***/
savedetails (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  if (word_count < 2)		/* all users */
    {
      sprintf (text,
	       "~OLGAEN:~RS Save all users details initiated by %s...\n",
	       user->vis ? user->name : "a digit");
      write_level (SAINT, text, user);
      sprintf (text, "%s saved all users details.\n", user->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_user (user, ">>>Save details for...");
      for (u = user_first; u != NULL; u = u->next)
	{
	  if (u->type != USER_TYPE || u->login)
	    continue;
	  sprintf (text, " %s", u->savename);
	  write_user (user, text);
	  save_user_details (u, 1);
	}
    }
  else				/* only one user */
    {
      if ((u = get_user (word[1])) == NULL)
	{
	  write_user (user, nosuchuser);
	  return;
	}
      sprintf (text, "~OLGAEN:~RS Save %s's details initiated by %s\n",
	       u->name, user->vis ? user->savename : "a digit");
      write_level (SAINT, text, user);
      sprintf (text, "%s saved %s's details.\n", user->savename, u->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      sprintf (text, ">>>Save details for... %s", u->savename);
      write_user (user, text);
      save_user_details (u, 1);
    }
  write_user (user, ".\n");
}

/*** Clear users' screen ***/
clsall (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  if (word_count < 2)
    {
      for (u = user_first; u != NULL; u = u->next)
	{
	  if (u->type == CLONE_TYPE || u->login || u == user
	      || u->ignall || u->level >= user->level)
	    continue;
	  cls (u);
	}
      write_user (user, ">>>You clear screen to all inferior users.\n");
      sprintf (text, "%s CLEARS screen to all inferior users.\n",
	       user->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      return;
    }
  else
    {
      if ((u = get_user (word[1])) == NULL)
	{
	  write_user (user, nosuchuser);
	  return;
	}
      if (u->level >= user->level)
	{
	  write_user (user, ">>>You cannot clear screen of that user!\n");
	  return;
	}
      cls (u);
      sprintf (text, ">>>You clear %s's screen.\n", u->name);
      write_user (user, text);
      sprintf (text, "%s CLEARS %s's screen.\n", user->savename, u->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
    }
}

/*** Social ***/
social (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  int i, len;
  if (word_count < 2)
    {
      write_usage (user, "social <command> [<user>]");
      return;
    }
  if (word_count == 2)
    {
      len = strlen (word[1]);
      for (i = 0; i < MAX_SOCIALS; i++)
	if (!strncmp (word[1], soc_com[i], len))
	  {
	    for (u = user_first; u != NULL; u = u->next)
	      {
		if (u->type == CLONE_TYPE || u == user || u->ignall
		    || u->igntell || u->login)
		  continue;
		if (is_ignoring (u, user))
		  {
		    sprintf (text,
			     ">>>%s is ignoring you. You must ask him to forgive you first.\n",
			     u->name);
		    write_user (user, text);
		    return;
		  }
		sprintf (text, "~OL>>>%s~RS~OL %s.\n", user->name,
			 soc_text_you[i]);
		write_user (u, text);
		sprintf (text, ">>>You %s %s.\n", soc_text[i], u->name);
		write_user (user, text);
	      }
	    return;
	  }
    }
  else
    {
      len = strlen (word[1]);
      if ((u = get_user (word[2])) == NULL)
	{
	  write_user (user, nosuchuser);
	  return;
	}
      for (i = 0; i < MAX_SOCIALS; i++)
	if (!strncmp (word[1], soc_com[i], len))
	  {
	    if (u->ignall || u->igntell)
	      {
		write_user (user,
			    ">>>That user is ignoring tells or all messages at this moment.\n");
		return;
	      }
	    if (is_ignoring (u, user))
	      {
		sprintf (text,
			 ">>>%s is ignoring you. You must ask him to forgive you first.\n",
			 u->name);
		write_user (user, text);
		return;
	      }
	    sprintf (text, "~OL>>>%s~RS~OL %s.\n", user->name,
		     soc_text_you[i]);
	    write_user (u, text);
	    sprintf (text, ">>>You %s %s.\n", soc_text[i], u->name);
	    write_user (user, text);
	    return;
	  }
    }
  write_user (user, ">>>Unknown social command, yet...\n");
}

/*** Substitute an user ***/
subst (user)
     UR_OBJECT user;
{
  FILE *fp;
  char filename[80];
  UR_OBJECT u;

  if (word_count < 2)
    {
      sprintf (filename, "%s/%s.S", USERFILES, user->savename);
      if (!(fp = fopen (filename, "r")))
	write_user (user, ">>>Substitution status: ~FROFF.\n");
      else
	{
	  write_user (user, ">>>Substitution status: ~FGON.\n");
	  fclose (fp);
	}
      return;
    }
  if (word_count < 3)
    {
      if (user->level < MIN_LEV_OSUBST)
	write_usage (user, "subst [ <level> <site> ]");
      else
	write_usage (user, "subst [ <level> <site> [<user>] ]");
      return;
    }
  if (word_count == 3)
    u = user;
  else
    {
      if (user->level < MIN_LEV_OSUBST)
	{
	  write_user (user, ">>>You cannot use this option!\n");
	  return;
	}
      if ((u = get_user (word[3])) == NULL)
	{
	  write_user (user, notloggedon);
	  return;
	}
      if (u->level >= user->level)	/* level too low... */
	{
	  write_user (user,
		      ">>>You cannot use this command for that user! Sorry...\n");
	  return;
	}
    }
  sprintf (filename, "%s/%s.S", USERFILES, u->savename);
  if (!(fp = fopen (filename, "w")))
    {
      write_user (user, ">>>Cannot save the substitute information.\n");
      sprintf (text, "SAVE_SUBST: Failed to save %s's subst.\n", u->savename);
      write_syslog (LOG_ERR, text, LOG_TIME);
      return;
    }
  if (!isnumber (word[1]))
    u->slevel = u->level;
  else
    u->slevel = (word[1][0] - '0') % (MAX_LEVELS - 2);
  strcpy (u->ssite, word[2]);	/* write substitution info */
  fprintf (fp, "%s\n", u->ssite);
  fprintf (fp, "%d\n", u->slevel);
  fclose (fp);
  u->subst = 1;
  if (u != user)
    {
      sprintf (text, ">>>Substitution for %s set.\n", u->savename);
      write_user (user, text);
      sprintf (text, "%s SUBSTituted %s's level & site.\n", user->savename,
	       u->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
    }
  else
    write_user (user, ">>>Substitution set.\n");
}

/*** Delete subst. file ***/
dsubst (user)
     UR_OBJECT user;
{
  FILE *fp;
  char filename[80];
  UR_OBJECT u;

  if (word_count < 2)
    sprintf (filename, "%s/%s.S", USERFILES, user->savename);
  else
    {
      if (user->level < MIN_LEV_OSUBST)	/* security feature... */
	{
	  write_user (user,
		      ">>>You cannot use this command with parameters...\n");
	  return;
	}
      word[1][0] = toupper (word[1][0]);
      sprintf (filename, "%s/%s.S", USERFILES, word[1]);
      if (!(fp = fopen (filename, "r")))
	{
	  write_user (user, ">>>Substitution file was already deleted.\n");
	  return;
	}
      fclose (fp);
      s_unlink (filename);
      write_user (user, ">>>Substitution file deleted!\n");
      if ((u = get_user (word[1])) != NULL)
	{
	  u->slevel = u->level;
	  strcpy (u->ssite, u->site);
	  u->subst = 0;
	  sprintf (text, ">>>%s has now the real site & level.\n",
		   u->savename);
	  write_user (user, text);
	}
      sprintf (text, "%s deleted %s's subst file.\n", user->savename,
	       word[1]);
      write_syslog (LOG_COM, text, LOG_TIME);
      return;
    }
  if (!(fp = fopen (filename, "r")))
    {
      write_user (user, ">>>Substitution file was already deleted.\n");
      return;
    }
  fclose (fp);
  unlink (filename);
  write_user (user, ">>>Substitution file deleted.\n");
  user->slevel = user->level;
  strcpy (user->ssite, user->site);
  user->subst = 0;
}

/*** Set user details ***/
setdata (user)
     UR_OBJECT user;
{
  FILE *fp;
  char filename[80], value[WORD_LEN + 1];

  if (word_count < 2)
    {
      write_usage (user,
		   "set <firstname> <lastname> <gender> <age> <email> <city/country>");
      if (user->level > SOUL)
	{
	  write_usage (user, "set ? / ?{ / ?}");
	  write_usage (user, "set var{ / var} / +{ / +} [<word>]");
	}
      return;
    }
  /* Show the value(s) of user variables */
  if (word_count == 2)
    {
      if (!strcmp (word[1], "?{"))
	{
	  sprintf (text, ">>>Value of { variable is '~FT%s~RS'\n",
		   user->var1);
	  write_user (user, text);
	  return;
	}
      if (!strcmp (word[1], "?}"))
	{
	  sprintf (text, ">>>Value of } variable is '~FT%s~RS'\n",
		   user->var2);
	  write_user (user, text);
	  return;
	}
      if (word[1][0] == '?')
	{
	  sprintf (text,
		   ">>>Value of { variable is '~FT%s~RS'\n>>>Value of } variable is '~FT%s~RS'\n",
		   user->var1, user->var2);
	  write_user (user, text);
	  return;
	}
      if (!strcmp (word[1], "var{"))
	{
	  write_user (user, ">>>Default value of { variable stored.\n");
	  strcpy (user->var1, "{");
	  return;
	}
      if (!strcmp (word[1], "var}"))
	{
	  write_user (user, ">>>Default value of } variable stored.\n");
	  strcpy (user->var2, "}");
	  return;
	}
      write_user (user, ">>>Wrong parameters.\n");
      return;
    }
  if (word_count == 3)
    {
      /* Checking if it's a special word */
      if (!strcmp (word[2], "_name"))
	strcpy (value, user->savename);
      else if (!strcmp (word[2], "_level"))
	sprintf (value, "%d", user->level);
      else
	strcpy (value, word[2]);
      /* Storing the value of variables */
      if (!strcmp (word[1], "var{"))
	{
	  write_user (user, ">>>Value of { variable stored.\n");
	  strcpy (user->var1, value);
	  return;
	}
      if (!strcmp (word[1], "var}"))
	{
	  write_user (user, ">>>Value of } variable stored.\n");
	  strcpy (user->var2, value);
	  return;
	}
      if (!strcmp (word[1], "+{"))
	{
	  if (strlen (user->var1) + strlen (value) > WORD_LEN)
	    write_user (user, ">>>Word too long.\n");
	  else
	    {
	      write_user (user, ">>>Value of { variable concatenated.\n");
	      strcat (user->var1, value);
	    }
	  return;
	}
      if (!strcmp (word[1], "+}"))
	{
	  if (strlen (user->var2) + strlen (value) > WORD_LEN)
	    write_user (user, ">>>Word too long.\n");
	  else
	    {
	      write_user (user, ">>>Value of } variable concatenated.\n");
	      strcat (user->var2, value);
	    }
	  return;
	}
      write_user (user, ">>>Wrong parameters.\n");
      return;
    }
  if (word_count != 7)
    {
      write_usage (user,
		   "set <firstname> <lastname> <gender> <age> <email> <city/country>");
      return;
    }
  sprintf (filename, "%s/%s.U", USERFILES, user->savename);
  if (!(fp = fopen (filename, "w")))
    {
      sprintf (text, ">>>%s: failed to save your information.\n", syserror);
      write_user (user, text);
      sprintf (text, "SET_DATA: Failed to save %s's information.\n",
	       user->savename);
      write_syslog (LOG_ERR, text, LOG_TIME);
      return;
    }
  word[1][0] = toupper (word[1][0]);
  word[2][0] = toupper (word[2][0]);
  word[6][0] = toupper (word[6][0]);
  fprintf (fp, "%s\n", word[1]);
  fprintf (fp, "%s\n", word[2]);
  fprintf (fp, "%s\n", word[3]);
  fprintf (fp, "%s\n", word[4]);
  fprintf (fp, "%s\n", word[5]);
  fprintf (fp, "%s\n", word[6]);
  fclose (fp);
  write_user (user, ">>>Information set:\n");
  sprintf (text, "\tFirst name:\t~FT%s\n", word[1]);
  write_user (user, text);
  sprintf (text, "\tLast name:\t~FT%s\n", word[2]);
  write_user (user, text);
  sprintf (text, "\tYour gender:\t~FT%s\n",
	   (word[3][0] == 'M' || word[3][0] == 'm') ? "Male" : "Female");
  write_user (user, text);
  sprintf (text, "\tYour age:\t~FT%s\n", word[4]);
  write_user (user, text);
  sprintf (text, "\tE-mail addr.:\t~FT%s\n", word[5]);
  write_user (user, text);
  sprintf (text, "\tCity/Country:\t~FT%s\n", word[6]);
  write_user (user, text);
}

/*** Flash... an user ( demoting, muzzling, moving to a hidden sky ) ***/
flash (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  if (word_count < 2)
    {
      write_usage (user, "flash <user>");
      return;
    }
  write_user (user, "~OL~UL~FR>>>You preparing an acid flash...\n");
  sprintf (text, "~OL~FR~UL>>>%s prepares an acid flash...\n",
	   user->vis ? user->name : invisname);
  write_room_except (NULL, text, user);
  demote (user);
  muzzle (user);
  strcpy (word[2], jailsky);
  word_count = 3;
  move (user);
  if ((rm = get_room (jailsky)) != NULL)
    rm->hidden = 1;
  sprintf (text, "%s used FLASH on %s.\n", user->savename, word[1]);
  write_syslog (LOG_COM, text, LOG_TIME);
}

/*** Emote some talker-defined macros ***/
macro (user)
     UR_OBJECT user;
{
  int i;
  if (has_unread_mail (user))
    write_user (user, "~EL~FT~OL~LI--->>> UNREAD MAIL! <<<---\n");
  if (word_count >= 2 && word[1][0] == '?')
    {
      write_user (user, ">>>Available macros are:\n\n");
      for (i = 0; i < MAX_MACROS; i++)
	{
	  sprintf (text, "%10s", macros_name[i]);
	  write_user (user, text);
	  if (i % 7 == 6)
	    write_user (user, "\n");
	}
      sprintf (text, "\n\n>>>Total of ~FT%d~RS macros.\n", MAX_MACROS);
      write_user (user, text);
      return;
    }
  if (word_count < 2)
    {
      say (user, macros[MAX_MACROS - 1]);
      return;
    }
  /* hidden macro (a Beavis idea) :) */
  if (!strcmp (word[1], "es"))
    {
      write_room (NULL, "~FB~OLGAEN: Eating shit is not allowed here!\n");
      return;
    }
  for (i = 0; i < MAX_MACROS; i++)
    if (!strcmp (word[1], macros_name[i]))
      {
	say (user, macros[i]);
	return;
      }
  write_user (user, ">>>Unknown macro name, yet...\n");
}

/*** Display details of room ***/
look (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  UR_OBJECT u;
  char temp[81], null[1], *ptr;
  char *afk = "~OL~FY<AFK>";
  int i, exits, users;

  if (word_count < 2)
    {
      rm = user->room;
    }
  else
    {
      if ((rm = get_room (word[1])) == NULL)
	{
	  rm = user->room;
	}
    }
  sprintf (text,
	   "\n~FTSky: ~FG%s ~RS-- ~FTDimension: ~FG%d ( ~OL%s~RS~FG )\n\n",
	   rm->hidden ? "<unknown>" : rm->name, user->dimension + 1,
	   dimens_name[user->dimension]);
  write_user (user, text);
  write_user (user, rm->hidden ? "" : rm->desc);

  exits = 0;
  null[0] = '\0';
  strcpy (text, "\n~FTExits are:");
  for (i = 0; i < MAX_LINKS; ++i)
    {
      if (rm->link[i] == NULL)
	break;
      if (rm->link[i]->access & PRIVATE)
	sprintf (temp, "  ~FR%s",
		 rm->link[i]->hidden ? "<unknown>" : rm->link[i]->name);
      else
	sprintf (temp, "  ~FG%s",
		 rm->link[i]->hidden ? "<unknown>" : rm->link[i]->name);
      strcat (text, temp);
      ++exits;
    }
  if (rm->netlink != NULL && rm->netlink->stage == UP)
    {
      if (rm->netlink->allow == IN)
	sprintf (temp, "  ~FR%s*", rm->netlink->service);
      else
	sprintf (temp, "  ~FG%s*", rm->netlink->service);
      strcat (text, temp);
    }
  else if (!exits)
    strcpy (text, "\n~FT>>>There are no exits.");
  strcat (text, "\n\n");
  write_user (user, text);

  users = 0;
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->room != rm || u == user || (!u->vis && u->level > user->level))
	continue;
      if (!users++)
	write_user (user, "~FT>>>You can see:\n");
      if (u->afk)
	ptr = afk;
      else
	ptr = null;
      if (!u->vis)
	sprintf (text, "	~FR*~RS%s %s~RS  %s\n",
		 u->name, strrepl (u->desc, "{name}", user->name), ptr);
      else
	sprintf (text, "	 %s %s~RS  %s\n",
		 u->name, strrepl (u->desc, "{name}", user->name), ptr);
      write_user (user, text);
    }
  if (!users)
    write_user (user, "~FT--->>> You are alone here :-( <<<---\n");
  write_user (user, "\n");

  strcpy (text, ">>>Access is ");
  switch (rm->access)
    {
    case PUBLIC:
      strcat (text, "set to ~FGPUBLIC~RS");
      break;
    case PRIVATE:
      strcat (text, "set to ~FRPRIVATE~RS");
      break;
    case FIXED_PUBLIC:
      strcat (text, "~FRfixed~RS to ~FGPUBLIC~RS");
      break;
    case FIXED_PRIVATE:
      strcat (text, "~FRfixed~RS to ~FRPRIVATE~RS");
      break;
    }
  sprintf (temp, " and there are ~OL~FM%d~RS message(s) on the board.\n",
	   rm->mesg_cnt);
  strcat (text, temp);
  write_user (user, text);
  if (rm->topic[0])
    {
      sprintf (text, ">>>Current topic: %s\n", rm->topic);
      write_user (user, text);
      return;
    }
  write_user (user, ">>>No topic has been set yet.\n");
  if (has_unread_mail (user))
    write_user (user, "~EL~FT~OL~LI--->>> UNREAD MAIL! <<<---\n");
}



/*** Switch between command and speech mode ***/
toggle_mode (user)
     UR_OBJECT user;
{
  if (user->command_mode)
    {
      write_user (user, ">>>Now in ~FGSPEECH~RS mode.\n");
      user->command_mode = 0;
      return;
    }
  write_user (user, ">>>Now in ~FRCOMMAND~RS mode.\n");
  user->command_mode = 1;
}


/*** Shutdown the talker ***/
talker_shutdown (user, str, reboot)
     UR_OBJECT user;
     char *str;
     int reboot;
{
  UR_OBJECT u;
  NL_OBJECT nl;
  int i;
  char *ptr;
  char *args[] = {
    progname, "-c", confile, NULL	/* command line arguments */
  };

  if (user != NULL)
    ptr = user->savename;
  else
    ptr = str;
  if (reboot)
    {
      write_room (NULL, "\07\n~OLGAEN:~FR~LI Rebooting now!!\n\n");
      sprintf (text, ">>> REBOOT initiated by %s <<<\n", ptr);
    }
  else
    {
      write_room (NULL, "\07\n~OLGAEN:~FR~LI Shutting down now!!\n\n");
      sprintf (text, ">>> SHUTDOWN initiated by %s <<<\n", ptr);
    }
  write_syslog (LOG_CHAT, text, LOG_TIME);
  for (nl = nl_first; nl != NULL; nl = nl->next)
    shutdown_netlink (nl);
  for (u = user_first; u != NULL; u = u->next)
    disconnect_user (u);
  for (i = 0; i < 3; ++i)
    close (listen_sock[i]);
  if (reboot)
    {
      /* If someone has changed the binary or the config filename while this
         prog has been running this won't work */
      execvp (progname, args);
      /* If we get this far it hasn't worked */
      sprintf (text, ">>> REBOOT failed at %s: %s <<<\n\n", long_date (1),
	       sys_errlist[errno]);
      write_syslog (LOG_CHAT, text, LOG_NOTIME);
      exit (12);
    }
  sprintf (text, ">>> SHUTDOWN complete at %s <<<\n\n", long_date (1));
  write_syslog (LOG_CHAT, text, LOG_NOTIME);
  exit (0);
}

/*** Say user speech. ***/
say (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char type[10], *name;

  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, you cannot speak...:-(\n");
      return;
    }
  if (user->room == NULL)
    {
      sprintf (text, "ACT %s say %s\n", user->savename, inpstr);
      write_sock (user->netlink->socket, text);
      no_prompt = 1;
      return;
    }
  if (word_count < 2 && user->command_mode)
    {
      write_user (user, ">>>Say what?\n");
      return;
    }
  switch (inpstr[strlen (inpstr) - 1])
    {
    case '?':
      strcpy (type, "ask");
      break;
    case '!':
      strcpy (type, "exclaim");
      break;
    case ':':
      strcpy (type, "enumerate");
      break;
    case ')':
      if (inpstr[strlen (inpstr) - 2] == ')' &&
	  inpstr[strlen (inpstr) - 3] == ':')
	{
	  strcpy (type, "laugh");
	  break;
	}
    case '>':
    case 'P':
    case 'p':
      if (inpstr[strlen (inpstr) - 2] == ':' ||
	  (inpstr[strlen (inpstr) - 2] == '-' &&
	   inpstr[strlen (inpstr) - 3] == ':'))
	{
	  strcpy (type, "smile");
	  break;
	}
    case '(':
      if (inpstr[strlen (inpstr) - 2] == ':' ||
	  (inpstr[strlen (inpstr) - 2] == '-' &&
	   inpstr[strlen (inpstr) - 3] == ':'))
	{
	  strcpy (type, "sadly say");
	  break;
	}
    default:
      strcpy (type, "say");
    }
  if (user->type == CLONE_TYPE)
    {
      sprintf (text, ">>>Clone of %s %ss: %s\n",
	       user->vis ? user->name : "a digit", type, inpstr);
      write_room (user->room, text);
      record (user->room, text);
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  sprintf (text, ">>>You %s: %s\n", type, inpstr);
  write_user (user, text);
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  sprintf (text, "%s>>>%s %ss%s%s\n",
	   user->stereo ? stereo_str[random () % MAX_STEREO] : "", name, type,
	   tones[user->say_tone], inpstr);
  write_room_except (user->room, text, user);
  record (user->room, text);
}


/*** Shout something ***/
shout (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char *name;

  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, you cannot shout...:-(\n");
      return;
    }
  if (word_count < 2)
    {
      write_user (user, ">>>Shout what?\n");
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  sprintf (text, "~OL~FR>>>You shout:~RS %s\n", inpstr);
  write_user (user, text);
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  sprintf (text, "~OL~FR>>>%s shouts%s%s\n", name, tones[user->say_tone],
	   inpstr);
  write_room_except (NULL, text, user);
  event = E_SHOUT;
  strcpy (event_var, user->savename);
}

/*** Mega shout something ***/
megashout (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char *name;

  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, you cannot megashout...:-(\n");
      return;
    }
  if (word_count < 2)
    {
      write_user (user, ">>>Megashout what?\n");
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  sprintf (text, "~OL~FR~UL>>>You mega shout:~RS %s\n", inpstr);
  write_user (user, text);
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  sprintf (text, "~EL~OL~FR~UL>>>%s shouts,~EL shouts, shouts:~RS ~EL%s\n",
	   name, inpstr);
  write_room_except (NULL, text, user);
  event = E_SHOUT;
  strcpy (event_var, user->savename);
}


/*** Tell another user something ***/
user_tell (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  UR_OBJECT u;
  char type[5], *name;

  if (user->muzzled)
    {
      write_user (user,
		  ">>>You are muzzled, you cannot tell anyone anything...:-(\n");
      return;
    }
  if (word_count < 3)
    {
      write_user (user, ">>>Tell who what?\n");
      return;
    }
  if (!(u = get_user (word[1])))
    {
      write_user (user, notloggedon);
      return;
    }
  if (u == user)
    {
      write_user (user,
		  ">>>Talking to yourself is the first sign of madness...or genius.\n");
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  if (u->afk)
    {
      sprintf (text, ">>>%s is AFK at the moment... { ~FT%s~RS }\n", u->name,
	       u->afkmesg);
      write_user (user, text);
      return;
    }
  if (u->ignall && (user->level < SPIRIT || u->level > user->level))
    {
      if (u->malloc_start != NULL)
	sprintf (text, ">>>%s is using the editor at the moment.\n", u->name);
      else
	sprintf (text, ">>>%s is ignoring everyone at the moment.\n",
		 u->name);
      write_user (user, text);
      return;
    }
  if (u->igntell && (user->level < SPIRIT || u->level > user->level))
    {
      sprintf (text, ">>>%s is ignoring tells at the moment.\n", u->name);
      write_user (user, text);
      return;
    }
  if (u->room == NULL)
    {
      sprintf (text,
	       ">>>%s is offsite and would not be able to reply to you.\n",
	       u->name);
      write_user (user, text);
      return;
    }
  if (is_ignoring (u, user))
    {
      sprintf (text,
	       ">>>%s is ignoring you. You must ask him to forgive you first.\n",
	       u->name);
      write_user (user, text);
      return;
    }

  inpstr = remove_first (inpstr);
  if (inpstr[strlen (inpstr) - 1] == '?')
    strcpy (type, "ask");
  else
    strcpy (type, "tell");
  sprintf (text, "~OL~FG>>>You %s %s:~RS %s\n", type, u->name, inpstr);
  write_user (user, text);
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  sprintf (text, "%s~OL~FG>>>%s %ss you%s%s\n",
	   user->stereo ? stereo_str[random () % MAX_STEREO] : "", name, type,
	   tones[user->tell_tone], inpstr);
  write_user (u, text);
  record_tell (u, text);
  strcpy (u->reply_user, user->savename);
}

/*** Public tell another user something ***/
user_to (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  UR_OBJECT u;
  char type[5], *name;

  if (user->muzzled)
    {
      write_user (user,
		  ">>>You are muzzled, you cannot tell anyone anything...:-(\n");
      return;
    }
  if (word_count < 3)
    {
      write_user (user, ">>>Public tell who what?\n");
      return;
    }
  if (!(u = get_user (word[1])))
    {
      write_user (user, notloggedon);
      return;
    }
  if (u == user)
    {
      write_user (user,
		  ">>>Talking to yourself is the first sign of madness...or genius.\n");
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  if (u->afk)
    {
      sprintf (text, ">>>%s is AFK at the moment... { ~FT%s~RS }\n", u->name,
	       u->afkmesg);
      write_user (user, text);
      return;
    }
  if (u->ignall && (user->level < SPIRIT || u->level > user->level))
    {
      if (u->malloc_start != NULL)
	sprintf (text, ">>>%s is using the editor at the moment.\n", u->name);
      else
	sprintf (text, ">>>%s is ignoring everyone at the moment.\n",
		 u->name);
      write_user (user, text);
      return;
    }
  if (u->igntell && (user->level < SPIRIT || u->level > user->level))
    {
      sprintf (text, ">>>%s is ignoring tells at the moment.\n", u->name);
      write_user (user, text);
      return;
    }
  if (u->room == NULL)
    {
      sprintf (text,
	       ">>>%s is offsite and would not be able to reply to you.\n",
	       u->name);
      write_user (user, text);
      return;
    }
  if (is_ignoring (u, user))
    {
      sprintf (text,
	       ">>>%s is ignoring you. You must ask him to forgive you first.\n",
	       u->name);
      write_user (user, text);
      return;
    }

  inpstr = remove_first (inpstr);
  if (inpstr[strlen (inpstr) - 1] == '?')
    strcpy (type, "ask");
  else
    strcpy (type, "tell");
  sprintf (text, "~OL~FY>>>You public %s to %s:~RS %s\n", type, u->name,
	   inpstr);
  write_user (user, text);
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  sprintf (text, "~OL~FY>>>%s public %ss to %s: %s\n", name, type,
	   u->vis ? u->name : "a digit", inpstr);
  write_room_except (user->room, text, user);
  record (user->room, text);
  strcpy (u->reply_user, user->savename);
}

/*** Emote something ***/
emote (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char *name;

  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, you cannot emote...:-(\n");
      return;
    }
  if (word_count < 2 && inpstr[1] < 33)
    {
      write_user (user, ">>>Emote what?\n");
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  if (inpstr[0] == ';')
    {
      if (emote_spacer)
	sprintf (text, ">>>%s %s\n", name, inpstr + 1);
      else
	sprintf (text, ">>>%s%s\n", name, inpstr + 1);
    }
  else
    sprintf (text, ">>>%s %s\n", name, inpstr);
  write_room (user->room, text);
  record (user->room, text);
}


/*** Do a shout emote ***/
semote (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char *name;

  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, you cannot emote...:-(\n");
      return;
    }
  if (word_count < 2 && inpstr[1] < 33)
    {
      write_user (user, ">>>Shout emote what?\n");
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  if (inpstr[0] == '#')
    sprintf (text, "~FT~OL>>>!!~RS %s%s\n", name, inpstr + 1);
  else
    sprintf (text, "~FT~OL>>>!!~RS %s %s\n", name, inpstr);
  write_room (NULL, text);
}

/*** Send ASCII pictures files ***/
picture (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  char filename[80];

  if (word_count < 2)
    {
      write_usage (user, "picture <name> [<user>]");
      return;
    }
  if (strlen (word[1]) > PICT_NAME_LEN)
    word[1][PICT_NAME_LEN] = '\0';
  sprintf (filename, "%s/%s.img", IMAGFILES, word[1]);
  if (word_count >= 3)
    {
      if ((u = get_user (word[2])) == NULL)
	{
	  write_user (user, nosuchuser);
	  return;
	}
      sprintf (text, ">>>You send picture of ~FM%s~RS to %s...\n", word[1],
	       u->name);
      write_user (user, text);
      vpict (user, u, filename);
      return;
    }
  sprintf (text, ">>>You sent picture of ~FM%s~RS to all...\n", word[1]);
  write_user (user, text);
  for (u = user_first; u != NULL; u = u->next)
    vpict (user, u, filename);
}

vpict (user, u, fname)
     UR_OBJECT user, u;
     char *fname;
{
  if (u->type == CLONE_TYPE)
    return;

  if (u->afk || u->login || is_ignoring (u, user))
    {
      write_user (user, ">>>That user is busy now he is ignoring you...\n");
      return;
    }
  if (u->ignall && u->malloc_start != NULL)
    {
      sprintf (text, ">>>%s is using the editor at the moment, sorry...\n",
	       u->name);
      write_user (user, text);
      return;
    }
  if (u->ignpict)
    {
      if (user->level < MIN_LEV_IPICT || u->level >= user->level)
	{
	  sprintf (text, ">>>%s sends you ~FM%s~RS, but you cannot view...\n",
		   user->vis ? user->name : invisname, fname);
	  write_user (u, text);
	  sprintf (text, ">>>%s is ignoring pictures at the moment.\n",
		   u->vis ? u->name : invisname);
	  write_user (user, text);
	  return;
	}
    }
  switch (more (u, u->socket, fname))
    {
    case 0:
      write_user (user, ">>>This picture is unavailable! Sorry...\n");
      return;
    case 1:
      u->misc_op = 2;
    }
  sprintf (text, ">>>%s wants you to have this...Enjoy!\n",
	   user->vis ? user->name : invisname);
  write_user (u, text);
  sprintf (text, "%12s: %12s\n", word[1], user->savename);
  record_pict (u, text);
}

/*** Do a private emote ***/
pemote (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char *name;
  UR_OBJECT u;

  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, you cannot private emote...:-(\n");
      return;
    }
  if (word_count < 3)
    {
      write_user (user, ">>>Private emote what?\n");
      return;
    }
  word[1][0] = toupper (word[1][0]);
  if (!strcmp (word[1], user->name))
    {
      write_user (user,
		  ">>>Emoting to yourself is the second sign of madness or genius.\n");
      return;
    }
  if (!(u = get_user (word[1])))
    {
      write_user (user, notloggedon);
      return;
    }
  if (u->afk)
    {
      sprintf (text, ">>>%s is AFK at the moment... { ~FT%s~RS }\n", u->name,
	       u->afkmesg);
      write_user (user, text);
      return;
    }
  if (u->ignall && (user->level < SPIRIT || u->level > user->level))
    {
      if (u->malloc_start != NULL)
	sprintf (text, ">>>%s is using the editor at the moment.\n", u->name);
      else
	sprintf (text, ">>>%s is ignoring everyone at the moment.\n",
		 u->name);
      write_user (user, text);
      return;
    }
  if (u->igntell && (user->level < SPIRIT || u->level > user->level))
    {
      sprintf (text, ">>>%s is ignoring private emotes at the moment.\n",
	       u->name);
      write_user (user, text);
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  if (u->room == NULL)
    {
      sprintf (text,
	       ">>>%s is offsite and would not be able to reply to you.\n",
	       u->name);
      write_user (user, text);
      return;
    }
  if (is_ignoring (u, user))
    {
      sprintf (text,
	       ">>>%s is ignoring you. You must ask him to forgive you first.\n",
	       u->name);
      write_user (user, text);
      return;
    }
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  inpstr = remove_first (inpstr);
  sprintf (text, "~OL~FB(To %s)~RS %s %s\n", u->name, name, inpstr);
  write_user (user, text);
  sprintf (text, "~OL~FB>>-->~RS %s %s\n", name, inpstr);
  write_user (u, text);
  record_tell (u, text);
}


/*** Echo something to screen ***/
echo (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, you cannot echo...:-(\n");
      return;
    }
  if (word_count < 2)
    {
      write_user (user, ">>>Echo what?\n");
      return;
    }
  sprintf (text, "~FT~OL(%s)~RS ", user->name);
  write_level (SPIRIT, text, NULL);
  sprintf (text, "%s\n", inpstr);
  write_room (user->room, text);
  record (user->room, text);
}



/*** Move to another room ***/
go (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  NL_OBJECT nl;
  int i;

  if (word_count < 2)
    {
      write_user (user, ">>>Go where?\n");
      return;
    }
  if (user->restrict[RESTRICT_GO] == restrict_string[RESTRICT_GO])
    {
      write_user (user, ">>>You cannot have access to another sky...\n");
      return;
    }
  if (user->room->hidden)
    {
      write_user (user, ">>>You're in a hidden sky, you cannot go...\n");
      return;
    }
  nl = user->room->netlink;
  if (nl != NULL && !strncmp (nl->service, word[1], strlen (word[1])))
    {
      if (user->pot_netlink == nl)
	{
	  write_user (user,
		      ">>>The remote service may be lagged, please be patient...\n");
	  return;
	}
      rm = user->room;
      if (nl->stage < 2)
	{
	  write_user (user, ">>>The netlink is inactive.\n");
	  return;
	}
      if (nl->allow == IN && user->netlink != nl)
	{
	  /* Link for incoming users only */
	  write_user (user, ">>>Sorry, link is for incoming users only.\n");
	  return;
	}
      /* If site is users home site then tell home system that we have removed
         him. */
      if (user->netlink == nl)
	{
	  write_user (user, "~FB~OL>>>You traverse cyberspace...\n");
	  sprintf (text, "REMVD %s\n", user->savename);
	  write_sock (nl->socket, text);
	  if (user->vis)
	    {
	      sprintf (text, ">>>%s goes to the %s\n", user->name,
		       nl->service);
	      write_room_except (rm, text, user);
	    }
	  else
	    write_room_except (rm, invisleave, user);
	  sprintf (text, "NETLINK: Remote user %s removed.\n",
		   user->savename);
	  write_syslog (LOG_LINK, text, LOG_TIME);
	  destroy_user_clones (user);
	  destruct_user (user);
	  reset_access (rm);
	  num_of_users--;
	  no_prompt = 1;
	  return;
	}
      /* Can't let remote user jump to yet another remote site because this will
         reset his user->netlink value and so we will lose his original link.
         2 netlinks are needed in the user structure really, from_netlink and
         to_netlink. I was going to fix the talker to allow this but it meant
         way too much rehacking of the code and I don't have the time or
         inclination to do it */
      if (user->type == REMOTE_TYPE)
	{
	  write_user (user,
		      ">>>Sorry, due to software limitations you can only traverse 1 netlink.\n");
	  return;
	}
      if (nl->ver_major <= 3 && nl->ver_minor <= 3 && nl->ver_patch < 1)
	{
	  if (!word[2][0])
	    sprintf (text, "TRANS %s %s %s\n", user->savename, user->pass,
		     user->desc);
	  else
	    sprintf (text, "TRANS %s %s %s\n", user->savename,
		     (char *) crypt (word[2], PASS_SALT), user->desc);
	}
      else
	{
	  if (!word[2][0])
	    sprintf (text, "TRANS %s %s %d %s\n", user->savename, user->pass,
		     user->level, user->desc);
	  else
	    sprintf (text, "TRANS %s %s %d %s\n", user->savename,
		     (char *) crypt (word[2], PASS_SALT), user->level,
		     user->desc);
	}
      write_sock (nl->socket, text);
      user->remote_com = GO;
      user->pot_netlink = nl;	/* potential netlink */
      no_prompt = 1;
      return;
    }
/* If someone tries to go somewhere else while waiting to go to a talker
   send the other site a release message */
  if (user->remote_com == GO)
    {
      sprintf (text, "REL %s\n", user->savename);
      write_sock (user->pot_netlink->socket, text);
      user->remote_com = -1;
      user->pot_netlink = NULL;
    }
/* Users can use shortened room name for speed, hence use strncmp */
  if ((rm = get_room (word[1])) == NULL)
    {
      write_user (user, nosuchroom);
      return;
    }
  if (rm == user->room)
    {
      sprintf (text, ">>>You are already in the %s!\n", rm->name);
      write_user (user, text);
      return;
    }
  if (rm->hidden)
    {
      write_user (user, ">>>That sky is only for immortals!\n");
      return;
    }
/* See if link from current room */
  for (i = 0; i < MAX_LINKS; ++i)
    {
      if (user->room->link[i] == rm)
	{
	  move_user (user, rm, 0);
	  return;
	}
    }
  if (user->level < MIN_LEV_GO)
    {
      sprintf (text, ">>>The %s is not adjoined to here.\n", rm->name);
      write_user (user, text);
      return;
    }
  move_user (user, rm, 1);
}


/*** Called by go() and move() ***/
move_user (user, rm, teleport)
     UR_OBJECT user;
     RM_OBJECT rm;
     int teleport;
{
  RM_OBJECT old_room;

  old_room = user->room;
/* Ignore gatecrash level if room is FIXED to private 'cos this may be one
   of the spirits rooms so let any user of SAINT and above in */
  if (teleport != 2 && !has_room_access (user, rm))
    {
      write_user (user,
		  ">>>That sky is currently private, you cannot enter.\n");
      return;
    }
/* Reset invite room if in it */
  if (user->invite_room == rm)
    user->invite_room = NULL;
  if (!user->vis)
    {
      write_room (rm, invisenter);
      write_room_except (user->room, invisleave, user);
      goto SKIP;
    }
  if (teleport == 1)
    {
      sprintf (text, "~FG~OL>>>%s appears in an explosion of green magic!\n",
	       user->name);
      write_room (rm, text);
      sprintf (text,
	       "~FG~OL>>>%s chants a spell and vanishes into a magical green vortex!\n",
	       user->name);
      write_room_except (old_room, text, user);
      goto SKIP;
    }
  if (teleport == 2)
    {
      write_user (user,
		  "\n~FG~OL>>>A giant hand grabs you and pulls you into an imaculate green vortex!\n");
      sprintf (text, "~FG~OL>>>%s falls out of an imaculate green vortex!\n",
	       user->name);
      write_room (rm, text);
      if (old_room == NULL)
	{
	  sprintf (text, "REL %s\n", user->savename);
	  write_sock (user->netlink->socket, text);
	  user->netlink = NULL;
	}
      else
	{
	  sprintf (text,
		   "~FG~OL>>>A giant hand grabs %s who is pulled into an imaculate green vortex!\n",
		   user->name);
	  write_room_except (old_room, text, user);
	}
      goto SKIP;
    }
  sprintf (text, ">>>%s %s.\n", user->name, user->in_phrase);
  write_room (rm, text);
  sprintf (text, ">>>%s %s to the %s.\n", user->name, user->out_phrase,
	   rm->name);
  write_room_except (user->room, text, user);

SKIP:
  user->room = rm;
  look (user);
  reset_access (old_room);
}


/*** Switch ignoring all on and off ***/
/*
toggle_ignall(user)
UR_OBJECT user;
{
if (!user->ignall) {
	write_user(user,">>>You are now ignoring everyone.\n");
	sprintf(text,">>>%s is now ignoring everyone.\n",user->name);
	write_room_except(user->room,text,user);
	user->ignall=1;
	return;
	}
write_user(user,">>>You will now hear everyone again.\n");
sprintf(text,">>>%s is listening again.\n",user->name);
write_room_except(user->room,text,user);
user->ignall=0;
}
*/

/*** Switch ignoring all on and off and manage ignores between users 
     Written by Victor Tarhon-Onu and fixed by Sabin-Corneliu Buraga ***/
ignore (user)
     UR_OBJECT user;
{
  if (word_count < 2)
    {
      if (!user->ignall)
	{
	  write_user (user, ">>>You are now ignoring everyone.\n");
	  sprintf (text, ">>>%s is now ignoring everyone.\n", user->name);
	  write_room_except (user->room, text, user);
	  user->ignall = 1;
	}
      else
	{
	  write_user (user, ">>>You will now hear everyone again.\n");
	  sprintf (text, ">>>%s is listening again.\n", user->name);
	  write_room_except (user->room, text, user);
	  user->ignall = 0;
	}
      return;
    }

  if (!strcmp (word[1], "+") || !strncmp (word[1], "add", strlen (word[1])))
    {
      if (word_count < 3)
	write_usage (user, "ignore + <user>");
      else
	add_ign_user (user, word[2]);
      return;
    }

  if (!strcmp (word[1], "-") || !strncmp (word[1], "del", strlen (word[1])))
    {
      if (word_count < 3)
	write_usage (user, "ignore - <user>");
      else
	del_ign_user (user, word[2]);
      return;
    }

  if (!strcmp (word[1], "#")
      || !strncmp (word[1], "forgive", strlen (word[1])))
    {
      if (word_count < 3)
	write_usage (user, "ignore # <user>");
      else
	forgive_ign_user (user, word[2]);
      return;
    }

  if (!strcmp (word[1], "?") || !strncmp (word[1], "list", strlen (word[1])))
    {
      list_ign_users (user);
      return;
    }
  write_usage (user, "ignore [ list/add/del/forgive <user> ]");
}

/*** Check if "user" is ignoring "u" ***/
is_ignoring (user, u)
     UR_OBJECT user, u;
{
  int i;

  if (u == NULL || user == NULL || !user->ign_number)
    return 0;

  for (i = 0; i < MAX_IGNRD_USERS; i++)
    if (user->ignored_users_list[i].name[0])
      if (!strcmp (user->ignored_users_list[i].name, u->savename))
	return 1;
  return 0;
}

/*** Add a new user to ignoring list ***/
add_ign_user (user, username)
     UR_OBJECT user;
     char *username;
{
  int i;
  UR_OBJECT u;

  if (user->ign_number >= MAX_IGNRD_USERS)
    {
      write_user (user,
		  ">>>Sorry, the maximum number of ignored users has been reached!\n");
      return 0;
    }

  if ((u = get_user (username)) == NULL)
    {
      sprintf (text,
	       ">>>Sorry, you can not ignore %s because is not logged on.\n",
	       username);
      write_user (user, text);
      return 0;
    }

  if (u == user)
    {
      write_user (user,
		  ">>>Trying to ignore yourself is the eighteenth sign of madness or genius.\n");
      return 0;
    }

  if (u->level >= user->level)
    {
      write_user (user,
		  ">>>You cannot ignore an user of equal or higher level than yourself.\n");
      return 0;
    }

  if (is_ignoring (user, u))
    {
      sprintf (text, ">>>You are already ignoring %s!\n", u->name);
      write_user (user, text);
      return 0;
    }

  for (i = 0; i < MAX_IGNRD_USERS; i++)	/* Finding a free slot... */
    if (!user->ignored_users_list[i].name[0])
      break;

  user->ign_number++;
  strcpy (user->ignored_users_list[i].name, u->savename);
  user->ignored_users_list[i].asked_for_forgiveness = 0;
  sprintf (text, "~OL~EL>>>%s is ignoring you! Better ask for forgiveness!\n",
	   user->name);
  write_user (u, text);
  sprintf (text, ">>>You are now ignoring %s.\n", u->name);
  write_user (user, text);
  return 1;
}

/*** Delete an user from ignoring list ***/
del_ign_user (user, username)
     UR_OBJECT user;
     char *username;
{
  int i;
  UR_OBJECT u;

  username[0] = toupper (username[0]);
  if ((u = get_user (username)) == NULL)
    {
      for (i = 0; i < user->ign_number; i++)
	if (!strcmp (user->ignored_users_list[i].name, username))
	  {
	    sprintf (text, ">>>%s removed from ignored users list.\n",
		     user->ignored_users_list[i].name);
	    write_user (user, text);
	    strcpy (user->ignored_users_list[i].name,
		    user->ignored_users_list[user->ign_number - 1].name);
	    user->ignored_users_list[user->ign_number - 1].name[0] = '\0';
	    user->ignored_users_list[user->ign_number -
				     1].asked_for_forgiveness = 0;
	    user->ign_number--;
	    return 1;
	  }
    }

  for (i = 0; i < user->ign_number; i++)
    {
      if (!strcmp (user->ignored_users_list[i].name, u->savename))
	{
	  strcpy (user->ignored_users_list[i].name,
		  user->ignored_users_list[user->ign_number - 1].name);
	  user->ignored_users_list[user->ign_number - 1].name[0] = '\0';
	  user->ignored_users_list[user->ign_number -
				   1].asked_for_forgiveness = 0;
	  user->ign_number--;
	  sprintf (text, "~OL>>>%s is no longer ignoring you! :-)\n",
		   user->name);
	  write_user (u, text);
	  sprintf (text, ">>>You are no longer ignoring %s.\n", u->name);
	  write_user (user, text);
	  return 1;
	}
    }

  sprintf (text, ">>>You are not ignoring %s!\n",
	   (u == NULL) ? username : u->savename);
  write_user (user, text);
  return 0;
}

/*** Ask to forgive an user... ***/
forgive_ign_user (user, username)
     UR_OBJECT user;
     char *username;
{
  int i;
  UR_OBJECT u;

  if ((u = get_user (username)) == NULL)
    {
      write_user (user, notloggedon);
      return;
    }

  if (!is_ignoring (u, user))
    {
      sprintf (text, ">>>%s is not ignoring you...!\n", u->name);
      write_user (user, text);
      return;
    }

  for (i = 0; i < u->ign_number; i++)
    if (!strcmp (u->ignored_users_list[i].name, user->savename))
      break;

  if (u->ignored_users_list[i].asked_for_forgiveness)
    {
      sprintf (text, ">>>You already asked %s to forgive you!\n", u->name);
      write_user (user, text);
      return;
    }
  else
    {
      u->ignored_users_list[i].asked_for_forgiveness = 1;
      sprintf (text,
	       ">>>%s asks you to forgive him and not to ignore him no more.\n",
	       user->name);
      write_user (u, text);
      sprintf (text, ">>>Forgive request for ignore sent to %s.\n", u->name);
      write_user (user, text);
    }
}

/*** List all ignored users ***/
list_ign_users (user)
     UR_OBJECT user;
{
  int i;

  if (!user->ign_number)
    {
      write_user (user, ">>>Your ignored users list is empty.\n");
      return;
    }
  else
    write_user (user, ">>>Your ignored users list:");

  for (i = 0; i < user->ign_number; i++)
    if (user->ignored_users_list[i].name[0])
      {
	sprintf (text, " %s", user->ignored_users_list[i].name);
	write_user (user, text);
      }

  write_user (user, ".\n");
}

/*** Switch prompt on and off ***/
toggle_prompt (user)
     UR_OBJECT user;
{
  if (user->prompt)
    {
      write_user (user, ">>>Prompt is ~FROFF.\n");
      user->prompt = 0;
      return;
    }
  write_user (user, ">>>Prompt is ~FGON.\n");
  user->prompt = 1;
}


/*** Set user description ***/
set_desc (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  if (word_count < 2)
    {
      sprintf (text, ">>>Your current description is: %s\n", user->desc);
      write_user (user, text);
      return;
    }
  if (strstr (word[1], "<CLONE>"))
    {
      write_user (user, ">>>You cannot have that description.\n");
      return;
    }
  if (strlen (inpstr) > USER_DESC_LEN)
    inpstr[USER_DESC_LEN] = '\0';
  if (user->muzzled)
    {
      write_user (user,
		  ">>>You are muzzled, you cannot change your description.\n");
      return;
    }
  strcpy (user->desc, inpstr);
  write_user (user, ">>>Description set.\n");
}


/*** Set in and out phrases ***/
set_iophrase (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  if (strlen (inpstr) > PHRASE_LEN)
    inpstr[PHRASE_LEN] = '\0';
  if (com_num == INPHRASE)
    {
      if (word_count < 2)
	{
	  sprintf (text, ">>>Your current in phrase is: %s\n",
		   user->in_phrase);
	  write_user (user, text);
	  return;
	}
      strcpy (user->in_phrase, inpstr);
      write_user (user, ">>>In phrase set.\n");
      return;
    }
  if (word_count < 2)
    {
      sprintf (text, ">>>Your current out phrase is: %s\n", user->out_phrase);
      write_user (user, text);
      return;
    }
  strcpy (user->out_phrase, inpstr);
  write_user (user, ">>>Out phrase set.\n");
}


/*** Set rooms to public or private ***/
set_room_access (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  RM_OBJECT rm;
  char *name;
  int cnt;

  rm = user->room;
  if (word_count < 2)
    rm = user->room;
  else
    {
      if (user->level < gatecrash_level)
	{
	  write_user (user,
		      ">>>You are not a high enough level to use the sky option.\n");
	  return;
	}
      if ((rm = get_room (word[1])) == NULL)
	{
	  write_user (user, nosuchroom);
	  return;
	}
    }
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  if (rm->access > PRIVATE)
    {
      if (rm == user->room)
	write_user (user, ">>>This sky's access is fixed.\n");
      else
	write_user (user, ">>>That sky's access is fixed.\n");
      return;
    }
  if (com_num == PUBCOM && rm->access == PUBLIC)
    {
      if (rm == user->room)
	write_user (user, ">>>This sky is already public.\n");
      else
	write_user (user, ">>>That sky is already public.\n");
      return;
    }
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  if (com_num == PRIVCOM)
    {
      if (rm->access == PRIVATE)
	{
	  if (rm == user->room)
	    write_user (user, ">>>This sky is already private.\n");
	  else
	    write_user (user, ">>>That sky is already private.\n");
	  return;
	}
      cnt = 0;
      for (u = user_first; u != NULL; u = u->next)
	if (u->room == rm)
	  ++cnt;
      if (cnt < min_private_users && user->level < ignore_mp_level)
	{
	  sprintf (text,
		   ">>>You need at least ~FM%d~RS people in a room before it can be made private.\n",
		   min_private_users);
	  write_user (user, text);
	  return;
	}
      write_user (user, ">>>Sky set to ~FRPRIVATE.\n");
      if (rm == user->room)
	{
	  sprintf (text, ">>>%s has set the sky to ~FRPRIVATE.\n", name);
	  write_room_except (rm, text, user);
	}
      else
	write_room (rm, ">>>This sky has been set to ~FRPRIVATE.\n");
      rm->access = PRIVATE;
      return;
    }
  write_user (user, ">>>Sky set to ~FGPUBLIC.\n");
  if (rm == user->room)
    {
      sprintf (text, ">>>%s has set the sky to ~FGPUBLIC.\n", name);
      write_room_except (rm, text, user);
    }
  else
    write_room (rm, ">>>This sky has been set to ~FGPUBLIC.\n");
  rm->access = PUBLIC;

/* Reset any invites into the room & clear review buffer */
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->invite_room == rm)
	u->invite_room = NULL;
    }
  clear_rbuff (rm);
}

/*** Ask to be let into a private room ***/
letmein (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  int i;

  if (word_count < 2)
    {
      write_user (user, ">>>Let you into where?\n");
      return;
    }
  if ((rm = get_room (word[1])) == NULL)
    {
      write_user (user, nosuchroom);
      return;
    }
  if (rm == user->room)
    {
      sprintf (text, ">>>You are already in the %s!\n", rm->name);
      write_user (user, text);
      return;
    }
  if (rm->hidden)
    {
      write_user (user, ">>>That sky is only for immortals!\n");
      return;
    }
  for (i = 0; i < MAX_LINKS; ++i)
    if (user->room->link[i] == rm)
      goto GOT_IT;
  sprintf (text, ">>>The %s is not adjoined to here.\n", rm->name);
  write_user (user, text);
  return;

GOT_IT:
  if (!(rm->access & 1))
    {
      sprintf (text, ">>>The %s is currently public.\n", rm->name);
      write_user (user, text);
      return;
    }
  sprintf (text, ">>>You shout asking to be let into the %s.\n", rm->name);
  write_user (user, text);
  sprintf (text, ">>>%s shouts asking to be let into the %s.\n", user->name,
	   rm->name);
  write_room_except (user->room, text, user);
  sprintf (text, ">>>%s shouts asking to be let in.\n", user->name);
  write_room (rm, text);
}


/*** Invite an user into a private room ***/
invite (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  RM_OBJECT rm;
  char *name;

  if (word_count < 2)
    {
      write_user (user, ">>>Invite who?\n");
      return;
    }
  rm = user->room;
  if (!(rm->access & 1))
    {
      write_user (user, ">>>This sky is currently public.\n");
      return;
    }
  if (!(u = get_user (word[1])))
    {
      write_user (user, notloggedon);
      return;
    }
  if (u == user)
    {
      write_user (user,
		  ">>>Inviting yourself to somewhere is the third sign of madness...or genius.\n");
      return;
    }
  if (u->room == rm)
    {
      sprintf (text, ">>>%s is already here!\n", u->name);
      write_user (user, text);
      return;
    }
  if (u->invite_room == rm)
    {
      sprintf (text, ">>>%s has already been invited into here.\n", u->name);
      write_user (user, text);
      return;
    }
  if (rm->hidden)
    {
      write_user (user, ">>>That sky is hidden...\n");
      return;
    }
  sprintf (text, ">>>You invite %s in.\n", u->name);
  write_user (user, text);
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  sprintf (text, ">>>%s has invited you into the %s.\n", name, rm->name);
  write_user (u, text);
  u->invite_room = rm;
}


/*** Set the room topic ***/
set_topic (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  RM_OBJECT rm;
  char *name;

  rm = user->room;
  if (word_count < 2)
    {
      if (!strlen (rm->topic))
	{
	  write_user (user, ">>>No topic has been set yet.\n");
	  return;
	}
      sprintf (text, ">>>The current topic is: %s\n", rm->topic);
      write_user (user, text);
      return;
    }
  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, you cannot change the topic.\n");
      return;
    }
  if (!strcmp (word[1], "wipe"))
    {
      rm->topic[0] = '\0';
      write_user (user, ">>>Topic wiped.\n");
      if (user->vis)
	name = user->name;
      else
	name = invisname;
      sprintf (text, ">>>%s has wiped the sky's topic.\n", name);
      write_room_except (rm, text, user);
      return;
    }
  if (strlen (inpstr) > TOPIC_LEN - USER_NAME_LEN - 20)
    {
      write_user (user, ">>>Topic too long.\n");
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  sprintf (text, ">>>Topic set to: %s\n", inpstr);
  write_user (user, text);
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  sprintf (text, ">>>%s has set the topic to: %s\n", name, inpstr);
  write_room_except (rm, text, user);
  sprintf (rm->topic, "~FT[%s %02d:%02d]~RS %s", name, thour, tmin, inpstr);
}


/*** Spirit moves an user to another room ***/
move (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  RM_OBJECT rm;
  char *name;

  if (word_count < 2)
    {
      write_usage (user, "move <user> [<sky>]");
      return;
    }
  if (!(u = get_user (word[1])))
    {
      write_user (user, notloggedon);
      return;
    }
  if (word_count < 3)
    rm = user->room;
  else
    {
      if ((rm = get_room (word[2])) == NULL)
	{
	  write_user (user, nosuchroom);
	  return;
	}
    }
  if (user == u)
    {
      write_user (user,
		  ">>>Trying to move yourself this way is the fourth sign of madness or genius.\n");
      return;
    }
  if (rm == u->room)
    {
      sprintf (text, ">>>%s is already in the %s.\n", u->name, rm->name);
      write_user (user, text);
      return;
    };
  if (u->level >= user->level)
    {
      write_user (user,
		  ">>>You cannot move an user of equal or higher level than yourself.\n");
      return;
    }
  if (u->restrict[RESTRICT_MOVE] == restrict_string[RESTRICT_MOVE])
    {
      write_user (user, ">>>This user is unmoveable...\n");
      return;
    }
  if (!has_room_access (user, rm))
    {
      sprintf (text,
	       ">>>The %s is currently private, %s cannot be moved there.\n",
	       rm->name, u->name);
      write_user (user, text);
      return;
    }
  write_user (user, "~FY~OL>>>You call the light power...\n");
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  sprintf (text, "~FY~OL>>>%s calls the light power...\n", name);
  write_room_except (user->room, text, user);
  u->dimension = user->dimension;
  move_user (u, rm, 2);
  prompt (u);
}

/*** Broadcast an important message ***/
bcast (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  if (word_count < 2)
    {
      write_usage (user, "bcast <message>");
      return;
    }
  if (user->muzzled)
    {
      write_user (user,
		  ">>>You cannot send broadcast messages, you're muzzled!\n");
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  force_listen = 1;
  if (user->vis)
    sprintf (text,
	     "~EL\n~BR--->>> Broadcast message from ~OL%s~RS~BR <<<---\n%s\n\n",
	     user->name, inpstr);
  else
    sprintf (text, "~EL\n~BR---> Broadcast message <<<---\n%s\n\n", inpstr);
  write_room (NULL, text);
  event = E_BCAST;
  strcpy (event_var, user->savename);
}


/*** Show who is on ***/
who (user, people)
     UR_OBJECT user;
     int people;
{
  UR_OBJECT u;
  int logins, cnt, total, invis, mins, remote, idle, cnt1, i, showall;
  char line[USER_NAME_LEN + USER_DESC_LEN * 2 + 6];
  char rname[ROOM_NAME_LEN + 1], portstr[5], idlestr[5], sockstr[3];
  int cntdims[MAX_DIMENS];

  if (user->restrict[RESTRICT_WHO] == restrict_string[RESTRICT_WHO])
    {
      write_user (user,
		  ">>>You have no right to use .who or .people! Sorry...\n");
      return;
    }
  showall = !(word_count >= 2 && 
             (word[1][0] == '-' || !strcmp (word[1], "brief"))); 
  total = 0;
  invis = 0;
  remote = 0;
  logins = 0;
  for (i = 0; i < MAX_DIMENS; i++)
    cntdims[i] = 0;
  if (showall) 
    {  
      if (user->login)
        sprintf (text, "\n--->>> Current users on %s <<<---\n\n", long_date (0));
      else
        sprintf (text,
	     "\n~FB~OL---~FR>~FY>~FB>    ~FT%s~RS~FB~OL    (current users) <<<---\n\n",
	     long_date (0));
      write_user (user, text);
      if (people)
        write_user (user,
		"~OL~FTName		: Level   Line Ignall Visi Idle Mins Port Site/Service:Port\n\n\r");
    }		
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->type == CLONE_TYPE)
	continue;
      mins = (int) (time (0) - u->last_login) / 60;
      idle = (int) (time (0) - u->last_input) / 60;
      if (u->type == REMOTE_TYPE)
	strcpy (portstr, "   -");
      else
	{
	  if (u->port == port[0])
	    strcpy (portstr, "MAIN");
	  else
	    strcpy (portstr, " ST.");
	}
      if (u->login)
	{
	  if (!people)
	    continue;
	  sprintf (text,
		   "~FT[Login stage %d] :	-   %2d      -	  - %3d    - %s %s:%d\n",
		   4 - u->login, u->socket, idle, portstr, u->site,
		   u->site_port);
	  write_user (user, text);
	  logins++;
	  continue;
	}
      ++total;
      if (u->type == REMOTE_TYPE)
	++remote;
      if (u->dimension > user->dimension)
	if (u->level > user->level)
	  continue;
      if (!u->vis)
	{
	  ++invis;
	  if (u->level > user->level)
	    continue;
	}
      for (i = 0; i < MAX_DIMENS; i++)
	if (i == u->dimension)
	  {
	    cntdims[i]++;
	    break;
	  }
      if (people)
	{
	  if (u->afk)
	    strcpy (idlestr, "AFK");
	  else
	    sprintf (idlestr, "%3d", idle);
	  if (u->type == REMOTE_TYPE)
	    strcpy (sockstr, " -");
	  else
	    sprintf (sockstr, "%2d", u->socket);
	  cnt = colour_com_count (level_name[u->slevel]);
	  cnt1 = colour_com_count (u->name);
	  sprintf (text, "%-*s : %-*s   %s    %s  %s %s %4d %s %s:%d\n",
		   15 + cnt1 * 3, u->name, 6 + cnt * 3, level_name[u->slevel],
		   sockstr, noyes1[u->ignall], noyes1[u->vis], idlestr, mins,
		   portstr, u->ssite, u->site_port);
	  write_user (user, text);
	  continue;
	}
      sprintf (line, "    %s %s~RS",
	       u->name, strrepl (u->desc, "{name}",
				 user->name[0] ? user->name : "<unknown>"));
      if (user->level >= MIN_LEV_VIEWDIM)
	{
	  line[1] = '0' + u->dimension + 1;
	  if (u->slevel < u->level)
	    line[2] = '<';
	  else if (u->slevel > u->level)
	    line[2] = '>';
	}
      if (!u->vis)
	line[0] = '*';
      if (u->muzzled)
	line[0] = '#';
      if (u->type == REMOTE_TYPE)
	line[1] = '@';
      if (user->level >= MIN_LEV_VIEWREN)
	if (strcmp (u->savename, u->name))
	  line[3] = '~';
      if (u->room == NULL)
	sprintf (rname, "@%s", u->netlink->service);
      else
	strcpy (rname, u->room->name);
      if (u->room != NULL && u->room->hidden)
	strcpy (rname, "<unknown>");
      if (u->ignall)
	line[0] = '%';
      if (idle)
	line[2] = '?';
      /* some special symbols */
      switch (u->misc_op)
	{
	  /* editor */
	case 3:
	case 4:
	case 5:
	case 10:
	  line[2] = ':';
	  break;
	  /* alert */
	case 8:
	  line[2] = '!';
	  break;
	  /* lock */
	case 9:
	  line[2] = '^';
	}
      /* Count number of colour coms to be taken account of when formatting */
      cnt = colour_com_count (line);
      cnt1 = colour_com_count (level_name[u->slevel]);
      sprintf (text, "%-*s :%-*s~RS:%-12s : ~FG%4d~RS\'",
	       45 + cnt * 3, line, 6 + cnt1 * 3, level_name[u->slevel], rname,
	       mins);
      strcat (text, u->afk ? "~OL~FY<AFK>~RS\n" : "\n");
      write_user (user, text);
    }
  if (showall)
    {  
       sprintf (text,
	   "\n>>>There are ~FT%d~RS visible, maybe ~FT%d~RS invisible, ~FT%d~RS remote users and ~FT%d~RS logins.\n>>>Total of about ~FT%d~RS users (total ~FT%d~RS in, ~FT%d~RS out, ~FT%d~RS swap).\n",
	   num_of_users - invis, invis, remote, logins, total, cnt_in,
	   cnt_out, cnt_swp);
       write_user (user, text);
       if (user->level > MIN_LEV_VIEWDIM)
         {
            write_user (user, ">>>Users in dimensions:");
            for (i = 0; i < MAX_DIMENS; i++)
	      {
	         sprintf (text, " %s~RS: (~FT%d~RS)", dimens_name[i], cntdims[i]);
	         write_user (user, text);
	      }
            write_user (user, "\n");
         }
      write_user (user, "\n");
   }   
}


/*** Return to home site ***/
home (user)
     UR_OBJECT user;
{
  if (user->room != NULL)
    {
      write_user (user,
		  ">>>You are already on your home system! You are nuts...?\n");
      return;
    }
  write_user (user, "~FB~OL>>>You (slowly?) traverse cyberspace...\n");
  sprintf (text, "REL %s\n", user->savename);
  write_sock (user->netlink->socket, text);
  sprintf (text, "NETLINK: User %s returned from %s.\n", user->savename,
	   user->netlink->service);
  write_syslog (LOG_LINK, text, LOG_TIME);
  user->room = user->netlink->connect_room;
  user->netlink = NULL;
  if (user->vis)
    {
      sprintf (text, ">>>%s %s\n", user->name, user->in_phrase);
      write_room_except (user->room, text, user);
    }
  else
    write_room_except (user->room, invisenter, user);
  look (user);
}


/*** Read the message board ***/
read_board (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  char filename[80], *name;
  int ret;

  if (word_count < 2)
    rm = user->room;
  else
    {
      if ((rm = get_room (word[1])) == NULL)
	{
	  write_user (user, nosuchroom);
	  return;
	}
      if (!has_room_access (user, rm))
	{
	  write_user (user,
		      ">>>That sky is currently private, you cannot read the board.\n");
	  return;
	}
    }
  if (rm->hidden_board > user->level)
    {
      write_user (user,
		  ">>>The message board is hidden, you cannot read it! Sorry...\n");
      return;
    }
  sprintf (text, "\n~FB~OL--->>> The ~FT%s~RS~FB~OL message board <<<---\n\n",
	   rm->name);
  write_user (user, text);
  sprintf (filename, "%s/%s.B", DATAFILES, rm->name);
  if (!(ret = more (user, user->socket, filename)))
    write_user (user, ">>>There are no messages on the board.\n\n");
  else if (ret == 1)
    user->misc_op = 2;
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  if (rm == user->room)
    {
      sprintf (text, ">>>%s reads the message board.\n", name);
      write_room_except (user->room, text, user);
    }
}

/*** Write on the message board ***/
write_board (user, inpstr, done_editing)
     UR_OBJECT user;
     char *inpstr;
     int done_editing;
{
  FILE *fp;
  int cnt, inp;
  char *ptr, *name, filename[80];

  if (user->muzzled)
    {
      write_user (user,
		  ">>>You are muzzled, you cannot write on the board.\n");
      return;
    }
  if (!done_editing)
    {
      if (word_count < 2)
	{
	  if (user->type == REMOTE_TYPE)
	    {
	      /* Editor won't work over netlink cos all the prompts will go
	         wrong, I'll address this in a later version. */
	      write_user (user,
			  ">>>Sorry, due to software limitations remote users cannot use the line editor.\n>>>Use the '~FT.write <mesg>~RS' method instead.\n");
	      return;
	    }
	  write_user (user,
		      "\n~FB~OL--->>> Writing board message <<<---\n\n");
	  user->misc_op = 3;
	  editor (user, NULL);
	  return;
	}
      ptr = inpstr;
      inp = 1;
    }
  else
    {
      ptr = user->malloc_start;
      inp = 0;
    }

  sprintf (filename, "%s/%s.B", DATAFILES, user->room->name);
  if (!(fp = fopen (filename, "a")))
    {
      sprintf (text, ">>>%s: cannot write to file. Sorry...\n", syserror);
      write_user (user, text);
      sprintf (text,
	       "ERROR: Couldn't open file %s to append in write_board().\n",
	       filename);
      write_syslog (LOG_ERR, text, LOG_NOTIME);
      return;
    }
  if (user->vis || user->level < SAINT)
    name = user->savename;
  else
    name = invisname;
/* The posting time (PT) is the time its written in machine readable form, this 
   makes it easy for this program to check the age of each message and delete 
   as appropriate in check_messages() */
  if (user->type == REMOTE_TYPE)
    sprintf (text, "PT: %d\r~OLFrom: %s@%s  %s\n",
	     (int) (time (0)), name, user->netlink->service, long_date (0));
  else
    sprintf (text, "PT: %d\r~OLFrom: %s  %s\n", (int) (time (0)), name,
	     long_date (0));
  fputs (text, fp);
  cnt = 0;
  while (*ptr != '\0')
    {
      putc (*ptr, fp);
      if (*ptr == '\n')
	cnt = 0;
      else
	++cnt;
      if (cnt == 80)
	{
	  putc ('\n', fp);
	  cnt = 0;
	}
      ++ptr;
    }
  if (inp)
    fputs ("\n\n", fp);
  else
    putc ('\n', fp);
  fclose (fp);
  write_user (user, ">>>You write the message on the board.\n");
  sprintf (text, ">>>%s writes a message on the board.\n",
	   user->vis ? user->name : invisname);
  write_room_except (user->room, text, user);
  sprintf (text, "%s writes a message on the %s's board.\n",
	   user->savename, user->room->name);
  write_syslog (LOG_COM, text, LOG_TIME);
  user->room->mesg_cnt++;
}


/*** Wipe some messages off the board ***/
wipe_board (user)
     UR_OBJECT user;
{
  int num, cnt, valid;
  char infile[80], line[82], id[82], *name;
  FILE *infp, *outfp;
  RM_OBJECT rm;

  if (word_count < 2
      || ((num = atoi (word[1])) < 1 && strcmp (word[1], "all")))
    {
      write_usage (user, "wipe <num>/all");
      return;
    }
  rm = user->room;
  if (rm->hidden_board)
    {
      write_user (user, ">>>You cannot wipe a hidden message board!\n");
      return;
    }
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  sprintf (infile, "%s/%s.B", DATAFILES, rm->name);
  if (!(infp = fopen (infile, "r")))
    {
      write_user (user, ">>>The message board is empty.\n");
      return;
    }
  if (!strcmp (word[1], "all"))
    {
      fclose (infp);
      s_unlink (infile);
      write_user (user, ">>>All messages deleted.\n");
      sprintf (text, ">>>%s wipes the message board.\n", name);
      write_room_except (rm, text, user);
      sprintf (text, "%s wiped all messages from the board in the %s.\n",
	       user->savename, rm->name);
      write_syslog (LOG_COM, text, LOG_TIME);
      rm->mesg_cnt = 0;
      return;
    }
  if (!(outfp = fopen ("tempfile", "w")))
    {
      sprintf (text, ">>>%s: couldn't open tempfile.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Couldn't open tempfile in wipe_board().\n", 
		    LOG_NOTIME);
      fclose (infp);
      return;
    }
  cnt = 0;
  valid = 1;
  fgets (line, 82, infp);	/* max of 80+newline+terminator = 82 */
  while (!feof (infp))
    {
      if (cnt <= num)
	{
	  if (*line == '\n')
	    valid = 1;
	  sscanf (line, "%s", id);
	  if (valid && !strcmp (id, "PT:"))
	    {
	      if (++cnt > num)
		fputs (line, outfp);
	      valid = 0;
	    }
	}
      else
	fputs (line, outfp);
      fgets (line, 82, infp);
    }
  fclose (infp);
  fclose (outfp);
  s_unlink (infile);
  if (cnt < num)
    {
      unlink ("tempfile");
      sprintf (text,
	       ">>>There were only ~FG%d~RS message(s) on the board, all now deleted.\n",
	       cnt);
      write_user (user, text);
      sprintf (text, ">>>%s wipes the message board.\n", name);
      write_room_except (rm, text, user);
      sprintf (text, "%s wiped all messages from the board in the %s.\n",
	       user->savename, rm->name);
      write_syslog (LOG_COM, text, LOG_TIME);
      rm->mesg_cnt = 0;
      return;
    }
  if (cnt == num)
    {
      unlink ("tempfile");	/* cos it'll be empty anyway */
      write_user (user, ">>>All messages deleted.\n");
      user->room->mesg_cnt = 0;
      sprintf (text, "%s wiped all messages from the board in the %s.\n",
	       user->savename, rm->name);
    }
  else
    {
      rename ("tempfile", infile);
      sprintf (text, ">>>%d message(s) deleted.\n", num);
      write_user (user, text);
      user->room->mesg_cnt -= num;
      sprintf (text, "%s wiped %d message(s) from the board in the %s.\n",
	       user->savename, num, rm->name);
    }
  write_syslog (LOG_COM, text, LOG_TIME);
  sprintf (text, ">>>%s wipes the message board.\n", name);
  write_room_except (rm, text, user);
}

/*** Search all the boards for the words given in the list. Rooms fixed to
	private will be ignore if the users level is less than gatecrash_level ***/
search_boards (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  FILE *fp;
  char filename[80], line[82], buff[(2 * MAX_LINES + 1) * 82], w1[81];
  int i, w, cnt, message, yes, room_given;

  if (word_count < 2)
    {
      write_usage (user, "search <word list>");
      return;
    }
/* Go through rooms */
  cnt = 0;
  for (i = 0; i < MAX_DIMENS; i++)
    for (rm = room_first[i]; rm != NULL; rm = rm->next)
      {
	sprintf (filename, "%s/%s.B", DATAFILES, rm->name);
	if (!(fp = fopen (filename, "r")))
	  continue;
	if (!has_room_access (user, rm) || rm->hidden_board > user->level)
	  {
	    fclose (fp);
	    continue;
	  }
	/* Go through file */
	fgets (line, 81, fp);
	yes = 0;
	message = 0;
	room_given = 0;
	buff[0] = '\0';
	while (!feof (fp))
	  {
	    if (*line == '\n')
	      {
		if (yes)
		  {
		    strcat (buff, "\n");
		    write_user (user, buff);
		  }
		message = 0;
		yes = 0;
		buff[0] = '\0';
	      }
	    if (!message)
	      {
		w1[0] = '\0';
		sscanf (line, "%s", w1);
		if (!strcmp (w1, "PT:"))
		  {
		    message = 1;
		    strcpy (buff, remove_first (remove_first (line)));
		  }
	      }
	    else
	      {
		if (strlen (buff) > (2 * MAX_LINES) * 82 - strlen (line))
		  buff[0] = '\0';
		strcat (buff, line);
	      }
	    for (w = 1; w < word_count; ++w)
	      {
		if (!yes && strstr (line, word[w]))
		  {
		    if (!room_given)
		      {
			sprintf (text,
				 "~FB~OL--->>> ~FT%s~RS~FB~OL <<<---\n\n",
				 rm->name);
			write_user (user, text);
			room_given = 1;
		      }
		    yes = 1;
		    cnt++;
		  }
	      }
	    fgets (line, 81, fp);
	  }
	if (yes)
	  {
	    strcat (buff, "\n");
	    write_user (user, buff);
	  }
	fclose (fp);
      }
  if (cnt)
    {
      sprintf (text, ">>>Total of ~FM%d~RS matching message(s).\n\n", cnt);
      write_user (user, text);
    }
  else
    write_user (user, ">>>No occurences found, try another pattern.\n");
}



/*** See review of conversation ***/
review (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  int i, line, cnt;

  if (user->restrict[RESTRICT_VIEW] == restrict_string[RESTRICT_VIEW])
    {
      write_user (user,
		  ">>>Sorry, you have no right to review the conversation.\n");
      return;
    }
  if (word_count < 2)
    rm = user->room;
  else
    {
      if ((rm = get_room (word[1])) == NULL)
	{
	  write_user (user, nosuchroom);
	  return;
	}
      if (!has_room_access (user, rm))
	{
	  write_user (user,
		      ">>>That sky is currently private, you cannot review the conversation. Sorry...\n");
	  return;
	}
      if (rm->hidden && user->level < MIN_LEV_HREV)
	{
	  write_user (user,
		      ">>>That sky is currently hidden, you cannot review the conversation...\n");
	  return;
	}
    }
  cnt = 0;
  for (i = 0; i < REVIEW_LINES; ++i)
    {
      line = (rm->revline + i) % REVIEW_LINES;
      if (rm->revbuff[line][0])
	{
	  cnt++;
	  if (cnt == 1)
	    {
	      sprintf (text,
		       "\n~FB~OL--->>> Review buffer for the ~FT%s~RS~FB~OL <<<---\n\n",
		       rm->name);
	      write_user (user, text);
	    }
	  write_user (user, rm->revbuff[line]);
	}
    }
  if (!cnt)
    write_user (user, ">>>Review buffer is empty.\n");
  else
    write_user (user, "\n~FB~OL----->>> End <<<-----\n\n");
  if (has_unread_mail (user))
    write_user (user, "~EL~FT~OL~LI--->>> UNREAD MAIL! <<<---\n");
}

/*** Do the help ***/
help (user)
     UR_OBJECT user;
{
  int ret;
  char filename[80];
  char *c;

  if (user->restrict[RESTRICT_HELP] == restrict_string[RESTRICT_HELP])
    {
      write_user (user, ">>>You have no access to view help files!\n");
      return;
    }
  if (has_unread_mail (user))
    write_user (user, "~EL~FT~OL~LI--->>> UNREAD MAIL! <<<---\n");
/* show main help */
  if (word_count < 2)
    {
      sprintf (filename, "%s/mainhelp", HELPFILES);
      if (!(ret = more (user, user->socket, filename)))
	{
	  write_user (user,
		      ">>>There is no main help at the moment. Sorry...\n");
	  return;
	}
      if (ret == 1)
	user->misc_op = 2;
      return;
    }
/* show commands list */
  if (!strcmp (word[1], "c") || !strcmp (word[1], "commands"))
    {
      help_commands (user);
      return;
    }
/* show credits */
  if (!strcmp (word[1], "credits"))
    {
      help_credits (user);
      return;
    }
/* show commands list by level */
  if (isnumber (word[1]))
    {
      if ((ret = word[1][0] - '0') > user->level)
	{
	  write_user (user,
		      ">>>Sorry, you cannot view the available commands for that level!\n");
	  return;
	}
      sprintf (text,
	       "\n\t\t~OL--->>> Available commands for %s~RS~OL level <<<---\n\n",
	       level_name[ret]);
      write_user (user, text);
      ret = help_commands_level (user, ret);
      sprintf (text, "\n>>>Total of ~FT%d~RS commands.\n", ret);
      write_user (user, text);
      write_user (user,
		  "\n>>>Type '~FG~OL.help <command name>~RS' for specific help on a command (if you want...)\n>>>Also, you can use '~FG~OL.help <level number>~RS' for all commands of specified level.\n>>>Remember, you can use a '~FG~OL.~RS' on its own to repeat your last command or speech.\n\n");
      return;
    }
/* Check for any illegal crap in searched for filename so they cannot list
   out the /etc/passwd file for instance. */
  c = word[1];
  while (*c)
    {
      if (*c == '.' || *c == '/')
	{
	  write_user (user,
		      ">>>Sorry, there is no help on that topic.\n>>>Please, try ~FG~OL.help~RS or ~FG~OL.help commands~RS...\n\n");
	  return;
	}
      ++c;
    }
/* show a help file */
  sprintf (filename, "%s/%s", HELPFILES, word[1]);
  if (!(ret = more (user, user->socket, filename)))
    write_user (user,
		">>>Sorry, there is no help on that topic.\n>>>Please, try ~FG~OL.help~RS or ~FG~OL.help commands~RS...\n\n");
  if (ret == 1)
    user->misc_op = 2;
/* show a hint after help */
  if (hint_at_help && user->hint_at_help)
    hint (user, 0);
}

/*** Show the credits. 
     Add your own credits here if you wish but PLEASE leave the credits intact. 
     Thanks. ***/
help_credits (user)
     UR_OBJECT user;
{
  sprintf (text,
	   "\n~FG~OL--->>> The Credits :) <<<---\n~FG~OLNUTS version %s, Copyright (C) Neil Robertson 1996.\n~FG~OLGAEN version %s, Copyright (C) Sabin Corneliu Buraga 1996-2001.\n",
	   VERSION, GAEN_VER);
  write_user (user, text);
  write_user (user,
	      "~BM             ~BB             ~BT             ~BG             ~BY             ~BR             ~RS\n");
  write_user (user,
	      "NUTS stands for Neils Unix Talk Server, a program which started out as a\nuniversity project in autumn 1992 and has progressed from thereon. In no\nparticular order thanks go to the following people who helped me develop or\n");
  write_user (user,
	      "debug this code in one way or another over the years:\n   ~OLDarren Seryck, Steve Guest, Dave Temple, Satish Bedi, Tim Bernhardt,\n   ~OLKien Tran, Jesse Walton, Pak Chan, Scott MacKenzie and Bryan McPhail.\n");
  write_user (user,
	      "Also thanks must go to anyone else who has emailed me with ideas and/or bug\nreports and all the people who have used NUTS over the intervening years.\n");
  write_user (user,
	      "If you wish to email me my address is '~FGneil@ogham.demon.co.uk~RS' and should\nremain so for the forseeable future.\t~OLNeil Robertson - November 1996.\n");
  write_user (user,
	      "\n~FG~OLGAEN is copyright of Sabin Corneliu Buraga (c)1996-2001~RS - ~FG~OL~LIbusaco@infoiasi.ro\n");
  write_user (user,
	      "~FG~OLGAEN~RS distribution page: ~FY~OLhttp://www.infoiasi.ro//~busaco/gaen/\n");
  write_user (user,
	      "   Thanks for help/suggestions to ~OLVictor Tarhon-Onu, Stefan Kocsis,\n   ~OLBogdan Petcu, Sergiu Craciun, Bogdan Craciun and many others!\n");
  write_user (user,
	      "~BM             ~BB             ~BT             ~BG             ~BY             ~BR             ~RS\n");
}

/*** Show the command available ***/
help_commands (user)
     UR_OBJECT user;
{
  int lev, total = 0;

  sprintf (text,
	   "\n~OL--->>> Available commands for %s~RS~OL level <<<---\n\n",
	   level_name[user->level]);
  write_user (user, text);

  for (lev = GUEST; lev <= user->level; ++lev)
    {
      sprintf (text, "~OL~FB--->>> ~RS%s~RS~OL~FB <<<---\n", level_name[lev]);
      write_user (user, text);
      total += help_commands_level (user, lev);
    }
  sprintf (text, "\n>>>Total of ~FT%d~RS commands.", total);
  write_user (user, text);
  write_user (user,
	      "\n>>>Type '~FG~OL.help <command name>~RS' for specific help on a command (if you want...)\n>>>Also, you can use '~FG~OL.help <level number>~RS' for all commands of specified level.\n>>>Remember, you can use a '~FG~OL.~RS' on its own to repeat your last command or speech.\n\n");
  if (random_bhole)
    blackhole (NULL);
}

/*** write commands for level 'lev' ***/
help_commands_level (user, lev)
     UR_OBJECT user;
     int lev;
{
  int com, cnt, even = 0, ncmds = 0;
  char temp[20];

  com = 0;
  cnt = 0;
  text[0] = '\0';
  while (command[com][0] != '*')
    {
      if (com_level[com] != lev)
	{
	  com++;
	  continue;
	}
      sprintf (temp, "%s%-10s ", even ? "~FT" : "~FG", command[com]);
      strcat (text, temp);
      even = !even;
      ncmds++;
      if (cnt == 6)
	{
	  strcat (text, "\n");
	  write_user (user, text);
	  text[0] = '\0';
	  cnt = -1;
	  even = !even;
	}
      com++;
      cnt++;
    }
  if (cnt)
    {
      strcat (text, "\n");
      write_user (user, text);
    }
  return (ncmds);
}

/*** Show some user stats ***/
status (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  char ir[ROOM_NAME_LEN + 1];
  int days, hours, mins, hs;

  if (word_count < 2 || user->level < MIN_LEV_OSTAT)
    {
      u = user;
      write_user (user,
		  "\n~FB~OL---------------->>> Your status <<<----------------\n\n");
    }
  else
    {
      if (!(u = get_user (word[1])))
	{
	  write_user (user, notloggedon);
	  return;
	}
      if (u->level > user->level)
	{
	  write_user (user,
		      ">>>You cannot stat an user of higher level than yourself.\n");
	  return;
	}
      sprintf (text,
	       "\n~FB~OL---------------->>> %s~RS~FB~OL's status <<<----------------\n\n",
	       u->name);
      write_user (user, text);
    }
  if (u->invite_room == NULL)
    strcpy (ir, "<nowhere>");
  else
    strcpy (ir, u->invite_room->name);
  sprintf (text, "Level       : %s~RS\t\tIgnoring all: %s\n",
	   level_name[u->slevel], noyes2[u->ignall]);
  write_user (user, text);
  sprintf (text, "Ign. shouts : %s\t\tIgn. tells  : %s\n",
	   noyes2[u->ignshout], noyes2[u->igntell]);
  write_user (user, text);
  sprintf (text, "Ign. pict.  : %s\t\tWrapping    : %s (%d)\n",
	   noyes2[u->ignpict], noyes2[u->wrap], wrap_cols);
  write_user (user, text);
  if (u->type == REMOTE_TYPE || u->room == NULL)
    hs = 0;
  else
    hs = 1;
  sprintf (text, "On home site: %s\t\tVisible     : %s\n",
	   noyes2[hs], noyes2[u->vis]);
  write_user (user, text);
  sprintf (text, "Muzzled     : %s\t\tUnread mail : %s\n",
	   noyes2[(u->muzzled > 0)], noyes2[has_unread_mail (u)]);
  write_user (user, text);
  sprintf (text,
	   "Char echo   : %s\t\tColour      : %s\nInvited to  : %-10s\tHint at help: %s\n",
	   noyes2[u->charmode_echo],
	   u->colour == 2 ? "transparent" : offon[u->colour], ir,
	   noyes2[u->hint_at_help]);
  write_user (user, text);
  sprintf (text, ".say tone   : %s\t\t.tell tone  : %s\n",
	   tone_codes[u->say_tone], tone_codes[u->tell_tone]);
  write_user (user, text);
  sprintf (text, "Hangman     : %s ( %c )\t\t",
	   noyes2[u->hangman != NULL],
	   u->hangman == NULL ? '-' : '0' + u->hangman->state);
  write_user (user, text);
  sprintf (text, "Puzzle      : %s\n", noyes2[u->puzzle != NULL]);
  write_user (user, text);
  sprintf (text, "Paintball # : %3d\t\tPaged lines : %d\n", u->pballs,
	   u->plines);
  write_user (user, text);
  if (user->level > MIN_LEV_OSTAT)
    {
      sprintf (text, "Restrictions: %s\tBlind mode  : %s\n",
	       u->restrict, noyes2[u->blind]);
      write_user (user, text);
    }
  sprintf (text, "Description : %s\nIn phrase   : %s\nOut phrase  : %s\n",
	   strrepl (u->desc, "{name}", user->name), u->in_phrase,
	   u->out_phrase);
  write_user (user, text);
  sprintf (text, "Dimension   : %d (%s~RS)\n",
	   u->dimension + 1, dimens_name[u->dimension]);
  write_user (user, text);
  mins = (int) (time (0) - u->last_login) / 60;
  sprintf (text, "Online for  : ~FG%d~RS minutes\n", mins);
  write_user (user, text);
  days = u->total_login / 86400;
  hours = (u->total_login % 86400) / 3600;
  mins = (u->total_login % 3600) / 60;
  sprintf (text,
	   "Total login : ~FG%d~RS days, ~FG%d~RS hours, ~FG%d~RS minutes\n\n",
	   days, hours, mins);
  write_user (user, text);
  if (u->level >= MIN_LEV_EX)
    {
      if (u == user)
	return;
      sprintf (text, "~UL>>>%s is looking at you!\n",
	       user->vis ? user->name : invisname);
      write_user (u, text);
      sprintf (text, ">>>%s says: %s\n",
	       u->vis ? u->name : invisname,
	       u->exam_mesg[0] ? u->exam_mesg : exam_mesg);
      write_user (user, text);
    }
}

/*** Read your mail ***/
rmail (user)
     UR_OBJECT user;
{
  FILE *infp, *outfp;
  int ret, c;
  char filename[80], line[DNL + 1];

  sprintf (filename, "%s/%s.M", USERFILES, user->savename);
  if (!(infp = fopen (filename, "r")))
    {
      write_user (user, ">>>You have no mail.\n");
      return;
    }
/* Update last read / new mail recieved time at head of file */
  if (outfp = fopen ("tempfile", "w"))
    {
      fprintf (outfp, "%d\r", (int) (time (0)));
      /* Skip first line of mail file */
      fgets (line, DNL, infp);

      /* Copy rest of file */
      c = getc (infp);
      while (!feof (infp))
	{
	  putc (c, outfp);
	  c = getc (infp);
	}

      fclose (outfp);
      rename ("tempfile", filename);
    }
  user->read_mail = time (0);
  fclose (infp);
  write_user (user, "\n~FB~OL--->>> Your mail <<<---\n\n");
  ret = more (user, user->socket, filename);
  if (ret == 1)
    user->misc_op = 2;
}

/*** Send mail message ***/
smail (user, inpstr, done_editing)
     UR_OBJECT user;
     char *inpstr;
     int done_editing;
{
  UR_OBJECT u;
  FILE *fp;
  int remote, has_account;
  char *c, filename[80];

  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, you cannot mail anyone.\n");
      return;
    }
  if (done_editing)
    {
      send_mail (user, user->mail_to, user->malloc_start);
      user->mail_to[0] = '\0';
      return;
    }
  if (word_count < 2)
    {
      write_user (user, ">>>Smail who?\n");
      return;
    }
/* See if it's to another site */
  remote = 0;
  has_account = 0;
  c = word[1];
  while (*c)
    {
      if (*c == '@')
	{
	  if (c == word[1])
	    {
	      write_user (user, ">>>Name missing before @ sign.\n");
	      return;
	    }
	  remote = 1;
	  break;
	}
      ++c;
    }
  word[1][0] = toupper (word[1][0]);
/* See if user exists */
  if (!remote)
    {
      u = NULL;
      if (!(u = get_user (word[1])))
	{
	  sprintf (filename, "%s/%s.D", USERFILES, word[1]);
	  if (!(fp = fopen (filename, "r")))
	    {
	      write_user (user, nosuchuser);
	      return;
	    }
	  has_account = 1;
	  fclose (fp);
	}
      if (u == user)
	{
	  write_user (user,
		      ">>>Trying to mail yourself is the fifth sign of madness... or genius.\n");
	  return;
	}
      if (u != NULL)
	strcpy (word[1], u->savename);
      if (!has_account)
	{
	  /* See if user has local account */
	  sprintf (filename, "%s/%s.D", USERFILES, word[1]);
	  if (!(fp = fopen (filename, "r")))
	    {
	      sprintf (text,
		       ">>>%s is a remote user and does not have a local account.\n",
		       u->name);
	      write_user (user, text);
	      return;
	    }
	  fclose (fp);
	}
    }
  if (word_count > 2)
    {
      /* One line mail */
      strcat (inpstr, "\n");
      send_mail (user, word[1], remove_first (inpstr));
      return;
    }
  if (user->type == REMOTE_TYPE)
    {
      write_user (user,
		  ">>>Sorry, due to software limitations remote users cannot use the line editor.\nUse the '.smail <user> <mesg>' method instead.\n");
      return;
    }
  sprintf (text,
	   "\n~FB~OL--->>> Writing mail message to ~FT%s~RS~FB~OL <<<---\n\n",
	   word[1]);
  write_user (user, text);
  user->misc_op = 4;
  strcpy (user->mail_to, word[1]);
  editor (user, NULL);
}

/*** Delete some or all of your mail. A problem here is once we have deleted
     some mail from the file do we mark the file as read? If not we could
     have a situation where the user deletes all his mail but still gets
     the YOU HAVE UNREAD MAIL message on logging in if the idiot forgot to
     read it first. ***/
dmail (user)
     UR_OBJECT user;
{
  FILE *infp, *outfp;
  int num, valid, cnt;
  char filename[80], w1[ARR_SIZE], line[ARR_SIZE];

  if (word_count < 2
      || ((num = atoi (word[1])) < 1 && strcmp (word[1], "all")))
    {
      write_usage (user, "dmail <number of messages>/all\n");
      return;
    }
  sprintf (filename, "%s/%s.M", USERFILES, user->savename);
  if (!(infp = fopen (filename, "r")))
    {
      write_user (user, ">>>You have no mail to delete.\n");
      return;
    }
  if (!strcmp (word[1], "all"))
    {
      fclose (infp);
      s_unlink (filename);
      write_user (user, ">>>All mail deleted.\n");
      return;
    }
  if (!(outfp = fopen ("tempfile", "w")))
    {
      sprintf (text, ">>>%s: couldn't open tempfile.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR, "ERROR: Couldn't open tempfile in dmail().\n",
		    LOG_NOTIME);
      fclose (infp);
      return;
    }
  fprintf (outfp, "%d\r", (int) time (0));
  user->read_mail = time (0);
  cnt = 0;
  valid = 1;
  fgets (line, DNL, infp);	/* Get header date */
  fgets (line, ARR_SIZE - 1, infp);
  while (!feof (infp))
    {
      if (cnt <= num)
	{
	  if (*line == '\n')
	    valid = 1;
	  sscanf (line, "%s", w1);
	  if (valid && !strcmp (w1, "From:"))
	    {
	      if (++cnt > num)
		fputs (line, outfp);
	      valid = 0;
	    }
	}
      else
	fputs (line, outfp);
      fgets (line, ARR_SIZE - 1, infp);
    }
  fclose (infp);
  fclose (outfp);
  s_unlink (filename);
  if (cnt < num)
    {
      unlink ("tempfile");
      sprintf (text,
	       ">>>There were only ~FG%d~RS message(s) in your mailbox, all now deleted.\n",
	       cnt);
      write_user (user, text);
      return;
    }
  if (cnt == num)
    {
      unlink ("tempfile");	/* cos it'll be empty anyway */
      write_user (user, ">>>All messages deleted.\n");
      user->room->mesg_cnt = 0;
    }
  else
    {
      rename ("tempfile", filename);
      sprintf (text, ">>>%d message(s) deleted.\n", num);
      write_user (user, text);
    }
}


/*** Show list of people your mail is from without seeing the whole lot 
     If only_count=1, then only it counts mail messages ***/
mail_from (user, only_count)
     UR_OBJECT user;
     int only_count;
{
  FILE *fp;
  int valid, cnt;
  char w1[ARR_SIZE], line[ARR_SIZE], filename[80];

  sprintf (filename, "%s/%s.M", USERFILES, user->savename);
  if (!(fp = fopen (filename, "r")))
    {
      if (!only_count)
	write_user (user, ">>>You have no mail.\n");
      return;
    }
  if (!only_count)
    write_user (user, "\n~FB~OL--->>> Mail from <<<---\n\n");
  valid = 1;
  cnt = 0;
  fgets (line, DNL, fp);
  fgets (line, ARR_SIZE - 1, fp);
  while (!feof (fp))
    {
      if (*line == '\n')
	valid = 1;
      sscanf (line, "%s", w1);
      if (valid && !strcmp (w1, "From:"))
	{
	  if (!only_count)
	    write_user (user, remove_first (line));
	  cnt++;
	  valid = 0;
	}
      fgets (line, ARR_SIZE - 1, fp);
    }
  fclose (fp);
  if (!only_count)
    sprintf (text, "\n>>>Total of ~FM%d~RS message(s).\n\n", cnt);
  else
    sprintf (text,
	     "\n~EL~OL--->>> You have ~FM%d~RS~OL message(s) in your mailbox. <<<---\n\n",
	     cnt);
  write_user (user, text);
}


/*** Enter user profile command interface ***/
enter_profile_com (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  FILE *fp;
  char filename[80];
  if (word_count < 2)
    enter_profile (user, 0);	/* call editor */
  else
    {				/* add a new line to profile */
      sprintf (filename, "%s/%s.P", USERFILES, user->savename);
      if (!(fp = fopen (filename, "a")))
	{
	  sprintf (text, ">>>%s: couldn't append your profile.\n", syserror);
	  write_user (user, text);
	  write_syslog (LOG_ERR,
			"ERROR: Couldn't append in enter_profile_com()\n", 
			LOG_NOTIME);
	  return;
	}
      if (strlen (inpstr) > 80)
	inpstr[80] = '\0';
      strcat (inpstr, "\n");
      fputs (inpstr, fp);
      fclose (fp);
      write_user (user, ">>>Profile updated.\n");
    }
}


/*** Enter user profile ***/
enter_profile (user, done_editing)
     UR_OBJECT user;
     int done_editing;
{
  FILE *fp;
  char *c, filename[80];

  if (!done_editing)
    {
      write_user (user, "\n~FB~OL--->>> Writing profile <<<---\n\n");
      user->misc_op = 5;
      editor (user, NULL);
      return;
    }
  sprintf (filename, "%s/%s.P", USERFILES, user->savename);
  if (!(fp = fopen (filename, "w")))
    {
      sprintf (text, ">>>%s: couldn't save your profile.\n", syserror);
      write_user (user, text);
      sprintf (text,
	       "ERROR: Couldn't open file %s to write in enter_profile().\n",
	       filename);
      write_syslog (LOG_ERR, text, LOG_NOTIME);
      return;
    }
  c = user->malloc_start;
  while (c != user->malloc_end)
    putc (*c++, fp);
  fclose (fp);
  write_user (user, ">>>Profile stored. Thanks...\n");
}


/*** Examine an user ***/
examine (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  UR_OBJECT u;
  FILE *fp;
  char filename[80], line[82], afk[6], site[80];
  int last_login, total_login, last_read, new_mail, level, slevel, loglen;
  int days, hours, mins, ago, onfor, days2, hours2, mins2, idle;
  int d1, d2, d3, d4, d5;
  char g1[4], g2[4];

  char country[WORD_LEN + 1], age[WORD_LEN + 1],
    firstname[WORD_LEN + 1], lastname[WORD_LEN + 1], email[WORD_LEN + 1],
    sex[11];

  if (word_count < 2)
    {
      write_usage (user, "examine <user>/setmsg [<text>]");
      return;
    }
/* set reply message */
  if (word[1][0] == '=' || !strcmp (word[1], "setmsg"))
    {
      if (word_count < 3)
	{
	  sprintf (text, ">>>Examine reply message: ~FT%s~RS\n",
		   user->exam_mesg[0] ? user->exam_mesg : "<none>");
	  write_user (user, text);
	}
      else
	{
	  inpstr = remove_first (inpstr);
	  if (strlen (inpstr) > EXAM_MESG_LEN)
	    inpstr[EXAM_MESG_LEN] = '\0';
	  strcpy (user->exam_mesg, inpstr);
	  write_user (user, ">>>Examine reply message set.\n");
	}
      return;
    }
  word[1][0] = toupper (word[1][0]);
  sprintf (filename, "%s/%s.D", USERFILES, word[1]);
  if (!(fp = fopen (filename, "r")))
    {
      write_user (user,
		  "~FR~OL>>>There is no such user or remote user does not have a local account.\n");
      return;
    }
  else
    fscanf (fp, "%s\n%d %d %d %d %d",
	    line, &last_login, &total_login, &loglen, &last_read, &level);
  if (level < 0 || level > SGOD)
    level = 0;
  fscanf (fp, "%d %d %d %d %d\n", &d1, &d2, &d3, &d4, &d5);
  fgets (line, 80, fp);		/* user host (ignored) */
  fgets (line, 80, fp);
  fclose (fp);
  line[strlen (line) - 1] = '\0';
  sprintf (text,
	   "\n~FB~OL---------------->>> ~RS~OL%s %s~RS~FB~OL <<<----------------\n\n",
	   word[1], strrepl (line, "{name}", user->name));
  write_user (user, text);
/* load user's information... */
  sprintf (filename, "%s/%s.U", USERFILES, word[1]);
  if (!(fp = fopen (filename, "r")))
    {
      strcpy (firstname, "Anonymous");
      strcpy (lastname, "Dreamer");
      strcpy (sex, "female");
      strcpy (age, "?");
      sprintf (email, "%s@insanity.ro", word[1]);
      strcpy (country, "Romania");
    }
  else
    {
      fgets (firstname, WORD_LEN + 2, fp);
      firstname[strlen (firstname) - 1] = '\0';
      fgets (lastname, WORD_LEN + 2, fp);
      lastname[strlen (lastname) - 1] = '\0';
      fgets (sex, 10, fp);
      sex[strlen (sex) - 1] = '\0';
      fgets (age, WORD_LEN + 2, fp);
      age[strlen (age) - 1] = '\0';
      fgets (email, WORD_LEN + 2, fp);
      email[strlen (email) - 1] = '\0';
      fgets (country, WORD_LEN + 2, fp);
      country[strlen (country) - 1] = '\0';
      fclose (fp);
    }
/* get user gender */
  sprintf (g1, "%s", (sex[0] == 'm' || sex[0] == 'M') ? "His" : "Her");
  sprintf (g2, "%s", (sex[0] == 'm' || sex[0] == 'M') ? "he" : "she");
  sprintf (text,
	   "~OL(Details)~RS: %s real name is ~OL~FY%s %s~RS and %s is from ~FG~OL%s~RS.\n",
	   g1, firstname, lastname, g2, country);
  write_user (user, text);
  sprintf (text,
	   "Also, %s is a ~OL~FT%s~RS years old person. E-mail address: ~FB~OL%s~RS.\n",
	   g2, age, email);
  write_user (user, text);

/* read substitution file */
  sprintf (filename, "%s/%s.S", USERFILES, word[1]);
  if ((fp = fopen (filename, "r")) != NULL)
    {
      fgets (site, 80, fp);
      site[strlen (site) - 1] = '\0';
      fscanf (fp, "%d", &slevel);
      if (slevel < 0 || slevel > SGOD)
	slevel = level;
      fclose (fp);
    }
  else
    slevel = level;
/* Show profile information */
  write_user (user, "~OL(Profile)\n");
  sprintf (filename, "%s/%s.P", USERFILES, word[1]);
  if (!(fp = fopen (filename, "r")))
    write_user (user, ">>>No profile.\n");
  else
    {
      fgets (line, 81, fp);
      while (!feof (fp))
	{
	  strcpy (text, strrepl (line, "{name}", user->name));
	  strcpy (text, strrepl (text, "{level}", level_name[user->slevel]));
	  write_user (user, text);
	  fgets (line, 81, fp);
	}
      fclose (fp);
    }
/* check if user has new mail */
  sprintf (filename, "%s/%s.M", USERFILES, word[1]);
  if (!(fp = fopen (filename, "r")))
    new_mail = 0;
  else
    {
      fscanf (fp, "%d", &new_mail);
      fclose (fp);
    }
/* Show login information for an user */
  if (!(u = get_exact_user (word[1])) || u->login)
    {
      days = total_login / 86400;
      hours = (total_login % 86400) / 3600;
      mins = (total_login % 3600) / 60;
      ago = (int) (time (0) - last_login);
      days2 = ago / 86400;
      hours2 = (ago % 86400) / 3600;
      mins2 = (ago % 3600) / 60;
      sprintf (text, "~OL(Other info)~RS:\n>>>Level           : %s\n"
	       ">>>Last login      : ~FG%s",
	       level_name[slevel], ctime ((time_t *) & last_login));
      write_user (user, text);
      sprintf (text,
	       ">>>Which was       : ~FG%d~RS days, ~FG%d~RS hours, ~FG%d~RS minutes ago\n",
	       days2, hours2, mins2);
      write_user (user, text);
      sprintf (text, ">>>Was on for      : ~FG%d~RS hours, ~FG%d~RS minutes\n"
	       ">>>Total login     : ~FG%d~RS days, ~FG%d~RS hours, ~FG%d~RS minutes\n",
	       loglen / 3600, (loglen % 3600) / 60, days, hours, mins);
      write_user (user, text);
      if (new_mail > last_read)
	{
	  sprintf (text, ">>>%s has unread mail.\n", word[1]);
	  write_user (user, text);
	}
      write_user (user, "\n");
      return;
    }
  days = u->total_login / 86400;
  hours = (u->total_login % 86400) / 3600;
  mins = (u->total_login % 3600) / 60;
  onfor = (int) (time (0) - u->last_login);
  hours2 = (onfor % 86400) / 3600;
  mins2 = (onfor % 3600) / 60;
  if (u->afk)
    strcpy (afk, "<AFK>");
  else
    afk[0] = '\0';
  idle = (int) (time (0) - u->last_input) / 60;
  sprintf (text, "~OL(Other info)~RS:\n>>>Level           : %s\n"
	   ">>>Ignoring all    : %s\n", level_name[u->slevel],
	   noyes2[u->ignall]);
  write_user (user, text);
  sprintf (text, ">>>On since        : ~FG%s"
	   ">>>On for          : ~FG%d~RS hours, ~FG%d~RS minutes\n",
	   ctime ((time_t *) & u->last_login), hours2, mins2);
  write_user (user, text);
  if (u->afk)
    {
      sprintf (text, ">>>AFK message     : ~FT%s\n", u->afkmesg);
      write_user (user, text);
    }
  sprintf (text, ">>>Idle for        : ~FG%d~RS minutes %s\n"
	   ">>>Total login     : ~FG%d~RS days, ~FG%d~RS hours, ~FG%d~RS minutes\n",
	   idle, afk, days, hours, mins);

  write_user (user, text);
  if (u->socket == -1)
    {
      sprintf (text, ">>>Home site       : %s\n", u->netlink->service);
      write_user (user, text);
    }
  if (new_mail > u->read_mail)
    {
      sprintf (text, ">>>%s has unread mail.\n", word[1]);
      write_user (user, text);
    }
  write_user (user, "\n");
  if (u->level >= MIN_LEV_EX)
    {
      if (u == user)
	return;
      sprintf (text, "~UL>>>%s is examining you!\n",
	       user->vis ? user->name : invisname);
      write_user (u, text);
      sprintf (text, ">>>%s says: %s\n",
	       u->vis ? u->name : invisname,
	       u->exam_mesg[0] ? u->exam_mesg : exam_mesg);
      write_user (user, text);
    }
}

/*** Show talker rooms ***/
rooms (user, show_topics)
     UR_OBJECT user;
     int show_topics;
{
  RM_OBJECT rm;
  UR_OBJECT u;
  NL_OBJECT nl;
  char access[15], stat[9], serv[SERV_NAME_LEN + 1];
  int cnt, dim;

  if (word_count >= 2 && isnumber (word[1]))
    dim = atoi (word[1]) - 1;
  else
    dim = user->dimension;	/* default dimension is user's dimension */
  if (dim < 0 || dim >= MAX_DIMENS)
    dim = user->dimension;
  if (show_topics)
    sprintf (text,
	     "\n~FB~OL------>>> Skies data ( dimension ~RS~OL%s~RS~FB~OL ) <<<------\n\n~FTSky name             : Access  Users  Mesgs  Topic\n\n",
	     dimens_name[dim]);
  else
    sprintf (text,
	     "\n~FB~OL------>>> Skies data ( dimension ~RS~OL%s~RS~FB~OL ) <<<------\n\n~FTSky name  	     : Access  Users  Mesgs  Inlink  LStat  Service\n\n",
	     dimens_name[dim]);
  write_user (user, text);
  for (rm = room_first[dim]; rm != NULL; rm = rm->next)
    {
      if (rm->access & 1)
	strcpy (access, "    ~FRPRIV");
      else
	strcpy (access, "     ~FGPUB");
      if (rm->access & 2)
	access[0] = '*';
      if (rm->hidden)
	access[1] = 'H';
      if (rm->hidden_board)
	access[2] = rm->hidden_board + '0';
      cnt = 0;
      for (u = user_first; u != NULL; u = u->next)
	if (u->type != CLONE_TYPE && u->room == rm)
	  ++cnt;
      if (show_topics)
	sprintf (text, "%-20s : %11s~RS	%3d    %3d  %s\n",
		 rm->name, access, cnt, rm->mesg_cnt, rm->topic);
      else
	{
	  nl = rm->netlink;
	  serv[0] = '\0';
	  if (nl == NULL)
	    {
	      if (rm->inlink)
		strcpy (stat, "~FRDOWN");
	      else
		strcpy (stat, "   -");
	    }
	  else if (nl->type == UNCONNECTED)
	    strcpy (stat, "~FRDOWN");
	  else if (nl->stage == UP)
	    strcpy (stat, "  ~FGUP");
	  else
	    strcpy (stat, " ~FYVER");
	  if (nl != NULL)
	    strcpy (serv, nl->service);
	  sprintf (text,
		   "%-20s : %11s~RS	%3d    %3d     %s   %s~RS  %s\n",
		   rm->name, access, cnt, rm->mesg_cnt, noyes1[rm->inlink],
		   stat, serv);
	}
      write_user (user, text);
    }
  write_user (user, "\n");
}


/*** List defined netlinks and their status ***/
netstat (user)
     UR_OBJECT user;
{
  NL_OBJECT nl;
  UR_OBJECT u;
  char *allow[] = { "  ?", "ALL", " IN", "OUT" };
  char *type[] = { "  -", " IN", "OUT" };
  char portstr[6], stat[9], vers[8];
  int iu, ou, a;

  if (nl_first == NULL)
    {
      write_user (user, ">>>No remote connections configured.\n");
      return;
    }
  write_user (user,
	      "\n~FB~OL--->>> Netlink data & status <<<---\n\n~FTService name	  : Allow Type Status IU OU Version  Site\n\n");
  for (nl = nl_first; nl != NULL; nl = nl->next)
    {
      iu = 0;
      ou = 0;
      if (nl->stage == UP)
	{
	  for (u = user_first; u != NULL; u = u->next)
	    {
	      if (u->netlink == nl)
		{
		  if (u->type == REMOTE_TYPE)
		    ++iu;
		  if (u->room == NULL)
		    ++ou;
		}
	    }
	}
      if (nl->port)
	sprintf (portstr, "%d", nl->port);
      else
	portstr[0] = '\0';
      if (nl->type == UNCONNECTED)
	{
	  strcpy (stat, "~FRDOWN");
	  strcpy (vers, "-");
	}
      else
	{
	  if (nl->stage == UP)
	    strcpy (stat, "  ~FGUP");
	  else
	    strcpy (stat, " ~FYVER");
	  if (!nl->ver_major)
	    strcpy (vers, "3.?.?");	/* Pre - 3.2 version */
	  else
	    sprintf (vers, "%d.%d.%d",
		     nl->ver_major, nl->ver_minor, nl->ver_patch);
	}
      /* If link is incoming and remoter vers < 3.2 we have no way of knowing
         what the permissions on it are so set to blank */
      if (!nl->ver_major && nl->type == INCOMING && nl->allow != IN)
	a = 0;
      else
	a = nl->allow + 1;
      sprintf (text, "%-15s :	%s  %s	 %s~RS %2d %2d %7s  %s %s\n",
	       nl->service, allow[a], type[nl->type], stat, iu, ou, vers,
	       nl->site, portstr);
      write_user (user, text);
    }
  write_user (user, "\n");
}

/*** Show type of data being received down links (this is usefull when a
     link has hung) ***/
netdata (user)
     UR_OBJECT user;
{
  NL_OBJECT nl;
  char from[80], name[USER_NAME_LEN + 1];
  int cnt;

  cnt = 0;
  write_user (user, "\n~FB~OL--->>> Mail receiving status <<<---\n\n");
  for (nl = nl_first; nl != NULL; nl = nl->next)
    {
      if (nl->type == UNCONNECTED || nl->mailfile == NULL)
	continue;
      if (++cnt == 1)
	write_user (user,
		    "To	       : From			    Last recv.\n\n");
      sprintf (from, "%s@%s", nl->mail_from, nl->service);
      sprintf (text, "%-15s : %-25s  %d seconds ago.\n",
	       nl->mail_to, from, (int) (time (0) - nl->last_recvd));
      write_user (user, text);
    }
  if (!cnt)
    write_user (user, "No mail being received.\n\n");
  else
    write_user (user, "\n");

  cnt = 0;
  write_user (user, "\n~FB~OL--->>> Message receiving status <<<---\n\n");
  for (nl = nl_first; nl != NULL; nl = nl->next)
    {
      if (nl->type == UNCONNECTED || nl->mesg_user == NULL)
	continue;
      if (++cnt == 1)
	write_user (user, "To	       : From		  Last recv.\n\n");
      if (nl->mesg_user == (UR_OBJECT) - 1)
	strcpy (name, "<unknown>");
      else
	strcpy (name, nl->mesg_user->name);
      sprintf (text, "%-15s : %-15s  %d seconds ago.\n",
	       name, nl->service, (int) (time (0) - nl->last_recvd));
      write_user (user, text);
    }
  if (!cnt)
    write_user (user, "No messages being received.\n\n");
  else
    write_user (user, "\n");
}


/*** Connect a netlink. Use the room as the key ***/
connect_netlink (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  NL_OBJECT nl;
  int ret, tmperr;

  if (word_count < 2)
    {
      write_usage (user, "connect <sky>");
      return;
    }
  if ((rm = get_room (word[1])) == NULL)
    {
      write_user (user, nosuchroom);
      return;
    }
  if ((nl = rm->netlink) == NULL)
    {
      write_user (user, ">>>That sky is not linked to a service.\n");
      return;
    }
  if (nl->type != UNCONNECTED)
    {
      write_user (user, ">>>That sky netlink is already up.\n");
      return;
    }
  write_user (user,
	      ">>>Attempting connect (this may cause a temporary hang)...\n");
  sprintf (text, "NETLINK: Connection attempt to %s initiated by %s.\n",
	   nl->service, user->savename);
  write_syslog (LOG_LINK, text, LOG_TIME);
  errno = 0;
  if (!(ret = connect_to_site (nl)))
    {
      write_user (user, "~FG>>>Initial connection made...\n");
      sprintf (text, "NETLINK: Connected to service %s (%s %d).\n",
	       nl->service, nl->site, nl->port);
      write_syslog (LOG_LINK, text, LOG_TIME);
      nl->connect_room = rm;
      return;
    }
  tmperr = errno;		/* On Linux errno seems to be reset between here and sprintf */
  write_user (user, "~FR>>>Connect failed: ");
  write_syslog (LOG_LINK, "NETLINK: Connection attempt failed: ", LOG_TIME);
  if (ret == 1)
    {
      sprintf (text, "%s.\n", sys_errlist[tmperr]);
      write_user (user, text);
      write_syslog (LOG_LINK, text, LOG_NOTIME);
      return;
    }
  write_user (user, ">>>Unknown hostname.\n");
  write_syslog (LOG_LINK, "Unknown hostname.\n", LOG_NOTIME);
}



/*** Disconnect a link ***/
disconnect_netlink (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  NL_OBJECT nl;

  if (word_count < 2)
    {
      write_usage (user, "disconnect <sky>");
      return;
    }
  if ((rm = get_room (word[1])) == NULL)
    {
      write_user (user, nosuchroom);
      return;
    }
  nl = rm->netlink;
  if (nl == NULL)
    {
      write_user (user, ">>>That sky is not linked to a service.\n");
      return;
    }
  if (nl->type == UNCONNECTED)
    {
      write_user (user, ">>>That sky netlink is not connected.\n");
      return;
    }
/* If link has hung at verification stage don't bother announcing it */
  if (nl->stage == UP)
    {
      sprintf (text, "~OLGAEN:~RS Disconnecting from service %s in the %s.\n",
	       nl->service, rm->name);
      write_room (NULL, text);
      sprintf (text, "NETLINK: Link to %s in the %s disconnected by %s.\n",
	       nl->service, rm->name, user->savename);
      write_syslog (LOG_LINK, text, LOG_TIME);
    }
  else
    {
      sprintf (text, "NETLINK: Link to %s disconnected by %s.\n", nl->service,
	       user->name);
      write_syslog (LOG_LINK, text, LOG_TIME);
    }
  shutdown_netlink (nl);
  write_user (user, ">>>Disconnected.\n");
}


/*** Change users password. Only superior users can change another users
     password and they do this by specifying the user at the end. When this is
     done the old password given can be anything, the superior user doesn't
     have to know it in advance. ***/
change_pass (user)
     UR_OBJECT user;
{
  UR_OBJECT u;

  if (word_count < 3)
    {
      if (user->level < MIN_LEV_PASS)
	write_usage (user, "passwd <old password> <new password>");
      else
	write_usage (user, "passwd <old password> <new password> [<user>]");
      return;
    }
  if (strlen (word[2]) < PASS_MIN_LEN)
    {
      write_user (user, ">>>New password too short.\n");
      return;
    }
  if (strlen (word[2]) > PASS_MAX_LEN)
    {
      write_user (user, ">>>New password too long.\n");
      return;
    }
/* Change own password */
  if (word_count == 3)
    {
      if (strcmp ((char *) crypt (word[1], PASS_SALT), user->pass))
	{
	  write_user (user, ">>>Old password incorrect.\n");
	  return;
	}
      if (!stricmp (word[1], word[2]))
	{
	  write_user (user, ">>>Old and new passwords are the same.\n");
	  return;
	}
      if (!secure_pass (word[2]))
	{
	  write_user (user,
		      ">>>New password too simple, try to use special characters.\n");
	  return;
	}
      if (strstr (word[1], word[2]) || strstr (word[2], word[1]))
	{
	  write_user (user, ">>>Old and new passwords are too similar.\n");
	  return;
	}
      strcpy (user->pass, (char *) crypt (word[2], PASS_SALT));
      save_user_details (user, 0);
      cls (user);
      sprintf (text, ">>>Password changed to \"~FY%s~RS\".\n", word[2]);
      write_user (user, text);
      return;
    }
/* Change someone elses */
  if (user->level < MIN_LEV_PASS)
    {
      write_user (user,
		  ">>>You are not a high enough level to use the <user> option. Sorry...\n");
      return;
    }
  word[3][0] = toupper (word[3][0]);
  if (!strcmp (word[3], user->savename))
    {
      /* security feature  - prevents someone coming to a saint terminal and
         changing his password since he wont have to know the old one */
      write_user (user,
		  ">>>You cannot change your own password using the <user> option.\n");
      return;
    }
  if ((u = get_exact_user (word[3])) != NULL)
    {
      if (u->level >= user->level)
	{
	  write_user (user,
		      ">>>You cannot change the password of an user of equal or higher level than yourself!\n");
	  return;
	}
      cls (u);
      sprintf (text, "~FR~OL>>>Your password has been changed by %s!\n",
	       user->vis ? user->name : "a digit");
      write_user (u, text);
      sprintf (text, "%s changed %s's password.\n", user->savename,
	       u->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      strcpy (u->pass, (char *) crypt (word[2], PASS_SALT));
      sprintf (text, ">>>%s's password changed to \"~FY%s~RS\".\n", word[3],
	       word[2]);
      write_user (user, text);
      return;
    }
  if ((u = create_user ()) == NULL)
    {
      sprintf (text, ">>>%s: unable to create temporary user object.\n",
	       syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Unable to create temporary user object in change_pass().\n",
		    LOG_NOTIME);
      return;
    }
  strcpy (u->savename, word[3]);
  if (!load_user_details (u))
    {
      write_user (user, nosuchuser);
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (u->level >= user->level)
    {
      write_user (user,
		  ">>>You cannot change the password of an user of equal or higher level than yourself!\n");
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (!secure_pass (word[2]))
    {
      write_user (user,
		  ">>>New password is too simple, try to use special characters.\n");
      destruct_user (u);
      destructed = 0;
      return;
    }
  sprintf (text, "%s changed %s's password.\n", user->savename, u->savename);
  write_syslog (LOG_COM, text, LOG_TIME);
  strcpy (u->pass, (char *) crypt (word[2], PASS_SALT));
  save_user_details (u, 0);
  destruct_user (u);
  destructed = 0;
  sprintf (text, ">>>%s's password changed to \"~FY%s~RS\".\n", word[3],
	   word[2]);
  write_user (user, text);
}


/*** Kill an user ***/
kill_user (user)
     UR_OBJECT user;
{
  UR_OBJECT victim, u;
  RM_OBJECT rm;
  char *name;
  int method;
  char filename[80];

  if (word_count < 2)
    {
      write_usage (user, "kill <user> [<method>/!]");
      return;
    }
  if (word_count == 2)
    method = 0;			/* no special method to kill the user... */
  else
    {
      if (word[2][0] == '!')	/* choose a random method */
	method = random () % MAX_LEVELS + 1;
      else
	method = atoi (word[2]);
      if (method <= 0)
	method = 0;
    }

  if (!(victim = get_user (word[1])))
    {
      write_user (user, notloggedon);
      return;
    }
  if (user == victim)
    {
      write_user (user,
		  ">>>Trying to commit suicide this way is the sixth sign of madness or genius.\n");
      return;
    }
  if (victim->level >= user->level)
    {
      write_user (user,
		  ">>>You cannot kill an user of equal or higher level than yourself!\n");
      sprintf (text, "~FR~OL>>>%s tried to kill you!\n",
	       user->vis ? user->name : invisname);
      write_user (victim, text);
      return;
    }
  if (victim->restrict[RESTRICT_KILL] == restrict_string[RESTRICT_KILL])
    {
      write_user (user, ">>>You cannot kill a protected user!\n");
      sprintf (text,
	       ">>>%s tried to kill you, but you are under protection :-)\n",
	       user->vis ? user->name : invisname);
      write_user (victim, text);
      return;
    }
/* Finally, the user is killed! */
  sprintf (text, ">>>%s KILLED %s.\n", user->savename, victim->savename);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_user (user, "~FM~OL>>>You call the dark power...\n");
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  if (method)
    sprintf (text, "~FM~OL>>>%s decides to kill %s...\n", name, victim->name);
  else
    sprintf (text, "~FM~OL>>>%s calls the dark power...\n", name);
  write_room (NULL, text, user);
  if (method)
    {
      sprintf (filename, "%s/kill.%d", KILLFILES, method);
      for (u = user_first; u != NULL; u = u->next)
	{
	  if (u->login || u->type == CLONE_TYPE || u->ignall)
	    continue;
	  switch (more (u, u->socket, filename))
	    {
	    case 0:
	      break;
	    case 1:
	      u->misc_op = 2;
	    }
	}
    }
  write_user (victim,
	      "~FM~OL>>>A shrieking furie rises up out of the ground, and devours you!!!\n");
  if (!method)
    sprintf (text,
	     "~FM~OL>>>A shrieking furie rises up out of the ground, devours %s and vanishes!!!\n",
	     victim->name);
  else
    sprintf (text,
	     "~FM~OL>>>%s, the true murderer of this world, is happy now!!!\n",
	     name);
  rm = victim->room;
  write_room_except (rm, text, victim);
  disconnect_user (victim);
  write_room (NULL,
	      "~FM~OL>>>You hear insane laughter from the hell's doors...\n");
  event = E_KILL;
  strcpy (event_var, user->savename);
}


/*** Promote an user ***/
promote (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  char text2[80];

  if (word_count < 2)
    {
      write_usage (user, "promote <user>");
      return;
    }
/* See if user is on atm */
  if ((u = get_user (word[1])) != NULL)
    {
      if (u->level >= user->level)
	{
	  write_user (user,
		      ">>>You cannot promote an user to a level higher than your own.\n");
	  return;
	}
      if (u->restrict[RESTRICT_PROM] == restrict_string[RESTRICT_PROM])
	{
	  write_user (user, ">>>This user cannot be promoted!\n");
	  return;
	}
      if (user->level < SAINT && u->level > SPARK)
	{
	  write_user (user, ">>>This user has already a good level!\n");
	  return;
	}
      if (u->type == REMOTE_TYPE && u->level >= ANGEL)
	{
	  write_user (user,
		      ">>>The user is a remote user (has already a good level!)\n");
	  return;
	}
      u->level++;
      if (u->slevel <= GOD)
	{
	  u->slevel++;
	}
      sprintf (text, ">>>You promote %s to level: ~OL%s.\n", u->name,
	       level_name[u->level]);
      write_user (user, text);
      sprintf (text, "~FG~OL>>>%s has promoted you to level: ~RS~OL%s! :-)\n",
	       user->vis ? user->name : invisname, level_name[u->level]);
      write_user (u, text);
      sprintf (text, "~OLGAEN:~RS %s promoted %s to level %s.\n",
	       user->savename, u->savename, level_name[u->level]);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      return;
    }
/* Create a temp session, load details, alter , then save. This is inefficient
   but its simpler than the alternative */
  if ((u = create_user ()) == NULL)
    {
      sprintf (text, ">>>%s: unable to create temporary user object.\n",
	       syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Unable to create temporary user object in promote().\n",
		    LOG_NOTIME);
      return;
    }
  strcpy (u->savename, word[1]);
  if (!load_user_details (u))
    {
      write_user (user, nosuchuser);
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (u->restrict[RESTRICT_PROM] == restrict_string[RESTRICT_PROM])
    {
      write_user (user, ">>>This user cannot be promoted!\n");
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (u->level >= user->level)
    {
      write_user (user,
		  ">>>You cannot promote an user to a level higher than your own.\n");
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (user->level < SAINT && u->level > SPARK)
    {
      write_user (user, ">>>This user has already a good level!\n");
      destruct_user (u);
      destructed = 0;
      return;
    }
  u->level++;
  u->socket = -2;
  strcpy (u->site, u->last_site);
  save_user_details (u, 0);
  sprintf (text, ">>>You promote %s to level: ~OL%s.\n", u->savename,
	   level_name[u->level]);
  write_user (user, text);
  sprintf (text2, "~FG~OL>>>You have been promoted to level: ~RS~OL%s.\n",
	   level_name[u->level]);
  send_mail (user, word[1], text2);
  sprintf (text, "~OLGAEN:~RS %s promoted %s to level %s.\n", user->savename,
	   u->savename, level_name[u->level]);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
  destruct_user (u);
  destructed = 0;
}


/*** Demote an user ***/
demote (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  char text2[80];

  if (word_count < 2)
    {
      write_usage (user, "demote <user>");
      return;
    }
/* See if user is on atm */
  if ((u = get_user (word[1])) != NULL)
    {
      if (u->level == GUEST)
	{
	  write_user (user, ">>>You cannot demote an user of level GUEST.\n");
	  return;
	}
      if (u->level >= user->level)
	{
	  write_user (user,
		      "You cannot demote an user of an equal or higher level than yourself.\n");
	  return;
	}
      if (u->restrict[RESTRICT_DEMO] == restrict_string[RESTRICT_DEMO])
	{
	  write_user (user, ">>>This user cannot be demoted!\n");
	  return;
	}
      u->level--;
      if (u->slevel > 0)
	u->slevel--;
      sprintf (text, ">>>You demote %s to level: ~OL%s.\n", u->name,
	       level_name[u->level]);
      write_user (user, text);
      sprintf (text, "~FR~OL>>>%s has demoted you to level: ~RS~OL%s! :-(\n",
	       user->vis ? user->name : invisname, level_name[u->level]);
      write_user (u, text);
      sprintf (text, "~OLGAEN:~RS %s demoted %s to level %s.\n",
	       user->savename, u->savename, level_name[u->level]);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      return;
    }
/* User not logged on */
  if ((u = create_user ()) == NULL)
    {
      sprintf (text, ">>>%s: unable to create temporary user object.\n",
	       syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Unable to create temporary user object in demote().\n",
		    LOG_NOTIME);
      return;
    }
  strcpy (u->savename, word[1]);
  if (!load_user_details (u))
    {
      write_user (user, nosuchuser);
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (u->level == GUEST)
    {
      write_user (user, ">>>You cannot demote an user of level GUEST.\n");
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (u->restrict[RESTRICT_DEMO] == restrict_string[RESTRICT_DEMO])
    {
      write_user (user, ">>>This user cannot be demoted!\n");
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (u->level >= user->level)
    {
      write_user (user,
		  ">>>You cannot demote an user of an equal or higher level than yourself.\n");
      destruct_user (u);
      destructed = 0;
      return;
    }
  u->level--;
  u->socket = -2;
  strcpy (u->site, u->last_site);
  save_user_details (u, 0);
  sprintf (text, ">>>You demote %s to level: ~OL%s.\n", u->savename,
	   level_name[u->level]);
  write_user (user, text);
  sprintf (text2, "~FR~OL>>>You have been demoted to level: ~RS~OL%s.\n",
	   level_name[u->level]);
  send_mail (user, word[1], text2);
  sprintf (text, "~OLGAEN:~RS %s demoted %s to level %s.\n",
	   user->savename, u->savename, level_name[u->level]);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
  destruct_user (u);
  destructed = 0;
}


/*** List banned sites or users ***/
listbans (user)
     UR_OBJECT user;
{
  char filename[80];

  if (!strcmp (word[1], "sites"))
    {
      write_user (user, "\n~FB~OL--->>> Banned sites/domains <<<---\n\n");
      sprintf (filename, "%s/%s", DATAFILES, SITEBAN);
      switch (more (user, user->socket, filename))
	{
	case 0:
	  write_user (user, ">>>There are no banned sites/domains.\n\n");
	  return;

	case 1:
	  user->misc_op = 2;
	}
      return;
    }
  if (!strcmp (word[1], "users"))
    {
      write_user (user, "\n~FB~OL--->>> Banned users <<<---\n\n");
      sprintf (filename, "%s/%s", DATAFILES, USERBAN);
      switch (more (user, user->socket, filename))
	{
	case 0:
	  write_user (user, ">>>There are no banned users.\n\n");
	  return;

	case 1:
	  user->misc_op = 2;
	}
      return;
    }
  write_usage (user, "listbans sites/users\n");
}


/*** Ban a site (or domain) or user ***/
ban (user)
     UR_OBJECT user;
{
  char *usage = "ban site/user <site/user name>";

  if (word_count < 3)
    {
      write_usage (user, usage);
      return;
    }
  if (!strcmp (word[1], "site"))
    {
      ban_site (user);
      return;
    }
  if (!strcmp (word[1], "user"))
    {
      ban_user (user);
      return;
    }
  write_usage (user, usage);
}


/*** Ban a site ***/
ban_site (user)
     UR_OBJECT user;
{
  FILE *fp;
  char filename[80], host[81], site[80];

  if (gethostname (host, 80) < 0)
    {
      sprintf (text,
	       ">>>%s: Can't get hostname, the execution of the command cannot continue.\n",
	       syserror);
      write_user (user, text);
      write_syslog (LOG_ERR, "ERROR: Couldn't get hostname in ban_site().\n",
		    LOG_NOTIME);
      return;
    }
  if (!strcmp (word[2], host) || !strcmp (word[2], "localhost")
      || !strcmp (word[2], "127.0.0.1"))
    {
      write_user (user,
		  ">>>You cannot ban the machine that this program is running on.\n");
      return;
    }
  sprintf (filename, "%s/%s", DATAFILES, SITEBAN);

/* See if ban already set for given site */
  if (fp = fopen (filename, "r"))
    {
      fscanf (fp, "%s", site);
      while (!feof (fp))
	{
	  if (!strcmp (site, word[2]))
	    {
	      write_user (user, ">>>That site/domain is already banned.\n");
	      fclose (fp);
	      return;
	    }
	  fscanf (fp, "%s", site);
	}
      fclose (fp);
    }

/* Write new ban to file */
  if (!(fp = fopen (filename, "a")))
    {
      sprintf (text, ">>>%s: Can't open file to append.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Couldn't open file to append in ban_site().\n",
		    LOG_NOTIME);
      return;
    }
  fprintf (fp, "%s\n", word[2]);
  fclose (fp);
  write_user (user, ">>>Site/domain banned.\n");
  sprintf (text, "~OLGAEN:~RS %s banned site/domain %s.\n", user->savename,
	   word[2]);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
}


/*** Ban an user ***/
ban_user (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  FILE *fp;
  char filename[80], filename2[80], p[20], name[USER_NAME_LEN + 1];
  int a, b, c, d, level;

  word[2][0] = toupper (word[2][0]);
  if (!strcmp (user->savename, word[2]))
    {
      write_user (user,
		  ">>>Trying to ban yourself is the seventh sign of madness or genius...\n");
      return;
    }

/* See if ban already set for given user */
  sprintf (filename, "%s/%s", DATAFILES, USERBAN);
  if (fp = fopen (filename, "r"))
    {
      fscanf (fp, "%s", name);
      while (!feof (fp))
	{
	  if (!strcmp (name, word[2]))
	    {
	      write_user (user, ">>>That user is already banned.\n");
	      fclose (fp);
	      return;
	    }
	  fscanf (fp, "%s", name);
	}
      fclose (fp);
    }

/* See if already on */
  if ((u = get_user (word[2])) != NULL)
    {
      if (u->level >= user->level)
	{
	  write_user (user,
		      ">>>You cannot ban an user of equal or higher level than yourself.\n");
	  return;
	}
    }
  else
    {
      /* User not on so load up his data */
      sprintf (filename2, "%s/%s.D", USERFILES, word[2]);
      if (!(fp = fopen (filename2, "r")))
	{
	  write_user (user, nosuchuser);
	  return;
	}
      fscanf (fp, "%s\n%d %d %d %d %d", p, &a, &b, &c, &d, &level);
      fclose (fp);
      if (level >= user->level)
	{
	  write_user (user,
		      ">>>You cannot ban an user of equal or higher level than yourself.\n");
	  return;
	}
    }

/* Write new ban to file */
  if (!(fp = fopen (filename, "a")))
    {
      sprintf (text, ">>>%s: Can't open file to append.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Couldn't open file to append in ban_user().\n",
		    LOG_NOTIME);
      return;
    }
  fprintf (fp, "%s\n", word[2]);
  fclose (fp);
  write_user (user, ">>>User banned.\n");
  sprintf (text, "~OLGAEN:~RS %s banned user %s.\n", user->savename, word[2]);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
  if (u != NULL)
    {
      write_user (u,
		  "\n\07~FR~OL~LI>>>You have been banned from here! :-(\n\n");
      disconnect_user (u);
    }
}



/*** uban a site (or domain) or user ***/
unban (user)
     UR_OBJECT user;
{
  char *usage = "unban site/user <site/user name>";

  if (word_count < 3)
    {
      write_usage (user, usage);
      return;
    }
  if (!strcmp (word[1], "site"))
    {
      unban_site (user);
      return;
    }
  if (!strcmp (word[1], "user"))
    {
      unban_user (user);
      return;
    }
  write_usage (user, usage);
}

/*** Unban a site ***/
unban_site (user)
     UR_OBJECT user;
{
  FILE *infp, *outfp;
  char filename[80], site[80];
  int found, cnt;

  sprintf (filename, "%s/%s", DATAFILES, SITEBAN);
  if (!(infp = fopen (filename, "r")))
    {
      write_user (user, ">>>That site/domain is not currently banned.\n");
      return;
    }
  if (!(outfp = fopen ("tempfile", "w")))
    {
      sprintf (text, ">>>%s: Couldn't open tempfile.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Couldn't open tempfile to write in unban_site().\n",
		    LOG_NOTIME);
      fclose (infp);
      return;
    }
  found = 0;
  cnt = 0;
  fscanf (infp, "%s", site);
  while (!feof (infp))
    {
      if (strcmp (word[2], site))
	{
	  fprintf (outfp, "%s\n", site);
	  cnt++;
	}
      else
	found = 1;
      fscanf (infp, "%s", site);
    }
  fclose (infp);
  fclose (outfp);
  if (!found)
    {
      write_user (user, ">>>That site/domain is not currently banned.\n");
      unlink ("tempfile");
      return;
    }
  if (!cnt)
    {
      s_unlink (filename);
      unlink ("tempfile");
    }
  else
    rename ("tempfile", filename);
  write_user (user, ">>>Site ban removed.\n");
  sprintf (text, "~OLGAEN:~RS %s unbanned site %s.\n", user->savename,
	   word[2]);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
}

/*** Unban an user ***/
unban_user (user)
     UR_OBJECT user;
{
  FILE *infp, *outfp;
  char filename[80], name[USER_NAME_LEN + 1];
  int found, cnt;

  sprintf (filename, "%s/%s", DATAFILES, USERBAN);
  if (!(infp = fopen (filename, "r")))
    {
      write_user (user, ">>>That user is not currently banned.\n");
      return;
    }
  if (!(outfp = fopen ("tempfile", "w")))
    {
      sprintf (text, ">>>%s: Couldn't open tempfile.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Couldn't open tempfile to write in unban_user().\n",
		    LOG_NOTIME);
      fclose (infp);
      return;
    }
  found = 0;
  cnt = 0;
  word[2][0] = toupper (word[2][0]);
  fscanf (infp, "%s", name);
  while (!feof (infp))
    {
      if (strcmp (word[2], name))
	{
	  fprintf (outfp, "%s\n", name);
	  cnt++;
	}
      else
	found = 1;
      fscanf (infp, "%s", name);
    }
  fclose (infp);
  fclose (outfp);
  if (!found)
    {
      write_user (user, ">>>That user is not currently banned.\n");
      unlink ("tempfile");
      return;
    }
  if (!cnt)
    {
      s_unlink (filename);
      unlink ("tempfile");
    }
  else
    rename ("tempfile", filename);
  write_user (user, ">>>User ban removed.\n");
  sprintf (text, "~OLGAEN:~RS %s unbanned user %s.\n", user->savename,
	   word[2]);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
}

/*** Set user visible or invisible ***/
visibility (user, vis)
     UR_OBJECT user;
     int vis;
{
  if (vis)
    {
      if (user->vis)
	{
	  write_user (user, ">>>You are already visible.\n");
	  return;
	}
      write_user (user,
		  "~FB~OL>>>You recite a melodic incantation and reappear.\n");
      sprintf (text,
	       "~FB~OL>>>You hear a melodic incantation chanted and %s materialises!\n",
	       user->name);
      write_room_except (user->room, text, user);
      user->vis = 1;
      return;
    }
  if (!user->vis)
    {
      write_user (user, ">>>You are already invisible.\n");
      return;
    }
  write_user (user,
	      "~FB~OL>>>You recite a melodic incantation and fade out.\n");
  sprintf (text,
	   "~FB~OL>>>%s recites a melodic incantation and disappears!\n",
	   user->name);
  write_room_except (user->room, text, user);
  user->vis = 0;
}


/*** Site an user ***/
site (user)
     UR_OBJECT user;
{
  UR_OBJECT u;

  if (word_count < 2)
    {
      write_usage (user, "site <user>");
      return;
    }
/* User currently logged in */
  if (u = get_user (word[1]))
    {
      if (u->type == REMOTE_TYPE)
	sprintf (text, ">>>%s is remotely connected from ~FY~OL%s~RS",
		 u->name, u->site);
      else
	sprintf (text, ">>>%s is logged in from ~FY~OL%s~RS (%s port)",
		 u->name, u->ssite, u->port == port[0] ? "Main" : "St.");
      write_user (user, text);
      sprintf (text, " - idle for ~FT%d~RS seconds.\n",
	       (int) (time (0) - u->last_input));
      write_user (user, text);
      return;
    }
/* User not logged in */
  if ((u = create_user ()) == NULL)
    {
      sprintf (text, ">>>%s: unable to create temporary user object.\n",
	       syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Unable to create temporary user object in site().\n",
		    LOG_NOTIME);
      return;
    }
  strcpy (u->savename, word[1]);
  if (!load_user_details (u))
    {
      write_user (user, nosuchuser);
      destruct_user (u);
      destructed = 0;
      return;
    }
  sprintf (text, ">>>%s was last logged in from ~FY~OL%s.\n", word[1],
	   u->subst ? u->ssite : u->last_site);
  write_user (user, text);
  destruct_user (u);
  destructed = 0;
}


/*** Wake up some sleepy herbert ***/
wake (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  UR_OBJECT u;
  char text1[WAKEMSG_LEN + 1];

  if (word_count < 2)
    {
      write_usage (user, "wake <user> [<message>]");
      return;
    }
  if (word_count < 3)
    strcpy (text1, "");
  else
    {
      inpstr = remove_first (inpstr);
      if (strlen (inpstr) > WAKEMSG_LEN)
	inpstr[WAKEMSG_LEN] = '\0';
      strcpy (text1, inpstr);
    }
  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, you cannot wake anyone.\n");
      return;
    }
  if (!(u = get_user (word[1])))
    {
      write_user (user, notloggedon);
      return;
    }
  if (u == user)
    {
      write_user (user,
		  ">>>Trying to wake yourself up is the eighth sign of madness or genius.\n");
      return;
    }
  if (u->afk && user->level < SGOD)
    {
      sprintf (text,
	       ">>>You cannot wake someone who is AFK... { ~FT%s~RS }\n",
	       u->afkmesg);
      write_user (user, text);
      return;
    }
  if (is_ignoring (u, user))
    {
      sprintf (text,
	       ">>>%s is ignoring you. You must ask him to forgive you first.\n",
	       u->name);
      write_user (user, text);
      return;
    }
  sprintf (text,
	   "~EL\n~BR--->>> %s says: ~OL~LIWAKE UP %s!!!~RS~BR <<<---\n\n",
	   user->name, text1);
  write_user (u, text);
  sprintf (text, ">>>Wake up call '~FT%s~RS' sent to %s.\n", text1, u->name);
  write_user (user, text);
  if (has_unread_mail (user))
    write_user (user, "~EL~FT~OL~LI--->>> UNREAD MAIL! <<<---\n");
}


/*** Shout something to other superior level users. 
     If the level isn't given it defaults to SPIRIT level. ***/
stshout (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  int lev;

  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, you cannot stshout.\n");
      return;
    }
  if (word_count < 2)
    {
      write_usage (user, "stshout [<superuser level>] <message>");
      return;
    }
  if (ban_swearing && contains_swearing (inpstr))
    {
      swear_action (user, inpstr);
      return;
    }
  strtoupper (word[1]);
  lev = get_level (word[1]);
  if (lev == -1)
    lev = MIN_LEV_STSHOUT;
  else
    {
      if (lev < MIN_LEV_STSHOUT || word_count < 3)
	{
	  write_usage (user, "stshout [<superuser level>] <message>");
	  return;
	}
      if (lev > user->level)
	{
	  write_user (user,
		      ">>>You can't specifically shout to users of a higher level than yourself.\n");
	  return;
	}
      inpstr = remove_first (inpstr);
      sprintf (text, "~FM~OL>>>You saintshout to level %s:~RS %s\n",
	       level_name[lev], inpstr);
      write_user (user, text);
      sprintf (text, "~FM~OL>>>%s saintshouts to level %s:~RS %s\n",
	       user->name, level_name[lev], inpstr);
      write_level (lev, text, user);
      return;
    }
  sprintf (text, "~OL~FM>>>You saintshout:~RS %s\n", inpstr);
  write_user (user, text);
  sprintf (text, "~OL~FM>>>%s saintshouts:~RS %s\n", user->name, inpstr);
  write_level (MIN_LEV_STSHOUT, text, user);
}

/*** Think... ***/
think (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char text1[REVIEW_LEN + 1];
  if (word_count < 2)
    {
      write_user (user,
		  ">>>Trying to think about nothing is the thirteenth sign of madness or genius...\n");
      sprintf (text,
	       ">>>%s tries to think...'~FY~OLErrr... about.... dunno what... never mind...~RS'\n",
	       user->vis ? user->name : invisname);
      write_room_except (NULL, text, user);
      return;
    }
  if (!strcmp (word[1], "?"))	/* show a little help */
    {
      write_usage (user, "think [ [ option ] idea ]");
      return;
    }
  if (strlen (inpstr) > REVIEW_LEN - 30)
    inpstr[REVIEW_LEN - 30] = '\0';
  if (word_count > 2)
    {
      switch (word[1][0])	/* different methods to think... like a philosopher */
	{
	case '-':
	  inpstr = remove_first (inpstr);
	  strrev (inpstr);
	  break;
	case '=':
	  inpstr = remove_first (inpstr);
	  strmix (inpstr);
	  break;
	case '|':
	  inpstr = remove_first (inpstr);
	  str_upper_lower (inpstr);
	  break;
	case '\\':
	  inpstr = remove_first (inpstr);
	  strswp (inpstr);
	  break;
	}
    }
  sprintf (text1, "thinks... '~FY~OL%s~RS'\n", inpstr);
  emote (user, text1);
}

/*** Sing something... ***/
sing (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char text1[REVIEW_LEN + 1];

  if (word_count < 2)
    {
      write_user (user, ">>>Sing what?\n");
      return;
    }
  if (strlen (inpstr) > REVIEW_LEN - 30)
    inpstr[REVIEW_LEN - 30] = '\0';
  sprintf (text1, "sings: <<< ~FM~OL%s~RS >>>\n", inpstr);
  emote (user, text1);
}

/*** Muzzle an annoying user so he cant speak, emote, echo, write,
     or smail. Muzzles have levels from SAINT to GOD so for instance a saint
     cannot remove a muzzle set by a god.  ***/
muzzle (user)
     UR_OBJECT user;
{
  UR_OBJECT u;

  if (word_count < 2)
    {
      write_usage (user, "muzzle <user>");
      return;
    }
  if ((u = get_user (word[1])) != NULL)
    {
      if (u == user)
	{
	  write_user (user,
		      ">>>Trying to muzzle yourself is the ninth sign of madness or genius.\n");
	  return;
	}
      if (u->level >= user->level)
	{
	  write_user (user,
		      ">>>You cannot muzzle an user of equal or higher level than yourself.\n");
	  return;
	}
      if (u->muzzled >= user->level)
	{
	  sprintf (text, ">>>%s is already muzzled.\n", u->name);
	  write_user (user, text);
	  return;
	}
      if (u->restrict[RESTRICT_MUZZ] == restrict_string[RESTRICT_MUZZ])
	{
	  write_user (user, ">>>This user cannot be muzzled!\n");
	  return;
	}
      sprintf (text, ">>>%s now has a muzzle of level %s.\n", u->name,
	       level_name[user->level]);
      write_user (user, text);
      write_user (u, "~FR~OL>>>You have been muzzled! :-(\n");
      sprintf (text, "~OLGAEN:~RS %s muzzled %s.\n", user->savename,
	       u->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      u->muzzled = user->level;
      return;
    }
/* User not logged on */
  if ((u = create_user ()) == NULL)
    {
      sprintf (text, ">>>%s: unable to create temporary user object.\n",
	       syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Unable to create temporary user object in muzzle().\n",
		    LOG_NOTIME);
      return;
    }
  strcpy (u->savename, word[1]);
  if (!load_user_details (u))
    {
      write_user (user, nosuchuser);
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (u->restrict[RESTRICT_MUZZ] == restrict_string[RESTRICT_MUZZ])
    {
      write_user (user, ">>>This user cannot be muzzled!\n");
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (u->level >= user->level)
    {
      write_user (user,
		  ">>>You cannot muzzle an user of equal or higher level than yourself.\n");
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (u->muzzled >= user->level)
    {
      sprintf (text, ">>>%s is already muzzled.\n", u->savename);
      write_user (user, text);
      destruct_user (u);
      destructed = 0;
      return;
    }
  u->socket = -2;
  u->muzzled = user->level;
  strcpy (u->site, u->last_site);
  save_user_details (u, 0);
  sprintf (text, ">>>%s given a muzzle of level %s.\n", u->savename,
	   level_name[user->level]);
  write_user (user, text);
  send_mail (user, word[1], "~FR~OL>>>You have been muzzled! :-(\n");
  sprintf (text, "~OLGAEN:~RS %s muzzled %s.\n", user->savename, u->savename);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
  destruct_user (u);
  destructed = 0;
}



/*** Unmuzzle the bastard now he's apologised and grovelled enough ***/
unmuzzle (user)
     UR_OBJECT user;
{
  UR_OBJECT u;

  if (word_count < 2)
    {
      write_usage (user, "unmuzzle <user>");
      return;
    }
  if ((u = get_user (word[1])) != NULL)
    {
      if (u == user)
	{
	  write_user (user,
		      ">>>Trying to unmuzzle yourself is the tenth sign of madness or genius...\n");
	  return;
	}
      if (!u->muzzled)
	{
	  sprintf (text, ">>>%s is not muzzled.\n", u->name);
	  write_user (user, text);
	  return;
	}
      if (u->restrict[RESTRICT_UNMU] == restrict_string[RESTRICT_UNMU])
	{
	  write_user (user, ">>>This user cannot be unmuzzled!\n");
	  return;
	}
      if (u->muzzled > user->level)
	{
	  sprintf (text,
		   ">>>%s's muzzle is set to level %s, you do not have the power to remove it.\n",
		   u->name, level_name[u->muzzled]);
	  write_user (user, text);
	  return;
	}
      sprintf (text, ">>>You remove %s's muzzle.\n", u->name);
      write_user (user, text);
      write_user (u, "~FG~OL>>>You have been unmuzzled! :-)\n");
      sprintf (text, "~OLGAEN:~RS %s unmuzzled %s.\n", user->savename,
	       u->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      u->muzzled = 0;
      return;
    }
/* User not logged on */
  if ((u = create_user ()) == NULL)
    {
      sprintf (text, ">>>%s: unable to create temporary user object.\n",
	       syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Unable to create temporary user object in unmuzzle().\n",
		    LOG_NOTIME);
      return;
    }
  strcpy (u->savename, word[1]);
  if (!load_user_details (u))
    {
      write_user (user, nosuchuser);
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (u->restrict[RESTRICT_UNMU] == restrict_string[RESTRICT_UNMU])
    {
      write_user (user, ">>>This user cannot be unmuzzled!\n");
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (u->muzzled > user->level)
    {
      sprintf (text,
	       ">>>%s's muzzle is set to level %s~RS, you do not have the power to remove it.\n",
	       u->name, level_name[u->muzzled]);
      write_user (user, text);
      destruct_user (u);
      destructed = 0;
      return;
    }
  u->socket = -2;
  u->muzzled = 0;
  strcpy (u->site, u->last_site);
  save_user_details (u, 0);
  sprintf (text, ">>>You remove %s's muzzle.\n", u->savename);
  write_user (user, text);
  send_mail (user, word[1], "~FG~OL>>>You have been unmuzzled. :-)\n");
  sprintf (text, "~OLGAEN:~RS %s unmuzzled %s.\n", user->savename,
	   u->savename);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
  destruct_user (u);
  destructed = 0;
}



/*** Switch system logging on and off ***/
logging (user)
     UR_OBJECT user;
{
  int type;
  char filename[80];

  if (word_count < 2)
    {
      write_usage (user, "logging <type> [ erase ]");
      return;
    }
  if (isnumber (word[1]))
    type = atoi (word[1]) % MAX_LOGS;
  else
    {
      for (type = 0; type < MAX_LOGS; type++)
	{
	  if (!strncmp (word[1], logtype[type], strlen (word[1])))
	    break;
	}
      if (type >= MAX_LOGS)
	{
	  write_usage (user, "logging <type> [ erase ]");
	  return;
	}
    }
  if (word_count == 3 && !strcmp (word[2], "erase"))
    {
      sprintf (filename, "%s/%s.%s", LOGFILES, SYSLOG, logtype[type]);
      if (!s_unlink (filename))
	{
	  write_user (user, ">>>Log file erased.\n");
	  sprintf (text, "~OLGAEN:~RS %s ERASED log file %s.\n",
		   user->savename, filename);
	  write_syslog (LOG_CHAT, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, user);
	}
      else
	write_user (user, ">>>Cannot erase log file.\n");
      return;
    }
  if (system_logging[type])
    {
      sprintf (text, ">>>System logging ( %s ) ~FROFF.\n", logtype[type]);
      write_user (user, text);
      sprintf (text, "~OLGAEN:~RS %s switched system logging ( %s ) OFF.\n",
	       user->savename, logtype[type]);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      system_logging[type] = 0;
      return;
    }
  system_logging[type] = 1;
  sprintf (text, ">>>System logging ( %s ) ~FGON.\n", logtype[type]);
  write_user (user, text);
  sprintf (text, "~OLGAEN:~RS %s switched system logging ( %s ) ON.\n",
	   user->savename, logtype[type]);
  write_syslog (LOG_CHAT, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
}


/*** Set minlogin level ***/
minlogin (user)
     UR_OBJECT user;
{
  char *usage = "minlogin NONE/<user level> [ keep ]";
  char levstr[20];
  char *name;
  int lev, cnt;
  UR_OBJECT u, next;

  if (word_count < 2)
    {
      write_usage (user, usage);
      return;
    }
  strtoupper (word[1]);
  lev = get_level (word[1]);
  if (lev == -1)
    {
      if (strcmp (word[1], "NONE"))
	{
	  write_usage (user, usage);
	  return;
	}
      lev = -1;
      strcpy (levstr, "NONE");
    }
  else
    {
      lev %= MAX_LEVELS;
      strcpy (levstr, level_name[lev]);
    }
  if (lev > user->level)
    {
      write_user (user,
		  ">>>You cannot set minlogin to a higher level than your own.\n");
      return;
    }
  if (minlogin_level == lev)
    {
      write_user (user, ">>>It is already set to that.\n");
      return;
    }
  minlogin_level = lev;
  sprintf (text, ">>>Minlogin level set to %s.\n", levstr);
  write_user (user, text);
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  sprintf (text, ">>>%s has set the minlogin level to: %s.\n", name, levstr);
  write_room_except (NULL, text, user);
  sprintf (text, "%s set the minlogin level to %s.\n", user->savename,
	   levstr);
  write_syslog (LOG_CHAT, text, LOG_TIME);

  if (word_count >= 3 && !strcmp (word[2], "keep"))
    return;

/* Now boot off anyone below that level */
  cnt = 0;
  u = user_first;
  while (u)
    {
      next = u->next;
      if (!u->login && u->type != CLONE_TYPE && u->level < lev)
	{
	  write_user (u,
		      "\n~FY~OL>>>Your level is now below the minlogin level, disconnecting you...\n");
	  disconnect_user (u);
	  ++cnt;
	}
      u = next;
    }
  sprintf (text, ">>>Total of ~FG%d~RS users were disconnected.\n", cnt);
  destructed = 0;
  write_user (user, text);
}

/*** Show talker system parameters etc ***/
system_details (user)
     UR_OBJECT user;
{
  NL_OBJECT nl;
  RM_OBJECT rm;
  UR_OBJECT u;
  char bstr[40], minlogin[15];
  char *ca[] = { "NONE  ", "IGNORE", "REBOOT" };
  char slog[MAX_LOGS + 1];
  int days, hours, mins, secs;
  int netlinks, live, inc, outg;
  int rms, inlinks, num_clones, mem, size;
  int d, save_col;

  sprintf (text, "\n~FB~OL--->>> NUTS version %s, GAEN version %s <<<---\n",
	   VERSION, GAEN_VER);
  write_user (user, text);

/* Fill system log files status */
  for (d = 0; d < MAX_LOGS; d++)
    slog[d] = system_logging[d] ? toupper (logtype[d][0]) : '.';
  slog[MAX_LOGS] = '\0';
/* Get some values */
  strcpy (bstr, ctime (&boot_time));
  secs = (int) (time (0) - boot_time);
  days = secs / 86400;
  hours = (secs % 86400) / 3600;
  mins = (secs % 3600) / 60;
  secs = secs % 60;
  num_clones = 0;
  mem = 0;
  size = sizeof (struct user_struct);
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->type == CLONE_TYPE)
	num_clones++;
      mem += size;
    }

  rms = 0;
  inlinks = 0;
  size = sizeof (struct room_struct);
  for (d = 0; d < MAX_DIMENS; d++)
    for (rm = room_first[d]; rm != NULL; rm = rm->next)
      {
	if (rm->inlink)
	  ++inlinks;
	++rms;
	mem += size;
      }

  netlinks = 0;
  live = 0;
  inc = 0;
  outg = 0;
  size = sizeof (struct netlink_struct);
  for (nl = nl_first; nl != NULL; nl = nl->next)
    {
      if (nl->type != UNCONNECTED && nl->stage == UP)
	live++;
      if (nl->type == INCOMING)
	++inc;
      if (nl->type == OUTGOING)
	++outg;
      ++netlinks;
      mem += size;
    }
  if (minlogin_level == -1)
    strcpy (minlogin, "NONE");
  else
    strcpy (minlogin, colour_com_strip (level_name[minlogin_level]));

/* Show header parameters */
  sprintf (text,
	   "~FG~OLProcess ID   : %d\t~FG~OLTalker booted: %s~FG~OLUptime       : %d days, %d hours, %d minutes, %d seconds (UID: %d, GID: %d)\n",
	   getpid (), bstr, days, hours, mins, secs, getuid (), getgid ());
  write_user (user, text);
  sprintf (text,
	   "~FG~OLPorts (M/S/L): %d,  %d,  %d [Logins: %d, logouts: %d, swappings: %d]\n",
	   port[0], port[1], port[2], cnt_in, cnt_out, cnt_swp);
  write_user (user, text);

  save_col = user->colour;
  user->colour = 0;

/* Show others */
  sprintf (text,
	   "Max users              : %-3d           Current num. of users  : %d\n",
	   max_users, num_of_users);
  write_user (user, text);
  sprintf (text,
	   "Max clones             : %-2d            Current num. of clones : %d\n",
	   max_clones, num_clones);
  write_user (user, text);
  sprintf (text,
	   "Current minlogin level : %-6s        Login idle time out    : %d secs.\n",
	   minlogin, login_idle_time);
  write_user (user, text);
  sprintf (text,
	   "User idle time out     : %-4d secs.    Heartbeat              : %d\n",
	   user_idle_time, heartbeat);
  write_user (user, text);
  sprintf (text,
	   "Remote user maxlevel   : %-6s         Rem./new user def. lev.: %s/%s\n",
	   level_name[rem_user_maxlevel],
	   colour_com_strip (level_name[rem_user_deflevel]),
	   colour_com_strip (level_name[newuser_deflevel]));
  write_user (user, text);
  sprintf (text,
	   "St.port minlogin level : %-6s         Gatecrash level        : %s\n",
	   level_name[stport_level], level_name[gatecrash_level]);
  write_user (user, text);
  sprintf (text,
	   "Private sky min count  : %-2d            Message lifetime       : %d days\n",
	   min_private_users, mesg_life);
  write_user (user, text);
  sprintf (text,
	   "Message check time     : %02d:%02d         Net idle time out      : %d secs.\n",
	   mesg_check_hour, mesg_check_min, net_idle_time);
  write_user (user, text);
  sprintf (text,
	   "Number of skies        : %-2d            Num. accepting connects: %d\n",
	   rms, inlinks);
  write_user (user, text);
  sprintf (text,
	   "Total netlinks         : %-2d            Number which are live  : %d\n",
	   netlinks, live);
  write_user (user, text);
  sprintf (text,
	   "Number incoming        : %-2d            Number outgoing        : %d\n",
	   inc, outg);
  write_user (user, text);
  sprintf (text,
	   "Ignoring sigterm       : %s           Echoing passwords      : %s\n",
	   noyes2[ignore_sigterm], noyes2[password_echo]);
  write_user (user, text);
  sprintf (text,
	   "Swearing banned        : %s           Time out afks/Safety   : %s/%s('%s')\n",
	   noyes2[ban_swearing], noyes2[time_out_afks], noyes2[safety],
	   SAFETY_EXT);
  write_user (user, text);
  sprintf (text,
	   "Allowing caps in name  : %s           New user prompt default: %s\n",
	   noyes2[allow_caps_in_name], offon[prompt_def]);
  write_user (user, text);
  sprintf (text,
	   "New user colour default: %s           System logging         : %s\n",
	   offon[colour_def], slog);
  write_user (user, text);
  sprintf (text,
	   "Crash action           : %s        Object memory allocated: %d bytes\n",
	   ca[crash_action], mem);
  write_user (user, text);
  sprintf (text,
	   "Prayers                : %-3d           Mirror in autosave     : %s\n",
	   prayer_number, noyes2[no_disable_mirror]);
  write_user (user, text);
  user->colour = save_col;
}

/*** "Away from keyboard" signaling command ***/
afk (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  char text1[AFK_MESG_LEN + 1];

  if (strlen (inpstr) > AFK_MESG_LEN)
    inpstr[AFK_MESG_LEN] = '\0';
  if (word_count < 2 || user->muzzled)
    strcpy (text1, AFK_DEF_MSG);
  else
    strcpy (text1, inpstr);
  if (ban_swearing && contains_swearing (text1))
    {
      swear_action (user, text1);
      return;
    }
  strcpy (user->afkmesg, text1);
  sprintf (text,
	   ">>>You are now AFK ~FT{ %s }~RS, press ~FG<RETURN>~RS to reset.\n",
	   text1);
  write_user (user, text);
  if (user->vis)
    {
      sprintf (text, ">>>%s goes AFK...~FT{ %s }\n", user->name, text1);
      write_room_except (NULL, text, user);
    }
  user->afk = 1;
  event = E_AFK;
  strcpy (event_var, user->savename);
}

/*** Set the character mode echo on or off. This is only for users logging in
     via a character mode client, those using a line mode client (eg unix
     telnet) will see no effect. ***/
charecho (user)
     UR_OBJECT user;
{
  if (!user->charmode_echo)
    {
      write_user (user, ">>>Echoing for character mode clients ~FGON.\n");
      user->charmode_echo = 1;
      return;
    }
  write_user (user, ">>>Echoing for character mode clients ~FROFF.\n");
  user->charmode_echo = 0;
}

/*** Set the wrapping lines mode on or off, 
     useful for dumb terminal clients ***/
wraplines (user)
     UR_OBJECT user;
{
  if (!user->wrap)
    {
      write_user (user, ">>>Wrapping lines ~FGON.\n");
      user->wrap = 1;
      return;
    }
  write_user (user, ">>>Wrapping lines ~FROFF.\n");
  user->wrap = 0;
}

/*** Terminal: set/reset some terminal properties ***/
terminal (user)
     UR_OBJECT user;
{
  int len;
  char seq[6];

  if (word_count < 2)
    {
      if (user->blind)
	{
	  user->blind = 0;
	  write_user (user, ">>>You exit from blind mode.\n");
	}
      else
        write_user (user, ">>>You are not in blind mode, so this command is ineffective.\n");	
      return;
    }
  len = strlen (word[1]);
  if (!strncmp (word[1], "charecho", len))
    {
      charecho (user);
      return;
    }
  if (!strncmp (word[1], "wrap", len))
    {
      wraplines (user);
      return;
    }
  if (!strncmp (word[1], "blind", len))
    {
      if (user->level < MIN_LEV_BLIND)
	{
	  write_user (user,
		      ">>>You have no enough level to use this option.\n");
	  return;
	}
      write_user (user,
		  "\n~FR~OL~LI>>>Warning!~RS ~FR~OLYou enter in blind mode, you cannot view any messages sent to you!\n~FR~OL>>>To cancel, you must type .terminal command!\n");
      user->blind = 1;
      return;
    }
  if (!strncmp (word[1], "lines", len))
    {
      if (word_count < 3)
	{
	  user->plines = def_lines;
	  write_user (user, ">>>Number of paged lines set to default.\n");
	  return;
	}
      if (!isnumber (word[2]))
	write_usage (user, "terminal lines [ <number> ]");
      else
	{
	  if (!(user->plines = atoi (word[2])))
	    user->plines = def_lines;
	  sprintf (text, ">>>Number of paged lines set to ~FT%d~RS.\n",
		   user->plines);
	  write_user (user, text);
	}
      return;
    }
  write_usage (user, "terminal [ blind / charecho / lines / wrap ]");
}

/*** Free a hung socket ***/
clearline (user)
     UR_OBJECT user;
{
  UR_OBJECT u, next;
  int sock;

  if (word_count < 2)
    {
      write_usage (user, "clearline <line>/all");
      return;
    }
  if (!strcmp (word[1], "all"))
    {
      u = user_first;
      while (u)
	{
	  next = u->next;
	  if (u->socket == -1)
	    disconnect_user (u);
	  u = next;
	}
      write_user (user, ">>>Wrong lines cleared.\n");
      sprintf (text, "~OLGAEN:~RS %s clears all wrong lines.\n",
	       user->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      return;
    }
  if (!isnumber (word[1]))
    {
      write_usage (user, "clearline <line>/all");
      return;
    }
  sock = atoi (word[1]);

/* Find line amongst users */
  for (u = user_first; u != NULL; u = u->next)
    if (u->type != CLONE_TYPE && u->socket == sock)
      goto FOUND;
  write_user (user, ">>>That line is not currently active.\n");
  return;

FOUND:
  if (!u->login)
    {
      write_user (user,
		  ">>>You cannot clear the line of a logged in user.\n");
      return;
    }
  write_user (u, "\n\n>>>This line is being cleared.\n\n");
  disconnect_user (u);
  sprintf (text, "~OLGAEN:~RS %s cleared line %d.\n", user->savename, sock);
  write_syslog (LOG_COM, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
  sprintf (text, ">>>Line ~FG%d~RS cleared.\n", sock);
  write_user (user, text);
  destructed = 0;
  no_prompt = 0;
}


/*** Change whether a room's access is fixed or not ***/
change_room_fix (user, fix)
     UR_OBJECT user;
     int fix;
{
  RM_OBJECT rm;
  char *name;

  if (word_count < 2)
    rm = user->room;
  else
    {
      if ((rm = get_room (word[1])) == NULL)
	{
	  write_user (user, nosuchroom);
	  return;
	}
    }
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  if (fix)
    {
      if (rm->access & 2)
	{
	  if (rm == user->room)
	    write_user (user, ">>>This sky access is already fixed.\n");
	  else
	    write_user (user, ">>>That sky access is already fixed.\n");
	  return;
	}
      sprintf (text, ">>>Access for sky %s is now ~FRFIXED.\n", rm->name);
      write_user (user, text);
      if (user->room == rm)
	{
	  sprintf (text, ">>>%s has ~FRFIXED~RS access for this sky.\n",
		   name);
	  write_room_except (rm, text, user);
	}
      else
	{
	  sprintf (text, ">>>This sky's access has been ~FRFIXED.\n");
	  write_room (rm, text);
	}
      sprintf (text, "%s FIXED access to room %s.\n", user->savename,
	       rm->name);
      write_syslog (LOG_COM, text, LOG_TIME);
      rm->access += 2;
      return;
    }
  if (!(rm->access & 2))
    {
      if (rm == user->room)
	write_user (user, ">>>This sky access is already unfixed.\n");
      else
	write_user (user, ">>>That sky access is already unfixed.\n");
      return;
    }
  sprintf (text, ">>>Access for sky %s is now ~FGUNFIXED.\n", rm->name);
  write_user (user, text);
  if (user->room == rm)
    {
      sprintf (text, ">>>%s has ~FGUNFIXED~RS access for this sky.\n", name);
      write_room_except (rm, text, user);
    }
  else
    {
      sprintf (text, ">>>This sky's access has been ~FGUNFIXED.\n");
      write_room (rm, text);
    }
  sprintf (text, "%s UNFIXED access to room %s.\n", user->savename, rm->name);
  write_syslog (LOG_COM, text, LOG_TIME);
  rm->access -= 2;
  reset_access (rm);
}



/*** A newbie is requesting an account. Get his email address off him so we
     can validate who he is before we promote him and let him loose as a
     proper user. ***/
account_request (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  if (user->level > GUEST)
    {
      write_user (user,
		  ">>>This command is for new users only, you already have a full account.\n");
      return;
    }
/* This is so some pillock doesn't keep doing it just to fill up the syslog */
  if (user->accreq)
    {
      write_user (user, ">>>You have already requested an account.\n");
      return;
    }
  if (word_count < 2)
    {
      write_usage (user,
		   "accreq ~FG~OL<an email address we can contact you on + any relevent info>");
      return;
    }
/* Could check validity of email address I guess but its a waste of time.
   If they give a duff address they don't get an account, simple. 
   The talker admin can use .identify command to check the user's e-mail ***/
  sprintf (text, "ACCOUNT REQUEST from %s: %s.\n", user->savename, inpstr);
  write_syslog (LOG_CHAT, text, LOG_TIME);
  sprintf (text, "~OLGAEN:~RS %s has made a request for an account.\n",
	   user->name);
  write_level (SPIRIT, text, NULL);
  write_user (user, ">>>Account request logged.\n");
  user->accreq = 1;
}


/*** Clear the review buffer ***/
revclr (user)
     UR_OBJECT user;
{
  char *name;
  RM_OBJECT rm;

/* Check room */
  if (word_count < 2)
    rm = user->room;
  else
    {
      if ((rm = get_room (word[1])) == NULL)
	{
	  write_user (user, nosuchroom);
	  return;
	}
    }

  clear_rbuff (rm);
  write_user (user, ">>>Buffer cleared for that sky.\n");
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  sprintf (text, ">>>%s has cleared the review buffer.\n", name);
  write_room_except (rm, text, user);
}


/*** Clone an user in another room ***/
create_clone (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  RM_OBJECT rm;
  char *name;
  int cnt;

  if (user->restrict[RESTRICT_CLON] == restrict_string[RESTRICT_CLON])
    {
      write_user (user,
		  ">>>You have no right to use this command! Sorry...\n");
      return;
    }
/* Check room */
  if (word_count < 2)
    rm = user->room;
  else
    {
      if ((rm = get_room (word[1])) == NULL)
	{
	  write_user (user, nosuchroom);
	  return;
	}
    }

/* If private, the user cannot create a clone... */
  if (!has_room_access (user, rm))
    {
      write_user (user,
		  ">>>That sky is currently private, you cannot create a clone there.\n");
      return;
    }

/* Count clones and see if user already has a copy there , no point having
   2 in the same room */
  cnt = 0;
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->type == CLONE_TYPE && u->owner == user)
	{
	  if (u->room == rm)
	    {
	      sprintf (text, ">>>You already have a clone in the %s.\n",
		       rm->name);
	      write_user (user, text);
	      return;
	    }
	  if (++cnt == max_clones)
	    {
	      write_user (user,
			  ">>>You already have the maximum number of clones allowed.\n");
	      return;
	    }
	}
    }
/* Create clone */
  if ((u = create_user ()) == NULL)
    {
      sprintf (text, ">>>%s: Unable to create copy.\n", syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Unable to create user copy in clone().\n", 
		    LOG_NOTIME);
      return;
    }
  u->type = CLONE_TYPE;
  u->socket = user->socket;
  u->room = rm;
  u->owner = user;
  strcpy (u->savename, user->savename);
  strcpy (u->name, user->name);
/* Could just copy the structure but this is easier to code */
  if (!load_user_details (u))
    {
      sprintf (text, ">>>%s: Unable to reload your details.\n", syserror);
      write_user (user, text);
      sprintf (text, "ERROR: Unable to reload %s's details in clone().\n",
	       user->savename);
      write_syslog (LOG_ERR, text, LOG_NOTIME);
      return;
    }
  strcpy (u->desc, "~BR<CLONE>");
  if (rm == user->room)
    write_user (user,
		"~FB~OL>>>You invoke the universal power and a clone is created here.\n");
  else
    {
      sprintf (text,
	       "~FB~OL>>>You invoke the universal power and a clone is created in the %s.\n",
	       rm->name);
      write_user (user, text);
    }
  if (user->vis)
    name = user->name;
  else
    name = invisname;
  sprintf (text, "~FB~OL>>>%s invokes the universal power...\n", name);
  write_room_except (user->room, text, user);
  sprintf (text,
	   "~FB~OL>>>A clone of %s appears in a swirling magical mist!\n",
	   user->vis ? user->name : "a digit");
  write_room_except (rm, text, user);
}


/*** Destroy an user clone ***/
destroy_clone (user)
     UR_OBJECT user;
{
  UR_OBJECT u, u2;
  RM_OBJECT rm;
  char *name;

/* Check room and user */
  if (word_count < 2)
    rm = user->room;
  else
    {
      if ((rm = get_room (word[1])) == NULL)
	{
	  write_user (user, nosuchroom);
	  return;
	}
    }
  if (word_count > 2)
    {
      if ((u2 = get_user (word[2])) == NULL)
	{
	  write_user (user, notloggedon);
	  return;
	}
      if (u2->level >= user->level)
	{
	  write_user (user,
		      ">>>You cannot destroy the clone of an user of an equal or higher level.\n");
	  return;
	}
    }
  else
    u2 = user;
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->type == CLONE_TYPE && u->room == rm && u->owner == u2)
	{
	  destruct_user (u);
	  reset_access (rm);
	  write_user (user,
		      "~FM~OL>>>You invoke the skies and the clone is destroyed.\n");
	  if (user->vis)
	    name = user->name;
	  else
	    name = invisname;
	  sprintf (text, "~FM~OL>>>%s invokes the skies...\n", name);
	  write_room_except (user->room, text, user);
	  sprintf (text, "~FM~OL>>>The clone of %s shimmers and vanishes.\n",
		   u2->vis ? u2->name : "a digit");
	  write_room (rm, text);
	  if (u2 != user)
	    {
	      sprintf (text,
		       "~OLGAEN:~FR%s has destroyed your clone in the %s.\n",
		       user->vis ? user->name : invisname, rm->name);
	      write_user (u2, text);
	    }
	  destructed = 0;
	  return;
	}
    }
  if (u2 == user)
    sprintf (text, ">>>You do not have a clone in the %s.\n", rm->name);
  else
    sprintf (text, ">>>%s does not have a clone the %s.\n", u2->name,
	     rm->name);
  write_user (user, text);
}


/*** Show users own clones ***/
myclones (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  int cnt;

  if (user->restrict[RESTRICT_CLON] == restrict_string[RESTRICT_CLON])
    {
      write_user (user,
		  ">>>You have no right to use this command! Sorry...\n");
      return;
    }

  cnt = 0;
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->type != CLONE_TYPE || u->owner != user)
	continue;
      if (++cnt == 1)
	write_user (user,
		    "\n~FT>>>You have clones in the following skies:\n");
      sprintf (text, "	  %s ( name: %s )\n", u->room->name, u->name);
      write_user (user, text);
    }
  if (!cnt)
    write_user (user, ">>>You have no clones.\n");
  else
    {
      sprintf (text, "\n>>>Total of ~FT%d~RS clones.\n", cnt);
      write_user (user, text);
    }
}


/*** Show all clones on the system ***/
allclones (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  RM_OBJECT rm;
  int cnt;

  if (user->restrict[RESTRICT_CLON] == restrict_string[RESTRICT_CLON])
    {
      write_user (user,
		  ">>>You have no right to use this command! Sorry...\n");
      return;
    }
  if (word_count >= 2 && !strcmp (word[1], "clear") && user->level > GOD)
    {				/* destroy all clones from system */
      cnt = 0;
      for (u = user_first; u != NULL; u = u->next)
	{
	  if (u->type == CLONE_TYPE)
	    {
	      rm = u->room;
	      sprintf (text, ">>>Clone %s (owner %s) from %s destroyed...\n",
		       u->savename, u->owner->savename, rm->name);
	      write_user (user, text);
	      destruct_user (u);
	      reset_access (rm);
	      cnt++;
	    }
	}
      if (cnt)
	{
	  sprintf (text, ">>>Total of ~FT%d~RS clones destroyed.\n", cnt);
	  write_user (user, text);
	  sprintf (text, "%s destroyed all system clones.\n", user->savename);
	  write_syslog (LOG_COM, text, LOG_TIME);
	}
      else
	write_user (user, ">>>No clones found.\n");
      return;
    }
  cnt = 0;
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->type != CLONE_TYPE)
	continue;
      if (++cnt == 1)
	{
	  sprintf (text,
		   "\n~FB~OL--->>> Current clones on ~FT%s~RS~FB~OL <<<---\n\n",
		   long_date (0));
	  write_user (user, text);
	}
      sprintf (text, "%15s\t( %15s ):\t %s\n", u->name, u->owner->savename,
	       u->room->name);
      write_user (user, text);
    }
  if (!cnt)
    write_user (user, ">>>There are no clones on the system.\n");
  else
    {
      sprintf (text, "\nTotal of ~FT%d~RS clones.\n\n", cnt);
      write_user (user, text);
    }
}


/*** User swaps places with his own clone. All we do is swap the rooms the
	objects are in. ***/
clone_switch (user)
     UR_OBJECT user;
{
  UR_OBJECT u;
  RM_OBJECT rm;

  if (word_count < 2)
    {
      write_usage (user, "switch <sky clone is in>");
      return;
    }
  if ((rm = get_room (word[1])) == NULL)
    {
      write_user (user, nosuchroom);
      return;
    }
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->type == CLONE_TYPE && u->room == rm && u->owner == user)
	{
	  write_user (user,
		      "\n~FB~OL>>>You experience a strange sensation...\n");
	  u->room = user->room;
	  user->room = rm;
	  sprintf (text, "~FT>>>The clone of %s comes alive!\n",
		   u->vis ? u->name : "a digit");
	  write_room_except (user->room, text, user);
	  sprintf (text, "~FT>>>%s turns into a clone!\n",
		   u->vis ? u->name : invisname);
	  write_room_except (u->room, text, u);
	  look (user);
	  return;
	}
    }
  write_user (user, ">>>You do not have a clone in that sky. Sorry...\n");
}


/*** Make a clone speak ***/
clone_say (user, inpstr)
     UR_OBJECT user;
     char *inpstr;
{
  RM_OBJECT rm;
  UR_OBJECT u;

  if (user->muzzled)
    {
      write_user (user, ">>>You are muzzled, your clone cannot speak.\n");
      return;
    }
  if (word_count < 3)
    {
      write_usage (user, "csay <sky clone is in> <message>");
      return;
    }
  if ((rm = get_room (word[1])) == NULL)
    {
      write_user (user, nosuchroom);
      return;
    }
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->type == CLONE_TYPE && u->room == rm && u->owner == user)
	{
	  say (u, remove_first (inpstr));
	  return;
	}
    }
  write_user (user, ">>>You do not have a clone in that sky.\n");
}


/*** Set what a clone will hear, either all speach , just bad language
	or nothing. ***/
clone_hear (user)
     UR_OBJECT user;
{
  RM_OBJECT rm;
  UR_OBJECT u;

  if (word_count < 3
      || (strcmp (word[2], "all")
	  && strcmp (word[2], "swears") && strcmp (word[2], "nothing")))
    {
      write_usage (user, "chear <sky clone is in> all/swears/nothing");
      return;
    }
  if ((rm = get_room (word[1])) == NULL)
    {
      write_user (user, nosuchroom);
      return;
    }
  for (u = user_first; u != NULL; u = u->next)
    {
      if (u->type == CLONE_TYPE && u->room == rm && u->owner == user)
	break;
    }
  if (u == NULL)
    {
      write_user (user, ">>>You do not have a clone in that sky.\n");
      return;
    }
  if (!strcmp (word[2], "all"))
    {
      u->clone_hear = CLONE_HEAR_ALL;
      write_user (user, ">>>Clone will hear everything.\n");
      return;
    }
  if (!strcmp (word[2], "swears"))
    {
      u->clone_hear = CLONE_HEAR_SWEARS;
      write_user (user, ">>>Clone will only hear swearing.\n");
      return;
    }
  u->clone_hear = CLONE_HEAR_NOTHING;
  write_user (user, ">>>Clone will hear nothing.\n");
}


/*** Stat a remote system ***/
remote_stat (user)
     UR_OBJECT user;
{
  NL_OBJECT nl;
  RM_OBJECT rm;

  if (word_count < 2)
    {
      write_usage (user, "rstat <sky service is linked to>");
      return;
    }
  if ((rm = get_room (word[1])) == NULL)
    {
      write_user (user, nosuchroom);
      return;
    }
  if ((nl = rm->netlink) == NULL)
    {
      write_user (user, ">>>That sky is not linked to a service.\n");
      return;
    }
  if (nl->stage != 2)
    {
      write_user (user, ">>>Not (fully) connected to service.\n");
      return;
    }
  if (nl->ver_major <= 3 && nl->ver_minor < 1)
    {
      write_user (user,
		  ">>>The NUTS version of that service does not support this facility.\n");
      return;
    }
  sprintf (text, "RSTAT %s\n", user->savename);
  write_sock (nl->socket, text);
  write_user (user, ">>>Request sent.\n");
}


/*** Swearing file operations ***/
swear_com (user)
     UR_OBJECT user;
{
  int i, sw_save;
  char filename[80], line[WORD_LEN + 1];
  FILE *fp, *fpout;

  if (word_count < 2)
    {
      write_usage (user, "swears list/add/erase/zap/on/off/reload");
      return;
    }
  sprintf (filename, "%s/%s", DATAFILES, SWEARFILE);
  if (word[1][0] == '?' || !strcmp (word[1], "list"))	/* list swear words */
    {
      i = 0;
      while (swear_words[i][0] != '*')
	{
	  if (!i)
	    write_user (user, "\n~FB~OL--->>> Swearing ban list <<<---\n\n");
	  sprintf (text, "\t'~FT%s~RS'\n", swear_words[i++]);
	  write_user (user, text);
	}
      if (!i)
	write_user (user, ">>>No swear words found.\n");
      else
	{
	  sprintf (text, ">>>Total of ~FT%d~RS swear words found.\n", i);
	  write_user (user, text);
	}
      return;
    }
  if (!strcmp (word[1], "zap"))	/* remove swear file */
    {
      s_unlink (filename);
      sprintf (text, "~OLGAEN:~RS %s zaps swear file.\n", user->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      write_user (user,
		  ">>>Swear file deleted.\n>>>You must use \".swear reload\" to clear internal words list...\n");
      return;
    }
  if (!strcmp (word[1], "on"))	/* swearing ban on */
    {
      if (ban_swearing)
	write_user (user, ">>>Swearing ban is already ~FGON.\n");
      else
	{
	  write_user (user, ">>>Swearing ban ~FGON.\n");
	  sprintf (text, "~OLGAEN:~RS %s switched swearing ban ON.\n",
		   user->savename);
	  write_syslog (LOG_COM, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, user);
	  ban_swearing = 1;
	}
      return;
    }
  if (!strcmp (word[1], "off"))	/* swearing ban off */
    {
      if (!ban_swearing)
	write_user (user, ">>>Swearing ban is already ~FROFF.\n");
      else
	{
	  write_user (user, ">>>Swearing ban ~FROFF.\n");
	  sprintf (text, "~OLGAEN:~RS %s switched swearing ban OFF.\n",
		   user->savename);
	  write_syslog (LOG_COM, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, user);
	  ban_swearing = 0;
	}
      return;
    }
  if (!strcmp (word[1], "add") || word[1][0] == '+')	/* add a word to swear file */
    {
      if (word_count < 3)
	write_usage (user, "swears add <word>\n");
      else
	{
	  for (i = 0; i < strlen (word[2]); i++)	/* change '_' in space */
	    if (word[2][i] == '_')
	      word[2][i] = ' ';
	  i = 0;
	  sw_save = ban_swearing;
	  ban_swearing = 0;
	  while (swear_words[i][0] != '*')
	    {
	      if (!strcmp (swear_words[i], word[2]))
		{
		  write_user (user, ">>>This word already exists!\n");
		  ban_swearing = sw_save;
		  return;
		}
	      i++;
	    }
	  if (i < MAX_SWEARS)
	    {
	      strcpy (swear_words[i], word[2]);
	      strcpy (swear_words[i + 1], "*");
	    }
	  else
	    {
	      write_user (user, ">>>Sorry, swear word list is full!\n");
	      ban_swearing = sw_save;
	      return;
	    }
	  if (!(fp = fopen (filename, "a")))
	    {
	      sprintf (text, ">>>%s: Unable to append file.\n", syserror);
	      write_user (user, text);
	      sprintf (text,
		       "ERROR: Unable to append %s file in swear_com().\n",
		       SWEARFILE);
	      write_syslog (LOG_ERR, text, LOG_NOTIME);
	      ban_swearing = sw_save;
	      return;
	    }
	  fprintf (fp, "%s\n", word[2]);
	  fclose (fp);
	  ban_swearing = sw_save;
	  write_user (user, ">>>Swear word added.\n");
	  sprintf (text, "~OLGAEN:~RS %s added '%s' to swear file.\n",
		   user->savename, word[2]);
	  write_syslog (LOG_COM, text, LOG_TIME);
	  write_level (MIN_LEV_NOTIFY, text, user);
	}
      return;
    }
  if (!strcmp (word[1], "reload"))	/* reload swear file */
    {
      sw_save = ban_swearing;
      load_swear_file (user);
      ban_swearing = sw_save;
      sprintf (text, "~OLGAEN:~RS %s reload swear file.\n", user->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      return;
    }
  if (!strcmp (word[1], "erase") || word[1][0] == '-')	/* erase a word from swear file */
    {
      if (word_count < 3)
	{
	  write_usage (user, "swears erase <word>\n");
	  return;
	}
      sprintf (filename, "%s/%s", DATAFILES, SWEARFILE);
      if (!(fp = fopen (filename, "r")))
	{
	  write_user (user, ">>>Swear file not found.\n");
	  return;
	}
      if (!(fpout = fopen ("tempfile", "w")))
	{
	  sprintf (text, "%s: Couldn't open tempfile.\n", syserror);
	  write_user (user, text);
	  write_syslog (LOG_ERR,
			"ERROR: Couldn't open tempfile to write in swear_com().\n",
			LOG_NOTIME);
	  fclose (fp);
	  return;
	}
      i = 0;
      fscanf (fp, "%s", line);
      while (!feof (fp))
	{
	  if (!strstr (word[2], line))
	    fprintf (fpout, "%s\n", line);
	  else
	    i = 1;
	  fscanf (fp, "%s", line);
	}
      fclose (fp);
      fclose (fpout);
      if (!i)
	{
	  write_user (user, ">>>Swear word not found.\n");
	  unlink ("tempfile");
	  return;
	}
      rename ("tempfile", filename);
      write_user (user,
		  ">>>Swear word erased.\n>>>You must reload swear file to update words internal list...\n");
      sprintf (text, "~OLGAEN:~RS %s erased '%s' from swear file.\n",
	       user->savename, word[2]);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      return;
    }
  write_user (user, ">>>Unknown option, yet...\n");
}

/*** Toggle colour user flag ***/
toggle_colour (user)
     UR_OBJECT user;
{
  int col;

  if (user->command_mode && user->ignall && user->charmode_echo
      && user->dimension > 0)
    {				/* a surprise... - an (cool?!) easter egg :) */
      for (col = 1; col < NUM_COLS; ++col)
	{
	  sprintf (text, ">>>/~%s: ~%sGAEN VIDEO TEST~RS\n", colcom[col],
		   colcom[col]);
	  write_user (user, text);
	}
      return;
    }
  if (user->level >= MIN_LEV_TRANSP &&
      word_count >= 2 && !strcmp (word[1], "transparent"))
    {
      user->colour = 2;
      write_user (user,
		  ">>>Now, you are transparent to login/logout time.\n");
      sprintf (text, "~OLGAEN:~RS %s become 'transparent'.\n",
	       user->savename);
      write_syslog (LOG_COM, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
      return;
    }
  if (user->colour)
    {
      write_user (user, ">>>Colour ~FROFF.\n");
      user->colour = 0;
    }
  else
    {
      user->colour = 1;
      write_user (user, ">>>Colour ~FGON.\n");
    }
  if (user->room == NULL)
    prompt (user);
}

/*** Toggle ignore shouts' user flag ***/
toggle_ignshout (user)
     UR_OBJECT user;
{
  if (user->ignshout)
    {
      write_user (user,
		  ">>>You are no longer ignoring shouts and shout emotes.\n");
      user->ignshout = 0;
      return;
    }
  write_user (user, ">>>You are now ignoring shouts and shout emotes.\n");
  user->ignshout = 1;
}

/*** Toggle ignore tells' user flag ***/
toggle_igntell (user)
     UR_OBJECT user;
{
  if (user->igntell)
    {
      write_user (user,
		  ">>>You are no longer ignoring tells and private emotes.\n");
      user->igntell = 0;
      return;
    }
  write_user (user, ">>>You are now ignoring tells and private emotes.\n");
  user->igntell = 1;
}

/*** Suicide ***/
suicide (user)
     UR_OBJECT user;
{
  if (word_count < 2)
    {
      write_usage (user, "suicide <your password>");
      return;
    }
  if (user->restrict[RESTRICT_SUIC] == restrict_string[RESTRICT_SUIC])
    {
      write_user (user, ">>>You cannot use right now this feature...\n");
      return;
    }
  if (strcmp ((char *) crypt (word[1], PASS_SALT), user->pass))
    {
      write_user (user, ">>>Incorrect password...\n");
      return;
    }
  write_user (user,
	      "\n\07~FR~OL~LI--->>> WARNING - This will delete your account! <<<---\n\n>>>Are you sure about this (y/n)? ");
  user->misc_op = 6;
  no_prompt = 1;
}

/*** Delete all user's files (used especially by delete_user() function) ***/
delete_files (name)
     char *name;
{
  char extens[] = "DMPSRUIONWE";
  int i;
  char filename[80];

  /* first, delete the user files */
  for (i = 0; i < strlen (extens); i++)
    {
      sprintf (filename, "%s/%s.%c", USERFILES, name, extens[i]);
      s_unlink (filename);
    }
  /* delete the run commands files */
  for (i = 0; i < 10; i++)
    {
      sprintf (filename, "%s/%s.%d", USERFILES, name, i);
      s_unlink (filename);
    }
  /* delete the environment files */
  for (i = 0; i < 10; i++)
    {
      sprintf (filename, "%s/%s.E%d", USERFILES, name, i);
      s_unlink (filename);
    }
}

/*** Delete an user ***/
delete_user (user, this_user)
     UR_OBJECT user;
     int this_user;
{
  UR_OBJECT u;
  char name[USER_NAME_LEN + 1];

  if (this_user)
    {
      /* User structure gets destructed in disconnect_user(), need to keep a
         copy of the name */
      strcpy (name, user->savename);
      write_user (user, "\n~FR~LI~OL>>>ACCOUNT DELETED!\n");
      disconnect_user (user);
      sprintf (text, "%s SUICIDED.\n", name);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      delete_files (name);
      return;
    }
  if (word_count < 2)
    {
      write_usage (user, "delete <user>");
      return;
    }
  word[1][0] = toupper (word[1][0]);
  if (!strcmp (word[1], user->savename))
    {
      write_user (user,
		  ">>>Trying to delete yourself is the eleventh sign of madness or genius.\n");
      return;
    }
  if (get_user (word[1]) != NULL || get_real_user (word[1]) != NULL)
    {
      /* Safety measure just in case. Will have to .kill them first */
      write_user (user,
		  ">>>You cannot delete an user who is currently logged on.\n");
      return;
    }
  if ((u = create_user ()) == NULL)
    {
      sprintf (text, ">>>%s: unable to create temporary user object.\n",
	       syserror);
      write_user (user, text);
      write_syslog (LOG_ERR,
		    "ERROR: Unable to create temporary user object in delete_user().\n",
		    LOG_NOTIME);
      return;
    }
  strcpy (u->savename, word[1]);
  if (!load_user_details (u))
    {
      write_user (user, nosuchuser);
      destruct_user (u);
      destructed = 0;
      return;
    }
  if (u->level >= user->level)
    {
      write_user (user,
		  ">>>You cannot delete an user of an equal or higher level than yourself.\n");
      destruct_user (u);
      destructed = 0;
      return;
    }
  destruct_user (u);
  destructed = 0;
  delete_files (word[1]);
  sprintf (text, "\07~FR~OL~LI>>>User %s deleted!\n", word[1]);
  write_user (user, text);
  sprintf (text, "~OLGAEN:~RS %s deleted %s.\n", user->savename, word[1]);
  write_syslog (LOG_CHAT, text, LOG_TIME);
  write_level (MIN_LEV_NOTIFY, text, user);
/* Delete user name from allow files */
  write_user (user, ">>>Searching user in allow files...\n");
  strcpy (word[2], word[1]);
  strcpy (word[1], "all");
  word_count = 3;
  dallow (user);
}


/*** Shutdown talker interface func. Countdown time is entered in seconds so
	we can specify less than a minute till reboot. ***/
shutdown_com (user)
     UR_OBJECT user;
{
  if (rs_which == 1)
    {
      write_user (user,
		  ">>>The reboot countdown is currently active, you must cancel it first.\n");
      return;
    }
  if (!strcmp (word[1], "cancel"))
    {
      if (!rs_countdown || rs_which != 0)
	{
	  write_user (user,
		      ">>>The shutdown countdown is not currently active.\n");
	  return;
	}
      if (rs_countdown && !rs_which && rs_user == NULL)
	{
	  write_user (user,
		      ">>>Someone else is currently setting the shutdown countdown.\n");
	  return;
	}
      write_room (NULL, "~OLGAEN:~RS~FG Shutdown cancelled.\n");
      sprintf (text, "%s cancelled the shutdown countdown.\n",
	       user->savename);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      rs_countdown = 0;
      rs_announce = 0;
      rs_which = -1;
      rs_user = NULL;
      return;
    }
  if (word_count > 1 && !isnumber (word[1]))
    {
      write_usage (user, "shutdown [ <secs>/cancel ]");
      return;
    }
  if (rs_countdown && !rs_which)
    {
      write_user (user,
		  ">>>The shutdown countdown is currently active, you must cancel it first.\n");
      return;
    }
  if (word_count < 2)
    {
      rs_countdown = 0;
      rs_announce = 0;
      rs_which = -1;
      rs_user = NULL;
    }
  else
    {
      rs_countdown = atoi (word[1]);
      rs_which = 0;
    }
  write_user (user,
	      "\n\07~FR~OL~LI--->>> WARNING - This will shutdown the talker! <<<---\n\n>>>Are you sure about this (y/n)? ");
  user->misc_op = 1;
  no_prompt = 1;
}


/*** Reboot talker interface func. ***/
reboot_com (user)
     UR_OBJECT user;
{
  if (!rs_which)
    {
      write_user (user,
		  ">>>The shutdown countdown is currently active, you must cancel it first.\n");
      return;
    }
  if (!strcmp (word[1], "cancel"))
    {
      if (!rs_countdown)
	{
	  write_user (user,
		      ">>>The reboot countdown is not currently active.\n");
	  return;
	}
      if (rs_countdown && rs_user == NULL)
	{
	  write_user (user,
		      ">>>Someone else is currently setting the reboot countdown.\n");
	  return;
	}
      write_room (NULL, "~OLGAEN:~RS~FG Reboot cancelled.\n");
      sprintf (text, "%s cancelled the reboot countdown.\n", user->savename);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      rs_countdown = 0;
      rs_announce = 0;
      rs_which = -1;
      rs_user = NULL;
      return;
    }
  if (word_count > 1 && !isnumber (word[1]))
    {
      write_usage (user, "reboot [ <secs>/cancel ]");
      return;
    }
  if (rs_countdown)
    {
      write_user (user,
		  ">>>The reboot countdown is currently active, you must cancel it first.\n");
      return;
    }
  if (word_count < 2)
    {
      rs_countdown = 0;
      rs_announce = 0;
      rs_which = -1;
      rs_user = NULL;
    }
  else
    {
      rs_countdown = atoi (word[1]);
      rs_which = 1;
    }
  write_user (user,
	      "\n\07~FY~OL~LI--->>> WARNING - This will reboot the talker! <<<---\n\n>>>Are you sure about this (y/n)? ");
  user->misc_op = 7;
  no_prompt = 1;
}



/*** Show recorded tells and pemotes ***/
revtell (user)
     UR_OBJECT user;
{
  int i, cnt, line;

  if (user->restrict[RESTRICT_VIEW] == restrict_string[RESTRICT_VIEW])
    {
      write_user (user,
		  ">>>Sorry, you have no right to access the revtell buffer.\n");
      return;
    }

  cnt = 0;
  for (i = 0; i < REVTELL_LINES; ++i)
    {
      line = (user->revline + i) % REVTELL_LINES;
      if (user->revbuff[line][0])
	{
	  cnt++;
	  if (cnt == 1)
	    write_user (user,
			"\n~FB~OL--->>> Your revtell buffer <<<---\n\n");
	  write_user (user, user->revbuff[line]);
	}
    }
  if (!cnt)
    write_user (user, ">>>Revtell buffer is empty.\n");
  else
    write_user (user, "\n~FB~OL------>>> End <<<------\n\n");
}


/*** Show recorded sent pictures ***/
review_pict (user)
     UR_OBJECT user;
{
  int i, cnt, line;

  if (user->restrict[RESTRICT_VIEW] == restrict_string[RESTRICT_VIEW])
    {
      write_user (user,
		  ">>>Sorry, you have no right to access the revpict buffer.\n");
      return;
    }

  cnt = 0;
  for (i = 0; i < REVPICT_LINES; ++i)
    {
      line = (user->revline + i) % REVPICT_LINES;
      if (user->revpict[line][0])
	{
	  cnt++;
	  if (cnt == 1)
	    write_user (user,
			"\n~FB~OL--->>> Review pictures buffer <<<---\n\n");
	  write_user (user, user->revpict[line]);
	}
    }
  if (!cnt)
    write_user (user, ">>>No pictures.\n");
  else
    write_user (user, "\n~FB~OL------>>> End <<<------\n\n");
}


/**************************** EVENT FUNCTIONS ******************************/

void
do_events ()
{
  set_date_time ();
  check_reboot_shutdown ();
  check_idle_and_timeout ();
  check_nethangs_send_keepalives ();
  check_messages (NULL, 0);
  autosave_details ();
  reset_alarm ();
}

/*** autosave details for all users ***/
autosave_details ()
{
  UR_OBJECT u;
  char message[25];
  char cmirrors[MAX_MIRRORS][12] =
    { "~FG~OLEDEN", "~OLGAEN", "~FR~OLHELL", "~FM~OLDILI" };

  if (++autosave_counter > autosave_maxiter)
    {
      for (u = user_first; u != NULL; u = u->next)
	{
	  if (u->type != USER_TYPE || u->login)
	    continue;
	  /* autosave user details */
	  save_user_details (u, 1);

	  if (u->ignall)
	    continue;
	}
      /* some additional messages... */
      if (autosave_action & AS_GREET)
	{
	  if (thour < 5 || thour >= 22)
	    strcpy (message, "~FB~OLGood night!");
	  else if (thour >= 5 && thour < 12)
	    strcpy (message, "~FR~OLGood morning!");
	  else if (thour >= 12 && thour < 18)
	    strcpy (message, "~FY~OLGood afternoon!");
	  else
	    strcpy (message, "~FT~OLGood evening!");
	  sprintf (text,
		   ">>>%s~RS %s~RS wishes you a nice day at ~FG%02d:%02d~RS... ~FY~OLKeep talking!~RS\n",
		   message, cmirrors[mirror], thour, tmin);
	  write_room (NULL, text);
	}
      if (autosave_action & AS_QUOTE)
	{
	  quote (NULL, 0);
	}
      write_syslog (LOG_CHAT, "Autosave\n", LOG_TIME);
      autosave_counter = 0;
      if (no_disable_mirror)
	mirror_chat (NULL);
    }
}

reset_alarm ()
{
  signal (SIGALRM, do_events);
  alarm (heartbeat);
}

/*** See if timed reboot or shutdown is underway ***/
check_reboot_shutdown ()
{
  int secs;
  char *w[] = { "~FRShutdown", "~FYRebooting" };

  if (rs_user == NULL)
    return;
  rs_countdown -= heartbeat;
  if (rs_countdown <= 0)
    talker_shutdown (rs_user, NULL, rs_which);

/* Print countodwn message every minute unless we have less than 1 minute
   to go when we print every 10 secs */
  secs = (int) (time (0) - rs_announce);
  if (rs_countdown >= 60 && secs >= 60)
    {
      sprintf (text, "\07~OLGAEN: %s in %d minutes, %d seconds.\n",
	       w[rs_which], rs_countdown / 60, rs_countdown % 60);
      write_room (NULL, text);
      rs_announce = time (0);
    }
  if (rs_countdown < 60 && secs >= 10)
    {
      sprintf (text, "\07~OLGAEN: %s in %d seconds.\n", w[rs_which],
	       rs_countdown);
      write_room (NULL, text);
      rs_announce = time (0);
    }
}



/*** login_time_out is the length of time someone can idle at login, 
     user_idle_time is the length of time they can idle once logged in. 
     Also ups users total login time. ***/
check_idle_and_timeout ()
{
  UR_OBJECT user, next;
  int tm;

/* Use while loop here instead of for loop for when user structure gets
   destructed, we may lose ->next link and crash the program */
  user = user_first;
  while (user)
    {
      next = user->next;
      if (user->type == CLONE_TYPE)
	{
	  user = next;
	  continue;
	}
      user->total_login += heartbeat;
      if (enable_event)
	exec_event (user);
      /* check alarm conditions */
      if (user->alarm_hour == thour)
	{
	  if (user->alarm_min == tmin && user->alarm_activated)
	    {
	      sprintf (text,
		       "~EL~EL~OLGAEN: ~FYAlarm activated!~RS~OL - command: ~FT%s\n",
		       user->alarm_cmd[0] ? user->alarm_cmd : "none");
	      write_user (user, text);
	      user->alarm_activated = 0;
	      /* try to execute the alarm command... */
	      if (user->alarm_cmd[0])
		{
		  clear_words ();
		  word_count = wordfind (user, user->alarm_cmd);
		  com_num = -1;
		  exec_com (user, user->alarm_cmd);
		}
	    }
	}
      if (user->level > timeout_maxlevel)
	{
	  user = next;
	  continue;		/* Don't time out superusers */
	}
      tm = (int) (time (0) - user->last_input);
      if (user->login && tm >= login_idle_time)
	{
	  write_user (user, "\n\n--->>> Time out <<<---\n\n");
	  disconnect_user (user);
	  user = next;
	  continue;
	}
      if (user->warned)
	{
	  if (tm < user_idle_time - 60)
	    {
	      user->warned = 0;
	      continue;
	    }
	  if (tm >= user_idle_time)
	    {
	      sprintf (text, "\n~OLGAEN:~RS~FR~OL %s has been timed out...\n",
		       user->savename);
	      write_level (SAINT, text, user);
	      write_user (user,
			  "\n\n\07~FR~OL~LI--->>> You have been timed out. <<<---\n\n");
	      disconnect_user (user);
	      user = next;
	      continue;
	    }
	}
      if ((!user->afk || (user->afk && time_out_afks))
	  && !user->login && !user->warned && tm >= user_idle_time - 60)
	{
	  write_user (user,
		      "\n\07~FY~OL~LI--->>> WARNING - Input within 1 minute or you will be disconnected. <<<---\n\n");
	  user->warned = 1;
	}
      user = next;
    }
  event = E_NONE;
  strcpy (event_var, "*");
}



/*** See if any net connections are dragging their feet. If they have been idle
     longer than net_idle_time the drop them. Also send keepalive signals down
     links, this saves having another function and loop to do it. ***/
check_nethangs_send_keepalives ()
{
  NL_OBJECT nl;
  int secs;

  for (nl = nl_first; nl != NULL; nl = nl->next)
    {
      if (nl->type == UNCONNECTED)
	{
	  nl->warned = 0;
	  continue;
	}

      /* Send keepalives */
      nl->keepalive_cnt += heartbeat;
      if (nl->keepalive_cnt >= keepalive_interval)
	{
	  write_sock (nl->socket, "KA\n");
	  nl->keepalive_cnt = 0;
	}

      /* Check time outs */
      secs = (int) (time (0) - nl->last_recvd);
      if (nl->warned)
	{
	  if (secs < net_idle_time - 60)
	    nl->warned = 0;
	  else
	    {
	      if (secs < net_idle_time)
		continue;
	      sprintf (text,
		       "~OLGAEN:~RS Disconnecting hung netlink to %s in the %s.\n",
		       nl->service, nl->connect_room->name);
	      write_room (NULL, text);
	      shutdown_netlink (nl);
	      nl->warned = 0;
	    }
	  continue;
	}
      if (secs > net_idle_time - 60)
	{
	  sprintf (text,
		   "~OLGAEN:~RS Netlink to %s in the %s has been hung for %d seconds.\n",
		   nl->service, nl->connect_room->name, secs);
	  write_level (SPIRIT, text, NULL);
	  nl->warned = 1;
	}
    }
  destructed = 0;
}



/*** Remove any expired messages from boards unless force = 2 in which case
	just do a recount. ***/
check_messages (user, force)
     UR_OBJECT user;
     int force;
{
  RM_OBJECT rm;
  FILE *infp, *outfp;
  char id[82], filename[80], line[82];
  int i, valid, pt, write_rest;
  int board_cnt, old_cnt, bad_cnt, tmp;
  static int done = 0;

  switch (force)
    {
    case 0:
      if (mesg_check_hour == thour && mesg_check_min == tmin)
	{
	  if (done)
	    return;
	}
      else
	{
	  done = 0;
	  return;
	}
      break;

    case 1:
      printf ("Checking boards...\n");
    }
  done = 1;
  board_cnt = 0;
  old_cnt = 0;
  bad_cnt = 0;

  for (i = 0; i < MAX_DIMENS; i++)
    for (rm = room_first[i]; rm != NULL; rm = rm->next)
      {
	tmp = rm->mesg_cnt;
	rm->mesg_cnt = 0;
	sprintf (filename, "%s/%s.B", DATAFILES, rm->name);
	if (!(infp = fopen (filename, "r")))
	  continue;
	if (force < 2)
	  {
	    if (!(outfp = fopen ("tempfile", "w")))
	      {
		if (force)
		  fprintf (stderr, "GAEN: Couldn't open tempfile.\n");
		write_syslog (LOG_ERR,
			      "ERROR: Couldn't open tempfile in check_messages().\n",
			      LOG_NOTIME);
		fclose (infp);
		return;
	      }
	  }
	board_cnt++;
	/* We assume that once 1 in date message is encountered all the others
	   will be in date too , hence write_rest once set to 1 is never set to
	   0 again */
	valid = 1;
	write_rest = 0;
	fgets (line, 82, infp);	/* max of 80+newline+terminator = 82 */
	while (!feof (infp))
	  {
	    if (*line == '\n')
	      valid = 1;
	    sscanf (line, "%s %d", id, &pt);
	    if (!write_rest)
	      {
		if (valid && !strcmp (id, "PT:"))
		  {
		    if (force == 2)
		      rm->mesg_cnt++;
		    else
		      {
			/* 86400 = num. of secs in a day */
			if ((int) time (0) - pt < mesg_life * 86400)
			  {
			    fputs (line, outfp);
			    rm->mesg_cnt++;
			    write_rest = 1;
			  }
			else
			  old_cnt++;
		      }
		    valid = 0;
		  }
	      }
	    else
	      {
		fputs (line, outfp);
		if (valid && !strcmp (id, "PT:"))
		  {
		    rm->mesg_cnt++;
		    valid = 0;
		  }
	      }
	    fgets (line, 82, infp);
	  }
	fclose (infp);
	if (force < 2)
	  {
	    fclose (outfp);
	    unlink (filename);
	    if (!write_rest)
	      unlink ("tempfile");
	    else
	      rename ("tempfile", filename);
	  }
	if (rm->mesg_cnt != tmp)
	  bad_cnt++;
      }
  switch (force)
    {
    case 0:
      if (bad_cnt)
	sprintf (text,
		 "CHECK_MESSAGES: %d file(s) checked, %d had an incorrect message count, %d message(s) deleted.\n",
		 board_cnt, bad_cnt, old_cnt);
      else
	sprintf (text,
		 "CHECK_MESSAGES: %d file(s) checked, %d message(s) deleted.\n",
		 board_cnt, old_cnt);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      break;

    case 1:
      printf ("  %d board file(s) checked, %d out of date message(s) found.\n",
	      board_cnt, old_cnt);
      break;

    case 2:
      sprintf (text,
	       ">>>~FG%d~RS board file(s) checked, ~FG%d~RS had an incorrect message count.\n",
	       board_cnt, bad_cnt);
      write_user (user, text);
      sprintf (text,
	       "~OLGAEN:~RS %s forced a recount of the message boards.\n",
	       user->savename);
      write_syslog (LOG_CHAT, text, LOG_TIME);
      write_level (MIN_LEV_NOTIFY, text, user);
    }
}

/************************** Made in England & Romania ************************/
