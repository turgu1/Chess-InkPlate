#!/bin/sh
#
# This script is used to generate a new release
#
# Guy Turcotte, January 2021
#

if [ "$1" = "" ]
then
  echo "Usage: $0 version type"
  echo "type = 6, 10, 6plus"
  return 1
fi

folder="release-$1-inkplate_$2"

if [ -f "$folder.zip" ]
then
  echo "File $folder.zip already exist!"
  return 1
fi

mkdir "$folder"

cp .pio/build/inkplate_$2_release/bootloader.bin bin
cp .pio/build/inkplate_$2_release/partitions.bin bin
cp .pio/build/inkplate_$2_release/firmware.bin bin
cp -r bin "$folder"

cp -r SDCard "$folder"

if [ -f "$folder/SDCard/current_game.save" ]
then
  rm "$folder/SDCard/current_game.save"
fi

if [ -f "$folder/SDCard/config.txt" ]
then
  rm $folder/SDCard/config.txt
fi

cd doc
./gener.sh
cd ..

cp "doc/USER GUIDE.pdf" "$folder"
cp "doc/INSTALL.pdf" "$folder"

zip -r "$folder.zip" "$folder"

rm -rf "$folder"

echo "Completed."
