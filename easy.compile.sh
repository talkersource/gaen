#!/bin/sh

#	Purpose: This script should make simplier the compiling of GAEN
# sources, avoiding user to search the right parametters for GCC compiler.
# There are a finite number of parameters/problems which could cause
# compiling crashes, all of them (we hope) being tested below.

#	Usage: easy.compile [SourceFile.c] [ -o ] [ OutputFile ]
#	If the "-o" parameter is not present then the arguments will be
# parsed in the following order: SourceFile OutputFile. If it is present,
# then the OutputFile will be taken from the next argument.

#       Any of those parameters may not be present, as you can see below:
#	$ easy.compile
#	$ easy.compile -o gaend
#	$ easy.compile MyOwnSourceFile.c
#	$ easy.compile -o MyOutputFile MyOwnSourceFile.c
#	$ easy.compile MyOwnSourceFile.c MyOutputFile
#	$ easy.compile --help

# Some modifications are made by Sabin Corneliu Buraga <busaco@infoiasi.ro>:
#	- messages' colours changed
#	- all temporary "a.out" files are written in "/tmp", 
# 	  instead of current directory 
#	- minor corrections to internal script comments/remarks
#	- the "--help" parameter added to show the syntax of this script
#	- using of "strip --strip-all" instead of "strip" without parameters

 
Version="[1m[33m1.3 final ([31minternal version 1.1[33m), 19 November 2001[0m"
echo;
echo "              >>>[32mGAEN[0m [1mEasy Compile[0m Concept script<<<"
echo;
echo ">>>Version "${Version}".";
echo ">>>Author: Victor Tarhon-Onu [1m<mituc@ac.tuiasi.ro>";
echo "           (modified by Sabin-Corneliu Buraga [1m<busaco@infoiasi.ro>)[0m";     
echo;

if [ $# -eq 1 ]; then
  if [ "$1" = "--help" ]; then
    echo;
    echo "Usage: easy.compile [SourceFile.c] [ -o ] [ OutputFile ]"
    echo;
    exit 2;
  fi
fi

# No cores...
ulimit -c 0;

# Setting up PATH if we are running on nasty configurated systems.
PATH=/usr/bin:/bin:/usr/local/bin:/usr/ucb

# Selecting the best tools for viewing processes and showing text...

# It seems like 'ps' does not have universal options. We'll use
# "-ef" or "-awxu" parameters, depending on ps's requirements.

if [ "`ps -elf 2>&1 1>/dev/null`" = "" ]; then
  PS_ARGS="-elf";
else
  PS_ARGS="-auxw"
fi;

# Some broken echo's may require this...
SYSV3=""

ECHO_ENUM=`whereis ps | cut -d " " -f 2-`;
for i in $ECHO_ENUM; do
  if [ "`echo -n`" = "" ]; then
    break;
  fi;
done;

# Does out echo support -e option...?
if [ "`echo -e`" = "" ]; then
  ECHO="echo -e";
else
  ECHO="echo";
fi;

# Initialising default parameters...
LIBS="";
FLAGS="";
DEFAULT_SOURCE_FILE="src/gaen.c";
DEFAULT_OUTPUT_FILE="gaend";
SOURCE_FILE="";
DEFAULT_BASE_FILE="gaen.c";
OUTPUT_FILE="";

# ... and some internal constants and functions.

UNIQUE="$RANDOM";
GAENCompillingScript="/tmp/"$UNIQUE"GAENCompillingScript";
LockFile="/tmp/"$UNIQUE"GAENLockFile";
ErrFile="/tmp/"$UNIQUE"GAENErrFile";
UserName=`whoami`;

# Terminating signals handler function.
make_clean()
{
	# Killing the gcc compiling the source file.
	for PID in `ps "$PS_ARGS" | grep "$UserName" | grep -v "${0}" | grep -v grep | grep "${BASE_FILE}" | awk {'print $4'}`; do
		kill -15 "${PID}" 2>/dev/null 1>/dev/null;
	done;

	# ...and the cc1's too. cc1 does not have an argument who's name
	# is alike with the source's name... So we'll kill'em all :)
	for PID in `ps "$PS_ARGS" | grep "$UserName" | grep -v "${0}" | grep -v grep | grep cc1 | awk '{print $4}'`; do
                kill -15 "${PID}" 2>/dev/null 1>/dev/null;
        done;

	# Remove all temporarily created files.
	rm -f "${GAENCompillingScript}" "${LockFile}" "${ErrFile}" \
	      /tmp/gcctest.c /tmp/crypt.c /tmp/socket.c \
	      /tmp/nsl.c /tmp/opt.c /tmp/a.out 2>/dev/null 1>/dev/null;
	echo "[0m";
	exit;
}

# This will delete all files made during tests and compiling activity.
trap make_clean 0 1 2 3 15;

# Parsing command line arguments - if any...

FoundOptFlag=0;
for Param in "${@}"; do	
	if [ "${Param}" = "-o" ]; then
		FoundOptFlag=1;
	    else
		if [ "${FoundOptFlag}" = "1" ]; then
			OUTPUT_FILE="${Param}";
			FoundOptFlag=0;
		    else
			if [ "${SOURCE_FILE}" = "" ]; then
				SOURCE_FILE="${Param}";
			    else
				OUTPUT_FILE="${Param}";
			fi;
		fi;
		
	fi;
done;

# If some params where not found then we'll initialise them default values.
if [ "${SOURCE_FILE}" = "" ]; then 
  SOURCE_FILE="${DEFAULT_SOURCE_FILE}";
  BASE_FILE="${DEFAULT_BASE_FILE}";
else
  BASE_FILE=`basename "${SOURCE_FILE}"`;
fi;

# Determine the relative filename of the sourcefile - useful in make_clean();
if [ "${OUTPUT_FILE}" = "" ]; then 
  OUTPUT_FILE="${DEFAULT_OUTPUT_FILE}";
fi;

# Anyway, if there is no source file, it makes no sense to make the rest
# of the test.
if [ ! -f "${SOURCE_FILE}" ]; then
	echo "[47m[5m[31m>>>Fatal error: Cannot open source file \""${SOURCE_FILE}"\" \
for [32mGAEN[5m[31m server.[0m" >&2;
	exit 1;
fi;

# We'll do it the same way if gcc is a jerk or just does not exist.
$ECHO ">>>Checking if [1mgcc[0m compiler exists and works... "\\c;
echo "main(){}" > /tmp/gcctest.c;

GCC="";

GCC_ENUM=`whereis gcc | cut -d " " -f 2-; whereis cc | cut -d " " -f 2-`;

for i in $GCC_ENUM; do
  if [ "`echo $i | grep obsolete`" != "" -o \
			"`echo $i | grep old`" != "" ]; then
    continue;
  fi;
  ERR=`$i /tmp/gcctest.c -o/tmp/a.out 2>&1 1>/dev/null`;
  if [ "$ERR" = "" ]; then
    GCC="$i";
    echo "[1m[32mdone[0m."
    break;
  fi;
done;

if [ "$GCC" = "" ]; then
  echo "[1m[31mfailed.";
  echo "[47m[31m[5m>>>Fatal error: It seems like gcc doesn't work ! <:-([0m";
  exit 1;
fi;

# The crypt() functions (and other similar functions) are not
# included in libc (or glibc), but in a library named 'libcrypt' which
# contains all the stuff concerning the crypt() functions.
# This happens for glibc packages newer than 5.3, we think.
$ECHO ">>>Searching for [1m[36mcrypt()[0m function in [1m[36mcrypt[0m library... "\\c;

cat > /tmp/crypt.c <<EOF
#include <unistd.h> 
main (void) /* test file created by GAEN easy compile script */
{
  crypt (NULL, NULL);
  return 0;
}
EOF

ERR=`$GCC /tmp/crypt.c -lcrypt -o/tmp/a.out 2>&1 1>/dev/null | grep "crypt"`;
if [ "${ERR}" = "" ]; then
	LIBS="-lcrypt";
	echo "[1m[32mfound[0m.";
    else
	echo "[1m[31mnot found[0m.";
fi;


# For some versions of GNU GCC - those coming with Solaris, some of the 
# functions concerning the networking part are all included in a stand-alone
# library named 'libsocket'.
$ECHO ">>>Searching for [1mBSD[0m socket functions in [1m[36msocket[0m library... "\\c;

cat > /tmp/socket.c <<EOF
#include <sys/socket.h>
main (void) /* test file created by GAEN easy compile script */
{
  return socket (AF_INET, SOCK_STREAM, 0);
}
EOF

ERR=`$GCC /tmp/socket.c -lsocket -o/tmp/a.out 2>&1 1>/dev/null | grep "socket"`;
if [ "${ERR}" = "" ]; then
	LIBS="-lsocket "${LIBS}"";
	echo "[1m[32mfound[0m.";
    else
	echo "[1m[31mnot found[0m.";
fi;


# ...and the rest of them in 'libnsl'.
$ECHO ">>>Searching for other [1m[36msocket[0m functions in [1m[36mnsl[0m library... "\\c;

cat > /tmp/nsl.c <<EOF
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
main (void) /* test file created by GAEN easy compile script */
{
  return socket (AF_INET, SOCK_STREAM, 0);
}
EOF

ERR=`$GCC /tmp/nsl.c -lnsl -o/tmp/a.out 2>&1 1>/dev/null`;
if [ "${ERR}" = "" ]; then
	LIBS="-lnsl "${LIBS}"";
	echo "[1m[32mfound[0m.";
    else
	echo "[1m[31mnot found[0m.";
fi;


# Older/particular versions of GCC don't know how to optimize code when 
# we demand it or just usually... Maybe newer versions of GCC having this
# problem but we just didn't heared about any...
$ECHO ">>>Checking if [1mgcc[0m can generate optimized code... "\\c;

cat >/tmp/opt.c <<EOF
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
main (void) /* test file created by GAEN easy compile script */
{
  (void) fprintf(stderr, "We really have to write something here?:)\n");
  socket (AF_INET, SOCK_STREAM, 0);
  crypt (NULL, NULL);
  return 0;
}
EOF

ERR=`$GCC /tmp/opt.c $LIBS -O -o/tmp/a.out 2>&1`;
if [ "$ERR" = "" ]; then
	FLAGS="-O";
	echo "[1m[32mok[0m.";
	CAN_OPTIMIZE="1";
    else
	echo "[1m[34mcannot[0m.";
fi;

# We obtained the smallest, the fastest and the most stable code we've got 
# under various versions of Linux compiling with "-O2" ("-O3" option 
# makes the ELF bigger). We suppose (hope) that's common to most of UNIX/GCC
# versions. We're thinking about OS's libraries... but at least GCC generates
# the code, not the OS. So, this should be common to most of GCC versions.
if [ "$CAN_OPTIMIZE" = "1" ]; then
	$ECHO ">>>Forcing [1mgcc[0m to a higher optimization level... "\\c;
	ERR=`$GCC /tmp/opt.c $LIBS -O2 -o/tmp/a.out 2>&1 1>/dev/null | grep "O"`;
	if [ "${ERR}" = "" ]; then
		FLAGS=""${FLAGS}"2";
		echo "[1m[32msuccess[0m.";
    	    else
		echo "[1m[34mcannot[0m.";
	fi;
fi;

# We just want to be sure the file does not exist. It may happen to exist
# sometimes... if you interrupted this script before it finishes its work.
# In this case, if the name of the lock file is the same or nor depends on
# the way the OS generates random numbers (via shell)... or if it really
# does this... Anyway, it's more safe if we removed it before.
rm -f "${LockFile}" 2>/dev/null 1>/dev/null;

# Here it will be generated a script based on the information already 
# obtained. The idea is to run it in a subshell, so it'll be easier to
# supervise.

cat >"${GAENCompillingScript}" <<EOF
#!/bin/sh
$GCC $SOURCE_FILE -o $OUTPUT_FILE $FLAGS $LIBS $NO_SUPPORT 2>>$ErrFile\\
      && strip --strip-all $OUTPUT_FILE 2>>$ErrFile
touch $LockFile 2>>$ErrFile
EOF

$ECHO ">>>Compiling [32mGAEN[0m server \"[1m"${OUTPUT_FILE}"[0m\" from file \"[1m"${SOURCE_FILE}"[0m\"..."\\c;

# Running the script in background...
. ${GAENCompillingScript} &

# Some poor visual effects using the lock file...
Time="-1";
dash="";
printf "%s" "[1m[34m";
while [ ! -f "${LockFile}" ]; do
	Time=`expr $Time + 1`;
	dash=`expr $Time % 4`;
	case "$dash" in
	  "0")
		printf "%s\b" "-";
		;;
	  "1")
		printf "%s\b" "\\";
		;;
	  "2")
		printf "%s\b" "|";
		;;
	  "3")
		printf "%s\b" "/";
		;;
	esac;
	sleep 1;
done;

# Showing some info about the compiling time and about any errors...
# which may be occured.
if [ -s "${ErrFile}" ]; then
	$ECHO \\b"[1m[31mfailed.";
	echo "[0m>>>[1m[34mDone in "$Time" seconds with [1m[31mthe following error(s):";
        echo "[33m********************************************************[31m";
	less "${ErrFile}" 1>&2;
	echo "[33m********************************************************";
    else
	$ECHO \\b"[1m[34mdone.";
	echo "[0m>>>[1m[34mDone in "$Time" seconds with [1m[32mno error.";
fi;
$ECHO \\b"[0m";

# Chatting is beautiful !:) That's COOL !!!:)))
