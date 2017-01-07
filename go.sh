#!/bin/sh

# Loader file ("go" version 1.2) for GAEN M (18.2) talker server
# Written by Sabin-Corneliu Buraga <busaco@infoiasi.ro> (c)2000-2002
# GAEN Web page: http://www.infoiasi.ro/~busaco/gaen/
# Last update: 18 May 2002

# Syntax: ./go [ -n | -a | -c ]
# Switches: 
#	-n no directories/files checkings
# 	-a no accounts checkings
#	-c no various config parameters checkings

VERSION="1.2"
echo ">>>GAEN loader $VERSION - command line: $0 $@"

# skipping different tests?
if [ $# -ne 1 ]
then

if [ "$1" != "-n" ]
then  


# check directories
echo ">>>Checking GAEN directories..."
for DIRS in logfiles pictfiles prayfiles mailspool helpfiles hintfiles \
             datafiles userfiles msgsfiles miscfiles conffiles killfiles  \
             dictfiles quotefiles;
do
  if [ ! -d $DIRS ]
  then
    echo "   Creating $DIRS/ directory..."
    mkdir $DIRS
  else
    echo "   Checking if $DIRS exists... ok."
  fi
done
 
# check different files
if [ ! -f datafiles/.swear ]
then
  echo ">>>Warning: Swears file 'datafiles/.swear' not found."
  echo "   See '.help swears' for details after connecting to GAEN."
fi
if [ ! -f datafiles/.commands ]
then
  echo ">>>Warning: Level commands file 'datafiles/.commands' not found."
  echo "   See '.help scommands' for details after connecting to GAEN."
fi
if ls datafiles/mapfile* >/dev/null 2>/dev/null
then
  echo ">>>Checking map files... ok."
else
  echo ">>>Warning: Map files 'datafiles/mapfile*' not found."
  echo "   Users may not able to correctly use .map command."
fi
if ls datafiles/?_alert >/dev/null 2>/dev/null
then
  echo ">>>Checking alert files... ok."
else
  echo ">>>Warning: Alert files 'datafiles/?_alert' not found."
  echo "   Users may not able to correctly use .alert command."
fi

# end of checking of directories/files
fi

# check accounts
if [ "$1" != "-a" ]
then  

if ls userfiles/*.D >/dev/null 2>/dev/null
then
  echo ">>>Checking GAEN accounts files... ok."
else 
  echo ">>>Warning: Apparently there are no GAEN accounts."
  echo "   Please, create an administration account by using 'gaend -b' option."
fi

# end of checking accounts
fi 

# check various config parameters
if [ "$1" != "-c" ]
then  

CONFIG="conffiles/config"

QUOTEF="quotefiles/"

QUOTES=`grep max_quotes $CONFIG | cut -c12- | cut -f1 -d"#"`

QUOTES_FILES=`ls $QUOTEF/quote* | wc -l`
 
if [ $QUOTES -eq $QUOTES_FILES ]
then
  echo ">>>Checking max_quotes config parameter... ok."
else
  echo ">>>Warning: max_quotes parameter value is not equal to number of quote files in $QUOTEF."
  echo "   Please, adjust max_quotes parameter in $CONFIG file to $QUOTES_FILES."
fi

HINTF=hintfiles/

HINTS=`grep max_hints $CONFIG | cut -c10- | cut -f1 -d"#"`

HINTS_FILES=`ls $HINTF/hint* | wc -l`
 
if [ $HINTS -eq $HINTS_FILES ]
then
  echo ">>>Checking max_hints config parameter... ok."
else
  echo ">>>Warning: max_hints parameter value is not equal to number of hint files in $QUOTEF."
  echo "   Please, adjust max_hints parameter in $CONFIG file to $HINTS_FILES."
fi

# end of checking config parameters
fi

fi 

# save old log-files
cd logfiles/
echo ">>>Creating backup logfiles..."
for FILES in main com io link err note swear unlink;
do
  echo "   Renaming syslog.$FILES as syslog.$FILES.bak." 
  mv syslog.$FILES syslog.$FILES.bak 2>/dev/null >/dev/null
done

cd ..

# start GAEN server (high priority)
echo ">>>Preparing 'gaend' GAEN server to boot..."

if [ ! -x gaend ] 
then
  echo ">>>Error: GAEN server executable not found."
  echo "   Please, compile GAEN sources first by using 'easy.compile' script."
  exit
fi

nice -n0 ./gaend 

# check for errors...
if [ $? -ne 0 ]
then
  echo ""
  echo ">>>There are some GAEN boot errors."
  echo "   Please, see logfiles/syslog.* for details..."
fi
