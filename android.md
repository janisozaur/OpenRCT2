# Android support
OpenRCT2 has support for Android, but it does require some additional steps of manual setup. This guide aims to help you set it up.

## Important to know
* The Android version is exactly the same as the "desktop" versions and has most of the same new features.
* Setup can be a bit challenging and finicky due to the differences between Android phones.
* User interface is not optimized for phones.
* Controls are not optimized for phones. (Currently: single tap is left-click, double tap is right-click)
* Plugins and multiplayer is only supported since v0.4.10 (#21631).

**If you have experience with Android development and are interested in helping out, it would be highly appreciated!**

## Requirements
For running OpenRCT2, the following things need to be present on the phone:
* RCT2 assets;
* OpenRCT2 data files;
* Installed OpenRCT2 app.

As of writing, the minimum required Android version is Android 7.0 ([SDK 24](https://github.com/OpenRCT2/OpenRCT2/blob/develop/src/openrct2-android/app/build.gradle#L21)).

### RCT2 assets
The Android version (just like other versions) requires assets from the original RCT2 game to work. This means you need to copy the RCT2 installation folder to your phone.

There a few ways to get these files:
* a) Install RCT2 on a Windows computer and navigate to the installation folder.
* b) If you own RCT on either Steam or GOG, but do not have access to a Windows computer: there's a few ways to get the files, like on MacOS described [here](https://docs.openrct2.io/en/latest/installing/installing-on-macos.html#from-a-gog-com-offline-installer) and the steps for Linux are described [here](https://docs.openrct2.io/en/latest/installing/installing-on-linux-bsd.html#getting-the-rct2-files-required).

The following folders from RCT2 are required: _'Data', 'ObjData'_

The following folders are optional: _'Landscapes', 'Saved Games', 'Scenarios', 'Tracks'_

The other folders and files are not used by OpenRCT2.

#### Steps
1. Get hold of the RCT2 asset folders from the game's installation folder as described above.
2. On your Android phone's main storage, create a folder called 'rct2'.
3. Copy the RCT2 folders you need into this new 'rct2' directory.
   - Tip: [how to transfer files between your computer & Android device](https://support.google.com/android/answer/9064445?hl=en)

**Result:** your 'rct2' folder should look like the first and third screenshots in the reference image below.

### OpenRCT2 data files
The Android version currently does not ship with the OpenRCT2 data files. You need to get them from a desktop version of OpenRCT2 and copy these files to your phone manually, like with the RCT2 assets.

**Note: these files need to be from the same version as the OpenRCT2 Android app.**

There are a few ways to get these files:
* a) Get them from an already installed OpenRCT2 on a desktop computer, which is the same version as the Android app you are planning to download.
   * If you use the OpenRCT2 Launcher on Windows, the data file folder is `Documents/OpenRCT2/bin/data`.
* b) Download a Windows portable ZIP with the correct version from the [website](https://openrct2.org/downloads). 
   * Click "View all versions" for the variant that matches your APK (release or develop), scroll down to "Other artifacts". 
   * Once downloaded, the data files are in the folder called _'data'_.

This folder contains the following required folders and files: _'language', 'object', 'sequence', 'shaders', 'fonts.dat', 'g2.dat', 'tracks.dat'_

#### Steps

1. Get a hold of the OpenRCT2 data files as described above.
2. On your Android phone's main storage, create a folder called 'openrct2'.
3. Copy the requires OpenRCT2 folders and files into this new 'openrct2' directory.

**Result:** your 'openrct2' folder should look like the first and second screenshot in the reference image below.

### OpenRCT2 app
Last but not least, you will need the OpenRCT2 app.

#### Steps
1. [Download](https://openrct2.org/downloads) the Android APK on your phone. Make sure the version of the game is the same of the one that was used for the OpenRCT2 data files.
2. Launch the APK on your phone to install the app.
   * Your phone may complain the APK is untrusted. This happens to all APK's that are not installed through Google's Play Store. You can enable the "Allow unknown sources" setting to continue installation. Make sure to disable the setting again after the installation is complete.
3. Launch the app from your phone's home screen or app launcher.
4. Accept the permissions to read and write to disk (so the game can read the assets and create saved games).

**Result:** OpenRCT2 should launch, like shown in the fourth reference screenshot.

The first launch may take some time, or crash once or twice, but after a while it should work. If you run into any errors, take a look at the [Potential problems](#potential-problems) section.

### Reference image
Your folders should look like this:
[![OpenRCT2 required folders.](https://i.imgur.com/X3mqnjc.png)](https://i.imgur.com/X3mqnjc.png)

## Savegames and other user content
Your savegames will be saved in the automatically created 'openrct2-user' folder in the root of your phone's main storage. You can also put downloaded parks, tracks, custom scenery and other custom content here.

## Potential problems
### Language files
If you get the following popup:
> Failed to load language file! Your installation may be damaged.

You have installed the OpenRCT2 data files in the incorrect location, or these files are incomplete. Please review the steps for the [OpenRCT2 data files](#OpenRCT2-data-files) again. If that does not work, please review the technical chapter about [Paths](#Paths).

### Original files
If you get the following popup:
> OpenRCT2 needs files from the original RollerCoaster Tycoon 2 in order to work. Please set the "game_path" variable in config.ini to the directory where you installed RollerCoaster Tycoon 2, then restart OpenRCT2.

1. Go to the 'openrct2-user' folder on the phone's main storage. This folder is automatically created after you launch the app for the first time.
2. Open 'config.ini'.
   * If you have an Android app that can edit *.ini files, you can use that. But be careful because some apps that allow text styling may corrupt this file.
   * If you have the phone connected to a desktop via USB, you can copy the file off the phone to your desktop and edit it there with programs like Notepad. Afterwards copy the edited file back to the phone's storage.
3. Inside 'config.ini', find the `game_path` property.
4. Ensure this is set to `"/sdcard/rct2"` or `"/storage/emulated/0/rct2"` (should work for most phones).

Example of the final setting:
```
game_path = "/storage/emulated/0/rct2"
```

If it still does not work, you have installed the RCT2 assets in the incorrect location, or the assets are incomplete. Please review the steps for the [RCT2 assets](#RCT2-assets) again. If that does not work, please review the technical chapter about [Paths](#Paths).

### Mismatched g2.dat size (or sprite or text issues)

If you launch the game and you get a warning about "Mismatched g2.dat", or there are some sprite or text issues (like all 'L' characters replaced with something else), then you have installed OpenRCT2 data files from a different version than what the APK is.

- When using the release version APK; use the OpenRCT2 data files from the exact same release version.
- When using the develop version APK; use the OpenRCT2 data files from exact same develop version and commit hash. The commit hash is the scrambled text that comes after the version number, for example in `v0.4.5-ed875a1` the commit hash is the `ed875a1` part. **Both the APK and the data files must have the same commit hash.**

For more information, see [OpenRCT2 data files](#OpenRCT2-data-files).

## Technical information
### Paths
By default the app searches for its asset and data files under `/sdcard` which is often an alias for `/storage/emulated/0`. This location is internal to the Android operating system and often the path where your main storage device gets mounted. On newer Android phones this is often the internal storage device, but on older phones it might be the actual external SD-card.

If the OpenRCT2 app cannot find the required files, despite trying all previous steps:
* It might help to try moving the files to other storage devices on your Android.
* Try to figure out for your specific phone where `/sdcard` maps too.
* Try to figure out what path maps to your favourite storage device and set it in the [Configuration](#Configuration).

### Configuration
Similar to the desktop versions, you can edit `/openrct2-user/config.ini` to reconfigure settings that are not in the ingame options window.

Some examples:
* `game_path`: where the RCT2 asset files are located.
* `rct1_path`: where the RCT1 asset files are located, if you want to link RCT1 as well.

### Launching from ADB
If you have the ADB (Android Debug Bridge) commandline tool installed, you can launch the game with `am` (activity manager) to pass in additional arguments, like `--verbose`:
```
adb shell am start \
    -a android.intent.action.MAIN \
    -n io.openrct2.debug/io.openrct2.MainActivity \
    --esa commandLineArgs --verbose,--rct2-data-path="/sdcard/rct2"
```
Following `commandLineArgs`, is the comma-seperated list of arguments for OpenRCT2.

To read out the logs, you can run this on Windows:
```
adb logcat | findstr /i openrct2
```
Or this on MacOS/Linux:
```
adb logcat | grep --ignore-case openrct2
```

More info regarding ABD [here](https://developer.android.com/studio/command-line/adb).


# Step-by-step guide

2024-12-21 Walkthrough

1. Determine which version you want. In this tutorial I will use the latest version.

   1. All versions release at the same time, so you can use the latest release for both PC and Android and they should match

   2. Double check the version matches for both PC and Android downloads

2. PC steps

   1. Download RCT2 from GOG or Steam (not covered in this tutorial. Optionally, also install RCT1)

   2. Download OpenRCT2 for your Windows PC

      1. <https://openrct2.io/download/release> OR <https://github.com/OpenRCT2/OpenRCT2/releases>

      2. Choose any version.

   3. Install the game (not covered in this tutorial)

   4. Launch the game to verify it is installed correctly

   5. Stage RCT2 files

      1. Create new folder anywhere called “OpenRCT2\_Staging”

      2. Enter OpenRCT2\_Staging folder

         1. Create new folder called “rct2”

         2. Enter rct2 folder

            1. Copy files from your GOG or Steam RCT2 Installation Directory

               1a. The default installation path for GOG RCT2 is: C:\Program Files (x86)\GOG Galaxy\Games\RollerCoaster Tycoon 2 Triple Thrill Pack\\

               1b. The default installation path for Steam RCT2 is: SteamLibrary\steamapps\common\RollerCoaster Tycoon 2\\

            2. Copy the following folders to the OpenRCT2\_Staging\\**rct2**\ folder:

               1. “Data” folder

               2. “ObjData” folder

            3. Note: The following folders are optional: 'Landscapes', 'Saved Games', 'Scenarios', 'Tracks'

   6. Stage OpenRCT2 files

      1. Enter OpenRCT2\_Staging folder

         1. Create new folder called “openrct2”

         2. Enter openrct2 folder

            1. Copy files from your OpenRCT2 Installation Directory

               1. The default installation path for OpenRCT2 is C:\Users\cartm\Documents\OpenRCT2\\

               2. Enter the “bin” folder

               3. Enter the “data” folder”

               4. Copy the following folders and files to the OpenRCT2\_Staging\\**openrct2**\ folder:

                  1. “language” folder

                  2. “object” folder

                  3. “sequence” folder

                  4. “shaders” folder

                  5. “fonts.dat” file

                  6. “g2.dat” file

                  7. “tracks.dat” file

   7. Optional custom content staging

      1. Enter OpenRCT2\_Staging folder

         1. Create new folder called “openrct2-user-FromPC”

         2. Enter openrct2-user-FromPC folder

            1. Copy files from your OpenRCT2 Installation Directory

               1. The default installation path for OpenRCT2 is C:\Users\cartm\Documents\OpenRCT2\\

               2.  Copy the following folders and files to the OpenRCT2\_Staging\\**openrct2-user-FromPC**\ folder:

                  1. “plugin” folder

                  2. “save” folder

                  3. “scenario” folder

                  4. “track” folder

   8. Optional RCT1 file staging

      1. Create new folder anywhere called “OpenRCT2\_Staging”

      2. Enter OpenRCT2\_Staging folder

         1. Create new folder called “rct1”

         2. Enter rct1 folder

            1. Copy files from your GOG RCT2 or Steam RCT Deluxe Installation Directory

               1a. The default installation path for GOG RCT2 is: C:\Program Files (x86)\GOG Galaxy\Games\RollerCoaster Tycoon Deluxe\\

               1b. The default installation path for Steam RCT Deluxe is: SteamLibrary\steamapps\common\RollerCoaster Tycoon Deluxe\\

            2. Enter the “RollerCoaster Tycoon Deluxe” folder

            3. Copy the entire contents to the OpenRCT2\_Staging\\**rct1**\ folder

   9. Copy files to Android

      1. Zip the OpenRCT2\_Staging folder

      2. Copy the zipped folder to the root of your Android storage

      3. Extract the zip’s contents to the root of Android storage

         1. i.e. Android storage should now look like this

            1. sdcard\\

               1. openrct2\\

               2. openrct2-user-FromPC\  (optional)

               3. rct1\   (optional)

               4. rct2\\

3. Android steps

   1. Download OpenRCT2 APK

      1. <https://openrct2.io/download/release> OR <https://github.com/OpenRCT2/OpenRCT2/releases>

      2. Choose the same version as you chose for OpenRCT2 PC Download above

   2. Install APK

      1. Launch the APK on your phone to install the app.

      2. Your phone may complain the APK is untrusted. This happens to all APK's that are not installed through Google's Play Store. You can enable the "Allow unknown sources" setting to continue installation. Make sure to disable the setting again after the installation is complete.

   3. Launch the app from your phone's home screen or app launcher.

   4. Accept the permissions to read and write to disk (so the game can read the assets and create saved games).

   5. Optional

      1. Import custom content from the openrct2-user-FromPC folder

         1. Open file explorer on your Android

         2. Go to the root of sdcard

         3. Notice folder openrct2-user which was created when launching OpenRCT2 Android for the first time

         4. Copy the contents of openrct2-user-FromPC into openrct2-user

      2. Configure path to RCT1 if it does not import automatically

         1. Open the openrct2-user folder

         2. Edit config.ini

         3. Enter a value for “rct1\_path” variable

            1. EX: rct1\_path = "/sdcard/rct1"

         4. Save the file

         5. Relaunch OpenRCT2 Android and check for RCT1 scenarios

\


Updating the game on Android (no PC required):

1. Download OpenRCT2 Portable

   1. <https://openrct2.io/download/release> OR <https://github.com/OpenRCT2/OpenRCT2/releases>

   2. Choose the latest Windows x86 Portable release

2. Unzip those files

3. Enter the “data” folder

4. Copy the following folders and files to the sdcard\\**openrct2**\ folder:

   1. “language” folder

   2. “object” folder

   3. “sequence” folder

   4. “shaders” folder

   5. “fonts.dat” file

   6. “g2.dat” file

   7. “tracks.dat” file

5. Uninstall your OpenRCT2 app

   1. NOTE: At time of writing an update-in-place failed. I had to uninstall the app. (Do not delete your folders!!!)

6. Download the latest OpenRCT2 APK as before

   1. <https://openrct2.io/download/release> OR <https://github.com/OpenRCT2/OpenRCT2/releases>

   2. Choose the same version as you chose for OpenRCT2 PC Download above

7. Install APK

   1. Launch the APK on your phone to install the app.

   2. Your phone may complain the APK is untrusted. This happens to all APK's that are not installed through Google's Play Store. You can enable the "Allow unknown sources" setting to continue installation. Make sure to disable the setting again after the installation is complete.

8. Launch the app from your phone's home screen or app launcher.

9. Accept the permissions to read and write to disk (so the game can read the assets and create saved games).

