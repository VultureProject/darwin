#!/bin/sh

RULES_PATH="/home/darwin/conf/fcontent_inspection/rules-default"
SED_RULES_PATH=$(/bin/echo $RULES_PATH | /usr/bin/sed "s/\\//\\\\\//g")
/bin/cd /home/darwin/conf/fcontent_inspection/

/bin/echo "updating Darwin's default yara rules..."
/usr/local/bin/wget https://github.com/Yara-Rules/rules/archive/master.zip -O yara_rules.zip
/usr/bin/unzip yara_rules.zip > /dev/null
/bin/rm -rf ./rules-default
/bin/mv -f rules-master/ rules-default/
/bin/cat /dev/null > ./VultureDefaultRules.yar
/bin/cat rules-default/CVE_Rules_index.yar | /usr/bin/sed "s/\"./\"$SED_RULES_PATH/g" >> ./VultureDefaultRules.yar
/bin/cat rules-default/Exploit-Kits_index.yar | /usr/bin/sed "s/\"./\"$SED_RULES_PATH/g" >> ./VultureDefaultRules.yar
/bin/cat rules-default/Webshells_index.yar | /usr/bin/sed "s/\"./\"$SED_RULES_PATH/g" >> ./VultureDefaultRules.yar
/bin/cat rules-default/malware_index.yar | /usr/bin/sed "s/\"./\"$SED_RULES_PATH/g" >> ./VultureDefaultRules.yar
/bin/echo "include \"$RULES_PATH/malware/MALW_Eicar\"" >> ./VultureDefaultRules.yar

/bin/rm -rf ./rules-master
/bin/rm yara_rules.zip

/bin/echo "Done."
