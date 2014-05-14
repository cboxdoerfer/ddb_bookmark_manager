#!/bin/sh
/bin/mkdir -pv ${HOME}/.local/lib/deadbeef

if [ -f ./ddb_misc_bookmark_manager.so ]; then
	/usr/bin/install -v -c -m 644 ./ddb_misc_bookmark_manager.so ${HOME}/.local/lib/deadbeef/
fi

CHECK_PATHS="/usr/local/lib/deadbeef /usr/lib/deadbeef"
for path in $CHECK_PATHS; do
	if [ -d $path ]; then
		if [ -f $path/ddb_misc_bookmark_manager.so ]; then
			echo "Warning: Some version of the bookmark manager plugin is present in $path, you should remove it to avoid conflicts!"
		fi
	fi
done
