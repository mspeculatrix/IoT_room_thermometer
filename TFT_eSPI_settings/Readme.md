Before you do anything else, install the TFT_eSPI library from the library manager.

If you already have the library installed, make sure you've updated to the latest version _before_ you use these files.

The files here need to be copied to the relevant locations in the TFT_eSPI folder in your Arduino libraries folder. The folder will be at:

    Arduino
      +-- libraries
              +-- TFT_eSPI

For example, you'll need to copy the files in the Fonts/Custom/ folder here to:

    ..Arduino/libraries/TFT_eSPI/Fonts/Custom/

In some cases, the files here will overwrite the library's default files (you might want to rename the originals first, so that you have them as a backup.)

**Do NOT simply replace the TFT_eSPI folder with this folder.** This folder contains only the modified or additional files, not the main library files.

If you update the TFT_eSPI library, you will lose these files. Keep copies of them in a safe place so that you can re-copy them over after each update.
