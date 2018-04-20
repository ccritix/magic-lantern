#!/bin/bash
workingDir=`dirname "$0"`
cd "${workingDir}"

#changes size of terminal window
#tip from here http://apple.stackexchange.com/questions/33736/can-a-terminal-window-be-resized-with-a-terminal-command
#Will move terminal window to the left corner

printf '\e[8;24;95t'
printf '\e[3;410;100t'

open -a Terminal

bold="$(tput bold)"
normal="$(tput sgr0)"
red="$(tput setaf 1)"
reset="$(tput sgr0)"
green="$(tput setaf 2)"
underline="$(tput smul)"
standout="$(tput smso)"
normal="$(tput sgr0)"
black="$(tput setaf 0)"
red="$(tput setaf 1)"
green="$(tput setaf 2)"
yellow="$(tput setaf 3)"
blue="$(tput setaf 4)"
magenta="$(tput setaf 5)"
cyan="$(tput setaf 6)"
white="$(tput setaf 7)"

#get rid of not needed file
    if ls .DS_store >/dev/null 2>&1;
    then 
    rm .DS_store
    fi

clear
#check for dependencies:
if ! [ -f "`which hg`" ]
then 
clear
echo $(tput bold)"
Checking for mercurial, please wait..."
sleep 2
read -p $(tput bold)"
mercurial(hg) is not installed would you like to install it?
(this action might install homebrew as well)$(tput setaf 1)

Y/N?"$(tput sgr0) -n 1 -r
if [[ $REPLY =~ ^[Yy]$ ]]
then
clear
echo "Follow instructions in terminal window"
sleep 2
[ ! -f "`which brew`" ] && /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
brew install mercurial
fi
fi
#Did we run hgrc changes?:
    dir=$PWD

    if ! [ -f "$dir"/.hg/hgrc_orig ]
    then
    cp "$dir"/.hg/hgrc "$dir"/.hg/hgrc_orig
    fi

#what´s added and not
    password="$(grep '@bitbucket.org' "$dir"/.hg/hgrc | cut -d ":" -f3 | cut -d "@" -f1 | head -1)"
    username="$(grep 'username' "$dir"/.hg/hgrc)"

#Check for password
    if [ x"$password" = x ]
    then 
    password=$red$(echo "MISSING")
    else
    password="$bold$blue$(echo "password added$(tput sgr0)(not shown)")"
    fi

#Check for username
    if [ x"$username" = x ]
    then 
    username=$red$(echo "MISSING")
    else
    if grep 'username = Jane Doe' "$dir"/.hg/hgrc
    then
    username=$red$(echo "MISSING")
    else
    username="$bold$blue$(grep 'username' "$dir"/.hg/hgrc)"
    fi
    fi

#repository name. Expecting this to cause problems in different repos. Needs testing
repname="$(grep 'bitbucket.org' "$dir"/.hg/hgrc | cut -d "." -f2 | cut -d "/" -f3 | head -1)"

clear
while :
do 
    cat<<EOF
    $(tput sgr0)====================			   
    ${bold}$(tput setaf 1)hg automation script$(tput sgr0)
    --------------------
 
    $(tput bold)(01) add password, username and email to your hgrc file 
    $(tput bold)(RE) reset hgrc to default settings$(tput sgr0)
    $(tput bold)(c)  pull, update, commit$(tput sgr0)(skips push and dmg creation)
    $(tput bold)(p)  pull, update, commit, push$(tput sgr0)(skips dmg creation)
    $(tput bold)(dm) create only the $repname.dmg file$(tput sgr0)
    $(tput bold)(du) create $repname.dmg and upload to downloads section$(tput sgr0)
    $(tput bold)(st) show changed files$(tput sgr0)
    $(tput bold)(s)  pull and update from your $repname repo$(tput sgr0)(fork,branch, main)
    $(tput bold)(m)  pull and update from main $repname repo$(tput sgr0)(will seek main repository if exists)
    $(tput bold)$(tput setaf 2)(r)  pull, update, commit, addremove, push and $repname.dmg upload$(tput sgr0)
    $(tput bold)$(tput setaf 1)(q)  exit from this menu$(tput sgr0)

    	 $(tput sgr0)$(tput smul)Your password:$(tput sgr0)$(tput setaf 2) $password$(tput sgr0)
    $(tput sgr0)$(tput smul)Username and email:$(tput sgr0)$(tput setaf 2) $username$(tput sgr0)


Please enter your selection number below:
EOF
    read -n2
    case "$REPLY" in

    "01") 
#if rerun on file already set 
    if ! [ "$username" = $(tput setaf 1)MISSING ]
    then 
clear
   echo $(tput bold)$(tput setaf 1)"Please reset your hgrc file before proceeding"
   sleep 2
   else

#Add this to get password:

clear
   echo $(tput bold)"Write your bitbucket password:$(tput sgr0) then press enter"
   read password

clear
   echo $(tput bold)"Write your username:$(tput sgr0) then press enter"
   read user

clear
   echo $(tput bold)"Write your email adress:$(tput sgr0) then press enter"
   read email

#password replacement:

#check for @ sign
   if ! grep '@' <<< $(cat "$dir"/.hg/hgrc | grep 'bitbucket.org' | head -1)
   then 
   end="$(grep 'bitbucket.org' "$dir"/.hg/hgrc | cut -d "/" -f3,4,5 | head -1)"
   begin=$(grep 'bitbucket.org' "$dir"/.hg/hgrc | cut -d ":" -f1 | head -1)://$(grep 'bitbucket.org' "$dir"/.hg/hgrc | head -1 | rev | cut -d "/" -f2 | rev)
   pass="$(echo :"$password"@)"
   else
   end="$(grep 'bitbucket.org' "$dir"/.hg/hgrc | cut -d "@" -f2 | head -1)"
   begin="$(grep 'bitbucket.org' "$dir"/.hg/hgrc | cut -d "@" -f1 | head -1)"
   pass="$(echo :"$password"@)"
   fi

#Check for password
    if [ x"$pass" = x ]
    then 
    clear
    echo "Password is missing and upload automation won´t work"
    sleep 2
    exit 0
    fi

#Replace bitbucket hgrc info
    sed -i".bak" "$(grep -n 'bitbucket.org' "$dir"/.hg/hgrc | cut -d ":" -f1 | head -1)"d "$dir"/.hg/hgrc
    sed -i".bak1" "$(grep -n '\[paths\]' "$dir"/.hg/hgrc | cut -d ":" -f1)"d "$dir"/.hg/hgrc
    sed -i".bak2" "$(grep -n '# username' "$dir"/.hg/hgrc | cut -d ":" -f1)"d "$dir"/.hg/hgrc
    sed -i".bak3" "$(grep -n '\[ui\]' "$dir"/.hg/hgrc | cut -d ":" -f1)"d "$dir"/.hg/hgrc

#path with password
    echo "[paths]" >> "$dir"/.hg/hgrc
    echo "$begin""$pass""$end" >> "$dir"/.hg/hgrc

#remove bak files
    rm "$dir"/.hg/hgrc.bak
    rm "$dir"/.hg/hgrc.bak1
    rm "$dir"/.hg/hgrc.bak2
    rm "$dir"/.hg/hgrc.bak3

#write username and email
    echo "[ui]" >> "$dir"/.hg/hgrc
    echo "username = "$user" <"$email">" >> "$dir"/.hg/hgrc
fi

#what´s added and not
    password="$(grep '@bitbucket.org' "$dir"/.hg/hgrc | cut -d ":" -f3 | cut -d "@" -f1 | head -1)"
    username="$(grep 'username' "$dir"/.hg/hgrc)"

#Check for password
    if [ x"$password" = x ]
    then 
    password=$red$(echo "MISSING")
    else
    password="$bold$blue$(echo "password added$(tput sgr0)(not shown)")"
    fi

#Check for username
    if [ x"$username" = x ]
    then 
    username=$red$(echo "MISSING")
    else
    if grep 'username = Jane Doe' "$dir"/.hg/hgrc
    then
    username=$red$(echo "MISSING")
    else
    username="$bold$blue$(grep 'username' "$dir"/.hg/hgrc)"
    fi
    fi
clear
;;

   "RE")  
mv "$dir"/.hg/hgrc_orig "$dir"/.hg/hgrc
rm "$dir"/.hg/hgrc_orig
clear
echo $(tput bold)"

$(tput sgr0)$(tput bold)$(tput setaf 1) 
hgrc file is now reset"$(tput sgr0) ; sleep 2

#what´s added and not
    password="$(grep '@bitbucket.org' "$dir"/.hg/hgrc | cut -d ":" -f3 | cut -d "@" -f1 | head -1)"
    username="$(grep 'username' "$dir"/.hg/hgrc)"

#Check for password
    if [ x"$password" = x ]
    then 
    password=$red$(echo "MISSING")
    else
    password="$bold$blue$(echo "password added$(tput sgr0)(not shown)")"
    fi

#Check for username
    if [ x"$username" = x ]
    then 
    username=$red$(echo "MISSING")
    else
    if grep 'username = Jane Doe' "$dir"/.hg/hgrc
    then
    username=$red$(echo "MISSING")
    else
    username="$bold$blue$(grep 'username' "$dir"/.hg/hgrc)"
    fi
    fi
clear
;;

   "c")  
  cd "$dir"/
clear
#commit and push in one swoop
#if username is missing 
    if [ "$username" = $(tput setaf 1)MISSING ]
    then 
clear
   echo $(tput bold)$(tput setaf 1)"Please add password, username and email before proceeding"
   sleep 2
   cd "$dir"/
clear
   else
   cd "$dir"/
clear
   echo $(tput bold)"Write a commit message$(tput sgr0) then press enter"
   read commit
#get rid of not needed file
    if ls .DS_store >/dev/null 2>&1;
    then 
    rm .DS_store
    fi
   hg pull
   hg update
   hg commit -m "$(echo $commit)"
clear
    fi
;;

  "p")  
  cd "$dir"/
clear
#commit and push in one swoop
#if username is missing 
    if [ "$username" = $(tput setaf 1)MISSING ]
    then 
clear
   echo $(tput bold)$(tput setaf 1)"Please add password, username and email before proceeding"
   sleep 2
  cd "$dir"/
clear
   else
   cd "$dir"/
clear
   echo $(tput bold)"Write a commit message$(tput sgr0) then press enter"
   read commit
#get rid of not needed file
    if ls .DS_store >/dev/null 2>&1;
    then 
    rm .DS_store
    fi
   hg pull
   hg update
   hg addremove
   hg commit -m "$(echo $commit)"
   hg push
clear
    fi
;;

   "dm")  
#let´s build the dmg file
  cd "$dir"/
clear

#Script originally for MLVFS
#https://bitbucket.org/dmilligan/mlvfs/src/9f8191808407bb49112b9ab14c27053ae5022749/build_installer.sh?at=master&fileviewer=file-view-default
# A lot of this script came from here:
# http://stackoverflow.com/questions/96882/how-do-i-create-a-nice-looking-dmg-for-mac-os-x-using-command-line-tools
source="install_temp"
title="$repname"
finalDMGName="$repname.dmg"
size=$(du -ks | cut -d '.' -f1 | cut -d '	' -f1)

mkdir "${source}"
cp -R `ls | grep -v 'hg.command\|README.md\|install_temp'` "${source}"

#remove any previously existing build
rm -f "${finalDMGName}"

hdiutil create -srcfolder "${source}" -volname "${title}" -fs HFS+ -fsargs "-c c=64,a=16,e=16" -format UDRW -size ${size}k pack.temp.dmg
device=$(hdiutil attach -readwrite -noverify -noautoopen "pack.temp.dmg" | egrep '^/dev/' | sed 1q | awk '{print $1}')
sleep 2
chmod -Rf go-w /Volumes/"${title}"
sync
sync
hdiutil detach ${device}
hdiutil convert "pack.temp.dmg" -format UDZO -imagekey zlib-level=9 -o "${finalDMGName}"
rm -f pack.temp.dmg
rm -R "${source}"

#back to start
    cd "$dir"/
clear
;;

   "du")  
#let´s build the dmg file
  cd "$dir"/
clear

#Script originally for MLVFS
#https://bitbucket.org/dmilligan/mlvfs/src/9f8191808407bb49112b9ab14c27053ae5022749/build_installer.sh?at=master&fileviewer=file-view-default
# A lot of this script came from here:
# http://stackoverflow.com/questions/96882/how-do-i-create-a-nice-looking-dmg-for-mac-os-x-using-command-line-tools
source="install_temp"
title="$repname"
finalDMGName="$repname.dmg"
size=$(du -ks | cut -d '.' -f1 | cut -d '	' -f1)

mkdir "${source}"
cp -R `ls | grep -v 'hg.command\|README.md\|install_temp'` "${source}"

#remove any previously existing build
rm -f "${finalDMGName}"

hdiutil create -srcfolder "${source}" -volname "${title}" -fs HFS+ -fsargs "-c c=64,a=16,e=16" -format UDRW -size ${size}k pack.temp.dmg
device=$(hdiutil attach -readwrite -noverify -noautoopen "pack.temp.dmg" | egrep '^/dev/' | sed 1q | awk '{print $1}')
sleep 2
chmod -Rf go-w /Volumes/"${title}"
sync
sync
hdiutil detach ${device}
hdiutil convert "pack.temp.dmg" -format UDZO -imagekey zlib-level=9 -o "${finalDMGName}"
rm -f pack.temp.dmg
rm -R "${source}"

clear

#create the upload automation script
    user="#$(grep 'bitbucket.org' "$dir"/.hg/hgrc | cut -d "." -f2 | cut -d "/" -f2 | head -1)"
    password="$(grep '@bitbucket.org' "$dir"/.hg/hgrc | cut -d ":" -f3 | cut -d "@" -f1 | head -1)"
    downloads="/"$(grep 'bitbucket.org' "$dir"/.hg/hgrc | cut -d "." -f2 | cut -d "/" -f2,3 | head -1)"/downloads"
    item="$(echo " "$dir"/$repname.dmg")"

#print password to external upload script
    echo "$user"" $password"" $downloads""$item" > ~/dmg_upload

#external upload script. Will be placed in repo folder
cat <<'EOF' >> ~/dmg_upload

#!/bin/bash
    usr=$1; pwd=$2; pge=$3; fil=$4

    echo "actual upload progress should appear right now as a progress bar, be patient:"
    curl --progress-bar           `# print the progress visually                                                                          ` \
         --location               `# follow redirects if we are told so                                                                 ` \
         --fail                   `# ensure that we are not succeeding when the server replies okay but with an error code             ` \
         --user "$usr:$pwd"       `# basic auth so that it lets us in                                                                ` \
         --form files=@"$fil" "https://api.bitbucket.org/2.0/repositories/${pge#/}" 1> ~/tmp1 #
    rm ~/tmp1 

EOF
    
#run the upload automation script
. ~/dmg_upload $(cat ~/dmg_upload | head -1 | tr -d '#')
rm ~/dmg_upload
rm "$dir"/$repname.dmg

#back to start
    cd "$dir"/
clear
;;


   "st")  
   cd "$dir"/
clear
echo "Following files changed:
"
   hg status
echo ""  
   echo $(tput bold)"Hit enter when done checking status"
   read anything
clear
;;


   "s")  
#pull from source
   cd "$dir"/
clear
#get rid of not needed file
    if ls .DS_store >/dev/null 2>&1;
    then 
    rm .DS_store
    fi
   hg pull 
   hg update
clear
;;

   "m")  
#pull from main source if exists
   cd "$dir"/
clear
#get rid of not needed file
    if ls .DS_store >/dev/null 2>&1;
    then 
    rm .DS_store
    fi
#if username is missing 
    if [ "$username" = $(tput setaf 1)MISSING ]
    then 
clear
   echo $(tput bold)$(tput setaf 1)"Please add password, username and email before proceeding"
   sleep 2
   cd "$dir"/
   else
   main="$(grep 'bitbucket.org' .hg/hgrc | grep -v '@bitbucket.org' | cut -d "=" -f2 | cut -d " " -f2)"
   if [ x"$main" = x ]
   then
clear
   echo $(tput bold)$(tput setaf 1)"Seems main repo is missing"
   sleep 2
   cd "$dir"/
   else
   hg pull "$main"
   hg update
   fi
   fi
clear
;;


   "q")  
osascript -e 'tell application "Terminal" to close first window' & exit
;;

   "r")  
   cd "$dir"/
clear
#commit, push and upload in one swoop
#if username is missing 
    if [ "$username" = $(tput setaf 1)MISSING ]
    then 
clear
   echo $(tput bold)$(tput setaf 1)"Please add password, username and email before proceeding"
   sleep 2
clear
   else
   cd "$dir"/
clear
   echo $(tput bold)"Write a commit message$(tput sgr0) then press enter"
   read commit
#get rid of not needed file
    if ls .DS_store >/dev/null 2>&1;
    then 
    rm .DS_store
    fi
   hg pull
   hg update
   hg addremove
   hg commit -m "$(echo $commit)"
   hg push

#let´s build the dmg file
#Script originally for MLVFS
#https://bitbucket.org/dmilligan/mlvfs/src/9f8191808407bb49112b9ab14c27053ae5022749/build_installer.sh?at=master&fileviewer=file-view-default
# A lot of this script came from here:
# http://stackoverflow.com/questions/96882/how-do-i-create-a-nice-looking-dmg-for-mac-os-x-using-command-line-tools
source="install_temp"
title="$repname"
finalDMGName="$repname.dmg"
size=$(du -ks | cut -d '.' -f1 | cut -d '	' -f1)

mkdir "${source}"
cp -R `ls | grep -v 'hg.command\|README.md\|install_temp'` "${source}"

#remove any previously existing build
rm -f "${finalDMGName}"

hdiutil create -srcfolder "${source}" -volname "${title}" -fs HFS+ -fsargs "-c c=64,a=16,e=16" -format UDRW -size ${size}k pack.temp.dmg
device=$(hdiutil attach -readwrite -noverify -noautoopen "pack.temp.dmg" | egrep '^/dev/' | sed 1q | awk '{print $1}')
sleep 2
chmod -Rf go-w /Volumes/"${title}"
sync
sync
hdiutil detach ${device}
hdiutil convert "pack.temp.dmg" -format UDZO -imagekey zlib-level=9 -o "${finalDMGName}"
rm -f pack.temp.dmg
rm -R "${source}"

clear

#create the upload automation script
    user="#$(grep 'bitbucket.org' "$dir"/.hg/hgrc | cut -d "." -f2 | cut -d "/" -f2 | head -1)"
    password="$(grep '@bitbucket.org' "$dir"/.hg/hgrc | cut -d ":" -f3 | cut -d "@" -f1 | head -1)"
    downloads="/"$(grep 'bitbucket.org' "$dir"/.hg/hgrc | cut -d "." -f2 | cut -d "/" -f2,3 | head -1)"/downloads"
    item="$(echo " "$dir"/$repname.dmg")"

#print password to external upload script
    echo "$user"" $password"" $downloads""$item" > ~/dmg_upload

#external upload script. Will be placed in repo folder
cat <<'EOF' >> ~/dmg_upload

#!/bin/bash
    usr=$1; pwd=$2; pge=$3; fil=$4

    echo "actual upload progress should appear right now as a progress bar, be patient:"
    curl --progress-bar           `# print the progress visually                                                                          ` \
         --location               `# follow redirects if we are told so                                                                 ` \
         --fail                   `# ensure that we are not succeeding when the server replies okay but with an error code             ` \
         --user "$usr:$pwd"       `# basic auth so that it lets us in                                                                ` \
         --form files=@"$fil" "https://api.bitbucket.org/2.0/repositories/${pge#/}" 1> ~/tmp1 #
    rm ~/tmp1 

EOF
    
#run the upload automation script
. ~/dmg_upload $(cat ~/dmg_upload | head -1 | tr -d '#')
rm ~/dmg_upload
rm "$dir"/$repname.dmg

#back to start
    cd "$dir"/
clear
    fi
;;

    "Q")  echo "case sensitive!!"   ;;
     * )  echo "invalid option"     ;;
    esac
    sleep 0.5
done






