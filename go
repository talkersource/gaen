#!/bin/sh

# Loader file ("go" version 1.0) for GAEN M (18) talker server
# Written by Sabin-Corneliu Buraga <busaco@infoiasi.ro>
# GAEN Web page: http://www.infoiasi.ro/~busaco/gaen/
# Last update: 19 November 2001

# Syntax: ./go [ -n ]
# Switches: -n no directories/files checkings

VERSION=1.0
echo ">>>GAEN loader $VERSION - command line: $0 $@"

# skipping different tests?
if [ $# -ne 1 ]
then
if [ "$1" != "-n" ]
then  

# check directories
if [ ! -d logfiles/ ]
then
  echo ">>>Creating logfiles/ directory..."
  mkdir logfiles
fi
if [ ! -d pictfiles/ ]
then
  echo ">>>Creating pictfiles/ directory..."
  mkdir pictfiles
fi
if [ ! -d prayfiles/ ]
then
  echo ">>>Creating prayfiles/ directory..."
  mkdir prayfiles
fi
if [ ! -d mailspool/ ]
then
  echo ">>>Creating mailspool/ directory..."
  mkdir mailspool
fi
if [ ! -d helpfiles/ ]
then
  echo ">>>Creating helpfiles/ directory..."
  mkdir helpfiles
fi
if [ ! -d hintfiles/ ]
then
  echo ">>>Creating hintfiles/ directory..."
  mkdir hintfiles
fi
if [ ! -d datafiles/ ]
then
  echo ">>>Creating datafiles/ directory..."
  mkdir datafiles
fi
if [ ! -d userfiles/ ]
then
  echo ">>>Creating datafiles/ directory..."
  mkdir userfiles
fi
if [ ! -d msgsfiles/ ]
then
  echo ">>>Creating msgsfiles/ directory..."
  mkdir msgsfiles
fi
if [ ! -d miscfiles/ ]
then
  echo ">>>Creating miscfiles/ directory..."
  mkdir miscfiles
fi
if [ ! -d conffiles/ ]
then
  echo ">>>Creating conffiles/ directory..."
  mkdir conffiles
fi
if [ ! -d killfiles/ ]
then
  echo ">>>Creating killfiles/ directory..."
  mkdir killfiles
fi
if [ ! -d dictfiles/ ]
then
  echo ">>>Creating dictfiles/ directory..."
  mkdir dictfiles
fi
if [ ! -d quotefiles/ ]
then
  echo ">>>Creating quotefiles/ directory..."
  mkdir quotefiles
fi

# check different files
if [ ! -f datafiles/.swear ]
then
  echo ">>>Warning: Swears file .swear not found."
  echo "   See '.help swears' for details after connecting to GAEN."
fi
if [ ! -f datafiles/.commands ]
then
  echo ">>>Warning: Level commands file .commands not found."
  echo "   See '.help scommands' for details after connecting to GAEN."
fi
if ls userfiles/*.D >/dev/null 2>/dev/null
then
  echo ">>>Checking if there are GAEN accounts... ok."
else 
  echo ">>>Warning: Apparently there are no GAEN accounts."
  echo "   Please, create an administration account by using ``gaend -b'' option."
fi

# end of checking
fi
fi

# save old log-files
cd logfiles/
echo ">>>Creating backup logfiles..."
mv syslog.main syslog.main.bak 2>/dev/null >/dev/null
mv syslog.com syslog.com.bak 2>/dev/null >/dev/null
mv syslog.io syslog.io.bak 2>/dev/null >/dev/null
mv syslog.link syslog.link.bak 2>/dev/null >/dev/null 
mv syslog.err syslog.err.bak 2>/dev/null >/dev/null
mv syslog.note syslog.note.bak 2>/dev/null >/dev/null
mv syslog.swear syslog.swear.bak 2>/dev/null >/dev/null
mv syslog.unlink syslog.unlink.bak 2>/dev/null >/dev/null
cd ..

# start GAEN server (high priority)
echo ">>>Starting..."
nice -n0 ./gaend 

# check for errors...
if [ $? -ne 0 ]
then
  echo ">>>There are some errors at booting GAEN server."
  echo ">>>Please, see log files for details..."
fi
